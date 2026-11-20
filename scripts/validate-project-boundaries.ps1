$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

function Read-ProjectItems([string] $projectPath, [string] $itemName) {
    [xml] $project = Get-Content (Join-Path $root $projectPath)
    $namespace = New-Object System.Xml.XmlNamespaceManager($project.NameTable)
    $namespace.AddNamespace('msb', 'http://schemas.microsoft.com/developer/msbuild/2003')
    return $project.SelectNodes("//msb:$itemName", $namespace) |
        Where-Object { -not [string]::IsNullOrWhiteSpace([string] $_.Include) } |
        ForEach-Object { [string] $_.Include }
}

function Normalize-RelativePath([string] $path) {
    $fullPath = [System.IO.Path]::GetFullPath((Join-Path $root $path))
    $rootPrefix = $root.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    if (-not $fullPath.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Asset path escapes the repository: $path"
    }
    return $fullPath.Substring($rootPrefix.Length).Replace('\', '/').ToLowerInvariant()
}

function Read-RuntimeAssets([string] $projectPath) {
    [xml] $project = Get-Content (Join-Path $root $projectPath)
    $namespace = New-Object System.Xml.XmlNamespaceManager($project.NameTable)
    $namespace.AddNamespace('msb', 'http://schemas.microsoft.com/developer/msbuild/2003')
    return $project.SelectNodes('//msb:RuntimeAsset', $namespace) | ForEach-Object {
        $targetPath = if ($_.TargetPath) { [string] $_.TargetPath } else { [string] $_.Include }
        [PSCustomObject]@{
            SourcePath = Normalize-RelativePath ([string] $_.Include)
            TargetPath = Normalize-RelativePath $targetPath
        }
    }
}

function Add-AssetReference([System.Collections.Generic.HashSet[string]] $references, [string] $path) {
    if ([string]::IsNullOrWhiteSpace($path)) {
        return
    }
    [void] $references.Add((Normalize-RelativePath $path))
}

[xml] $solution = Get-Content (Join-Path $root 'GM31_prroject2.slnx')
$actualProjects = @($solution.Solution.Project | ForEach-Object { $_.Path })
$expectedProjects = @('Engine.vcxproj', 'AnimationLib.vcxproj', 'ImGuiLib.vcxproj', 'WireAction.vcxproj', 'BattingCenter.vcxproj', 'ClassroomWalk.vcxproj', 'ShaderGallery.vcxproj', 'FpsGame.vcxproj', 'SlowAction.vcxproj')
if (Compare-Object $expectedProjects $actualProjects) {
    throw 'The solution must contain Engine, AnimationLib, ImGuiLib, WireAction, BattingCenter, ClassroomWalk, ShaderGallery, FpsGame, and SlowAction.'
}

$gameHeaders = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
foreach ($projectPath in @('WireAction.vcxproj', 'BattingCenter.vcxproj', 'ClassroomWalk.vcxproj', 'ShaderGallery.vcxproj', 'FpsGame.vcxproj', 'SlowAction.vcxproj')) {
    foreach ($header in Read-ProjectItems $projectPath 'ClInclude') {
        [void] $gameHeaders.Add((Split-Path $header -Leaf))
    }
}

$violations = @()
# ゲームに依存してはいけないライブラリ(Engine と AnimationLib)の .cpp と .h のどれも、
# ゲームのヘッダーを #include していないことを確かめます(ヘッダー経由の依存も見逃さないため)。
$libraryFiles = foreach ($projectPath in @('Engine.vcxproj', 'AnimationLib.vcxproj')) {
    foreach ($itemName in @('ClCompile', 'ClInclude')) {
        Read-ProjectItems $projectPath $itemName
    }
}
foreach ($relativeSource in $libraryFiles) {
    $sourcePath = Join-Path $root $relativeSource
    foreach ($line in Get-Content -LiteralPath $sourcePath) {
        if ($line -match '^\s*#include\s+"([^"]+)"') {
            $includedFile = Split-Path $Matches[1] -Leaf
            if ($gameHeaders.Contains($includedFile)) {
                $violations += "$relativeSource includes game header $includedFile"
            }
        }
    }
}
if ($violations.Count -gt 0) {
    throw ($violations -join [Environment]::NewLine)
}

foreach ($projectPath in @('WireAction.vcxproj', 'BattingCenter.vcxproj', 'ClassroomWalk.vcxproj', 'ShaderGallery.vcxproj', 'FpsGame.vcxproj', 'SlowAction.vcxproj')) {
    if ((Get-Content (Join-Path $root $projectPath) -Raw) -match 'assets\\\*\*\\\*\.\*') {
        throw "$projectPath uses a wildcard asset copy instead of an explicit manifest."
    }
}

# コードが直接参照するアセットと、OBJ/MTLから間接参照されるアセットを
# 明示的なRuntimeAsset一覧と照合します。実行時の「描画されない」をビルド前に検出します。
$engineSources = @(Read-ProjectItems 'Engine.vcxproj' 'ClCompile')
$engineSources = @($engineSources | Where-Object {
    -not [string]::IsNullOrWhiteSpace($_) -and (Split-Path $_ -Leaf) -ne 'engineSelfTests.cpp'
})
foreach ($projectPath in @('WireAction.vcxproj', 'BattingCenter.vcxproj', 'ClassroomWalk.vcxproj', 'ShaderGallery.vcxproj', 'FpsGame.vcxproj', 'SlowAction.vcxproj')) {
    $assets = @(Read-RuntimeAssets $projectPath)
    $targetPaths = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase)
    foreach ($asset in $assets) {
        [void] $targetPaths.Add($asset.TargetPath)
        if (-not (Test-Path -LiteralPath (Join-Path $root $asset.SourcePath) -PathType Leaf)) {
            $violations += "$projectPath RuntimeAsset source does not exist: $($asset.SourcePath)"
        }
    }

    $requiredAssets = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase)
    $compiledSources = @($engineSources) + @(Read-ProjectItems $projectPath 'ClCompile')
    foreach ($relativeSource in $compiledSources) {
        $sourcePath = Join-Path $root $relativeSource
        $sourceText = Get-Content -LiteralPath $sourcePath -Raw
        # Match complete quoted filenames, including spaces; ignore directory prefixes
        # used to construct a path dynamically in diagnostics.
        foreach ($match in [regex]::Matches($sourceText, '"(assets[\\/][^"\r\n]+\.[A-Za-z0-9]+)"')) {
            Add-AssetReference $requiredAssets $match.Groups[1].Value
        }
    }

    foreach ($asset in $assets) {
        $sourcePath = Join-Path $root $asset.SourcePath
        $targetDirectory = Split-Path $asset.TargetPath -Parent
        $extension = [System.IO.Path]::GetExtension($asset.SourcePath).ToLowerInvariant()
        if ($extension -eq '.obj') {
            foreach ($line in Get-Content -LiteralPath $sourcePath) {
                if ($line -match '^\s*mtllib\s+(.+?)\s*$') {
                    $dependencyTarget = Join-Path $targetDirectory $Matches[1]
                    Add-AssetReference $requiredAssets $dependencyTarget
                }
            }
        }
        elseif ($extension -eq '.mtl') {
            foreach ($line in Get-Content -LiteralPath $sourcePath) {
                if ($line -match '^\s*map_Kd\s+(.+?)\s*$') {
                    $dependencyTarget = Join-Path $targetDirectory $Matches[1]
                    Add-AssetReference $requiredAssets $dependencyTarget
                }
            }
        }
    }

    foreach ($requiredAsset in $requiredAssets) {
        if (-not $targetPaths.Contains($requiredAsset)) {
            $violations += "$projectPath is missing RuntimeAsset: $requiredAsset"
        }
    }
}

if ($violations.Count -gt 0) {
    throw ($violations -join [Environment]::NewLine)
}

Write-Host 'Project boundary validation passed.'
