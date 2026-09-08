

#pragma once
#include <iostream>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <crtdbg.h>
#endif


#include <vulkan/vulkan.hpp>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE


#include "src/base/engine.h"


using namespace std;

int main(int argc, char **argv) {

#ifdef _WIN32
    // This executable is a non-interactive worker. Report failures through
    // stderr/exit codes and suppress Windows crash/abort dialog boxes.
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif

    // const InputParser parser(argc, argv);
    // const string path = parser.getString("-w", "");     //// 路径，单独运行时的文件夹
    // const string version = parser.getString("-v", "");  ////
    // std::string filePath;
    // if (version.empty())
    // {
    //     cout << "Wrong version input" << endl;
    //     return true;
    // }
    // if (!path.empty())
    // {
    // }


    // std::string version = "eVoxeleb";
    // std::string path= "D://data//hyperSpectral//test_1202";
    // std::string filePath;

    std::string mode = argc > 1 ? argv[1] : "eVoxelEB";
    std::string inputPath = argc > 2 ? argv[2] : "";
    std::string outputPath = argc > 3 ? argv[3] : "";
    if (mode != "eVoxelEB" && mode != "eFacetEB" && mode != "eFacetRT"
        && mode != "eVoxelRT" && mode != "eRaytracing") {
        std::cerr << "Usage: histream [eVoxelEB|eFacetEB|eFacetRT|eVoxelRT|eRaytracing] [project.json] [output.json]\n";
        return 2;
    }

    try {
        Engine engine;
        engine.input(inputPath, mode, outputPath);
        if (!engine.create()) {
            std::cerr << "Failed to create engine resources for " << mode << '\n';
            engine.destroy();
            return 3;
        }
        const int runCode = engine.run();
        engine.destroy();
        return runCode;
    } catch (const std::exception& error) {
        std::cerr << "HiStream initialization failed: " << error.what() << '\n';
        return 4;
    }
}
