#ifndef BENSCR_SPATIAL_POINT_INDEX_HPP
#define BENSCR_SPATIAL_POINT_INDEX_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace BenScr::Spatial
{
inline constexpr double kMaxCoordinate = 1'000'000'000'000.0;
inline constexpr double kMinCellSize = 0.000001;
inline constexpr std::size_t kMaxPoints = 1'000'000;
using PointId = std::uint64_t;

/// Cartesian coordinates in caller-selected units, finite and within +/-kMaxCoordinate.
struct Point
{
    double X = 0.0;
    double Y = 0.0;
    bool operator==(const Point &) const = default;
};

/// A copied result. All IDs, including zero and UInt64 maximum, are valid.
struct Entry
{
    PointId Id = 0;
    Point Position{};
    bool operator==(const Entry &) const = default;
};

/// Owning uniform-grid point index. IDs are supplied by the caller and unique per index.
/// Concurrent const calls are safe only while no thread mutates the index. Mutation needs
/// external synchronization. Copies are independent; moved-from objects are empty and usable.
class PointIndex
{
  public:
    /// cellSize must be finite and in [kMinCellSize,kMaxCoordinate]; capacity in [1,kMaxPoints].
    /// Throws invalid_argument for invalid cellSize, length_error for invalid capacity.
    explicit PointIndex(double cellSize = 1.0, std::size_t capacity = kMaxPoints);
    PointIndex(const PointIndex &) = default;
    PointIndex(PointIndex &&other) noexcept;
    PointIndex &operator=(const PointIndex &other);
    PointIndex &operator=(PointIndex &&other) noexcept;
    ~PointIndex() = default;

    [[nodiscard]] std::size_t Count() const noexcept;
    [[nodiscard]] std::size_t Capacity() const noexcept;
    [[nodiscard]] double CellSize() const noexcept;
    [[nodiscard]] bool Contains(PointId id) const;
    /// Returns a copy, or nullopt for a missing ID. No view into the index is exposed.
    [[nodiscard]] std::optional<Point> Find(PointId id) const;

    /// Validates position first, then returns false for duplicate IDs without replacing them.
    /// A new entry beyond capacity throws length_error. Allocation failures leave state unchanged.
    [[nodiscard]] bool Insert(PointId id, Point position);
    /// Validates position first; returns false for a missing ID. Allocation failures leave state
    /// unchanged.
    [[nodiscard]] bool Update(PointId id, Point position);
    /// Returns whether an entry was removed. Missing IDs have no effect.
    [[nodiscard]] bool Remove(PointId id);
    /// Removes all points, retaining cell size and capacity.
    void Clear() noexcept;

    /// Inclusive axis-aligned bounds. Both corners must be valid points, minimum <= maximum.
    /// Returns copies sorted by ascending ID. Invalid bounds throw invalid_argument.
    [[nodiscard]] std::vector<Entry> QueryRectangle(Point minimum, Point maximum) const;
    /// Inclusive Euclidean distance: hypot(point.X-center.X, point.Y-center.Y) <= radius.
    /// Center must be valid, radius finite and >=0 (including zero). No tolerance is added.
    /// Returns copies sorted by ID; validation errors throw invalid_argument.
    [[nodiscard]] std::vector<Entry> QueryRadius(Point center, double radius) const;

  private:
    struct Cell
    {
        std::int64_t X;
        std::int64_t Y;
        bool operator==(const Cell &) const = default;
    };
    struct CellHash
    {
        std::size_t operator()(Cell cell) const noexcept;
    };
    [[nodiscard]] Cell CellOf(Point point) const noexcept;
    void AddToCell(Cell cell, PointId id);
    void RemoveFromCell(Cell cell, PointId id);
    void Swap(PointIndex &other) noexcept;
    static void ValidatePoint(Point point);

    double cellSize;
    std::size_t capacity;
    std::unordered_map<PointId, Point> points;
    std::unordered_map<Cell, std::vector<PointId>, CellHash> cells;
};
} // namespace BenScr::Spatial
#endif
