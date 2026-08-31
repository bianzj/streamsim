/******************************************************************************
 * Copyright 1998-2018 NVIDIA Corp. All Rights Reserved.
 *****************************************************************************/

#ifndef OBJLOADER_H
#define OBJLOADER_H
#pragma once
#ifdef USE_NEW_NVPRO_CORE
// #include "libInterpolate/Interpolate.hpp"
#else
// #include "Interpolate.hpp"
#endif

#define TINYOBJLOADER_IMPLEMENTATION


#include "structs.h"
//#include "nvmath/nvmath.h"
#include <array>
#include <iostream>
#include <unordered_map>
#include <vector>

// #include "../thirdparty/libInterpolate/Interpolate.hpp"
#include "gdal.h"
#include "gdal_priv.h"

#include <opencv2/opencv.hpp>



struct ShapeInfo
{
	uint32_t offset;
	uint32_t nbFacet;
	std::string name;
	int matIndex;
};

class ObjLoader
{
public:
	void loadModel(const std::string& filename);
	void loadMesh(const std::string& filename, const std::string& meshname);
	void createBackground(glm::vec3);
    void createBackground(glm::vec3,float step);

	void getModelMeshNames(const std::string& filename);

//	void creatBackgroundFromDEM(const std::string& filename, glm::vec3 sceneSize);
//
//	void createBackgroundFromDEM(const std::string& filename, nvmath::vec3f sceneSize, _2D::ThinPlateSplineInterpolator<double>& interp);
	// void createBackgroundFromDEM(const std::string& filename, glm::vec3 sceneSize, _2D::BilinearInterpolator<double>& interp);//�������ǰ���ԭ����DEM���ɣ�����������ʱ�볡����С��һ�£�
//    void createBackgroundFromResizedDEM(const std::string &filename, nvmath::vec3f sceneSize, _2D::BilinearInterpolator<double> &interp);//�Ƚ�DEM��ֵΪ������С��������
//	double getShiftInterp(_2D::BilinearInterpolator<double> interp, nvmath::vec3f shift0);

	void createBackgroundFromDEM(const std::string& filename, glm::vec3 sceneSize, glm::vec2 demResolution);
	double getShiftInterp(glm::vec3 shift0, glm::vec2 demResolution);

	ObjMesh m_objmesh;

	std::vector<std::string> m_meshNames;
	std::vector<ObjMesh> m_objMeshs;

	float minElevation{ 0 };

private:
	std::vector<VertexAttribute> m_vertices;
	std::vector<uint32_t> m_indices;
    void clearCurrentInfo();
	double getShiftInterp(float indexX, float indexY);

	std::vector<ShapeInfo> m_shapeInfo;

	float m_scale{1};
    float m_step{5};
    int nImgSizeX{1};
    int nImgSizeY{1};

	cv::Mat DEM;

};

#endif //FIELD_RADIOSITY_SCENE_H
