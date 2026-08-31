//
// Created by admin on 2024/1/24.
//

#ifndef FIELD_GEOMETRY_H
#define FIELD_GEOMETRY_H

#include "src/raytracing/raytracingio.h"
#include "src/voxeleb/voxelebio.h"
#include "src/voxelrt/voxelrtio.h"
#include "nvh/cameramanipulator.hpp"
#include "nvmath/nvmath.h"
#include "utils.h"
#include "thirdparty/spa.h"
#include <Eigen/Dense>

class Geometry {
public:
    Geometry(){};



    SensorMatrix createSensor(glm::vec3 size, glm::vec3 origin, float vza, float vaa, float ratio = 1.0);
    LightSet createLight(float sza, float saa, float direct, float diffuse,float,float);
    SensorMatrix createSensor(glm::vec3 sensorPos_XZY, glm::vec3 center);

    bool createGeometry(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RaytracingIO> &modelio);
    void updateAngle(std::shared_ptr<RaytracingIO> &modelio, int kangle);
    void updateSensor(std::shared_ptr<RaytracingIO> &modelio, SensorMatrix &sensor);
    void updateLight(std::shared_ptr<RaytracingIO> &modelio, LightSet &light);
    void orthcorrect(std::shared_ptr<RaytracingIO> &modelio,float vza, float vaa,
                     Eigen::VectorXd & cx, Eigen::VectorXd & cy);


    bool createGeometry(std::shared_ptr<FileIO> &fileio, std::shared_ptr<VoxelebIO> &modelio);
    void updateAngle(std::shared_ptr<VoxelebIO> &modelio, int kangle);
    void updateSensor(std::shared_ptr<VoxelebIO> &modelio, SensorMatrix &sensor);
    void updateLight(std::shared_ptr<VoxelebIO> &modelio, LightSet &light);
    void orthcorrect(std::shared_ptr<VoxelebIO> &modelio,float vza, float vaa,
                               Eigen::VectorXd &cx, Eigen::VectorXd &cy);
    static void updateSolarAngle(std::shared_ptr<VoxelebIO> &modelio, Angle &angle);


    bool createGeometry(std::shared_ptr<FileIO> &fileio, std::shared_ptr<VoxelrtIO> &modelio);
    void updateAngle(std::shared_ptr<VoxelrtIO> &modelio, int kangle);
    void updateSensorPos(std::shared_ptr<VoxelrtIO> &modelio, int kPos);

    void updateSensor(std::shared_ptr<VoxelrtIO> &modelio, SensorMatrix &sensor);
    void updateLight(std::shared_ptr<VoxelrtIO> &modelio, LightSet &light);
    void orthcorrect(std::shared_ptr<VoxelrtIO> &modelio,float vza, float vaa,
                     Eigen::VectorXd & cx, Eigen::VectorXd & cy);


};


#endif //FIELD_GEOMETRY_H
