#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
#include "radiation_spectrum.h"
#include "production_normalization.hpp"

namespace {
unsigned checks = 0;
void require(bool valid, const char* message) {
    ++checks;
    if (!valid) { std::cerr << message << '\n'; std::exit(1); }
}
void close(double actual, double expected, const char* message) {
    require(std::isfinite(actual) && std::abs(actual - expected) <=
        2.0e-6 * std::max(1.0, std::abs(expected)), message);
}
double integral(const std::vector<float>& shape) {
    double total = 0;
    for (size_t i = 1; i < shape.size(); ++i)
        total += (static_cast<double>(shape[i - 1]) + shape[i]) * 0.5;
    return total;
}
std::vector<float> normalized(const std::vector<float>& shape, float fraction) {
    const float total = static_cast<float>(integral(shape));
    std::vector<float> coefficients(shape.size()), unusedDiffuse(shape.size());
    productionShortwaveCoefficients(shape, shape, total, total, fraction, coefficients, unusedDiffuse);
    return coefficients;
}
void rejected(float sample, float total, float fraction) {
    bool failed = false;
    try { (void)normalizedShortwaveCoefficient(sample, total, fraction); }
    catch (const std::runtime_error&) { failed = true; }
    require(failed, "Invalid spectrum silently produced a usable shortwave coefficient");
}
}

int main(int argc, char** argv) {
    require(argc == 2, "The parser fixture directory must be explicit");
    const auto fixtureDirectory = std::filesystem::u8path(argv[1]);
    auto fixture = [&](const std::string& name, const std::string& contents) {
        const auto file = fixtureDirectory / std::filesystem::u8path(name);
        std::ofstream output(file, std::ios::binary);
        output << contents;
        output.close();
        require(output.good(), "Parser fixture cannot be written");
        return file.u8string();
    };
    auto parsed = [&](const std::string& contents, const std::vector<float>& expected) {
        const auto file = fixture("valid.txt", contents);
        require(readIncidentSpectrum(file, expected.size()) == expected,
            "Incident spectrum parser changed numeric samples or whitespace handling");
    };
    parsed("0\n1\n", {0, 1});
    parsed(std::string("\xEF\xBB\xBF") + "0\r\n1\r\n", {0, 1});
    parsed("\n\t0\tignored\r\n 1.5 other-column\n2e-3\n", {0, 1.5f, 0.002f});
    parsed("  \r\n\t\n", {});
    const auto utf8File = fixture(u8"光谱.txt", "0\n1\n");
    require(readIncidentSpectrum(utf8File, 2) == std::vector<float>({0, 1}),
        "Incident spectrum Unicode paths must remain usable on Windows");
    auto parserRejected = [&](const std::string& contents, size_t required) {
        const auto file = fixture("invalid.txt", contents);
        bool failed = false;
        try { (void)readIncidentSpectrum(file, required); }
        catch (const std::runtime_error&) { failed = true; }
        require(failed, "Invalid or short incident spectrum silently became valid input");
    };
    for (const std::string& invalid : {"bad\n", "2bad\n", "NaN\n", "Inf\n",
         "1e100\n", "-1\n", "0\nwrong\n"}) parserRejected(invalid, 1);
    parserRejected("0\n", 2);
    parserRejected("\n \n", 1);
    bool missingRejected = false;
    try { (void)readIncidentSpectrum((fixtureDirectory / "missing.txt").u8string(), 1); }
    catch (const std::runtime_error&) { missingRejected = true; }
    require(missingRejected, "Missing incident spectrum must be rejected");
    close(normalizedShortwaveCoefficient(1.0f, 1.0f, 1.0f), 1000.0,
        "A 1 nm unit-energy sample must use the GPU coefficient scale 1000");
    close(normalizedShortwaveCoefficient(1.0f, 1.0f, 0.0f), 0.0,
        "Zero direct fraction must eliminate direct spectral energy");
    close(normalizedShortwaveCoefficient(0.0f, 0.0f, 0.0f), 0.0,
        "An unused zero-energy spectral shape must be allowed");

    // Same amplitude and different spectral shapes must obey the configured
    // energy fractions, rather than inheriting each file's arbitrary scale.
    const std::vector<float> sun = {0, 1, 3, 2, 0};
    const std::vector<float> sky = {0, 5, 1, 1, 0};
    for (float directFraction : {0.0f, 0.2f, 0.5f, 0.8f, 1.0f}) {
        std::vector<float> direct(sun.size()), diffuse(sky.size());
        productionShortwaveCoefficients(sun, sky, static_cast<float>(integral(sun)),
            static_cast<float>(integral(sky)), directFraction, direct, diffuse);
        double directIntegral = 0, diffuseIntegral = 0;
        for (float coefficient : direct) directIntegral += coefficient * 0.001;
        for (float coefficient : diffuse) diffuseIntegral += coefficient * 0.001;
        close(directIntegral, directFraction,
            "Direct spectral integral must match the configured direct fraction");
        close(diffuseIntegral, 1.0f - directFraction,
            "Diffuse spectral integral must match the configured diffuse fraction");
        close(directIntegral + diffuseIntegral, 1.0,
            "Combined shortwave spectra must preserve incident-energy normalization");
        if (directFraction == 0.0f)
            for (float coefficient : direct)
                require(coefficient == 0.0f, "Pure diffuse illumination leaked direct energy");
        if (directFraction == 1.0f)
            for (float coefficient : diffuse)
                require(coefficient == 0.0f, "Pure direct illumination leaked diffuse energy");
    }
    auto scaledSun = sun;
    for (float& sample : scaledSun) sample *= 16.0f;
    const auto base = normalized(sun, 0.8f);
    const auto scaled = normalized(scaledSun, 0.8f);
    for (size_t i = 0; i < base.size(); ++i)
        close(scaled[i], base[i], "Source file amplitude must not change a normalized spectral shape");

    // A monochromatic sample centred at 660 nm on the 1 nm grid integrates
    // to one; adjacent zero samples avoid an endpoint quadrature ambiguity.
    std::vector<float> monochromatic(301, 0.0f);
    monochromatic[260] = 1.0f;
    const auto mono = normalized(monochromatic, 1.0f);
    double monoEnergy = 0;
    for (float coefficient : mono) monoEnergy += coefficient * 0.001;
    close(monoEnergy, 1.0, "Monochromatic normalized incident energy is not conserved");
    close(mono[260] * 0.001 * 660.0 / 119.7117122, 5.513245010624783,
        "Monochromatic normalized energy must reach the expected photon flux");
    for (size_t endpoint : {size_t{0}, size_t{300}}) {
        std::vector<float> shape(301, 0.0f);
        shape[endpoint] = 1.0f;
        const auto coefficients = normalized(shape, 1.0f);
        double energy = 0;
        for (float coefficient : coefficients) energy += coefficient * 0.001;
        close(energy, 1.0, "Trapezoidal endpoints cannot double the GPU band-sum energy");
    }

    close(normalizedShortwaveCoefficient(1.0e38f, 1.0e38f, 0.8f), 800.0,
        "Finite high-amplitude inputs cannot overflow before normalization");
    close(normalizedShortwaveCoefficient(1.0e-38f, 1.0e-38f, 0.8f), 800.0,
        "Finite low-amplitude inputs cannot underflow their normalization ratio");
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    for (float bad : {-1.0f, nan, inf}) {
        rejected(bad, 1.0f, 0.8f);
        rejected(1.0f, bad, 0.8f);
        rejected(1.0f, 1.0f, bad);
        rejected(bad, 0.0f, 0.0f);
    }
    rejected(1.0f, 1.0f, 1.1f);
    rejected(1.0f, 0.0f, 1.0f);
    rejected(1.0f, std::numeric_limits<float>::denorm_min(), 1.0f);
    std::cout << "{\"test\":\"production_shortwave_normalization\",\"checks\":" << checks
        << ",\"configured_fractions\":true,\"pure_diffuse\":true,\"pure_direct\":true,\"invalid_rejected\":true}\n";
}
