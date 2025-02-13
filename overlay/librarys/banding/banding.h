#pragma once

#include <Windows.h>
#include <tlhelp32.h>
#include <string>
#include <tchar.h>

enum ZBID
{
    ZBID_DEFAULT = 0,
    ZBID_DESKTOP = 1,
    ZBID_UIACCESS = 2,
    ZBID_IMMERSIVE_IHM = 3,
    ZBID_IMMERSIVE_NOTIFICATION = 4,
    ZBID_IMMERSIVE_APPCHROME = 5,
    ZBID_IMMERSIVE_MOGO = 6,
    ZBID_IMMERSIVE_EDGY = 7,
    ZBID_IMMERSIVE_INACTIVEMOBODY = 8,
    ZBID_IMMERSIVE_INACTIVEDOCK = 9,
    ZBID_IMMERSIVE_ACTIVEMOBODY = 10,
    ZBID_IMMERSIVE_ACTIVEDOCK = 11,
    ZBID_IMMERSIVE_BACKGROUND = 12,
    ZBID_IMMERSIVE_SEARCH = 13,
    ZBID_GENUINE_WINDOWS = 14,
    ZBID_IMMERSIVE_RESTRICTED = 15,
    ZBID_SYSTEM_TOOLS = 16,
    // Win10
    ZBID_LOCK = 17,
    ZBID_ABOVELOCK_UX = 18,
};

typedef BOOL(WINAPI* GetWindowBand)(HWND hWnd, PDWORD pdwBand);
typedef HWND(WINAPI* CreateWindowInBand)(_In_ DWORD dwExStyle, _In_opt_ ATOM atom, _In_opt_ LPCWSTR lpWindowName, _In_ DWORD dwStyle, _In_ int X, _In_ int Y, _In_ int nWidth, _In_ int nHeight, _In_opt_ HWND hWndParent, _In_opt_ HMENU hMenu, _In_opt_ HINSTANCE hInstance, _In_opt_ LPVOID lpParam, DWORD band);
typedef BOOL(WINAPI* SetWindowBand)(HWND hWnd, DWORD dwBand);

DWORD CreateWinLogon(DWORD session, DWORD access, PHANDLE token) {
    PRIVILEGE_SET ps = { 1, PRIVILEGE_SET_ALL_NECESSARY };
    if (!LookupPrivilegeValue(NULL, SE_TCB_NAME, &ps.Privilege[0].Luid))
        return GetLastError();

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return GetLastError();

    PROCESSENTRY32 pe = { sizeof(pe) };
    DWORD status = ERROR_NOT_FOUND;

    for (BOOL cont = Process32First(snapshot, &pe); cont; cont = Process32Next(snapshot, &pe)) {
        if (_tcsicmp(pe.szExeFile, TEXT("winlogon.exe"))) 
            continue;

        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
        if (!process)
            continue;

        HANDLE h_token;
        if (OpenProcessToken(process, TOKEN_QUERY | TOKEN_DUPLICATE, &h_token)) {
            DWORD sid, ret;
            BOOL ftcb;
            if (PrivilegeCheck(h_token, &ps, &ftcb) && ftcb &&
                GetTokenInformation(h_token, TokenSessionId, &sid, sizeof(sid), &ret) && sid == session &&
                DuplicateTokenEx(h_token, access, NULL, SecurityImpersonation, TokenImpersonation, token)) {
                status = ERROR_SUCCESS;
                CloseHandle(h_token);
                CloseHandle(process);
                break;
            }
            CloseHandle(h_token);
        }
        CloseHandle(process);
    }
    CloseHandle(snapshot);
    return status;
}

DWORD CreateToken(PHANDLE token) {
    HANDLE token_self;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &token_self))
        return GetLastError();

    DWORD session, ret;
    if (!GetTokenInformation(token_self, TokenSessionId, &session, sizeof(session), &ret))
        return GetLastError();

    DWORD status = CreateWinLogon(session, TOKEN_IMPERSONATE, token);
    if (status != ERROR_SUCCESS)
        return status;

    if (SetThreadToken(NULL, *token) && DuplicateTokenEx(token_self, TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_ADJUST_DEFAULT, NULL, SecurityAnonymous, TokenPrimary, token)) {
        BOOL access = TRUE;
        if (SetTokenInformation(*token, TokenUIAccess, &access, sizeof(access))) {
            status = ERROR_SUCCESS;
        }
        else {
            status = GetLastError();
            CloseHandle(*token);
        }
        RevertToSelf();
    }
    else {
        status = GetLastError();
    }

    CloseHandle(token_self);
    return status;
}

DWORD BandingCheck() {
    HANDLE h_token;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &h_token)) {
        DWORD access;
        DWORD ret;
        if (GetTokenInformation(h_token, TokenUIAccess, &access, sizeof(access), &ret) && access) {
            CloseHandle(h_token);
            return ERROR_SUCCESS;
        }
        CloseHandle(h_token);
    }
    else {
        return GetLastError();
    }

    DWORD status = CreateToken(&h_token);
    if (status != ERROR_SUCCESS)
        return status;

    STARTUPINFO si = { 0 };
    PROCESS_INFORMATION pi;
    GetStartupInfo(&si);
    if (CreateProcessAsUser(h_token, NULL, GetCommandLine(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        ExitProcess(0);
    }
    else {
        return GetLastError();
    }
}

