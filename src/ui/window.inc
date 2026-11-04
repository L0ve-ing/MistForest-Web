LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam,
                            LPARAM lParam) { // Windows窗口消息分派。
    switch (message) {                       // 绘图、计时和输入互不阻塞。
    case WM_CREATE: {
        s.window = window;
        visual.introAt = GetTickCount64();
        visual.introSkipped = false;
        visual.introDuration = 2200;
        visual.introPending = true;
        visual.introBlocking = true;
        SetTimer(window, 3, 50, nullptr);
        s.addressEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"127.0.0.1",
                                        WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 1, 1, window,
                                        nullptr, nullptr, nullptr);
        s.portEdit =
            CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"39091", WS_CHILD | WS_TABSTOP | ES_NUMBER,
                            0, 0, 1, 1, window, nullptr, nullptr, nullptr);
        s.editor.widthEdit =
            CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"5", WS_CHILD | WS_TABSTOP | ES_NUMBER, 0,
                            0, 1, 1, window, nullptr, nullptr, nullptr);
        s.editor.heightEdit =
            CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"5", WS_CHILD | WS_TABSTOP | ES_NUMBER, 0,
                            0, 1, 1, window, nullptr, nullptr, nullptr);
        for (HWND edit : {s.editor.widthEdit, s.editor.heightEdit})
            SendMessageW(edit, EM_SETLIMITTEXT, 2, 0);
        for (HWND edit : {s.addressEdit, s.portEdit, s.editor.widthEdit, s.editor.heightEdit})
            SendMessageW(edit, WM_SETFONT,
                         reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(s.addressEdit, EM_SETLIMITTEXT, 15, 0);
        SendMessageW(s.portEdit, EM_SETLIMITTEXT, 5, 0);
        return 0;
    } // 原生输入框支持复制粘贴。
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        invalidate();
        return 0; // 双缓冲避免背景闪烁。
    case WM_GETMINMAXINFO: {
        auto *info = reinterpret_cast<MINMAXINFO *>(lParam);
        info->ptMinTrackSize = {900, 670};
        return 0;
    } // 最小窗口确保二十五格可读。
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC screen = BeginPaint(window, &ps);
        RECT client{};
        GetClientRect(window, &client);
        int w = client.right, h = client.bottom;
#ifdef _WIN32
        static PageFrame backBuffer;
        if (!backBuffer.allocate(std::max(1, w), std::max(1, h))) {
            EndPaint(window, &ps);
            return 0;
        }
        HDC dc = backBuffer.dc;
        int saved = SaveDC(dc);
#else
        HDC dc = CreateCompatibleDC(screen);
        HBITMAP bitmap = CreateCompatibleBitmap(screen, std::max(1, w), std::max(1, h));
        HGDIOBJ old = SelectObject(dc, bitmap);
#endif
        fill(dc, client, BG);
        s.scale = std::min(w / double(canvasWidth()), h / double(canvasHeight()));
        s.offsetX = widePage() ? 0 : int((w - canvasWidth() * s.scale) / 2);
        s.offsetY = widePage() ? 0 : int((h - canvasHeight() * s.scale) / 2);
        SetMapMode(dc, MM_ANISOTROPIC);
        SetWindowExtEx(dc, canvasWidth(), canvasHeight(), nullptr);
        SetViewportExtEx(dc, widePage() ? w : std::max(1, int(canvasWidth() * s.scale)),
                         widePage() ? h : std::max(1, int(canvasHeight() * s.scale)), nullptr);
        SetViewportOrgEx(dc, s.offsetX, s.offsetY, nullptr);
        if (visual.introPending) {
            // Finish cold resource work before starting the visible opening.
            forestScene::painting();
            forestScene::clouds();
            // Prepare the clean first page before its animation clock starts.
            // The following draw reuses that page and only composes the live fog.
            bool skipped = visual.introSkipped;
            visual.introSkipped = true;
            draw(dc);
            visual.introSkipped = skipped;
            visual.introAt = GetTickCount64();
            visual.introPending = false;
        }
        draw(dc);
        SetMapMode(dc, MM_TEXT);
        SetViewportOrgEx(dc, 0, 0, nullptr);
        BitBlt(screen, 0, 0, w, h, dc, 0, 0, SRCCOPY);
#ifdef _WIN32
        RestoreDC(dc, saved);
#else
        SelectObject(dc, old);
        DeleteObject(bitmap);
        DeleteDC(dc);
#endif
        EndPaint(window, &ps);
        return 0;
    } // 在内存完成整帧后一次显示。
    case WM_LBUTTONUP: {
        visual.pressed = 0;
        if (dismissIntro()) {
            invalidate();
            return 0;
        }
        SetFocus(window);
        POINT p{LONG((GET_X_LPARAM(lParam) - s.offsetX) / std::max(.01, s.scale)),
                LONG((GET_Y_LPARAM(lParam) - s.offsetY) / std::max(.01, s.scale))};
        for (auto it = s.hits.rbegin(); it != s.hits.rend(); ++it)
            if (it->enabled && PtInRect(&it->rect, p)) {
                int id = it->id;
                command(id);
                break;
            }
        return 0;
    } // 点击匹配最上层的有效按钮。
    case WM_KEYDOWN:
        if (dismissIntro()) {
            invalidate();
            return 0;
        }
        if (!(lParam & (LPARAM(1) << 30)))
            keyboard(wParam);
        return 0; // 快捷键与鼠标共用校验流程。
#ifdef _WIN32
    case WM_RBUTTONUP: {
        if (s.pendingLeave || s.inspected >= 0 || s.playbackOptions || s.page != Page::Battle)
            return 0;
        POINT p{LONG((GET_X_LPARAM(lParam) - s.offsetX) / std::max(.01, s.scale)),
                LONG((GET_Y_LPARAM(lParam) - s.offsetY) / std::max(.01, s.scale))};
        for (const auto &hit : s.hits)
            if (PtInRect(&hit.rect, p)) {
                int unit = hit.id >= DetailsBase && hit.id < DetailsBase + 8 ? hit.id - DetailsBase
                           : hit.id == Unit0 || hit.id == Unit1 ? s.viewer * 2 + hit.id - Unit0
                                                                : -1;
                if (unit >= 0) {
                    command(DetailsBase + unit);
                    break;
                }
            }
        return 0;
    }
    case WM_MOUSEMOVE: {
        POINT p{LONG((GET_X_LPARAM(lParam) - s.offsetX) / std::max(.01, s.scale)),
                LONG((GET_Y_LPARAM(lParam) - s.offsetY) / std::max(.01, s.scale))};
        int hovered = 0;
        for (const auto &hit : s.hits)
            if ((hit.enabled || hit.id == Unit0 || hit.id == Unit1) && PtInRect(&hit.rect, p))
                hovered = hit.id;
        if (hovered != visual.hover) {
            visual.hover = hovered;
            InvalidateRect(window, nullptr, FALSE);
        }
        TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, window, 0};
        TrackMouseEvent(&track);
        return 0;
    }
    case WM_MOUSELEAVE:
        visual.hover = visual.pressed = 0;
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_LBUTTONDOWN:
        visual.pressed = visual.hover;
        InvalidateRect(window, nullptr, FALSE);
        return 0;
#endif
    case WM_MOUSEWHEEL: {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (ScreenToClient(window, &point)) {
            point.x = LONG((point.x - s.offsetX) / std::max(.01, s.scale));
            point.y = LONG((point.y - s.offsetY) / std::max(.01, s.scale));
            wheelAt(point, GET_WHEEL_DELTA_WPARAM(wParam));
        }
        return 0;
    }
    case WM_TIMER:
        if (wParam == 3) {
            ULONGLONG now = GetTickCount64();
            static int interval = 50;
            int desired = introOpacity(now) > 0 || presentationBusy() ||
                                  ((s.playback.state().fxLive &&
                                    effectOpacity(s.playback.state().fxAt, feedbackNow(),
                                                  SkillFxDuration) > 0) ||
                                   replayAnimating(now))
                              ? 16
                              : 50;
            if (desired != interval) {
                SetTimer(s.window, 3, desired, nullptr);
                interval = desired;
            }
            if (s.page == Page::Battle && s.phase != Phase::Cover && s.inspected < 0 &&
                !s.pendingLeave)
                advancePresentation(GetTickCount64());
            if (s.window)
                InvalidateRect(s.window, nullptr, FALSE);
            return 0;
        }
        if (wParam == 2) {
            if (s.online && s.net && s.net->phase() != forest::Phase::Idle && s.net->poll())
                syncNetwork();
        } else if (wParam == 1) {
            KillTimer(window, 1);
            if (!s.online && s.bot && s.game && !s.game->finished() &&
                inputPlayer(s.game->state()) != 0 && s.page == Page::Battle) {
                if (!botStep())
                    SetTimer(window, 1, 200, nullptr);
            }
        }
        return 0; // 电脑按设置的风格决策，始终仅使用本人视图。
    case WM_DESTROY:
        KillTimer(window, 1);
        KillTimer(window, 2);
        KillTimer(window, 3);
        if (s.net)
            s.net->close();
        PostQuitMessage(0);
        return 0; // 退出时释放连接，不残留房间监听。
    default:
        return DefWindowProcW(window, message, wParam, lParam); // 默认系统窗口行为。
    } // 结束系统消息处理。
} // 结束窗口过程。
