// Original transparent PNGs are embedded in the EXE; no relative asset path is used.
// Battlefield animation is handled independently in character_sprites.inc.
#ifdef _WIN32
namespace portraitArt {
using namespace Gdiplus;

struct Asset {
    bool attempted = false;
    std::unique_ptr<Bitmap> image;
    RectF body, face;
};

int slot(Role role, int form) {
    return role == Role::HiddenWolf && !form ? 13 : int(role);
}

Asset &asset(Role role, int form) {
    art::ready();
    static std::array<Asset, 14> cache;
    int index = slot(role, form);
    if (index < 0 || index >= int(cache.size()))
        index = 0;
    auto &out = cache[index];
    if (out.attempted)
        return out;
    out.attempted = true;
    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(2100 + index), RT_RCDATA);
    if (!resource)
        return out;
    DWORD size = SizeofResource(module, resource);
    const void *bytes = LockResource(LoadResource(module, resource));
    if (!bytes || !size)
        return out;
    HGLOBAL storage = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!storage)
        return out;
    void *destination = GlobalLock(storage);
    if (!destination) {
        GlobalFree(storage);
        return out;
    }
    std::memcpy(destination, bytes, size);
    GlobalUnlock(storage);
    IStream *stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(storage, TRUE, &stream))) {
        GlobalFree(storage);
        return out;
    }
    {
        Bitmap decoded(stream);
        if (decoded.GetLastStatus() == Ok)
            out.image.reset(decoded.Clone(0, 0, int(decoded.GetWidth()), int(decoded.GetHeight()),
                                          PixelFormat32bppARGB));
    }
    stream->Release();
    if (!out.image || out.image->GetLastStatus() != Ok) {
        out.image.reset();
        return out;
    }
    int w = int(out.image->GetWidth()), h = int(out.image->GetHeight());
    int left = w, top = h, right = 0, bottom = 0;
    BitmapData data{};
    Rect whole(0, 0, w, h);
    if (out.image->LockBits(&whole, ImageLockModeRead, PixelFormat32bppARGB, &data) == Ok) {
        for (int y = 0; y < h; ++y) {
            auto row = static_cast<const BYTE *>(data.Scan0) + std::ptrdiff_t(y) * data.Stride;
            for (int x = 0; x < w; ++x)
                if (row[x * 4 + 3] > 12) {
                    left = std::min(left, x);
                    right = std::max(right, x + 1);
                    top = std::min(top, y);
                    bottom = std::max(bottom, y + 1);
                }
        }
        out.image->UnlockBits(&data);
    }
    if (right <= left || bottom <= top) {
        left = 0;
        top = 0;
        right = w;
        bottom = h;
    }
    out.body = RectF(float(left), float(top), float(right - left), float(bottom - top));

    struct Focus {
        float x, y, width;
    };

    static constexpr Focus focus[] = {{.44f, .24f, .43f}, {.71f, .43f, .52f}, {.39f, .26f, .59f},
                                      {.44f, .25f, .57f}, {.52f, .24f, .48f}, {.55f, .27f, .58f},
                                      {.46f, .25f, .53f}, {.54f, .27f, .62f}, {.50f, .26f, .56f},
                                      {.51f, .28f, .58f}, {.50f, .31f, .91f}, {.52f, .25f, .58f},
                                      {.50f, .28f, .58f}, {.54f, .25f, .56f}};
    auto f = focus[index];
    float side = out.body.Width * f.width;
    float x = out.body.X + out.body.Width * f.x - side / 2,
          y = out.body.Y + out.body.Height * f.y - side / 2;
    out.face = RectF(std::clamp(x, 0.f, std::max(0.f, w - side)),
                     std::clamp(y, 0.f, std::max(0.f, h - side)), side, side);
    return out;
}

void image(Graphics &g, Asset &portrait, RectF destination, bool face) {
    RectF source = face ? portrait.face : portrait.body;
    if (!face) {
        float scale =
            std::min(destination.Width / source.Width, destination.Height / source.Height);
        float w = source.Width * scale, h = source.Height * scale;
        destination = RectF(destination.X + (destination.Width - w) / 2,
                            destination.Y + destination.Height - h, w, h);
    }
    auto saved = g.Save();
    g.SetInterpolationMode(InterpolationModeNearestNeighbor);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    ImageAttributes attributes;
    attributes.SetWrapMode(WrapModeClamp);
    g.DrawImage(portrait.image.get(), destination, source.X, source.Y, source.Width, source.Height,
                UnitPixel, &attributes);
    g.Restore(saved);
}
} // namespace portraitArt
#endif

bool hasCharacterPortrait(Role role, int form = 1) {
#ifdef _WIN32
    return portraitArt::asset(role, form).image != nullptr;
#else
    return false;
#endif
}

void characterAvatar(HDC dc, RECT r, Role role, int form = 1, bool own = true,
                     bool selected = false) {
#ifdef _WIN32
    using namespace Gdiplus;
    auto &portrait = portraitArt::asset(role, form);
    if (portrait.image) {
        art::Canvas canvas(dc);
        auto &g = *canvas.graphics;
        float side = float(std::min(r.right - r.left, r.bottom - r.top));
        float x = r.left + (r.right - r.left - side) / 2.f,
              y = r.top + (r.bottom - r.top - side) / 2.f;
        art::ellipse(g, x, y, side, side,
                     art::color(selected ? GOLD
                                : own    ? RGB(169, 194, 174)
                                         : CORAL));
        art::ellipse(g, x + 2, y + 2, side - 4, side - 4, art::color(PALE));
        auto saved = g.Save();
        GraphicsPath clip;
        clip.AddEllipse(x + 3, y + 3, side - 6, side - 6);
        g.SetClip(&clip, CombineModeIntersect);
        portraitArt::image(g, portrait, RectF(x + 3, y + 3, side - 6, side - 6), true);
        g.Restore(saved);
        return;
    }
#endif
    emblem(dc, r, role, own, selected);
}

void characterIllustration(HDC dc, RECT r, Role role, int form = 1) {
#ifdef _WIN32
    using namespace Gdiplus;
    auto &portrait = portraitArt::asset(role, form);
    if (portrait.image) {
        art::Canvas canvas(dc);
        auto &g = *canvas.graphics;
        art::ellipse(g, r.left + (r.right - r.left) * .17f, r.bottom - 10.f,
                     (r.right - r.left) * .66f, 8, art::color(LINE, 140));
        portraitArt::image(g, portrait,
                           RectF(float(r.left), float(r.top), float(r.right - r.left),
                                 float(r.bottom - r.top - 5)),
                           false);
        return;
    }
#endif
    emblem(dc, r, role, true, true);
}

std::wstring portraitSkillText(Role role) {
    auto description = roleHelp(role);
    auto start = description.find(L"\n\n");
    return start == std::wstring::npos ? description : description.substr(start + 2);
}
