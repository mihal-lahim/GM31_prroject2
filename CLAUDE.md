# GM31_prroject2 — Claude 向けの作業ルール

プロジェクトの概要は [README.md](README.md)、ビルドとテストは [docs/development.md](docs/development.md) を参照。

## Git の運用

変更を加えたら、ユーザーに確認せずにコミットと `origin/master` へのプッシュまで行う。

- **ブランチ**: `master` に直接コミットし、`git push origin master` でプッシュする。作業ブランチや PR は作らない。
- **タイミング**: 依頼された作業が完了した時点。途中の状態はコミットしない。
- **まとまり**: 1コミット = 1つの意味のある変更。無関係な変更(例: 文書の修正と VS Code の設定)は別々のコミットに分ける。
- **ビルドが通る状態だけを送る**: ビルドエラーやテストの失敗がある状態はコミットもプッシュもしない。
  - コミット前に `powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify.ps1` を実行する。
    未コミットの変更から影響するアプリを判断し、CI と同じく Debug と Release の両方でビルドして `--self-test` を実行する
    (文書だけなら何もしない)。失敗したら直してから再実行し、直せなければコミットせずユーザーに報告する。
  - `git push` の直前には、フック(`.claude/hooks/pre-push-verify.sh`)がプッシュする全コミットを同じ方法で検証し、
    失敗したらプッシュを止める。止められたら原因を直して新しいコミットを作る(force push や `--no-verify` で回避しない)。
- **アプリの実行**: アプリは GUI の実行ファイルなので、PowerShell から `.\App.exe --self-test` と直接呼ぶと終了を待たず結果が取れない。
  `scripts\run-self-tests.ps1` か `Start-Process -Wait -PassThru` を使う。
- **メッセージ**: 日本語で「対象: 内容」の形にする。対象はゲーム名・`Engine`・`文書`・`ビルド` など。
  例: `WireAction: 敵の巡回範囲を踊り場の端で折り返す`、`文書: 文書間の矛盾を修正`。
  1行目で「何をなぜ」が分かるようにし、必要なら空行の後に詳細を書く。
- **含めないもの**: ビルド出力(`bin/`、`obj/`)、テストのログや画像、`.claude/settings.local.json`、秘密情報。
  `git add -A` ではなく、変えたファイルを指定して `git add` する。
- **ユーザーの変更**: Claude が触っていない未コミットの変更が見つかったら、勝手にコミットせずユーザーに確認する。
- **プッシュが失敗したら**: `git pull --rebase origin master` で取り込み直してから再度プッシュする。
  衝突したら自分で解決せず、状況をユーザーに報告する。force push はしない。
