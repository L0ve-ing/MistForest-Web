#ifndef UNICODE       // 原生界面使用宽字符。
#define UNICODE       // 中文控件与标题。
#endif                // 宽字符设置结束。
#ifndef NOMINMAX      // 标准库算法不被系统宏覆盖。
#define NOMINMAX      // 禁用旧式大小宏。
#endif                // 宏设置结束。
#include <windows.h>  // 原生窗口与绘图。
#include <windowsx.h> // 鼠标坐标。
#ifdef _WIN32
#include <gdiplus.h>
#include <ole2.h>
#include <cstring>
#endif
#include <commdlg.h>  // 地图打开和保存。
#include <algorithm>  // 界面排序与范围处理。
#include <filesystem> // 中文文件路径。
#include <fstream>    // 导出复盘。
#include <cmath>
#include <tuple>
#include <sstream>       // 组合报告。
#include "map_files.hpp" // 地图持久化工具。
#include "presentation/playback.hpp"
#include "presentation/effect_assets.hpp"
#include "presentation/character_assets.hpp"

#include "action_selection.hpp"
#include "ui/command_catalog.hpp"
#include "editor/map_editor.hpp"
#include "tutorial/guide.hpp"
#include "network.hpp" // 双角色联机。

namespace ui { // 界面只读取玩家受限视图。
#include "ui/state.inc"
#include "ui/drawing.inc"
#include "ui/portraits.inc"
#include "ui/character_sprites.inc"
#include "ui/view_helpers.inc"
#include "ui/menu_pages.inc"
#include "ui/presentation.inc"
#include "ui/playback_options.inc"
#include "ui/board.inc"
#include "ui/battle_page.inc"
#include "ui/editor.inc"
#include "ui/replay_page.inc"
#include "ui/page_rendering.inc"
#include "ui/match_flow.inc"
#include "ui/action_selection.inc"
#include "ui/commands.inc"
#include "ui/input.inc"
#include "ui/window.inc"
#include "ui/session.inc"
} // 结束界面命名空间。

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR,
                    int show) { // 宽字符图形入口，不创建控制台。
    SetProcessDPIAware();
    ui::loadPresentationPreferences();
    WNDCLASSW wc{};
    wc.lpfnWndProc = ui::windowProc;
    wc.hInstance = instance;
    wc.lpszClassName = mist::WindowClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    if (!RegisterClassW(&wc))
        return 1; // 注册新版窗口类。
    RECT area{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0);
    int w = std::min(1540, int(area.right - area.left) - 40),
        h = std::min(1000, int(area.bottom - area.top) - 40);
    HWND window = CreateWindowExW(
        0, wc.lpszClassName,
        (L"雾隐之森 V" + std::wstring(mist::VersionText) + L" · 多人战场").c_str(),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, (area.left + area.right - w) / 2,
        (area.top + area.bottom - h) / 2, w, h, nullptr, nullptr, instance, nullptr);
    if (!window)
        return 1;
    ShowWindow(window, show);
    UpdateWindow(window);
    MSG msg{}; // 创建居中窗口并显示首帧。
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if ((GetFocus() == ui::s.addressEdit || GetFocus() == ui::s.portEdit) &&
            IsDialogMessageW(window, &msg))
            continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return int(msg.wParam); // 标准消息循环保持界面响应。
} // 结束程序入口。
