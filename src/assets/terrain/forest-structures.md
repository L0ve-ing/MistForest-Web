## v1.1.8 棋盘与建筑

草纹恢复自然色与较高不透明度，只对坐标、名字及可见己方角色所在区域局部避让。漩涡按固定岸圈的实际内缘覆盖水面，岸边最后盖住水纹接缝；不再依赖偏小的椭圆洞口。

`forest-structures-atlas.png` 使用内置 imagegen 根据确认的建筑图生成，未用 CLI/API 回退，保留 RGBA 透明度。实际 1774×887，资源2253，四列两行：方墙、单格城墙、堡垒、守卫军长城、废墟、横向边墙、竖向边墙、低石台。线形墙沿格线连续，连接处不额外增加圆柱/方墩。各建筑仍按原有公开位置和规则显示，玫瑰和飞剑保留原符号。

生成提示词原文：

Production transparent sprite atlas for 雾隐之森 matching approved design reference exactly in shape, palette and painted2D slightly elevated top/front view. NOT a design sheet. True transparent RGBA PNG, FOUR COLUMNS TWO ROWS eight equal squarecells, prefer2048x1024, NO borders/gutters/text/numbers/paper/grass tile backgrounds. Isolated objects fit eachcell localx.08..92 y.08..92.
ROW1 lefttoright:0 solid square mossyGREY stoneblock mass 方墙 with filledtop no door/courtyard;1 ONE warmSANDSTONE low crenellated barricade 城墙, horizontal front with separate visiblebase, not pair/edgewall;2 ONE BLUEGREY compact square crenellated towerkeep 堡垒 smallclosedwooddoor greenpennant;3 ONE GREYGREEN broadlow battlementplatform 长城 with smalltimberarrowturret/greenpennant rightrear andopenflatstoneplatform. Allsinglecell. Match reference art faithfully.
ROW2 lefttoright:4 LOW rubblefoundation with fracturedlowmasonry andfewgrey stones 废墟, nofire/skulls;5 veryslender seamless straight HORIZONTAL masonryedgewallstrip acrossFULLwidth x0..1 in verticalcenter y.43..57 ONLY, allotherarea transparent, topdownstone, constantthickness,nopillars/posts;6 matching VERTICALstrip acrossFULLheight y0..1 at x.43..57 ONLY, constantthickness,noposts;7 simpleLOW GREYGREEN squarestoneplatform/slab thinedging sparsemoss opentopsurface noheight/turret/door, forbelowstandingcharacter.
Readable at100pxcell. Sparse smallflowers/ferns only bybase, no oversizedplants or glow. No white matte/checkerboard/background painted in, no label/grid/watermark. Truealpha transparent outside object.
