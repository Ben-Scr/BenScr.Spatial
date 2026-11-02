#include <benscr/spatial/point_index.hpp>

#include <iostream>
#include <vector>

int main()
{
    namespace Spatial = BenScr::Spatial;
    Spatial::PointIndex index(2.0, 100);
    if (!index.Insert(42, {3.0, 4.0}) || !index.Insert(7, {-1.0, 0.0}) ||
        !index.Insert(99, {20.0, 20.0}))
    {
        return 1;
    }
    const auto nearby = index.QueryRadius({0.0, 0.0}, 5.0);
    if (nearby != std::vector<Spatial::Entry>{{7, {-1.0, 0.0}}, {42, {3.0, 4.0}}})
    {
        return 2;
    }
    if (!index.Update(99, {1.0, 1.0}) || !index.Remove(7) || index.Find(7).has_value())
    {
        return 3;
    }
    const auto rectangle = index.QueryRectangle({0.0, 0.0}, {3.0, 4.0});
    if (rectangle != std::vector<Spatial::Entry>{{42, {3.0, 4.0}}, {99, {1.0, 1.0}}})
    {
        return 4;
    }
    std::cout << "Spatial index verified.\n";
}
