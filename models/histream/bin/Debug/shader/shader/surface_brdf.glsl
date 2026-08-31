#ifndef SURFACE_BRDF_GLSL
#define SURFACE_BRDF_GLSL

// Both directions are converted to vectors pointing away from the surface.
vec3 surfaceHemisphereDirection(vec3 direction, vec3 normal)
{
    vec3 d = normalize(direction);
    return dot(d, normal) >= 0.0 ? d : -d;
}

float hapkeHFunction(float mu, float singleScatteringAlbedo)
{
    float gamma = sqrt(max(1.0 - singleScatteringAlbedo, 1e-6));
    return (1.0 + 2.0 * mu) / (1.0 + 2.0 * mu * gamma);
}

float hapkePhaseFunction(float cosPhase, float asymmetry)
{
    float g = clamp(asymmetry, -0.95, 0.95);
    float denominator = max(1.0 + 2.0 * g * cosPhase + g * g, 1e-6);
    return (1.0 - g * g) / pow(denominator, 1.5);
}

float hapkeKernel(float mu0, float mu, float phaseAngle,
                  float singleScatteringAlbedo, float B0, float h, float g)
{
    float opposition = max(B0, 0.0)
        / (1.0 + tan(0.5 * phaseAngle) / max(h, 1e-4));
    float phase = hapkePhaseFunction(cos(phaseAngle), g);
    float multiple = hapkeHFunction(mu0, singleScatteringAlbedo)
                   * hapkeHFunction(mu, singleScatteringAlbedo) - 1.0;
    return singleScatteringAlbedo / (4.0 * PI)
         * mu0 / max(mu0 + mu, 1e-5)
         * ((1.0 + opposition) * phase + multiple);
}

float hapkeReflectanceWeight(float reflectance, vec3 incomingDirection,
                             vec3 outgoingDirection, vec3 surfaceNormal,
                             float B0, float h, float g)
{
    vec3 n = normalize(surfaceNormal);
    vec3 wi = surfaceHemisphereDirection(incomingDirection, n);
    vec3 wo = surfaceHemisphereDirection(outgoingDirection, n);
    float mu0 = max(dot(n, wi), 1e-4);
    float mu = max(dot(n, wo), 1e-4);
    float phaseAngle = acos(clamp(dot(wi, wo), -1.0, 1.0));

    // Convert the configured Lambert reflectance to an equivalent Hapke
    // single-scattering albedo, preserving its wavelength dependence.
    float r = clamp(reflectance, 0.0, 0.999);
    float singleScatteringAlbedo = clamp(4.0 * r / pow(1.0 + r, 2.0), 1e-5, 0.999);
    float directional = hapkeKernel(mu0, mu, phaseAngle,
                                    singleScatteringAlbedo, B0, h, g);
    float reference = hapkeKernel(1.0, 1.0, 0.0,
                                  singleScatteringAlbedo, 0.0, h, g);
    return clamp(r * directional / max(reference, 1e-6), 0.0, 4.0);
}

float dielectricFresnel(float cosIncident, float refractiveIndex)
{
    float eta = max(refractiveIndex, 1.0001);
    float cosI = clamp(cosIncident, 0.0, 1.0);
    float sinT2 = (1.0 - cosI * cosI) / (eta * eta);
    if (sinT2 >= 1.0)
        return 1.0;
    float cosT = sqrt(max(1.0 - sinT2, 0.0));
    float rs = (cosI - eta * cosT) / max(cosI + eta * cosT, 1e-6);
    float rp = (eta * cosI - cosT) / max(eta * cosI + cosT, 1e-6);
    return 0.5 * (rs * rs + rp * rp);
}

float coxMunkReflectanceWeight(float reflectance, vec3 incomingDirection,
                               vec3 outgoingDirection, vec3 surfaceNormal,
                               float refractiveIndex, float configuredVariance,
                               float diffuseFraction, float windSpeed)
{
    vec3 n = normalize(surfaceNormal);
    vec3 wi = surfaceHemisphereDirection(incomingDirection, n);
    vec3 wo = surfaceHemisphereDirection(outgoingDirection, n);
    float mu0 = max(dot(n, wi), 1e-4);
    float mu = max(dot(n, wo), 1e-4);
    vec3 halfVector = normalize(wi + wo);
    float muHalf = max(dot(n, halfVector), 1e-4);

    // Cox-Munk isotropic mean-square slope; an explicit XML value overrides it.
    float variance = configuredVariance > 0.0
        ? configuredVariance
        : 0.003 + 0.00512 * clamp(windSpeed, 0.0, 30.0);
    variance = max(variance, 1e-4);
    float tanHalf2 = max(1.0 - muHalf * muHalf, 0.0) / (muHalf * muHalf);
    float slopeDistribution = exp(-tanHalf2 / variance)
        / (PI * variance * pow(muHalf, 4.0));
    float fresnel = dielectricFresnel(max(dot(wi, halfVector), 0.0),
                                      refractiveIndex);
    float specularBrdf = fresnel * slopeDistribution / (4.0 * mu0 * mu);
    float diffuse = clamp(diffuseFraction, 0.0, 1.0) * max(reflectance, 0.0);
    return clamp(diffuse + PI * specularBrdf, 0.0, 16.0);
}

float surfaceReflectanceWeight(uint typeId, uint bioId, float reflectance,
                               vec3 incomingDirection, vec3 outgoingDirection,
                               vec3 surfaceNormal)
{
    if (typeId == TYPE_SOIL && soilSets[bioId].brdfModel == 1)
    {
        return hapkeReflectanceWeight(
            reflectance, incomingDirection, outgoingDirection, surfaceNormal,
            soilSets[bioId].hapkeB0, soilSets[bioId].hapkeH,
            soilSets[bioId].hapkeG);
    }
    if (typeId == TYPE_WATER && waterSets[bioId].brdfModel == 1)
    {
        return coxMunkReflectanceWeight(
            reflectance, incomingDirection, outgoingDirection, surfaceNormal,
            waterSets[bioId].refractiveIndex, waterSets[bioId].slopeVariance,
            waterSets[bioId].diffuseFraction, ubom.meteo.u);
    }
    return max(reflectance, 0.0);
}

#endif
