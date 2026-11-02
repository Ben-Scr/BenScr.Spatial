param(
    [string]$Source = 'https://api.nuget.org/v3/index.json',
    [switch]$Published,
    [string]$ExpectedPackage = ''
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (-not $IsWindows) { throw 'The native vcxproj consumer requires Windows and Visual Studio C++ tools.' }
if ($Published) { $Source = 'https://api.nuget.org/v3/index.json' }
if ($ExpectedPackage) { $ExpectedPackage = (Resolve-Path -LiteralPath $ExpectedPackage).ProviderPath }
$root = Split-Path -Parent $PSScriptRoot
$toolDirectory = Join-Path $root 'artifacts/tools'
New-Item -ItemType Directory -Force -Path $toolDirectory | Out-Null
$toolArchive = Join-Path $toolDirectory 'nuget.commandline.7.9.0.nupkg'
if (-not (Test-Path -LiteralPath $toolArchive)) {
    Invoke-WebRequest -Uri 'https://api.nuget.org/v3-flatcontainer/nuget.commandline/7.9.0/nuget.commandline.7.9.0.nupkg' -OutFile $toolArchive
}
if ((Get-FileHash -LiteralPath $toolArchive -Algorithm SHA256).Hash -ne 'FB2074E4427BBF8105F9A5C3DF6DD77105FB82D696C0FC72FC25DA903D1A63B2') { throw 'NuGet command-line tool checksum mismatch.' }
$nuget = Join-Path $toolDirectory 'nuget.exe'
$zip = [IO.Compression.ZipFile]::OpenRead($toolArchive)
try { [IO.Compression.ZipFileExtensions]::ExtractToFile($zip.GetEntry('tools/NuGet.exe'), $nuget, $true) }
finally { $zip.Dispose() }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = (& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
if (-not $installation) { throw 'Visual Studio C++ tools not found.' }
$msbuild = Join-Path $installation 'MSBuild/Current/Bin/MSBuild.exe'
$consumer = Join-Path ([IO.Path]::GetTempPath()) ('benscr-spatial-nuget-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $consumer | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'examples/spatial.cpp') -Destination (Join-Path $consumer 'main.cpp')
$escapedSource = [Security.SecurityElement]::Escape($Source)
@"
<configuration>
  <packageSources><clear /><add key="verified" value="$escapedSource" /></packageSources>
  <fallbackPackageFolders><clear /></fallbackPackageFolders>
</configuration>
"@ | Set-Content -LiteralPath (Join-Path $consumer 'NuGet.Config') -Encoding utf8
@'
<packages><package id="BenScr.Spatial" version="1.0.0" targetFramework="native" /></packages>
'@ | Set-Content -LiteralPath (Join-Path $consumer 'packages.config') -Encoding utf8
@'
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Release|x64"><Configuration>Release</Configuration><Platform>x64</Platform></ProjectConfiguration>
    <ProjectConfiguration Include="Debug|x64"><Configuration>Debug</Configuration><Platform>x64</Platform></ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals"><ProjectGuid>{7922CBE1-8F49-4FB0-8683-113C236D986C}</ProjectGuid><WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion></PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
  <PropertyGroup Label="Configuration">
    <ConfigurationType>Application</ConfigurationType><PlatformToolset>v143</PlatformToolset>
    <UseDebugLibraries Condition="'$(Configuration)'=='Debug'">true</UseDebugLibraries>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />
  <Import Project="packages\BenScr.Spatial.1.0.0\build\native\BenScr.Spatial.props" />
  <PropertyGroup><OutDir>$(ProjectDir)out\$(Configuration)\</OutDir><IntDir>$(ProjectDir)obj\$(Configuration)\</IntDir></PropertyGroup>
  <ItemDefinitionGroup><ClCompile><LanguageStandard>stdcpp20</LanguageStandard><WarningLevel>Level4</WarningLevel><TreatWarningAsError>true</TreatWarningAsError><AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions></ClCompile></ItemDefinitionGroup>
  <ItemGroup><ClCompile Include="main.cpp" /><None Include="packages.config" /></ItemGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />
  <Import Project="packages\BenScr.Spatial.1.0.0\build\native\BenScr.Spatial.targets" />
</Project>
'@ | Set-Content -LiteralPath (Join-Path $consumer 'Consumer.vcxproj') -Encoding utf8
Push-Location $consumer
try {
    & $nuget restore packages.config -PackagesDirectory packages -ConfigFile NuGet.Config -NoHttpCache -DirectDownload -NonInteractive -Verbosity quiet
    if ($LASTEXITCODE -ne 0) { throw 'Native package restore failed.' }
    foreach ($configuration in @('Release','Debug')) {
        & $msbuild Consumer.vcxproj /nologo /v:minimal "/p:Configuration=$configuration" /p:Platform=x64
        if ($LASTEXITCODE -ne 0) { throw "Native $configuration consumer build failed." }
        $output = & (Join-Path $consumer "out/$configuration/Consumer.exe")
        if ($LASTEXITCODE -ne 0 -or $output -ne 'Spatial index verified.') { throw "Native $configuration consumer output failed." }
    }
    $package = Join-Path $consumer 'packages/BenScr.Spatial.1.0.0/BenScr.Spatial.1.0.0.nupkg'
    if ($ExpectedPackage) {
        $expected = [IO.Compression.ZipFile]::OpenRead((Resolve-Path -LiteralPath $ExpectedPackage))
        $actual = [IO.Compression.ZipFile]::OpenRead($package)
        try {
            foreach ($entry in $expected.Entries) {
                if ($entry.FullName -notmatch '^(include/|src/|build/|README\.md$|LICENSE\.txt$|icon\.png$|BenScr\.Spatial\.nuspec$)') { continue }
                $other=$actual.GetEntry($entry.FullName)
                if (-not $other) { throw "Published package misses $($entry.FullName)" }
                $first=$entry.Open(); $second=$other.Open()
                try {
                    $hash1=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($first))
                    $hash2=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($second))
                    if ($hash1 -ne $hash2) { throw "Published payload mismatch: $($entry.FullName)" }
                } finally { $first.Dispose(); $second.Dispose() }
            }
        } finally { $expected.Dispose(); $actual.Dispose() }
    }
} finally { Pop-Location }
$evidence = [ordered]@{package_id='BenScr.Spatial';version='1.0.0';source=$Source;consumer='vcxproj_packages_config';configurations=@('Release|x64','Debug|x64');empty_package_directory=$true;passed=$true;payloads_compared=[bool]$ExpectedPackage;verified_at_utc=[DateTime]::UtcNow.ToString('o')}
$evidence | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $root ('artifacts/nuget-consumer-' + $(if ($Published) {'published'} else {'local'}) + '.json')) -Encoding utf8
Write-Output "Native NuGet consumer passed: $consumer"
