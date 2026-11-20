#include "main.h"
#include "crashReporter.h"

#include <DbgHelp.h>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <ctime>

#pragma comment(lib, "dbghelp.lib")

namespace Engine
{
namespace
{
	// 例外発生時はヒープが壊れている可能性があるため、動的確保を避けて
	// 固定バッファとWin32 APIだけで書き出します。
	char g_LogPath[MAX_PATH] = {};
	char g_DumpPath[MAX_PATH] = {};

	const char* DescribeException(DWORD code)
	{
		switch (code)
		{
		case EXCEPTION_ACCESS_VIOLATION:         return "ACCESS_VIOLATION";
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "ARRAY_BOUNDS_EXCEEDED";
		case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "INT_DIVIDE_BY_ZERO";
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "FLT_DIVIDE_BY_ZERO";
		case EXCEPTION_FLT_INVALID_OPERATION:    return "FLT_INVALID_OPERATION";
		case EXCEPTION_STACK_OVERFLOW:           return "STACK_OVERFLOW";
		case EXCEPTION_ILLEGAL_INSTRUCTION:      return "ILLEGAL_INSTRUCTION";
		case EXCEPTION_BREAKPOINT:               return "BREAKPOINT";
		case 0xE06D7363:                         return "CPP_EXCEPTION (std::exception など)";
		default:                                 return "UNKNOWN";
		}
	}

	// bufferの末尾へ書式付きで追記し、lengthを進めます。収まらない分は切り捨て、書式の失敗(負の戻り値)では何もしません。
	// sprintf_sの戻り値をそのまま足すと、失敗時の-1でlengthが戻ってバッファの手前を指してしまうためです。
	void AppendFormat(char* buffer, std::size_t capacity, std::size_t& length, const char* format, ...)
	{
		if (capacity == 0 || length + 1 >= capacity)
		{
			return;
		}
		va_list arguments;
		va_start(arguments, format);
		const int written = std::vsnprintf(buffer + length, capacity - length, format, arguments);
		va_end(arguments);
		if (written < 0)
		{
			buffer[length] = '\0';
			return;
		}
		const std::size_t room = capacity - length - 1;
		length += static_cast<std::size_t>(written) < room ? static_cast<std::size_t>(written) : room;
	}

	// アクセス違反の種類です(EXCEPTION_RECORD::ExceptionInformation[0])。8はDEPによる実行の違反です。
	const char* DescribeAccess(ULONG_PTR kind)
	{
		switch (kind)
		{
		case 0:  return "read";
		case 1:  return "write";
		case 8:  return "execute";
		default: return "unknown";
		}
	}

	// 実行ファイルのフォルダーにfileNameを付けたパスをoutへ書きます。収まらなければ空のままにします。
	void BuildPathNextToExecutable(const char* directory, const char* fileName, char (&out)[MAX_PATH])
	{
		out[0] = '\0';
		// 実行ファイルの場所が分からないときは、作業フォルダーへ書かずに空のままにします。
		if (directory[0] == '\0')
		{
			return;
		}
		const int length = std::snprintf(out, MAX_PATH, "%s%s", directory, fileName);
		if (length < 0 || length >= MAX_PATH)
		{
			out[0] = '\0';
		}
	}

	void WriteLog(EXCEPTION_POINTERS* pointers)
	{
		if (g_LogPath[0] == '\0')
		{
			return;
		}
		HANDLE file = CreateFileA(g_LogPath, GENERIC_WRITE, 0, nullptr,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file == INVALID_HANDLE_VALUE)
		{
			return;
		}

		char buffer[1024];
		const EXCEPTION_RECORD* record = pointers != nullptr ? pointers->ExceptionRecord : nullptr;
		const DWORD code = record != nullptr ? record->ExceptionCode : 0;
		const void* address = record != nullptr ? record->ExceptionAddress : nullptr;

		// 発生アドレスが属するモジュールと、その先頭からのオフセットを出します。
		// マップファイルや逆アセンブルと突き合わせれば関数を特定できます。
		HMODULE module = nullptr;
		char moduleName[MAX_PATH] = "?";
		if (address != nullptr && GetModuleHandleExA(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			static_cast<LPCSTR>(address), &module))
		{
			GetModuleFileNameA(module, moduleName, MAX_PATH);
		}
		const uintptr_t offset = module != nullptr
			? reinterpret_cast<uintptr_t>(address) - reinterpret_cast<uintptr_t>(module)
			: 0;

		std::time_t now = std::time(nullptr);
		char timeText[64] = {};
		std::tm local{};
		localtime_s(&local, &now);
		std::strftime(timeText, sizeof(timeText), "%Y-%m-%d %H:%M:%S", &local);

		buffer[0] = '\0';
		std::size_t length = 0;
		AppendFormat(buffer, sizeof(buffer), length,
			"time: %s\r\n"
			"exception: 0x%08X (%s)\r\n"
			"address: %p\r\n"
			"module: %s\r\n"
			"offset: 0x%llX\r\n",
			timeText, static_cast<unsigned>(code), DescribeException(code),
			address, moduleName, static_cast<unsigned long long>(offset));
		if (code == EXCEPTION_ACCESS_VIOLATION && record != nullptr && record->NumberParameters >= 2)
		{
			AppendFormat(buffer, sizeof(buffer), length,
				"access: %s at %p\r\n",
				DescribeAccess(record->ExceptionInformation[0]),
				reinterpret_cast<void*>(record->ExceptionInformation[1]));
		}
		AppendFormat(buffer, sizeof(buffer), length, "dump: %s\r\n", g_DumpPath);

		DWORD written = 0;
		WriteFile(file, buffer, static_cast<DWORD>(length), &written, nullptr);
		CloseHandle(file);
	}

	void WriteDump(EXCEPTION_POINTERS* pointers)
	{
		if (g_DumpPath[0] == '\0')
		{
			return;
		}
		HANDLE file = CreateFileA(g_DumpPath, GENERIC_WRITE, 0, nullptr,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file == INVALID_HANDLE_VALUE)
		{
			return;
		}

		MINIDUMP_EXCEPTION_INFORMATION info{};
		info.ThreadId = GetCurrentThreadId();
		info.ExceptionPointers = pointers;
		info.ClientPointers = FALSE;

		// 呼び出し履歴とローカル変数が見える程度の情報量にし、ファイルを小さく保ちます。
		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
			static_cast<MINIDUMP_TYPE>(MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory),
			pointers != nullptr ? &info : nullptr, nullptr, nullptr);
		CloseHandle(file);
	}

	LONG WINAPI HandleUnhandledException(EXCEPTION_POINTERS* pointers)
	{
		WriteLog(pointers);
		WriteDump(pointers);
		OutputDebugStringA("CrashReporter: crash.log / crash.dmp を書き出しました。\n");
		// 既定の処理(プロセス終了)へ渡します。デバッガ接続時はデバッガが先に捕まえます。
		return EXCEPTION_CONTINUE_SEARCH;
	}
}

void CrashReporter::Install()
{
	// 実行ファイルの隣に置きます。作業ディレクトリに依存させません。
	// パスが長すぎて収まらないときは書き出し先を空にし、記録だけ諦めます
	// (sprintf_sで溢れると、無効パラメータのハンドラで起動直後に落ちるため)。
	char exePath[MAX_PATH] = {};
	const DWORD exeLength = GetModuleFileNameA(nullptr, exePath, MAX_PATH);
	if (exeLength == 0 || exeLength >= MAX_PATH)
	{
		exePath[0] = '\0';
	}
	char* lastSeparator = strrchr(exePath, '\\');
	if (lastSeparator != nullptr)
	{
		*(lastSeparator + 1) = '\0';
	}
	BuildPathNextToExecutable(exePath, "crash.log", g_LogPath);
	BuildPathNextToExecutable(exePath, "crash.dmp", g_DumpPath);

	SetUnhandledExceptionFilter(HandleUnhandledException);
}
}
