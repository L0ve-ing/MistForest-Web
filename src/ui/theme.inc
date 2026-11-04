// Decorative animation never reads the game or advances presentation/game state.
struct VisualState {
    ULONGLONG introAt = 0, previewAt = 0;
    ULONGLONG introDuration = 2200;
    bool introSkipped = true, introBlocking = true, introPending = false;
    int hover = 0, pressed = 0;
} visual;

ULONGLONG visualNow() {
    return visual.previewAt ? visual.previewAt : GetTickCount64();
}

double introOpacity(ULONGLONG now) {
    if (visual.introSkipped || now >= visual.introAt + visual.introDuration)
        return 0;
    double t = now <= visual.introAt ? 0 : double(now - visual.introAt) / visual.introDuration;
    return 1 - t * t * (3 - 2 * t);
}

bool dismissIntro() {
    if (!visual.introBlocking || introOpacity(visualNow()) <= 0)
        return false;
    visual.introSkipped = true;
    return visual.introBlocking;
}

void beginPageTransition();
void rememberPageFrame(HDC dc);
void pageCrossfade(HDC dc, ULONGLONG now);

void changePage(Page page) {
    if (s.page == page)
        return;
    beginPageTransition();
    s.page = page;
    s.moreActions = false;
    visual.introAt = visualNow();
    visual.introDuration = 700;
    visual.introSkipped = false;
    visual.introBlocking = false;
    s.hits.clear();
}

double effectOpacity(ULONGLONG start, ULONGLONG now, ULONGLONG duration) {
    if (now < start || now - start >= duration)
        return 0;
    double age = double(now - start);
    return age < 90 ? 1 : (duration - age) / (duration - 90);
}
#ifdef _WIN32
namespace art {
using namespace Gdiplus;

struct Runtime {
    ULONG_PTR token = 0;

    Runtime() {
        GdiplusStartupInput input;
        GdiplusStartup(&token, &input, nullptr);
    }

    ~Runtime() {
        if (token)
            GdiplusShutdown(token);
    }
};

void ready() {
    static Runtime runtime;
}

struct Canvas {
    HDC dc;
    int saved;
    std::unique_ptr<Graphics> graphics;

    explicit Canvas(HDC target) : dc(target), saved(SaveDC(target)) {
        ready();
        SIZE window{1, 1}, viewport{1, 1};
        POINT origin{};
        bool mapped = GetMapMode(dc) == MM_ANISOTROPIC;
        if (mapped) {
            GetWindowExtEx(dc, &window);
            GetViewportExtEx(dc, &viewport);
            GetViewportOrgEx(dc, &origin);
        }
        // Reset the inherited mapping before constructing Graphics, then apply it once.
        SetMapMode(dc, MM_TEXT);
        SetViewportOrgEx(dc, 0, 0, nullptr);
        graphics = std::make_unique<Graphics>(dc);
        graphics->SetSmoothingMode(SmoothingModeAntiAlias);
        graphics->SetPixelOffsetMode(PixelOffsetModeHighQuality);
        if (mapped) {
            Matrix matrix(float(viewport.cx) / window.cx, 0, 0, float(viewport.cy) / window.cy,
                          float(origin.x), float(origin.y));
            graphics->SetTransform(&matrix);
        }
    }

    ~Canvas() {
        graphics.reset();
        RestoreDC(dc, saved);
    }
};

Color color(COLORREF c, BYTE alpha = 255) {
    return Color(alpha, GetRValue(c), GetGValue(c), GetBValue(c));
}

void ellipse(Graphics &g, float x, float y, float w, float h, Color c) {
    SolidBrush b(c);
    g.FillEllipse(&b, x, y, w, h);
}

void line(Graphics &g, float x1, float y1, float x2, float y2, Color c, float width = 1) {
    Pen p(c, width);
    p.SetStartCap(LineCapRound);
    p.SetEndCap(LineCapRound);
    g.DrawLine(&p, x1, y1, x2, y2);
}

void leaf(Graphics &g, float x, float y, float size, float rotation, Color c) {
    auto saved = g.Save();
    g.TranslateTransform(x, y);
    g.RotateTransform(rotation);
    GraphicsPath p;
    p.AddBezier(0.f, 0.f, -size * .6f, -size * .6f, -size * .25f, -size * 1.4f, 0.f, -size * 1.8f);
    p.AddBezier(0.f, -size * 1.8f, size * .65f, -size * 1.1f, size * .6f, -size * .3f, 0.f, 0.f);
    p.CloseFigure();
    SolidBrush b(c);
    g.FillPath(&b, &p);
    g.Restore(saved);
}

void sprig(Graphics &g, float x, float y, float size, bool mirror, Color c) {
    float d = mirror ? -1.f : 1.f;
    line(g, x, y, x + d * size, y - size * 1.3f, c, std::max(1.f, size * .025f));
    for (int i = 1; i <= 4; ++i) {
        float t = i / 5.f;
        leaf(g, x + d * size * t, y - size * 1.3f * t, size * .18f, mirror ? -65.f : 65.f, c);
        leaf(g, x + d * size * t, y - size * 1.3f * t, size * .18f, mirror ? 20.f : -20.f, c);
    }
}

void tree(Graphics &g, float x, float bottom, float height, float width, Color c) {
    PointF p[] = {{x, bottom - height},
                  {x - width * .26f, bottom - height * .67f},
                  {x - width * .16f, bottom - height * .69f},
                  {x - width * .42f, bottom - height * .42f},
                  {x - width * .27f, bottom - height * .47f},
                  {x - width * .55f, bottom - height * .14f},
                  {x - width * .08f, bottom - height * .27f},
                  {x - width * .07f, bottom},
                  {x + width * .07f, bottom},
                  {x + width * .08f, bottom - height * .27f},
                  {x + width * .55f, bottom - height * .14f},
                  {x + width * .27f, bottom - height * .47f},
                  {x + width * .42f, bottom - height * .42f},
                  {x + width * .16f, bottom - height * .69f},
                  {x + width * .26f, bottom - height * .67f}};
    SolidBrush b(c);
    g.FillPolygon(&b, p, 15);
    line(g, x, bottom - 8, x, bottom - height * .78f, Color(c.GetA() / 3, 148, 184, 149), 1.5f);
}

void cornerVine(Graphics &g, float x, float y, float directionX, float directionY,
                float scale = 1) {
    auto saved = g.Save();
    g.TranslateTransform(x, y);
    g.ScaleTransform(directionX * scale, directionY * scale);
    Pen stem(Color(155, 150, 137, 82), 1.1f);
    g.DrawBezier(&stem, 0.f, 42.f, 8.f, 25.f, -6.f, 9.f, 11.f, 4.f);
    g.DrawBezier(&stem, 11.f, 4.f, 23.f, -1.f, 32.f, 9.f, 49.f, 1.f);
    for (int i = 0; i < 4; ++i) {
        leaf(g, 6.f + i * 11, 4.f, 3.5f, i % 2 ? 120.f : 55.f, Color(140, 108, 138, 91));
        leaf(g, 2.f, 11.f + i * 8, 3.f, i % 2 ? -30.f : 200.f, Color(125, 120, 148, 100));
    }
    ellipse(g, 6, 5, 2.4f, 2.4f, Color(200, 192, 154, 74));
    g.Restore(saved);
}
}

void artPaint(Gdiplus::Graphics &g, Gdiplus::GraphicsPath &p, Gdiplus::Color c) {
    Gdiplus::SolidBrush b(c);
    g.FillPath(&b, &p);
}

void artPaint(Gdiplus::Graphics &g, Gdiplus::GraphicsPath &p, const char *id, float opacity) {
    using namespace Gdiplus;
    RectF bounds;
    p.GetBounds(&bounds);
    if (bounds.Width < .01f || bounds.Height < .01f)
        return;
    auto alpha = [&](int a) { return BYTE(std::clamp(a * opacity, 0.f, 255.f)); };
    std::string name = id;
    Color a, b;
    float angle = 0;
    if (name == "#rosePetals") {
        a = Color(alpha(255), 229, 131, 148);
        b = Color(alpha(255), 185, 63, 93);
        angle = 60;
    } else if (name == "#bladeMetal") {
        a = Color(alpha(255), 248, 255, 255);
        b = Color(alpha(255), 163, 195, 201);
    } else {
        a = Color(0, 37, 143, 126);
        b = name == "#trailV4" ? Color(alpha(70), 165, 246, 226) : Color(alpha(255), 228, 253, 255);
        angle = 315;
    }
    LinearGradientBrush brush(bounds, a, b, angle);
    if (name == "#edgeV4" || name == "#trailV4") {
        Color colors[] = {
            a,
            name == "#trailV4" ? Color(alpha(166), 64, 189, 166) : Color(alpha(255), 217, 255, 245),
            name == "#trailV4" ? Color(alpha(65), 100, 219, 200) : Color(alpha(255), 255, 255, 255),
            b};
        float stops[] = {0, .38f, .7f, 1};
        brush.SetInterpolationColors(colors, stops, 4);
    }
    g.FillPath(&brush, &p);
}

#include "art.inc"
#endif
void drawArt(HDC dc, RECT r, int kind, float opacity = 1) {
#ifdef _WIN32
    art::ready();
    art::Canvas canvas(dc);
    auto &g = *canvas.graphics;
    g.TranslateTransform(float(r.left), float(r.top));
    float view = kind == 17 ? 100.f
                            : (kind == 1 || kind == 2 || kind == 3 || kind == 4 || kind == 5 ||
                                       kind == 6 || kind == 10
                                   ? 48.f
                                   : 24.f);
    g.ScaleTransform((r.right - r.left) / view, (r.bottom - r.top) / view);
    approvedArt(g, kind, opacity);
#endif
}

void emblem(HDC dc, RECT r, Role role, bool own = true, bool selected = false) {
#ifdef _WIN32
    art::ready();
    {
        art::Canvas canvas(dc);
        auto &g = *canvas.graphics;
        float size = float(std::min(r.right - r.left, r.bottom - r.top));
        float x = r.left + (r.right - r.left - size) / 2.f,
              y = r.top + (r.bottom - r.top - size) / 2.f;
        art::ellipse(g, x, y, size, size, art::color(selected ? GOLD : RGB(169, 194, 174)));
        art::ellipse(g, x + 2, y + 2, size - 4, size - 4, art::color(RGB(237, 244, 235)));
        art::ellipse(g, x + 4, y + 4, size - 8, size - 8, art::color(own ? TEAL : CORAL));
    }
#endif
    int margin = std::max(4, int(std::min(r.right - r.left, r.bottom - r.top) * .16));
    drawArt(dc,
            rect(r.left + margin, r.top + margin, int(r.right - r.left) - 2 * margin,
                 int(r.bottom - r.top) - 2 * margin),
            int(role));
}

#include "forest_scene.inc"

void renderForestBackground(HDC dc, ULONGLONG now) {
#ifdef _WIN32
    if (forestScene::background(dc, now))
        return;
#endif
#ifdef _WIN32
    using namespace Gdiplus;
    art::ready();
    art::Canvas canvas(dc);
    auto &g = *canvas.graphics;
    float w = float(canvasWidth()), h = float(canvasHeight());
    bool lobby = s.page == Page::Lobby;
    LinearGradientBrush bg(RectF(0, 0, w, h),
                           lobby ? Color(255, 13, 46, 39) : Color(255, 225, 235, 220),
                           lobby ? Color(255, 57, 102, 82) : Color(255, 242, 243, 231), 90);
    g.FillRectangle(&bg, 0.f, 0.f, w, h);
    for (int layer = 0; layer < 3; ++layer)
        for (int i = 0; i < 10; ++i) {
            float x = w * (i + .2f) / 9.f;
            float height = h * (.38f + .08f * ((i * 7 + layer * 3) % 5)),
                  bottom = h * (.78f + .07f * layer);
            art::tree(g, x, bottom, height, w * .12f,
                      lobby ? Color(BYTE(85 + layer * 38), BYTE(96 - layer * 27),
                                    BYTE(140 - layer * 28), BYTE(112 - layer * 25))
                            : Color(BYTE(20 + layer * 7), 85, 122, 87));
        }
    // Broad, translucent banks drift over an eighteen-second cycle, behind content.
    float drift = std::sin(double(now % 18000) / 18000 * 6.2831853) * w * .1f;
    for (int i = 0; i < 3; ++i) {
        RectF bank(-w * .25f + drift * (i % 2 ? -1 : 1), h * (.40f + i * .23f), w * 1.5f, h * .2f);
        GraphicsPath p;
        p.AddEllipse(bank);
        PathGradientBrush mist(&p);
        mist.SetCenterColor(lobby ? Color(60, 214, 228, 201) : Color(20, 255, 255, 247));
        Color edge(0, 224, 237, 212);
        int count = 1;
        mist.SetSurroundColors(&edge, &count);
        g.FillPath(&mist, &p);
    }
    auto leafColor = lobby ? Color(255, 68, 110, 77) : Color(125, 119, 146, 99);
    art::sprig(g, 18, h - 20, 105, false, leafColor);
    art::sprig(g, w - 18, h - 20, 105, true, leafColor);
    art::sprig(g, 18, 150, 70, false, leafColor);
    art::sprig(g, w - 18, 150, 70, true, leafColor);
    art::cornerVine(g, 9, lobby ? 18.f : 104.f, 1, 1, 2);
    art::cornerVine(g, w - 9, lobby ? 18.f : 104.f, -1, 1, 2);
    art::cornerVine(g, 9, h - 9, 1, -1, 1.7f);
    art::cornerVine(g, w - 9, h - 9, -1, -1, 1.7f);
#endif
}

void forestBackground(HDC dc, ULONGLONG now) {
#ifdef _WIN32
    // Mist moves slowly: cache its large alpha surfaces independently of fast feedback.
    struct Cache {
        HDC dc = nullptr;
        HBITMAP bitmap = nullptr;
        HGDIOBJ previous = nullptr;
        int width = 0, height = 0;
        Page page = Page::Lobby;
        ULONGLONG tick = ~ULONGLONG(0);

        void clear() {
            if (dc && previous)
                SelectObject(dc, previous);
            if (bitmap)
                DeleteObject(bitmap);
            if (dc)
                DeleteDC(dc);
            dc = nullptr;
            bitmap = nullptr;
            previous = nullptr;
        }

        ~Cache() {
            clear();
        }
    };

    static Cache cache;
    int width = canvasWidth(), height = canvasHeight();
    if (!cache.dc || cache.width != width || cache.height != height) {
        cache.clear();
        cache.dc = CreateCompatibleDC(nullptr);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void *pixels = nullptr;
        cache.bitmap = CreateDIBSection(cache.dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if (!cache.dc || !cache.bitmap) {
            cache.clear();
            renderForestBackground(dc, now);
            return;
        }
        cache.previous = SelectObject(cache.dc, cache.bitmap);
        cache.width = width;
        cache.height = height;
        cache.tick = ~ULONGLONG(0);
    }
    ULONGLONG tick = now / 200;
    if (tick != cache.tick || s.page != cache.page) {
        renderForestBackground(cache.dc, now);
        cache.tick = tick;
        cache.page = s.page;
    }
    int saved = SaveDC(dc);
    SetStretchBltMode(dc, HALFTONE);
    SetBrushOrgEx(dc, 0, 0, nullptr);
    StretchBlt(dc, 0, 0, width, height, cache.dc, 0, 0, width, height, SRCCOPY);
    RestoreDC(dc, saved);
#endif
}

void panelOrnaments(HDC dc, RECT r, COLORREF color) {
#ifdef _WIN32
    if (r.right - r.left < 240 || r.bottom - r.top < 120)
        return;
    art::ready();
    art::Canvas canvas(dc);
    auto &g = *canvas.graphics;
    // Faint paper grain remains below the subsequently drawn text and controls.
    if (color == WHITE) {
        Gdiplus::LinearGradientBrush paper(
            Gdiplus::RectF(float(r.left + 6), float(r.top + 6), float(r.right - r.left - 12),
                           float(r.bottom - r.top - 12)),
            Gdiplus::Color(32, 230, 221, 177), Gdiplus::Color(0, 250, 248, 230), 90);
        g.FillRectangle(&paper, r.left + 6.f, r.top + 6.f, float(r.right - r.left - 12),
                        float(r.bottom - r.top - 12));
    }
    // Short insets leave the corners light instead of ruled paper or double boxes.
    art::line(g, r.left + 21.f, r.top + 5.f, r.right - 21.f, r.top + 5.f,
              Gdiplus::Color(75, 190, 176, 126));
    art::line(g, r.left + 21.f, r.bottom - 6.f, r.right - 21.f, r.bottom - 6.f,
              Gdiplus::Color(55, 190, 176, 126));
    art::sprig(g, float(r.right - 12), float(r.top + 39), 23, true,
               Gdiplus::Color(60, 106, 139, 95));
    art::cornerVine(g, r.left + 7.f, r.top + 7.f, 1, 1, .7f);
    art::cornerVine(g, r.right - 7.f, r.top + 7.f, -1, 1, .7f);
    art::cornerVine(g, r.left + 7.f, r.bottom - 7.f, 1, -1, .7f);
    art::cornerVine(g, r.right - 7.f, r.bottom - 7.f, -1, -1, .7f);
#endif
}

void buttonOrnaments(HDC dc, RECT r, bool primary, bool enabled) {
#ifdef _WIN32
    art::ready();
    art::Canvas canvas(dc);
    auto &g = *canvas.graphics;
    Gdiplus::Pen p(
        art::color(enabled ? (primary ? RGB(174, 158, 104) : RGB(220, 211, 177)) : LINE, 160), 1);
    art::line(g, r.left + 10.f, r.top + 3.f, r.right - 10.f, r.top + 3.f,
              art::color(primary ? RGB(222, 211, 161) : RGB(255, 255, 247), enabled ? 160 : 55));
    if (primary && enabled && r.right - r.left > 300 && r.bottom - r.top > 55)
        art::sprig(g, float(r.right - 15), float(r.bottom - 12), 22, true,
                   Gdiplus::Color(120, 192, 207, 155));

#endif
}

void forestCrest(HDC dc, RECT r) {
#ifdef _WIN32
    art::ready();
    art::Canvas canvas(dc);
    auto &g = *canvas.graphics;
    float x = (r.left + r.right) / 2.f, y = (r.top + r.bottom) / 2.f,
          side = float(r.right - r.left);
    Gdiplus::Pen p(Gdiplus::Color(190, 202, 183, 121), 1.4f);
    g.DrawEllipse(&p, float(r.left), float(r.top), side, side);
    g.DrawEllipse(&p, r.left + 5.f, r.top + 5.f, side - 10, side - 10);
    art::tree(g, x, y + side * .25f, side * .65f, side * .38f, Gdiplus::Color(255, 234, 235, 203));
    art::sprig(g, x - 5, y + side * .32f, side * .27f, true, Gdiplus::Color(200, 180, 199, 150));
    art::sprig(g, x + 5, y + side * .32f, side * .27f, false, Gdiplus::Color(200, 180, 199, 150));
#endif
}

// Each page keeps a clean logical frame; transitions never render old game state again.
#ifdef _WIN32
struct PageFrame {
    HDC dc = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ previous = nullptr;
    int width = 0, height = 0;
    Page page = Page::Lobby;

    ~PageFrame() {
        clear();
    }

    void clear() {
        if (dc && previous)
            SelectObject(dc, previous);
        if (bitmap)
            DeleteObject(bitmap);
        if (dc)
            DeleteDC(dc);
        dc = nullptr;
        bitmap = nullptr;
        previous = nullptr;
        width = height = 0;
    }

    bool allocate(int w, int h) {
        if (dc && width == w && height == h)
            return true;
        clear();
        dc = CreateCompatibleDC(nullptr);
        BITMAPINFO i{};
        i.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        i.bmiHeader.biWidth = w;
        i.bmiHeader.biHeight = -h;
        i.bmiHeader.biPlanes = 1;
        i.bmiHeader.biBitCount = 32;
        void *bits = nullptr;
        bitmap = CreateDIBSection(dc, &i, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!dc || !bitmap) {
            clear();
            return false;
        }
        previous = SelectObject(dc, bitmap);
        width = w;
        height = h;
        return true;
    }
};

struct PageFrames {
    PageFrame latest, outgoing;
};

PageFrames &pageFrames() {
    static PageFrames frames;
    return frames;
}
#endif
void beginPageTransition() {
#ifdef _WIN32
    auto &f = pageFrames();
    if (!f.latest.dc || f.latest.page != s.page) {
        f.outgoing.clear();
        return;
    }
    if (f.outgoing.allocate(f.latest.width, f.latest.height)) {
        BitBlt(f.outgoing.dc, 0, 0, f.latest.width, f.latest.height, f.latest.dc, 0, 0, SRCCOPY);
        f.outgoing.page = s.page;
    }
#endif
}

void rememberPageFrame(HDC dc) {
#ifdef _WIN32
    auto &f = pageFrames().latest;
    int w = canvasWidth(), h = canvasHeight();
    if (f.allocate(w, h)) {
        int saved = SaveDC(f.dc);
        SetStretchBltMode(f.dc, HALFTONE);
        SetBrushOrgEx(f.dc, 0, 0, nullptr);
        StretchBlt(f.dc, 0, 0, w, h, dc, 0, 0, w, h, SRCCOPY);
        RestoreDC(f.dc, saved);
        f.page = s.page;
    }
#endif
}

void pageCrossfade(HDC dc, ULONGLONG now) {
#ifdef _WIN32
    auto &old = pageFrames().outgoing;
    double pageT = now <= visual.introAt ? 0 : std::min(1., double(now - visual.introAt) / 300.);
    double opacity =
        visual.introBlocking || visual.introSkipped ? 0 : 1 - pageT * pageT * (3 - 2 * pageT);
    if (!old.dc || opacity <= 0)
        return;
    BLENDFUNCTION blend{AC_SRC_OVER, 0, BYTE(std::clamp(255 * opacity, 0., 255.)), 0};
    AlphaBlend(dc, 0, 0, canvasWidth(), canvasHeight(), old.dc, 0, 0, old.width, old.height, blend);
#endif
}

void introFog(HDC dc, ULONGLONG now) {
#ifdef _WIN32
    double opacity = introOpacity(now);
    if (opacity <= 0)
        return;
    art::ready();
    auto &clouds = forestScene::clouds();

    struct Surface {
        HDC dc = nullptr;
        HBITMAP bitmap = nullptr;
        HGDIOBJ previous = nullptr;
        BYTE *bits = nullptr;
        int width = 0, height = 0;

        void clear() {
            if (dc && previous)
                SelectObject(dc, previous);
            if (bitmap)
                DeleteObject(bitmap);
            if (dc)
                DeleteDC(dc);
            dc = nullptr;
            bitmap = nullptr;
            previous = nullptr;
            bits = nullptr;
        }

        ~Surface() {
            clear();
        }

        bool allocate(int w, int h) {
            if (dc && width == w && height == h)
                return true;
            clear();
            dc = CreateCompatibleDC(nullptr);
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = w;
            info.bmiHeader.biHeight = -h;
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, reinterpret_cast<void **>(&bits),
                                      nullptr, 0);
            if (!dc || !bitmap) {
                clear();
                return false;
            }
            previous = SelectObject(dc, bitmap);
            width = w;
            height = h;
            return true;
        }
    };

    static Surface surface;
    const int w = canvasWidth(), h = canvasHeight();
    if (!surface.allocate(384, std::max(1, 384 * h / w)))
        return;
    float t = float(
        std::clamp(double(now - std::min(now, visual.introAt)) / visual.introDuration, 0., 1.));
    float drift = t * t * t * (10 - 15 * t + 6 * t * t);
    double flow = double(now % 9000) / 9000 * 6.2831853;

    struct Bank {
        float x, y, w, h, blend, alpha;
    };

    std::array<Bank, 6> banks;
    for (int layer = 0; layer < 6; ++layer) {
        bool right = layer % 2;
        double phase = flow + layer * 1.7;
        float shift = drift * (right ? .43f : .32f) * (layer < 2 ? 1.f : .8f);
        banks[layer] = {(right ? .13f : -.23f) + (right ? shift : -shift) +
                            .025f * float(std::sin(phase * .73)),
                        -.17f + .09f * float(std::cos(phase * .57 + layer)),
                        layer < 2 ? .95f : .82f,
                        layer < 2 ? 1.35f : 1.1f,
                        float(.5 + .5 * std::sin(phase)),
                        (layer < 2 ? 1.f : .58f) *
                            (layer >= 4 ? float(std::pow(1 - t, 1.6)) : 1.f)};
    }
    auto sample = [](const std::vector<float> &field, float u, float v) {
        if (u <= 0 || v <= 0 || u >= 1 || v >= 1)
            return 0.f;
        float x = u * 383, y = v * 255;
        int ix = int(x), iy = int(y);
        float a = x - ix, b = y - iy;
        int i = iy * 384 + ix;
        return (field[i] * (1 - a) + field[i + 1] * a) * (1 - b) +
               (field[i + 384] * (1 - a) + field[i + 385] * a) * b;
    };
    // Blend precomputed density fields directly into one premultiplied surface.
    // This avoids twelve colour-matrix image resamples on every animation frame.
    for (int y = 0; y < surface.height; ++y) {
        auto row = surface.bits + y * surface.width * 4;
        float v = (y + .5f) / surface.height;
        for (int x = 0; x < surface.width; ++x) {
            float u = (x + .5f) / surface.width, remain = 1;
            for (int layer = 0; layer < 6; ++layer) {
                const auto &bank = banks[layer];
                float localU = (u - bank.x) / bank.w, localV = (v - bank.y) / bank.h;
                float a = sample(clouds.fields[layer], localU, localV);
                float b = sample(clouds.fields[(layer + 3) % 6], localU - .027f / bank.w,
                                 localV - .023f / bank.h);
                float density = (a * (1 - bank.blend) + b * bank.blend) * bank.alpha;
                remain *= 1 - density;
            }
            float curl = forestScene::noise(u * 9 + t * .7f, v * 7, 77) - .5f;
            float radius = forestScene::ease((t - .06f) / .94f) * .68f;
            radius += .055f * (1 - forestScene::ease((v - .30f) / .4f)) * forestScene::ease(t * 4);
            float keep =
                forestScene::ease((std::abs(u - .5f) + curl * .12f - radius + .10f) / .20f);
            float veil =
                (1 - forestScene::ease(t * 2.5f)) * (.38f + .20f * sample(clouds.fields[2], u, v));
            float density = 1 - remain * remain * (1 - veil);
            // Start fully veiled; reveal the centre only after the opening begins.
            keep = 1 - forestScene::ease(t * 5) * (1 - keep);
            float a =
                float(std::sqrt(opacity)) * (visual.introBlocking ? 1.f : .78f) * density * keep;
            row[x * 4] = BYTE(227 * a + .5f);
            row[x * 4 + 1] = BYTE(234 * a + .5f);
            row[x * 4 + 2] = BYTE(224 * a + .5f);
            row[x * 4 + 3] = BYTE(255 * a + .5f);
        }
    }
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    AlphaBlend(dc, 0, 0, w, h, surface.dc, 0, 0, surface.width, surface.height, blend);
#endif
}

void damageEdges(HDC dc, double opacity) {
#ifdef _WIN32
    if (opacity <= 0)
        return;
    art::ready();
    art::Canvas canvas(dc);
    auto &g = *canvas.graphics;
    float w = float(canvasWidth()), h = float(canvasHeight());
    Gdiplus::GraphicsPath edge;
    edge.AddRectangle(Gdiplus::RectF(0, 0, w, h));
    Gdiplus::PathGradientBrush vignette(&edge);
    vignette.SetCenterColor(Gdiplus::Color(0, 215, 99, 85));
    Gdiplus::Color colors[] = {Gdiplus::Color(BYTE(100 * opacity), 215, 99, 85)};
    int count = 1;
    vignette.SetSurroundColors(colors, &count);
    vignette.SetFocusScales(.73f, .73f);
    g.FillRectangle(&vignette, 0.f, 0.f, w, h);
#endif
}
