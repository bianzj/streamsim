# Production biochemical scalar regression

Run `node tools/test-biochemical-core.mjs`. MSVC (Windows) or a C++17 compiler
is required. No Vulkan dispatch or engine build is performed.

The runner extracts scalar helpers and the complete `main()` bodies of both
production biochemical shaders, compiles them with float32 constants, and
executes bounds-checked synthetic SSBOs. Atomic stores are sequential assignments
for this single-invocation scalar test; GPU dispatch and binding validation are
separate engine tests.

Coverage includes the exact zero-curvature electron-transport limit, weak light,
large excitation, cancellation-resistant and degenerate quadratic equations,
C3/C4 daylight, direct/diffuse/night distinction, zero CO2/RH/capacity/stress,
Kelvin/Celsius input handling, material-type buffer isolation, and explicit NaN
for invalid parameters or non-real model equations. The tested finite parameter
grid is a numerical stability check; it does not establish biological accuracy
over that temperature, humidity or concentration range.

`normal-domain-baseline.json` freezes 32 cases evaluated with the production
shader bodies from commit `7cf3bfb`: both methods, C3/C4, 25 C, RH 0.5/1,
CO2 400/1000 ppm and direct PAR 100/2000 plus 20% diffuse PAR. The old Collatz
input is divided by 1000 to cancel its historical `Je * 1000`; Farquhar is
given photon PAR directly to isolate the equation changes from the separately
corrected writer units. Carbon flux, resistance and intercellular CO2 must agree
within float32 root-evaluation tolerance. The fixture is a numerical baseline,
not an independent biological reference.

In particular, the full C4 carboxylation equation can have a genuinely negative
discriminant at very high leaf temperature and very low CO2. Such a result stays
missing and must be rejected by the engine's numerical diagnostics. A negative
discriminant may only be rounded to zero when it lies within the scalar
roundoff tolerance. Unsupported or overflowed calculations are never replaced
with a seemingly valid zero-carbon output.

`NPP` in the existing binary contract is leaf net CO2 assimilation (`Ag - Rd`),
not whole-plant or ecosystem net primary production. These regressions validate
the equations and numerical boundaries, not independent field observations.
