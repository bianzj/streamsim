#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include "production_par.hpp"

namespace {
unsigned checks = 0;
void require(bool valid, const char* message) {
    ++checks;
    if (!valid) { std::cerr << message << '\n'; std::exit(1); }
}
void close(float actual, double expected, double relative, const char* message) {
    require(std::isfinite(actual) &&
        std::abs(static_cast<double>(actual) - expected) <=
        relative * std::max(std::abs(expected), 1.0e-8), message);
}
double reference(double watts, double wavelengthNm) {
    // Independently use production's legacy constant; no hidden 1e-3 or 1000.
    return watts * wavelengthNm / 119.7117122;
}
}

int main() {
    const float monochromatic = productionDirectPar(1.0f, 660.0f);
    close(monochromatic, 5.513245010624783, 2.0e-7,
        "1 W/m2 at 660 nm must be about 5.51 umol photons/m2/s");
    close(productionDiffusePar(1.0f, 660.0f), monochromatic, 1.0e-7,
        "Direct and diffuse PAR must have identical physical units");
    for (float wavelength : {399.0f, 399.99f, 700.01f, 800.0f, 2400.0f}) {
        require(productionDirectPar(1.0f, wavelength) == 0.0f,
            "Direct non-PAR wavelengths cannot contribute photons");
        require(productionDiffusePar(1.0f, wavelength) == 0.0f,
            "Diffuse non-PAR wavelengths cannot contribute photons");
    }
    for (float wavelength : {400.0f, 700.0f}) {
        close(productionDirectPar(1.0f, wavelength), reference(1.0, wavelength),
            2.0e-7, "Inclusive 400 and 700 nm boundaries must contribute direct PAR");
        close(productionDiffusePar(1.0f, wavelength), reference(1.0, wavelength),
            2.0e-7, "Inclusive 400 and 700 nm boundaries must contribute diffuse PAR");
    }

    // The legacy constant differs slightly from exact SI, not by 1000.
    constexpr double exactSI = 6.02214076e23 * 6.62607015e-34 * 299792458.0 * 1.0e3;
    constexpr double legacyConstantRelativeError = (119.7117122 - exactSI) / exactSI;
    require(legacyConstantRelativeError > 0.0007 && legacyConstantRelativeError < 0.0008,
        "Photon-energy constant drift must remain explicitly below 0.08 percent");

    // Accelerated groups may approximate spectra at a representative sample,
    // but no group may mix the inclusive 700 nm PAR edge with >700 nm power.
    for (int width : {-9, 0, 1, 2, 5, 8, 17, 301, 700, 2001, 1000000000}) {
        unsigned visits[2001] = {};
        unsigned parVisits = 0;
        for (int first = 0; first < 2001;) {
            const int end = shortwaveGroupEnd(first, width, 2001);
            require(end > first && end <= 2001, "Spectral groups must advance within the buffer");
            require(!(first < 301 && end > 301), "PAR and non-PAR samples cannot share a group");
            const int centre = (first + end - 1) / 2;
            const bool isPar = 400 + centre <= 700;
            for (int band = first; band < end; ++band) {
                ++visits[band];
                if (isPar) ++parVisits;
                require(isPar == (band <= 300), "Group PAR classification must match every sample");
            }
            first = end;
        }
        require(parVisits == 301, "Exactly the 400 through 700 nm samples must contribute to PAR");
        for (unsigned count : visits)
            require(count == 1, "All 2001 shortwave samples must be visited exactly once");
    }

    const float wavelengths[] = {400.0f, 550.0f, 660.0f, 700.0f};
    const float directWatts[] = {2.0f, 3.0f, 7.0f, 1.0f};
    const float diffuseWatts[] = {1.0f, 5.0f, 2.0f, 4.0f};
    float direct = 0.0f, diffuse = 0.0f;
    double directExpected = 0.0, diffuseExpected = 0.0;
    for (unsigned i = 0; i < 4; ++i) {
        direct += productionDirectPar(directWatts[i], wavelengths[i]);
        diffuse += productionDiffusePar(diffuseWatts[i], wavelengths[i]);
        directExpected += reference(directWatts[i], wavelengths[i]);
        diffuseExpected += reference(diffuseWatts[i], wavelengths[i]);
    }
    close(direct, directExpected, 2.0e-7, "Multi-band direct energy must be photon-weighted");
    close(diffuse, diffuseExpected, 2.0e-7, "Multi-band diffuse energy must be photon-weighted");

    close(productionCollatzQ(diffuse, direct, 1), directExpected + diffuseExpected,
        2.0e-7, "Collatz sunlit leaves need direct plus diffuse PAR");
    close(productionCollatzQ(diffuse, direct, 0), diffuseExpected,
        2.0e-7, "Collatz shaded leaves need diffuse PAR");
    close(productionFarquharQ(diffuse, direct, 0), directExpected + diffuseExpected,
        2.0e-7, "Farquhar sunlit leaves need direct plus diffuse PAR");
    close(productionFarquharQ(diffuse, direct, 1), diffuseExpected,
        2.0e-7, "Farquhar shaded leaves need diffuse PAR");
    for (int component = 0; component < 2; ++component) {
        close(productionCollatzQ(diffuse, 0.0f, component), diffuseExpected,
            2.0e-7, "Pure diffuse light must illuminate both Collatz components");
        close(productionFarquharQ(diffuse, 0.0f, component), diffuseExpected,
            2.0e-7, "Pure diffuse light must illuminate both Farquhar components");
        require(productionCollatzQ(0.0f, 0.0f, component) == 0.0f,
            "Collatz night PAR must be zero");
        require(productionFarquharQ(0.0f, 0.0f, component) == 0.0f,
            "Farquhar night PAR must be zero");
    }

    // Old method 0 compensated the millimole buffer. Its physical light scale
    // must be preserved after removing both conversion factors together.
    constexpr float po0 = 0.81f, beta = 0.5f, fPAR = 1.0f;
    for (float watts : {0.0f, 1.0e-8f, 1.0f, 100.0f, 500.0f}) {
        const float newQ = productionDirectPar(watts, 660.0f);
        const float oldQ = watts * 660.0f * 1.0e-3f / static_cast<float>(119.7117122);
        const float oldJe = 0.5f * po0 * oldQ * 1000.0f * fPAR;
        close(productionCollatzJe(newQ, po0, fPAR), oldJe, 3.0e-7,
            "Removing producer 1e-3 and Collatz 1000 must preserve method 0 light scale");
        close(productionFarquharQ2(newQ, beta, po0),
            beta * po0 * reference(watts, 660.0), 3.0e-7,
            "Farquhar Q2 must consume micromoles, including weak light and night");
    }
    const float nan = std::numeric_limits<float>::quiet_NaN();
    require(std::isnan(productionDirectPar(nan, 660.0f)),
        "Invalid absorbed band energy cannot silently become valid PAR");
    std::cout << std::setprecision(12)
        << "{\"test\":\"production_par_float32\",\"checks\":" << checks
        << ",\"monochromatic_660nm_1W_umol_m2_s\":" << monochromatic
        << ",\"direct_multiband_umol_m2_s\":" << direct
        << ",\"diffuse_multiband_umol_m2_s\":" << diffuse
        << ",\"legacy_AHC_relative_error\":" << legacyConstantRelativeError
        << ",\"units\":\"umol photons m^-2 s^-1\"}\n";
}
