#include <windows.h>
#include <shlobj.h>
#include <shellscalingapi.h>
#include <filesystem>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

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

// 读取配置文件
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
    return false; // 默认值
}

// Windows GUI 应用程序入口
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    EnableDPIAwareness(); // 让 UI 清晰

    // 解析命令行参数
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc < 2) {
        MessageBoxW(NULL, L"Please drag the folder to this program or call it from the right-click menu!", L"error", MB_OK | MB_ICONERROR);
        return 1;
    }

    fs::path folderPath = argv[1];
    LocalFree(argv); // 释放内存

    if (!fs::is_directory(folderPath)) {
        MessageBoxW(NULL, L"error:Please provide a valid folder path!", L"error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // 获取程序目录
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    fs::path configPath = fs::path(exePath).remove_filename() / "config.ini";

    bool successPopup = ReadConfig(configPath, L"SuccessPopup");

    if (MoveFolderContents(folderPath)) {
        if (successPopup) {
            MessageBoxW(NULL, L"The folder has been successfully sorted and deleted. ", L" completed", MB_OK | MB_ICONINFORMATION);
        }
    } else {
        MessageBoxW(NULL, L"Some files were not moved successfully and the folder was not deleted.", L"warning", MB_OK | MB_ICONWARNING);
    }

    return 0;
}