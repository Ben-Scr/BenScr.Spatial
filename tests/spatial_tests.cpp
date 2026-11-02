#include <benscr/spatial/point_index.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace Spatial = BenScr::Spatial;
using Spatial::Entry;
using Spatial::Point;
using Spatial::PointId;
using Spatial::PointIndex;

namespace
{
void Check(bool condition)
{
    if (!condition)
    {
        throw std::runtime_error("Contract check failed.");
    }
}

template <typename Error, typename Function> void Throws(Function function)
{
    try
    {
        function();
    }
    catch (const Error &)
    {
        return;
    }
    throw std::runtime_error("Expected exception was not thrown.");
}

std::vector<PointId> Ids(const std::vector<Entry> &entries)
{
    std::vector<PointId> ids;
    for (const auto &entry : entries)
    {
        ids.push_back(entry.Id);
    }
    return ids;
}

std::vector<Entry> All(const PointIndex &index)
{
    return index.QueryRectangle({-Spatial::kMaxCoordinate, -Spatial::kMaxCoordinate},
                                {Spatial::kMaxCoordinate, Spatial::kMaxCoordinate});
}

void ReferenceQueries()
{
    std::uint64_t state = 1701;
    auto next = [&]()
    {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return state;
    };
    auto coordinate = [&]() { return static_cast<double>(next() % 20001) / 100.0 - 100.0; };
    for (const double cellSize : {0.000001, 0.25, 2.0, 1e12})
    {
        PointIndex index(cellSize);
        std::map<PointId, Point> reference;
        for (PointId id = 0; id < 400; ++id)
        {
            const Point point{coordinate(), coordinate()};
            Check(index.Insert(id, point));
            reference.emplace(id, point);
        }
        for (int iteration = 0; iteration < 250; ++iteration)
        {
            const PointId id = next() % 400;
            if (iteration % 3 == 0)
            {
                Check(index.Remove(id) == (reference.erase(id) != 0));
            }
            else
            {
                const Point point{coordinate(), coordinate()};
                Check(index.Update(id, point) == reference.contains(id));
                if (reference.contains(id))
                {
                    reference.at(id) = point;
                }
            }
            const Point center{coordinate(), coordinate()};
            const double radius = static_cast<double>(next() % 15000) / 100.0;
            const Point minimum{center.X - radius, center.Y - radius};
            const Point maximum{center.X + radius, center.Y + radius};
            std::vector<Entry> rectangle;
            std::vector<Entry> circle;
            for (const auto &[key, point] : reference)
            {
                if (point.X >= minimum.X && point.X <= maximum.X && point.Y >= minimum.Y &&
                    point.Y <= maximum.Y)
                {
                    rectangle.push_back({key, point});
                }
                // Independent full scan: the reference never consults grid cells.
                if (std::hypot(point.X - center.X, point.Y - center.Y) <= radius)
                {
                    circle.push_back({key, point});
                }
            }
            Check(index.QueryRectangle(minimum, maximum) == rectangle);
            Check(index.QueryRadius(center, radius) == circle);
            Check(index.Count() == reference.size());
        }
    }
}
} // namespace

int main()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    const std::vector<std::pair<const char *, std::function<void()>>> cases{
        {"empty and defaults",
         []
         {
             const PointIndex index;
             Check(index.Count() == 0 && index.CellSize() == 1 &&
                   index.Capacity() == Spatial::kMaxPoints);
             Check(!index.Contains(5) && !index.Find(5) && All(index).empty());
         }},
        {"invalid cell sizes",
         [&]
         {
             for (const auto size : {0.0, -1.0, nan, infinity, 1e-7, 1e13})
             {
                 Throws<std::invalid_argument>([size] { PointIndex index(size); });
             }
         }},
        {"invalid capacity",
         []
         {
             Throws<std::length_error>([] { PointIndex index(1, 0); });
             Throws<std::length_error>([] { PointIndex index(1, Spatial::kMaxPoints + 1); });
         }},
        {"insert and copied lookup",
         []
         {
             PointIndex index;
             Check(index.Insert(17, {1, 2}));
             auto copy = index.Find(17);
             Check(copy == Point{1, 2});
             copy->X = 100;
             Check(index.Find(17) == Point{1, 2});
         }},
        {"duplicate IDs preserve position",
         []
         {
             PointIndex index;
             Check(index.Insert(4, {1, 2}) && !index.Insert(4, {3, 4}));
             Check(index.Count() == 1 && index.Find(4) == Point{1, 2});
         }},
        {"duplicate positions and extreme IDs",
         []
         {
             PointIndex index;
             Check(index.Insert(0, {1, 2}));
             Check(index.Insert(std::numeric_limits<PointId>::max(), {1, 2}));
             Check(Ids(index.QueryRadius({1, 2}, 0)) ==
                   std::vector<PointId>{0, std::numeric_limits<PointId>::max()});
         }},
        {"capacity and duplicates",
         []
         {
             PointIndex index(1, 1);
             Check(index.Insert(1, {0, 0}) && !index.Insert(1, {1, 1}));
             Throws<std::length_error>([&] { (void)index.Insert(2, {0, 0}); });
             Check(index.Update(1, {2, 2}) && index.Remove(1) && index.Insert(2, {3, 3}));
         }},
        {"invalid insert is atomic",
         [&]
         {
             PointIndex index;
             for (const Point point : {Point{nan, 0}, {0, infinity}, {1e13, 0}, {0, -1e13}})
             {
                 Throws<std::invalid_argument>([&] { (void)index.Insert(1, point); });
                 Check(index.Count() == 0);
             }
         }},
        {"validation precedes duplicate or missing",
         [&]
         {
             PointIndex index;
             Check(index.Insert(1, {0, 0}));
             Throws<std::invalid_argument>([&] { (void)index.Insert(1, {nan, 0}); });
             Throws<std::invalid_argument>([&] { (void)index.Update(999, {nan, 0}); });
         }},
        {"update same cell",
         []
         {
             PointIndex index(10);
             Check(index.Insert(1, {1, 1}) && index.Update(1, {2, 2}));
             Check(index.QueryRadius({1, 1}, 0).empty());
             Check(Ids(index.QueryRadius({2, 2}, 0)) == std::vector<PointId>{1});
         }},
        {"update across cells",
         []
         {
             PointIndex index;
             Check(index.Insert(1, {-2, -2}) && index.Update(1, {20, 20}));
             Check(index.QueryRadius({-2, -2}, 1).empty());
             Check(Ids(index.QueryRadius({20, 20}, 0)) == std::vector<PointId>{1});
         }},
        {"invalid update preserves entry",
         [&]
         {
             PointIndex index;
             Check(index.Insert(1, {1, 2}));
             Throws<std::invalid_argument>([&] { (void)index.Update(1, {0, nan}); });
             Check(index.Find(1) == Point{1, 2});
         }},
        {"remove missing and existing",
         []
         {
             PointIndex index;
             Check(!index.Remove(5) && !index.Update(5, {0, 0}));
             Check(index.Insert(5, {0, 0}) && index.Remove(5) && !index.Remove(5));
             Check(All(index).empty());
         }},
        {"removal from shared bucket",
         []
         {
             PointIndex index(100);
             for (PointId id = 0; id < 20; ++id)
             {
                 Check(index.Insert(id, {1, 1}));
             }
             Check(index.Remove(7) && index.Remove(0) && index.Remove(19));
             Check(All(index).size() == 17 && !index.Contains(7));
         }},
        {"clear and reuse",
         []
         {
             PointIndex index(7, 20);
             Check(index.Insert(1, {2, 3}));
             index.Clear();
             Check(index.Count() == 0 && index.CellSize() == 7 && index.Capacity() == 20);
             Check(index.Insert(1, {3, 4}));
         }},
        {"inclusive rectangle edges",
         []
         {
             PointIndex index;
             Check(index.Insert(5, {-1, -1}) && index.Insert(2, {1, 1}) &&
                   index.Insert(3, {1.1, 1}));
             Check(Ids(index.QueryRectangle({-1, -1}, {1, 1})) == std::vector<PointId>{2, 5});
         }},
        {"zero-area rectangle",
         []
         {
             PointIndex index;
             Check(index.Insert(1, {0, 0}) && index.Insert(2, {0, 1}));
             Check(Ids(index.QueryRectangle({0, 0}, {0, 0})) == std::vector<PointId>{1});
         }},
        {"invalid rectangles including empty index",
         [&]
         {
             const PointIndex index;
             Throws<std::invalid_argument>([&] { (void)index.QueryRectangle({1, 0}, {0, 1}); });
             Throws<std::invalid_argument>([&] { (void)index.QueryRectangle({0, 1}, {1, 0}); });
             Throws<std::invalid_argument>([&] { (void)index.QueryRectangle({nan, 0}, {1, 1}); });
             Throws<std::invalid_argument>([&]
                                           { (void)index.QueryRectangle({0, 0}, {infinity, 1}); });
         }},
        {"negative cell boundaries",
         []
         {
             PointIndex index(1);
             Check(index.Insert(1, {-1, -1}) && index.Insert(2, {-0.999, -0.999}));
             Check(index.Insert(3, {-1.001, -1.001}));
             Check(Ids(index.QueryRectangle({-1, -1}, {0, 0})) == std::vector<PointId>{1, 2});
         }},
        {"positive cell boundaries",
         []
         {
             PointIndex index;
             Check(index.Insert(1, {0, 0}) && index.Insert(2, {1, 1}) && index.Insert(3, {2, 2}));
             Check(Ids(index.QueryRectangle({1, 1}, {2, 2})) == std::vector<PointId>{2, 3});
         }},
        {"Pythagorean circle and outside square",
         []
         {
             PointIndex index;
             Check(index.Insert(1, {3, 4}) && index.Insert(2, {4, 4}) && index.Insert(3, {-3, -4}));
             Check(Ids(index.QueryRadius({0, 0}, 5)) == std::vector<PointId>{1, 3});
         }},
        {"zero radius and negative zero",
         []
         {
             PointIndex index;
             Check(index.Insert(1, {0, 0}) && index.Insert(2, {0.001, 0}));
             Check(Ids(index.QueryRadius({-0.0, 0}, -0.0)) == std::vector<PointId>{1});
         }},
        {"invalid radii and centers",
         [&]
         {
             const PointIndex index;
             for (const auto radius : {-1.0, nan, infinity})
             {
                 Throws<std::invalid_argument>([&] { (void)index.QueryRadius({0, 0}, radius); });
             }
             Throws<std::invalid_argument>([&] { (void)index.QueryRadius({nan, 0}, 1); });
         }},
        {"huge finite radius",
         []
         {
             PointIndex index(Spatial::kMinCellSize);
             Check(index.Insert(1, {-1e12, -1e12}) && index.Insert(2, {1e12, 1e12}));
             Check(index.QueryRadius({1e12, 1e12}, std::numeric_limits<double>::max()).size() == 2);
         }},
        {"coordinate limits and enormous grid range",
         []
         {
             PointIndex index(Spatial::kMinCellSize);
             Check(index.Insert(1, {-1e12, 1e12}) && index.Insert(2, {1e12, -1e12}));
             Check(All(index).size() == 2);
         }},
        {"radius cancellation broad phase",
         []
         {
             PointIndex index(0.000001);
             const Point point{0.00005, 0};
             Check(std::hypot(point.X + 1e12, point.Y) == 1e12);
             Check(index.Insert(1, point));
             Check(Ids(index.QueryRadius({-1e12, 0}, 1e12)) == std::vector<PointId>{1});
         }},
        {"copied index independent",
         []
         {
             PointIndex index(4);
             Check(index.Insert(7, {1, 1}));
             PointIndex copy = index;
             Check(copy.Update(7, {20, 20}) && index.Find(7) == Point{1, 1});
             index = copy;
             copy.Clear();
             Check(index.Find(7) == Point{20, 20});
         }},
        {"move construction and reuse",
         []
         {
             PointIndex original(3);
             Check(original.Insert(1, {2, 2}));
             PointIndex moved(std::move(original));
             Check(moved.Find(1) == Point{2, 2} && original.Count() == 0);
             Check(original.Insert(2, {3, 3}));
         }},
        {"move assignment and self assignment",
         []
         {
             PointIndex first(3), second(7);
             Check(first.Insert(1, {2, 2}) && second.Insert(2, {3, 3}));
             second = std::move(first);
             Check(second.Contains(1) && !second.Contains(2) && first.Count() == 0);
             auto *same = &second;
             second = *same;
             second = std::move(*same);
             Check(second.Count() == 1);
         }},
        {"deterministic order independent of insertion",
         []
         {
             PointIndex first, second;
             for (PointId id = 1; id <= 100; ++id)
             {
                 Check(first.Insert(id, {1, 2}));
             }
             for (PointId id = 100; id > 0; --id)
             {
                 Check(second.Insert(id, {1, 2}));
             }
             Check(All(first) == All(second));
         }},
        {"concurrent const queries",
         []
         {
             PointIndex index;
             for (PointId id = 0; id < 100; ++id)
             {
                 Check(index.Insert(id, {1, 2}));
             }
             const PointIndex &readOnly = index;
             std::atomic<bool> valid{true};
             std::vector<std::thread> readers;
             for (int worker = 0; worker < 4; ++worker)
             {
                 readers.emplace_back(
                     [&]
                     {
                         for (int i = 0; i < 100; ++i)
                         {
                             if (readOnly.QueryRadius({1, 2}, 0).size() != 100)
                             {
                                 valid = false;
                             }
                         }
                     });
             }
             for (auto &reader : readers)
             {
                 reader.join();
             }
             Check(valid);
         }},
        {"2000 full-scan reference queries with mutation", ReferenceQueries}};
    for (const auto &[name, test] : cases)
    {
        try
        {
            test();
        }
        catch (const std::exception &error)
        {
            std::cerr << name << ": " << error.what() << '\n';
            return 1;
        }
    }
    std::cout << cases.size() << " spatial contract cases passed.\n";
}
