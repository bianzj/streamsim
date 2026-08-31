//
// Created by zhongsy on 2024/8/9.
//
#include "myFunction.h"

//bool myFunction::sensorNameExist(std::vector<SensorStruct> sensorDatasets, std::string name)
//{
//    for (auto data : sensorDatasets)
//    {
//        if (data.name == name)
//        {
//            return true;
//        }
//    }
//    return false;
//}

float myFunction::generateRandomFloat(int min, int max)
{
    // 用 dis 变换 gen 生成的随机 unsigned int 为 [1, 2) 中的 double
    // 每次调用 dis(gen) 都生成新的随机 double
    std::random_device              rd;  // Will be used to obtain a seed for the random number engine
    std::mt19937                    gen(rd());  // Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<> disx(min, max);
    float offx = disx(gen);
    return offx;
}

std::vector<std::string> myFunction::mySplitStr(const std::string& str, const std::string& delim)
{
    std::vector<std::string> res;
    if ("" == str) return res;
    //先将要切割的字符串从string类型转换为char*类型
    char* strs = new char[str.length() + 1]; //不要忘了
    strcpy_s(strs, str.length() + 1, str.c_str());

    char* d = new char[delim.length() + 1];
    strcpy_s(d, delim.length() + 1, delim.c_str());

    char* ptr = NULL;
    char* p = strtok_s(strs, d, &ptr);
    while (p) {
        res.push_back(p); //存入结果数组
        p = strtok_s(NULL, d, &ptr);
    }
    return res;
}

std::vector<int> myFunction::mySplitInt(const std::string& str, const std::string& delim)
{
    std::vector<int> res;
    if ("" == str) return res;
    //先将要切割的字符串从string类型转换为char*类型
    char* strs = new char[str.length() + 1]; //不要忘了
    strcpy_s(strs, str.length() + 1, str.c_str());

    char* d = new char[delim.length() + 1];
    strcpy_s(d, delim.length() + 1, delim.c_str());

    char* ptr = NULL;
    char* p = strtok_s(strs, d, &ptr);//相较于strtok()函数，strtok_s函数需要用户传入一个指针，用于函数内部判断从哪里开始处理字符串
    while (p) {
        int s = atoi(p); //分割得到的字符串转换为int类型
        res.push_back(s); //存入结果数组
        p = strtok_s(NULL, d, &ptr);//其他的使用与strtok()函数相同
    }
    return res;
}

std::vector<float> myFunction::mySplitFloat(const std::string& str, const std::string& delim)
{
    std::vector<float> res;
    if ("" == str) return res;
    //先将要切割的字符串从string类型转换为char*类型
    char* strs = new char[str.length() + 1]; //不要忘了
    strcpy_s(strs, str.length() + 1, str.c_str());

    char* d = new char[delim.length() + 1];
    strcpy_s(d, delim.length() + 1, delim.c_str());

    char* ptr = NULL;
    char* p = strtok_s(strs, d, &ptr);
    while (p) {
        float s = atof(p); //分割得到的字符串转换为float类型
        res.push_back(s); //存入结果数组
        p = strtok_s(NULL, d, &ptr);
    }
    return res;
}

//int myFunction::GetListNodeLen(struct ListNode* l1)
//{
//    if (l1 == NULL)
//    {
//        return 0;
//    }
//    struct ListNode* p=l1;
//    int aListLen = 0;
//    while(p != NULL)    //判断当前节点指针是否为空
//    {
//        aListLen ++;
//        p = p->next;
//    }
//    return aListLen;
//}

