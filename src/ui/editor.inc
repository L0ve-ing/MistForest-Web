void editor(HDC dc) { // 地图编辑器不显示任何对战秘密位置。
    card(dc, rect(36, 122, 570, 599));
    card(dc, rect(638, 122, 446, 599));
    text(dc,
         L"地图 DIY · " + std::to_wstring(s.editor.document.map().width) + L"×" +
             std::to_wstring(s.editor.document.map().height),
         rect(64, 146, 514, 43), 25, INK, true);
    text(dc, L"点击格子添加地形；线形墙工具点击格子边。", rect(65, 195, 510, 26), 14,
         MUTED); // 左侧编辑方式。
    board(dc, mapMode(s.editor.document.map()), s.editor.document.map(), {});
    text(dc, L"棕色：边墙；蓝色：湖泊；紫色：漩涡。", rect(65, 688, 516, 25), 13,
         MUTED); // 地形使用不同颜色和文字区分。
    text(dc, L"塑造你的战场", rect(664, 151, 392, 43), 25, INK, true);
    button(dc, BrushGround, L"空地", rect(664, 211, 190, 40), s.editor.document.brush() == Brush::Ground);
    button(dc, BrushWall, L"方形墙", rect(867, 211, 190, 40),
           s.editor.document.brush() == Brush::Wall); // 全部格子均可编辑。
    button(dc, BrushLake, L"湖泊", rect(664, 262, 190, 40), s.editor.document.brush() == Brush::Lake);
    button(dc, BrushWhirlpool, L"漩涡", rect(867, 262, 190, 40), s.editor.document.brush() == Brush::Whirlpool);
    button(dc, BrushLine, L"线形墙", rect(664, 313, 190, 40), s.editor.document.brush() == Brush::Line);
    button(dc, BrushErase, L"橡皮擦", rect(867, 313, 190, 40), s.editor.document.brush() == Brush::Erase);
    button(dc, MapUndo, L"撤销修改", rect(664, 364, 393, 36), false,
           !(s.editor.document.undoDepth() == 0)); // 加入漩涡工具并保留撤销。
    text(
        dc,
        L"墙挡移动与攻击；湖泊只挡移动。\n漩涡可站立，开局及每轮开始吸引邻格角色。\n至少保留一个可站立格。",
        rect(664, 416, 392, 65), 15, MUTED); // 简明地形说明。
    button(dc, MapEmpty, L"全空地图", rect(664, 483, 190, 37));
    button(dc, MapReset, L"恢复默认", rect(867, 483, 190, 37));
    button(dc, MapSave, L"保存到本地…", rect(664, 532, 190, 42), false,
           validMap(Mode::Heroes, s.editor.document.map()));
    button(dc, MapLoad, L"打开地图…",
           rect(867, 532, 190, 42)); // 保存和打开通过系统文件对话框完成。
    text(dc,
         s.editor.notice.empty() ? L"保存文件后可分享；点击下方按钮应用到对战。" : s.editor.notice,
         rect(664, 589, 392, 49), 13, TEAL);
    button(dc, MapApply, L"使用此地图 · 返回设置", rect(664, 653, 393, 43), true,
           validMap(Mode::Heroes, s.editor.document.map())); // 草稿应用后可用于人机、本地与联机。
    button(dc, RandomMap, L"生成随机地图", rect(664, 765, 190, 42), true);
    button(dc, SeedRepeat, L"按上次种子重建", rect(867, 765, 190, 42));
    text(dc, L"宽（列数）", rect(64, 728, 170, 28), 19, TEAL, true);
    text(dc, L"高（行数）", rect(300, 728, 170, 28), 19, TEAL, true);
    button(dc, ResizeMap, L"应用尺寸（1—9）", rect(64, 841, 490, 48), true);
    text(dc, L"调整尺寸保留重叠区域地形和边墙；缩小移除超出区域。", rect(64, 910, 920, 47), 17,
         MUTED);
    card(dc, rect(180, 765, 100, 42));
    text(dc, std::to_wstring(inputNumber(s.editor.widthEdit, s.editor.document.map().width)), rect(185, 770, 90, 32),
         22);
    card(dc, rect(385, 765, 100, 42));
    text(dc, std::to_wstring(inputNumber(s.editor.heightEdit, s.editor.document.map().height)), rect(390, 770, 90, 32),
         22); // 输入宽高后明确提交调整。
    footer(
        dc,
        L"线形墙再次点击即可移除；空地工具只清除整格地形，清除边墙请用线形墙或橡皮擦。"); // 避免误解格子与墙边是两种独立对象。
} // 结束地图编辑器绘制。

void changeMapCell(int cell) {
    if (s.editor.document.paintCell(cell))
        s.editor.notice.clear();
}

void mapFileDialog(bool save) { // 文件选择允许玩家自行命名并选择保存位置。
    if (!s.window) {
        return;
    }
    if (save && !validMap(Mode::Heroes, s.editor.document.map())) {
        s.editor.notice = L"至少保留一个可站立格后才能保存。";
        return;
    } // 漩涡也算可站立格。
    wchar_t executable[32768]{}, filename[32768]{};
    GetModuleFileNameW(nullptr, executable, 32768);
    auto folder = std::filesystem::path(executable).parent_path() / L"maps";
    std::error_code error;
    std::filesystem::create_directories(folder, error);
    std::wstring initial = folder.wstring(); // 默认将玩家地图保存到程序旁的地图目录。
    std::wstring chosen =
        s.editor.path.empty() ? (save ? L"forest-map.mistmap" : L"") : s.editor.path.wstring();
    lstrcpynW(filename, chosen.c_str(), 32768);
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = s.window;
    dialog.lpstrFile = filename;
    dialog.nMaxFile = 32768;
    dialog.lpstrInitialDir = initial.c_str(); // 文件名支持中文路径。
    dialog.lpstrFilter = L"雾隐之森地图 (*.mistmap)\0*.mistmap\0所有文件 (*.*)\0*.*\0\0";
    dialog.lpstrDefExt = L"mistmap";
    dialog.lpstrTitle = save ? L"保存自定义地图" : L"打开自定义地图";
    dialog.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST |
                   (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST); // 覆盖已有文件由系统明确提示。
    if (!(save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog))) {
        if (CommDlgExtendedError())
            s.editor.notice = L"文件窗口打开失败，请重试。";
        return;
    } // 取消对话框不会改变地图。
    if (save) {
        if (saveMapFile(filename, s.editor.document.map(), &s.editor.notice)) {
            s.editor.path = filename;
            s.editor.notice = L"已保存：" + s.editor.path.filename().wstring();
        }
    } // 保存失败保持原文件和当前编辑草稿。
    else {
        Map loaded;
        if (loadMapFile(filename, loaded, &s.editor.notice)) {
            s.editor.document.replace(loaded);
            s.editor.notice.clear();
            syncDimensions();
            s.editor.path = filename;
            s.editor.notice = L"已打开：" + s.editor.path.filename().wstring();
        }
    } // 加载成功后也能撤销回原草稿。
} // 结束地图文件选择。

bool mapEditorCommand(int id) {
    auto &document = s.editor.document;
    switch (id) {
    case MapDefault:
        s.mode = Mode::Heroes;
        s.customEnabled = false;
        break;
    case MapUse:
        s.mode = Mode::Heroes;
        s.customEnabled = true;
        break;
    case BrushGround:
    case BrushWall:
    case BrushLake:
    case BrushWhirlpool:
    case BrushLine:
    case BrushErase:
        document.selectBrush(Brush(id - BrushGround));
        break;
    case MapUndo:
        if (document.undo())
            syncDimensions();
        break;
    case MapEmpty:
        document.clear();
        s.editor.notice.clear();
        break;
    case MapReset:
        document.reset();
        s.editor.notice.clear();
        syncDimensions();
        break;
    case ResizeMap:
        if (!document.resize(inputNumber(s.editor.widthEdit, document.map().width),
                             inputNumber(s.editor.heightEdit, document.map().height)))
            s.editor.notice = L"宽和高分别填写1—9。";
        else
            s.editor.notice = L"尺寸已调整。";
        break;
    case RandomMap:
        document.generate(unsigned(s.rng()));
        s.customMap = document.map();
        s.customEnabled = true;
        s.editor.notice = L"随机地图种子：" + std::to_wstring(document.seed());
        syncDimensions();
        break;
    case SeedRepeat:
        document.generate(document.seed());
        s.editor.notice.clear();
        break;
    case MapSave:
        mapFileDialog(true);
        break;
    case MapLoad:
        mapFileDialog(false);
        break;
    case MapApply:
        if (validMap(Mode::Heroes, document.map())) {
            s.customMap = document.map();
            s.customEnabled = true;
            s.mode = mapMode(document.map());
            changePage(Page::Config);
        }
        break;
    default:
        return false;
    }
    return true;
}
