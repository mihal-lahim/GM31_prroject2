# GM31_prroject2 — Antigravity(レビュー・指示担当)向けの作業ルール

プロジェクトの概要は [README.md](README.md)、ビルドとテストは [docs/development.md](docs/development.md)、実装担当のルールは [CLAUDE.md](CLAUDE.md) を参照。

## 役割分担(外部 Claude Code との 2 エージェント連携)

ユーザーから「エンジンの評価・レビューと修正サイクルを回して」等の指示を受けた場合は、外部の Claude Code 拡張機能(実装担当)と `artifacts/review-cycle.md` を介して自動ピンポン運用を行う。

- **Antigravity(このエージェント: レビュー・指示担当)**:
  - リポジトリのソースコード・設定・文書は**直接編集しない**(`artifacts/review-cycle.md` のみ読み書きする)。
  - ソースコードやスクリプトを実読して実在する欠陥・仕様バグ・安全性の問題を特定し、根拠(ファイル・行番号・再現条件)と完了条件を `artifacts/review-cycle.md` に書く。
  - Claude の修正差分(`git status --short` / `git diff`)を再レビューし、問題があれば差し戻し、LGTM なら承認と次サイクル(または終了)を指示する。
- **Claude Code(外部実装担当)**:
  - `artifacts/review-cycle.md` の指示に従って実装・`scripts\verify.ps1`(および必要に応じた Debug `--render-test`)を実行し、**未コミットのまま** `STATUS: READY_FOR_REVIEW` を返す。
  - Antigravity の LGTM 後にのみ、承認済み変更をコミットして `origin/master` へプッシュする。

## ハンドオフプロトコル(`artifacts/review-cycle.md`)

`artifacts/review-cycle.md` は `.gitignore` 対象の作業ファイルであり、コミット対象にしない。先頭 6 行のヘッダーでターンを管理する:

```text
STATUS: NEEDS_FIX | READY_FOR_REVIEW | DONE
CYCLE: <番号>
ROUND: <番号>
TURN: claude | reviewer | none
ACTION: IMPLEMENT | FIX_REVIEW | COMMIT_AND_START_NEXT | COMMIT_AND_DONE | REVIEW | DONE
UPDATED_AT: YYYY-MM-DD HH:MM
```

### Antigravity 側の手順

1. **サイクル開始 / 次の指摘作成**:
   - 現在の `git status` とコードを確認し、推測ではなく実読に基づく実在の欠陥を 2〜4 件程度に絞ってまとめる(回帰テストと必要な `--render-test` 条件も明記)。
   - `artifacts/review-cycle.md` に指摘事項と完了条件を書き、`STATUS: NEEDS_FIX` / `TURN: claude` / `ACTION: IMPLEMENT`(初回)または `COMMIT_AND_START_NEXT`(前サイクル承認と同時)を設定する。
   - **重要**: 指示文には「新サイクルの変更は未コミットのまま `STATUS: READY_FOR_REVIEW` にして 10 秒ポーリング待機へ戻る」ことを毎回明示する。
2. **待機(ポーリング)**:
   - シェルの `sleep` は使わず、`schedule` ツール(90〜150 秒)でタイマーを仕掛けてターンを終了し、自動復帰を待つ。
   - 復帰時はまず `artifacts/review-cycle.md` の先頭 10 行だけを読み、`STATUS: READY_FOR_REVIEW` または `DONE` でなければ再度 `schedule` で待つ(トークン節約)。
3. **再レビュー**:
   - `STATUS: READY_FOR_REVIEW` になったら、`artifacts/review-cycle.md` 全文と `git status --short` / `git diff` を確認する。
   - 欠陥・漏れ・仕様との衝突があれば `ACTION: FIX_REVIEW` / `ROUND` を +1 して `STATUS: NEEDS_FIX` で差し戻す。
   - 問題なければ LGTM とし、継続するなら `ACTION: COMMIT_AND_START_NEXT` で前サイクルのコミット・プッシュと次サイクルの実装を同時に指示する。
4. **終了手順(ユーザーから「次のプッシュで終了」等の指示があった場合)**:
   - 進行中サイクルの LGTM 後、`ACTION: COMMIT_AND_DONE` / `STATUS: NEEDS_FIX` / `TURN: claude` で最終コミット・プッシュと `STATUS: DONE` 書き込みを指示する。
   - `STATUS: DONE` と `git status` クリーン・`origin/master` へのプッシュ完了を確認したら、ユーザーへ要約を報告してタイマーを仕掛けず終了する。

## 次回からの起動方法

1. ユーザーが Antigravity に「自動レビュー・修正サイクルを回して(対象や終了条件があれば指定)」と指示する。
2. Antigravity が Cycle 1 の指摘を `artifacts/review-cycle.md` に書き出し、`schedule` で待機する。
3. Claude Code 側で「`CLAUDE.md` の外部レビュアー連携手順で進めて」と 1 回だけ伝えれば、以降は `STATUS: DONE` まで自動で往復する。
