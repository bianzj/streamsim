#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(location = 0) flat out uint outCameraSide;

layout(push_constant) uniform VisibilityPush {
    mat4 viewProjection;
    vec4 viewDirection;
    uvec4 raster;
} pc;

void main()
{
    const uint facet = uint(gl_VertexIndex) / 3u;
    const uint opposite = dot(inNormal, pc.viewDirection.xyz) >= 0.0 ? 0u : 1u;
    outCameraSide = facet * 2u + opposite;
    gl_Position = pc.viewProjection * vec4(inPosition, 1.0);
}
