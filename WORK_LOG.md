# Development record

## 1.0.0

- Defined bounded point ownership, caller IDs, query ordering and inclusive boundaries.
- Implemented adaptive cell traversal to avoid enumeration of enormous empty grids.
- Added outward rounding around radius-query bounds after reproducing cancellation near
  a center of -1e12 and a radius of 1e12; the final hypot predicate remains authoritative.
- Added independent full-scan comparison queries, mutation/copy/move checks and controlled
  allocation failures for Insert, Update and copy assignment.
- Local MSVC v143 Release passed 32 contract cases, 2,000 reference query comparisons,
  standalone header and 120 allocation-failure/success attempts, plus the example.
- Full package and installed-consumer outcomes are recorded by `tools/Verify.ps1` in its
  exact per-run release manifest. CI configuration alone is not publication evidence.

NuGet publication and public-feed checks are separate from local verification. The release
manifest accompanying a published version identifies its exact source commit and artifacts.
