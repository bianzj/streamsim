//
// Created by admin on 2024/1/24.
//

#include "geometry.h"


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
    if (vza == 0.0 || vza == 45.0) vza = vza + ANGLE_COR;

    if ((vaa - int((vaa +ANGLE_COR) / 45.0) * 45.0) < 0.05 && (vaa - int((vaa +ANGLE_COR) / 45.0) * 45.0) > 0)// vaa比45的倍数大一点
        vaa = vaa + ANGLE_COR * 2;
    if ((vaa - int((vaa +ANGLE_COR) / 45.0) * 45.0) > -0.05 && (vaa - int((vaa +ANGLE_COR) / 45.0) * 45.0) < 0)// vaa比45的倍数小一点
        vaa = vaa - ANGLE_COR * 2;

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
    CameraManip.setLookat(sensorPos, origin, glm::vec3(0, 1, 0));
    float fovv = CameraManip.getFov();




    //CameraManip.fit(dimensionMin * ratio / scale, dimensionMax * ratio / scale); // the sensor position height is changed.
    CameraManip.fit(dimensionMin * ratio, dimensionMax * ratio); // the sensor position height is changed.


    float width = CameraManip.getWidth();
    float height = CameraManip.getHeight();
    const float aspectRatio = CameraManip.getWidth() / static_cast<float>(CameraManip.getHeight());
    glm::mat4 view = CameraManip.getMatrix();
    nvmath::mat4f projj = nvmath::perspectiveVK(CameraManip.getFov(), aspectRatio, 0.0001f, 10000.0f);
    // glm::mat4 proj = projglm::perspective(glm::radians(CameraManip.getFov()), aspectRatio, 0.01f, 1000.0f);
    glm::mat4 proj = glm::perspectiveRH_ZO(glm::radians(CameraManip.getFov()), aspectRatio, 0.1f, 1000.0f);
    proj[1][1] *= -1;
    sensor.viewInverse = glm::inverse(view);
    sensor.projInverse = glm::inverse(proj);
    glm::vec3 eye, center,up;
    CameraManip.getLookat(eye, center, up);                             // get sensor (eye) and center.

    float fov = CameraManip.getFov();
    sensor.focalDist = glm::length(center - eye);
    sensor.aperture = 0.0;
    sensor.direction = eye - center;


    //  sensor.n_wave = modelio->waves.size();





    return sensor;
}

SensorMatrix Geometry::createSensor(glm::vec3 sensorPos_XZY, glm::vec3 center_XZY) {

    // auto & sceneio = modelio->m_sceneio;

    SensorMatrix sensor;

    float r = SENSOR_HEIGHT;
    float rd = DEG2RAD;
    glm::vec3 origin = glm::vec3(0, 0, 0);
//    CameraManip.setFov(SENSOR_FOV);
    CameraManip.setLookat(sensorPos_XZY, center_XZY, glm::vec3(0, 1, 0));
    float fovv = CameraManip.getFov();


    //CameraManip.fit(dimensionMin * ratio / scale, dimensionMax * ratio / scale); // the sensor position height is changed.
    //CameraManip.fit(dimensionMin * ratio, dimensionMax * ratio); // the sensor position height is changed.


    float width = CameraManip.getWidth();
    float height = CameraManip.getHeight();
    const float aspectRatio = CameraManip.getWidth() / static_cast<float>(CameraManip.getHeight());
    glm::mat4 view = CameraManip.getMatrix();
    nvmath::mat4f projj = nvmath::perspectiveVK(CameraManip.getFov(), aspectRatio, 0.0001f, 10000.0f);
    //glm::mat4 proj = glm::perspective(CameraManip.getFov(), aspectRatio, 0.0001f, 10000.0f);
    glm::mat4 proj = glm::perspectiveRH_ZO(glm::radians(CameraManip.getFov()), aspectRatio, 0.1f, 1000.0f);
    proj[1][1] *= -1;
    sensor.viewInverse = glm::inverse(view);
    sensor.projInverse = glm::inverse(proj);
    glm::vec3 eye, center,up;
    CameraManip.getLookat(eye, center, up);                             // get sensor (eye) and center.

    float fov = CameraManip.getFov();
    sensor.focalDist = glm::length(center - eye);
    sensor.aperture = 0.0;
    sensor.direction = eye - center;


    //  sensor.n_wave = modelio->waves.size();





    return sensor;
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

    //---------------------------------------------------------
    // LIGHT AND SENSOR INI with Angle 0 and Band 0
    //---------------------------------------------------------
    vza = modelio->angles[0].vza;
    vaa = modelio->angles[0].vaa;
    sza = modelio->angles[0].sza;
    saa = modelio->angles[0].saa;
    modelio->sensor = createSensor(modelio->voxelSize,modelio->voxelOrigin, vza, vaa, 1.0);


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
    SensorMatrix sensorMatrix = createSensor(modelio->voxelSize,modelio->voxelOrigin,
                                             angle.vza, angle.vaa, ratio);
    updateSensor(modelio, sensorMatrix);

    LightSet lightSet = createLight(angle.sza, angle.saa,modelio->light.direct,
                                    modelio->light.diffuse,modelio->light.solarTemperature,
                                    modelio->light.skyTemperature);
    updateLight(modelio,lightSet);

}

void Geometry::updateSensor( std::shared_ptr<RaytracingIO> &modelio, SensorMatrix &sensor){


    auto &m_pBufferSensor = modelio->m_pBufferSensor;
    auto &m_device = modelio->m_device;
    auto &m_queueIndex = modelio->m_queueIndex;

    nvvk::CommandPool cmdBufGet(m_device, m_queueIndex);
    vk::CommandBuffer cmdBuf = cmdBufGet.createCommandBuffer();
    vkCmdUpdateBuffer(cmdBuf, (*m_pBufferSensor).buffer, 0, sizeof(SensorMatrix), &sensor);
    cmdBufGet.submitAndWait(cmdBuf);
}

void Geometry::updateLight(std::shared_ptr<RaytracingIO> &modelio, LightSet &light){


    auto &m_pBufferLight = modelio->m_pBufferLight;
    auto &m_device = modelio->m_device;
    auto &m_queueIndex = modelio->m_queueIndex;

    nvvk::CommandPool cmdBufGet(m_device, m_queueIndex);
    vk::CommandBuffer cmdBuf = cmdBufGet.createCommandBuffer();
    vkCmdUpdateBuffer(cmdBuf, (*m_pBufferLight).buffer, 0, sizeof(LightSet), &light);
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
    if (vza == 0.0 || vza == 45.0) vza = vza + ANGLE_COR;
    glm::vec3 origin = glm::vec3(0, 0, 0);
    glm::vec3 sensorPos = glm::vec3(r * std::sin(vza * rd) * std::cos(vaa * rd),
                                    r * std::cos(vza * rd), r * std::sin(vza * rd) * std::sin(vaa * rd));
    CameraManip.setFov(SENSOR_FOV);
    CameraManip.setLookat(sensorPos, origin, glm::vec3(0, 1, 0));
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
    leftupper = glm::vec4(dimensionMin.x,0,dimensionMax.z,1);
    leftbottom = glm::vec4(dimensionMin.x,0,dimensionMin.z,1);
    rightupper = glm::vec4(dimensionMax.x,0,dimensionMax.z,1);
    rightbottom = glm::vec4(dimensionMax.x,0,dimensionMin.z,1);

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


    float vza0 = 0;
    float vaa0 = 0;
    ratio = 0.707;
    if (vza0 == 0.0 || vza0 == 45.0) vza0 = vza0 + ANGLE_COR;
    glm::vec3 origin0 = glm::vec3(0, 0, 0);
    glm::vec3 sensorPos0 = glm::vec3(r * std::sin(vza0 * rd) * std::cos(vaa0 * rd),
                                     r * std::cos(vza0 * rd), r * std::sin(vza0 * rd) * std::sin(vaa0 * rd));
    CameraManip.setFov(SENSOR_FOV);
    CameraManip.setLookat(sensorPos0, origin0, glm::vec3(0, 1, 0));
    float fovv0 = CameraManip.getFov();
    //CameraManip.fit(dimensionMin * ratio / scale, dimensionMax * ratio / scale); // the sensor position height is changed.
    CameraManip.fit(dimensionMin * ratio, dimensionMax * ratio); // the sensor position height is changed.

    glm::mat4 view0 = CameraManip.getMatrix();
    // nvmath::mat4f projj0 = nvmath::perspectiveVK(CameraManip.getFov(), aspectRatio, 0.0001f, 10000.0f);
    //glm::mat4 proj = glm::perspective(CameraManip.getFov(), aspectRatio, 0.0001f, 10000.0f);
    glm::mat4 proj0 = glm::perspectiveRH_ZO(glm::radians(CameraManip.getFov()), aspectRatio, 0.1f, 1000.0f);
    proj0[1][1] *= -1;
    glm::vec2 lu0,lb0,ru0,rb0;
    lu = proj0 * view0 * leftupper;
    lu = lu/lu.w;
    lu0.x = (lu.x+1.0f)/2.0*width;
    lu0.y = (1.0f + lu.y)/2.0*height;

    lb = proj0 * view0 * leftbottom;
    lb = lb/lb.w;
    lb0.x = (lb.x+1.0f)/2.0*width;
    lb0.y = (1.0f + lb.y)/2.0*height;

    ru = proj0 * view0 * rightupper;
    ru = ru/ru.w;
    ru0.x = (ru.x+1.0f)/2.0*width;
    ru0.y = (1.0f + ru.y)/2.0*height;


    rb = proj0 * view0 * rightbottom;
    rb = rb/rb.w;
    rb0.x = (rb.x+1.0f)/2.0*width;
    rb0.y = (1.0f + rb.y)/2.0*height;




    Eigen::VectorXd xx(4),yy(4),xx0(4),yy0(4);
    xx0 << rb0.x,ru0.x,lb0.x,lu0.x;
    yy0 << rb0.y,ru0.y,lb0.y,lu0.y;
    xx << rbn.x,run.x,lbn.x,lun.x;
    yy << rbn.y,run.y,lbn.y,lun.y;
    Eigen::MatrixXd ww(4,4);
    ww << rb0.x, rb0.y, rb0.x*rb0.y,1,
            ru0.x, ru0.y, ru0.x*ru0.y,1,
            lb0.x, lb0.y, lb0.x*lb0.y,1,
            lu0.x, lu0.y, lu0.x*lu0.y,1;

    cx = ww.jacobiSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(xx);
    cy = ww.jacobiSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(yy);


    int aa = 0;
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

    //---------------------------------------------------------
    // LIGHT AND SENSOR INI with Angle 0 and Band 0
    //---------------------------------------------------------
    vza = modelio->angles[0].vza;
    vaa = modelio->angles[0].vaa;
    sza = modelio->angles[0].sza;
    saa = modelio->angles[0].saa;
    modelio->sensor = createSensor(modelio->voxelSize_XZY, modelio->voxelOrigin_XZY, vza, vaa, 1.0);
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
    SensorMatrix sensorMatrix = createSensor(modelio->voxelSize_XZY, modelio->voxelOrigin_XZY,
                                             angle.vza, angle.vaa, ratio);
    updateSensor(modelio, sensorMatrix);

    // if (angle.sza >= 85) angle.sza = 85;
    LightSet lightSet = createLight(angle.sza, angle.saa,modelio->light.direct,
                                    modelio->light.diffuse,modelio->light.solarTemperature,
                                    modelio->light.skyTemperature);
    updateLight(modelio,lightSet);

}

void Geometry::updateSensor( std::shared_ptr<VoxelebIO> &modelio, SensorMatrix &sensor){


    auto &m_pBufferSensor = modelio->m_pBufferSensor;
    auto &m_device = modelio->m_device;
    auto &m_queueIndex = modelio->m_queueIndex;

    nvvk::CommandPool cmdBufGet(m_device, m_queueIndex);
    vk::CommandBuffer cmdBuf = cmdBufGet.createCommandBuffer();
    vkCmdUpdateBuffer(cmdBuf, (*m_pBufferSensor).buffer, 0, sizeof(SensorMatrix), &sensor);
    cmdBufGet.submitAndWait(cmdBuf);
}

void Geometry::updateLight(std::shared_ptr<VoxelebIO> &modelio, LightSet &light){


    auto &m_pBufferLight = modelio->m_pBufferLight;
    auto &m_device = modelio->m_device;
    auto &m_queueIndex = modelio->m_queueIndex;

    nvvk::CommandPool cmdBufGet(m_device, m_queueIndex);
    vk::CommandBuffer cmdBuf = cmdBufGet.createCommandBuffer();
    vkCmdUpdateBuffer(cmdBuf, (*m_pBufferLight).buffer, 0, sizeof(LightSet), &light);
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
    data.pressure = 800;
    data.temperature = 25;
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
    if (vza == 0.0 || vza == 45.0) vza = vza + ANGLE_COR;
    glm::vec3 origin = glm::vec3(0, 0, 0);
    glm::vec3 sensorPos = glm::vec3(r * std::sin(vza * rd) * std::cos(vaa * rd),
                                    r * std::cos(vza * rd),
                                    r * std::sin(vza * rd) * std::sin(vaa * rd));
    CameraManip.setFov(SENSOR_FOV);
    CameraManip.setLookat(sensorPos, origin, glm::vec3(0, 1, 0));
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
    leftupper = glm::vec4(dimensionMin.x,0,dimensionMax.z,1);
    leftbottom = glm::vec4(dimensionMin.x,0,dimensionMin.z,1);
    rightupper = glm::vec4(dimensionMax.x,0,dimensionMax.z,1);
    rightbottom = glm::vec4(dimensionMax.x,0,dimensionMin.z,1);

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


    float vza0 = 0;
    float vaa0 = 0;
    ratio = 0.707;
    if (vza0 == 0.0 || vza0 == 45.0) vza0 = vza0 + ANGLE_COR;
    glm::vec3 origin0 = glm::vec3(0, 0, 0);
    glm::vec3 sensorPos0 = glm::vec3(r * std::sin(vza0 * rd) * std::cos(vaa0 * rd),
                                     r * std::cos(vza0 * rd),
                                     r * std::sin(vza0 * rd) * std::sin(vaa0 * rd));
    CameraManip.setFov(SENSOR_FOV);
    CameraManip.setLookat(sensorPos0, origin0, glm::vec3(0, 1, 0));
    float fovv0 = CameraManip.getFov();
    //CameraManip.fit(dimensionMin * ratio / scale, dimensionMax * ratio / scale); // the sensor position height is changed.
    CameraManip.fit(dimensionMin * ratio, dimensionMax * ratio); // the sensor position height is changed.

    glm::mat4 view0 = CameraManip.getMatrix();
    // nvmath::mat4f projj0 = nvmath::perspectiveVK(CameraManip.getFov(), aspectRatio, 0.0001f, 10000.0f);
    //glm::mat4 proj = glm::perspective(CameraManip.getFov(), aspectRatio, 0.0001f, 10000.0f);
    glm::mat4 proj0 = glm::perspectiveRH_ZO(glm::radians(CameraManip.getFov()), aspectRatio, 0.1f, 1000.0f);
    proj0[1][1] *= -1;
    glm::vec2 lu0,lb0,ru0,rb0;
    lu = proj0 * view0 * leftupper;
    lu = lu/lu.w;
    lu0.x = (lu.x+1.0f)/2.0*width;
    lu0.y = (1.0f + lu.y)/2.0*height;

    lb = proj0 * view0 * leftbottom;
    lb = lb/lb.w;
    lb0.x = (lb.x+1.0f)/2.0*width;
    lb0.y = (1.0f + lb.y)/2.0*height;

    ru = proj0 * view0 * rightupper;
    ru = ru/ru.w;
    ru0.x = (ru.x+1.0f)/2.0*width;
    ru0.y = (1.0f + ru.y)/2.0*height;


    rb = proj0 * view0 * rightbottom;
    rb = rb/rb.w;
    rb0.x = (rb.x+1.0f)/2.0*width;
    rb0.y = (1.0f + rb.y)/2.0*height;




    Eigen::VectorXd xx(4),yy(4),xx0(4),yy0(4);
    xx0 << rb0.x,ru0.x,lb0.x,lu0.x;
    yy0 << rb0.y,ru0.y,lb0.y,lu0.y;
    xx << rbn.x,run.x,lbn.x,lun.x;
    yy << rbn.y,run.y,lbn.y,lun.y;
    Eigen::MatrixXd ww(4,4);
    ww << rb0.x, rb0.y, rb0.x*rb0.y,1,
            ru0.x, ru0.y, ru0.x*ru0.y,1,
            lb0.x, lb0.y, lb0.x*lb0.y,1,
            lu0.x, lu0.y, lu0.x*lu0.y,1;

    cx = ww.jacobiSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(xx);
    cy = ww.jacobiSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(yy);


    int aa = 0;
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

    modelio->isUAVTrave = fileio->m_pVoxelrtXml->settingxml.isUAVtrave;
    modelio->n_pos = fileio->m_pVoxelrtXml->sensorxml.uavPoses.size();
    modelio->uavposes = fileio->m_pVoxelrtXml->sensorxml.uavPoses;

//    if(modelio->isUAVTrave == true){
//        modelio->sensor = createSensor(glm::vec3(0,10,1),glm::vec3(0,0,0));}
//    else {
//        modelio->sensor = createSensor(modelio->voxelSize_XZY, modelio->voxelOrigin_XZY, vza, vaa, 1.0);
//    }
    modelio->sensor = createSensor(modelio->voxelSize_XZY, modelio->voxelOrigin_XZY, vza, vaa, 1.0f);
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

        sensorMatrix = createSensor(modelio->voxelSize_XZY, modelio->voxelOrigin_XZY,
                                                 angle.vza, angle.vaa, ratio);
    }else {
        sensorMatrix = createSensor(glm::vec3(0, 1000, 1), glm::vec3(0, 900, 0));
    }
    updateSensor(modelio, sensorMatrix);

    LightSet lightSet = createLight(angle.sza, angle.saa,modelio->light.direct,
                                    modelio->light.diffuse,modelio->light.solarTemperature,
                                    modelio->light.skyTemperature);
    updateLight(modelio,lightSet);

}

void Geometry::updateSensorPos(std::shared_ptr<VoxelrtIO> &modelio, int kpos){

    glm::vec3 sensorpos = modelio->uavposes[kpos];
    Angle angle = modelio->angles[0];
//    std::cout << "Angle Info:"
//              << "    vza_" << std::to_string(angles.x) << "    vaa_" << std::to_string(angles.y)
//              << "    sza_" << std::to_string(angles.z) << "    saa_" << std::to_string(angles.w) << std::endl;

    float ratio = 1.0;
    //ratio = 0.707;
    sensorpos = sensorpos/modelio->stepsize_surface;
    SensorMatrix sensorMatrix;

    float r = 10;
    float rd = PI/180.0;

    glm::vec3 sensortarget = glm::vec3(r * std::sin(angle.vza * rd) * std::cos(angle.vaa * rd),
                                    r * std::cos(angle.vza * rd), r * std::sin(angle.vza * rd) * std::sin(angle.vaa * rd));


    sensorMatrix = createSensor(sensorpos, sensorpos-sensortarget);

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
    if (vza == 0.0 || vza == 45.0) vza = vza + ANGLE_COR;
    glm::vec3 origin = glm::vec3(0, 0, 0);
    glm::vec3 sensorPos = glm::vec3(r * std::sin(vza * rd) * std::cos(vaa * rd),
                                    r * std::cos(vza * rd),
                                    r * std::sin(vza * rd) * std::sin(vaa * rd));
    CameraManip.setFov(SENSOR_FOV);
    CameraManip.setLookat(sensorPos, origin, glm::vec3(0, 1, 0));
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
    leftupper = glm::vec4(dimensionMin.x,0,dimensionMax.z,1);
    leftbottom = glm::vec4(dimensionMin.x,0,dimensionMin.z,1);
    rightupper = glm::vec4(dimensionMax.x,0,dimensionMax.z,1);
    rightbottom = glm::vec4(dimensionMax.x,0,dimensionMin.z,1);

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


    float vza0 = 0;
    float vaa0 = 0;
    ratio = 0.707;
    if (vza0 == 0.0 || vza0 == 45.0) vza0 = vza0 + ANGLE_COR;
    glm::vec3 origin0 = glm::vec3(0, 0, 0);
    glm::vec3 sensorPos0 = glm::vec3(r * std::sin(vza0 * rd) * std::cos(vaa0 * rd),
                                     r * std::cos(vza0 * rd),
                                     r * std::sin(vza0 * rd) * std::sin(vaa0 * rd));
    CameraManip.setFov(SENSOR_FOV);
    CameraManip.setLookat(sensorPos0, origin0, glm::vec3(0, 1, 0));
    float fovv0 = CameraManip.getFov();
    //CameraManip.fit(dimensionMin * ratio / scale, dimensionMax * ratio / scale); // the sensor position height is changed.
    CameraManip.fit(dimensionMin * ratio, dimensionMax * ratio); // the sensor position height is changed.

    glm::mat4 view0 = CameraManip.getMatrix();
    // nvmath::mat4f projj0 = nvmath::perspectiveVK(CameraManip.getFov(), aspectRatio, 0.0001f, 10000.0f);
    //glm::mat4 proj = glm::perspective(CameraManip.getFov(), aspectRatio, 0.0001f, 10000.0f);
    glm::mat4 proj0 = glm::perspectiveRH_ZO(glm::radians(CameraManip.getFov()), aspectRatio, 0.1f, 1000.0f);
    proj0[1][1] *= -1;
    glm::vec2 lu0,lb0,ru0,rb0;
    lu = proj0 * view0 * leftupper;
    lu = lu/lu.w;
    lu0.x = (lu.x+1.0f)/2.0*width;
    lu0.y = (1.0f + lu.y)/2.0*height;

    lb = proj0 * view0 * leftbottom;
    lb = lb/lb.w;
    lb0.x = (lb.x+1.0f)/2.0*width;
    lb0.y = (1.0f + lb.y)/2.0*height;

    ru = proj0 * view0 * rightupper;
    ru = ru/ru.w;
    ru0.x = (ru.x+1.0f)/2.0*width;
    ru0.y = (1.0f + ru.y)/2.0*height;


    rb = proj0 * view0 * rightbottom;
    rb = rb/rb.w;
    rb0.x = (rb.x+1.0f)/2.0*width;
    rb0.y = (1.0f + rb.y)/2.0*height;




    Eigen::VectorXd xx(4),yy(4),xx0(4),yy0(4);
    xx0 << rb0.x,ru0.x,lb0.x,lu0.x;
    yy0 << rb0.y,ru0.y,lb0.y,lu0.y;
    xx << rbn.x,run.x,lbn.x,lun.x;
    yy << rbn.y,run.y,lbn.y,lun.y;
    Eigen::MatrixXd ww(4,4);
    ww << rb0.x, rb0.y, rb0.x*rb0.y,1,
            ru0.x, ru0.y, ru0.x*ru0.y,1,
            lb0.x, lb0.y, lb0.x*lb0.y,1,
            lu0.x, lu0.y, lu0.x*lu0.y,1;

    cx = ww.jacobiSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(xx);
    cy = ww.jacobiSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(yy);


    int aa = 0;
}


