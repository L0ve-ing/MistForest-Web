// The approved forest painting is embedded in four independent atlas quadrants:
// stationary shore, water only, quiet lake, and small ground plants.
#ifdef _WIN32
namespace terrainArt {
using namespace Gdiplus;

struct Asset {
    bool attempted = false;
    std::unique_ptr<Bitmap> image;
};

Asset &asset(int resourceId = 2250) {
    art::ready();
    static std::array<Asset, 3> cache;
    auto &cached = cache[resourceId == 2253 ? 2 : resourceId == 2251 ? 1 : 0];
    if (cached.attempted)
        return cached;
    cached.attempted = true;
    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (!resource)
        return cached;
    DWORD size = SizeofResource(module, resource);
    const void *bytes = LockResource(LoadResource(module, resource));
    if (!bytes || !size)
        return cached;
    HGLOBAL storage = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!storage)
        return cached;
    void *destination = GlobalLock(storage);
    if (!destination) {
        GlobalFree(storage);
        return cached;
    }
    std::memcpy(destination, bytes, size);
    GlobalUnlock(storage);
    IStream *stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(storage, TRUE, &stream))) {
        GlobalFree(storage);
        return cached;
    }
    {
        Bitmap decoded(stream);
        if (decoded.GetLastStatus() == Ok)
            cached.image.reset(decoded.Clone(0, 0, int(decoded.GetWidth()),
                                             int(decoded.GetHeight()), PixelFormat32bppARGB));
    }
    stream->Release();
    if (cached.image && cached.image->GetLastStatus() != Ok)
        cached.image.reset();
    return cached;
}

Bitmap *regionBitmap(int quadrant, RectF region, float opacity, int resourceId = 2250) {
    auto &image = asset(resourceId).image;
    if (!image)
        return nullptr;

    struct Patch {
        int resourceId, quadrant, alpha;
        RectF region;
        std::unique_ptr<Bitmap> image;
    };

    // Bake source cropping and alpha once. Applying a colour matrix to the
    // entire large PNG on every grass tuft made a full board needlessly slow.
    static std::vector<Patch> patches;
    int alpha = int(std::clamp(opacity, 0.f, 1.f) * 255 + .5f);
    for (auto &patch : patches)
        if (patch.resourceId == resourceId && patch.quadrant == quadrant && patch.alpha == alpha &&
            patch.region.X == region.X && patch.region.Y == region.Y &&
            patch.region.Width == region.Width && patch.region.Height == region.Height)
            return patch.image.get();
    const int columns = resourceId == 2251 || resourceId == 2253 ? 4 : 2;
    const float w = image->GetWidth() / float(columns), h = image->GetHeight() / 2.f;
    float scale = std::min(1.f, 256.f / std::max(region.Width * w, region.Height * h));
    int width = std::max(1, int(std::ceil(region.Width * w * scale))),
        height = std::max(1, int(std::ceil(region.Height * h * scale)));
    auto bitmap = std::make_unique<Bitmap>(width, height, PixelFormat32bppPARGB);
    ImageAttributes attributes;
    ColorMatrix matrix = {{{1, 0, 0, 0, 0},
                           {0, 1, 0, 0, 0},
                           {0, 0, 1, 0, 0},
                           {0, 0, 0, alpha / 255.f, 0},
                           {0, 0, 0, 0, 1}}};
    attributes.SetColorMatrix(&matrix);
    attributes.SetWrapMode(WrapModeClamp);
    {
        Graphics paint(bitmap.get());
        paint.Clear(Color(0, 0, 0, 0));
        paint.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        paint.DrawImage(image.get(), RectF(0, 0, float(width), float(height)),
                        (quadrant % columns + region.X) * w, (quadrant / columns + region.Y) * h,
                        region.Width * w, region.Height * h, UnitPixel, &attributes);
    }
    patches.push_back({resourceId, quadrant, alpha, region, std::move(bitmap)});
    return patches.back().image.get();
}

void drawRegion(Graphics &g, int quadrant, RectF region, RectF destination, float opacity,
                bool mirror = false, int resourceId = 2250) {
    auto image = regionBitmap(quadrant, region, opacity, resourceId);
    if (!image)
        return;
    auto saved = g.Save();
    if (mirror) {
        g.TranslateTransform(destination.X * 2 + destination.Width, 0);
        g.ScaleTransform(-1, 1);
    }
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.DrawImage(image, destination);
    g.Restore(saved);
}

void drawPatch(Graphics &g, int quadrant, RectF destination, float opacity, bool mirror = false) {
    // Exclude the atlas gutters, including antialias pixels along quadrant edges.
    drawRegion(g, quadrant, RectF(.02f, .02f, .96f, .96f), destination, opacity, mirror);
}

PointF waterEye() {
    // Register the visible funnel AFTER source cropping, resampling and alpha
    // baking. A geometric atlas centre makes an off-centre painted eye orbit.
    static const PointF eye = [] {
        auto image = regionBitmap(1, RectF(.015f, .18f, .97f, .77f), .96f);
        if (!image)
            return PointF(.5f, .5f);
        double xSum = 0, ySum = 0, count = 0;
        for (UINT y = 0; y < image->GetHeight(); ++y)
            for (UINT x = 0; x < image->GetWidth(); ++x) {
                Color color;
                image->GetPixel(x, y, &color);
                float alpha = color.GetA() / 255.f;
                // The texture is composited over the shallow-water fill, not
                // bare paper. Include that fill when registering visible ink.
                float r = color.GetR() * alpha + 77.4f * (1 - alpha),
                      green = color.GetG() * alpha + 177.24f * (1 - alpha),
                      b = color.GetB() * alpha + 189.72f * (1 - alpha);
                if (r < 65 && green < 130 && b > green + 20) {
                    xSum += x + .5;
                    ySum += y + .5;
                    ++count;
                }
            }
        return count ? PointF(float(xSum / count / image->GetWidth()),
                              float(ySum / count / image->GetHeight()))
                     : PointF(.5f, .5f);
    }();
    return eye;
}

const std::vector<PointF> &waterOpening() {
    // Trace the real transparent opening rather than imposing an ellipse on
    // an irregular painted shoreline. The half-pixel overlap sits UNDER the
    // stones, so antialiasing cannot expose a crescent of the tile background.
    static const std::vector<PointF> points = [] {
        std::vector<PointF> boundary;
        auto shore = regionBitmap(0, RectF(.02f, .02f, .96f, .96f), 1.f);
        for (int i = 0; i < 192; ++i) {
            double angle = i * 6.283185307179586 / 192;
            float dx = float(std::cos(angle)), dy = float(std::sin(angle));
            float radius = 40;
            if (shore) {
                for (float r = 2; r < 49; r += .2f) {
                    int x = int((50 + dx * r) * shore->GetWidth() / 100),
                        y = int((52 + dy * r) * shore->GetHeight() / 100);
                    if (x < 0 || y < 0 || x >= int(shore->GetWidth()) ||
                        y >= int(shore->GetHeight()))
                        break;
                    Color color;
                    shore->GetPixel(x, y, &color);
                    if (color.GetA() >= 80) {
                        radius = r + .8f;
                        break;
                    }
                }
            }
            boundary.emplace_back(50 + dx * radius, 52 + dy * radius);
        }
        return boundary;
    }();
    return points;
}

void quietTextAreas(Bitmap &image, bool occupied) {
    // Feather the alpha around text instead of cutting rectangular white holes
    // into the grass. PARGB channels must fade together with their alpha.
    auto ease = [](float x) {
        x = std::clamp(x, 0.f, 1.f);
        return x * x * (3 - 2 * x);
    };
    BitmapData pixels{};
    Rect bounds(0, 0, int(image.GetWidth()), int(image.GetHeight()));
    if (image.LockBits(&bounds, ImageLockModeRead | ImageLockModeWrite, PixelFormat32bppPARGB,
                       &pixels) != Ok)
        return;
    for (int y = 0; y < bounds.Height; ++y) {
        auto row = static_cast<BYTE *>(pixels.Scan0) + std::ptrdiff_t(y) * pixels.Stride;
        for (int x = 0; x < bounds.Width; ++x) {
            float u = (x + .5f) * 100 / bounds.Width, v = (y + .5f) * 100 / bounds.Height;
            float coordinate = (1 - ease((u - 27) / 10)) * (1 - ease((v - 9) / 10));
            float name = (1 - ease((std::abs(u - 50) - 16) / 10)) * ease((v - 84) / 10);
            float actor = occupied ? (1 - ease((std::abs(u - 50) - 13) / 18)) *
                                         ease((v - 20) / 12) * (1 - ease((v - 75) / 12))
                                   : 0;
            float alpha = 1 - std::max(.65f * std::max(coordinate, name), .4f * actor);
            for (int channel = 0; channel < 4; ++channel)
                row[x * 4 + channel] = BYTE(row[x * 4 + channel] * alpha + .5f);
        }
    }
    image.UnlockBits(&pixels);
}
} // namespace terrainArt
#endif

bool paintedTerrain(HDC dc, RECT cell, Terrain terrain, ULONGLONG now, bool occupied) {
#ifdef _WIN32
    using namespace Gdiplus;
    if (!terrainArt::asset().image)
        return false;
    const float side = float(std::min(cell.right - cell.left - 4, cell.bottom - cell.top - 22));
    if (side < 8)
        return true;
    art::Canvas canvas(dc);
    auto &g = *canvas.graphics;
    g.TranslateTransform((cell.left + cell.right - side) / 2.f, cell.top + 11.f);
    g.ScaleTransform(side / 100, side / 100);
    // Only known occupants reach this argument. Terrain never consults enemy positions.
    const float opacity = occupied ? .34f : .96f;
    if (terrain == Terrain::Lake) {
        terrainArt::drawPatch(g, 2, RectF(0, 0, 100, 100), opacity);
        if (s.fullAnimation) {
            float t = float(now % 5200) / 5200.f;
            Pen ripple(Color(BYTE(std::sin(t * 3.14159265f) * 100 * opacity), 240, 255, 251), .65f);
            g.DrawEllipse(&ripple, 48 - t * 14, 56 - t * 5, 5 + t * 28, 2 + t * 10);
        }
        return true;
    }
    // Compress a rotating circular texture AFTER rotation. The water stays an
    // ellipse at every angle, with no rocking outline or rotating stones.
    auto waterState = g.Save();
    GraphicsPath opening;
    const auto &boundary = terrainArt::waterOpening();
    opening.AddPolygon(boundary.data(), int(boundary.size()));
    g.SetClip(&opening, CombineModeIntersect);
    SolidBrush depth(Color(BYTE(255 * opacity), 70, 174, 187));
    g.FillPath(&depth, &opening);
    g.TranslateTransform(50, 52);
    g.ScaleTransform(1, .72f);
    g.RotateTransform(s.fullAnimation ? float(now % 28000) * (360.f / 28000) : 0.f);
    // Put the visible painted eye on the same pivot as the water opening.
    const auto eye = terrainArt::waterEye();
    const RectF source(.015f, .18f, .97f, .77f);
    const RectF destination(-86 * eye.X, -86 * eye.Y, 86, 86);
    terrainArt::drawRegion(g, 1, source, destination, opacity);
    g.Restore(waterState);
    terrainArt::drawPatch(g, 0, RectF(0, 0, 100, 100), opacity);
    return true;
#else
    return false;
#endif
}

int groundVariant(int cell, int columns) {
    // Public coordinates alone determine a stable layout. Adjacent cells use
    // different compositions on both square and rectangular maps.
    columns = std::max(1, columns);
    int row = (cell - 1) / columns, column = (cell - 1) % columns;
    return (column * 3 + row * 5 + row / 2 * 2) % 8;
}

void drawGroundDecoration(HDC dc, RECT cellRect, Terrain terrain, int cell, bool occupied,
                          int columns = 5) {
#ifdef _WIN32
    using namespace Gdiplus;
    if (terrain != Terrain::Ground || cellRect.right - cellRect.left < 40)
        return;
    // Eight paintings and eight distinct plant layouts are cached as complete
    // transparent layers, so frame cost stays independent of ornament count.
    terrainArt::asset();
    static std::array<std::unique_ptr<Bitmap>, 16> tiles;
    int variant = groundVariant(cell, columns);
    auto &tile = tiles[variant + (occupied ? 8 : 0)];
    if (!tile) {
        tile = std::make_unique<Bitmap>(128, 128, PixelFormat32bppPARGB);
        {
            Graphics g(tile.get());
            g.Clear(Color(0, 0, 0, 0));
            g.ScaleTransform(1.28f, 1.28f);
            g.SetClip(RectF(2, 2, 96, 96));
            // The source is already pale watercolor. Do not wash it out again
            // across an occupied tile; only the visible actor/text areas fade.
            terrainArt::drawRegion(g, variant, RectF(.03f, .04f, .94f, .92f), RectF(1, 1, 98, 98),
                                   .9f, false, 2251);
            g.SetClip(RectF(2, 17, 96, 80), CombineModeIntersect);
            const float opacity = .92f;
            // Position, scale and density vary together, rather than mirroring a
            // single bottom-left / top-right arrangement on every tile.
            static const RectF mainPlant[] = {{1, 71, 29, 26}, {73, 17, 25, 25}, {73, 71, 25, 26},
                                              {2, 82, 16, 14}, {1, 75, 23, 22},  {2, 18, 26, 25},
                                              {1, 43, 24, 24}, {76, 46, 22, 23}};
            static const RectF smallPlant[] = {{78, 19, 20, 18}, {3, 82, 17, 15},  {3, 20, 17, 16},
                                               {83, 24, 13, 12}, {82, 40, 16, 14}, {82, 81, 16, 14},
                                               {81, 78, 17, 16}, {2, 81, 20, 16}};
            terrainArt::drawRegion(
                g, 3, variant == 3 ? RectF(.69f, .66f, .27f, .25f) : RectF(0, .44f, .51f, .50f),
                mainPlant[variant], opacity, variant % 2 != 0);
            terrainArt::drawRegion(g, 3, RectF(.64f, 0, .35f, .30f), smallPlant[variant], opacity,
                                   variant % 3 == 0);
            if (variant == 2 || variant == 6)
                terrainArt::drawRegion(g, 3, RectF(.68f, .36f, .28f, .17f),
                                       RectF(77, variant == 2 ? 46.f : 20.f, 21, 13), opacity);
        }
        terrainArt::quietTextAreas(*tile, occupied);
    }
    art::Canvas canvas(dc);
    auto &g = *canvas.graphics;
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.DrawImage(tile.get(), RectF(float(cellRect.left), float(cellRect.top),
                                  float(cellRect.right - cellRect.left),
                                  float(cellRect.bottom - cellRect.top)));
#endif
}

bool paintedStructure(HDC dc, RECT slot, int frame, float opacity = 1) {
#ifdef _WIN32
    if (!terrainArt::asset(2253).image)
        return false;
    art::Canvas canvas(dc);
    auto &g = *canvas.graphics;
    terrainArt::drawRegion(g, frame, Gdiplus::RectF(.02f, .02f, .96f, .96f),
                           Gdiplus::RectF(float(slot.left), float(slot.top),
                                          float(slot.right - slot.left),
                                          float(slot.bottom - slot.top)),
                           opacity, false, 2253);
    return true;
#else
    return false;
#endif
}

void paintedBuilding(HDC dc, RECT cell, Structure kind, bool occupied) {
    int width = cell.right - cell.left, height = cell.bottom - cell.top;
    if (occupied) {
        // This branch receives only a visible unit. A low slab leaves its
        // silhouette intact; the small corner emblem still identifies type.
        paintedStructure(
            dc,
            rect(cell.left + 7, cell.bottom - height / 3, width - 14, std::max(8, height / 3 - 7)),
            7, .9f);
    } else {
        int frame = kind == Structure::Fort ? 2 : kind == Structure::GreatWall ? 3 : 1;
        paintedStructure(
            dc, rect(cell.left + 3, cell.top + 13, width - 6, std::max(8, height - 30)), frame);
    }
}

bool paintedLineWall(HDC dc, int x, int y, int length, bool horizontal) {
#ifdef _WIN32
    if (!terrainArt::asset(2253).image)
        return false;
    art::Canvas canvas(dc);
    auto &g = *canvas.graphics;
    // Keep the footprint a union of thin edge strips. In particular, no large
    // corner/end pillars should imply a different diagonal blocking rule.
    const float thickness = std::clamp(length * .065f, 3.f, 7.f);
    const auto destination =
        horizontal ? Gdiplus::RectF(float(x), y - thickness / 2, float(length), thickness)
                   : Gdiplus::RectF(x - thickness / 2, float(y), thickness, float(length));
    const auto source =
        horizontal ? Gdiplus::RectF(0, .43f, 1, .14f) : Gdiplus::RectF(.43f, 0, .14f, 1);
    // A stone-coloured backing joins both strips without pinholes even if the
    // source paint feathers at a tile end. It never extends beyond the strip.
    Gdiplus::SolidBrush mortar(Gdiplus::Color(255, 145, 151, 127));
    Gdiplus::Pen outline(Gdiplus::Color(210, 71, 88, 57), 1.5f);
    g.DrawRectangle(&outline, destination);
    g.FillRectangle(&mortar, destination);
    terrainArt::drawRegion(g, horizontal ? 5 : 6, source, destination, 1.f, false, 2253);
    return true;
#else
    return false;
#endif
}

void forestBoardFrame(HDC dc, RECT first, RECT last) {
#ifdef _WIN32
    using namespace Gdiplus;
    art::Canvas canvas(dc);
    auto &g = *canvas.graphics;
    // The board's narrow outer gutter carries foliage; no ornament covers a
    // cell, wall, hit area, coordinate or panel heading.
    for (int side = 0; side < 2; ++side) {
        auto saved = g.Save();
        float x = side ? float(last.right + 3) : float(first.left - 18);
        g.SetClip(RectF(x, float(first.top + 8), 15, float(last.bottom - first.top - 16)),
                  CombineModeIntersect);
        for (int y = first.top + 18; y < last.bottom - 16; y += 108) {
            terrainArt::drawRegion(g, 3, RectF(0, .44f, .51f, .50f), RectF(x - 5, float(y), 25, 38),
                                   .7f, side != 0);
            art::cornerVine(g, x + 6, float(y + 44), side ? -1.f : 1.f, 1, .8f);
        }
        g.Restore(saved);
    }
#endif
}
