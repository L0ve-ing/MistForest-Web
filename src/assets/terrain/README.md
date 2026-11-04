# 森林棋盘素材

v1.1.8 的草地、漩涡接缝及建筑调整见 [本版补充说明](forest-structures.md)。下文记录 v1.1.6 原始素材与当时实现。

`forest-water-atlas.png` 使用 Codex 内置 imagegen 生成，依据用户确认的《棋盘装饰与漩涡调整》设计图；未使用 CLI/API 回退。PNG 原始输出保留透明度，作为资源 2250 嵌入程序。

四个等大区域依次为：左上固定岸边、右上独立水流、左下湖泊、右下草丛／苔石／落叶／短枝。`ui/terrain_art.inc` 使用归一化源区域绘制，裁去图集外缘；水流在椭圆内部转动，岸石与植物保持固定。静态图块和透明度预先缓存，避免逐帧处理大图。简洁动画使用静态水面；只有可见角色能减弱背景对比。

角色继续使用原有像素图集，未用设计图中的近似角色替换。实际画面见 `../../../tests_v1_1_8/previews/`。

## 生成提示词（原文）

Use reference as EXACT visual style and palette reference for a production game terrain sprite atlas, NOT a mockup. Generate a square 1024x1024 transparent RGBA PNG with FOUR equal 512x512 cells in a precise 2x2 grid, no separators, no text, no numbers, no border, no paper background, no arrows, no characters. Hand-painted delicate botanical pixel-friendly illustration, same teal water and moss stones as reference. Top-left cell: ONLY the fixed ring-shaped shoreline of the reference whirlpool, irregular mossy grey rocks, reeds and tiny white flowers around a gently oval hole. Center MUST be completely transparent (no water) so animated water can be rendered behind. Pool opening should fit a horizontal oval centered at local (256,260), width 368 height 304. Shore outer silhouette fits local x30..482 y45..467; plants at corners. Top-right cell: ONLY whirlpool water disk, no rocks no plants. Teal turquoise outer water, spiraling curved WHITE foam streaks, small dark deep-blue funnel center. Horizontal oval centered at local (256,260), width 402 height 334. Outside disk transparent. Keep entire foam texture inside, strong reference resemblance. Bottom-left: entire quiet lake from reference, teal-blue water with tiny soft ripples, irregular shore mossy grey rocks and reeds and a few white flowers. Fits local x30..482 y45..467; transparent exterior. Bottom-right: four sparse botanical corner clusters arranged around a fully TRANSPARENT empty central region reserved for a character: largest grass with two grey rocks and tiny white flowers bottom-left, smaller grass with moss pebble top-right, tiny grass bottom-right, single golden fallen leaf left middle; very small twig at right middle. All clusters within their512cell, central x120..390 y100..430 entirely empty. No rectangular tiles, no gradients across transparency, no shadows outside objects. Atlas must preserve true alpha transparency. Match reference lovingly, lush but readable at100pixels. Separate each quadrant's objects within its own quadrant.

工具实际输出为 1254×1254；渲染使用比例坐标，不依赖提示词中的分辨率。

## 浅草地底纹（2026-11-01）

`grass-floor-atlas.png` 由内置 imagegen 根据用户确认的《浅草地底纹·棋盘示意》生成，保留 PNG 透明度，作为资源 2251 嵌入程序。实际尺寸 1774×887，按四列两行切分为八种细草、苔藓、落叶、疏草等普通地面外观；所有变体具有相同规则。草纹延伸到格内中央，采用低透明度叠加以保留移动、建筑和选中底色；坐标及名字区域通过平滑透明度遮罩避让。

八种底纹分别搭配不同位置、大小及疏密的边角植物。组合按公开格子坐标固定，不随行动时间或隐藏敌人的位置变化；缓存完整图层，避免逐帧处理大图。

漩涡对齐会在裁切和缩放后的嵌入素材中注册深蓝涡心，并使用与实际浅水底色相同的合成条件。水面开口与旋转轴共用中心。原生测试跟踪整圈十六个时点的涡心像素，容差折算到正常棋盘不足一像素，并检查 28 秒循环首尾一致。岸边继续固定。

生成提示词原文：

Production game texture atlas, based on the attached approved pale forest-board design. Output one wide 2048x1024 transparent RGBA PNG, exact4columns by2rows equalcells, no gaps no borders no labels no numbers no characters no UI no white background no checkerboard painted in. Eight distinct top-down PALE GRASS GROUND OVERLAYS for cream-colored tactical board cells, matching the reference's '建议' finegrass floor. Use muted light sage, pale olive, subtle yellow-green moss speckles, a few subdued ochre leaf flecks. Transparent negative space between separate brushstrokes; no opaque rectangular paint underneath. Fine tiny short grass strokes and delicate moss patches spread THROUGHOUT THE MIDDLE of each cell, not justcorners. Keep lowcontrast pastel natural watercolor/pixel-friendly detail, with~60%transparent space scattered across thefield. Verysmall transparent quietregion atupperleft forcoordinate, and lowercentraledge forrolename. Softpatches blendedtowardtheedges, everycell artisolatedinitsowncell. No giant grass tufts or bigrocks (these are added separately bygame). Composition row1 cellslefttoright:1 shortgrass diagonallyscattered withdensepatchlowerleft;2 patchesofroundedsubtlemoss with tiny pebblespecks mostlyupperright;3 shortgrasswith5smallgoldfallenleaves dispersed;4 sparsegrasswith largercreamtransparentgaps andfewpaleearthspeckles. Row2:5finegrass with2tinywhiteflowerclusters nearopposingedges;6thinangledgrasspatchesupperleft/lower right andsubtlemoss;7lowfuzzy mossfield withfewearthflecks andoneverysmalltwigoffcenter;8variedgrasssprigsandverysmallleaves scattered evenly. All8use SAME brightness and palette, same ordinaryterrain, no separate gameplayidentity. Groundmust feel lightly textured and alive while supporting darkcrisp sprites andnumbers on top. Keep texturereadableat100pixelcellwidth, avoidnoise and broadsolidgreenwash, no darkgreen densecarpet, no gradients fadingintoopaquebackground. Wholeimage aspect2:1 exact4x2 atlas with transparentbackground.
