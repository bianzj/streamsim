//
// Created by zhongsy on 2024/8/9.
//

#ifndef FIELD_MYFUNCTION_H
#define FIELD_MYFUNCTION_H
#pragma once
#include <vector>
#include <random>

class myFunction
{
public:
    myFunction() {};
    ~myFunction() {};

//    static bool sensorNameExist(std::vector<SensorStruct> sensorDatasets, std::string name);
    static float generateRandomFloat(int min, int max);

//    static int GetListNodeLen(struct ListNode* l1);
    static std::vector<std::string> mySplitStr(const std::string& str, const std::string& delim);
    static std::vector<int> mySplitInt(const std::string& str, const std::string& delim);
    static std::vector<float> mySplitFloat(const std::string& str, const std::string& delim);
};

#endif //FIELD_MYFUNCTION_H
