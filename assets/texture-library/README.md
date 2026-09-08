# Physical reflectance-variance textures

These seamless BaseColor images are used only as spatial variance templates.
HiStream converts sRGB to linear luminance, removes the image mean, samples the
result with world-space triplanar projection, and area-normalizes every material
to a factor mean of exactly 1.0. The configured spectral reflectance remains the
material mean; thermal-infrared emissivity is not changed.

Physical textures are disabled by default. `strength` controls contrast and
`repeatSize` is the world-space tile size in metres.

All images are ambientCG CC0 assets. See `LICENSE.md` and `catalog.json`.
