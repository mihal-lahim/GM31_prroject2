#!/usr/bin/env bash
# PreToolUse フック: Claude が git push を実行する前に、プッシュする変更をビルド・自己テストで検証する。
# 失敗したら終了コード 2 でプッシュを止め、理由を Claude に伝える(CLAUDE.md の「Git の運用」)。

input=$(cat)

# コマンドの先頭、または && ; || 改行の後に「git push」があるときだけ対象にする。
# (コミットメッセージの本文などに含まれる「git push」という文字では反応しない)
if ! printf '%s' "$input" | grep -Eq '"command"[[:space:]]*:[[:space:]]*"(([^"\\]|\\.)*(&&|;|\|\||\\n)[[:space:]]*)?git([[:space:]]+-c[[:space:]]+[^[:space:]]+)*[[:space:]]+push'; then
    exit 0
fi

cd "${CLAUDE_PROJECT_DIR:-.}" || exit 0

# プッシュ先との差分を対象にする。上流ブランチが無ければ全アプリを検証する。
if upstream=$(git rev-parse --verify --quiet '@{u}'); then
    args="-Since $upstream"
else
    args="-All"
fi

# PowerShell の出力を UTF-8 にしないと、日本語のメッセージが文字化けする。
log=$(powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "[Console]::OutputEncoding = [Text.Encoding]::UTF8; & ./scripts/verify.ps1 $args; exit \$LASTEXITCODE" 2>&1)
code=$?
if [ "$code" -eq 0 ]; then
    exit 0
fi

{
    echo "プッシュを止めました: scripts/verify.ps1 が失敗しました(終了コード $code)。"
    echo "ビルドエラーやテストの失敗を直してからコミットし直し、もう一度プッシュしてください。直せない場合はユーザーに報告してください。"
    echo "---- verify.ps1 の出力(末尾) ----"
    printf '%s\n' "$log" | tail -n 40
} >&2
exit 2
