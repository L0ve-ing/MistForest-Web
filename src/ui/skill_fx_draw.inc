ULONGLONG liveFxDuration() {
    const auto &p = s.playback.state();
    return p.endsAt > p.fxAt ? std::min(p.fxDuration, p.endsAt - p.fxAt) : p.fxDuration;
}

double slashOpacity() {
    return s.playback.state().fxLive
               ? effectOpacity(s.playback.state().fxAt, feedbackNow(), liveFxDuration())
               : 0;
}

const SkillFx *visibleSkillFx() {
    if (s.page == Page::Replay) {
        const auto &p = s.replayActors.state();
        if (s.replayFxStep != s.replay || !p.running || p.phaseIndex == 0)
            return nullptr;
        if (s.replayEffects.empty())
            return &s.replayEffect;
        auto phase = std::min(p.phaseIndex - 1, s.replayEffects.size() - 1);
        return &s.replayEffects[phase];
    }
    return s.historyTurn < 0 && s.playback.state().fxLive ? &s.playback.state().effect : nullptr;
}

double skillFxTime() {
    if (s.page == Page::Replay) {
        const auto &p = s.replayActors.state();
        auto now = playbackClock(visualNow());
        if (!p.running || p.phaseIndex == 0 || now < p.fxAt)
            return -1;
        auto duration =
            p.endsAt > p.fxAt ? std::min(p.fxDuration, p.endsAt - p.fxAt) : p.fxDuration;
        return double(now - p.fxAt) / duration;
    }
    ULONGLONG at = s.playback.state().fxAt, now = feedbackNow();
    return now < at ? -1 : double(now - at) / liveFxDuration();
}

bool replayAnimating(ULONGLONG now) {
    const auto &p = s.replayActors.state();
    // A delayed paint must still display every phase. Once the last phase has
    // been shown, the supplied (possibly paused) clock controls the reading tail.
    return s.page == Page::Replay && s.replayFxStep == s.replay && p.running &&
           (p.phaseIndex < p.phases.size() || playbackClock(now) < p.endsAt);
}

bool advanceReplay(ULONGLONG now) {
    if (s.page != Page::Replay || s.playbackOptions || s.pendingLeave || s.inspected >= 0)
        return false;
    return s.replayActors.advance(now);
}

bool growingRose(int cell) {
    const auto *fx = visibleSkillFx();
    double t = skillFxTime();
    return fx && cell > 0 && cell <= 81 && fx->buds[cell - 1] && t >= 0 && t < 1;
}

#ifdef _WIN32
namespace skillArt {
using namespace Gdiplus;
constexpr float Pi = 3.14159265f;

float clamp(float x) {
    return std::clamp(x, 0.f, 1.f);
}

float ease(float x) {
    x = clamp(x);
    return x * x * (3 - 2 * x);
}

Color color(COLORREF c, float alpha = 1) {
    return Color(BYTE(255 * clamp(alpha)), GetRValue(c), GetGValue(c), GetBValue(c));
}

COLORREF palette(FxStyle style, Role role) {
    switch (style) {
    case FxStyle::Snipe:
        return RGB(205, 153, 75);
    case FxStyle::Bite:
        return RGB(158, 133, 100);
    case FxStyle::Shotgun:
    case FxStyle::Dash:
        return role == Role::Shadow  ? RGB(128, 70, 176)
               : role == Role::Heavy ? RGB(174, 139, 91)
                                     : RGB(110, 153, 132);
    case FxStyle::ShadowCut:
    case FxStyle::ShadowLink:
        return RGB(143, 75, 190);
    case FxStyle::CrimsonClaw:
    case FxStyle::Blood:
        return RGB(202, 54, 64);
    case FxStyle::Arrow:
    case FxStyle::Claw:
    case FxStyle::Transform:
        return RGB(123, 167, 220);
    case FxStyle::Charge:
    case FxStyle::Thrust:
    case FxStyle::ArmorUp:
    case FxStyle::ArmorDown:
        return role == Role::Knight ? RGB(199, 176, 107) : RGB(174, 139, 91);
    case FxStyle::Fire:
    case FxStyle::FireStep:
    case FxStyle::Burn:
        return RGB(230, 113, 49);
    case FxStyle::RosePlant:
    case FxStyle::RoseBloom:
        return RGB(196, 65, 94);
    case FxStyle::SwordStab:
    case FxStyle::SwordThrow:
    case FxStyle::SwordRecall:
    case FxStyle::Advance:
        return RGB(75, 187, 207);
    default:
        return RGB(163, 186, 195);
    }
}

void line(Graphics &g, COLORREF c, float alpha, float width, float x, float y, float dx, float dy) {
    Pen p(color(c, alpha), width);
    p.SetStartCap(LineCapRound);
    p.SetEndCap(LineCapRound);
    g.DrawLine(&p, x, y, dx, dy);
}

void ellipse(Graphics &g, COLORREF c, float alpha, float x, float y, float w, float h) {
    SolidBrush b(color(c, alpha));
    g.FillEllipse(&b, x, y, w, h);
}

void ring(Graphics &g, COLORREF c, float alpha, float size, float width = 1.5f) {
    Pen p(color(c, alpha), width);
    g.DrawEllipse(&p, 50 - size, 50 - size, size * 2, size * 2);
}

void shards(Graphics &g, COLORREF c, float t, float alpha, int count = 7) {
    for (int i = 0; i < count; ++i) {
        float a = i * 2 * Pi / count + .37f, r = 12 + ease(t) * 30;
        float x = 50 + std::cos(a) * r, y = 50 + std::sin(a) * r * .75f;
        line(g, c, alpha * (1 - t), 1.5f, x, y, x + std::cos(a) * 4, y + std::sin(a) * 4);
    }
}

#include "skill_fx_polish.inc"

// Other skills retain their existing narrow ribbons.
void blade(Graphics &g, float t, COLORREF c, float alpha, bool mirror = false, float delay = 0,
           float weight = 1) {
    float local = (t - delay) / .43f;
    if (local <= 0)
        return;
    float head = ease(std::min(1.f, local)), tail = std::max(0.f, head - .78f);
    std::vector<PointF> left, right;
    for (int i = 0; i <= 20; ++i) {
        float f = i / 20.f, p = tail + (head - tail) * f;
        float x = 9 + 82 * p, y = 86 - 77 * p - 10 * std::sin(Pi * p);
        float w = weight * 6 * std::sin(Pi * f) * std::sqrt(std::max(0.f, head));
        if (mirror)
            x = 100 - x;
        float normalX = (mirror ? -1.f : 1.f) * w * .67f;
        left.emplace_back(x - normalX, y - w * .67f);
        right.emplace_back(x + normalX, y + w * .67f);
    }
    std::reverse(right.begin(), right.end());
    left.insert(left.end(), right.begin(), right.end());
    GraphicsPath path;
    path.AddPolygon(left.data(), int(left.size()));
    LinearGradientBrush brush(RectF(5, 5, 90, 90), color(c, alpha * .8f),
                              Color(BYTE(alpha * 255), 235, 253, 244), 45);
    g.FillPath(&brush, &path);
    Pen edge(color(c, alpha * .75f), .65f);
    g.DrawPath(&edge, &path);
    if (local > .3f)
        shards(g, c, t, alpha * .6f, 4);
}

void spear(Graphics &g, float t, COLORREF c, float alpha, bool arrow = false) {
    float head = 85 - ease(t / .4f) * 70, tail = std::min(94.f, head + 48);
    line(g, c, alpha * .25f, 8, 50, tail, 50, head);
    line(g, RGB(244, 250, 235), alpha, 2, 50, tail, 50, head);
    PointF points[] = {{50, head - 8}, {45, head + 7}, {50, head + 3}, {55, head + 7}};
    SolidBrush b(color(c, alpha));
    g.FillPolygon(&b, points, 4);
    if (arrow) {
        line(g, c, alpha, 2, 46, tail - 8, 50, tail);
        line(g, c, alpha, 2, 54, tail - 8, 50, tail);
    }
    shards(g, c, t, alpha * .6f, 5);
}

void claws(Graphics &g, float t, COLORREF c, float alpha, bool crimson) {
    for (int i = 0; i < 3; ++i) {
        auto saved = g.Save();
        g.TranslateTransform(float(i * 11 - 11), float(i * 4 - 4));
        blade(g, t, c, alpha, false, float(i) * .025f, .36f);
        g.Restore(saved);
    }
    if (crimson)
        for (int i = 0; i < 3; ++i) {
            auto saved = g.Save();
            g.TranslateTransform(float(i * 9 - 9), float(-i * 3));
            blade(g, t, c, alpha * .8f, true, .16f + float(i) * .02f, .3f);
            g.Restore(saved);
        }
}

void rose(Graphics &g, float t, float alpha, bool falling = false) {
    if (!falling) {
        roseOpening(g, t, alpha);
        return;
    }
    float growth = ease(t / .3f), open = ease((t - .16f) / .5f);
    COLORREF green = falling ? RGB(124, 133, 111) : RGB(56, 127, 79);
    line(g, green, alpha, 2.5f, 50, 85, 50, 85 - 43 * growth);
    if (growth > .4f) {
        auto saved = g.Save();
        g.TranslateTransform(50, 66);
        g.RotateTransform(-30);
        ellipse(g, green, alpha, -18, -4, 19 * growth, 7);
        g.Restore(saved);
    }
    for (int i = 0; i < 6; ++i) {
        float a = i * Pi / 3 - .5f, spread = falling ? 10 + t * 24 : 2 + open * 12;
        float x = 50 + std::cos(a) * spread,
              y = 40 + std::sin(a) * spread * .75f + (falling ? t * t * 32 : 0);
        auto saved = g.Save();
        g.TranslateTransform(x, y);
        g.RotateTransform(a * 180 / Pi + 90);
        ellipse(g, i % 2 ? RGB(218, 102, 128) : RGB(181, 53, 85), alpha, -(3 + open * 3), -9,
                6 + open * 6, 17);
        g.Restore(saved);
    }
    if (!falling) {
        ellipse(g, RGB(154, 43, 68), alpha, 46, 35, 8, 9);
        Pen curl(color(RGB(245, 170, 168), alpha * .8f), 1.2f);
        g.DrawArc(&curl, 46.f, 35.f, 8.f, 8.f, 40, 270);
    }
}

void petals(Graphics &g, float t, float alpha) {
    for (int i = 0; i < 7; ++i) {
        float a = i * 2 * Pi / 7 + t * .8f, r = 8 + ease(t) * 28;
        auto saved = g.Save();
        g.TranslateTransform(50 + std::cos(a) * r, 50 + std::sin(a) * r * .6f + t * 10);
        g.RotateTransform(i * 49 + t * 80);
        ellipse(g, i % 2 ? RGB(227, 119, 143) : RGB(179, 58, 87), alpha, -3, -6, 6, 11);
        g.Restore(saved);
    }
}

void fire(Graphics &g, float t, float alpha, bool quiet = false) {
    for (int i = 0; i < (quiet ? 3 : 6); ++i) {
        float x = 25 + i * (quiet ? 22.f : 10.f),
              h = (quiet ? 18.f : 38.f) * (1 - std::abs(i - 2.5f) * .13f);
        h *= .65f + .35f * std::sin(t * 8 - i * .9f);
        float y = 76 - ease(t / .35f) * 12;
        GraphicsPath flame;
        flame.AddBezier(x - 7, y, x - 14, y - h * .45f, x + 5, y - h * .55f, x, y - h);
        flame.AddBezier(x, y - h, x + 20, y - h * .2f, x + 10, y + 4, x - 7, y);
        flame.CloseFigure();
        LinearGradientBrush b(RectF(x - 10, y - h, 25, h + 5), color(RGB(255, 215, 114), alpha),
                              color(RGB(222, 79, 37), alpha * .7f), 90);
        g.FillPath(&b, &flame);
    }
    shards(g, RGB(241, 145, 59), t, alpha * .7f, 5);
}

void shield(Graphics &g, float t, COLORREF c, float alpha, bool opening = false) {
    for (int i = 0; i < 4; ++i) {
        auto saved = g.Save();
        float shift = (opening ? ease(t) : 1 - ease(t)) * 18;
        g.TranslateTransform((i % 2 ? 1.f : -1.f) * shift, (i / 2 ? 1.f : -1.f) * shift);
        float x = 29 + (i % 2) * 21.f, y = 23 + (i / 2) * 24.f;
        PointF points[] = {
            {x, y}, {x + 20, y}, {x + 18, y + 23}, {x + 10, y + 31}, {x + 2, y + 23}};
        SolidBrush b(color(c, alpha * .25f));
        Pen p(color(c, alpha), 1.7f);
        g.FillPolygon(&b, points, 5);
        g.DrawPolygon(&p, points, 5);
        g.Restore(saved);
    }
}

void draw(Graphics &g, const SkillFx &fx, int cell, float t, float alpha) {
    COLORREF c = palette(fx.style, fx.role);
    bool painted = fx.cells[cell] && paintedRoleAttack(g, fx, cell, t, alpha);
    if (fx.cells[cell] && !painted)
        switch (fx.style) {
        case FxStyle::None:
            break;
        case FxStyle::Cross:
            crossSlash(g, t, alpha);
            break;
        case FxStyle::Snipe:
            line(g, RGB(102, 97, 91), alpha * .6f, 2, 28, 72, 74, 27);
            for (int i = 0; i < 4; ++i) {
                auto saved = g.Save();
                g.TranslateTransform(50, 50);
                g.RotateTransform(float(i * 90));
                line(g, c, alpha * (1 - t), 2, 0, 8, 0, 17 + 13 * ease(t));
                g.Restore(saved);
            }
            ellipse(g, RGB(250, 225, 160), alpha, 46, 46, 8 * (1 - t), 8 * (1 - t));
            shards(g, c, t, alpha);
            break;
        case FxStyle::Bite:
            for (int sign : {-1, 1})
                for (int i = 0; i < 3; ++i) {
                    float x = 34 + i * 16.f, y = 50 + sign * (24 - 17 * ease(t / .4f));
                    line(g, c, alpha, 3, x - 4, y + sign * 7, x, y - sign * 4);
                }
            shards(g, c, t, alpha * .6f, 5);
            break;
        case FxStyle::Shotgun:
            for (int i = 0; i < 6; ++i) {
                float x = 26 + i * 9.f, y = 43 + (i % 3) * 7.f;
                line(g, RGB(220, 232, 225), alpha, 2.2f, x - 5, y + 13 * (1 - ease(t)), x + 4,
                     y - 4);
                ellipse(g, c, alpha * (1 - t), x, y, 3, 3);
            }
            {
                Pen p(color(c, alpha * .65f), 2);
                g.DrawArc(&p, 18.f, 18.f, 64.f, 64.f, t * 120 - 45, 170);
            }
            break;
        case FxStyle::ShadowCut:
            blade(g, t, c, alpha, false, 0, .65f);
            blade(g, t - .13f, c, alpha * .35f, false, 0, .45f);
            shards(g, c, t, alpha, 9);
            break;
        case FxStyle::Claw:
            claws(g, t, c, alpha, false);
            break;
        case FxStyle::CrimsonClaw:
            claws(g, t, c, alpha, true);
            break;
        case FxStyle::Arrow:
            spear(g, t, c, alpha, true);
            break;
        case FxStyle::Thrust:
            spear(g, t, c, alpha);
            break;
        case FxStyle::SwordStab:
            swordEnergy(g, t, alpha);
            break;
        case FxStyle::SwordThrow: {
            auto saved = g.Save();
            g.TranslateTransform(0, -(1 - ease(t / .5f)) * 55);
            line(g, c, alpha, 3, 50, 25, 50, 68);
            line(g, RGB(237, 255, 255), alpha, 1, 50, 25, 50, 68);
            line(g, RGB(197, 177, 109), alpha, 3, 40, 34, 60, 34);
            g.Restore(saved);
        }
            ring(g, c, alpha * .5f, 12 + ease(t) * 22);
            break;
        case FxStyle::SwordRecall: {
            float shrink = 1 - ease(t);
            line(g, c, alpha, 2, 50, 50 - 23 * shrink, 50, 50 + 23 * shrink);
            line(g, RGB(192, 173, 103), alpha, 2, 50 - 10 * shrink, 40, 50 + 10 * shrink, 40);
            ring(g, c, alpha * .55f, 5 + shrink * 25);
        } break;
        case FxStyle::RosePlant:
            if (!fx.buds[cell])
                rose(g, t, alpha);
            break;
        case FxStyle::RoseBloom:
            if (fx.attacks[cell]) {
                Pen p(color(RGB(47, 120, 68), alpha), 3);
                g.DrawBezier(&p, 15.f, 80.f, 20.f, 45.f, 70.f, 72.f, 85.f, 25.f);
                for (int i = 0; i < 4; ++i)
                    line(g, RGB(47, 120, 68), alpha, 2, 29 + i * 13.f, 65 - i * 6.f, 25 + i * 13.f,
                         53 - i * 6.f);
            }
            petals(g, t, alpha);
            break;
        case FxStyle::Charge: {
            Pen p(color(c, alpha), 4);
            g.DrawArc(&p, 15.f, 20.f, 70.f, 60.f, 185 - ease(t) * 25, 170);
            line(g, c, alpha * .4f, 7, 22, 77, 77, 26);
            shards(g, c, t, alpha, 8);
        } break;
        case FxStyle::Fire:
            fire(g, t, alpha);
            break;
        case FxStyle::Burn:
            fire(g, t, alpha * .65f, true);
            break;
        case FxStyle::FireStep:
            fire(g, t, alpha * .65f, true);
            ring(g, c, alpha * .5f, 10 + ease(t) * 22);
            break;
        case FxStyle::Dash:
            for (int i = 0; i < 3; ++i) {
                Pen p(color(c, alpha * (.25f + i * .2f)), 2);
                g.DrawArc(&p, 14.f + i * 7, 20.f, 60.f, 58.f, -40 + ease(t) * 90 + i * 30, 135);
            }
            break;
        case FxStyle::Transform:
            for (int i = 0; i < 2; ++i) {
                Pen p(color(c, alpha * (1 - t)), 2);
                g.DrawArc(&p, 22.f + i * 5, 15.f + i * 5, 56.f - i * 10, 70.f - i * 10,
                          t * 180 + i * 180, 150);
            }
            if (fx.form)
                claws(g, t, c, alpha * .4f, false);
            else
                spear(g, t, c, alpha * .4f, true);
            break;
        case FxStyle::ShadowLink: {
            float radius = fx.kind == Kind::Follow ? 30 - 15 * ease(t) : 14 + 16 * ease(t);
            Pen p(color(c, alpha * .8f), 2);
            g.DrawArc(&p, 50 - radius, 50 - radius, radius * 2, radius * 2, t * 140, 270);
            ellipse(g, c, alpha * .5f, 41, 35, 18, 25);
            if (fx.kind == Kind::Lead)
                blade(g, t, c, alpha * .35f);
            shards(g, c, t, alpha, 6);
        } break;
        case FxStyle::Blood:
            ring(g, c, alpha * .65f, 10 + ease(t) * 27);
            claws(g, t, c, alpha * .45f, false);
            break;
        case FxStyle::ArmorUp:
            shield(g, t, c, alpha);
            break;
        case FxStyle::ArmorDown:
            shield(g, t, c, alpha, true);
            break;
        case FxStyle::Silence:
            ring(g, RGB(194, 174, 117), alpha, 32 - 14 * ease(t));
            line(g, c, alpha, 2, 33, 33, 67, 67);
            line(g, c, alpha, 2, 67, 33, 33, 67);
            break;
        case FxStyle::Jump:
            ring(g, c, alpha * .55f,
                 fx.kind == Kind::Retreat ? 35 - ease(t) * 27 : 8 + ease(t) * 27);
            shards(g, RGB(211, 193, 139), t, alpha, 6);
            break;
        case FxStyle::Advance:
            ring(g, c, alpha * .55f, 32 - 20 * ease(t));
            shards(g, c, t, alpha * .6f, 5);
            break;
        case FxStyle::Build:
        case FxStyle::Demolish:
            for (int i = 0; i < 6; ++i) {
                float y = 66 - (i / 3) * 17.f +
                          (fx.style == FxStyle::Demolish ? ease(t) * 19 : -(1 - ease(t)) * 25);
                SolidBrush b(color(RGB(160, 175, 150), alpha * .7f));
                g.FillRectangle(&b, 24.f + (i % 3) * 18, y, 16.f, 14.f);
            }
            break;
        }
    if (fx.buds[cell])
        rose(g, t, alpha);
    if (fx.petals[cell] && (fx.style != FxStyle::RoseBloom || !fx.attacks[cell]) &&
        !paintedEffect(g, EffectAsset::RosePetals, t, alpha))
        petals(g, t, alpha);
    if (fx.wither[cell] && !paintedEffect(g, EffectAsset::RoseWither, t, alpha))
        rose(g, t, alpha * (1 - t), true);
    if (fx.shield[cell] && !paintedFeedback(g, EffectAsset::ShieldHit, t, alpha)) {
        ring(g, RGB(89, 173, 186), alpha * .9f, 20 + ease(t) * 10, 2);
        shield(g, t, RGB(115, 197, 208), alpha * .6f);
    }
    if (fx.health[cell]) {
        if (!paintedFeedback(g, EffectAsset::HealthHit, t, alpha)) {
            ring(g, RGB(204, 81, 65), alpha * .7f, 8 + ease(t) * 24);
            shards(g, RGB(218, 105, 76), t, alpha * .8f, 5);
        }
    } else if (fx.impact[cell] && !fx.shield[cell]) {
        if (!paintedFeedback(g, EffectAsset::PublicHit, t, alpha))
            ring(g, RGB(210, 175, 99), alpha * .6f, 6 + ease(t) * 20);
    }
}
} // namespace skillArt
#endif

void attackSlash(HDC dc, RECT cell, int number, const RECT *roseSlot = nullptr) {
    const auto *fx = visibleSkillFx();
    double time = skillFxTime();
    if (!fx || time < 0 || time >= 1 || (!fx->cells[number - 1] && !fx->wither[number - 1]))
        return;
#ifdef _WIN32
    float side = float(std::min(cell.right - cell.left - 8, cell.bottom - cell.top - 28));
    bool outerSilence =
        fx->style == FxStyle::Silence && fx->area[number - 1] && number != fx->anchor;
    if (outerSilence)
        side *= .72f;
    if (side < 6)
        return;
    art::ready();
    float t = float(time);
    bool bud = fx->buds[number - 1];
    RECT slot = roseSlot ? *roseSlot : objectSlots(cell, 1).front();
    float settle = bud ? skillArt::ease((t - .70f) / .30f) : 0;
    {
        art::Canvas canvas(dc);
        auto &g = *canvas.graphics;
        g.SetClip(Gdiplus::RectF(float(cell.left + 2), float(cell.top + 23),
                                 float(cell.right - cell.left - 4),
                                 float(cell.bottom - cell.top - 25)),
                  Gdiplus::CombineModeIntersect);
        float x = (cell.left + cell.right - side) / 2.f, y = cell.top + 23.f;
        float targetSide = float(std::min(slot.right - slot.left, slot.bottom - slot.top));
        g.TranslateTransform(x + (slot.left - x) * settle, y + (slot.top - y) * settle);
        float scale = (side + (targetSide - side) * settle) / 100;
        g.ScaleTransform(scale, scale);
        bool polished = fx->style == FxStyle::Cross || fx->style == FxStyle::SwordStab ||
                        skillArt::paintedRoleIndex(fx->style, fx->role) >= 0 ||
                        skillArt::paintedSkillAsset(*fx) != mist::presentation::EffectAsset::None ||
                        fx->wither[number - 1] || fx->petals[number - 1];
        float alpha =
            bud ? 1 - settle
            : polished
                ? skillArt::polishOpacity(t)
                : float(effectOpacity(0, ULONGLONG(time * SkillFxDuration), SkillFxDuration));
        if (outerSilence)
            alpha *= .52f;
        if (!s.fullAnimation)
            alpha *= .55f;
        skillArt::draw(g, *fx, number - 1, t, alpha);
    }
    // Resolve into the exact existing public-object icon, without a last-frame pop.
    if (settle > 0)
        drawArt(dc, slot, 13, settle);
#endif
}

void fireEmbers(HDC dc, RECT cell, ULONGLONG now) {
#ifdef _WIN32
    if (!s.fullAnimation)
        return;
    if (cell.right - cell.left < 35)
        return;
    art::ready();
    art::Canvas canvas(dc);
    auto &g = *canvas.graphics;
    if (skillArt::effectAtlas(
            mist::presentation::effectSlot(mist::presentation::EffectAsset::Lake).group)) {
        float side = float(std::min(cell.right - cell.left - 8, 44L));
        g.SetClip(Gdiplus::RectF(float(cell.left + 2), float(cell.top + 23),
                                 float(cell.right - cell.left - 4),
                                 float(cell.bottom - cell.top - 25)));
        g.TranslateTransform(float(cell.right) - side - 4, float(cell.bottom) - side - 2);
        g.ScaleTransform(side / 100, side / 100);
        float phase = float(now % 3000) / 3000 * 2 * skillArt::Pi;
        skillArt::paintedEffect(g, mist::presentation::EffectAsset::Embers,
                                .325f * (1 - std::cos(phase)), .65f);
        return;
    }
    for (int i = 0; i < 3; ++i) {
        float t = float((now + i * 460) % 1800) / 1800;
        skillArt::ellipse(g, RGB(214, 119, 48), std::sin(t * skillArt::Pi) * .45f,
                          float(cell.right - 12 - i * 5), float(cell.bottom - 8) - t * 13, 2, 2);
    }
#endif
}
