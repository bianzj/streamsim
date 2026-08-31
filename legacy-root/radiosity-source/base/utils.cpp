//
// Created by bianzunjian on 2024/3/21.
//

#include "utils.h"
#include <numeric>
#include <algorithm>
#include <vector>

namespace Utils
{




    void arraysort(std::vector<int> &vindex, std::vector<float> &distance, int num) {
        float iv,v;
        int j=0;
        for(int i = 0;i<num;i++) vindex[i] = i;

        for (int i = 1;i<num;i++)
        {
            iv = vindex[i];
            v = distance[i];

            for(j = i - 1;j>=0;j--)
            {
                if (distance[j] > v)
                {
                    distance[j + 1] = distance[j];
                    vindex[j + 1] = vindex[j];
                }
                else
                    break;
            }
            distance[j + 1] = v;
            vindex[j + 1] = iv;
        }
    }

    void transform(glm::vec3 xvw, glm::vec3 points, float *xh, float *yh, float &xx, float &yy) {
//        for(int i=0;i<3;i++)
//        {
////            (xx)+=(points[i]-xvw[i])*xh[i];
////            (yy)+=(points[i]-xvw[i])*yh[i];
//
//
//        }
        (xx)+=(points.x-xvw.x)*xh[0];
        (yy)+=(points.x-xvw.x)*yh[0];
        (xx)+=(points.y-xvw.y)*xh[1];
        (yy)+=(points.y-xvw.y)*yh[1];
        (xx)+=(points.z-xvw.z)*xh[2];
        (yy)+=(points.z-xvw.z)*yh[2];
    }




}


float Utils::max(double a, float b)
{
    if (a>b)
    {
        return a;
    }else
    {
        return b;
    }
}

/// "<" from -10,0,10
/// ">" from 10,0 ,-10
/// \param v
/// \return
std::vector<int> Utils::sort_index(std::vector<float> &v) {
    // 初始化索引向量
   std::vector<int> idx(v.size());
    //使用iota对向量赋0~？的连续值
    std::iota(idx.begin(), idx.end(), 0);
    // 通过比较v的值对索引idx进行排序
    std::stable_sort(idx.begin(), idx.end(), [&v](int i1, int i2) { return v[i1] < v[i2]; });
    return idx;
}


float Utils::expint(float x)
{
    int i1 = 1000;
    int i2 = 100000;
    double sum = 0.0, ii;
    for (int i = i1; i < i2; i++)
    {
        ii = i / 1000.0;
        sum = sum + exp(-x * ii) / ii * 0.001;
    }
    return sum;

}


void Utils::dvdiff(glm::vec3 ac,glm::vec3 aa, glm::vec3 ab)
{
//    ac[0] = aa[0] - ab[0];
//    ac[1] = aa[1] - ab[1];
//    ac[2] = aa[2] - ab[2];

    ac.x = aa.x - ab.x;
    ac.y = aa.y - ab.y;
    ac.z = aa.z - ab.z;

}

float Utils::ddt(glm::vec3 u,glm::vec3 v)
{
    float res=0;
//    res = u[1]*v[1]+u[2]*v[2]+u[0]*v[0];
    res = u.x*v.x+u.y*v.y+u.z*v.z;
    return res;
}

float* Utils::readascfile(std::string infileName, int skip, int col, int &num)
{
    // std::string line;
    // std::vector <std::string> fields;
    // std::string deli(" ");
    //
    // std::ifstream infile(infileName.c_str());
    // if (infile.is_open())
    // {
    //     num = 0;
    //     if (skip != 0)
    //     {
    //         for (int i = 0; i < skip; i++) std::getline(infile, line);
    //     }
    //     while (std::getline(infile, line))
    //     {
    //         fields = Utils::splitt(line, deli);
    //         if (int(fields.size()) >= 1)
    //         {
    //             num++;
    //         }
    //     }
    // }
    // else std::cout << "Unable to open the file: " << infileName << std::endl;
    // infile.close();
    //
    // float* mydata = new float[num];
    // if (num >= 1)
    // {
    //
    //     int jj = 0;
    //     std::ifstream infilee(infileName.c_str());
    //     mydata = new float[num];
    //     //getline(infilee,line);
    //
    //     if (skip != 0)
    //     {
    //         for (int i = 0; i < skip; i++) std::getline(infilee, line);
    //     }
    //     while (std::getline(infilee, line))
    //     {
    //         fields = Utils::splitt(line, deli);
    //         if (int(fields.size()) > col)
    //         {
    //             mydata[jj] = atof(fields[col].c_str());
    //             jj = jj + 1;
    //         }
    //     }
    //     infilee.close();
    // }
    //
    // return mydata;
    std::string line;
    std::vector <std::string> fields;
    std::string deli(" ");

    std::ifstream infile(infileName.c_str());
    if (infile.is_open())
    {
        num = 0;
        if (skip != 0)
        {
            for (int i = 0; i < skip; i++) std::getline(infile, line);
        }
        while (std::getline(infile, line))
        {
            fields = Utils::splitt(line, deli);
            if (int(fields.size()) >= 1)
            {
                num++;
            }
        }
    }
    else std::cout << "Unable to open the file: " << infileName << std::endl;
    infile.close();

    float* mydata = new float[num];
    if (num >= 1)
    {

        int jj = 0;
        std::ifstream infilee(infileName.c_str());
        mydata = new float[num];
        //getline(infilee,line);

        if (skip != 0)
        {
            for (int i = 0; i < skip; i++) std::getline(infilee, line);
        }
        while (std::getline(infilee, line))
        {
            fields = Utils::splitt(line, deli);
            if (int(fields.size()) > col)
            {
                mydata[jj] = atof(fields[col].c_str());
                jj = jj + 1;
            }
        }
        infilee.close();
    }

    return mydata;
}



int Utils::readascfileinout(std::string infileName, int skip, int col, std::vector<float> &data, int &num)
{
    std::string line;
    std::vector <std::string> fields;
    std::string deli(" ");
    data.clear();

    std::ifstream infile(infileName.c_str());
    if (infile.is_open())
    {
        num = 0;
        if (skip != 0)
        {
            for (int i = 0; i < skip; i++) std::getline(infile, line);
        }
        while (std::getline(infile, line))
        {
            fields = Utils::splitt(line, deli);
            if (int(fields.size()) >= 1)
            {
                num++;
            }
        }
    }
    else std::cout << "Unable to open the file: " << infileName << std::endl;
    infile.close();

//    float* mydata = new float[num];

    data.clear();
    if (num >= 1)
    {
        int jj = 0;
        std::ifstream infilee(infileName.c_str());
//        mydata = new float[num];
        //getline(infilee,line);

        if (skip != 0)
        {
            for (int i = 0; i < skip; i++) std::getline(infilee, line);
        }
        while (std::getline(infilee, line))
        {
            fields = Utils::splitt(line, deli);
            if (int(fields.size()) > col)
            {
//                mydata[jj] = atof(fields[col].c_str());
                data.push_back(atof(fields[col].c_str()));
                jj = jj + 1;
            }
        }
        infilee.close();
    }

    return 1;
}



std::string Utils::getDirectoryPath(const std::string& fullPath)
{
    size_t lastSlash = fullPath.find_last_of('/');
    if(lastSlash!=std::string::npos)
    {
        return fullPath.substr(0,lastSlash);
    }else{
        return "";
    }
}

std::string Utils::getFileName(const std::string& fullPath)
{
    size_t lastSlash = fullPath.find_last_of('/');
    if(lastSlash!=std::string::npos)
    {
        return fullPath.substr(lastSlash+1);
    }else{
        return fullPath;
    }
}

void Utils::cross(float *vec1, float *vec2,float *norm)
{
    float vec[3];
    vec[0]=vec1[1]*vec2[2]-vec1[2]*vec2[1];
    vec[1]=vec1[2]*vec2[0]-vec1[0]*vec2[2];
    vec[2]=vec1[0]*vec2[1]-vec1[1]*vec2[0];

    float su=0;
    for(int k=0;k<3;k++)
    {
        su = su+vec[k]*vec[k];
    }
    su = sqrt(su);
    for(int k=0;k<3;k++)
    {
        norm[k] = vec[k]*1.0/su;
    }
}

int Utils::readImageinout1(std::string infilename,std::vector<float> &collected,
                           int &width, int &height, int &nband) {


    GDALAllRegister();
    CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");

    GDALDataset *poDataset;
    GDALDriver *poDriver;


    poDataset = (GDALDataset *) GDALOpen(infilename.c_str(), GA_ReadOnly);//
    if (poDataset == NULL) {
        std::cout << "指定的文件不能打开!" << std::endl;
        return 0;
    }
    width = poDataset->GetRasterXSize();          //获取影像信息
    height = poDataset->GetRasterYSize();
    nband = poDataset->GetRasterCount();

    GDALDataType gBand = poDataset->GetRasterBand(1)->GetRasterDataType();
    int nBits = GDALGetDataTypeSize(gBand);


    //std::vector<std::vector<float>> collected(nband);

    GDALRasterBand * poBand = poDataset->GetRasterBand(1);
    float *bandData = (float *)CPLMalloc(sizeof(float)*width*height);
    CPLErr result = poBand->RasterIO(GF_Read,0,0,width,height,bandData,width,height,GDT_Float32,0,0);

    collected.assign(bandData,bandData+width*height);
    CPLFree(bandData);


    // std::cout<<"123"<<std::endl;
//    double geoTransform[6];                       //获取坐标信息
//    poDataset->GetGeoTransform(geoTransform);
//    const char *spatialRef = poDataset->GetProjectionRef();  //获取投影信息

    GDALClose(poDataset);

    return 1;
}

int Utils::readImageinout(std::string infilename,std::vector<std::vector<float>> &collected,
                          int &width, int &height, int &nband) {


    GDALAllRegister();
    CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");

    GDALDataset *poDataset;
    GDALDriver *poDriver;


    poDataset = (GDALDataset *) GDALOpen(infilename.c_str(), GA_ReadOnly);//
    if (poDataset == NULL) {
        std::cout << "指定的文件不能打开!" << std::endl;
        return 0;
    }
    width = poDataset->GetRasterXSize();          //获取影像信息
    height = poDataset->GetRasterYSize();
    nband = poDataset->GetRasterCount();

    GDALDataType gBand = poDataset->GetRasterBand(1)->GetRasterDataType();
    int nBits = GDALGetDataTypeSize(gBand);


    //std::vector<std::vector<float>> collected(nband);
    for(int kband=1;kband < nband+1;kband++)
    {
//        GDALRasterBand * band = poDataset->GetRasterBand(kband);
//        float *bandData = new float(width*height);
//        band->RasterIO(GF_Read,0,0,width,height,bandData,width,height,GDT_Float32,0,0);
//        collected[kband-1].assign(bandData,bandData+width*height);
//        delete [] bandData;

        GDALRasterBand * poBand = poDataset->GetRasterBand(kband);
        float *bandData = (float *)CPLMalloc(sizeof(float)*width*height);
        CPLErr result = poBand->RasterIO(GF_Read,0,0,width,height,bandData,width,height,GDT_Float32,0,0);
        std::vector<float> tempcollected;
        tempcollected.assign(bandData,bandData+width*height);
        collected.push_back(tempcollected);
        CPLFree(bandData);

    }

    double geoTransform[6];                       //获取坐标信息
    poDataset->GetGeoTransform(geoTransform);
    const char *spatialRef = poDataset->GetProjectionRef();  //获取投影信息

    GDALClose(poDataset);

    return 1;
}

std::vector<std::vector<float>> Utils::readImage(std::string infilename) {


    GDALAllRegister();
    CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");

    GDALDataset *poDataset;
    GDALDriver *poDriver;


    poDataset = (GDALDataset *) GDALOpen(infilename.c_str(), GA_ReadOnly);//
    if (poDataset == NULL) {
        std::cout << "指定的文件不能打开!" << std::endl;
        return std::vector<std::vector<float>>();
    }
    int width = poDataset->GetRasterXSize();          //获取影像信息
    int height = poDataset->GetRasterYSize();
    int nBands = poDataset->GetRasterCount();

    GDALDataType gBand = poDataset->GetRasterBand(1)->GetRasterDataType();
    int nBits = GDALGetDataTypeSize(gBand);


    std::vector<std::vector<float>> collected(nBands);
    for(int kband=1;kband < nBands;kband++)
    {
//        GDALRasterBand * band = poDataset->GetRasterBand(kband);
//        float *bandData = new float(width*height);
//        band->RasterIO(GF_Read,0,0,width,height,bandData,width,height,GDT_Float32,0,0);
//        collected[kband-1].assign(bandData,bandData+width*height);
//        delete [] bandData;

        GDALRasterBand * poBand = poDataset->GetRasterBand(kband);
        float *bandData = (float *)CPLMalloc(sizeof(float)*width*height);
        CPLErr result = poBand->RasterIO(GF_Read,0,0,width,height,bandData,width,height,GDT_Float32,0,0);
        collected[kband-1].assign(bandData,bandData+width*height);
        CPLFree(bandData);
    }

    double geoTransform[6];                       //获取坐标信息
    poDataset->GetGeoTransform(geoTransform);
    const char *spatialRef = poDataset->GetProjectionRef();  //获取投影信息

    GDALClose(poDataset);

    return collected;
}


int Utils::saveImage1(std::string outfilepath, std::vector<float> &c,
                      int width, int height,int band, std::string proj, double trans[6]) {

    CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
    GDALAllRegister();  //注册所有的驱动

//    GDALDataset *poDataset;   //GDAL数据集
//    GDALRasterBand* poBand = poDataset->GetRasterBand(band);
//    GDALDataType type = poBand->GetRasterDataType();

    GDALDriver *driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    GDALDataset *ods = driver->Create(outfilepath.c_str(),width,height,1,GDT_Float32,NULL);
    GDALRasterBand *oBand = ods->GetRasterBand(1);
    //float *bandData = c.data();
    CPLErr result =oBand->RasterIO(GF_Write,0, 0,width,height,c.data(),width,height,GDT_Float32,0,0);

    GDALClose(ods);
    return 1;
}

int Utils::saveImage1(std::string outfilepath, std::vector<int> &c,
                      int width, int height,int band, std::string proj, double trans[6]) {

    CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
    GDALAllRegister();  //注册所有的驱动

//    GDALDataset *poDataset;   //GDAL数据集
//    GDALRasterBand* poBand = poDataset->GetRasterBand(band);
//    GDALDataType type = poBand->GetRasterDataType();

    GDALDriver *driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    GDALDataset *ods = driver->Create(outfilepath.c_str(),width,height,1,GDT_Int32,NULL);
    GDALRasterBand *oBand = ods->GetRasterBand(1);
    //float *bandData = c.data();
    CPLErr result =oBand->RasterIO(GF_Write,0, 0,width,height,c.data(),width,height,GDT_Int32,0,0);

    GDALClose(ods);
    return 1;
}


int Utils::saveImage(std::string outfilepath, std::vector<std::vector<float>> &c,
                     int width, int height,int band, std::string proj, double trans[6]) {

    CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
    GDALAllRegister();  //注册所有的驱动

//    GDALDataset *poDataset;   //GDAL数据集
//    GDALRasterBand* poBand = poDataset->GetRasterBand(band);
//    GDALDataType type = poBand->GetRasterDataType();

    GDALDriver *driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    GDALDataset *ods = driver->Create(outfilepath.c_str(),width,height,band,GDT_Float32,NULL);
    for(int kband=0;kband<band;kband++) {
        GDALRasterBand *oBand = ods->GetRasterBand(kband+1);
        //float *bandData = c[kband].data();
        CPLErr result = oBand->RasterIO(GF_Write, 0, 0, width, height, c[kband].data(), width, height, GDT_Float32, 0, 0);

    }
    GDALClose(ods);
    return 1;
}

std::vector<std::string> Utils::splitt(std::string& s, std::string& deli)
{
    // std::vector <std::string> ret;
    // int last = 0;
    // int index = s.find_first_of(deli, last);
    // int endx = s.find_last_not_of(deli);
    // std::string subpart;
    // while (index != int(std::string::npos))
    // {
    //     subpart = s.substr(last, index - last);
    //     if (subpart.size() != 0) ret.push_back(subpart);
    //     last = index + 1;
    //     index = s.find_first_of(deli, last);
    // }
    // if (endx - last > 0) ret.push_back(s.substr(last, endx));
    // return ret;
    std::vector <std::string> ret;
    int last = 0;
    int index = s.find_first_of(deli, last);
    int endx = s.find_last_not_of(deli);
    std::string subpart;
    while (index != int(std::string::npos))
    {
        subpart = s.substr(last, index - last);
        if (subpart.size() != 0) ret.push_back(subpart);
        last = index + 1;
        index = s.find_first_of(deli, last);
    }
    if (endx - last >= 0) ret.push_back(s.substr(last, endx));
    return ret;
}

int * Utils::infile2num_bi(std::string infileName)
{
    std::ifstream infile(infileName.c_str(),std::ios::binary);
    if (infile)
    {
        // get length of file:
        infile.seekg (0, infile.end);
        int length = infile.tellg();
        // cout<<length<<endl;
        infile.seekg (0, infile.beg);

        int temp = 0;
        int *mydata = new int[length/4];

        // std::cout << "Reading " << length << " characters... ";
        // read data as a block:
        for(int i=0; i<length/4; i++)
        {
            infile.read ((char*)&temp,sizeof(temp));
            //cout<<temp<<endl;
            mydata[i]=temp;

        }

        if (!infile)
            //  std::cout << "all characters read successfully.";
            std::cout << "error: only " << infile.gcount() << " could be read";
        infile.close();

        // ...buffer contains the entire file...
        return mydata;
        // delete[] buffer;
    }
}

//float Utils::expint(float x)
//{
//    int i1 = 1000;
//    int i2 = 100000;
//    double sum = 0.0, ii;
//    for (int i = i1; i < i2; i++)
//    {
//        ii = i / 1000.0;
//        sum = sum + exp(-x * ii) / ii * 0.001;
//    }
//    return sum;
//
//}

float* Utils::infile2num_float(std::string infileName, int skip, int col, int& num)
{
    std::string line;
    std::vector <std::string> fields;
    std::string deli(" ");



    std::ifstream infile(infileName.c_str());
    if (infile.is_open())
    {
        num = 0;
        if (skip != 0)
        {
            for (int i = 0; i < skip; i++) std::getline(infile, line);
        }
        while (std::getline(infile, line))
        {
            fields = Utils::splitt(line, deli);
            if (int(fields.size()) >= 1)
            {
                num++;
            }
        }
    }
    else std::cout << "Unable to open the file: " << infileName << std::endl;
    infile.close();

    float* mydata = new float[num];
    if (num >= 1)
    {

        int jj = 0;
        std::ifstream infilee(infileName.c_str());
        mydata = new float[num];
        //getline(infilee,line);

        if (skip != 0)
        {
            for (int i = 0; i < skip; i++) std::getline(infilee, line);
        }
        while (std::getline(infilee, line))
        {
            fields = Utils::splitt(line, deli);
            if (int(fields.size()) > col)
            {
                mydata[jj] = atof(fields[col].c_str());
                jj = jj + 1;
            }
        }
        infilee.close();
    }

    return mydata;
}

int* Utils::infile2num_int(std::string infileName, int skip, int col, int& num)
{
    std::string line;
    std::vector <std::string> fields;
    std::string deli(" ");



    std::ifstream infile(infileName.c_str());
    if (infile.is_open())
    {
        num = 0;
        if (skip != 0)
        {
            for (int i = 0; i < skip; i++) std::getline(infile, line);
        }
        while (std::getline(infile, line))
        {
            fields = Utils::splitt(line, deli);
            if (int(fields.size()) >= 1)
            {
                num++;
            }
        }
    }
    else std::cout << "Unable to open the file: " << infileName << std::endl;
    infile.close();

    int* mydata = new int[num];
    if (num >= 1)
    {

        int jj = 0;
        std::ifstream infilee(infileName.c_str());
        mydata = new int[num];
        //getline(infilee,line);

        if (skip != 0)
        {
            for (int i = 0; i < skip; i++) std::getline(infilee, line);
        }
        while (std::getline(infilee, line))
        {
            fields = Utils::splitt(line, deli);
            if (int(fields.size()) > col)
            {
                mydata[jj] = atoi(fields[col].c_str());
                jj = jj + 1;
            }
        }
        infilee.close();
    }

    return mydata;
}


int Utils::getMapIndex(std::map<std::string, int> maps, std::string name)
{
    int loc = 0;
    std::map<std::string, int>::iterator it;
    it = maps.find(name);
    if (it != maps.end())
        loc = it->second;
    return loc;
    return 0;
}

bool isnum(char n)
{
    return (n >= '0' && n <= '9');
}
std::vector <int> Utils::findnum(std::string& ch)
{
    std::vector <int> numVec;
    int k = ch.size();
    int* num = new int[k];
    int result;
    int n = 0;
    int i = 0;
    while (n < k) {
        result = 0;
        if (isnum(ch[n]))
        {
            result = ch[n] - '0';
            while (n < k && isnum(ch[++n]))
                result = (ch[n] - '0') + 10 * result;
            num[i++] = result;
        }
        ++n;
    }
    for (int j = 0; j < i; ++j)
    {
        numVec.push_back(num[j]);
    }
    return numVec;
}




