mist::presentation::sprites::Pose currentActorPose(int unit, int piece) {
    if (s.page == Page::Replay)
        return s.replayActors.actorPose(unit, piece, playbackClock(visualNow()), s.fullAnimation);
    if (s.page == Page::Battle && s.historyTurn < 0)
        return s.playback.actorPose(unit, piece, feedbackNow(), s.fullAnimation);
    return {};
}

// 1 is an uncommitted selection; 2 is a live, viewer-known association.
int shadowMark(const View &v, int unit, int piece) {
    for (int id = v.player * 2; id >= 0 && id < v.player * 2 + v.teamSize && id < 8; ++id) {
        const auto &u = v.units[id];
        if (u.role != Role::Shadow || u.health <= 0 || u.silenced)
            continue;
        if (u.follow >= 0 && u.follow < 8 && v.units[u.follow].health > 0 &&
            (unit == id || (unit == u.follow && (piece < 0 || piece == u.followPiece))))
            return 2;
        if (battleInputReady() && id == s.choice.state().unit &&
            (s.choice.state().kind == Kind::Follow || s.choice.state().kind == Kind::Lead) &&
            (unit == id || (unit == s.choice.state().subject &&
                            (piece < 0 || piece == s.choice.state().enemyPiece))))
            return 1;
    }
    return 0;
}

void shadowRing(HDC dc, RECT slot, int state, bool portrait) {
#ifdef _WIN32
    if (!state)
        return;
    art::Canvas canvas(dc);
    auto &g = *canvas.graphics;
    float width = float(slot.right - slot.left), height = float(slot.bottom - slot.top);
    float pulse = state == 2 && s.fullAnimation
                      ? .1f * std::sin(float(feedbackNow() % 1800) / 1800 * 6.2831853f)
                      : 0;
    Gdiplus::Pen pen(Gdiplus::Color(BYTE((.7f + pulse) * 255), 137, 88, 182),
                     state == 2 ? 2.f : 1.5f);
    if (state == 1)
        pen.SetDashStyle(Gdiplus::DashStyleDash);
    if (portrait)
        g.DrawEllipse(&pen, float(slot.left + 2), float(slot.top + 2), width - 4, height - 4);
    else {
        float y = float(slot.bottom) - std::min(17.f, height * .3f);
        g.DrawEllipse(&pen, slot.left + 2.f, y - 4, width - 4, 7.f);
        if (state == 2) {
            art::line(g, slot.left + width * .22f, y, slot.left + width * .28f, y - 9,
                      Gdiplus::Color(100, 137, 88, 182), 1.5f);
            art::line(g, slot.right - width * .22f, y, slot.right - width * .28f, y - 9,
                      Gdiplus::Color(100, 137, 88, 182), 1.5f);
        }
    }
#endif
}

void actorStatus(HDC dc, RECT slot, const View &v, int unit, int piece) {
    shadowRing(dc, slot, shadowMark(v, unit, piece));
#ifdef _WIN32
    const auto &u = v.units[unit];
    bool silence = u.role == Role::WallGuard ? u.soldierSilenced[piece] : u.silenced;
    if (silence && slot.right - slot.left >= 22 && slot.bottom - slot.top >= 22) {
        art::Canvas canvas(dc);
        auto &g = *canvas.graphics;
        float x = float(slot.right - 14), y = float(slot.top + 1);
        art::ellipse(g, x, y, 12, 12, Gdiplus::Color(230, 51, 97, 145));
        Gdiplus::Pen rim(Gdiplus::Color(230, 215, 181, 95), 1);
        g.DrawEllipse(&rim, x, y, 12.f, 12.f);
        for (int i = 0; i < 3; ++i) {
            float half = i == 1 ? 3.f : 1.5f;
            art::line(g, x + 3 + i * 3, y + 6 - half, x + 3 + i * 3, y + 6 + half,
                      Gdiplus::Color(255, 246, 246, 220));
        }
    }
#endif
}

void selectionCorners(HDC dc, RECT r) {
    int length = std::clamp(int(r.right - r.left) / 7, 5, 13);
    HPEN pen = CreatePen(PS_SOLID, 2, GOLD);
    auto old = SelectObject(dc, pen);
    for (int x : {int(r.left + 3), int(r.right - 4)})
        for (int y : {int(r.top + 3), int(r.bottom - 4)}) {
            int dx = x == r.left + 3 ? 1 : -1, dy = y == r.top + 3 ? 1 : -1;
            MoveToEx(dc, x + dx * length, y, nullptr);
            LineTo(dc, x, y);
            LineTo(dc, x, y + dy * length);
        }
    SelectObject(dc, old);
    DeleteObject(pen);
}

void impactAboveActor(HDC dc, RECT r, int cell) {
#ifdef _WIN32
    const auto *fx = visibleSkillFx();
    double t = skillFxTime();
    if (!fx || t < 0 || t >= 1 ||
        !(fx->health[cell - 1] || fx->shield[cell - 1] || fx->impact[cell - 1]))
        return;
    art::Canvas canvas(dc);
    auto &g = *canvas.graphics;
    float x = (r.left + r.right) / 2.f, y = r.top + (r.bottom - r.top) * .5f;
    float strength = float(1 - t), radius = (s.fullAnimation ? 11.f : 6.f) * strength;
    auto c = fx->shield[cell - 1] ? Gdiplus::Color(BYTE(180 * strength), 92, 172, 217)
                                  : Gdiplus::Color(BYTE(190 * strength), 218, 112, 79);
    for (int i = 0; i < 4; ++i) {
        double angle = i * 1.57079632679 + .4;
        art::line(g, x + float(std::cos(angle)) * radius * .5f,
                  y + float(std::sin(angle)) * radius * .5f, x + float(std::cos(angle)) * radius,
                  y + float(std::sin(angle)) * radius, c, 1.8f);
    }
#endif
}

void routePreview(HDC dc, Mode mode) {
#ifdef _WIN32
    if (!battleInputReady())
        return;
    View preview = liveView();
    auto plan = s.choice.state().planned;
    if (plan.empty() && s.choice.state().selected &&
        mist::presentation::sprites::movesActor(s.choice.state().kind)) {
        auto a = selectedAction(preview, s.choice.state().selected);
        if (validate(preview, a))
            plan.push_back(a);
    }
    for (const auto &action : plan) {
        if (!mist::presentation::sprites::movesActor(action.kind))
            continue;
        auto after = preview;
        int to = action.target;
        if (action.kind == Kind::Dash) {
            auto path = dashPath(preview, action.unit, to);
            if (!path.empty())
                to = path.back();
        }
        if (to < 1 || to > cellCount(mode))
            continue;
        auto &unit = after.units[action.unit];
        if (unit.role == Role::WallGuard)
            unit.soldiers[action.piece] = to;
        else
            unit.position = to;
        auto routes = mist::presentation::sprites::visibleMovements(preview, after, action.kind);
        for (const auto &route : routes) {
            if (route.style == mist::presentation::sprites::MotionStyle::Phase)
                continue;
            art::Canvas canvas(dc);
            auto &g = *canvas.graphics;
            RECT first = cellRect(mode, 1), last = cellRect(mode, cellCount(mode));
            g.SetClip(Gdiplus::RectF(float(first.left), float(first.top),
                                     float(last.right - first.left),
                                     float(last.bottom - first.top)),
                      Gdiplus::CombineModeIntersect);
            float gap = float(boardGap(mode)), cx = (first.left + first.right) / 2.f,
                  cy = first.top + (first.bottom - first.top) * .67f;
            Gdiplus::Pen pen(Gdiplus::Color(150, 38, 128, 92), 1.8f);
            pen.SetDashStyle(Gdiplus::DashStyleDot);
            for (std::size_t k = 1; k < route.path.size(); ++k)
                for (int sx :
                     (unit.role == Role::Shadow ? std::vector<int>{-size(mode), 0, size(mode)}
                                                : std::vector<int>{0}))
                    for (int sy :
                         (unit.role == Role::Shadow ? std::vector<int>{-rows(mode), 0, rows(mode)}
                                                    : std::vector<int>{0})) {
                        auto a = route.path[k - 1], b = route.path[k];
                        float x1 = cx + float(a.column + sx) * gap,
                              y1 = cy + float(a.row + sy) * gap;
                        float x2 = cx + float(b.column + sx) * gap,
                              y2 = cy + float(b.row + sy) * gap;
                        if (route.style == mist::presentation::sprites::MotionStyle::Leap)
                            g.DrawBezier(&pen, x1, y1, x1, y1 - gap * .5f, x2, y2 - gap * .5f, x2,
                                         y2);
                        else
                            g.DrawLine(&pen, x1, y1, x2, y2);
                        art::ellipse(g, x2 - 2, y2 - 2, 4, 4, Gdiplus::Color(170, 38, 128, 92));
                    }
        }
        preview = std::move(after);
    }
#endif
}
