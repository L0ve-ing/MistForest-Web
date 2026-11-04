# 已确认方向的特效素材

2026-10-18运行资源做无损PNG重编码：七张图集减少1,832,299字节，尺寸和每个RGBA像素不变。
下文保留生成与初始导入历史。manifest中的source_sha256记录重编码前来源，sha256对应当前内嵌资源；复现工具为仓库根tools/compress-effects.py。

2026-10-17补齐其余三张已确认设计：使用内置imagegen技能生成以下四张透明RGBA生产图集，原PNG逐字节复制进项目。参考图保存在references/full-*-approved.png；完整最终提示词分别为full-core-prompt.txt、full-growth-prompt.txt、full-motion-prompt.txt、full-world-prompt.txt。本次没有使用CLI或网络素材。

| 新素材 | 资源 | 八行依次对应 |
| --- | --- | --- |
| full-core-keyframes.png | 2203 | 弩箭、玄甲长枪、守卫军长枪、骑士倾轧、沉默、嗜血、飞剑、召回 |
| full-growth-keyframes.png | 2204 | 长生剑落点、荆棘、凋落、散瓣、负甲、卸甲、白墨冲刺、随影移动 |
| full-motion-keyframes.png | 2205 | 附骨、狼形变身、人形变身、跳跃、回撤、私人推进、火步、燃烧 |
| full-world-keyframes.png | 2206 | 建造、拆除、护盾、生命、命中、湖泊、漩涡、余烬 |

四图均887×1774、4列×8行；四列为起势/展开/峰值/收束。生成图的行间透明留白不等距，程序使用经过实际画面验收的固定行边界（四列共用），不会逐帧按不透明区域裁剪；缓存边缘柔化，避免相邻技能杂边和动画跳位。真实边界及SHA256记录在manifest.json，切图逻辑在ui/skill_fx_sprite.inc。反馈缩小显示，移动不混用攻击；水面采用缓慢往返混合与漩涡旋转，循环衔接平滑。

原有2200—2202及11组效果保留。全新总览为版本目录full-vfx-preview.png，full-1-preview.gif至full-4-preview.gif由本版程序实际绘制的42帧PNG编码，非概念图动画。生成素材与提示词存在细节差异；实际视觉以原生样片为准。所有素材内嵌EXE。

工具：内置 image_gen（imagegen技能），2026-10-17；基于本聊天已确认的关键帧概念图重新生成，非网络素材。原始输出PNG按字节复制为[approved-keyframes.png](approved-keyframes.png)，1448×1086、透明RGBA、4列×3行，单格362×362。

SHA256：`9f2d729dd1a3d582d459b8c70d110fb5d816c977a9bfa49f29a88ba098b0cb12`。

本次追加八组：内置image_gen根据已确认的两张设计图与角色立绘生成透明RGBA素材，PNG原字节保留。每张1254×1254、4列×4行，四列分别为起势/展开/峰值/消散。程序以共享整数边界切成313或314px的注册帧，缓存时柔化边缘杂点，避免相邻图帧漏色与生硬切边；均内嵌EXE，无外置文件依赖。

| 素材 | 资源 | 四行依次对应 | 最终生成提示词 |
| --- | --- | --- | --- |
| [role-melee-keyframes.png](role-melee-keyframes.png) | 2201 | 随影、阿尔法、隐狼狼形、座狼 | [全文](role-melee-prompt.txt) |
| [role-ranged-keyframes.png](role-ranged-keyframes.png) | 2202 | 烬、白墨、祝融喷火、重骑冲锋 | [全文](role-ranged-prompt.txt) |

两个PNG的SHA256依次为9674ddcf6c5127d9b19baea316c9e2178a88785b2ad02640f4906a513cfbaded、c9130d6695e1e5ff5ab4e0d990dd6a7037e82510506955156d6548fa7af8141e。来源清单见[manifest.json](manifest.json)。各角色的其他技能仍单独选择表现，祝融保留原名称与火焰机制。

程序资源2200内嵌，运行时固定裁格、缓存、混合关键帧及局部位移，实战/回放共用。玫瑰收尾混入既有图标；无资源时使用原生图形回退。不同图帧是美术阶段，不是新增攻击次数，不改变命中、伤害或隐藏信息。

生成最终PNG时使用的提示词全文：

```text
Use case: stylized-concept. Production GAME VFX SPRITE SHEET, genuine transparent background, absolutely NO text, NO captions, NO borders, NO grid lines, NO UI, NO backdrop. Use the supplied concept sheet ONLY as visual reference for the actual effects and their texture, remove all interface.
CRITICAL TECHNICAL COMPOSITION: exact regular 4 COLUMN x 3 ROW grid of equal square sprite cells. Output image aspect ratio 4:3. Each row is a different effect, FOUR KEYFRAMES from left to right. Every effect fits completely within central 76% of its own invisible cell, with generous transparent gutters, no spill into neighbouring cell. Cell centres and scale are consistent, effects remain centered. Do not add any separator or annotations. Transparent pixels outside the effects. No white/black/green background.
Row1 X slash: cell1 one sharply tapered swept silver/gold crescent from lower left to upper right, DARK graphite back edge, luminous narrow cream cutting edge with textured broken trails; cell2 opposing crescent begins, first crescent still clearly visible; cell3 full powerful angular X of two CURVED blade energy ribbons, small brilliant white-gold center flash, 5 sparse tiny dark-gold fragments, irregular broken pixel trails; cell4 pale fragmented X afterimages, no solid blade, mostly transparent. Preserve reference row1's tapered silhouette and scratched pixel-textured highlights, NOT plain smooth vector symbols, NOT physical crossed swords, NOT perfect geometric lines.
Row2 sword energy: cell1 one SLENDER clearly recognizable sword with thin white-cyan blade, a tiny gold crossguard and dark teal hilt, tilted tip to upper right, light local blue mist; cell2 strong cyan-white directional energy curl around that sword, broken tapered arcs; cell3 a sharp diagonal cyan energy strike with the same slender sword core, white cutting highlight and rich dark teal shadow, energy flowing lower left to upper right, tiny gold accent, NOT a perfect circular ring; cell4 short scattered blue ice-like streaks trailing along the same diagonal, no solid sword. Follow reference row2 peak's irregular dynamic blade energy, NOT a bow arrow, no triangular arrowhead, no new character.
Row3 rose planting: cell1 tightly furled CRIMSON rose BUD, short stem with two green sepals and two small leaves; cell2 the SAME bud's outer petals half unfold with tightly curled center; cell3 fully open layered deep-red ROSE with intricate overlapping curled velvety petals and clear spiral centre, short dark-green stem and leaves, 2 detached petals and 2 small warm gold sparks; cell4 the SAME rose fully open, compact but same central scale, detached petals fade, clean persistent rose ready to resolve to icon. Match reference row3, NOT a flat daisy, NOT a symmetric rosette/flat disk, NOT a spiral lollipop.
Art style: refined hand-painted PIXEL ART VFX compatible with the referenced chibi portraits, crisp pixel-stepped edges, rich limited palettes, strong dark colored edge contrast for display against an ivory game tile. Controlled luminous core, very restrained halo, layered texture and irregular trailing broken pixels. NOT smooth vector art, not simple flat icons, no thick cartoon outlines, no photorealism. This is a usable sprite sheet not a mockup.
```

实际生成结果与提示词细节并非逐项相同；选择此图是为了实现用户认可的刀光、剑气与花瓣质感。程序不会把四幅美术阶段当成四次游戏行动。视觉验收使用版本目录中的native-vfx-preview.gif，非本素材表本身。
