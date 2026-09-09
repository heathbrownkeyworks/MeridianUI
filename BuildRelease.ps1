param(
    [ValidateSet('release', 'debug')]
    [string]$preset = "release",
    [int]$threads,
    [switch]$buildTests = $false,
    [switch]$skipTests = $false,
    [switch]$noLTO = $false,
    [switch]$fresh
)
$ErrorActionPreference = "Stop"

$defaultThreads = 16

function Import-VsDevCmdEnvironment {
    param(
        [Parameter(Mandatory = $true)]
        [string]$VsDevCmdPath
    )

    if (-not (Test-Path $VsDevCmdPath)) {
        Write-Warning "VsDevCmd fallback not found at '$VsDevCmdPath'"
        return $false
    }

    Write-Host "Falling back to VsDevCmd environment import: $VsDevCmdPath" -ForegroundColor Yellow

    $cmdLine = 'call "' + $VsDevCmdPath + '" -arch=amd64 -host_arch=amd64 >nul && set'
    $envDump = cmd /c $cmdLine

    if ($LASTEXITCODE -ne 0 -or -not $envDump) {
        Write-Warning "VsDevCmd fallback failed to produce an environment"
        return $false
    }

    $seenPath = $false
    foreach ($line in $envDump) {
        $idx = $line.IndexOf('=')
        if ($idx -le 0) {
            continue
        }

        $name = $line.Substring(0, $idx)
        $value = $line.Substring($idx + 1)

        if ($name -ieq 'PATH') {
            if (-not $seenPath) {
                $env:Path = $value
                [System.Environment]::SetEnvironmentVariable('Path', $value, 'Process')
                $seenPath = $true
            }
            continue
        }

        if ($name -ieq 'Path') {
            continue
        }

        [System.Environment]::SetEnvironmentVariable($name, $value, 'Process')
    }

    return $true
}

# Load in template default variables
. (Join-Path $PSScriptRoot 'Build_Config_Template.ps1')

# Load in local variable overides
$localConfigPath = Join-Path $PSScriptRoot 'Build_Config_Local.ps1'
if (Test-Path -LiteralPath $localConfigPath) {
    . $localConfigPath
}
if (-not $threads) {
    if ($localDefaultThreads) {
        $threads = $localDefaultThreads
        Write-Host "Using thread count from local config: $threads"
    } else {
        $threads = $defaultThreads
        Write-Host "Using default thread count: $threads"
    }
}

Write-Host "Running preset $preset"

# Set up Visual Studio 2022 x64 environment
# Override with env var if set
if ($env:VS_DEV_SHELL_PATH) {
    if (Test-Path $env:VS_DEV_SHELL_PATH) {
        $vsDevShellPath = $env:VS_DEV_SHELL_PATH
        Write-Host "Using VS Dev Shell path from environment: $vsDevShellPath"
    }
}
# Verify the path exists
if (-not (Test-Path $vsDevShellPath)) {
    Write-Error "Visual Studio Dev Shell script not found at '$vsDevShellPath'. Please check the path."
    exit 1
}
# The VS dev shell - and the VsDevCmd fallback below - overwrite VCPKG_ROOT with
# Visual Studio's bundled vcpkg, which is usually older than the vcpkg this project's
# pinned registry baseline needs. CMakePresets.json expands $env{VCPKG_ROOT} into
# toolchainFile, so allowing that through silently swaps the toolchain, invalidates
# every cached package ABI, and forces a full from-source rebuild of all dependencies.
$savedVcpkgRoot = $env:VCPKG_ROOT

# Save current directory, launch VS dev shell, and return to original directory
$currentDirectory = $PWD.Path
& $vsDevShellPath -Arch amd64; Set-Location -Path "${currentDirectory}"

$requiredCommands = @("cl", "cmake", "git")
$missingCommands = @($requiredCommands | Where-Object { -not (Get-Command $_ -ErrorAction SilentlyContinue) })
if ($missingCommands.Count -gt 0) {
    Write-Host "Developer PowerShell bootstrap did not expose required commands: $($missingCommands -join ', ')" -ForegroundColor Yellow
    $vsDevCmdPath = Join-Path (Split-Path $vsDevShellPath -Parent) "VsDevCmd.bat"
    if (-not (Import-VsDevCmdEnvironment -VsDevCmdPath $vsDevCmdPath)) {
        Write-Error "Failed to import Visual Studio build environment. Missing commands: $($missingCommands -join ', ')"
        exit 1
    }

    $missingCommands = @($requiredCommands | Where-Object { -not (Get-Command $_ -ErrorAction SilentlyContinue) })
    if ($missingCommands.Count -gt 0) {
        Write-Error "Visual Studio build environment still missing required commands after fallback: $($missingCommands -join ', ')"
        exit 1
    }
}

# Restore VCPKG_ROOT if the Visual Studio environment setup replaced it (see note above).
if ($savedVcpkgRoot -and $env:VCPKG_ROOT -ne $savedVcpkgRoot) {
    Write-Host "Restoring VCPKG_ROOT to '$savedVcpkgRoot' (VS environment set it to '$env:VCPKG_ROOT')" -ForegroundColor Yellow
    $env:VCPKG_ROOT = $savedVcpkgRoot
}

$clCommand = Get-Command cl -ErrorAction SilentlyContinue
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
$gitCommand = Get-Command git -ErrorAction SilentlyContinue

if (-not $clCommand -or -not $cmakeCommand) {
    Write-Error "Build environment is not ready. cl.exe or cmake.exe is still unavailable."
    exit 1
}

if ($gitCommand) {
    $env:GIT = $gitCommand.Source
}

# Build cmake configure arguments
$cmakeArgs = @("-S", $PSScriptRoot, "--preset=$preset", "-DCMAKE_COMPILE_JOBS=$threads", "-Wno-dev")
$cmakeArgs += "-DCMAKE_CXX_COMPILER=$($clCommand.Source)"

if ($buildTests) {
    $cmakeArgs += "-DBUILD_TESTING=ON"
    Write-Host "Test compilation enabled (-buildTests flag)"
} else {
    $cmakeArgs += "-DBUILD_TESTING=OFF"
}

if ($skipTests) {
    Write-Host "Test execution disabled (-skipTests flag)"
}

if ($noLTO) {
    $cmakeArgs += "-DENABLE_LTO=OFF"
    Write-Host "LTO disabled (-noLTO flag) for faster link times"
} else {
    $cmakeArgs += "-DENABLE_LTO=ON"
}

if ($fresh) {
    $cmakeArgs += "--fresh"
}

$global:buildStartTime = Get-Date

& cmake $cmakeArgs
if ($LASTEXITCODE -ne 0) {
    Write-Host "cmake configure failed with exit code $LASTEXITCODE." -ForegroundColor Red
    exit $LASTEXITCODE
}


# Select the configuration explicitly for multi-config generators (Visual Studio)
# so the requested preset and its matching dependency libraries stay aligned.
$buildConfig = if ($preset -ieq "debug") { "Debug" } else { "Release" }

$buildDirectory = Join-Path $PSScriptRoot "build/$preset"
& cmake --build $buildDirectory --config $buildConfig --parallel $threads
if ($LASTEXITCODE -ne 0) {
    Write-Host "cmake build failed with exit code $LASTEXITCODE." -ForegroundColor Red
    exit $LASTEXITCODE
}

$build_time = (Get-Date) - $global:buildStartTime

# Run tests after build (unless -skipTests)
if ($buildTests -and -not $skipTests) {
    Write-Host "Running tests..."
    #Example to exclude perf tests:
    #& ctest --test-dir "build/$preset" --output-on-failure -LE "perf|performance"
    # --timeout 300 is a backstop only; no healthy test comes near it.
    & ctest --test-dir $buildDirectory -C $buildConfig --output-on-failure -j 2 --timeout 300 --progress --no-tests=error
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ctest failed with exit code $LASTEXITCODE." -ForegroundColor Red
        exit $LASTEXITCODE
    }
}

Write-Host "`nBuild time: $($build_time.TotalSeconds) seconds.`n" -ForegroundColor Green
