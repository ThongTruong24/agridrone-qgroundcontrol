[CmdletBinding()]
param(
    [string]$BuildDirectory = "build/Desktop_Qt_6_11_1_MSVC2022_64bit-Debug",
    [string]$Configuration = "",
    [ValidateRange(1, 256)]
    [int]$Jobs = [Environment]::ProcessorCount
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "../..")).Path
if (-not [System.IO.Path]::IsPathRooted($BuildDirectory)) {
    $BuildDirectory = Join-Path $repositoryRoot $BuildDirectory
}
$BuildDirectory = [System.IO.Path]::GetFullPath($BuildDirectory)
$cachePath = Join-Path $BuildDirectory "CMakeCache.txt"
if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf)) {
    throw "Configure this build directory in Qt Creator first: $BuildDirectory"
}

$cache = @{}
foreach ($line in Get-Content -LiteralPath $cachePath) {
    if ($line -match '^([^#/:][^:]*):[^=]+=(.*)$') {
        $cache[$Matches[1]] = $Matches[2]
    }
}

$cmakePath = $cache["CMAKE_COMMAND"]
if (-not $cmakePath -or -not (Test-Path -LiteralPath $cmakePath -PathType Leaf)) {
    throw "The cached CMake executable is missing. Reconfigure this build in Qt Creator."
}
if (-not $cache["CMAKE_HOME_DIRECTORY"] -or
    [System.IO.Path]::GetFullPath($cache["CMAKE_HOME_DIRECTORY"]) -ne $repositoryRoot) {
    throw "This build directory belongs to a different source checkout: $BuildDirectory"
}
if (-not $Configuration) {
    $Configuration = $cache["CMAKE_BUILD_TYPE"]
    if (-not $Configuration) {
        $Configuration = "Debug"
    }
}
if (-not $cache["CMAKE_CONFIGURATION_TYPES"] -and $cache["CMAKE_BUILD_TYPE"] -ne $Configuration) {
    throw "This build uses $($cache['CMAKE_BUILD_TYPE']). Select its configuration or another build directory."
}

# Ninja builds need the MSVC SDK environment even when cl.exe is cached by absolute path.
$compilerPath = $cache["CMAKE_CXX_COMPILER"]
if ($compilerPath -and $compilerPath -match '^(.*)[\\/]VC[\\/]Tools[\\/]MSVC[\\/].*[\\/]cl\.exe$') {
    $vcVarsPath = Join-Path $Matches[1] "VC/Auxiliary/Build/vcvars64.bat"
    if (-not (Test-Path -LiteralPath $vcVarsPath -PathType Leaf)) {
        throw "Cannot find the Visual Studio x64 environment: $vcVarsPath"
    }
    $environmentLines = & $env:ComSpec /d /s /c "`"$vcVarsPath`" >nul && set"
    if ($LASTEXITCODE -ne 0) {
        throw "Visual Studio environment setup failed with exit code $LASTEXITCODE."
    }
    foreach ($line in $environmentLines) {
        $separatorIndex = $line.IndexOf("=")
        if ($separatorIndex -gt 0) {
            [Environment]::SetEnvironmentVariable(
                $line.Substring(0, $separatorIndex), $line.Substring($separatorIndex + 1), "Process")
        }
    }
}

Write-Host "Configuring QGC and resolving the latest MAVLink main..."
& $cmakePath -S $repositoryRoot -B $BuildDirectory
if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed with exit code $LASTEXITCODE. Build was not started."
}

Write-Host "Building QGC ($Configuration)..."
& $cmakePath --build $BuildDirectory --config $Configuration --parallel $Jobs
if ($LASTEXITCODE -ne 0) {
    throw "CMake build failed with exit code $LASTEXITCODE."
}
Write-Host "QGC build completed with the MAVLink revision selected during configure."
