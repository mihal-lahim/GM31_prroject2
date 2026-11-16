#pragma once

namespace Engine
{
	// 未処理の例外(アクセス違反・0除算など)を捕まえ、実行ファイルの隣に
	// crash.log と crash.dmp を書き出します。
	//
	// 「特定の操作でゲームが落ちる」という報告を再現できないとき、
	// 発生側でこのファイルを取ってもらえば、例外コードと発生アドレス、
	// そしてダンプからの呼び出し履歴で原因箇所を特定できます。
	// ゲーム固有の型には依存しないため、両アプリで共通に使えます。
	class CrashReporter
	{
	public:
		// WinMainの最初で一度だけ呼びます。
		static void Install();
	};
}
