#include <windows.h>
#include <shlobj.h>
#include <shellscalingapi.h>
#include <filesystem>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

// Constants for IPC (using Local\ instead of Global\ to avoid admin requirement)
#define MUTEX_NAME L"Local\\UnfolderMutex"
#define EVENT_NAME L"Local\\UnfolderEvent"
#define MAPPING_NAME L"Local\\UnfolderMapping"
#define MAPPING_SIZE 65536
#define WAIT_TIMEOUT 5000

// 将路径转换为 Windows API 兼容的 `std::vector<wchar_t>`（双零结尾）
std::vector<wchar_t> to_windows_path(const fs::path& path) {
    std::wstring pathStr = path.wstring();
    pathStr += L'\0'; // 添加结尾的第一个 '\0'
    pathStr += L'\0'; // 确保是双 '\0' 结尾
    return std::vector<wchar_t>(pathStr.begin(), pathStr.end());
}

// 使用 `SHFileOperationW` 移动文件，确保冲突处理正确
bool MoveFilesToParent(const std::vector<fs::path>& files, const fs::path& parent) {
    std::wstring fromPaths;
    std::wstring toPaths;

    for (const auto& file : files) {
        fromPaths += file.wstring() + L'\0';
        toPaths += (parent / file.filename()).wstring() + L'\0';
    }
    fromPaths += L'\0';
    toPaths += L'\0';

    SHFILEOPSTRUCTW fileOp = { 0 };
    fileOp.wFunc = FO_MOVE;
    fileOp.pFrom = fromPaths.c_str();
    fileOp.pTo = toPaths.c_str();
    fileOp.fFlags = FOF_ALLOWUNDO | FOF_MULTIDESTFILES; // 保留原生的文件冲突提示框

    int result = SHFileOperationW(&fileOp);

    if (result == 0) {
        return true; // 移动成功
    } else {
        std::wcout << L"文件未移动: " << fromPaths << L"\n"; // 记录未移动的文件
        return false; // 失败（用户可能选择了跳过）
    }
}

// 处理整个文件夹
bool MoveFolderContents(const fs::path& folder) {
    fs::path parent = folder.parent_path();
    std::vector<fs::path> files;

    for (const auto& entry : fs::directory_iterator(folder)) {
        files.push_back(entry.path());
    }

    if (MoveFilesToParent(files, parent) && fs::is_empty(folder)) {
        std::wstring folderStr = folder.wstring() + L'\0' + L'\0';
        SHFILEOPSTRUCTW delOp = { 0 };
        delOp.wFunc = FO_DELETE;
        delOp.pFrom = folderStr.c_str();
        delOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION;

        return (SHFileOperationW(&delOp) == 0);
    }

    return false; // 移动失败或文件夹非空
}

// 设置高 DPI 适配，防止模糊
void EnableDPIAwareness() {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

// Read config file
bool ReadConfig(const std::wstring& configPath, const std::wstring& key) {
    std::wifstream configFile(configPath);
    std::wstring line;
    while (std::getline(configFile, line)) {
        std::wistringstream lineStream(line);
        std::wstring currentKey;
        if (std::getline(lineStream, currentKey, L'=') && currentKey == key) {
            int value;
            if (lineStream >> value) {
                return value != 0;
            }
        }
    }
    return false;
}

// Send folder path to existing instance
bool SendToExistingInstance(const std::wstring& folderPath) {
    HANDLE hMapFile = OpenFileMappingW(FILE_MAP_WRITE, FALSE, MAPPING_NAME);
    if (hMapFile == NULL) return false;

    LPVOID pBuf = MapViewOfFile(hMapFile, FILE_MAP_WRITE, 0, 0, MAPPING_SIZE);
    if (pBuf == NULL) {
        CloseHandle(hMapFile);
        return false;
    }

    // Write the folder path
    wcscpy_s((wchar_t*)pBuf, MAPPING_SIZE / sizeof(wchar_t), folderPath.c_str());

    // Signal the event
    HANDLE hEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, EVENT_NAME);
    if (hEvent != NULL) {
        SetEvent(hEvent);
        CloseHandle(hEvent);
    }

    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);
    return true;
}

// Collect paths with timeout
std::vector<std::wstring> CollectPaths(HANDLE hEvent, HANDLE hMapFile, LPVOID pBuf) {
    std::vector<std::wstring> paths;
    auto startTime = std::chrono::steady_clock::now();
    
    while (true) {
        DWORD waitResult = WaitForSingleObject(hEvent, 500); // Wait 500ms
        
        if (waitResult == WAIT_OBJECT_0) {
            // Received a path
            wchar_t* pathData = (wchar_t*)pBuf;
            if (wcslen(pathData) > 0) {
                paths.push_back(pathData);
                // Clear the buffer
                memset(pBuf, 0, MAPPING_SIZE);
            }
            ResetEvent(hEvent);
            startTime = std::chrono::steady_clock::now(); // Reset timeout
        } else {
            // Check if timeout exceeded
            auto elapsed = std::chrono::steady_clock::now() - startTime;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > 1000) {
                // No more paths for 1 second, we're done
                break;
            }
        }
    }
    
    return paths;
}

// Windows GUI application entry point
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    EnableDPIAwareness();

    // Parse command line arguments
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    
    if (argc < 2) {
        MessageBoxW(NULL, L"Please drag a folder to this program or call it from the right-click menu!", L"Error", MB_OK | MB_ICONERROR);
        LocalFree(argv);
        return 1;
    }

    // Try to create or open mutex
    HANDLE hMutex = CreateMutexW(NULL, FALSE, MUTEX_NAME);
    DWORD mutexError = GetLastError();
    
    if (mutexError == ERROR_ALREADY_EXISTS) {
        // Another instance is running, send our path to it
        for (int i = 1; i < argc; i++) {
            SendToExistingInstance(argv[i]);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        LocalFree(argv);
        CloseHandle(hMutex);
        return 0;
    }

    // We are the first instance, create shared memory and event
    HANDLE hEvent = CreateEventW(NULL, TRUE, FALSE, EVENT_NAME);
    if (hEvent == NULL) {
        std::wstringstream errMsg;
        errMsg << L"Failed to create event! Error code: " << GetLastError();
        MessageBoxW(NULL, errMsg.str().c_str(), L"Error", MB_OK | MB_ICONERROR);
        LocalFree(argv);
        CloseHandle(hMutex);
        return 1;
    }

    HANDLE hMapFile = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, MAPPING_SIZE, MAPPING_NAME);
    if (hMapFile == NULL) {
        std::wstringstream errMsg;
        errMsg << L"Failed to create file mapping! Error code: " << GetLastError();
        MessageBoxW(NULL, errMsg.str().c_str(), L"Error", MB_OK | MB_ICONERROR);
        LocalFree(argv);
        CloseHandle(hEvent);
        CloseHandle(hMutex);
        return 1;
    }

    LPVOID pBuf = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, MAPPING_SIZE);

    if (pBuf == NULL) {
        std::wstringstream errMsg;
        errMsg << L"Failed to map view of file! Error code: " << GetLastError();
        MessageBoxW(NULL, errMsg.str().c_str(), L"Error", MB_OK | MB_ICONERROR);
        LocalFree(argv);
        CloseHandle(hEvent);
        CloseHandle(hMapFile);
        CloseHandle(hMutex);
        return 1;
    }

    // Collect all paths from command line
    std::vector<std::wstring> allPaths;
    for (int i = 1; i < argc; i++) {
        allPaths.push_back(argv[i]);
    }
    LocalFree(argv);

    // Wait for additional paths from other instances
    auto additionalPaths = CollectPaths(hEvent, hMapFile, pBuf);
    allPaths.insert(allPaths.end(), additionalPaths.begin(), additionalPaths.end());

    // Get config
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    fs::path configPath = fs::path(exePath).remove_filename() / "config.ini";
    bool successPopup = ReadConfig(configPath, L"SuccessPopup");

    // Process all folders
    int successCount = 0;
    int failureCount = 0;
    std::wstring errorMessages;

    for (const auto& pathStr : allPaths) {
        fs::path folderPath = pathStr;

        if (!fs::is_directory(folderPath)) {
            errorMessages += L"• " + folderPath.wstring() + L" (not a valid folder)\n";
            failureCount++;
            continue;
        }

        if (MoveFolderContents(folderPath)) {
            successCount++;
        } else {
            errorMessages += L"• " + folderPath.wstring() + L" (failed to move files)\n";
            failureCount++;
        }
    }

    // Display results
    if (failureCount == 0 && successPopup) {
        std::wstringstream msgStream;
        msgStream << L"Successfully processed " << successCount << L" folder(s).";
        MessageBoxW(NULL, msgStream.str().c_str(), L"Completed", MB_OK | MB_ICONINFORMATION);
    } else if (failureCount > 0) {
        std::wstringstream msgStream;
        msgStream << L"Success: " << successCount << L"\n";
        msgStream << L"Failed: " << failureCount << L"\n\n";
        msgStream << L"Failed folders:\n" << errorMessages;
        MessageBoxW(NULL, msgStream.str().c_str(), L"Partial Success", MB_OK | MB_ICONWARNING);
    }

    // Cleanup
    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);
    CloseHandle(hEvent);
    CloseHandle(hMutex);

    return 0;
}