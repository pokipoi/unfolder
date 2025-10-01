#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
// Global variable to store user's choice
static int globalChoice = 0; // 0: Ask, 1: Replace All, 2: Skip All
// Function declarations
static BOOL FileExists(const wchar_t *path) {
    DWORD attrs = GetFileAttributesW(path);
    return (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
}

int ShowFileConflictDialog(const wchar_t *filename) {
    // Simple MessageBox fallback instead of TaskDialog
    wchar_t message[MAX_PATH + 100];
    _snwprintf(message, MAX_PATH + 100, 
        L"文件 '%s' 已存在。\nyes替换全部，no跳过，cancel选择每个文件", filename);

    int result = MessageBoxW(NULL, message, L"文件冲突",
        MB_YESNOCANCEL | MB_ICONQUESTION);
    
    switch (result) {
        case IDYES:    return 0; // Replace All
        case IDNO:     return 1; // Skip All  
        case IDCANCEL: return 2; // Choose per file
        default:       return 1; // Default to Skip
    }
}

void moveFile(const wchar_t *src, const wchar_t *dest) {
    if (FileExists(dest)) {
        if (globalChoice == 0) { // No global choice yet
            int choice = ShowFileConflictDialog(dest);
            
            switch (choice) {
                case 0: // Replace All
                    globalChoice = 1;
                    break;
                case 1: // Skip All
                    globalChoice = 2;
                    return;
                case 2: // Choose per file
                    {
                        SHFILEOPSTRUCTW op = {0};
                        op.wFunc = FO_MOVE;
                        wchar_t srcBuffer[MAX_PATH + 2] = {0};
                        wchar_t destBuffer[MAX_PATH + 2] = {0};
                        wcscpy(srcBuffer, src);
                        wcscpy(destBuffer, dest);
                        op.pFrom = srcBuffer;
                        op.pTo = destBuffer;
                        op.fFlags = FOF_ALLOWUNDO | FOF_WANTMAPPINGHANDLE;
                        
                        int result = SHFileOperationW(&op);
                        if (result != 0) {
                            MessageBoxW(NULL, L"移动文件失败", L"错误", MB_ICONERROR);
                            ExitProcess(1);
                        }
                        return;
                    }
            }
        }
        
        if (globalChoice == 2) { // Skip All
            return;
        }
        
        // Replace file (globalChoice == 1 or after individual choice)
        DeleteFileW(dest);
    }
    
    if (!MoveFileW(src, dest)) {
        MessageBoxW(NULL, L"移动文件失败", L"错误", MB_ICONERROR);
        ExitProcess(1);
    }
}

void removeDirectory(const wchar_t *path) {
    WIN32_FIND_DATAW ffd;
    wchar_t searchPath[MAX_PATH];
    wchar_t filePath[MAX_PATH];
    
    // 构造搜索路径
    wcscpy(searchPath, path);
    wcscat(searchPath, L"\\*");
    
    HANDLE hFind = FindFirstFileW(searchPath, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        MessageBoxW(NULL, L"打开目录失败", L"错误", MB_ICONERROR);
        ExitProcess(1);
    }
    
    do {
        if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) {
            continue;
        }
        
        wcscpy(filePath, path);
        wcscat(filePath, L"\\");
        wcscat(filePath, ffd.cFileName);
        
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            removeDirectory(filePath);
        } else {
            if (!DeleteFileW(filePath)) {
                MessageBoxW(NULL, L"删除文件失败", L"错误", MB_ICONERROR);
                FindClose(hFind);
                ExitProcess(1);
            }
        }
    } while (FindNextFileW(hFind, &ffd));
    
    FindClose(hFind);
    
    if (!RemoveDirectoryW(path)) {
        MessageBoxW(NULL, L"删除目录失败", L"错误", MB_ICONERROR);
        ExitProcess(1);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
    LPSTR lpCmdLine, int nCmdShow) {
    
    int argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    
    if (argc != 2) {
        MessageBoxW(NULL, L"用法错误", L"错误", MB_ICONERROR);
        LocalFree(argv);
        return 1;
    }
    
    wchar_t dir[MAX_PATH];
    wchar_t parentDir[MAX_PATH];
    wchar_t src[MAX_PATH];
    wchar_t dest[MAX_PATH];
    
    wcscpy(dir, argv[1]);
    
    // 将反斜杠标准化
    for (wchar_t *p = dir; *p; ++p) {
        if (*p == L'/') {
            *p = L'\\';
        }
    }
    
    // 获取父目录
    wcscpy(parentDir, dir);
    wchar_t *lastSlash = wcsrchr(parentDir, L'\\');
    if (lastSlash) {
        *lastSlash = L'\0';
    }
    
    WIN32_FIND_DATAW ffd;
    wchar_t searchPath[MAX_PATH];
    wcscpy(searchPath, dir);
    wcscat(searchPath, L"\\*");
    
    HANDLE hFind = FindFirstFileW(searchPath, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        MessageBoxW(NULL, L"打开目录失败", L"错误", MB_ICONERROR);
        LocalFree(argv);
        return 1;
    }
    
    do {
        if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) {
            continue;
        }
        
        wcscpy(src, dir);
        wcscat(src, L"\\");
        wcscat(src, ffd.cFileName);
        
        wcscpy(dest, parentDir);
        wcscat(dest, L"\\");
        wcscat(dest, ffd.cFileName);
        
        moveFile(src, dest);
    } while (FindNextFileW(hFind, &ffd));
    
    FindClose(hFind);
    removeDirectory(dir);
    
    LocalFree(argv);
    return 0;
}
