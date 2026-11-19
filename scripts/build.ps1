# VS Code などから、Developer PowerShell を開かずにビルドするためのスクリプトです。
#   scripts\build.ps1                         ソリューション全体(Debug)
#   scripts\build.ps1 -Target WireAction      1つのアプリと、それが参照するライブラリだけ
#   scripts\build.ps1 -Target WireAction,SlowAction   複数のアプリ
#   scripts\build.ps1 -Configuration Release
#   scripts\build.ps1 -Target WireAction -Rebuild
param(
    [string[]]$Target = @(),
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [switch]$Rebuild
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw "vswhere.exe が見つかりません。Visual Studio (C++ によるデスクトップ開発) をインストールしてください。"
}
$msbuild = & $vswhere -latest -prerelease -products * -requires Microsoft.Component.MSBuild `
    -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
if (-not $msbuild) {
    throw "MSBuild.exe が見つかりません。"
}

# 「WireAction,SlowAction」のように1つの文字列で渡されても分けて扱います。
$apps = @($Target | ForEach-Object { $_ -split "," } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
if ($apps.Count -eq 0) {
    $targetName = if ($Rebuild) { "Rebuild" } else { "Build" }
} elseif ($Rebuild) {
    $targetName = ($apps | ForEach-Object { "${_}:Rebuild" }) -join ";"
} else {
    $targetName = $apps -join ";"
}

# GenerateFullPaths: VS Code の「問題」パネルからエラー行へ飛べるよう、フルパスで出力します。
& $msbuild (Join-Path $root "GM31_prroject2.slnx") `
    "/t:$targetName" "/p:Configuration=$Configuration" "/p:Platform=x64" `
    "/p:GenerateFullPaths=true" "/m" "/nologo" "/v:minimal"
exit $LASTEXITCODE
