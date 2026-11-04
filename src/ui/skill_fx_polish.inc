// Local, deterministic artwork only: no source coordinates or game-state mutation.
#include "skill_fx_sprite.inc"

float polishOpacity(float t) {
    return 1 - ease((t - .67f) / .33f);
}

void shape(Graphics &g, GraphicsPath &path, COLORREF fill, float alpha,
           COLORREF edge = RGB(55, 71, 68), float width = .7f) {
    SolidBrush brush(color(fill, alpha));
    g.FillPath(&brush, &path);
    if (width > 0) {
        Pen pen(color(edge, alpha), width);
        pen.SetLineJoin(LineJoinRound);
        g.DrawPath(&pen, &path);
    }
}

void glint(Graphics &g, float x, float y, float radius, float alpha) {
    PointF points[] = {{x, y - radius}, {x + radius * .18f, y - radius * .18f},
                       {x + radius, y}, {x + radius * .18f, y + radius * .18f},
                       {x, y + radius}, {x - radius * .18f, y + radius * .18f},
                       {x - radius, y}, {x - radius * .18f, y - radius * .18f}};
    GraphicsPath star;
    star.AddPolygon(points, 8);
    shape(g, star, RGB(255, 251, 215), alpha, RGB(198, 144, 53), .6f);
}

// Swept curved blade with a dark back, bronze bevel and bright cutting edge.
void crescent(Graphics &g, float t, float alpha, bool mirror) {
    if (t <= 0 || t >= 1)
        return;
    float head = 1 - std::pow(1 - clamp(t / .43f), 3.f);
    float tail = ease((t - .60f) / .40f) * .80f;
    alpha *= 1 - ease((t - .66f) / .34f);
    auto saved = g.Save();
    if (mirror) {
        g.TranslateTransform(100, 0);
        g.ScaleTransform(-1, 1);
    }
    for (int layer = 0; layer < 3; ++layer) {
        std::vector<PointF> points, other;
        for (int i = 0; i <= 32; ++i) {
            float f = i / 32.f, p = tail + (head - tail) * f;
            float x = 11 + 78 * p + 18 * std::sin(Pi * p), y = 86 - 74 * p;
            float w = std::pow(std::max(0.f, std::sin(Pi * f)), .8f) * std::sqrt(head) *
                      (layer == 0   ? 8.2f
                       : layer == 1 ? 5.7f
                                    : 2.3f);
            float offset = layer == 2 ? -1.8f : 0;
            points.emplace_back(x - (w + offset) * .7f, y - (w + offset) * .7f);
            other.emplace_back(x + (w - offset) * .7f, y + (w - offset) * .7f);
        }
        std::reverse(other.begin(), other.end());
        points.insert(points.end(), other.begin(), other.end());
        GraphicsPath ribbon;
        ribbon.AddPolygon(points.data(), int(points.size()));
        shape(g, ribbon,
              layer == 0   ? RGB(65, 73, 77)
              : layer == 1 ? RGB(211, 158, 73)
                           : RGB(255, 252, 224),
              alpha, RGB(65, 73, 77), layer == 0 ? .5f : 0);
    }
    // Short parallel trails follow the blade rather than becoming another solid X.
    for (int i = 0; i < 2; ++i) {
        float begin = .12f + i * .22f, end = std::min(head - .07f, begin + .22f);
        if (end <= begin)
            continue;
        PointF points[9];
        for (int j = 0; j < 9; ++j) {
            float p = begin + (end - begin) * j / 8;
            points[j] = PointF(11 + 78 * p + 18 * std::sin(Pi * p) - 8 - i * 3, 86 - 74 * p - 4);
        }
        Pen trail(color(RGB(173, 125, 56), alpha * .55f), .9f);
        g.DrawLines(&trail, points, 9);
    }
    g.Restore(saved);
}

void crossSlash(Graphics &g, float t, float alpha) {
    if (paintedEffect(g, EffectAsset::Cross, t, alpha))
        return;
    crescent(g, t, alpha, false);
    crescent(g, (t - .15f) / .85f, alpha, true);
    float flash = clamp(1 - std::abs(t - .40f) / .13f);
    if (flash > 0)
        glint(g, 61, 42, 5 + flash * 10, alpha * flash);
    for (int i = 0; i < 5 && t > .28f; ++i) {
        float age = clamp((t - .28f) / .72f), a = i * 2 * Pi / 5 + .4f;
        float r = 12 + ease(age) * 26, x = 56 + std::cos(a) * r, y = 45 + std::sin(a) * r * .80f;
        auto saved = g.Save();
        g.TranslateTransform(x, y);
        g.RotateTransform(a * 180 / Pi + 35);
        PointF p[] = {{-1.6f, -2.8f}, {1.5f, -1.6f}, {1.1f, 2.8f}, {-1.4f, 1.2f}};
        GraphicsPath chip;
        chip.AddPolygon(p, 4);
        shape(g, chip, RGB(231, 177, 79), alpha * (1 - ease(age)), RGB(98, 78, 50), .5f);
        g.Restore(saved);
    }
}

void swordEnergy(Graphics &g, float t, float alpha) {
    if (paintedEffect(g, EffectAsset::SwordStab, t, alpha))
        return;
    float sweep = ease((t - .08f) / .45f), settle = 1 - ease((t - .58f) / .42f);
    // Cyan crescent wakes wrap a recognizable slender sword, all within this target cell.
    auto saved = g.Save();
    g.TranslateTransform(50, 50);
    g.RotateTransform(25 - 18 * sweep);
    g.TranslateTransform(-50, -50);
    for (int i = 0; i < 2; ++i) {
        auto turn = g.Save();
        if (i) {
            g.TranslateTransform(100, 100);
            g.RotateTransform(180);
        }
        g.TranslateTransform(0, (1 - sweep) * 11);
        float a = alpha * ease((t - i * .07f) / .15f) * settle;
        GraphicsPath wake;
        wake.AddBezier(18.f, 78.f, 75.f, 90.f, 99.f, 34.f, 71.f, 14.f);
        wake.AddBezier(71.f, 14.f, 90.f, 42.f, 67.f, 72.f, 18.f, 78.f);
        wake.CloseFigure();
        shape(g, wake, RGB(74, 191, 211), a * .85f, RGB(25, 117, 153), .7f);
        Pen core(color(RGB(232, 255, 255), a), 1.4f);
        g.DrawBezier(&core, 30.f, 77.f, 65.f, 77.f, 92.f, 43.f, 76.f, 22.f);
        Pen trail(color(RGB(49, 153, 184), a * .5f), .8f);
        g.DrawBezier(&trail, 22.f, 84.f, 69.f, 91.f, 101.f, 42.f, 81.f, 20.f);
        g.Restore(turn);
    }
    g.Restore(saved);
    saved = g.Save();
    g.TranslateTransform(50 + sweep * 4, 55 - sweep * 6);
    g.RotateTransform(31);
    float body = alpha * (1 - ease((t - .65f) / .30f));
    GraphicsPath bladeCore;
    PointF p[] = {{0, -39}, {3.2f, -26}, {2.5f, 16}, {0, 20}, {-2.5f, 16}, {-3.2f, -26}};
    bladeCore.AddPolygon(p, 6);
    shape(g, bladeCore, RGB(214, 248, 248), body, RGB(33, 119, 146), 1);
    line(g, RGB(255, 255, 244), body, 1.2f, 0, -35, 0, 17);
    line(g, RGB(51, 117, 139), body, 4, 0, 20, 0, 31);
    line(g, RGB(211, 174, 78), body, 3, -8, 18, 8, 18);
    line(g, RGB(255, 234, 159), body, 1, -7, 17, 7, 17);
    ellipse(g, RGB(201, 162, 72), body, -2, 30, 4, 4);
    g.Restore(saved);
    if (t > .24f && t < .60f)
        glint(g, 60, 37, 4 * std::sin((t - .24f) / .36f * Pi), alpha * .8f);
    for (int i = 0; i < 5 && t > .3f; ++i) {
        float u = ease((t - .3f) / .7f), x = 20 + i * 14.f + u * 7, y = 78 - i * 13.f - u * 10;
        line(g, RGB(33, 155, 190), alpha * u, 1.7f, x, y, x + 4, y - 7);
        line(g, RGB(226, 253, 251), alpha * u, .7f, x, y, x + 3, y - 5);
    }
}

void roseLeaf(Graphics &g, float x, float y, float angle, float scale, float alpha) {
    auto saved = g.Save();
    g.TranslateTransform(x, y);
    g.RotateTransform(angle);
    g.ScaleTransform(scale, scale);
    GraphicsPath leaf;
    leaf.AddBezier(0.f, 0.f, -12.f, 4.f, -20.f, -4.f, -24.f, -12.f);
    leaf.AddBezier(-24.f, -12.f, -11.f, -13.f, -3.f, -8.f, 0.f, 0.f);
    shape(g, leaf, RGB(79, 128, 65), alpha, RGB(42, 86, 53));
    line(g, RGB(164, 181, 99), alpha * .8f, .8f, -2, -1, -20, -10);
    g.Restore(saved);
}

void roseOpening(Graphics &g, float t, float alpha) {
    if (paintedEffect(g, EffectAsset::RosePlant, t, alpha))
        return;
    float grow = ease(t / .24f), open = ease((t - .15f) / .48f);
    Pen stem(color(RGB(43, 96, 58), alpha), 2.5f);
    g.DrawBezier(&stem, 50.f, 86.f, 58.f, 70.f, 43.f, 65.f, 50.f, 48.f + (1 - grow) * 32);
    roseLeaf(g, 51, 72, 12, grow * .85f, alpha);
    roseLeaf(g, 52, 80, 171, grow * .72f, alpha);
    auto saved = g.Save();
    g.TranslateTransform(50, 43 + (1 - grow) * 34);
    g.ScaleTransform(.40f + open * .60f, .65f + grow * .35f);
    // Overlapping cupped petals: broad outside, tightly curled in the centre.
    for (int ring = 0; ring < 3; ++ring) {
        int count = ring == 2 ? 3 : 5;
        for (int i = 0; i < count; ++i) {
            auto petal = g.Save();
            float a = i * 360.f / count + ring * 37.f;
            g.RotateTransform(a);
            float spread = (ring == 0 ? 11.f : ring == 1 ? 5.f : 1.f) * open;
            g.TranslateTransform(0, -spread);
            float size = ring == 0 ? 1.f : ring == 1 ? .72f : .43f;
            g.ScaleTransform(size, size);
            GraphicsPath p;
            p.AddBezier(-12.f, 5.f, -23.f, -2.f, -22.f, -14.f, -11.f, -17.f);
            p.AddBezier(-11.f, -17.f, -2.f, -24.f, 13.f, -20.f, 17.f, -10.f);
            p.AddBezier(17.f, -10.f, 22.f, 4.f, 8.f, 13.f, -12.f, 5.f);
            shape(g, p, i % 2 ? RGB(193, 32, 54) : RGB(221, 49, 58), alpha, RGB(119, 24, 44), .9f);
            Pen rim(color(RGB(255, 128, 110), alpha * .8f), 1.2f);
            g.DrawBezier(&rim, -16.f, -8.f, -10.f, -18.f, 5.f, -19.f, 12.f, -12.f);
            g.Restore(petal);
        }
    }
    Pen curl(color(RGB(255, 175, 123), alpha), 1.3f);
    g.DrawBezier(&curl, -6.f, 0.f, -10.f, -9.f, 7.f, -10.f, 7.f, -2.f);
    g.DrawBezier(&curl, 7.f, -2.f, 7.f, 5.f, -4.f, 6.f, -3.f, 0.f);
    g.Restore(saved);
    if (t > .34f)
        for (int i = 0; i < 3; ++i) {
            float u = clamp((t - .34f - i * .05f) / .66f);
            auto petal = g.Save();
            g.TranslateTransform(30 + i * 22.f + std::sin(u * 3 + i) * 8,
                                 41 - ease(u) * (22 + i * 3) + u * u * 9);
            g.RotateTransform(i * 71 + u * 120);
            GraphicsPath p;
            p.AddBezier(-3.f, 3.f, -8.f, -4.f, 2.f, -8.f, 5.f, -3.f);
            p.AddBezier(5.f, -3.f, 1.f, -2.f, 2.f, 2.f, -3.f, 3.f);
            shape(g, p, RGB(212, 52, 66), alpha * std::sin(Pi * u), RGB(137, 29, 48), .5f);
            g.Restore(petal);
        }
    glint(g, 22, 52, 2.3f, alpha * open * .7f);
    glint(g, 79, 35, 2.6f, alpha * open * .7f);
}
