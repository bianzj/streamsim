//
// Created by bianzunjian on 2024/3/21.
//

#ifndef FIELD_RADIOSITY_UTILS_H
#define FIELD_RADIOSITY_UTILS_H

#include "structs.h"
#include <sstream>
#include <opencv2/opencv.hpp>
#include "gdal.h"
#include "gdal_priv.h"

namespace Utils {

    void arraysort(std::vector<int> &vindex, std::vector<float> &distance, int num);
    void transform(glm::vec3 xvw, glm::vec3 points, float xh[], float yh[], float &xx, float &yy);

    template<typename T>
    int getNextPow2Number(T number) {
        return std::pow(2, std::ceil(std::log2(number)));
    }

    void dvdiff(glm::vec3 ac,glm::vec3 aa, glm::vec3 ab);

    float ddt(glm::vec3 u, glm::vec3 v);

    std::vector <std::string> splitt(std::string& s, std::string& deli);

    std::vector<std::vector<float>> readImage(std::string infilename);


    /***************** Mat转vector **********************/
    template<typename _Tp>
    std::vector<_Tp> convertMat2Vector(const cv::Mat &mat)
    {
        return (std::vector<_Tp>)(mat.reshape(1, 1));//通道数不变，按行转为一行
    }

    /****************** vector转Mat *********************/
    template<typename _Tp>
    cv::Mat convertVector2Mat(std::vector<_Tp> v, int channels, int rows)
    {
        cv::Mat mat = cv::Mat(v).clone();//将vector变成单列的mat，这里需要clone(),因为这里的赋值操作是浅拷贝
        cv::Mat dest = mat.reshape(channels, rows);
        return dest;
    }

    float expint(float x);
    float* infile2num_float(std::string infileName, int skip, int col, int& num);

    int* infile2num_int(std::string infileName, int skip, int col, int& num);
    int getMapIndex(std::map<std::string, int> maps, std::string name);

    float max(double,float);

    std::vector<int> sort_index(std::vector<float> &v);

    void cross(float *vec1, float *vec2,float *norm);

    int readImageinout(std::string infilename,std::vector<std::vector<float>> &c,
                       int &width, int &height, int &nband);

    int readImageinout1(std::string infilename,std::vector<float> &c,
                        int &width, int &height, int &nband);

    int readImageinout11(std::string infilename,std::vector<float> &c,
                         int &width, int &height, int &nband,double *trans, std::string &proj);

    int saveImage1(std::string outfilepath, std::vector<float> &c,
                   int width, int height,int band, std::string proj, double trans[6]);

    int saveImage1(std::string outfilepath, std::vector<int> &c,
                   int width, int height,int band, std::string proj, double trans[6]);

    int saveImage(std::string outfilepath, std::vector<std::vector<float>> &c,
                  int width, int height,int band, std::string proj, double trans[6]);


    float* readascfile(std::string infileName, int skip, int col, int& num);

    int readascfileinout(std::string infileName, int skip, int col,
                         std::vector<float> &data, int& num);

    /*void imageToBuffer(VkDevice m_device, int m_queueIndex, const nvvk::Texture& imgIn, nvmath::vec3i size, const vk::Buffer& pixelBufferOut);
	void bufferToBuffer(VkDevice m_device, int m_queueIndex, const nvvk::Buffer& bufferIn, VkDeviceSize size, const nvvk::Buffer& bufferOut);*/

    std::string getDirectoryPath(const std::string& fullPath);

    std::string getFileName(const std::string& fullPath);

    template <typename T>
    void saveOut(std::string outFile, T data, int num);

    std::vector <int> findnum(std::string& ch);

    float expint(float x);

// Formating with local number representation
    template <class T>
    std::string FormatNumbers(T value)
    {
        std::stringstream ss;
        ss.imbue(std::locale(""));
        ss << std::fixed << value;
        return ss.str();
    }

    int * infile2num_bi(std::string infileName);
}



#endif //FIELD_RADIOSITY_UTILS_H
