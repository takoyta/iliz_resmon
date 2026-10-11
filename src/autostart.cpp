// Elevation helpers and autostart via Task Scheduler (schtasks.exe + XML), no COM.
#include "common.h"
#include "autostart.h"

BOOL IsElevated() {
    HANDLE tk; TOKEN_ELEVATION e = {0}; DWORD n;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tk)) return FALSE;
    GetTokenInformation(tk, TokenElevation, &e, sizeof(e), &n);
    CloseHandle(tk);
    return e.TokenIsElevated != 0;
}

static DWORD Run(const WCHAR *file, const WCHAR *args, BOOL admin, BOOL wait) {   // exit code, or -1
    SHELLEXECUTEINFOW si = {sizeof(si)};
    si.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC;
    si.lpVerb = admin ? L"runas" : 0; si.lpFile = file; si.lpParameters = args;
    si.nShow = wait ? SW_HIDE : SW_SHOWNORMAL;
    if (!ShellExecuteExW(&si) || !si.hProcess) return (DWORD)-1;
    DWORD code = 0;
    if (wait && WaitForSingleObject(si.hProcess, 15000) == WAIT_OBJECT_0) GetExitCodeProcess(si.hProcess, &code);
    else if (wait) code = (DWORD)-1;
    CloseHandle(si.hProcess);
    return code;
}

static DWORD Schtasks(const WCHAR *args, BOOL admin) { return Run(L"schtasks.exe", args, admin && !IsElevated(), TRUE); }

BOOL AutostartEnabled() { return Schtasks(L"/Query /TN \"CpuTray\"", FALSE) == 0; }

static BOOL WriteXml(const WCHAR *path) {
    WCHAR exe[MAX_PATH], user[256], dom[128], *x = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, 8192 * sizeof(WCHAR));
    if (!x) return FALSE;
    GetModuleFileNameW(0, exe, MAX_PATH);
    DWORD n = 128; dom[0] = 0; GetEnvironmentVariableW(L"USERDOMAIN", dom, n);
    n = 128; WCHAR un[128] = {0}; GetEnvironmentVariableW(L"USERNAME", un, n);
    wsprintfW(user, L"%s\\%s", dom, un);
    const WCHAR *parts[] = {                             // wsprintf caps at 1024 chars -> concatenate
        L"\xFEFF<?xml version=\"1.0\" encoding=\"UTF-16\"?>\r\n"
        L"<Task version=\"1.2\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">\r\n"
        L"<Triggers><LogonTrigger><Enabled>true</Enabled><UserId>", user,
        L"</UserId></LogonTrigger></Triggers>\r\n<Principals><Principal id=\"Author\"><UserId>", user,
        L"</UserId><LogonType>InteractiveToken</LogonType><RunLevel>HighestAvailable</RunLevel></Principal></Principals>\r\n"
        L"<Settings><MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>"
        L"<DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries><StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>"
        L"<ExecutionTimeLimit>PT0S</ExecutionTimeLimit><Enabled>true</Enabled></Settings>\r\n"
        L"<Actions Context=\"Author\"><Exec><Command>", exe,
        L"</Command></Exec></Actions>\r\n</Task>\r\n"};
    x[0] = 0;
    for (int i = 0; i < (int)(sizeof(parts) / sizeof(parts[0])); i++) lstrcatW(x, parts[i]);
    int len = lstrlenW(x);
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    DWORD w = 0; BOOL ok = f != INVALID_HANDLE_VALUE && WriteFile(f, x, len * sizeof(WCHAR), &w, 0);
    if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
    HeapFree(GetProcessHeap(), 0, x);
    return ok;
}

void AutostartSet(BOOL on) {
    if (!on) { Schtasks(L"/Delete /TN \"CpuTray\" /F", TRUE); return; }
    WCHAR path[MAX_PATH], args[MAX_PATH + 64];
    GetTempPathW(MAX_PATH - 20, path); lstrcatW(path, L"cputray_task.xml");
    if (!WriteXml(path)) return;
    wsprintfW(args, L"/Create /TN \"CpuTray\" /XML \"%s\" /F", path);
    Schtasks(args, TRUE);
    DeleteFileW(path);
}

void RelaunchElevated() {
    WCHAR exe[MAX_PATH]; GetModuleFileNameW(0, exe, MAX_PATH);
    Run(exe, 0, TRUE, FALSE);
}
