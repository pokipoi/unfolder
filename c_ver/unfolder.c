#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

void moveFile(const wchar_t *src, const wchar_t *dest) {
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
