using namespace mist;
using mist::presentation::fxAny;
using mist::presentation::FxStyle;
using mist::presentation::fxVisible;
using mist::presentation::makeSkillFx;
using mist::presentation::SkillFx;
using mist::presentation::SkillFxDuration;
using mist::presentation::skillStyle;

constexpr int WIDTH = 1120, HEIGHT = 1100; // 增高逻辑画布以放大判定区域。
const COLORREF BG = RGB(241, 245, 239), INK = RGB(22, 43, 36), MUTED = RGB(100, 117, 107),
               TEAL = RGB(26, 110, 78), PALE = RGB(222, 239, 225), CORAL = RGB(188, 87, 67),
               LINE = RGB(211, 224, 213), WHITE = RGB(255, 255, 255),
               GOLD = RGB(205, 153, 45); // 森林主题。
enum class Page { Lobby, Config, Battle, Replay, Network, Editor, Skills, Tutorial };
enum class Phase { Cover, Setup, Playing, Waiting, Hold, Finished, Error }; // 页面与私密交接独立。

using Brush = mist::editor::Brush;

struct EditorPanel {
    mist::editor::MapEditor document;
    HWND widthEdit = nullptr, heightEdit = nullptr;
    std::filesystem::path path;
    std::wstring notice;
};

struct Hit {
    RECT rect;
    int id;
    bool enabled;
}; // 地形工具和命中区域。

using mist::tutorial::GuideStage;
using mist::tutorial::Lesson;
using mist::tutorial::LessonGoal;
using mist::tutorial::LessonTask;

struct State { // 每次只能选择一个尚未执行的单步动作。
    HWND window = nullptr, addressEdit = nullptr, portEdit = nullptr;
    Page page = Page::Lobby;
    Phase phase = Phase::Setup;
    Mode mode = Mode::Heroes;
    bool bot = true, online = false, assist = false, series = false, coverSetup = false,
         lastOnline = false, previousLog = false;
    int playerCount = 2, historyTurn = -1;
    bool teams = false;
    int teamSize = 2, viewer = 0, setupPlayer = 0, setupSlot = 0, setupPiece = 0, round = 1,
        replay = 0, historyOffset = 0; // 选人只在本回合第一项动作前允许改变。
    bool moreActions = false;
    int pendingLeave = 0, currentCommand = 0, historyMeasuredTurn = -2, historyMeasuredMax = 0;
    ULONGLONG leaveAt = 0;
    bool allowLeave = false;
    bool playbackOptions = false, fullAnimation = true;
    int playbackSpeed = 100;
    ULONGLONG optionsAt = 0;
    double replayRate = 1;
    std::vector<mist::presentation::sprites::Movement> replayMovements;
    mist::presentation::Playback replayActors;
    int actionScroll = 0;
    bool networkMode = false;
    BotStyle botStyle = BotStyle::Balanced;
    int wheelDelta = 0, wheelZone = 0; // 高精度滚轮余量只在同一操作区累计。
    bool tutorial = false, replayPrivate = false;
    int savedRevision = -1, savedAuthority = -1;
    mist::selection::ActionSelection choice;
    std::vector<View> replayViews;
    mist::tutorial::Guide guide;
    mist::presentation::Playback playback;
    mist::presentation::sprites::Timeline actorSprites;
    Mask replayAttacks{};
    SkillFx replayEffect;
    std::vector<SkillFx> replayEffects;
    ULONGLONG replayFxAt = 0;
    int replayFxStep = -1;
    int inspected = -1;
    ULONGLONG inspectAt = 0; // 局内说明仅展示角色资料，保持对局。
    Formations formations{};
    std::array<int, 8> starts{};
    std::array<Role, 8> roles{Role::Sniper, Role::Wolf, Role::Sniper, Role::Wolf};
    Role selectedRole = Role::Sniper;
    std::array<int, 4> score{}; // 四单位原始布阵只在本地裁判保存。
    Map customMap = defaultMap(Mode::Heroes), roundMap = defaultMap(Mode::Heroes);
    bool customEnabled = false;
    EditorPanel editor; // 草稿、撤销与平台控件不再散落在全局界面状态中。
    std::shared_ptr<Game> game, last;
    std::unique_ptr<forest::Session> net;
    std::wstring notice, addresses;
    std::vector<Hit> hits;
    std::mt19937 rng{std::random_device{}()};
    double scale = 1;
    int offsetX = 0, offsetY = 0; // 窗口状态、受限网络和最近复盘。
} s;                              // 单窗口实例。

std::filesystem::path recoveryFile();
void openReplay();
void submitPlan();
void launchTutorial(Role role, bool basics);
void tutorialProgress(const Action &action);
void resumeNetwork();
void exportAnyReplay();
void tutorialBattle(HDC dc);
void tutorialContinue();
void tutorialRetry();
void tutorialDemo();
bool tutorialInputAllowed();
void tutorialObserved(int id);
Action tutorialEnemyAction();
void selectReplayStep(int step);
int shadowMark(const View &v, int unit, int piece = 0);
void shadowRing(HDC dc, RECT slot, int state, bool portrait = false);
