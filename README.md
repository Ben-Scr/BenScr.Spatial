# BenScr.Spatial

C++20 spatial hashing for 2D points: stable caller-supplied IDs, insertion, movement, removal,
and inclusive rectangle/radius queries. Results are copied and sorted by ID. The library
has no third-party runtime dependencies.

Created by Ben Schneider — https://github.com/Ben-Scr

```cpp
#include <benscr/spatial/point_index.hpp>

namespace Spatial = BenScr::Spatial;

Spatial::PointIndex index(2.0, 1000); // cell width, maximum point count
bool inserted = index.Insert(42, {3.0, 4.0});
auto nearby = index.QueryRadius({0.0, 0.0}, 5.0); // includes ID 42
bool moved = index.Update(42, {10.0, 2.0});
auto inside = index.QueryRectangle({9.0, 1.0}, {11.0, 3.0});
bool removed = index.Remove(42);
```

The [complete executable example](https://github.com/Ben-Scr/BenScr.Spatial/blob/master/examples/spatial.cpp)
checks query results, movement and removal and is run against both package delivery paths.

## Installation

For a Visual Studio native C++ project using `packages.config`:

```powershell
Install-Package BenScr.Spatial -Version 1.0.0
```

Select C++20 or newer. `build/native/BenScr.Spatial.props` adds the include directory and
`BenScr.Spatial.targets` compiles `src/point_index.cpp` with the consumer's toolchain.
Do not compile that source a second time. The package carries source rather than a prebuilt
ABI: architecture, CRT, configuration and generated debug symbols follow your project.
The verified native NuGet integration is MSVC v143, Windows x64, Debug and Release.
This is a native package; managed `PackageReference` integration is not a supported path.

For CMake, build and install the repository:

```sh
cmake -S . -B out/build -DCMAKE_BUILD_TYPE=Release
cmake --build out/build --config Release
cmake --install out/build --config Release --prefix /your/prefix
```

Then use the installed package, including after relocating the prefix:

```cmake
find_package(BenScrSpatial 1 CONFIG REQUIRED)
add_executable(app main.cpp)
target_link_libraries(app PRIVATE BenScr::Spatial)
```

Configure your application with `-DCMAKE_PREFIX_PATH=/your/prefix`. The installed static
library is specific to its compiler, CRT, architecture and configuration. Build it again
for other toolchains. `add_subdirectory` also exposes `BenScr::Spatial`, with tests and
examples disabled by default when included as a subproject.

## Contracts

| Operation | Behavior |
| --- | --- |
| `Insert(id, position)` | Adds a unique ID; an existing ID returns false without replacement. |
| `Update(id, position)` | Moves an existing ID, including across cells; a missing ID returns false. |
| `Remove(id)` | Returns whether an ID was removed; a missing ID has no effect. |
| `Find(id)` / `Contains(id)` | Copied optional position / membership test. |
| `QueryRectangle(min, max)` | All points satisfying inclusive coordinate comparisons on both axes. |
| `QueryRadius(center, radius)` | All points satisfying `hypot(dx, dy) <= radius`, inclusive. |
| `Clear()` | Empties the index and retains its cell size and capacity. |

All `uint64_t` IDs are valid, including zero and the maximum value. IDs are never generated
or recycled implicitly. Different IDs may share a position. Query results contain no
duplicates and are sorted numerically by ID, independent of insertion order and bucket
iteration order. Mutating results or a `Find` result never changes the index.

Coordinates must be finite and within `[-1e12,1e12]`. Cell size must be finite and in
`[1e-6,1e12]`; capacity must be between 1 and 1,000,000 points. Capacity limits entries,
not memory bytes, and does not preallocate them. A new entry beyond capacity throws
`std::length_error`; removing an entry frees a logical slot. Invalid coordinates, cell
sizes, reversed rectangle corners, negative/nonfinite radii throw `std::invalid_argument`.
Invalid capacity throws `std::length_error`. Insert and Update validate the position even
when the ID is duplicate or missing. Query validation runs even on an empty index.

A rectangle may have zero width/height. Radius may be zero or any finite nonnegative double,
including `double::max`. The center follows the coordinate bounds. The final distance uses
binary64 `std::hypot` with no added tolerance; it is not arbitrary-precision geometry.
The broad phase rounds outward to avoid losing points that the final rounded distance
accepts. At large coordinate magnitudes, small offsets may round to the same coordinate.
IEEE 754 binary64 is required; fast-math and altered rounding modes are unsupported.

## Ownership, failures and cost

The index owns copied positions, IDs and bucket storage. Copy construction and assignment
produce independent indexes. Move operations transfer contents and leave the source empty
and usable with a valid configuration; after move assignment its configuration may be the
destination's former configuration. Self-assignment, including self-move, has no effect.

Insert, Update and copy assignment leave the logical contents unchanged if allocation
fails. Queries may allocate output and temporary storage and propagate allocation failures.
The library exposes no borrowed views or iterators. Concurrent const calls are supported
while no mutation occurs; all mutation requires external synchronization. There are no
global mutable library objects or background threads.

Cells use `floor(coordinate/cellSize)`, including negative coordinates. Narrow rectangles
look up covered cells when their count is at most the number of occupied buckets. Wider
rectangles scan occupied buckets instead of walking potentially enormous empty grids.
Candidate points are then filtered by their actual coordinates. Radius queries use an
outward-rounded bounding rectangle followed by the distance test. Results are sorted.
This is useful when points are spread across cells; clustered inputs and very broad queries
can inspect every point. Hash lookup costs are expected, not worst-case constant-time promises.
Returned `k` entries require `O(k)` storage and `O(k log k)` sorting work.

## Verification

Requires CMake 3.28+, C++20 and PowerShell 7. Packaging uses pinned .NET SDK 10.0.303 and
checksum-verified NuGet.CommandLine 7.9.0; the native library itself needs no .NET runtime.

```powershell
pwsh -File tools/Verify.ps1 -Generator 'Visual Studio 17 2022' -Toolset v143
pwsh -File tools/VerifyNuGetConsumer.ps1 -Published
```

The verifier runs 32 contract cases, 2,000 full-scan reference queries with mutations,
120 allocation-failure/success attempts, a standalone-header check and the example.
It checks a relocated CMake installation, exact package contents and native NuGet consumers
in Debug and Release. CI runs Windows/MSVC and Linux/GCC 14/Clang 18, including Linux
ASan/UBSan. Each run's manifest records actual outcomes. macOS, ARM, 32-bit platforms,
MSVC sanitizers and ThreadSanitizer are unverified.

See [release procedure](https://github.com/Ben-Scr/BenScr.Spatial/blob/master/docs/RELEASE.md),
[migration notes](https://github.com/Ben-Scr/BenScr.Spatial/blob/master/MIGRATION.md) and
[provenance](https://github.com/Ben-Scr/BenScr.Spatial/blob/master/docs/PROVENANCE.md).
The original [license](https://github.com/Ben-Scr/BenScr.Spatial/blob/master/LICENSE.txt)
requires visible attribution to Ben Schneider as specified in that file. The shared
BenScr icon is embedded unchanged.
