#pragma once        // 防止重复包含网络接口。
#include "game.hpp" // 使用自定义地图、完整回合与职业规则。
#include <memory>
#include <filesystem> // 会话独占网络实现并共享终局回放。

namespace forest { // 联网模块与游戏裁判分开。
enum class Phase {
    Idle,
    Listening,
    Connecting,
    Setup,
    Waiting,
    Playing,
    Finished,
    Recovering,
    Aborted,
    Error
}; // 明确连接和对局的生命周期。

class Session { // 一个会话代表一位本地玩家。
  public:       // 界面只能通过这些公开方法操作连接。
    Session();
    ~Session(); // 自动初始化和释放网络资源。
    bool host(unsigned short port, mist::Mode mode);
    bool join(const std::string &ipv4, unsigned short port); // 创建指定地图的房间或加入房主。
    bool host(unsigned short port, mist::Mode mode, const mist::Map &map, int teamSize = 2,
              int playerCount = 2,
              bool teams = false); // 房主可以指定自定义地形，握手时同步给客人。
    bool poll();
    void close();
    void interrupt();
    void finishRecovery();
    void setRecoveryTimeout(int milliseconds);
    bool saveRecovery(const std::filesystem::path &path) const;
    bool loadRecovery(const std::filesystem::path &path);
    bool actBatch(const std::vector<mist::Action> &actions);
    bool previewBatch(const std::vector<mist::Action> &actions, mist::View &result) const;
    int authority() const;
    const std::vector<mist::View> &history() const; // 非阻塞推进收发，或主动离开。
    bool chooseStart(std::array<int, 2> cells, std::array<mist::Role, 2> roles,
                     std::array<std::array<int, 3>, 2> formations = {});
    bool act(const mist::Action &action);
    bool newRound(); // 一次锁定己方阵容，之后逐步行动并可房主重赛。
    Phase phase() const;
    bool isHost() const;
    bool connected() const;
    int player() const;
    mist::Mode mode() const;
    int teamSize() const;
    int playerCount() const;
    bool teams() const;
    int joinedCount() const;          // 读取连接状态与公开房间信息。
    const mist::Map &terrain() const; // 选点之前即可读取双方确认的公开地形。
    bool hasView() const;
    bool hasStart() const;
    mist::View view() const; // 读取本人就绪状态与受限视图。
    const std::vector<mist::Event> &publicEvents() const;
    const std::wstring &status() const; // 进行中只提供脱敏的日志。
    std::shared_ptr<mist::Game> replay() const;
    std::shared_ptr<mist::Game>
    takeCompletedReplay(); // 终局才能获得全知回放，完成通知独立于下一局状态。
  private:                 // 平台套接字和完整裁判均隐藏于实现对象。
    struct Impl;
    std::unique_ptr<Impl> impl_; // 独占指针负责内部资源生命周期。
}; // 结束网络会话。

std::vector<std::string> localAddresses(); // 返回可分享的本机 IPv4 地址。
} // 结束网络命名空间。
