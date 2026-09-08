#ifndef HISTREAM_FLUID_COUPLING_GLSL
#define HISTREAM_FLUID_COUPLING_GLSL

int fluidLinearIndex(ivec3 cell)
{
    ivec3 size = fluidParameters.grid.xyz;
    if (cell.x < 0 || cell.y < 0 || cell.z < 0 ||
        cell.x >= size.x || cell.y >= size.y || cell.z >= size.z) return -1;
    return cell.x + size.x * (cell.z + size.z * cell.y);
}

// Air temperature adjacent to a sparse radiative voxel. Solid cells use
// their surrounding air. The return value is Celsius for EB shaders.
float localAirTemperatureC(uint sparseIndex)
{
    if (fluidParameters.grid.w == 0) return ubom.meteo.Ta;
    ivec3 sparseCell = voxelLinks[sparseIndex].voxelId;
    vec3 radiativeSpacing = max(
        fluidParameters.couplingSpacing.xyz, vec3(0.001));
    vec3 fluidSpacing = max(fluidParameters.spacingTime.xyz, vec3(0.001));
    vec3 position = (vec3(max(sparseCell, ivec3(0))) + vec3(0.5))
        * radiativeSpacing;
    if (sparseCell.y < 0) position.y = 0.5 * fluidSpacing.y;
    ivec3 cell = ivec3(floor(position / fluidSpacing));
    int center = fluidLinearIndex(cell);
    if (center < 0) return ubom.meteo.Ta;
    if (fluidMeta[center].kind != 1) {
        float temperature = fluidScalarA[center].x;
        return (isnan(temperature) || isinf(temperature))
            ? ubom.meteo.Ta : temperature - 273.15;
    }
    const ivec3 offsets[6] = ivec3[6](
        ivec3(1,0,0), ivec3(-1,0,0), ivec3(0,1,0),
        ivec3(0,-1,0), ivec3(0,0,1), ivec3(0,0,-1));
    float sum = 0.0;
    float count = 0.0;
    for (int i = 0; i < 6; ++i) {
        int neighbour = fluidLinearIndex(cell + offsets[i]);
        if (neighbour >= 0 && fluidMeta[neighbour].kind != 1) {
            float temperature = fluidScalarA[neighbour].x;
            if (!isnan(temperature) && !isinf(temperature)) {
                sum += temperature;
                count += 1.0;
            }
        }
    }
    return count > 0.0 ? sum / count - 273.15 : ubom.meteo.Ta;
}

// Local resolved air speed used by aerodynamic, sensible-heat and latent-heat
// resistance calculations. Solid surfaces sample their adjacent air cells.
float localWindSpeed(uint sparseIndex)
{
    if (fluidParameters.grid.w == 0) return ubom.meteo.u;
    ivec3 sparseCell = voxelLinks[sparseIndex].voxelId;
    vec3 radiativeSpacing = max(
        fluidParameters.couplingSpacing.xyz, vec3(0.001));
    vec3 fluidSpacing = max(fluidParameters.spacingTime.xyz, vec3(0.001));
    vec3 position = (vec3(max(sparseCell, ivec3(0))) + vec3(0.5))
        * radiativeSpacing;
    if (sparseCell.y < 0) position.y = 0.5 * fluidSpacing.y;
    ivec3 cell = ivec3(floor(position / fluidSpacing));
    int center = fluidLinearIndex(cell);
    if (center < 0) return ubom.meteo.u;
    if (fluidMeta[center].kind != 1) {
        float speed = length(fluidVelocityA[center].xyz);
        return (isnan(speed) || isinf(speed)) ? ubom.meteo.u : speed;
    }
    const ivec3 offsets[6] = ivec3[6](
        ivec3(1,0,0), ivec3(-1,0,0), ivec3(0,1,0),
        ivec3(0,-1,0), ivec3(0,0,1), ivec3(0,0,-1));
    float sum = 0.0;
    float count = 0.0;
    for (int i = 0; i < 6; ++i) {
        int neighbour = fluidLinearIndex(cell + offsets[i]);
        if (neighbour >= 0 && fluidMeta[neighbour].kind != 1) {
            float speed = length(fluidVelocityA[neighbour].xyz);
            if (!isnan(speed) && !isinf(speed)) {
                sum += speed;
                count += 1.0;
            }
        }
    }
    return count > 0.0 ? sum / count : ubom.meteo.u;
}

#endif
