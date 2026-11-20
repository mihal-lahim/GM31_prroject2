# GM31_prroject2 — Claude 向けの作業ルール

プロジェクトの概要は [README.md](README.md)、ビルドとテストは [docs/development.md](docs/development.md) を参照。

## 役割分担(2エージェント)

- **メイン実装(このセッションの Claude)**: 依頼を実装し、検証・レビュー対応・コミット・プッシュまで行う。
- **レビュー担当(サブエージェント `code-reviewer`)**: コミット前の差分を読んで指摘するだけ。ファイルは変更しない。
  定義は [.claude/agents/code-reviewer.md](.claude/agents/code-reviewer.md)。

作業の流れ:

1. 実装する。
2. `scripts\verify.ps1` でビルドと自己テストを通す(下の「Git の運用」)。
3. `code-reviewer` に未コミットの差分のレビューを依頼する。依頼には「何のための変更か」と、特に見てほしい点を書く。
   文書の誤字だけ・コメントだけの変更はレビューを省いてよい。
4. 指摘に対応する。「重大」は必ず直す。「要修正」は直すか、直さない理由をユーザーへの報告に書く。
   「軽微 / 要確認」は判断して対応する。レビュアーの指摘が誤りだと判断したら、根拠を確かめたうえで採用しない。
5. 直したらもう一度 `verify.ps1` を通す。直した内容が大きければ再レビューを依頼する。
6. コミットしてプッシュし、ユーザーにはレビューで何が指摘され、どう対応したかも短く報告する。

## Git の運用

変更を加えたら、ユーザーに確認せずにコミットと `origin/master` へのプッシュまで行う。

- **ブランチ**: `master` に直接コミットし、`git push origin master` でプッシュする。作業ブランチや PR は作らない。
- **タイミング**: 依頼された作業が完了した時点。途中の状態はコミットしない。
- **まとまり**: 1コミット = 1つの意味のある変更。無関係な変更(例: 文書の修正と VS Code の設定)は別々のコミットに分ける。
- **ビルドが通る状態だけを送る**: ビルドエラーやテストの失敗がある状態はコミットもプッシュもしない。
  - コミット前に `powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify.ps1` を実行する。
    未コミットの変更から影響するアプリを判断し、CI と同じく Debug と Release の両方でビルドして `--self-test` を実行する
    (文書だけなら何もしない)。失敗したら直してから再実行し、直せなければコミットせずユーザーに報告する。
  - `git push` すると、git の pre-push フック([scripts/git-hooks/pre-push](scripts/git-hooks/pre-push))が
    プッシュするコミット(`<リモート>..HEAD`)を同じ方法で検証し、失敗したらプッシュを止める。
    フックは Claude Code から実行したときだけ動く(環境変数 `CLAUDECODE`)。人のプッシュには影響しない。
    `core.hooksPath` は SessionStart フック(`.claude/settings.json`)が毎回設定する。
  - プッシュの前に作業ツリーをきれいにしておく。ビルドに影響する未コミット・未追跡のファイルがあると、
    プッシュする内容と違うものを検証することになるので、フックはプッシュを止める。
  - 検証には数分かかることがあるので、`git push` は Bash の `timeout` を 600000 にして実行する。
  - 止められたら原因を直して新しいコミットを作る(force push や `--no-verify` で回避しない)。
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

## 外部レビュアー(Antigravity)との自動レビュー・修正サイクル

ユーザーから Antigravity(レビュー・指示担当)と連携して自動サイクルを回す指示を受けた場合は、通常の単体フローではなく次のプロトコルに従う。

- **役割分担**:
  - **Antigravity(レビュアー)**: コード調査・指摘・再レビューのみ行い、リポジトリのファイルは編集しない(`artifacts/review-cycle.md` のみ更新)。
  - **Claude Code(実装担当)**: 修正の実装、`scripts\verify.ps1` および必要に応じた `--render-test` の実行、承認後のコミット・`origin/master` へのプッシュを行う。
- **ハンドオフファイル**: `artifacts/review-cycle.md`(`.gitignore` 対象。**絶対に `git add` / コミットしない**)。
  先頭ヘッダーで状態を同期する:
  - `STATUS`: `NEEDS_FIX`(Claude のターン) / `READY_FOR_REVIEW`(Antigravity のターン) / `DONE`(終了)
  - `CYCLE` / `ROUND` / `UPDATED_AT`
  - `TURN`: `claude` / `reviewer` / `none`
  - `ACTION`: `IMPLEMENT` / `FIX_REVIEW` / `COMMIT_AND_START_NEXT` / `COMMIT_AND_DONE` / `REVIEW` / `DONE`
- **基本ループ**:
  1. `artifacts/review-cycle.md` が `STATUS: NEEDS_FIX` かつ `TURN: claude` になったら内容を読む。
  2. `ACTION: COMMIT_AND_START_NEXT` または `COMMIT_AND_DONE` の場合は、まず直前サイクルの承認済み変更を明示ファイル指定で `git add` し、`git commit -F`(件名と本文の間に空行を入れる)でコミットして `git push origin master` する(`COMMIT_AND_DONE` で追加の文書作業等が指示されていれば先にコミットを分け、1 回の `git push` でまとめて送る)。
  3. `COMMIT_AND_DONE` の場合は、プッシュ完了後に `artifacts/review-cycle.md` を `STATUS: DONE` / `TURN: none` / `ACTION: DONE` に書き換えて待機ループへ戻らず終了する。
  4. 新規サイクル(`IMPLEMENT` / `COMMIT_AND_START_NEXT`)や差し戻し(`FIX_REVIEW`)を実装する。
  5. `scripts\verify.ps1` を Debug / Release 両構成で通す。`Engine` のコア(ライフサイクル・ID・描画・シェーダー・リソース管理)や `AnimationLib` を変更した場合は、影響するアプリの Debug `--render-test`(例: `WireAction`, `ClassroomWalk`, `SlowAction`, `ShaderGallery`, `FpsGame` 等。`BattingCenter` には `--render-test` が無い)も実行し、終了コード 0・`errors: 0` を確認する。
  6. **重要(未コミット提出ルール)**: 実装・検証が終わっても**新しいサイクルの変更はコミットせず未コミットのまま残す**。`artifacts/review-cycle.md` の「実装担当からの報告」を更新し、ヘッダーを `STATUS: READY_FOR_REVIEW` / `TURN: reviewer` / `ACTION: REVIEW` に書き換えたうえで、以下の 10 秒ポーリング待機ループに入る:
     ```bash
     while ! grep -qE '^STATUS: (NEEDS_FIX|DONE)' artifacts/review-cycle.md; do sleep 10; done
     ```
  7. `STATUS: NEEDS_FIX` で抜けたら手順 1 に戻り、`STATUS: DONE` で抜けたら終了する。
- **開始の合図**: ユーザーが「外部レビュアーと自動サイクルを回して」「`CLAUDE.md` の外部レビュアー連携手順で進めて」等と伝えたら、まず `artifacts/review-cycle.md` を確認し、すでに `STATUS: NEEDS_FIX` / `TURN: claude` ならすぐ着手、それ以外なら上記 10 秒ポーリング待機ループに入って待機する。
