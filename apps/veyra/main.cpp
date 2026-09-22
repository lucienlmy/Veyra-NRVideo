#include <windows.h>
#include "veyra/Log.h"
#include "veyra/RuntimePaths.h"
#include "veyra/engine/FgCompatibilityProbe.h"
#include <shellapi.h>
#include <dbghelp.h>
#include <format>
#include <filesystem>
#include <string>
int runVeyraApp(HINSTANCE,int);
namespace {
// Last-chance handler: NGX/XeSS/driver faults are structured exceptions the
// C++ catch in the engine worker never sees, so the final 250 ms of buffered
// log and any crash context were lost (sweep 2026-09-22 B2). Write a
// minidump next to the log, flush the logger, then let Windows terminate.
std::wstring crashDumpDirectory;
LONG WINAPI crashFilter(EXCEPTION_POINTERS* info){
    static LONG entered=0;if(InterlockedIncrement(&entered)>1)return EXCEPTION_CONTINUE_SEARCH;
    const auto code=info&&info->ExceptionRecord?info->ExceptionRecord->ExceptionCode:0u;
    const auto address=info&&info->ExceptionRecord?reinterpret_cast<uintptr_t>(info->ExceptionRecord->ExceptionAddress):0u;
    veyra::log::error("crash",std::format("unhandled exception code=0x{:08X} address=0x{:X} thread={}",code,address,GetCurrentThreadId()));
    SYSTEMTIME st{};GetLocalTime(&st);
    wchar_t name[96]{};
    swprintf_s(name,L"veyra-crash-%04u%02u%02u-%02u%02u%02u-%lu.dmp",unsigned(st.wYear),unsigned(st.wMonth),unsigned(st.wDay),unsigned(st.wHour),unsigned(st.wMinute),unsigned(st.wSecond),GetCurrentProcessId());
    const std::wstring path=(std::filesystem::path(crashDumpDirectory)/name).wstring();
    HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file!=INVALID_HANDLE_VALUE){
        MINIDUMP_EXCEPTION_INFORMATION mei{GetCurrentThreadId(),info,FALSE};
        const BOOL ok=MiniDumpWriteDump(GetCurrentProcess(),GetCurrentProcessId(),file,MiniDumpWithIndirectlyReferencedMemory,info?&mei:nullptr,nullptr,nullptr);
        CloseHandle(file);
        veyra::log::error("crash",std::format("minidump written={} path={}",ok!=FALSE,std::filesystem::path(path).string()));
    }else veyra::log::error("crash",std::format("minidump open failed error={}",GetLastError()));
    veyra::Logger::instance().flush();
    if(GetEnvironmentVariableW(L"VEYRA_TEST_RAISE_SEH",nullptr,0))return EXCEPTION_EXECUTE_HANDLER;
    return EXCEPTION_CONTINUE_SEARCH;
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show){
    int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    if(argv&&argc==3&&std::wstring_view(argv[1])==L"--fg-compat-probe"){
        const auto mapping=reinterpret_cast<HANDLE>(_wcstoui64(argv[2],nullptr,10));LocalFree(argv);
        return veyra::engine::runFgCompatibilityProbe(mapping);
    }
    if(argv)LocalFree(argv);
    wchar_t executable[32768]{};
    if(GetModuleFileNameW(nullptr,executable,32768))veyra::engine::setFgCompatibilityProbeExecutable(executable);
    wchar_t logOverride[32768]{};
    const auto length=GetEnvironmentVariableW(L"VEYRA_LOG_FILE",logOverride,32768);
    auto logPath=length>0&&length<32768?std::filesystem::path(logOverride):veyra::runtime::logsDirectory()/L"veyra-app.log";
    // Preserve the failure preceding a restart. Concurrent GUI/test instances
    // must not truncate the active log or silently lose their own diagnostics.
    if(!veyra::Logger::instance().openFile(logPath.wstring(),true)){
        logPath=logPath.parent_path()/(L"veyra-app-"+std::to_wstring(GetCurrentProcessId())+L".log");
        (void)veyra::Logger::instance().openFile(logPath.wstring(),true);
    }
    veyra::log::info("app", "Veyra GUI session started");
    veyra::Logger::instance().flush();
    crashDumpDirectory=logPath.parent_path().wstring();
    SetUnhandledExceptionFilter(crashFilter);
    if(GetEnvironmentVariableW(L"VEYRA_TEST_RAISE_SEH",nullptr,0)){
        // Diagnostic: raise a structured exception after startup to verify
        // the dump/flush path; the filter then returns EXECUTE_HANDLER so the
        // process exits with the exception code instead of hanging on WER.
        veyra::log::info("app","VEYRA_TEST_RAISE_SEH: raising test exception");veyra::Logger::instance().flush();
        RaiseException(0xE0564559u,EXCEPTION_NONCONTINUABLE,0,nullptr);
    }
    const int result=runVeyraApp(instance,show);
    veyra::log::info("app", "Veyra GUI session stopped");
    veyra::Logger::instance().flush();
    return result;
}
