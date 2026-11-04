bool widePage() {
    return s.page == Page::Lobby || s.page == Page::Config;
}

int canvasWidth() {
    if (widePage()) {
        RECT r{};
        if (s.window && GetClientRect(s.window, &r) && r.right && r.bottom) {
            double k = std::min(r.right / 1510., r.bottom / 930.);
            return int(std::lround(r.right / k));
        }
        return 1510;
    }
    return s.page == Page::Battle ? 1510 : WIDTH;
}

int canvasHeight() {
    if (widePage()) {
        RECT r{};
        if (s.window && GetClientRect(s.window, &r) && r.right && r.bottom) {
            double k = std::min(r.right / 1510., r.bottom / 930.);
            return int(std::lround(r.bottom / k));
        }
        return 930;
    }
    return s.page == Page::Battle ? 930 : HEIGHT;
} // 大厅与设置铺满窗口；战场保留横向三栏。

RECT sideRect(int x, int y, int w, int h) {
    return RECT{x + 402, y, x + 402 + w, y + h};
} // 原有角色操作区平移到右栏，命中区域同步移动。

RECT rect(int x, int y, int w, int h) {
    return RECT{x, y, x + w, y + h};
} // 将左上角与宽高转换为 Win32 矩形。

// Settings controls and wheel input share the same count-selector geometry.
RECT configPeopleRect() {
    return rect(224, 516, 134, 52);
}

void fill(HDC dc, RECT r, COLORREF color) {
    HBRUSH b = CreateSolidBrush(color);
    FillRect(dc, &r, b);
    DeleteObject(b);
} // 创建画刷填充矩形并及时释放资源。

#include "theme.inc"

#ifdef _WIN32
void roundedPanelPath(Gdiplus::GraphicsPath &path, RECT r, float radius) {
    float x = float(r.left), y = float(r.top), w = float(r.right - r.left),
          h = float(r.bottom - r.top), d = std::min({radius * 2, w, h});
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d, y, d, d, 270, 90);
    path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    path.AddArc(x, y + h - d, d, d, 90, 90);
    path.CloseFigure();
}
#endif

void card(HDC dc, RECT r, COLORREF color = WHITE, COLORREF border = LINE, int radius = 16,
          int width = 1) { // 绘制带圆角与边框的面板。
#ifdef _WIN32
    if (r.right - r.left >= 240 && r.bottom - r.top >= 120) {
        art::Canvas canvas(dc);
        for (int i = 3; i > 0; --i) {
            RECT shadow = r;
            shadow.top += i + 2;
            shadow.bottom += i + 2;
            shadow.left -= i;
            shadow.right += i;
            Gdiplus::GraphicsPath path;
            roundedPanelPath(path, shadow, radius / 2.f + i);
            Gdiplus::SolidBrush brush(Gdiplus::Color(9, 12, 35, 25));
            canvas.graphics->FillPath(&brush, &path);
        }
    }
#endif
    HBRUSH b = CreateSolidBrush(color);
    HPEN p = CreatePen(PS_SOLID, width, border);                     // 创建本次绘图用的画刷与画笔。
    HGDIOBJ oldB = SelectObject(dc, b), oldP = SelectObject(dc, p);  // 保存原绘图对象，之后恢复。
    RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius); // 按逻辑坐标绘制圆角矩形。
    SelectObject(dc, oldB);
    SelectObject(dc, oldP);
    DeleteObject(b);
    DeleteObject(p);
    // Botanical corners belong to major panels, not repeated choices or map cells.
    if (r.right - r.left >= 300 && r.bottom - r.top >= 220)
        panelOrnaments(dc, r, color);
} // 结束面板绘制函数。

void text(HDC dc, const std::wstring &value, RECT r, int size = 16, COLORREF color = INK,
          bool bold = false, UINT format = DT_LEFT | DT_WORDBREAK) { // 输出自动换行的中文文字。
    HFONT font =
        CreateFontW(-size, 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                    DEFAULT_PITCH, L"Microsoft YaHei UI"); // 使用清晰的中文系统字体。
    HGDIOBJ old = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color); // 设置透明背景和文字颜色。
    DrawTextW(dc, value.c_str(), static_cast<int>(value.size()), &r,
              format | DT_NOPREFIX); // DT_NOPREFIX 防止文字中的 & 被当成快捷键标记。
    SelectObject(dc, old);
    DeleteObject(font); // 恢复旧字体，防止 GDI 对象泄漏。
} // 结束文本绘制函数。

int roleFontSize(HDC dc, const std::wstring &value, int width, int height, int preferred = 18) {
    for (int size = preferred; size >= 12; --size) {
        HFONT font = CreateFontW(-size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH, L"Microsoft YaHei UI");
        auto old = SelectObject(dc, font);
        RECT measured = rect(0, 0, width, 0);
        DrawTextW(dc, value.c_str(), int(value.size()), &measured,
                  DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
        SelectObject(dc, old);
        DeleteObject(font);
        if (measured.bottom <= height)
            return size;
    }
    return 12;
}

void roleText(HDC dc, const std::wstring &value, RECT bounds, int preferred = 18) {
    text(dc, value, bounds,
         roleFontSize(dc, value, bounds.right - bounds.left, bounds.bottom - bounds.top, preferred),
         INK);
}

void button(HDC dc, int id, const std::wstring &label, RECT r, bool primary = false,
            bool enabled = true, int fontSize = 16) { // 允许关键对战按钮使用更大的字号。
    RECT hitRect = r;
    bool hovered = enabled && visual.hover == id, pressed = enabled && visual.pressed == id;
    // A selected option and the page's next action have different visual weight.
    bool action = primary && (id == Cpu || id == Begin || id == Commit || id == Ready ||
                              id == Pass || id == Again || id == EndTurn || id == NetHost ||
                              id == NetJoin || id == NetResume || id == MapApply || id == MapSave ||
                              id == RandomMap || id == TutorialRole || id == TutorialNext ||
                              id == TutorialContinue || id == TutorialRetry || id == TutorialDemo ||
                              id == ClosePlaybackOptions || id == LeaveCancel);
    bool selected = primary && !action;
    if (pressed) {
        ++r.top;
        ++r.bottom;
    }
    COLORREF background = !enabled   ? RGB(231, 233, 222)
                          : action   ? (hovered ? RGB(35, 125, 89) : RGB(24, 108, 78))
                          : selected ? (hovered ? RGB(206, 227, 194) : RGB(220, 234, 205))
                                     : (hovered ? RGB(245, 245, 224) : RGB(253, 249, 235));
    COLORREF border = !enabled   ? RGB(211, 216, 203)
                      : action   ? RGB(184, 157, 88)
                      : selected ? TEAL
                                 : RGB(207, 195, 157);
#ifdef _WIN32
    {
        art::Canvas canvas(dc);
        auto &g = *canvas.graphics;
        Gdiplus::GraphicsPath path;
        roundedPanelPath(path, r, 6);
        auto top = [&](COLORREF c) {
            return RGB(std::min(255, GetRValue(c) + (action ? 13 : 3)),
                       std::min(255, GetGValue(c) + (action ? 13 : 3)),
                       std::min(255, GetBValue(c) + (action ? 10 : 3)));
        };
        Gdiplus::LinearGradientBrush paint(Gdiplus::RectF(float(r.left), float(r.top),
                                                          float(r.right - r.left),
                                                          float(r.bottom - r.top)),
                                           art::color(top(background)), art::color(background), 90);
        g.FillPath(&paint, &path);
        Gdiplus::Pen edge(art::color(border), selected ? 1.6f : 1.f);
        g.DrawPath(&edge, &path);
        if (enabled && r.bottom - r.top >= 36 && r.right - r.left >= 90) {
            RECT inset{r.left + 3, r.top + 3, r.right - 3, r.bottom - 3};
            Gdiplus::GraphicsPath inner;
            roundedPanelPath(inner, inset, 4);
            Gdiplus::Pen innerEdge(Gdiplus::Color(action ? 95 : 65, 184, 157, 88), .7f);
            g.DrawPath(&innerEdge, &inner);
            if (action || s.page == Page::Lobby) {
                art::sprig(g, float(r.left + 6), float(r.bottom - 6), 12, false,
                           Gdiplus::Color(150, 176, 174, 104));
            }
        }
        if (s.page == Page::Lobby) {
            auto color = art::color(enabled ? (action ? RGB(248, 239, 198) : TEAL) : MUTED);
            float sz = r.bottom - r.top > 60 ? 24.f : 15.f;
            float x = float(r.left + (r.right - r.left) * .23), y = (r.top + r.bottom) / 2.f;
            if (id == Cpu || id == Skills) {
                Gdiplus::PointF book[] = {{x - sz / 2, y - sz / 2}, {x, y - sz / 3},
                                          {x + sz / 2, y - sz / 2}, {x + sz / 2, y + sz / 2},
                                          {x, y + sz / 3},          {x - sz / 2, y + sz / 2}};
                Gdiplus::Pen pen(color, 1.6f);
                g.DrawPolygon(&pen, book, 6);
                art::line(g, x, y - sz / 3, x, y + sz / 3, color, 1.5f);
                for (float dy : {-4.f, 2.f}) {
                    art::line(g, x - sz / 2 + 3, y + dy, x - 3, y + dy + 1, color);
                    art::line(g, x + 3, y + dy + 1, x + sz / 2 - 3, y + dy, color);
                }
            } else if (id == Hotseat) {
                art::line(g, x - 9, y - 10, x + 9, y + 10, color, 2);
                art::line(g, x + 9, y - 10, x - 9, y + 10, color, 2);
                art::line(g, x - 10, y + 3, x - 3, y + 10, color, 2);
                art::line(g, x + 10, y + 3, x + 3, y + 10, color, 2);
            } else if (id == Settings) {
                Gdiplus::Pen pen(color, 2.f);
                g.DrawEllipse(&pen, x - 4, y - 4, 8.f, 8.f);
                for (int i = 0; i < 8; ++i) {
                    float a = i * 3.14159265f / 4;
                    art::line(g, x + 6 * std::cos(a), y + 6 * std::sin(a), x + 9 * std::cos(a),
                              y + 9 * std::sin(a), color, 2.5f);
                }
            } else if (id == Review) {
                for (int i = 0; i < 3; ++i) {
                    Gdiplus::SolidBrush bar(color);
                    g.FillRectangle(&bar, x - 8 + i * 6, y + 5 - (i + 1) * 4, 4.f,
                                    float((i + 1) * 4));
                }
            } else {
                Gdiplus::Pen pen(color, 1.7f);
                g.DrawEllipse(&pen, x - sz / 2, y - sz / 2, sz * .75f, sz);
                g.DrawEllipse(&pen, x - sz / 4, y - sz / 2, sz * .75f, sz);
            }
        }
        if (action && enabled) {
            art::line(g, float(r.left + 8), float(r.top + 2), float(r.right - 8), float(r.top + 2),
                      Gdiplus::Color(90, 244, 229, 174));
        }
        if (selected && enabled && r.right - r.left >= 85) {
            float x = float(r.left + 12), y = (r.top + r.bottom) / 2.f;
            art::line(g, x, y, x + 4, y + 4, art::color(TEAL), 1.8f);
            art::line(g, x + 4, y + 4, x + 12, y - 5, art::color(TEAL), 1.8f);
        }
    }
#else
    card(dc, r, background, border, 12);
#endif
    buttonOrnaments(dc, r, action, enabled);
    int key = s.page == Page::Battle ? shortcut(id) : 0;
    std::wstring caption =
        label +
        (key ? L" [" + (key == VK_TAB ? std::wstring(L"Tab") : std::wstring(1, wchar_t(key))) + L"]"
             : L"");
    RECT captionRect = r;
    if (s.page == Page::Lobby)
        captionRect.left += (r.bottom - r.top > 60 ? 34 : 20);
    if (selected && enabled && r.right - r.left >= 85)
        captionRect.left += 18;
    text(dc, caption, captionRect, key ? std::max(12, fontSize - 2) : fontSize,
         enabled ? (action ? RGB(255, 250, 225) : INK) : MUTED, true,
         DT_CENTER | DT_VCENTER | DT_SINGLELINE); // 快捷键与实际派发共用映射。
    s.hits.push_back({hitRect, id, enabled});     // 点击时只派发 enabled 为真的按钮。
} // 结束按钮函数。

void controls() {
    if (!s.window || !s.portEdit)
        return;
    RECT c{};
    GetClientRect(s.window, &c);
    double k = std::min(c.right / double(canvasWidth()), c.bottom / double(canvasHeight()));
    int x = int((c.right - canvasWidth() * k) / 2), y = int((c.bottom - canvasHeight() * k) / 2);
    auto place = [&](HWND h, int px, int py, int w, int height, bool visible) {
        MoveWindow(h, x + int(px * k), y + int(py * k), int(w * k), int(height * k), TRUE);
        ShowWindow(h, visible ? SW_SHOW : SW_HIDE);
    };
    place(s.portEdit, 65, 320, 210, 44, s.page == Page::Network);
    place(s.addressEdit, 602, 320, 450, 44, s.page == Page::Network);
    place(s.editor.widthEdit, 180, 765, 100, 42, s.page == Page::Editor);
    place(s.editor.heightEdit, 385, 765, 100, 42, s.page == Page::Editor);
} // 网络地址与地图尺寸仍使用原生输入框。

int inputNumber(HWND h, int fallback) {
    if (!h)
        return fallback;
    wchar_t value[16]{};
    GetWindowTextW(h, value, 16);
    std::wstring text = value;
    if (text.empty() || text.find_first_not_of(L"0123456789") != std::wstring::npos)
        return -1;
    return _wtoi(value);
} // 拒绝空白和非数字配置。

void syncDimensions() {
    if (s.editor.widthEdit)
        SetWindowTextW(s.editor.widthEdit, std::to_wstring(s.editor.document.map().width).c_str());
    if (s.editor.heightEdit)
        SetWindowTextW(s.editor.heightEdit, std::to_wstring(s.editor.document.map().height).c_str());
} // 打开、撤销、重置地图后同步输入框。

void invalidate() {
    s.hits.clear();
    controls();
    if (s.window)
        InvalidateRect(s.window, nullptr, FALSE);
} // 页面改变后立即清理旧点击区域。
