// Fixed, registered cells keep each attack's position stable across its keyframes.
constexpr int EffectRows[] = {3, 4, 4, 8, 8, 8, 8};
// Unequal transparent row gutters are measured once, shared by all four frames.
// This preserves registration without pulling a neighbouring skill into a tile.
constexpr int EffectRowCuts[][9] = {{0, 208, 418, 617, 840, 1060, 1299, 1511, 1774},
                                    {0, 222, 457, 672, 889, 1107, 1328, 1552, 1774},
                                    {0, 225, 450, 693, 927, 1129, 1330, 1551, 1774},
                                    {0, 263, 503, 739, 935, 1143, 1345, 1574, 1774}};

struct EffectImages {
    bool attempted = false;
    std::unique_ptr<Bitmap> atlas;
    std::array<std::unique_ptr<Bitmap>, 32> frames;
};

EffectImages *effectImages(int group) {
    static std::array<EffectImages, std::size(EffectRows)> groups;
    return group >= 0 && group < int(groups.size()) ? &groups[group] : nullptr;
}

Bitmap *effectAtlas(int group = 0) {
    auto *images = effectImages(group);
    if (!images)
        return nullptr;
    auto &atlas = images->atlas;
    if (images->attempted)
        return atlas.get();
    images->attempted = true;
    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(2200 + group), RT_RCDATA);
    if (!resource)
        return nullptr;
    DWORD size = SizeofResource(module, resource);
    const void *bytes = LockResource(LoadResource(module, resource));
    if (!bytes || !size)
        return nullptr;
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!memory)
        return nullptr;
    void *destination = GlobalLock(memory);
    if (!destination) {
        GlobalFree(memory);
        return nullptr;
    }
    std::memcpy(destination, bytes, size);
    GlobalUnlock(memory);
    IStream *stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &stream))) {
        GlobalFree(memory);
        return nullptr;
    }
    {
        Bitmap decoded(stream);
        if (decoded.GetLastStatus() == Ok && decoded.GetWidth() >= 128 &&
            decoded.GetHeight() >= 128 &&
            ((group >= 3 && decoded.GetWidth() == 887 && decoded.GetHeight() == 1774) ||
             (group < 3 && std::abs(decoded.GetWidth() / 4.f -
                                    decoded.GetHeight() / float(EffectRows[group])) < 1)))
            atlas.reset(decoded.Clone(0, 0, int(decoded.GetWidth()), int(decoded.GetHeight()),
                                      PixelFormat32bppARGB));
    }
    stream->Release();
    if (atlas && atlas->GetLastStatus() != Ok)
        atlas.reset();
    return atlas.get();
}

bool paintedEffect(Graphics &g, int row, float t, float alpha, int group = 0) {
    auto *images = effectImages(group);
    auto *atlas = effectAtlas(group);
    if (!atlas || row < 0 || row >= EffectRows[group])
        return false; // Procedural artwork remains a safe resource-load fallback.
    float f = group == 0 && row == 2 ? ease(t / .70f) * 3 : clamp(t / .65f) * 3;
    int first = std::min(3, int(f)), next = std::min(3, first + 1);
    float blend = ease(f - first);
    auto &frames = images->frames;
    auto saved = g.Save();
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    // A small local impulse adds motion without ever drawing an origin-to-target projectile.
    float shift = group == 0 && row == 1 ? 4 * (1 - ease(t / .45f)) : 0;
    RectF destination(3 - shift, 3 + shift, 94, 94);
    for (int pass = 0; pass < 2; ++pass) {
        float opacity = alpha * (pass ? blend : 1 - blend);
        if (opacity <= 0)
            continue;
        ColorMatrix matrix = {{{1, 0, 0, 0, 0},
                               {0, 1, 0, 0, 0},
                               {0, 0, 1, 0, 0},
                               {0, 0, 0, opacity, 0},
                               {0, 0, 0, 0, 1}}};
        ImageAttributes attributes;
        attributes.SetColorMatrix(&matrix);
        attributes.SetWrapMode(WrapModeTileFlipXY);
        int index = row * 4 + (pass ? next : first);
        if (!frames[index]) {
            // Shared integer boundaries partition even atlases not divisible by four.
            int left = (index % 4) * atlas->GetWidth() / 4,
                right = (index % 4 + 1) * atlas->GetWidth() / 4,
                top = row * atlas->GetHeight() / EffectRows[group],
                bottom = (row + 1) * atlas->GetHeight() / EffectRows[group];
            if (group >= 3) {
                top = EffectRowCuts[group - 3][row];
                bottom = EffectRowCuts[group - 3][row + 1];
            }
            frames[index].reset(
                atlas->Clone(left, top, right - left, bottom - top, PixelFormat32bppARGB));
            // Feather the packing gutter once in the cached frame. Stray pixels from
            // neighbouring artwork disappear without a hard rectangular crop of the blade.
            if (group > 0 && frames[index]) {
                Rect bounds(0, 0, right - left, bottom - top);
                BitmapData data{};
                if (frames[index]->LockBits(&bounds, ImageLockModeRead | ImageLockModeWrite,
                                            PixelFormat32bppARGB, &data) == Ok) {
                    for (int y = 0; y < bounds.Height; ++y) {
                        auto *pixels = static_cast<BYTE *>(data.Scan0) + y * data.Stride;
                        for (int x = 0; x < bounds.Width; ++x) {
                            int edge =
                                std::min({x, y, bounds.Width - 1 - x, bounds.Height - 1 - y});
                            auto &a = pixels[x * 4 + 3];
                            a = a < 4 ? 0 : BYTE(a * ease((edge - 4) / 10.f));
                        }
                    }
                    frames[index]->UnlockBits(&data);
                }
            }
        }
        if (frames[index] && frames[index]->GetLastStatus() == Ok)
            g.DrawImage(frames[index].get(), destination, 0.f, 0.f,
                        float(frames[index]->GetWidth()), float(frames[index]->GetHeight()),
                        UnitPixel, &attributes);
    }
    g.Restore(saved);
    return true;
}

int paintedRoleIndex(FxStyle style, Role role) {
    switch (style) {
    case FxStyle::ShadowCut:
        return 0;
    case FxStyle::CrimsonClaw:
        return 1;
    case FxStyle::Claw:
        return 2;
    case FxStyle::Bite:
        return 3;
    case FxStyle::Snipe:
        return 4;
    case FxStyle::Shotgun:
        return 5;
    case FxStyle::Fire:
        return 6;
    case FxStyle::Charge:
        return role == Role::Heavy ? 7 : -1;
    default:
        return -1;
    }
}

using mist::presentation::EffectAsset;
using mist::presentation::effectSlot;

bool paintedEffect(Graphics &g, EffectAsset asset, float t, float alpha) {
    auto slot = effectSlot(asset);
    return slot.group >= 0 && paintedEffect(g, slot.row, t, alpha, slot.group);
}

bool paintedFeedback(Graphics &g, EffectAsset asset, float t, float alpha) {
    auto saved = g.Save();
    float scale = asset == EffectAsset::ShieldHit ? .65f : .45f;
    g.TranslateTransform(50 * (1 - scale), 55 * (1 - scale));
    g.ScaleTransform(scale, scale);
    bool drawn = paintedEffect(g, asset, t, alpha);
    g.Restore(saved);
    return drawn;
}

EffectAsset paintedSkillAsset(const SkillFx &fx) {
    switch (fx.style) {
    case FxStyle::Arrow:
        return EffectAsset::Arrow;
    case FxStyle::Thrust:
        return fx.role == Role::WallGuard ? EffectAsset::GuardThrust : EffectAsset::ArmigerThrust;
    case FxStyle::Charge:
        return fx.role == Role::Knight ? EffectAsset::KnightCharge : EffectAsset::None;
    case FxStyle::Silence:
        return EffectAsset::Silence;
    case FxStyle::Blood:
        return EffectAsset::Blood;
    case FxStyle::SwordThrow:
        return EffectAsset::SwordThrow;
    case FxStyle::SwordRecall:
        return EffectAsset::SwordRecall;
    case FxStyle::Advance:
        return EffectAsset::SwordLanding;
    case FxStyle::RoseBloom:
        return EffectAsset::RoseBloom;
    case FxStyle::ArmorUp:
        return EffectAsset::ArmorUp;
    case FxStyle::ArmorDown:
        return EffectAsset::ArmorDown;
    case FxStyle::Dash:
        return fx.role == Role::Heavy    ? EffectAsset::HeavyDash
               : fx.role == Role::Shadow ? EffectAsset::ShadowDash
                                         : EffectAsset::InkDash;
    case FxStyle::ShadowLink:
        return EffectAsset::ShadowLink;
    case FxStyle::Transform:
        return fx.form ? EffectAsset::WolfForm : EffectAsset::HumanForm;
    case FxStyle::Jump:
        return fx.kind == Kind::Retreat ? EffectAsset::Retreat : EffectAsset::Leap;
    case FxStyle::FireStep:
        return EffectAsset::FireStep;
    case FxStyle::Burn:
        return EffectAsset::Burn;
    case FxStyle::Build:
        return EffectAsset::Build;
    case FxStyle::Demolish:
        return EffectAsset::Demolish;
    default:
        return EffectAsset::None;
    }
}

bool paintedRoleAttack(Graphics &g, const SkillFx &fx, int cell, float t, float alpha) {
    int index = paintedRoleIndex(fx.style, fx.role);
    if (index >= 0)
        return paintedEffect(g, mist::presentation::RoleEffectAssets[index], t, alpha);
    // A rose losing petals does not turn its own cell into a thorn attack.
    if (fx.style == FxStyle::RoseBloom && !fx.attacks[cell])
        return true;
    return paintedEffect(g, paintedSkillAsset(fx), t, alpha);
}
