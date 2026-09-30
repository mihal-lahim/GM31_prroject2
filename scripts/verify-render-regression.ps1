param(
    [switch]$SkipBuild,
    # Capture the current render-test output as the new baseline instead of comparing.
    [switch]$UpdateBaseline,
    [string]$Configuration = "Debug",
    # Explicit MSBuild.exe. Mixing Visual Studio versions on one obj/ directory breaks the
    # precompiled header (C1853), so use the same one you build with in the IDE.
    [string]$MSBuild = ""
)

# Regression gate for engine changes that must not alter the picture:
#   build -> self-tests -> project boundaries -> render tests -> PNG + log comparison.
#
# Baseline lives in artifacts/render-baseline (gitignored, ~100MB). Capture it once on a
# known-good commit with -UpdateBaseline, then run without switches after each change.
# ASCII only on purpose: Windows PowerShell 5.1 reads BOM-less UTF-8 as ANSI.

$ErrorActionPreference = "Continue"
$repo = Split-Path -Parent $PSScriptRoot
$baseline = Join-Path $repo "artifacts\render-baseline"
$bin = Join-Path $repo "bin\x64\$Configuration"
$wireDir = Join-Path $bin "WireAction"
$classroomDir = Join-Path $bin "ClassroomWalk"

# ClassroomWalk draws a wall-clock FPS readout in the top-left; it never reproduces.
$fpsMask = "0,0,760,48"

function Find-MSBuild {
    if ($MSBuild -ne "") { return $MSBuild }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        # The projects use PlatformToolset v143, which is Visual Studio 2022 (17.x). Prefer it so a
        # newer side-by-side install does not rebuild with a different compiler.
        $found = & $vswhere -version "[17.0,18.0)" -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
        if ($found) { return $found }
        $found = & $vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
        if ($found) { return $found }
    }
    $command = Get-Command msbuild -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    return $null
}

$failures = @()

function Step($name, [scriptblock]$body) {
    Write-Output ""
    Write-Output "=== $name ==="
    $global:LASTEXITCODE = 0
    & $body
    if ($LASTEXITCODE -ne 0) {
        $script:failures += $name
        Write-Output "FAILED: $name (exit $LASTEXITCODE)"
    }
}

function Compare-Log($expected, $actual) {
    if (-not (Test-Path $expected)) { Write-Output "missing baseline: $expected"; $global:LASTEXITCODE = 1; return }
    if (-not (Test-Path $actual)) { Write-Output "missing output: $actual"; $global:LASTEXITCODE = 1; return }
    $diff = Compare-Object (Get-Content $expected) (Get-Content $actual)
    if ($diff) { $diff | Format-Table -AutoSize | Out-String | Write-Output; $global:LASTEXITCODE = 1 }
    else { Write-Output "identical"; $global:LASTEXITCODE = 0 }
}

Push-Location $repo
try {
    if (-not $SkipBuild) {
        $msbuild = Find-MSBuild
        if (-not $msbuild) { Write-Output "MSBuild not found. Run from a Developer PowerShell or pass -SkipBuild."; exit 1 }
        Write-Output "MSBuild: $msbuild"
        Step "build" {
            & $msbuild "GM31_prroject2.slnx" "-p:Configuration=$Configuration" -p:Platform=x64 -m -v:q -nologo |
                Where-Object { $_ -notmatch "warning X3578" }
        }
        if ($failures.Count -gt 0) { Write-Output "`nBUILD FAILED - stopping."; exit 1 }
    }

    Step "WireAction --self-test" { & "$wireDir\WireAction.exe" --self-test | Out-Null }
    Step "BattingCenter --self-test" { & "$bin\BattingCenter\BattingCenter.exe" --self-test | Out-Null }
    Step "project boundaries" { & powershell -ExecutionPolicy Bypass -File "$PSScriptRoot\validate-project-boundaries.ps1" }

    Step "WireAction --render-test" { & "$wireDir\WireAction.exe" --render-test | Out-Null }
    Step "ClassroomWalk --render-test" { & "$classroomDir\ClassroomWalk.exe" --render-test | Out-Null }

    if ($UpdateBaseline) {
        if ($failures.Count -gt 0) { Write-Output "`nTests failed - refusing to capture a baseline."; exit 1 }
        Remove-Item -Recurse -Force $baseline -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force "$baseline\wire", "$baseline\classroom" | Out-Null
        Copy-Item "$wireDir\race-*.png" "$baseline\wire\"
        Copy-Item "$classroomDir\*.png" "$baseline\classroom\"
        Copy-Item "$wireDir\render-test-race.log" "$baseline\race.log"
        Copy-Item "$classroomDir\render-test-classroom.log" "$baseline\classroom.log"
        Write-Output "`nBaseline captured in $baseline"
        exit 0
    }

    if (-not (Test-Path $baseline)) {
        Write-Output "`nNo baseline at $baseline. Run with -UpdateBaseline on a known-good commit first."
        exit 1
    }

    Step "pixel compare: WireAction" {
        & powershell -ExecutionPolicy Bypass -File "$PSScriptRoot\compare-renders.ps1" `
            -Baseline "$baseline\wire" -Current $wireDir
    }
    Step "pixel compare: ClassroomWalk" {
        & powershell -ExecutionPolicy Bypass -File "$PSScriptRoot\compare-renders.ps1" `
            -Baseline "$baseline\classroom" -Current $classroomDir -IgnoreRect $fpsMask
    }
    Step "log compare: race" { Compare-Log "$baseline\race.log" "$wireDir\render-test-race.log" }
    Step "log compare: classroom" { Compare-Log "$baseline\classroom.log" "$classroomDir\render-test-classroom.log" }
}
finally {
    Pop-Location
}

Write-Output ""
Write-Output "================================"
if ($failures.Count -eq 0) {
    Write-Output "ALL CHECKS PASSED"
    exit 0
}
Write-Output "FAILURES: $($failures -join ', ')"
exit 1
