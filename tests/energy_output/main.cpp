#include "energy_output.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace streamsim::energy_output;
int checks = 0;
void near(double actual, double expected)
{
    ++checks;
    if (!std::isfinite(actual) || std::abs(actual - expected) > 1e-9 * std::max(1.0, std::abs(expected)))
        throw std::runtime_error("Energy-output analytic assertion failed");
}
int main()
{
    near(emittedFlux(300.0, 1.0), 459.3024);
    near(emittedFlux(600.0, 1.0), 16.0 * 459.3024);
    Input input;
    input.exchange = Exchange::singleSurface;
    input.sunlitTemperature = input.shadedTemperature = 300.0;
    input.diffuseLongwave = 459.3024;
    auto solid = calculate(input);
    near(solid.netRadiation, 0.0); // blackbody in an isothermal radiative enclosure
    input.exchange = Exchange::twoSidedLeaf;
    auto leaf = calculate(input);
    near(leaf.netRadiation, 0.0);
    near(leaf.emittedLongwave, 918.6048);

    input.diffuseShortwave = 100.0;
    input.diffuseLongwave = 150.0;
    input.directShortwave = 200.0;
    input.directLongwave = 20.0;
    input.sunlitFraction = 0.25;
    input.sensibleSunlit = 20.0;
    input.sensibleShaded = 10.0;
    input.latentSunlit = 40.0;
    input.latentShaded = 30.0;
    input.storageSunlit = 1000.0;
    input.storageShaded = 2000.0;
    leaf = calculate(input);
    near(leaf.absorbedShortwave, 250.0);
    near(leaf.absorbedLongwave, 305.0);
    near(leaf.netRadiation, -363.6048);
    near(leaf.heatFluxSum, 90.0); // two faces; leaf does not conduct into a soil column
    near(leaf.energyResidual, -453.6048); // independent, signed, and not clipped to zero
    near(leaf.netRadiationSunlit, -198.6048);
    near(leaf.netRadiationShaded, -418.6048);
    input.exchange = Exchange::singleSurface;
    solid = calculate(input);
    near(solid.netRadiation, -154.3024);
    near(solid.heatFluxSum, 1795.0);
    near(solid.energyResidual, -1949.3024);

    input.sunlitTemperature = 600.0;
    input.shadedTemperature = 300.0;
    solid = calculate(input);
    near(solid.emittedLongwave, 4.75 * 459.3024); // mix T^4, never fourth power of mean T
    input.thermalReflectance = 0.2;
    input.thermalTransmittance = 0.3;
    solid = calculate(input);
    near(solid.thermalEmissivity, 0.5);
    near(solid.emittedLongwave, 2.375 * 459.3024);
    input.thermalReflectance = 1.2;
    near(calculate(input).thermalEmissivity, 0.0);
    input.thermalReflectance = -1.0;
    input.thermalTransmittance = 0.0;
    near(calculate(input).thermalEmissivity, 1.0);
    input.sunlitFraction = 2.0;
    near(calculate(input).sunlitFraction, 1.0);
    input.sunlitFraction = -1.0;
    near(calculate(input).sunlitFraction, 0.0);

    input.exchange = Exchange::prescribedMedium;
    auto medium = calculate(input);
    if (medium.closureApplicable) throw std::runtime_error("Medium is not a surface energy balance");
    near(medium.exchangeFaceCount, 0.0);
    near(medium.netRadiation, 0.0); // documented finite not-applicable process placeholder
    near(medium.energyResidual, 0.0);
    std::cout << "{\"passed\":true,\"assertions\":" << checks
              << ",\"scope\":\"production independent energy-output helper; no GPU or solver convergence validation\"}\n";
}
