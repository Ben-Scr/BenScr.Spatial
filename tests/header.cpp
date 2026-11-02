#include <benscr/spatial/point_index.hpp>

int main()
{
    const BenScr::Spatial::PointIndex index;
    return index.Count() == 0 ? 0 : 1;
}
