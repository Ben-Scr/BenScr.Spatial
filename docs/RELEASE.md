# Release verification

Use a clean checkout on the existing branch. PowerShell 7 and .NET SDK 10.0.303 are needed
only for the verification and NuGet-pack scripts; library consumers only need C++20.
`global.json` disables SDK roll-forward. Dependency restore uses the committed lock file.
CI pins CMake 3.31.6, NuGet.CommandLine 7.9.0 (SHA-256 verified before execution), and
GitHub Actions by immutable commit. CI uses Windows 2022/MSVC v143 and Ubuntu 24.04 with
GCC 14 and Clang 18. Hosted image patches may advance; build logs capture compiler versions.

```powershell
./tools/Verify.ps1 -Generator 'Visual Studio 17 2022' -Toolset v143
# Linux with CXX=g++-14 or clang++-18:
./tools/Verify.ps1 -Generator Ninja -Sanitizers
```

For another Visual Studio installation, pass its generator and `-CMake` executable path.
The verifier runs strict-warning Release builds, 32 contract cases (2,000 reference query comparisons), 120 allocation rollback attempts, and the standalone-header
check, the complete example, and an installed consumer from a different temporary prefix.
It packs the exact source commit, audits every archive entry, and tests real native NuGet
restoration on Windows using a fresh temporary package directory. Both Debug and Release
consumers compile the packaged `.cpp` through the imported targets. Linux runs ASan/UBSan
tests and a relocated consumer; the Windows-only native MSBuild path is explicitly skipped.

`artifacts/latest-verification.txt` identifies the chosen result directory. Its
`release-manifest.json` records the package SHA-256, source commit, verification results,
and intended tag `packages/BenScr.Spatial/1.0.0`. Dirty-tree checks produce development
evidence only. A ready package must come from a clean verified commit.

Before publication, require all three CI jobs to pass for the exact release commit; resolve
the tag to that commit, then publish only the explicitly selected verified `.nupkg`. Do not
repack or glob unrelated artifacts. NuGet indexing and signing are external steps, distinct
from a successful local pack. Verify the published package in a fresh consumer:

```powershell
./tools/VerifyNuGetConsumer.ps1 -Published -ExpectedPackage '<verified-package-path>'
```

The tool restores only from nuget.org with HTTP caching disabled, executes native Debug
and Release consumers, and compares README, license, icon, header, source, build imports,
and package metadata against the pre-publication archive. NuGet signing may change ZIP
metadata, so payload comparison is used. The resulting
`artifacts/nuget-consumer-published.json` is publication evidence. Never report publication
solely because local verification or source hosting passed.

macOS, ARM, x86, MSVC sanitizers, altered rounding modes, and fast-math are not part of the
executed matrix. No performance benchmark or cross-platform floating-point identity is
claimed. Exact evidence is attached to the CI run and release, not inferred from this guide.
