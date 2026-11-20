# 変更したファイルから影響するアプリを判断し、CI と同じ確認(プロジェクト境界の検証、Debug と Release の
# ビルド、--self-test)をそのアプリについて行います。CLAUDE.md の「Git の運用」で使います。
#   scripts\verify.ps1                          未コミットの変更(未追跡のファイルを含む)が対象。コミット前に使う
#   scripts\verify.ps1 -Since origin/master     そのコミット以降の変更 + 未コミットの変更が対象
#   scripts\verify.ps1 -Pushed <上流のSHA>       <上流>..HEAD のコミットが対象。git の pre-push フックが使う
#   scripts\verify.ps1 -All                     6アプリすべて
#   scripts\verify.ps1 -DryRun                  対象のアプリを表示するだけ
#   scripts\verify.ps1 -Configuration Debug     片方の構成だけ(既定は CI と同じく Debug と Release の両方)
param(
    [string]$Since = "HEAD",
    [string]$Pushed = "",
    [switch]$All,
    [switch]$DryRun,
    [ValidateSet("Both", "Debug", "Release")]
    [string]$Configuration = "Both"
)

# ネイティブコマンド(git・msbuild)の stderr で止まらないよう、終了コードで判定します。
$ErrorActionPreference = "Continue"
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
$projectText = @{}
foreach ($app in $allApps) {
    $projectText[$app] = [IO.File]::ReadAllText((Join-Path $root "$app.vcxproj"))
}

function Get-AffectedApps([string]$path) {
    $p = $path -replace "\\", "/"
    # RuntimeAsset などで .vcxproj に名前が書かれているファイルは、場所に関係なくそのアプリに影響します
    # (例: THIRD_PARTY.md や licenses/ は各アプリの出力へコピーされます)。
    $needle = $p -replace "/", "\"
    $listed = @($allApps | Where-Object { $projectText[$_].Contains($needle) })

    switch -Regex ($p) {
        '^(src|include)/engine/'                           { return $allApps }
        '^(Common|SurfaceShaders)\.props$'                 { return $allApps }
        '^Directory\.Build\.(props|targets)$'              { return $allApps }
        '^Engine\.vcxproj'                                 { return $allApps }
        '^GM31_prroject2\.slnx$'                           { return $allApps }
        '^shaders/'                                        { return $allApps }
        '^(src|include)/animation/|^AnimationLib\.vcxproj' { return $animationApps }
        '^third_party/imgui/|^ImGuiLib\.vcxproj'           { return $imguiApps }
        '^third_party/|^lib/'                              { return $allApps }
        '^(src|include)/shared/'                           { return $sharedApps }
        '^(src|include)/games/([^/]+)/' {
            $app = $gameDirs[$Matches[2]]
            if ($app) { return @($app) }
            return $allApps
        }
        '^([A-Za-z]+)\.vcxproj' {
            if ($allApps -contains $Matches[1]) { return @($Matches[1]) }
            return $allApps
        }
        # 実行時アセットは、RuntimeAsset に登録しているアプリにだけ影響します(未登録なら影響なし)。
        '^(assets|licenses)/'                              { return $listed }
    }
    if ($listed.Count -gt 0) { return $listed }

    # ビルドに使わないと分かっているものだけを「影響なし」にします。それ以外の知らない場所は、
    # 安全側に倒して全アプリを対象にします。
    if ($p -match '^(docs|scripts|\.vscode|\.claude|\.github)/' -or
        $p -match '^[^/]+\.md$' -or
        $p -match '^\.(editorconfig|gitattributes|gitignore)$') {
        return @()
    }
    return $allApps
}

# git の出力を取り出します。失敗(存在しない参照など)したら検証全体を失敗にします。
function Invoke-GitLines([string[]]$arguments) {
    # core.quotepath=false: 日本語などのパスを "\343..." と引用符付きにしないため。
    # core.safecrlf=false: 改行コード変換の警告(LF will be replaced by CRLF)を出さないため。
    $output = & git -c core.quotepath=false -c core.safecrlf=false @arguments
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[FAIL] git $($arguments -join ' ') が失敗しました(終了コード $LASTEXITCODE)。"
        exit 1
    }
    return @($output | Where-Object { $_ })
}

# .vcxproj と .props に名前が書かれたファイル(ソース・シェーダー・実行時アセット)が、すべて git で追跡されているか
# 確かめます。手元にはあるのに追跡されていないファイル(git add 忘れや、.gitignore の bin/ などに当たる
# 新しい DLL)は、手元のビルドは通っても CI では失敗します。
function Test-ProjectFilesTracked {
    $tracked = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    foreach ($line in (Invoke-GitLines @("ls-files"))) { [void]$tracked.Add(($line -replace "\\", "/")) }

    $missing = @()
    $projectFiles = Get-ChildItem -Path $root -File | Where-Object { $_.Extension -in ".vcxproj", ".props" }
    foreach ($project in $projectFiles) {
        $text = [IO.File]::ReadAllText($project.FullName)
        $pattern = '<(ClCompile|ClInclude|FxCompile|RuntimeAsset|None|Image|Text)\s+Include="([^"]+)"'
        foreach ($match in [regex]::Matches($text, $pattern)) {
            $path = $match.Groups[2].Value
            # ワイルドカードや MSBuild のプロパティを含むものは、ファイル1つを指さないので対象外です。
            if ($path -match '[\*\?]|\$\(') { continue }
            $normalized = $path -replace "\\", "/"
            if (-not $tracked.Contains($normalized)) { $missing += "$($project.Name): $path" }
        }
    }
    if ($missing.Count -gt 0) {
        Write-Host "[FAIL] プロジェクトに登録されているのに git で追跡されていないファイルがあります(CI では見つからず失敗します)。"
        Write-Host "       git add するか(.gitignore に当たるなら git add -f)、プロジェクトから外してください。"
        $missing | Sort-Object -Unique | ForEach-Object { Write-Host "       $_" }
        exit 1
    }
}

Push-Location $root
try {
    if ($All) {
        $files = @("(すべて)")
        $apps = $allApps
    } elseif ($Pushed) {
        # プッシュするのはコミットだけです。対象は <上流>..HEAD の差分で決めます。
        # 比べる相手が無いとき(新しいブランチなど)は、フックが "all" を渡すので全アプリにします。
        if ($Pushed -eq "all") {
            $files = @("(すべて)")
            $apps = $allApps
        } else {
            $files = @(Invoke-GitLines @("diff", "--no-renames", "--name-only", $Pushed, "HEAD"))
            $apps = @($files | ForEach-Object { Get-AffectedApps $_ } | Sort-Object -Unique)
        }

        # ビルドは作業ツリーで行うので、ビルドに影響する未コミット・未追跡のファイルがあると、プッシュする
        # コミットとは違う内容を検証してしまいます(git add 忘れを見逃す・作業中の変更で無関係に止まる)。
        # その場合は検証せずに止めます。文書など影響のないファイルは残っていても構いません。
        # scripts/ はアプリには影響しませんが、検証そのもの(このスクリプトや pre-push フック)が作業ツリーの
        # ものを使うので、未コミットの変更があれば止めます。
        $dirty = @(Invoke-GitLines @("diff", "--no-renames", "--name-only", "HEAD")) + @(Invoke-GitLines @("ls-files", "--others", "--exclude-standard"))
        $blocking = @($dirty | Sort-Object -Unique | Where-Object { $_ -match '^scripts/' -or @(Get-AffectedApps $_).Count -gt 0 })
        if ($blocking.Count -gt 0) {
            Write-Host "[FAIL] ビルドに影響する未コミット・未追跡のファイルがあります。プッシュするコミットと作業ツリーが"
            Write-Host "       違うため、正しく検証できません。コミットし忘れていないか確かめ、コミットするか退避してください。"
            $blocking | ForEach-Object { Write-Host "       $_" }
            exit 1
        }
    } else {
        $files = @(Invoke-GitLines @("diff", "--no-renames", "--name-only", $Since)) + @(Invoke-GitLines @("ls-files", "--others", "--exclude-standard"))
        $files = @($files | Sort-Object -Unique)
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

Push-Location $root
try { Test-ProjectFilesTracked } finally { Pop-Location }

# CI の最初の手順と同じく、プロジェクトの境界(Engine がゲームのヘッダーを読まない、アセットの明示登録など)を確かめます。
Write-Host "==== プロジェクト境界の検証 ===="
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "validate-project-boundaries.ps1")
if ($LASTEXITCODE -ne 0) {
    Write-Host "[FAIL] プロジェクト境界の検証に失敗しました。"
    exit 1
}

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
