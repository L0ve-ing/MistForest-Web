std::filesystem::path presentationPreferencesFile() {
    wchar_t exe[32768]{};
    GetModuleFileNameW(nullptr, exe, 32768);
    return std::filesystem::path(exe).parent_path() / L"mist-presentation.ini";
}

void loadPresentationPreferences() {
    std::ifstream input(presentationPreferencesFile());
    int full = 1, speed = 100;
    if (input >> full >> speed) {
        s.fullAnimation = full != 0;
        s.playbackSpeed = speed == 80 || speed == 125 ? speed : 100;
        s.playback.setSpeed(s.playbackSpeed);
    }
}

void savePresentationPreferences() {
    std::ofstream output(presentationPreferencesFile(), std::ios::trunc);
    output << int(s.fullAnimation) << ' ' << s.playbackSpeed << '\n';
}

void closePlaybackOptions() {
    if (!s.playbackOptions)
        return;
    auto elapsed = visualNow() - s.optionsAt;
    s.playbackOptions = false;
    s.playback.shiftTime(elapsed);
    s.replayActors.shiftTime(elapsed);
    s.replayFxAt += elapsed;
    s.playback.setSpeed(s.playbackSpeed);
    savePresentationPreferences();
    invalidate();
}

void playbackOptionsModal(HDC dc) {
    if (!s.playbackOptions)
        return;
#ifdef _WIN32
    {
        art::Canvas canvas(dc);
        Gdiplus::SolidBrush dim(Gdiplus::Color(135, 20, 39, 30));
        canvas.graphics->FillRectangle(&dim, 0, 0, canvasWidth(), canvasHeight());
    }
#endif
    s.hits.clear();
    int x = (canvasWidth() - 570) / 2, y = (canvasHeight() - 370) / 2;
    card(dc, rect(x, y, 570, 370), RGB(251, 250, 239), GOLD, 22, 2);
    text(dc, L"播放设置", rect(x + 30, y + 25, 500, 40), 28, TEAL, true);
    text(dc, L"动作表现", rect(x + 32, y + 88, 110, 28), 18, INK, true);
    button(dc, AnimationFull, L"完整", rect(x + 160, y + 80, 176, 42), s.fullAnimation);
    button(dc, AnimationSimple, L"简洁", rect(x + 350, y + 80, 186, 42), !s.fullAnimation);
    text(dc, L"播放速度", rect(x + 32, y + 155, 110, 28), 18, INK, true);
    button(dc, SpeedSlow, L"0.8×", rect(x + 160, y + 147, 118, 42), s.playbackSpeed == 80);
    button(dc, SpeedNormal, L"1.0×", rect(x + 289, y + 147, 118, 42), s.playbackSpeed == 100);
    button(dc, SpeedFast, L"1.25×", rect(x + 418, y + 147, 118, 42), s.playbackSpeed == 125);
    text(
        dc,
        L"动作、特效与战报统一调速。\n简洁模式保留路径、技能范围和命中提示。\n新速度从下一项行动或下一步回放开始生效。",
        rect(x + 32, y + 214, 506, 78), 16, MUTED);
    button(dc, ClosePlaybackOptions, L"完成 · 返回 [Esc]", rect(x + 32, y + 308, 506, 40), true);
}

double replayElapsed() {
    auto now = playbackClock(visualNow());
    return now >= s.replayFxAt ? double(now - s.replayFxAt) * s.replayRate : 0;
}
