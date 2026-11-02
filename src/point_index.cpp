#include <benscr/spatial/point_index.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace BenScr::Spatial
{
static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53,
              "BenScr.Spatial requires IEEE 754 binary64 double.");

PointIndex::PointIndex(double cellSize, std::size_t capacity)
    : cellSize(cellSize), capacity(capacity)
{
    if (!std::isfinite(cellSize) || cellSize < kMinCellSize || cellSize > kMaxCoordinate)
    {
        throw std::invalid_argument("Cell size must be finite and in [1e-6,1e12].");
    }
    if (capacity == 0 || capacity > kMaxPoints)
    {
        throw std::length_error("Point capacity must be in [1,1000000].");
    }
}

PointIndex::PointIndex(PointIndex &&other) noexcept
    : cellSize(other.cellSize), capacity(other.capacity), points(std::move(other.points)),
      cells(std::move(other.cells))
{
    other.Clear();
}

PointIndex &PointIndex::operator=(const PointIndex &other)
{
    if (this != &other)
    {
        PointIndex copy(other);
        Swap(copy);
    }
    return *this;
}

PointIndex &PointIndex::operator=(PointIndex &&other) noexcept
{
    if (this != &other)
    {
        Swap(other);
        other.Clear();
    }
    return *this;
}

void PointIndex::Swap(PointIndex &other) noexcept
{
    std::swap(cellSize, other.cellSize);
    std::swap(capacity, other.capacity);
    points.swap(other.points);
    cells.swap(other.cells);
}

std::size_t PointIndex::Count() const noexcept
{
    return points.size();
}
std::size_t PointIndex::Capacity() const noexcept
{
    return capacity;
}
double PointIndex::CellSize() const noexcept
{
    return cellSize;
}
bool PointIndex::Contains(PointId id) const
{
    return points.contains(id);
}

std::optional<Point> PointIndex::Find(PointId id) const
{
    const auto entry = points.find(id);
    if (entry == points.end())
    {
        return std::nullopt;
    }
    return entry->second;
}

void PointIndex::ValidatePoint(Point point)
{
    if (!std::isfinite(point.X) || !std::isfinite(point.Y) || std::abs(point.X) > kMaxCoordinate ||
        std::abs(point.Y) > kMaxCoordinate)
    {
        throw std::invalid_argument("Point coordinates must be finite and within +/-1e12.");
    }
}

PointIndex::Cell PointIndex::CellOf(Point point) const noexcept
{
    // Validated bounds keep the quotient within +/-1e18, far from Int64 overflow.
    return {static_cast<std::int64_t>(std::floor(point.X / cellSize)),
            static_cast<std::int64_t>(std::floor(point.Y / cellSize))};
}

std::size_t PointIndex::CellHash::operator()(Cell cell) const noexcept
{
    auto mix = [](std::uint64_t value)
    {
        value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
        value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
        return value ^ (value >> 31);
    };
    return static_cast<std::size_t>(
        mix(static_cast<std::uint64_t>(cell.X)) ^
        mix(static_cast<std::uint64_t>(cell.Y) + 0x9E3779B97F4A7C15ULL));
}

void PointIndex::AddToCell(Cell cell, PointId id)
{
    auto [bucket, created] = cells.try_emplace(cell);
    try
    {
        bucket->second.push_back(id);
    }
    catch (...)
    {
        if (created)
        {
            cells.erase(bucket);
        }
        throw;
    }
}

void PointIndex::RemoveFromCell(Cell cell, PointId id)
{
    const auto bucket = cells.find(cell);
    auto &ids = bucket->second;
    const auto item = std::find(ids.begin(), ids.end(), id);
    *item = ids.back();
    ids.pop_back();
    if (ids.empty())
    {
        cells.erase(bucket);
    }
}

bool PointIndex::Insert(PointId id, Point position)
{
    ValidatePoint(position);
    if (points.contains(id))
    {
        return false;
    }
    if (points.size() == capacity)
    {
        throw std::length_error("Point index capacity reached.");
    }
    const auto [entry, inserted] = points.emplace(id, position);
    (void)inserted;
    try
    {
        AddToCell(CellOf(position), id);
    }
    catch (...)
    {
        points.erase(entry);
        throw;
    }
    return true;
}

bool PointIndex::Update(PointId id, Point position)
{
    ValidatePoint(position);
    const auto entry = points.find(id);
    if (entry == points.end())
    {
        return false;
    }
    const Cell oldCell = CellOf(entry->second);
    const Cell newCell = CellOf(position);
    if (oldCell != newCell)
    {
        AddToCell(newCell, id);
        RemoveFromCell(oldCell, id);
    }
    entry->second = position;
    return true;
}

bool PointIndex::Remove(PointId id)
{
    const auto entry = points.find(id);
    if (entry == points.end())
    {
        return false;
    }
    RemoveFromCell(CellOf(entry->second), id);
    points.erase(entry);
    return true;
}

void PointIndex::Clear() noexcept
{
    cells.clear();
    points.clear();
}

std::vector<Entry> PointIndex::QueryRectangle(Point minimum, Point maximum) const
{
    ValidatePoint(minimum);
    ValidatePoint(maximum);
    if (minimum.X > maximum.X || minimum.Y > maximum.Y)
    {
        throw std::invalid_argument("Rectangle minimum must not exceed maximum.");
    }
    std::vector<Entry> result;
    if (points.empty())
    {
        return result;
    }
    const Cell first = CellOf(minimum);
    const Cell last = CellOf(maximum);
    const auto width = static_cast<std::uint64_t>(last.X - first.X) + 1;
    const auto height = static_cast<std::uint64_t>(last.Y - first.Y) + 1;
    auto visit = [&](const std::vector<PointId> &ids)
    {
        for (const auto id : ids)
        {
            const auto point = points.at(id);
            if (point.X >= minimum.X && point.X <= maximum.X && point.Y >= minimum.Y &&
                point.Y <= maximum.Y)
            {
                result.push_back({id, point});
            }
        }
    };
    // Never enumerate more grid cells than the number of occupied buckets. Division avoids
    // overflow in enormous rectangle products; wide queries scan occupied cells instead.
    if (width <= static_cast<std::uint64_t>(cells.size()) / height)
    {
        for (auto y = first.Y; y <= last.Y; ++y)
        {
            for (auto x = first.X; x <= last.X; ++x)
            {
                const auto bucket = cells.find({x, y});
                if (bucket != cells.end())
                {
                    visit(bucket->second);
                }
            }
        }
    }
    else
    {
        for (const auto &[cell, ids] : cells)
        {
            if (cell.X >= first.X && cell.X <= last.X && cell.Y >= first.Y && cell.Y <= last.Y)
            {
                visit(ids);
            }
        }
    }
    std::sort(result.begin(), result.end(),
              [](const Entry &a, const Entry &b) { return a.Id < b.Id; });
    return result;
}

std::vector<Entry> PointIndex::QueryRadius(Point center, double radius) const
{
    ValidatePoint(center);
    if (!std::isfinite(radius) || radius < 0.0)
    {
        throw std::invalid_argument("Radius must be finite and nonnegative.");
    }
    Point minimum{-kMaxCoordinate, -kMaxCoordinate};
    Point maximum{kMaxCoordinate, kMaxCoordinate};
    if (radius < 4.0 * kMaxCoordinate)
    {
        // Round the broad phase outward: subtraction/addition rounding must not drop a
        // point accepted by the final hypot comparison at the inclusive boundary.
        const double outwardRadius = std::nextafter(radius, INFINITY);
        minimum = {std::max(-kMaxCoordinate, std::nextafter(center.X - outwardRadius, -INFINITY)),
                   std::max(-kMaxCoordinate, std::nextafter(center.Y - outwardRadius, -INFINITY))};
        maximum = {std::min(kMaxCoordinate, std::nextafter(center.X + outwardRadius, INFINITY)),
                   std::min(kMaxCoordinate, std::nextafter(center.Y + outwardRadius, INFINITY))};
    }
    auto result = QueryRectangle(minimum, maximum);
    std::erase_if(
        result, [center, radius](const Entry &entry)
        { return std::hypot(entry.Position.X - center.X, entry.Position.Y - center.Y) > radius; });
    return result;
}
} // namespace BenScr::Spatial
