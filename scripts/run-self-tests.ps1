# 各アプリの --self-test を実行し、1つでも失敗したら終了コード 1 を返します(CI とローカルの両方で使います)。
#   scripts\run-self-tests.ps1                                 6アプリすべて(Debug)
#   scripts\run-self-tests.ps1 -Configuration Release
#   scripts\run-self-tests.ps1 -Apps WireAction,SlowAction
#
# アプリは GUI(Windows サブシステム)の実行ファイルなので、PowerShell から .\App.exe と直接呼ぶと
# 終了を待たず、終了コードも取れません。そのため Start-Process -Wait で待ってから判定します。
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [string[]]$Apps = @("BattingCenter", "WireAction", "ClassroomWalk", "FpsGame", "SlowAction", "ShaderGallery")
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$failed = @()

foreach ($app in $Apps) {
    $dir = Join-Path $root "bin\x64\$Configuration\$app"
    $exe = Join-Path $dir "$app.exe"
    if (-not (Test-Path $exe)) {
        Write-Host "[FAIL] $app : $exe が見つかりません(先にビルドしてください)"
        $failed += $app
        continue
    }

    # 前回のログが残っていると結果を取り違えるので消してから実行します。
    Get-ChildItem -Path $dir -Filter "self-test-*.log" -ErrorAction SilentlyContinue | Remove-Item -Force
    $process = Start-Process -FilePath $exe -ArgumentList "--self-test" -WorkingDirectory $dir -Wait -PassThru -NoNewWindow
    $code = $process.ExitCode

    if ($code -eq 0) {
        Write-Host "[PASS] $app"
    } else {
        Write-Host "[FAIL] $app (終了コード $code)"
        $failed += $app
        # 失敗した項目だけログから出します。
        Get-ChildItem -Path $dir -Filter "self-test-*.log" -ErrorAction SilentlyContinue | ForEach-Object {
            Select-String -Path $_.FullName -Pattern "FAIL|unknown" | ForEach-Object { Write-Host "    $($_.Filename): $($_.Line)" }
        }
    }
}

if ($failed.Count -gt 0) {
    Write-Host "失敗: $($failed -join ', ')"
    exit 1
}
Write-Host "すべて成功 ($($Apps.Count) アプリ)"
exit 0
