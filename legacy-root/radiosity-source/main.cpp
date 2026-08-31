#include <iostream>
#include "base/engine.h"


int main() {

    std::string path = "D:/data/field_app/radiosityEB";
    std::string v = "eRadiosityEB";
    Engine engine;

    engine.input(path, v);
    engine.run();
    std::cout << "Hello, World!" << std::endl;
    return 0;
}
