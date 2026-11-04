#pragma once // 头文件只载入一次。
#include "version.hpp"
#include <array>  // 固定容量保存最多八名角色。
#include <vector> // 保存动作和复盘记录。
#include <string> // 中文界面文字。
#include <random> // 电脑仅使用受限视图选择动作。

namespace mist {                                        // 游戏规则独立于窗口和网络。
enum class Mode { Scout = 3, Heroes = 5 };              // 保留三格侦察与五格职业地图。
enum class Terrain { Ground, Wall, Lake, Whirlpool };   // 四种可编辑地形。
enum class BotStyle { Balanced, Aggressive, Cautious }; // 人机风格仅改变受限视角下的行动偏好。
enum class Role {
    Sniper,
    Wolf,
    Ink,
    Shadow,
    Heavy,
    HiddenWolf,
    Knight,
    Alpha,
    Armiger,
    Flamer,
    WallGuard,
    Prince,
    Sword
}; // 十三个职业。
enum class Kind {
    Move,
    Attack,
    Row,
    Col,
    Dash,
    Follow,
    Lead,
    Charge,
    Transform,
    Leap,
    Hunt,
    Silence,
    Cleave,
    CleaveRush,
    Retreat,
    BuildFort,
    BuildWall,
    AttackRush,
    Blood,
    ArmorUp,
    ArmorDown,
    Thrust,
    Flame,
    FireStep,
    FlameAdvance,
    Demolish,
    Surrender,
    Share,
    ShadowMode,
    PlantRose,
    Bloom,
    SwordStab,
    ThrowSword,
    RecallSword,
    SwordAdvance,
    End
}; // 一次提交只执行一个动作，结束回合单独提交。

struct Map {
    int width = 5, height = 5;
    std::array<Terrain, 81> cells{};
    std::array<bool, 90> horizontal{}, vertical{};
}; // 地图编码沿用旧版文件。
enum class Structure { None, Fort, Wall, GreatWall }; // 建筑与地图固有墙体分开，建筑可以摧毁。

struct Building {
    Structure kind = Structure::None;
    int owner = -1;
    double health = 0, directDamage = 0;
}; // 城墙每格独立拥有一点生命。

struct Fire {
    int owner = -1, expires = 0;
};

struct Rose {
    int unit = -1, cell = 0;
    double health = 3;
    int id = 0;
    double directDamage = 0;
}; // 玫瑰生命1.5，内部数值乘二。

struct Sword {
    int unit = -1, cell = 0, spent = 0;
}; // 飞剑永久驻场，直到主人主动召回。 // 火焰按双方完成的回合数确定熄灭时刻。

using Formations = std::array<std::array<int, 3>, 8>; // 各角色的三士兵初始位置，非守卫军保持零。
using Mask = std::array<bool, 81>;                    // 辅助推理只展示保守候选，不表示概率。

struct Unit { // 一个单位的完整状态，传给对手前清理私人字段。
    Role role = Role::Sniper;
    int position = 0, form = 0, cooldown = 0;
    double armor = 0;
    double temporaryHealth = 0;    // 嗜血溢出血量，在己方下一回合开始移除剩余部分。
    double health = 2, shield = 0; // 生命与护盾使用半点为单位的浮点值，支持0.6生命及平摊。
    std::array<int, 3> soldiers{};
    std::array<double, 3> soldierHealth{};
    int ammo = 0, armorLock = 0;
    bool ammoBlocked = false; // 守卫军每名士兵独立生命，祝融弹药和负甲锁定。
    std::array<int, 3> optionCooldown{};
    bool vulnerable = false;
    int controlSource = -1, controlExpires = 0;
    std::array<bool, 3> soldierVulnerable{};
    std::array<int, 3> soldierControlSource{{-1, -1, -1}}, soldierControlExpires{};
    bool silenced = false, guard = false, whirlImmune = false; // 沉默保护和漩涡免疫独立计时。
    std::array<bool, 3> soldierSilenced{}, soldierGuard{},
        soldierWhirl{}; // 控制与漩涡免疫逐个士兵结算，不能扩散到范围外。
    int follow = -1, followPiece = 0, sense = -2, swordQi = 9;
    std::wstring report; // 指定跟随目标和本人报告；距离负二表示未探测，负一表示无可探测目标。
    double directDamage = 0;
    std::array<double, 3>
        soldierDirectDamage{}; // 两次己方回合开始之间的直接受击强度；移动不清空，分担不修改其他士兵记录。
}; // 单位状态结束。

struct Turn { // 只记录当前所选单位已经执行的动作，不允许撤销。
    int active = -1, moves = 0, attacks = 0, used = 0, phase = 0, dr = 0, dc = 0, lastOption = 0,
        options = 0, anchor = 0; // 重骑阶段一为普通移动、二为冲锋；骑士阶段三为强化游猎。
    int squadMask = 0, squadDone = 0;
    Mask hitCells{}, flameCells{};
    bool fireStep = false, advance = false,
         flameChoice = false;             // 存活士兵与已行动士兵分别记录，同格攻击仍只结算一次。
    int shadowMode = 0, extraAttacks = 0; // 回剑提供的通用额外行动，可用于移动或普攻。
    bool blood = false, swordThrown = false;
    Mask swordCells{};
    std::vector<int> roseUsed;
    bool swordAdvance = false, swordBaseUsed = false;
    int pellets = 0;
    std::array<double, 24> inkDamage{};
    bool led = false, followed = false, transformed = false, dashed = false, dashLast = false,
         instinct = false;        // 免费技能也必须遵守次数和先后限制。
    std::vector<int> chargeCells; // 冲锋结束时才按坐标顺序公开判定格。
}; // 当前回合资源结束。

struct Action {
    int unit = 0;
    Kind kind = Kind::Move;
    int target = 0, subject = -1, piece = 0, extra = 0, amount = 1, landing = 0;
}; // 目标是内部格号；附骨目标另用单位编号。

enum class PublicEffect { None, Charge, Hit, Kill };

struct PublicCue {
    std::size_t textOffset = 0;
    PublicEffect effect = PublicEffect::None;
    bool absorbed = false;
};

struct Event {
    int player = 0, unit = 0, turn = 0;
    Action action;
    Mask attackCells{};
    std::wstring feedback;
    std::wstring publicText, privateText;
    std::array<std::wstring, 4> playerNotes{};
    std::vector<PublicCue> publicCues;
}; // 公共结果显示受伤和阵亡兵种；私人位移不进入公共文字。

struct Notice {
    int revision = 0, turn = 0, player = 0, weight = 0;
    std::wstring text;
    Mask attacks{};
    int unit = -1;
    bool feedback = false;
    Kind kind = Kind::Attack;
    PublicEffect effect = PublicEffect::None;
    bool absorbed = false;
}; // 播放节奏依据真实动作类型，所有信息仍来自脱敏事件。

struct Reaction {
    int unit = -1, piece = 0, mask = 0;
    std::array<int, 3> health{};
}; // 受击时由防守方选择承担者，候选信息仅对本人公开。

struct View { // 同一玩家可见双方职业与生命，但仅看见己方位置和私人状态。
    std::array<double, 81> roundDamage{};
    int roundNumber = 1,
        roundMask = 0; // 本轮各格攻击强度仅用于公共反馈；伤害上限由受击对象独立记录。
    Reaction reaction;
    Mode mode = Mode::Heroes;
    Map map;
    std::array<Building, 81> buildings{};
    std::array<Fire, 81> fires{};
    std::array<int, 81> rubble{};
    std::vector<Rose> roses;
    std::vector<Sword> swords;
    Mask intel{};
    std::array<Unit, 8> units{};
    int playerCount = 2;
    bool teams = false;
    std::array<bool, 4> eliminated{};
    std::array<int, 4> playerTurns{};
    int teamSize = 2, player = 0, turn = 0, winner = -1, turns = 0, revision = 0;
    Turn budget;
    std::array<Mask, 8> possible{}; // 敌方位置、报告和私密预算在视图构造时清除。
    Mask enemyAttacks{};
    int enemyAttackTurn = -1;
    std::wstring skillFeedback;  // 只包含已公开攻击格和本人技能反馈。
    std::vector<Notice> notices; // 整局各行动回合的安全提示，供界面按时间顺序播放。
    std::wstring result, turnReport, previousReport; // 最新公开判定，供放大的结果面板显示。
}; // 受限视图结束。

struct Snapshot {
    std::array<double, 81> roundDamage{};
    int roundNumber = 1;
    Reaction reaction;
    Mode mode = Mode::Heroes;
    Map map;
    std::array<Building, 81> buildings{};
    std::array<Fire, 81> fires{};
    std::array<int, 81> rubble{};
    std::vector<Rose> roses;
    std::vector<Sword> swords;
    std::array<Mask, 4> intel{};
    std::array<Unit, 8> units{};
    int playerCount = 2;
    bool teams = false;
    std::array<bool, 4> eliminated{};
    std::array<int, 4> playerTurns{};
    int roundMask = 0;
    int teamSize = 2, turn = 0, winner = -1, turns = 0, revision = 0;
    Turn budget;
    std::array<std::array<Mask, 8>, 4> possible{};
}; // 全知快照仅供裁判与赛后复盘。

int size(Mode mode);
int rows(Mode mode);
int cellCount(Mode mode);
Mode mapMode(const Map &map);
bool allied(bool teams, int a, int b);
bool heroMode(Mode mode);
bool validMode(int mode);
bool validRole(int role); // 模式与职业输入检查。
double directAttackDelta(double prior,
                         double power); // 对象当前受击周期新增直接攻击强度，先于护甲及形态减伤。
double initialHealth(Role role, Mode mode); // 内部使用两倍生命：普通1.5、重骑2、绝影1、士兵0.8。
std::vector<int> swordLine(const View &view, int from, int direction, bool includeSelf);
Map randomMap(int width, int height, unsigned seed);
Map defaultMap(Mode mode);
bool validMap(Mode mode, const Map &map);
std::string encodeMap(const Map &map);
bool decodeMap(Mode mode, const std::string &text, Map &map); // 兼容地图文件格式。
bool valid(Mode mode, int cell, const Map &map);
bool valid(Mode mode, int cell);
int distance(Mode mode, int a, int b); // 站位与曼哈顿距离。
bool canMove(Mode mode, int from, int to, const Map &map);
bool clearShot(Mode mode, int from, int to, const Map &map);
bool clearShot(Mode mode, int from, int to); // 墙边不挡子弹，跨墙仍挡移动。
std::vector<int> dashPath(Mode mode, int from, int to, const Map &map);
std::vector<int> dashPath(const View &view, int unit, int target);
bool canLeap(Mode mode, int from, int to, const Map &map); // 冲刺两步与日字三段路径。
std::wstring coordinate(Mode mode, int cell), halfPoints(double value), roleName(Role role),
    skillName(Role role), roleStats(Role role), roleHelp(Role role),
    roleTips(Role role); // 全界面统一坐标、技能名及生命显示。
bool silenceArea(Mode mode, int origin, int target,
                 const Map &map); // 沉默只作用于中心及上下左右距离一格，可以穿墙。
bool validFormation(
    Mode mode, const Map &map, int anchor,
    const std::array<int, 3> &cells); // 自由布阵只检查合法站位及首个坐标，不强制连通。
std::array<int, 3>
guardSpawn(Mode mode, int anchor,
           const Map &map); // 从起点生成三个上下左右相连的初始士兵，无法布阵返回零。
int soldierCell(const Unit &unit, int piece);
int connectedSquad(Mode mode, const Unit &unit, int piece); // 士兵选择和当前连通分组。
bool buildReach(const View &view, int from, int to);
bool buildSite(const View &view, int from, int to); // 建造与拆除不跨墙，目标公开条件提前判定。
bool canEnter(const View &view, int unit, int cell);
bool worldMove(const View &view, int unit, int from, int to);
bool worldShot(const View &view, int from, int to); // 地形与动态建筑的统一阻挡。
int inputPlayer(const View &view);
int inputPlayer(const Snapshot &state);
bool guardWall(const Building &building); // 防守选择不改变正常回合归属。
int inkPelletLimit(const Turn &turn);     // 白墨每回合三发，冲刺不改变弹数。
int attackRange(Mode mode, const Unit &unit);
bool immune(const Unit &unit); // 隐狼人形的探测和附骨免疫。
bool validate(const View &view, const Action &action,
              std::wstring *why = nullptr); // 合法性只依据玩家能够知道的信息。
bool pendingFollowup(const View &view);     // 攻击反馈后必须完成一次秘密落点选择。
bool hasAction(const View &view, int unit); // 包含免费技能，只排除主动结束，避免提前吞掉剩余行动。
std::vector<Action> choices(const View &view, int unit);
Action chooseAction(const View &view, std::mt19937 &rng,
                    BotStyle style = BotStyle::Balanced); // 人机不读取敌方位置。

class Game { // 最多四玩家、八角色的裁判，统一处理本地、联机和复盘。
  public:    // 仅通过合法动作改变局面。
    Game(std::array<int, 8> positions, std::array<Role, 8> roles, Mode mode = Mode::Heroes,
         int first = 0, Map map = defaultMap(Mode::Heroes), int teamSize = 2,
         Formations formations = {}, int playerCount = 2,
         bool teams = false); // 每方两个不同职业；侦察模式保留双方各一单位。
    View view(int player) const;
    bool act(const Action &action);
    bool actBatch(const std::vector<Action> &actions, std::wstring *why = nullptr);
    bool canActBatch(const std::vector<Action> &actions) const;
    bool previewBatch(const std::vector<Action> &actions, int player, View &result) const;
    View historicalView(int player, std::size_t step) const;
    bool finished() const; // 构造受限视图、执行一步及检查终局。
    const Snapshot &state() const;
    const std::vector<Snapshot> &snapshots() const;
    const std::vector<Event> &events() const; // 完整数据只供裁判或终局使用。
  private:                                    // 私人状态不由客户端修改。
    bool actCore(const Action &action);
    struct Trial {};
    Game(const Game &source, Trial);
    bool runBatch(const std::vector<Action> &actions, std::wstring *why);
    View viewAt(const Snapshot &snapshot, std::size_t eventCount, int player) const;
    Snapshot state_;
    std::vector<Snapshot> snapshots_;
    std::vector<Event> events_; // 每步保存快照，可逐步复盘。
    void startTurn();
    void copyLeap(int id, int dest);
    bool applyV102(const Action &action, Event &event);
    int pullDestination(int cell) const;
    void whirlpools();
    void endTurn(Event &event); // 吸附按轮结算，冷却按所属玩家回合结算。
    void moveUnit(int unit, int target, bool leap, bool publicPosition, int lead = -1,
                  int leadPiece = 0, bool blink = false);
    void damage(int unit, int target, bool ranged, bool execute, Event &event, bool hitSelf = false,
                double power = 2, bool burning = false,
                int burningPlayer = -1); // 同步位移先于伤害，同格敌人分别受伤。
    void destroyBuilding(int cell);      // 所有摧毁入口统一生成持续一轮的废墟。
    void checkVictory();
    void ignite(int actor, int cell, Event &event, bool immediate, bool hitSelf = true);
    void build(const Action &action, Event &event); // 建筑失败也消耗回合，火焰统一结算。
    void guardAction(const Action &action,
                     Event &event); // 每名士兵独立选择移动、攻击、建造或拆除。
    void
    resetDirectDamage(int player); // 在所属玩家回合开始清空其角色、士兵、建筑和玫瑰的受击记录。
    void resetCandidates(int observer, int unit);
    void revealCharge(Event &event); // 保守推理防止秘密路径经候选格泄露。
}; // 裁判接口结束。
} // 游戏命名空间结束。
