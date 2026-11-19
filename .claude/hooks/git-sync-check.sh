#!/usr/bin/env bash
# Stop フック: Claude が応答を終える前に、未コミットの変更と未プッシュのコミットが無いか確かめる。
# 残っていれば終了を止め、CLAUDE.md の「Git の運用」に従ってコミットとプッシュをするよう Claude に伝える。

input=$(cat)

# このフックが止めた後の再停止では、二度止めない(無限ループ防止)。
if printf '%s' "$input" | grep -q '"stop_hook_active"[[:space:]]*:[[:space:]]*true'; then
    exit 0
fi

cd "${CLAUDE_PROJECT_DIR:-.}" || exit 0
git rev-parse --is-inside-work-tree >/dev/null 2>&1 || exit 0

changes=$(git status --porcelain)
ahead=$(git rev-list --count '@{u}..HEAD' 2>/dev/null || echo 0)

if [ -z "$changes" ] && [ "$ahead" = "0" ]; then
    exit 0
fi

reason="Git に反映していない作業があります。"
if [ -n "$changes" ]; then
    count=$(printf '%s\n' "$changes" | wc -l | tr -d ' ')
    reason="$reason 未コミットのファイルが ${count} 件あります。"
fi
if [ "$ahead" != "0" ]; then
    reason="$reason 未プッシュのコミットが ${ahead} 件あります。"
fi
reason="$reason CLAUDE.md の「Git の運用」に従い、今回の作業が完了していれば意味のまとまりごとにコミットして origin/master へプッシュしてください。作業途中・ビルドが通らない・ユーザーが作った変更などコミットすべきでない場合は、その理由をユーザーに伝えて終了してください。"

# JSON 文字列として安全にするため、" と \ をエスケープする。
escaped=$(printf '%s' "$reason" | sed 's/\\/\\\\/g; s/"/\\"/g')
printf '{"decision":"block","reason":"%s"}\n' "$escaped"
exit 0
