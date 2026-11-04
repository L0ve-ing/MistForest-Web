// Pixel atlases are embedded, decoded once, and sampled with nearest-neighbour filtering.
#ifdef _WIN32
namespace characterArt {
struct Asset {
    bool attempted = false;
    std::unique_ptr<Gdiplus::Bitmap> image;
};

Asset &asset(int resourceId) {
    // Register GDI+ shutdown before the bitmap cache, so cached images die first.
    art::ready();
    static std::array<Asset, 42> cache;
    auto &out = cache[std::clamp(resourceId - 2400, 0, int(cache.size()) - 1)];
    if (out.attempted)
        return out;
    out.attempted = true;
    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (!resource)
        return out;
    DWORD size = SizeofResource(module, resource);
    auto bytes = LockResource(LoadResource(module, resource));
    if (!bytes || !size)
        return out;
    HGLOBAL storage = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!storage)
        return out;
    void *data = GlobalLock(storage);
    if (!data) {
        GlobalFree(storage);
        return out;
    }
    std::memcpy(data, bytes, size);
    GlobalUnlock(storage);
    IStream *stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(storage, TRUE, &stream))) {
        GlobalFree(storage);
        return out;
    }
    {
        Gdiplus::Bitmap decoded(stream);
        if (decoded.GetLastStatus() == Gdiplus::Ok)
            out.image.reset(decoded.Clone(0, 0, int(decoded.GetWidth()), int(decoded.GetHeight()),
                                          PixelFormat32bppARGB));
    }
    stream->Release();
    if (out.image && out.image->GetLastStatus() != Gdiplus::Ok)
        out.image.reset();
    return out;
}
} // namespace characterArt
#endif

int characterSlot(Role role, int form) {
    return std::clamp(role == Role::HiddenWolf && !form ? 13 : int(role), 0, 13);
}

bool characterSprite(HDC dc, RECT destination, const Unit &unit, int actorId, bool walking,
                     int horizontalDirection = 0, mist::presentation::sprites::Pose pose = {}) {
#ifdef _WIN32
    using namespace Gdiplus;
    namespace sprites = mist::presentation::sprites;
    int slot = characterSlot(unit.role, unit.form);
    const auto &set = sprites::Clips[slot];
    auto frame = s.actorSprites.sample(actorId, slot, walking, set,
                                       horizontalDirection ? horizontalDirection : pose.facing);
    if (!s.fullAnimation || pose.lean != 0 || pose.flash != 0 || pose.opacity < 1) {
        frame.animation =
            walking && s.fullAnimation ? sprites::Animation::Walk : sprites::Animation::Idle;
        frame.index = 0;
    }
    const auto &clip = set[int(frame.animation)];
    auto &asset = characterArt::asset(clip.resource);
    if (!asset.image || asset.image->GetWidth() != UINT(clip.frames * 128) ||
        asset.image->GetHeight() != 128)
        return false;
    const auto &bounds = sprites::BoundsByActor[slot];
    // Union bounds include EVERY frame of all three clips. Never crop each frame separately.
    int width = 2 * std::max(64 - bounds.left, bounds.right - 64);
    float scale = std::min(float(destination.right - destination.left) / std::max(1, width),
                           float(destination.bottom - destination.top) /
                               std::max(1, bounds.bottom - bounds.top));
    if (scale <= 0)
        return false;
    float x = (destination.left + destination.right) / 2.f - 64 * scale;
    float baseline = destination.bottom - (bounds.bottom - 120) * scale;
    float y = baseline - clip.groundY * scale;
    art::Canvas canvas(dc);
    auto &g = *canvas.graphics;
    float bodyHeight = float(destination.bottom - destination.top);
    float center = (destination.left + destination.right) / 2.f;
    int direction = frame.horizontalDirection   ? frame.horizontalDirection
                    : unit.role == Role::Shadow ? -1
                                                : 1;
    g.TranslateTransform(center + float(pose.recoil * bodyHeight),
                         baseline - float(pose.lift * bodyHeight));
    g.RotateTransform(float(pose.lean * direction * 45));
    g.TranslateTransform(-center, -baseline);
    g.SetInterpolationMode(InterpolationModeNearestNeighbor);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    // Shadow's supplied native pose faces left; the other supplied side poses face right.
    int nativeDirection = unit.role == Role::Shadow ? -1 : 1;
    if (frame.horizontalDirection && frame.horizontalDirection != nativeDirection) {
        g.TranslateTransform(2 * x + 128 * scale, 0);
        g.ScaleTransform(-1, 1);
    }
    ImageAttributes attributes;
    attributes.SetWrapMode(WrapModeClamp);
    float flash = float(pose.flash), alpha = float(std::clamp(pose.opacity, 0., 1.));
    ColorMatrix colorMatrix = {{{1 - flash, 0, 0, 0, 0},
                                {0, 1 - flash, 0, 0, 0},
                                {0, 0, 1 - flash, 0, 0},
                                {0, 0, 0, alpha, 0},
                                {flash, flash * .45f, flash * .3f, 0, 1}}};
    attributes.SetColorMatrix(&colorMatrix);
    g.DrawImage(asset.image.get(), RectF(x, y, 128 * scale, 128 * scale), float(frame.index * 128),
                0.f, 128.f, 128.f, UnitPixel, &attributes);
    return true;
#else
    (void)dc;
    (void)destination;
    (void)unit;
    (void)actorId;
    (void)walking;
    (void)horizontalDirection;
    return false;
#endif
}
