// Embedded painted forest and deterministic, purely decorative cloud textures.
#ifdef _WIN32
namespace forestScene {
using namespace Gdiplus;

Bitmap *painting() {
    art::ready();
    static std::unique_ptr<Bitmap> cached;
    static bool attempted = false;
    if (attempted)
        return cached.get();
    attempted = true;
    auto module = GetModuleHandleW(nullptr);
    auto resource = FindResourceW(module, MAKEINTRESOURCEW(2252), RT_RCDATA);
    if (!resource)
        return nullptr;
    auto size = SizeofResource(module, resource);
    auto bytes = LockResource(LoadResource(module, resource));
    auto storage = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!storage || !bytes)
        return nullptr;
    auto data = GlobalLock(storage);
    if (!data) {
        GlobalFree(storage);
        return nullptr;
    }
    std::memcpy(data, bytes, size);
    GlobalUnlock(storage);
    IStream *stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(storage, TRUE, &stream))) {
        GlobalFree(storage);
        return nullptr;
    }
    {
        Bitmap decoded(stream);
        if (decoded.GetLastStatus() == Ok)
            cached.reset(decoded.Clone(0, 0, decoded.GetWidth(), decoded.GetHeight(),
                                       PixelFormat32bppPARGB));
    }
    stream->Release();
    return cached.get();
}

float ease(float t) {
    t = std::clamp(t, 0.f, 1.f);
    return t * t * (3 - 2 * t);
}

float random(int x, int y, int seed) {
    unsigned n = unsigned(x) * 374761393u + unsigned(y) * 668265263u + unsigned(seed) * 1442695041u;
    n = (n ^ (n >> 13)) * 1274126177u;
    return ((n ^ (n >> 16)) & 65535u) / 65535.f;
}

float noise(float x, float y, int seed) {
    int ix = int(std::floor(x)), iy = int(std::floor(y));
    float a = ease(x - ix), b = ease(y - iy);
    float top = random(ix, iy, seed) * (1 - a) + random(ix + 1, iy, seed) * a;
    float bottom = random(ix, iy + 1, seed) * (1 - a) + random(ix + 1, iy + 1, seed) * a;
    return top * (1 - b) + bottom * b;
}

float billow(float x, float y, int seed) {
    return .52f * noise(x, y, seed) + .28f * noise(x * 2.07f, y * 2.07f, seed + 3) +
           .14f * noise(x * 4.13f, y * 4.13f, seed + 7) +
           .06f * noise(x * 8.3f, y * 8.3f, seed + 11);
}

struct Clouds {
    std::array<std::vector<float>, 6> fields;

    Clouds() {
        for (int seed = 0; seed < 6; ++seed) {
            fields[seed].resize(384 * 256);
            for (int y = 0; y < 256; ++y)
                for (int x = 0; x < 384; ++x) {
                    float u = (x + .5f) / 384, v = (y + .5f) / 256;
                    float warp = noise(u * 4, v * 4, seed + 41);
                    float n = billow(u * 4.8f + warp * .7f, v * 3.6f - warp * .6f, seed * 17 + 1);
                    float radius =
                        std::pow((u - .5f) / .53f, 2.f) + std::pow((v - .5f) / .55f, 2.f);
                    float edge = ease((1.18f - radius + (n - .5f) * .65f) / .55f);
                    float feather = ease(u / .17f) * ease((1 - u) / .17f) * ease(v / .17f) *
                                    ease((1 - v) / .17f);
                    fields[seed][y * 384 + x] = feather * (.98f * edge * ease((n - .25f) / .48f));
                }
        }
    }
};

Clouds &clouds() {
    static Clouds value;
    return value;
}

bool background(HDC dc, ULONGLONG now) {
    auto image = painting();
    if (!image)
        return false;
    art::Canvas canvas(dc);
    auto &g = *canvas.graphics;
    float w = float(canvasWidth()), h = float(canvasHeight());
    // The painting and page tint are static. Only the low floor haze changes with time.
    // Keep the expensive resample/gradient out of the 200ms background refresh.
    static std::unique_ptr<Bitmap> base;
    static int baseWidth = 0, baseHeight = 0, basePalette = -1;
    int palette = s.page == Page::Lobby ? 0 : s.page == Page::Config ? 1 : 2;
    if (!base || baseWidth != int(w) || baseHeight != int(h) || basePalette != palette) {
        base = std::make_unique<Bitmap>(int(w), int(h), PixelFormat32bppPARGB);
        Graphics paint(base.get());
        float sourceW = float(image->GetWidth()), sourceH = float(image->GetHeight());
        float scale = std::max(w / sourceW, h / sourceH);
        float cropW = w / scale, cropH = h / scale;
        paint.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        paint.SetSmoothingMode(SmoothingModeAntiAlias);
        paint.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        paint.DrawImage(image, RectF(0, 0, w, h), (sourceW - cropW) * .5f,
                        (sourceH - cropH) * .5f, cropW, cropH, UnitPixel);
        if (palette == 0) {
            // A restrained centre shade preserves the light title over the clearing.
            GraphicsPath path;
            path.AddEllipse(w * .18f, h * .04f, w * .64f, h * .53f);
            PathGradientBrush shade(&path);
            shade.SetCenterColor(Color(62, 10, 44, 34));
            Color edge(0, 10, 44, 34);
            int count = 1;
            shade.SetSurroundColors(&edge, &count);
            paint.FillPath(&shade, &path);
        } else if (palette == 2) {
            SolidBrush soften(Color(122, 232, 240, 226));
            paint.FillRectangle(&soften, 0.f, 0.f, w, h);
        }
        baseWidth = int(w);
        baseHeight = int(h);
        basePalette = palette;
    }
    g.DrawImage(base.get(), RectF(0, 0, w, h), 0, 0, w, h, UnitPixel);
    // A small translucent haze near the forest floor, never over page content.
    float drift = float(std::sin(double(now % 18000) / 18000 * 6.2831853));
    GraphicsPath path;
    path.AddEllipse(-w * .15f + w * .03f * drift, h * .78f, w * 1.3f, h * .35f);
    PathGradientBrush mist(&path);
    mist.SetCenterColor(Color(s.page == Page::Lobby ? 26 : 12, 221, 236, 219));
    Color edge(0, 221, 236, 219);
    int count = 1;
    mist.SetSurroundColors(&edge, &count);
    g.FillPath(&mist, &path);
    return true;
}
}
#endif
