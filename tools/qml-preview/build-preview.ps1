[CmdletBinding()]
param(
    [string]$QtPath = "C:/QtS/6.11.1/msvc2022_64",
    [string]$BuildDirectory = "",
    [switch]$Run
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Find-Executable {
    param(
        [Parameter(Mandatory)]
        [string]$Name,

        [Parameter(Mandatory)]
        [string[]]$Candidates
    )

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    throw "Cannot find $Name. Install the Visual Studio C++ tools and Qt 6.11.1, or add $Name to PATH."
}

$sourceDirectory = (Resolve-Path -LiteralPath $PSScriptRoot).Path
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "../..")).Path
$qtDirectory = (Resolve-Path -LiteralPath $QtPath).Path

if (-not $BuildDirectory) {
    $BuildDirectory = Join-Path $repositoryRoot "build-qml-preview"
} elseif (-not [System.IO.Path]::IsPathRooted($BuildDirectory)) {
    $BuildDirectory = Join-Path (Get-Location).Path $BuildDirectory
}
$BuildDirectory = [System.IO.Path]::GetFullPath($BuildDirectory)

$vsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio/Installer/vswhere.exe"
$visualStudioDirectory = ""
if (Test-Path -LiteralPath $vsWhere -PathType Leaf) {
    $visualStudioDirectory = (& $vsWhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
}

$cmakePath = Find-Executable -Name "cmake" -Candidates @(
    $(if ($visualStudioDirectory) {
        Join-Path $visualStudioDirectory "Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
    })
)

$qtRoot = Split-Path (Split-Path $qtDirectory -Parent) -Parent
$ninjaPath = Find-Executable -Name "ninja" -Candidates @(
    (Join-Path $qtRoot "Tools/Ninja/ninja.exe")
)

if ($visualStudioDirectory) {
    $vcVarsPath = Join-Path $visualStudioDirectory "VC/Auxiliary/Build/vcvars64.bat"
    if (Test-Path -LiteralPath $vcVarsPath -PathType Leaf) {
        $environmentLines = & $env:ComSpec /d /s /c "`"$vcVarsPath`" >nul && set"
        foreach ($line in $environmentLines) {
            $separatorIndex = $line.IndexOf("=")
            if ($separatorIndex -gt 0) {
                $name = $line.Substring(0, $separatorIndex)
                $value = $line.Substring($separatorIndex + 1)
                [Environment]::SetEnvironmentVariable($name, $value, "Process")
            }
        }
    }
}

Write-Host "Configuring CC Telemetry Preview with Qt $qtDirectory"
& $cmakePath -S $sourceDirectory -B $BuildDirectory -G Ninja `
    "-DCMAKE_PREFIX_PATH=$qtDirectory" `
    "-DCMAKE_MAKE_PROGRAM=$ninjaPath"
if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed with exit code $LASTEXITCODE."
}

& $cmakePath --build $BuildDirectory
if ($LASTEXITCODE -ne 0) {
    throw "CMake build failed with exit code $LASTEXITCODE."
}

$executable = Join-Path $BuildDirectory "cc-qml-preview.exe"
Write-Host "Preview executable: $executable"

if ($Run) {
    $qtBinDirectory = Join-Path $qtDirectory "bin"
    $env:PATH = "$qtBinDirectory;$env:PATH"
    & $executable
}
