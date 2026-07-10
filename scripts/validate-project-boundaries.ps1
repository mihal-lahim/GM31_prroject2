$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

function Read-ProjectItems([string] $projectPath, [string] $itemName) {
    [xml] $project = Get-Content (Join-Path $root $projectPath)
    $namespace = New-Object System.Xml.XmlNamespaceManager($project.NameTable)
    $namespace.AddNamespace('msb', 'http://schemas.microsoft.com/developer/msbuild/2003')
    return $project.SelectNodes("//msb:$itemName", $namespace) | ForEach-Object { $_.Include }
}

[xml] $solution = Get-Content (Join-Path $root 'GM31_prroject2.slnx')
$actualProjects = @($solution.Solution.Project | ForEach-Object { $_.Path })
$expectedProjects = @('Engine.vcxproj', 'WireAction.vcxproj', 'BattingCenter.vcxproj')
if (Compare-Object $expectedProjects $actualProjects) {
    throw 'The solution must contain exactly Engine, WireAction, and BattingCenter.'
}

$gameHeaders = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
foreach ($projectPath in @('WireAction.vcxproj', 'BattingCenter.vcxproj')) {
    foreach ($header in Read-ProjectItems $projectPath 'ClInclude') {
        [void] $gameHeaders.Add((Split-Path $header -Leaf))
    }
}

$violations = @()
foreach ($relativeSource in Read-ProjectItems 'Engine.vcxproj' 'ClCompile') {
    $sourcePath = Join-Path $root $relativeSource
    foreach ($line in Get-Content $sourcePath) {
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

foreach ($projectPath in @('WireAction.vcxproj', 'BattingCenter.vcxproj')) {
    if ((Get-Content (Join-Path $root $projectPath) -Raw) -match 'assets\\\*\*\\\*\.\*') {
        throw "$projectPath uses a wildcard asset copy instead of an explicit manifest."
    }
}

Write-Host 'Project boundary validation passed.'
