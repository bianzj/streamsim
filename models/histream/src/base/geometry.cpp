//
// Created by admin on 2024/1/24.
//

#include "geometry.h"
#include <algorithm>
#include <cmath>

namespace {

constexpr double kOrthographicScale = 1.0;

glm::vec3 observationCameraUp(float zenithDegrees, float azimuthDegrees)
{
    const float zenith = zenithDegrees * DEG2RAD;
    const float azimuth = azimuthDegrees * DEG2RAD;
    const glm::vec3 cameraOut{
        std::sin(zenith) * std::cos(azimuth),
        std::cos(zenith),
        std::sin(zenith) * std::sin(azimuth)};
    const glm::vec3 forward = -glm::normalize(cameraOut);
    // +X is north. Project geographic north into the image plane so native
    // output rows are already north-up and image columns increase eastward.
    const glm::vec3 north{1.0f, 0.0f, 0.0f};
    glm::vec3 up = north - glm::dot(north, forward) * forward;
    if (glm::dot(up, up) < 1.0e-12f) {
        const glm::vec3 worldUp{0.0f, 1.0f, 0.0f};
        up = worldUp - glm::dot(worldUp, forward) * forward;
    }
    up = glm::normalize(up);
    glm::vec3 right = glm::cross(forward, up);
    if (glm::dot(right, right) < 1.0e-12f) right = {0.0f, 0.0f, 1.0f};
    else right = glm::normalize(right);
    return glm::normalize(glm::cross(right, forward));
}

glm::vec3 perspectiveCameraUp(float zenithDegrees, float azimuthDegrees)
{
    const float zenith = zenithDegrees * DEG2RAD;
    const float azimuth = azimuthDegrees * DEG2RAD;
    const glm::vec3 cameraOut{
        std::sin(zenith) * std::cos(azimuth),
        std::cos(zenith),
        std::sin(zenith) * std::sin(azimuth)};
    const glm::vec3 forward = -glm::normalize(cameraOut);
    // Perspective imagery behaves like a forward-looking camera: keep the
    // horizon horizontal. At nadir, vertical-up is degenerate, so use north-up.
    const glm::vec3 worldUp{0.0f, 1.0f, 0.0f};
    glm::vec3 up = worldUp - glm::dot(worldUp, forward) * forward;
    if (glm::dot(up, up) < 1.0e-12f) {
        const glm::vec3 north{1.0f, 0.0f, 0.0f};
        up = north - glm::dot(north, forward) * forward;
    }
    up = glm::normalize(up);
    glm::vec3 right = glm::cross(forward, up);
    if (glm::dot(right, right) < 1.0e-12f) right = {0.0f, 0.0f, 1.0f};
    else right = glm::normalize(right);
    return glm::normalize(glm::cross(right, forward));
}

void transferToComputeBarrier(vk::CommandBuffer command, VkBuffer buffer, VkDeviceSize size)
{
    VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = buffer;
    barrier.offset = 0;
    barrier.size = size;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                         0, nullptr, 1, &barrier, 0, nullptr);
}

void solveNadirMapping(float imageWidth, float imageHeight,
                       float sceneWidth, float sceneDepth,
                       const glm::vec2& leftUpper, const glm::vec2& leftBottom,
                       const glm::vec2& rightUpper, const glm::vec2& rightBottom,
                       Eigen::VectorXd& cx, Eigen::VectorXd& cy)
{
    // Scale factor is fixed at 1.0: preserve the real scene aspect ratio and
    // fit it once into the nadir raster without the legacy 0.707 shrink.
    // Pixel indices address sample centres.  The raster boundaries therefore
    // lie at -0.5 and size-0.5.  Fitting scene corners to 0 and size-1 puts
    // the outer samples exactly on triangle boundaries and creates a ring of
    // false zero/no-hit pixels, most visibly as a nadir-angle dip.
    const double rasterWidth = std::max(1.0, static_cast<double>(imageWidth));
    const double rasterHeight = std::max(1.0, static_cast<double>(imageHeight));
    const double groundPerPixel = std::max(
        static_cast<double>(sceneDepth) / rasterWidth,
        static_cast<double>(sceneWidth) / rasterHeight);
    const double targetWidth = static_cast<double>(sceneDepth) / groundPerPixel
                               * kOrthographicScale;
    const double targetHeight = static_cast<double>(sceneWidth) / groundPerPixel
                                * kOrthographicScale;
    const double x0 = (rasterWidth - targetWidth) * 0.5 - 0.5;
    const double x1 = x0 + targetWidth;
    const double y0 = (rasterHeight - targetHeight) * 0.5 - 0.5;
    const double y1 = y0 + targetHeight;

    Eigen::MatrixXd design(4, 4);
    design << x1, y0, x1 * y0, 1.0,
              x1, y1, x1 * y1, 1.0,
              x0, y0, x0 * y0, 1.0,
              x0, y1, x0 * y1, 1.0;
    Eigen::VectorXd sourceX(4), sourceY(4);
    // Destination raster: top is north, right is east.
    sourceX << rightUpper.x, rightBottom.x, leftUpper.x, leftBottom.x;
    sourceY << rightUpper.y, rightBottom.y, leftUpper.y, leftBottom.y;
    cx = design.jacobiSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(sourceX);
    cy = design.jacobiSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(sourceY);
}

} // namespace


LightSet Geometry::createLight(float sza, float saa, float direct, float diffuse,float solarT,float skyT)
{
    LightSet light;
    if (sza == 0.0 || sza == 45.0) sza = sza + ANGLE_COR;

    // if (abs(saa - int((saa +ANGLE_COR)/ 45.0) * 45.0) < 0.05) saa = saa + ANGLE_COR;
    if ((saa - int((saa +ANGLE_COR) / 45.0) * 45.0) < 0.05 && (saa - int((saa +ANGLE_COR) / 45.0) * 45.0) > 0)// saa比45的倍数大一点
        saa = saa + ANGLE_COR * 2;
    if ((saa - int((saa +ANGLE_COR) / 45.0) * 45.0) > -0.05 && (saa - int((saa +ANGLE_COR) / 45.0) * 45.0) < 0)// saa比45的倍数小一点
        saa = saa - ANGLE_COR * 2;


    float r = SENSOR_HEIGHT;
    float rd = DEG2RAD;
    glm::vec3 origin = glm::vec3(0, 0, 0);
    // Azimuth is clockwise from north: +X is north and +Z is east.
    glm::vec3 lightPos = glm::vec3(r * std::sin(sza * rd) * std::cos(saa * rd),
                                   r * std::cos(sza * rd),
                                   r * std::sin(sza * rd) * std::sin(saa * rd));

    light.direction = lightPos;
    light.direct = direct;
    light.diffuse = diffuse;
    light.skyTemperature = skyT;
    light.solarTemperature = solarT;

    return light;
}

SensorMatrix Geometry::createSensor(glm::vec3 size, glm::vec3 origen, float vza, float vaa, float ratio) {

    // auto & sceneio = modelio->m_sceneio;

    SensorMatrix sensor;

    glm::vec3 semi = { size.x / 2.0, 0, size.z / 2.0 };
    glm::vec3 dimensionMin = -semi + glm::vec3{ origen.x, 0, origen.z };
    glm::vec3 dimensionMax = semi + glm::vec3{ origen.x, 0, origen.z };

    //float scale = m_pRaytracingXml->scene.stepSize;
    dimensionMin.y = 0;
    dimensionMax.y = 0;
    float r = SENSOR_HEIGHT;
    float rd = DEG2RAD;
    glm::vec3 origin = glm::vec3(0, 0, 0);
    glm::vec3 sensorPos = glm::vec3(r * std::sin(vza * rd) * std::cos(vaa * rd),
                                    r * std::cos(vza * rd),
                                    r * std::sin(vza * rd) * std::sin(vaa * rd));

    CameraManip.setFov(SENSOR_FOV);
    CameraManip.setLookat(sensorPos, origin, observationCameraUp(vza, vaa));
    float fovv = CameraManip.getFov();




    //CameraManip.fit(dimensionMin * ratio / scale, dimensionMax * ratio / scale); // the sensor position height is changed.
    CameraManip.fit(dimensionMin * ratio, dimensionMax * ratio); // the sensor position height is changed.


    float width = CameraManip.getWidth();
    float height = CameraManip.getHeight();
    const float aspectRatio = CameraManip.getWidth() / static_cast<float>(CameraManip.getHeight());
    glm::mat4 view = CameraManip.getMatrix();
    nvmath::mat4f projj = nvmath::perspectiveVK(CameraManip.getFov(), aspectRatio, 0.0001f, 10000.0f);
    // glm::mat4 proj = projglm::perspective(glm::radians(CameraManip.getFov()), aspectRatio, 0.01f, 1000.0f);
    glm::mat4 proj = glm::perspectiveRH_ZO(glm::radians(CameraManip.getFov()), aspectRatio, 0.01f, 10000.0f);
    proj[1][1] *= -1;
    sensor.viewInverse = glm::inverse(view);
    sensor.projInverse = glm::inverse(proj);
    glm::vec3 eye, center, currentUp;
    CameraManip.getLookat(eye, center, currentUp);                       // get sensor (eye) and center.

    float fov = CameraManip.getFov();
    sensor.focalDist = glm::length(center - eye);
    sensor.aperture = 0.0;
    sensor.direction = eye - center;


    //  sensor.n_wave = modelio->waves.size();





    return sensor;
}

SensorMatrix Geometry::createSensor(glm::vec3 sensorPos_XZY, glm::vec3 center_XZY,
                                    glm::vec3 sceneSize_XZY, float fovDegrees,
                                    glm::vec3 up) {

    // auto & sceneio = modelio->m_sceneio;

    SensorMatrix sensor;

    const float distance = std::max(0.01f, glm::length(sensorPos_XZY - center_XZY));
    const float aspect = std::max(0.01f, CameraManip.getWidth() /
                                           static_cast<float>(CameraManip.getHeight()));
    const float radius = std::max(0.01f, 0.5f * glm::length(sceneSize_XZY));
    const float requiredHalfHeight = radius / std::min(1.0f, aspect);
    const float fittedFov = glm::degrees(2.0f * std::atan(requiredHalfHeight / distance)) * 1.02f;
    CameraManip.setFov(std::clamp(fovDegrees > 0.0f ? fovDegrees : fittedFov, 0.1f, 120.0f));
    CameraManip.setLookat(sensorPos_XZY, center_XZY, up);
    float fovv = CameraManip.getFov();


    //CameraManip.fit(dimensionMin * ratio / scale, dimensionMax * ratio / scale); // the sensor position height is changed.
    //CameraManip.fit(dimensionMin * ratio, dimensionMax * ratio); // the sensor position height is changed.


    float width = CameraManip.getWidth();
    float height = CameraManip.getHeight();
    const float aspectRatio = CameraManip.getWidth() / static_cast<float>(CameraManip.getHeight());
    glm::mat4 view = CameraManip.getMatrix();
    nvmath::mat4f projj = nvmath::perspectiveVK(CameraManip.getFov(), aspectRatio, 0.0001f, 10000.0f);
    //glm::mat4 proj = glm::perspective(CameraManip.getFov(), aspectRatio, 0.0001f, 10000.0f);
    const float farPlane = std::max(10000.0f, distance + radius * 4.0f);
    glm::mat4 proj = glm::perspectiveRH_ZO(glm::radians(CameraManip.getFov()), aspectRatio, 0.01f, farPlane);
    proj[1][1] *= -1;
    sensor.viewInverse = glm::inverse(view);
    sensor.projInverse = glm::inverse(proj);
    glm::vec3 eye, center, currentUp;
    CameraManip.getLookat(eye, center, currentUp);                       // get sensor (eye) and center.

    float fov = CameraManip.getFov();
    sensor.focalDist = glm::length(center - eye);
    sensor.aperture = 0.0;
    sensor.direction = eye - center;


    //  sensor.n_wave = modelio->waves.size();





    return sensor;
}

void Geometry::configureSensor(const SensorXml& sensor, glm::vec3 sceneSize_XYZ,
                               float metresPerUnit) {
    const float scale = std::max(0.0001f, metresPerUnit);
    m_sensorMetresPerUnit = scale;
    m_sensorProjection = sensor.projection;
    m_sensorFov = std::clamp(sensor.sensorFov, 0.1f, 120.0f);
    m_sensorSceneSize_XZY = {sceneSize_XYZ.x / scale, sceneSize_XYZ.z / scale,
                             sceneSize_XYZ.y / scale};
    m_sensorPosition_XZY = {(sensor.position.x - sceneSize_XYZ.x * 0.5f) / scale,
                            sensor.position.z / scale,
                            (sensor.position.y - sceneSize_XYZ.y * 0.5f) / scale};
    m_sensorTarget_XZY = {0.0f, sceneSize_XYZ.z * 0.5f / scale, 0.0f};
}

SensorMatrix Geometry::createConfiguredSensor(glm::vec3 size_XZY, glm::vec3 origin_XZY,
                                               float vza, float vaa, float ratio) {
    if (m_sensorProjection == Projection::PERSPECTIVE) {
        return createPerspectiveSensor(m_sensorPosition_XZY, vza, vaa);
    }
    return createSensor(size_XZY, origin_XZY, vza, vaa, ratio);
}

SensorMatrix Geometry::createPerspectiveSensor(glm::vec3 position_XZY, float vza, float vaa) {
    const float zenith = std::clamp(vza, 0.0f, 89.999f) * DEG2RAD;
    const float azimuth = vaa * DEG2RAD;
    const glm::vec3 cameraOut{
        std::sin(zenith) * std::cos(azimuth),
        std::cos(zenith),
        std::sin(zenith) * std::sin(azimuth)};
    const float lookDistance = std::max(1.0f, glm::length(m_sensorSceneSize_XZY));
    return createSensor(position_XZY, position_XZY - cameraOut * lookDistance,
                        m_sensorSceneSize_XZY, m_sensorFov,
                        perspectiveCameraUp(vza, vaa));
}

glm::vec3 Geometry::sensorWorldToXzy(glm::vec3 position_XYZ) const {
    return {
        position_XYZ.x / m_sensorMetresPerUnit - m_sensorSceneSize_XZY.x * 0.5f,
        position_XYZ.z / m_sensorMetresPerUnit,
        position_XYZ.y / m_sensorMetresPerUnit - m_sensorSceneSize_XZY.z * 0.5f};
}


bool Geometry::createGeometry(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RaytracingIO> &modelio){

//    auto & sceneio = modelio->m_sceneio;


    //--------------------------------------------------------
    //--- Resolution
    //-------------------------------------------------------
    modelio->imageSize = fileio->m_pRaytracingXml->sensorxml.resolution;
    CameraManip.setWindowSize(modelio->imageSize.x, modelio->imageSize.y );
    //---------------------------------------------------------
    // Angles
    //---------------------------------------------------------
    float vza, vaa, sza, saa;
    LightXml lightxml = fileio->m_pRaytracingXml->lightxml;
    SensorXml sensorxml = fileio->m_pRaytracingXml->sensorxml;
    configureSensor(sensorxml, fileio->m_pRaytracingXml->scenexml.background.sceneSize, 1.0f);


    sza = lightxml.solarAngle[0];
    saa = lightxml.solarAngle[1];
    for (int i = 0; i < sensorxml.viewAngles.size(); i++)
    {
        vza = sensorxml.viewAngles[i][0];
        vaa = sensorxml.viewAngles[i][1];
        modelio->angles.emplace_back(Angle{vza, vaa, sza, saa});
    }
    //modelio->n_angle = sensorxml.viewAngles.size();
    //---------------------------------------------------------
    // Bands
    //---------------------------------------------------------
    modelio->waves = fileio->m_pRaytracingXml->sensorxml.waves;
    modelio->isUAVTrave = fileio->m_pRaytracingXml->settingxml.isUAVtrave
                          && !sensorxml.uavPoses.empty();
    modelio->uavposes = sensorxml.uavPoses;
    modelio->uavViewAzimuths = sensorxml.uavViewAzimuths;
    modelio->n_pos = static_cast<int>(modelio->uavposes.size());

    //---------------------------------------------------------
    // LIGHT AND SENSOR INI with Angle 0 and Band 0
    //---------------------------------------------------------
    vza = modelio->angles[0].vza;
    vaa = modelio->angles[0].vaa;
    sza = modelio->angles[0].sza;
    saa = modelio->angles[0].saa;
    modelio->sensor = createConfiguredSensor(modelio->voxelSize, modelio->voxelOrigin, vza, vaa, 1.0f);


    modelio->light = createLight(sza, saa, fileio->m_pRaytracingXml->lightxml.direct, fileio->m_pRaytracingXml->lightxml.diffuse,
                                      fileio->m_pRaytracingXml->lightxml.solarTemperature,
                                      fileio->m_pRaytracingXml->lightxml.skyTemperature);



    return true;

}

void Geometry::updateAngle(std::shared_ptr<RaytracingIO> &modelio, int kangle){

    Angle &angle = modelio->angles[kangle];
//    std::cout << "Angle Info:"
//              << "    vza_" << std::to_string(angles.x) << "    vaa_" << std::to_string(angles.y)
//              << "    sza_" << std::to_string(angles.z) << "    saa_" << std::to_string(angles.w) << std::endl;

    float ratio = 1.0;
    // ratio = 0.707;
    SensorMatrix sensorMatrix = createConfiguredSensor(modelio->voxelSize, modelio->voxelOrigin,
                                                       angle.vza, angle.vaa, ratio);
    updateSensor(modelio, sensorMatrix);

    LightSet lightSet = createLight(angle.sza, angle.saa,modelio->light.direct,
                                    modelio->light.diffuse,modelio->light.solarTemperature,
                                    modelio->light.skyTemperature);
    updateLight(modelio,lightSet);

}

void Geometry::updateSensorPos(std::shared_ptr<RaytracingIO> &modelio, int kpos) {
    if (kpos < 0 || kpos >= static_cast<int>(modelio->uavposes.size()) || modelio->angles.empty()) return;
    Angle& angle = modelio->angles.front();
    if (kpos < static_cast<int>(modelio->uavViewAzimuths.size())) angle.vaa = modelio->uavViewAzimuths[kpos];
    SensorMatrix sensor = createPerspectiveSensor(sensorWorldToXzy(modelio->uavposes[kpos]),
                                                  angle.vza, angle.vaa);
    updateSensor(modelio, sensor);
}

void Geometry::updateSensor( std::shared_ptr<RaytracingIO> &modelio, SensorMatrix &sensor){


    auto &m_pBufferSensor = modelio->m_pBufferSensor;
    auto &m_device = modelio->m_device;
    auto &m_queueIndex = modelio->m_queueIndex;

    nvvk::CommandPool cmdBufGet(m_device, m_queueIndex);
    vk::CommandBuffer cmdBuf = cmdBufGet.createCommandBuffer();
    vkCmdUpdateBuffer(cmdBuf, (*m_pBufferSensor).buffer, 0, sizeof(SensorMatrix), &sensor);
    transferToComputeBarrier(cmdBuf, (*m_pBufferSensor).buffer, sizeof(SensorMatrix));
    cmdBufGet.submitAndWait(cmdBuf);
}

void Geometry::updateLight(std::shared_ptr<RaytracingIO> &modelio, LightSet &light){


    auto &m_pBufferLight = modelio->m_pBufferLight;
    auto &m_device = modelio->m_device;
    auto &m_queueIndex = modelio->m_queueIndex;

    nvvk::CommandPool cmdBufGet(m_device, m_queueIndex);
    vk::CommandBuffer cmdBuf = cmdBufGet.createCommandBuffer();
    vkCmdUpdateBuffer(cmdBuf, (*m_pBufferLight).buffer, 0, sizeof(LightSet), &light);
    transferToComputeBarrier(cmdBuf, (*m_pBufferLight).buffer, sizeof(LightSet));
    cmdBufGet.submitAndWait(cmdBuf);
}


void Geometry::orthcorrect(std::shared_ptr<RaytracingIO> &modelio,float vza, float vaa,
                           Eigen::VectorXd &cx, Eigen::VectorXd &cy) {

    glm::vec3 size = modelio->voxelSize;
    glm::vec3 origen =modelio->voxelOrigin;


    glm::vec3 semi = { size.x / 2.0, 0, size.z / 2.0 };

    glm::vec3 dimensionMin = -semi + glm::vec3{ origen.x, 0, origen.z };
    glm::vec3 dimensionMax = semi + glm::vec3{ origen.x, 0, origen.z };
    //float scale = m_pRaytracingXml->scene.stepSize;
    dimensionMin.y = 0;
    dimensionMax.y = 0;
    float r = SENSOR_HEIGHT;
    float rd = DEG2RAD;


    float ratio = 1.0;


    //SensorMatrix sensor;
    glm::vec3 origin = glm::vec3(0, 0, 0);
    glm::vec3 sensorPos = glm::vec3(r * std::sin(vza * rd) * std::cos(vaa * rd),
                                    r * std::cos(vza * rd), r * std::sin(vza * rd) * std::sin(vaa * rd));
    CameraManip.setFov(SENSOR_FOV);
    CameraManip.setLookat(sensorPos, origin, observationCameraUp(vza, vaa));
    float fovv = CameraManip.getFov();
    //CameraManip.fit(dimensionMin * ratio / scale, dimensionMax * ratio / scale); // the sensor position height is changed.
    CameraManip.fit(dimensionMin * ratio, dimensionMax * ratio); // the sensor position height is changed.
    float width = CameraManip.getWidth();
    float height = CameraManip.getHeight();
    const float aspectRatio = CameraManip.getWidth() / static_cast<float>(CameraManip.getHeight());
    glm::mat4 view = CameraManip.getMatrix();
    nvmath::mat4f projj = nvmath::perspectiveVK(CameraManip.getFov(), aspectRatio, 0.0001f, 10000.0f);
    //glm::mat4 proj = glm::perspective(CameraManip.getFov(), aspectRatio, 0.0001f, 10000.0f);
    glm::mat4 proj = glm::perspectiveRH_ZO(glm::radians(CameraManip.getFov()), aspectRatio, 0.1f, 1000.0f);
    proj[1][1] *= -1;

    glm::vec4 leftupper,leftbottom,rightupper,rightbottom;
    leftupper = glm::vec4(dimensionMax.x,0,dimensionMin.z,1);
    leftbottom = glm::vec4(dimensionMin.x,0,dimensionMin.z,1);
    rightupper = glm::vec4(dimensionMax.x,0,dimensionMax.z,1);
    rightbottom = glm::vec4(dimensionMin.x,0,dimensionMax.z,1);

    glm::vec4 lu,lb,ru,rb;
    glm::vec2 lun,lbn,run,rbn;
    lu = proj * view * leftupper;
    lu = lu/lu.w;
    lun.x = (lu.x+1.0f)/2.0*width;
    lun.y = (lu.y+1.0f)/2.0*height;

    lb = proj * view * leftbottom;
    lb = lb/lb.w;
    lbn.x = (lb.x+1.0f)/2.0*width;
    lbn.y = (1.0f + lb.y)/2.0*height;

    ru = proj * view * rightupper;
    ru = ru/ru.w;
    run.x = (ru.x+1.0f)/2.0*width;
    run.y = (1.0f + ru.y)/2.0*height;


    rb = proj * view * rightbottom;
    rb = rb/rb.w;
    rbn.x = (rb.x+1.0f)/2.0*width;
    rbn.y = (1.0f + rb.y)/2.0*height;


    solveNadirMapping(width, height,
                      dimensionMax.x - dimensionMin.x,
                      dimensionMax.z - dimensionMin.z,
                      lun, lbn, run, rbn, cx, cy);
}



bool Geometry::createGeometry(std::shared_ptr<FileIO> &fileio, std::shared_ptr<VoxelebIO> &modelio){

//    auto & sceneio = modelio->m_sceneio;

    //--------------------------------------------------------
    //--- Resolution
    //-------------------------------------------------------
    modelio->imageSize = fileio->m_pVoxelebXml->sensorxml.resolution;
    CameraManip.setWindowSize(modelio->imageSize.x, modelio->imageSize.y );
    //---------------------------------------------------------
    // Angles
    //---------------------------------------------------------
    float vza, vaa, sza, saa;
    LightXml lightxml = fileio->m_pVoxelebXml->lightxml;
    SensorXml sensorxml = fileio->m_pVoxelebXml->sensorxml;
    configureSensor(sensorxml, fileio->m_pVoxelebXml->scenexml.background.sceneSize,
                    fileio->m_pVoxelebXml->scenexml.background.stepsize_surface);




    sza = lightxml.solarAngle[0];
    saa = lightxml.solarAngle[1];
    for (int i = 0; i < sensorxml.viewAngles.size(); i++)
    {
        vza = sensorxml.viewAngles[i][0];
        vaa = sensorxml.viewAngles[i][1];
        modelio->angles.emplace_back(Angle{vza, vaa, sza, saa});
    }
    //modelio->n_angle = sensorxml.viewAngles.size();
    //---------------------------------------------------------
    // Bands
    //---------------------------------------------------------
    modelio->waves = fileio->m_pVoxelebXml->sensorxml.waves;
    modelio->isUAVTrave = fileio->m_pVoxelebXml->settingxml.isUAVtrave
                          && !sensorxml.uavPoses.empty();
    modelio->uavposes = sensorxml.uavPoses;
    modelio->uavViewAzimuths = sensorxml.uavViewAzimuths;
    modelio->n_pos = static_cast<int>(modelio->uavposes.size());

    //---------------------------------------------------------
    // LIGHT AND SENSOR INI with Angle 0 and Band 0
    //---------------------------------------------------------
    vza = modelio->angles[0].vza;
    vaa = modelio->angles[0].vaa;
    sza = modelio->angles[0].sza;
    saa = modelio->angles[0].saa;
    modelio->sensor = createConfiguredSensor(modelio->voxelSize_XZY, modelio->voxelOrigin_XZY, vza, vaa, 1.0f);
    modelio->light = createLight(sza, saa, fileio->m_pVoxelebXml->lightxml.direct, fileio->m_pVoxelebXml->lightxml.diffuse,
                                 fileio->m_pVoxelebXml->lightxml.solarTemperature,
                                 fileio->m_pVoxelebXml->lightxml.skyTemperature);



    return true;

}

void Geometry::updateAngle(std::shared_ptr<VoxelebIO> &modelio, int kangle){

    Angle &angle = modelio->angles[kangle];
//    std::cout << "Angle Info:"
//              << "    vza_" << std::to_string(angles.x) << "    vaa_" << std::to_string(angles.y)
//              << "    sza_" << std::to_string(angles.z) << "    saa_" << std::to_string(angles.w) << std::endl;

    updateSolarAngle(modelio,angle);

    float ratio = 1.0;
    //ratio = 0.707;
    SensorMatrix sensorMatrix = createConfiguredSensor(modelio->voxelSize_XZY, modelio->voxelOrigin_XZY,
                                                       angle.vza, angle.vaa, ratio);
    updateSensor(modelio, sensorMatrix);

    // if (angle.sza >= 85) angle.sza = 85;
    LightSet lightSet = createLight(angle.sza, angle.saa,modelio->light.direct,
                                    modelio->light.diffuse,modelio->light.solarTemperature,
                                    modelio->light.skyTemperature);
    updateLight(modelio,lightSet);

}

void Geometry::updateSensorPos(std::shared_ptr<VoxelebIO> &modelio, int kpos) {
    if (kpos < 0 || kpos >= static_cast<int>(modelio->uavposes.size()) || modelio->angles.empty()) return;
    Angle& angle = modelio->angles.front();
    if (kpos < static_cast<int>(modelio->uavViewAzimuths.size())) angle.vaa = modelio->uavViewAzimuths[kpos];
    SensorMatrix sensor = createPerspectiveSensor(sensorWorldToXzy(modelio->uavposes[kpos]),
                                                  angle.vza, angle.vaa);
    updateSensor(modelio, sensor);
}

void Geometry::updateSensor( std::shared_ptr<VoxelebIO> &modelio, SensorMatrix &sensor){


    auto &m_pBufferSensor = modelio->m_pBufferSensor;
    auto &m_device = modelio->m_device;
    auto &m_queueIndex = modelio->m_queueIndex;

    nvvk::CommandPool cmdBufGet(m_device, m_queueIndex);
    vk::CommandBuffer cmdBuf = cmdBufGet.createCommandBuffer();
    vkCmdUpdateBuffer(cmdBuf, (*m_pBufferSensor).buffer, 0, sizeof(SensorMatrix), &sensor);
    transferToComputeBarrier(cmdBuf, (*m_pBufferSensor).buffer, sizeof(SensorMatrix));
    cmdBufGet.submitAndWait(cmdBuf);
}

void Geometry::updateLight(std::shared_ptr<VoxelebIO> &modelio, LightSet &light){


    auto &m_pBufferLight = modelio->m_pBufferLight;
    auto &m_device = modelio->m_device;
    auto &m_queueIndex = modelio->m_queueIndex;

    nvvk::CommandPool cmdBufGet(m_device, m_queueIndex);
    vk::CommandBuffer cmdBuf = cmdBufGet.createCommandBuffer();
    vkCmdUpdateBuffer(cmdBuf, (*m_pBufferLight).buffer, 0, sizeof(LightSet), &light);
    transferToComputeBarrier(cmdBuf, (*m_pBufferLight).buffer, sizeof(LightSet));
    cmdBufGet.submitAndWait(cmdBuf);
}

void Geometry::updateSolarAngle(std::shared_ptr<VoxelebIO> &modelio, Angle &angle) {
    SPACalc spa;
    spa_data data;


    int kmonth,kday;
    float t = modelio->meteo.t;
    int kdoy = floor(t);
    int h = floor((t - kdoy)*24);
    int m = floor(((t - kdoy)*24 - h)*60);
    Utils::calculateMonthAndDay(modelio->m_year, kdoy, &kmonth, &kday);

    data.year = modelio->m_year;
    data.month = kmonth;
    data.day = kday;
    data.hour = h;
    data.minute = m;
    data.second = 0;
    data.timezone = 8;
    data.pressure = std::max(100.0f, modelio->meteo.p);
    data.temperature = modelio->meteo.Ta;
    data.delta_t = Utils::calculateDeltaT(data.year, data.month);
    data.longitude = modelio->lon;
    data.latitude = modelio->lat;
    data.atmos_refract = 0.5667;
    data.elevation = 100;
    data.slope = 0;
    data.azm_rotation = 0;
    data.function = SPA_ZA;

    spa.spa_calculate(&data);
    angle.sza = data.zenith;
    angle.saa = data.azimuth;

}

void Geometry::orthcorrect(std::shared_ptr<VoxelebIO> &modelio,float vza, float vaa,
                           Eigen::VectorXd &cx, Eigen::VectorXd &cy) {

    glm::vec3 size = modelio->voxelSize_XZY;
    glm::vec3 origen =modelio->voxelOrigin_XZY;
    glm::vec3 semi = { size.x / 2.0, 0, size.z / 2.0 };
    glm::vec3 dimensionMin = -semi + glm::vec3{ origen.x, 0, origen.z };
    glm::vec3 dimensionMax = semi + glm::vec3{ origen.x, 0, origen.z };
    //float scale = m_pRaytracingXml->scene.stepSize;
    dimensionMin.y = 0;
    dimensionMax.y = 0;
    float r = SENSOR_HEIGHT;
    float rd = DEG2RAD;


    float ratio = 1.0;


    //SensorMatrix sensor;
    glm::vec3 origin = glm::vec3(0, 0, 0);
    glm::vec3 sensorPos = glm::vec3(r * std::sin(vza * rd) * std::cos(vaa * rd),
                                    r * std::cos(vza * rd),
                                    r * std::sin(vza * rd) * std::sin(vaa * rd));
    CameraManip.setFov(SENSOR_FOV);
    CameraManip.setLookat(sensorPos, origin, observationCameraUp(vza, vaa));
    float fovv = CameraManip.getFov();
    //CameraManip.fit(dimensionMin * ratio / scale, dimensionMax * ratio / scale); // the sensor position height is changed.
    CameraManip.fit(dimensionMin * ratio, dimensionMax * ratio); // the sensor position height is changed.
    float width = CameraManip.getWidth();
    float height = CameraManip.getHeight();
    const float aspectRatio = CameraManip.getWidth() / static_cast<float>(CameraManip.getHeight());
    glm::mat4 view = CameraManip.getMatrix();
    nvmath::mat4f projj = nvmath::perspectiveVK(CameraManip.getFov(), aspectRatio, 0.0001f, 10000.0f);
    //glm::mat4 proj = glm::perspective(CameraManip.getFov(), aspectRatio, 0.0001f, 10000.0f);
    glm::mat4 proj = glm::perspectiveRH_ZO(glm::radians(CameraManip.getFov()), aspectRatio, 0.1f, 1000.0f);
    proj[1][1] *= -1;

    glm::vec4 leftupper,leftbottom,rightupper,rightbottom;
    leftupper = glm::vec4(dimensionMax.x,0,dimensionMin.z,1);
    leftbottom = glm::vec4(dimensionMin.x,0,dimensionMin.z,1);
    rightupper = glm::vec4(dimensionMax.x,0,dimensionMax.z,1);
    rightbottom = glm::vec4(dimensionMin.x,0,dimensionMax.z,1);

    glm::vec4 lu,lb,ru,rb;
    glm::vec2 lun,lbn,run,rbn;
    lu = proj * view * leftupper;
    lu = lu/lu.w;
    lun.x = (lu.x+1.0f)/2.0*width;
    lun.y = (lu.y+1.0f)/2.0*height;

    lb = proj * view * leftbottom;
    lb = lb/lb.w;
    lbn.x = (lb.x+1.0f)/2.0*width;
    lbn.y = (1.0f + lb.y)/2.0*height;

    ru = proj * view * rightupper;
    ru = ru/ru.w;
    run.x = (ru.x+1.0f)/2.0*width;
    run.y = (1.0f + ru.y)/2.0*height;


    rb = proj * view * rightbottom;
    rb = rb/rb.w;
    rbn.x = (rb.x+1.0f)/2.0*width;
    rbn.y = (1.0f + rb.y)/2.0*height;


    solveNadirMapping(width, height,
                      dimensionMax.x - dimensionMin.x,
                      dimensionMax.z - dimensionMin.z,
                      lun, lbn, run, rbn, cx, cy);
}


bool Geometry::createGeometry(std::shared_ptr<FileIO> &fileio, std::shared_ptr<VoxelrtIO> &modelio){

//    auto & sceneio = modelio->m_sceneio;

    //--------------------------------------------------------
    //--- Resolution
    //-------------------------------------------------------
    modelio->imageSize = fileio->m_pVoxelrtXml->sensorxml.resolution;
    CameraManip.setWindowSize(modelio->imageSize.x, modelio->imageSize.y );
    //---------------------------------------------------------
    // Angles
    //---------------------------------------------------------
    float vza, vaa, sza, saa;
    LightXml lightxml = fileio->m_pVoxelrtXml->lightxml;
    SensorXml sensorxml = fileio->m_pVoxelrtXml->sensorxml;
    configureSensor(sensorxml, fileio->m_pVoxelrtXml->scenexml.background.sceneSize,
                    fileio->m_pVoxelrtXml->scenexml.background.stepsize_surface);




    sza = lightxml.solarAngle[0];
    saa = lightxml.solarAngle[1];
    for (int i = 0; i < sensorxml.viewAngles.size(); i++)
    {
        vza = sensorxml.viewAngles[i][0];
        vaa = sensorxml.viewAngles[i][1];
        modelio->angles.emplace_back(Angle{vza, vaa, sza, saa});
    }
    //modelio->n_angle = sensorxml.viewAngles.size();
    //---------------------------------------------------------
    // Bands
    //---------------------------------------------------------
    modelio->waves = fileio->m_pVoxelrtXml->sensorxml.waves;

    //---------------------------------------------------------
    // LIGHT AND SENSOR INI with Angle 0 and Band 0
    //---------------------------------------------------------
    vza = modelio->angles[0].vza;
    vaa = modelio->angles[0].vaa;
    sza = modelio->angles[0].sza;
    saa = modelio->angles[0].saa;

    modelio->isUAVTrave = fileio->m_pVoxelrtXml->settingxml.isUAVtrave
                          && !fileio->m_pVoxelrtXml->sensorxml.uavPoses.empty();
    modelio->n_pos = fileio->m_pVoxelrtXml->sensorxml.uavPoses.size();
    modelio->uavposes = fileio->m_pVoxelrtXml->sensorxml.uavPoses;
    modelio->uavViewAzimuths = fileio->m_pVoxelrtXml->sensorxml.uavViewAzimuths;

//    if(modelio->isUAVTrave == true){
//        modelio->sensor = createSensor(glm::vec3(0,10,1),glm::vec3(0,0,0));}
//    else {
//        modelio->sensor = createSensor(modelio->voxelSize_XZY, modelio->voxelOrigin_XZY, vza, vaa, 1.0);
//    }
    modelio->sensor = createConfiguredSensor(modelio->voxelSize_XZY, modelio->voxelOrigin_XZY, vza, vaa, 1.0f);
    modelio->light = createLight(sza, saa, fileio->m_pVoxelrtXml->lightxml.direct,
                                 fileio->m_pVoxelrtXml->lightxml.diffuse,
                                 fileio->m_pVoxelrtXml->lightxml.solarTemperature,
                                 fileio->m_pVoxelrtXml->lightxml.skyTemperature);



    return true;

}

void Geometry::updateAngle(std::shared_ptr<VoxelrtIO> &modelio, int kangle){

    Angle &angle = modelio->angles[kangle];
//    std::cout << "Angle Info:"
//              << "    vza_" << std::to_string(angles.x) << "    vaa_" << std::to_string(angles.y)
//              << "    sza_" << std::to_string(angles.z) << "    saa_" << std::to_string(angles.w) << std::endl;

    float ratio = 1.0;
    //ratio = 0.707;
    SensorMatrix sensorMatrix;
    if(modelio->isUAVTrave == false) {

        sensorMatrix = createConfiguredSensor(modelio->voxelSize_XZY, modelio->voxelOrigin_XZY,
                                                   angle.vza, angle.vaa, ratio);
    }else {
        sensorMatrix = createSensor(glm::vec3(0, 1000, 1), glm::vec3(0, 900, 0), m_sensorSceneSize_XZY);
    }
    updateSensor(modelio, sensorMatrix);

    LightSet lightSet = createLight(angle.sza, angle.saa,modelio->light.direct,
                                    modelio->light.diffuse,modelio->light.solarTemperature,
                                    modelio->light.skyTemperature);
    updateLight(modelio,lightSet);

}

void Geometry::updateSensorPos(std::shared_ptr<VoxelrtIO> &modelio, int kpos){
    if (kpos < 0 || kpos >= static_cast<int>(modelio->uavposes.size()) || modelio->angles.empty()) return;
    Angle& angle = modelio->angles.front();
    if (kpos < static_cast<int>(modelio->uavViewAzimuths.size())) angle.vaa = modelio->uavViewAzimuths[kpos];
    SensorMatrix sensorMatrix = createPerspectiveSensor(sensorWorldToXzy(modelio->uavposes[kpos]),
                                                        angle.vza, angle.vaa);

    updateSensor(modelio, sensorMatrix);

    LightSet lightSet = createLight(angle.sza, angle.saa,modelio->light.direct,
                                    modelio->light.diffuse,modelio->light.solarTemperature,
                                    modelio->light.skyTemperature);
    updateLight(modelio,lightSet);

}

void Geometry::updateSensor( std::shared_ptr<VoxelrtIO> &modelio, SensorMatrix &sensor){


    auto &m_pBufferSensor = modelio->m_pBufferSensor;
    auto &m_device = modelio->m_device;
    auto &m_queueIndex = modelio->m_queueIndex;

    nvvk::CommandPool cmdBufGet(m_device, m_queueIndex);
    vk::CommandBuffer cmdBuf = cmdBufGet.createCommandBuffer();
    vkCmdUpdateBuffer(cmdBuf, (*m_pBufferSensor).buffer, 0, sizeof(SensorMatrix), &sensor);
    cmdBufGet.submitAndWait(cmdBuf);
}

void Geometry::updateLight(std::shared_ptr<VoxelrtIO> &modelio, LightSet &light){


    auto &m_pBufferLight = modelio->m_pBufferLight;
    auto &m_device = modelio->m_device;
    auto &m_queueIndex = modelio->m_queueIndex;

    nvvk::CommandPool cmdBufGet(m_device, m_queueIndex);
    vk::CommandBuffer cmdBuf = cmdBufGet.createCommandBuffer();
    vkCmdUpdateBuffer(cmdBuf, (*m_pBufferLight).buffer, 0, sizeof(LightSet), &light);
    cmdBufGet.submitAndWait(cmdBuf);
}

void Geometry::orthcorrect(std::shared_ptr<VoxelrtIO> &modelio,float vza, float vaa,
                           Eigen::VectorXd &cx, Eigen::VectorXd &cy) {

    glm::vec3 size = modelio->voxelSize_XZY;
    glm::vec3 origen =modelio->voxelOrigin_XZY;
    glm::vec3 semi = { size.x / 2.0, 0, size.z / 2.0 };
    glm::vec3 dimensionMin = -semi + glm::vec3{ origen.x, 0, origen.z };
    glm::vec3 dimensionMax = semi + glm::vec3{ origen.x, 0, origen.z };
    //float scale = m_pRaytracingXml->scene.stepSize;
    dimensionMin.y = 0;
    dimensionMax.y = 0;
    float r = SENSOR_HEIGHT;
    float rd = DEG2RAD;


    float ratio = 1.0;


    //SensorMatrix sensor;
    glm::vec3 origin = glm::vec3(0, 0, 0);
    glm::vec3 sensorPos = glm::vec3(r * std::sin(vza * rd) * std::cos(vaa * rd),
                                    r * std::cos(vza * rd),
                                    r * std::sin(vza * rd) * std::sin(vaa * rd));
    CameraManip.setFov(SENSOR_FOV);
    CameraManip.setLookat(sensorPos, origin, observationCameraUp(vza, vaa));
    float fovv = CameraManip.getFov();
    //CameraManip.fit(dimensionMin * ratio / scale, dimensionMax * ratio / scale); // the sensor position height is changed.
    CameraManip.fit(dimensionMin * ratio, dimensionMax * ratio); // the sensor position height is changed.
    float width = CameraManip.getWidth();
    float height = CameraManip.getHeight();
    const float aspectRatio = CameraManip.getWidth() / static_cast<float>(CameraManip.getHeight());
    glm::mat4 view = CameraManip.getMatrix();
    nvmath::mat4f projj = nvmath::perspectiveVK(CameraManip.getFov(), aspectRatio, 0.0001f, 10000.0f);
    //glm::mat4 proj = glm::perspective(CameraManip.getFov(), aspectRatio, 0.0001f, 10000.0f);
    glm::mat4 proj = glm::perspectiveRH_ZO(glm::radians(CameraManip.getFov()), aspectRatio, 0.1f, 1000.0f);
    proj[1][1] *= -1;

    glm::vec4 leftupper,leftbottom,rightupper,rightbottom;
    leftupper = glm::vec4(dimensionMax.x,0,dimensionMin.z,1);
    leftbottom = glm::vec4(dimensionMin.x,0,dimensionMin.z,1);
    rightupper = glm::vec4(dimensionMax.x,0,dimensionMax.z,1);
    rightbottom = glm::vec4(dimensionMin.x,0,dimensionMax.z,1);

    glm::vec4 lu,lb,ru,rb;
    glm::vec2 lun,lbn,run,rbn;
    lu = proj * view * leftupper;
    lu = lu/lu.w;
    lun.x = (lu.x+1.0f)/2.0*width;
    lun.y = (lu.y+1.0f)/2.0*height;

    lb = proj * view * leftbottom;
    lb = lb/lb.w;
    lbn.x = (lb.x+1.0f)/2.0*width;
    lbn.y = (1.0f + lb.y)/2.0*height;

    ru = proj * view * rightupper;
    ru = ru/ru.w;
    run.x = (ru.x+1.0f)/2.0*width;
    run.y = (1.0f + ru.y)/2.0*height;


    rb = proj * view * rightbottom;
    rb = rb/rb.w;
    rbn.x = (rb.x+1.0f)/2.0*width;
    rbn.y = (1.0f + rb.y)/2.0*height;


    solveNadirMapping(width, height,
                      dimensionMax.x - dimensionMin.x,
                      dimensionMax.z - dimensionMin.z,
                      lun, lbn, run, rbn, cx, cy);
}
