#include "../../models/histream/src/base/voxel_boundary.h"
#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>

using namespace voxel_boundary;
using Edge = std::array<Point, 2>;
void check(const std::vector<Cell>& cells, size_t facesExpected) {
    size_t count = 0;
    double volume = 0;
    std::map<Edge, int> edgeBalance;
    emit(cells, [&](const Quad& q, const Cell& expectedNormal) {
        ++count;
        for (int corner = 1; corner <= 2; ++corner) {
            Point a{}, b{}, n{};
            for (int axis = 0; axis < 3; ++axis) {
                a[axis] = q[corner][axis] - q[0][axis];
                b[axis] = q[corner+1][axis] - q[0][axis];
            }
            for (int axis = 0; axis < 3; ++axis) {
                n[axis] = a[(axis+1)%3]*b[(axis+2)%3] - a[(axis+2)%3]*b[(axis+1)%3];
                if (std::abs(n[axis] - expectedNormal[axis]) > 1e-6)
                    throw std::runtime_error("Inward or degenerate triangle");
                volume += double(q[0][axis]) * n[axis] / 6.0;
            }
        }
        for (int i=0;i<4;++i) {
            Point a=q[i], b=q[(i+1)%4];
            bool sorted=a<b;
            edgeBalance[sorted ? Edge{a,b} : Edge{b,a}] += sorted ? 1 : -1;
        }
    });
    if (count != facesExpected) throw std::runtime_error("Internal/absent boundary face");
    if (std::abs(volume - cells.size()) > 1e-5) throw std::runtime_error("Wrong enclosed volume");
    for (const auto& edge:edgeBalance)
        if (edge.second != 0) throw std::runtime_error("Open boundary or inconsistent winding");
}
int main() {
    check({},0);
    check({{0,0,0}},6);
    check({{-7,-2,-3}},6);
    check({{0,0,0},{1,0,0}},10);
    check({{0,0,0},{1,0,0},{0,1,0}},14);
    std::vector<Cell> block;
    for(int x=0;x<3;++x) for(int y=0;y<2;++y) for(int z=0;z<4;++z) block.push_back({x,y,z});
    check(block,52);
    std::cout << "PASS: empty, single, negative, adjacent, concave and solid-block boundaries; outward normals, closed edges, exact volume.\n";
}
