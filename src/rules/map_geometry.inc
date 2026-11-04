int size(Mode mode) {
    int code = int(mode);
    return code == 3 ? 3 : code == 5 ? 5 : code / 10;
} // 编码的十位为地图宽度，兼容旧默认模式。

int rows(Mode mode) {
    int code = int(mode);
    return code == 3 ? 3 : code == 5 ? 5 : code % 10;
} // 个位为地图高度。

int cellCount(Mode mode) {
    return size(mode) * rows(mode);
} // 长方形格数不能使用宽度平方。

Mode mapMode(const Map &map) {
    return map.width == 5 && map.height == 5 ? Mode::Heroes : Mode(map.width * 10 + map.height);
} // 自定义地图尺寸与职业模式同时保存。

bool allied(bool teams, int a, int b) {
    return a == b || (teams && a % 2 == b % 2);
} // 二对二固定玩家一三同队、二四同队，交替行动。

bool heroMode(Mode mode) {
    return mode != Mode::Scout;
} // 除基础侦察外均为职业对战。

bool validMode(int mode) {
    return mode == 3 || mode == 5 ||
           (mode >= 11 && mode <= 99 && mode / 10 >= 1 && mode / 10 <= 9 && mode % 10 >= 1 &&
            mode % 10 <= 9);
} // 每个尺寸范围一至九。

bool validRole(int role) {
    return role >= 0 && role < 13;
} // 十三个职业。

Map defaultMap(Mode mode) {
    Map map;
    map.width = size(mode);
    map.height = rows(mode);
    if (mode == Mode::Heroes)
        for (int c : {1, 5, 13, 21, 25})
            map.cells[c - 1] = Terrain::Wall;
    return map;
} // 默认职业地图保留原有五个禁入格。

bool validMap(Mode mode, const Map &map) {
    if (!validMode(int(mode)) || map.width < 1 || map.width > 9 || map.height < 1 || map.height > 9)
        return false;
    int total = map.width * map.height, standable = 0;
    for (int c = 0; c < 81; ++c) {
        int value = int(map.cells[c]);
        if (value < 0 || value > 3)
            return false;
        if (c < total && (value == 0 || value == 3))
            ++standable;
        if (c >= total && value)
            return false;
    }
    for (int i = 0; i < 90; ++i)
        if ((i >= (map.height + 1) * map.width && map.horizontal[i]) ||
            (i >= map.height * (map.width + 1) && map.vertical[i]))
            return false;
    return standable > 0;
} // 有界校验，地图至少有一个可站立格。

std::string encodeMap(const Map &map) {
    std::string out = std::to_string(map.width) + std::to_string(map.height);
    for (auto c : map.cells)
        out += char('0' + int(c));
    for (bool x : map.horizontal)
        out += x ? '1' : '0';
    for (bool x : map.vertical)
        out += x ? '1' : '0';
    return out;
} // 尺寸和最多八十一格一起编码。

bool decodeMap(Mode mode, const std::string &text, Map &map) {
    Map next;
    int cells = 81, edges = 90, offset = 2;
    if (text.size() == 85) {
        cells = 25;
        edges = 30;
        offset = 0;
    } else if (text.size() == 263) {
        next.width = text[0] - '0';
        next.height = text[1] - '0';
    } else
        return false;
    for (int i = 0; i < cells; ++i) {
        char c = text[offset + i];
        if (c < '0' || c > '3')
            return false;
        next.cells[i] = Terrain(c - '0');
    }
    for (int i = 0; i < edges; ++i) {
        char h = text[offset + cells + i], v = text[offset + cells + edges + i];
        if ((h != '0' && h != '1') || (v != '0' && v != '1'))
            return false;
        next.horizontal[i] = h == '1';
        next.vertical[i] = v == '1';
    }
    if (!validMap(mode, next))
        return false;
    map = next;
    return true;
} // 兼容旧五乘五地图文件。

bool valid(Mode mode, int cell, const Map &map) {
    return validMode(int(mode)) && cell >= 1 && cell <= cellCount(mode) &&
           (map.cells[cell - 1] == Terrain::Ground || map.cells[cell - 1] == Terrain::Whirlpool);
} // 地面和漩涡可以落地。

bool valid(Mode mode, int cell) {
    return valid(mode, cell, defaultMap(mode));
} // 默认地图兼容入口。

int distance(Mode mode, int a, int b) {
    int n = size(mode);
    return std::abs((a - 1) / n - (b - 1) / n) + std::abs((a - 1) % n - (b - 1) % n);
} // 攻击仍使用非环绕曼哈顿距离。

bool canMove(Mode mode, int from, int to, const Map &map) {
    if (!valid(mode, from, map) || !valid(mode, to, map) || distance(mode, from, to) > 1)
        return false;
    if (from == to)
        return true;
    int n = size(mode), r = (from - 1) / n, c = (from - 1) % n, tr = (to - 1) / n,
        tc = (to - 1) % n;
    return r == tr ? !map.vertical[r * (n + 1) + std::max(c, tc)]
                   : !map.horizontal[std::max(r, tr) * n + c];
} // 边墙索引随地图宽度变化。

bool segmentBox(
    double sx, double sy, double ex, double ey, double left, double top, double right,
    double
        bottom) { // 只有穿入墙体内部才遮挡，擦边、碰角及碰孤立线墙端点允许通过；相连端点另行检查。
    constexpr double epsilon = 1e-9; // 格中心坐标为整数，使用极小容差区分边界与内部。
    if (left == right || top == bottom) {
        bool vertical = left == right;
        double delta = vertical ? ex - sx : ey - sy;
        if (std::abs(delta) < epsilon)
            return false;
        double t = ((vertical ? left : top) - (vertical ? sx : sy)) / delta;
        double along = vertical ? sy + t * (ey - sy) : sx + t * (ex - sx);
        return t > epsilon && t < 1 - epsilon && along > (vertical ? top : left) + epsilon &&
               along < (vertical ? bottom : right) - epsilon;
    } // 线墙只有在内部被横穿才挡子弹，平行擦边和孤立端点交会不挡，相连端点由clearShot统一遮挡。
    left += epsilon;
    top += epsilon;
    right -= epsilon;
    bottom -= epsilon; // 方墙缩去无厚度边界，以检测是否真正进入内部。
    double start[2] = {sx, sy}, end[2] = {ex, ey}, lower[2] = {left, top},
           upper[2] = {right, bottom}, lo = 0, hi = 1; // 交点参数初始范围为整条线段。
    for (int axis = 0; axis < 2; ++axis) {
        double delta = end[axis] - start[axis]; // 逐轴缩小交点的参数范围。
        if (delta == 0) {
            if (start[axis] < lower[axis] || start[axis] > upper[axis])
                return false;
        } // 平行且在障碍范围外则没有交点。
        else {
            double x = (lower[axis] - start[axis]) / delta, y = (upper[axis] - start[axis]) / delta;
            if (x > y)
                std::swap(x, y);
            lo = std::max(lo, x);
            hi = std::min(hi, y);
            if (lo > hi + 1e-12)
                return false;
        } // 包含边界和端点，因此擦边或碰角也被遮挡。
    } // 两轴仍有公共参数说明相交。
    return true; // 命中墙体的内部、边缘或端点。
} // 结束通用几何相交计算。

bool clearShot(Mode mode, int a, int b,
               const Map &map) { // 从起点格中心连到目标格中心，检查全部墙体。
    int n = size(mode);
    if (!valid(mode, a, map) || b < 1 || b > cellCount(mode) || map.cells[b - 1] == Terrain::Wall)
        return false; // 湖泊可以成为攻击目标，方墙不能。
    double sx = (a - 1) % n, sy = (a - 1) / n, ex = (b - 1) % n,
           ey = (b - 1) / n; // 格子中心采用零起点横纵坐标。
    for (int cell = 1; cell <= cellCount(mode); ++cell)
        if (map.cells[cell - 1] == Terrain::Wall) {
            double x = (cell - 1) % n, y = (cell - 1) / n;
            if (segmentBox(sx, sy, ex, ey, x - 0.5, y - 0.5, x + 0.5, y + 0.5))
                return false;
        } // 只检查方墙，湖泊不会遮挡视线。
    for (int i = 0; i < 90; ++i) {
        if (map.horizontal[i]) {
            double x = i % n, y = i / n - 0.5;
            if (segmentBox(sx, sy, ex, ey, x - 0.5, y, x + 0.5, y))
                return false;
        }
        if (map.vertical[i]) {
            double x = i % (n + 1) - 0.5, y = i / (n + 1);
            if (segmentBox(sx, sy, ex, ey, x, y - 0.5, x, y + 0.5))
                return false;
        }
    } // 每条边墙按一格长封闭线段检测。
    // A joint blocks a ray only when its wall arms straddle the ray. Grazing
    // the outside of an L is legal; passing between its arms is not. Straight,
    // T and cross joints still have no diagonal hole at their shared vertex.
    if (std::abs(ex - sx) > 1e-9 && std::abs(ey - sy) > 1e-9)
        for (int row = 0; row <= rows(mode); ++row)
            for (int col = 0; col <= n; ++col) {
                double dx = ex - sx, dy = ey - sy;
                bool positive = false, negative = false;
                auto arm = [&](double cross) {
                    positive = positive || cross > 1e-9;
                    negative = negative || cross < -1e-9;
                };
                if (col > 0 && map.horizontal[row * n + col - 1])
                    arm(dy); // Left.
                if (col < n && map.horizontal[row * n + col])
                    arm(-dy); // Right.
                if (row > 0 && map.vertical[(row - 1) * (n + 1) + col])
                    arm(-dx); // Up.
                if (row < rows(mode) && map.vertical[row * (n + 1) + col])
                    arm(dx); // Down.
                if (!positive || !negative)
                    continue;
                double x = col - .5, y = row - .5, t = (x - sx) / (ex - sx);
                if (t > 1e-9 && t < 1 - 1e-9 && std::abs(sy + t * (ey - sy) - y) < 1e-9)
                    return false;
            }
    return true; // 起点和目标之间没有任何不透明障碍。
} // 结束攻击遮挡检查。

bool clearShot(Mode mode, int a, int b) {
    return clearShot(mode, a, b, defaultMap(mode));
} // 兼容默认地图的攻击检查。
