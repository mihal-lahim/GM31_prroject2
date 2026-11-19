# 変更したファイルから影響するアプリを判断し、それだけをビルドして --self-test を実行します。
# コミット前の確認に使います(CLAUDE.md の「Git の運用」)。
#   scripts\verify.ps1                       未コミットの変更(追加・未追跡のファイルを含む)が対象
#   scripts\verify.ps1 -Since origin/master  そのコミット以降の変更 + 未コミットの変更が対象
#   scripts\verify.ps1 -All                  6アプリすべて
#   scripts\verify.ps1 -DryRun               対象のアプリを表示するだけ
#   scripts\verify.ps1 -Configuration Debug  片方の構成だけ(既定は CI と同じく Debug と Release の両方)
param(
    [string]$Since = "HEAD",
    [switch]$All,
    [switch]$DryRun,
    [ValidateSet("Both", "Debug", "Release")]
    [string]$Configuration = "Both"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$allApps = @("BattingCenter", "WireAction", "ClassroomWalk", "FpsGame", "SlowAction", "ShaderGallery")

# ライブラリ・フォルダと、それを使うアプリの対応です(各 .vcxproj の ProjectReference と ClCompile から)。
$animationApps = @("WireAction", "SlowAction", "ClassroomWalk")
$imguiApps = @("SlowAction", "ShaderGallery")
$sharedApps = @("BattingCenter", "WireAction", "ClassroomWalk", "FpsGame", "SlowAction")
$gameDirs = @{
    "wire" = "WireAction"; "slow" = "SlowAction"; "fps" = "FpsGame"
    "batting" = "BattingCenter"; "classroom" = "ClassroomWalk"; "shaderGallery" = "ShaderGallery"
}

function Get-AffectedApps([string]$path) {
    $p = $path -replace "\\", "/"
    switch -Regex ($p) {
        '^(src|include)/engine/'                          { return $allApps }
        '^(Common|SurfaceShaders)\.props$'                { return $allApps }
        '^Engine\.vcxproj'                                { return $allApps }
        '^GM31_prroject2\.slnx$'                          { return $allApps }
        '^shaders/'                                       { return $allApps }
        '^(src|include)/animation/|^AnimationLib\.vcxproj' { return $animationApps }
        '^third_party/imgui/|^ImGuiLib\.vcxproj'          { return $imguiApps }
        '^third_party/assimp/|^lib/'                      { return $allApps }
        '^(src|include)/shared/'                          { return $sharedApps }
        '^(src|include)/games/([^/]+)/' {
            $app = $gameDirs[$Matches[2]]
            if ($app) { return @($app) }
            return $allApps
        }
        '^([A-Za-z]+)\.vcxproj' {
            if ($allApps -contains $Matches[1]) { return @($Matches[1]) }
            return @()
        }
        '^assets/' {
            # 実行時アセットは、それを RuntimeAsset に登録しているアプリにだけ影響します。
            $needle = $path -replace "/", "\"
            return @($allApps | Where-Object {
                (Get-Content (Join-Path $root "$_.vcxproj") -Raw).Contains($needle)
            })
        }
    }
    # 文書・スクリプト・VS Code の設定などは、どのアプリのビルドにも影響しません。
    return @()
}

Push-Location $root
try {
    if ($All) {
        $files = @("(すべて)")
        $apps = $allApps
    } else {
        # core.safecrlf=false: 改行コード変換の警告(LF will be replaced by CRLF)を出さないため。
        $files = @(git -c core.safecrlf=false diff --name-only $Since) + @(git ls-files --others --exclude-standard)
        $files = @($files | Where-Object { $_ } | Sort-Object -Unique)
        $apps = @($files | ForEach-Object { Get-AffectedApps $_ } | Sort-Object -Unique)
    }
} finally {
    Pop-Location
}

Write-Host "変更されたファイル: $($files.Count) 件"
if ($apps.Count -eq 0) {
    Write-Host "影響するアプリはありません(ビルド・テスト不要)。"
    exit 0
}
Write-Host "対象のアプリ: $($apps -join ', ')"
if ($DryRun) { exit 0 }

# CI は Debug と Release の両方でビルドします。警告をエラーにしているので、片方だけ失敗することがあります。
$configurations = if ($Configuration -eq "Both") { @("Debug", "Release") } else { @($Configuration) }
foreach ($config in $configurations) {
    Write-Host "==== $config ===="
    & (Join-Path $PSScriptRoot "build.ps1") -Target $apps -Configuration $config
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[FAIL] $config のビルドに失敗しました。"
        exit 1
    }
    & (Join-Path $PSScriptRoot "run-self-tests.ps1") -Apps $apps -Configuration $config
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[FAIL] $config の自己テストに失敗しました。"
        exit 1
    }
}
Write-Host "検証成功: $($configurations -join ' と ') でビルドと自己テストが通りました。"
exit 0
