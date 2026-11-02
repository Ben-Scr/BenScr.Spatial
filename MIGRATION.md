# Migration

BenScr.Spatial is a new library and does not replace an earlier published BenScr package.
Version 1.0.0 defines the initial public API in `include/benscr/spatial/point_index.hpp`.

The index stores points only, not rectangles, colliders or engine entities. Applications
associate IDs with their own objects and explicitly update positions. `Insert` does not
overwrite an existing ID; use `Update` for movement. Query boundaries are inclusive and
results are ordered by ID, not distance. A caller requiring nearest-neighbor ordering must
apply its own ordering to the returned values.

The package name is `BenScr.Spatial`, the C++ namespace is `BenScr::Spatial` and the exported
CMake target is `BenScr::Spatial`. No engine, GUI, .NET binding or shared base package is required.
