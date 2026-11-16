#include "main.h"
#include "crashReporter.h"

#include <DbgHelp.h>
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

	void WriteLog(EXCEPTION_POINTERS* pointers)
	{
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

		int length = sprintf_s(buffer,
			"time: %s\r\n"
			"exception: 0x%08X (%s)\r\n"
			"address: %p\r\n"
			"module: %s\r\n"
			"offset: 0x%llX\r\n",
			timeText, static_cast<unsigned>(code), DescribeException(code),
			address, moduleName, static_cast<unsigned long long>(offset));
		if (code == EXCEPTION_ACCESS_VIOLATION && record != nullptr && record->NumberParameters >= 2)
		{
			length += sprintf_s(buffer + length, sizeof(buffer) - length,
				"access: %s at %p\r\n",
				record->ExceptionInformation[0] == 0 ? "read" : "write",
				reinterpret_cast<void*>(record->ExceptionInformation[1]));
		}
		length += sprintf_s(buffer + length, sizeof(buffer) - length,
			"dump: %s\r\n", g_DumpPath);

		DWORD written = 0;
		WriteFile(file, buffer, static_cast<DWORD>(length), &written, nullptr);
		CloseHandle(file);
	}

	void WriteDump(EXCEPTION_POINTERS* pointers)
	{
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
	char exePath[MAX_PATH] = {};
	GetModuleFileNameA(nullptr, exePath, MAX_PATH);
	char* lastSeparator = strrchr(exePath, '\\');
	if (lastSeparator != nullptr)
	{
		*(lastSeparator + 1) = '\0';
	}
	sprintf_s(g_LogPath, "%scrash.log", exePath);
	sprintf_s(g_DumpPath, "%scrash.dmp", exePath);

	SetUnhandledExceptionFilter(HandleUnhandledException);
}
}
