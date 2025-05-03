#include "graphviz.h"

#include <fstream>
#include <string>
#include <iostream>

void GenerateDot(std::string filename, std::string content) {
    std::ofstream ofs(filename, std::ios::out);
    
    if (!ofs) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    ofs << content << std::endl;

    ofs.close();

    return;
}