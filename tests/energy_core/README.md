`node tools/test-energy-core.mjs` extracts the production GLSL scalar functions
and compiles/runs them with native C++ float32 arithmetic. Requires MSVC C++
build tools on Windows, or `c++` on Unix. No GPU or engine build is performed.

Checks: steady/isothermal profiles, nonuniform finite-volume transient energy
conservation with a nonzero lower-boundary flux, maximum principle, exact
surface-flux derivative against finite differences, material heat capacity,
density, conductivity, SMC percentage/fraction and saturation sensitivity,
and the production Farquhar nonvegetation dispatch with checked material buffers.
The production primitive-building input helper is compiled directly as well:
single bindings expand to wall/roof, roof overrides remain intact, position
files supply all instances, and malformed or empty rows are rejected safely.

`node tools/test-energy-core.mjs --prepare-engine` creates the mixed-scene
projects without launching the engine. `--engine` additionally runs the built
Release engine serially for traditional and accelerated radiation. Each case
has one midday node, a 12 m × 8 m × 4 m domain and 1 m voxels. The soil, building
and water indices deliberately differ from the only leaf index. Process scene
links must contain all four actual types and both requested building placements.
Energy/carbon fields must be finite,
all nonvegetation GPP/NPP must be zero, building latent heat must be zero, and
at least one leaf must photosynthesize. Results and logs use fresh directories
under `.test/energy-core` and retain the tested engine SHA256.

These are conservation and software regression checks, not validation against
field observations or a claim of full-scene physical accuracy.
