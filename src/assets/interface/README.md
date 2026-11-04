# 界面素材与动画

`forest-clearing.png` 使用内置 imagegen 根据用户确认的大厅设计生成，未使用 CLI/API 回退；保留原始 PNG。作为资源 2252 内置，程序从任意工作目录启动均可使用。无文字/控件，所有按钮、命中区域和页面文字由原生界面绘制。

画面以原青绿、米白、淡金为主色；大厅/配置保留完整森林，其余页使用较浅的覆盖色。背景尺寸变化使用居中等比裁切并缓存；不读取游戏或敌人位置。

开场约2.2秒，素材准备完成后才计时，六层噪声云具有不同速度和位移，中间先显露，边缘逐渐散开。普通页面散雾700毫秒、内容交叉淡入300毫秒。点击可跳过开场；页面过渡不阻挡首个输入。

原始生成提示词（内置工具）：

Production background asset for Windows game 雾隐之森. Match ONLY the LEFT LARGE forest lobby view in the reference: beautiful painted 2D emerald/sage woodland, natural irregular arching leafy trees framing both sides, pale distant forest depth, softly dappled daylight, a little warm cream clearing light, moss stones and small white flowers/ferns along foreground edges, quiet path into central clearing. Output ONLY FOREST environment. NO title/logo/text/controls/paper panels/frames/borders/storyboard/UI/sample sheet. Wide landscape 16:10 highresolution. Central55% softer, calm and slightly shaded for superimposed cream title and ivory menu panel. Rich natural foreground greens, airy hazy trees in distance. Faithful sophisticated hand-painted style, not photorealistic or gothic, not repeating triangular trees. No characters/animals/new game items. No thick foreground fog, moving fog will be rendered in game.
