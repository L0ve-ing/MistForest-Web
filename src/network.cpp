#include "socket_compat.hpp" // Windows与Linux共享真实TCP测试。
#include "network.hpp"       // 原生界面的会话接口。
#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <random>
#include <iomanip>

namespace forest {
namespace {
using namespace mist;
using Clock = std::chrono::steady_clock;
constexpr int Version = ProtocolVersion;
constexpr std::size_t MaxFrame = 8 * 1024 * 1024;

template <class T, std::size_t N> void put(std::ostream &o, const std::array<T, N> &a) {
    for (auto x : a)
        o << ' ' << x;
}

template <class T, std::size_t N> bool get(std::istream &in, std::array<T, N> &a) {
    for (auto &x : a)
        if (!(in >> x))
            return false;
    return true;
}

void actionOut(std::ostream &o, const Action &a) {
    o << ' ' << a.unit << ' ' << int(a.kind) << ' ' << a.target << ' ' << a.subject << ' '
      << a.piece << ' ' << a.extra << ' ' << a.amount << ' ' << a.landing;
}

bool actionIn(std::istream &in, Action &a) {
    int k;
    if (!(in >> a.unit >> k >> a.target >> a.subject >> a.piece >> a.extra >> a.amount >>
          a.landing) ||
        a.unit < 0 || a.unit >= 8 || k < 0 || k > int(Kind::End) || a.target < 0 || a.target > 81 ||
        a.subject < -1 || a.subject >= 8 || a.piece < 0 || a.piece > 2 || a.extra < 0 ||
        a.extra > 81 || a.amount < 0 || a.amount > 9 || a.landing < 0 || a.landing > 81)
        return false;
    a.kind = Kind(k);
    return true;
}

bool complete(std::istream &in) {
    in >> std::ws;
    return in.eof() && !in.bad();
}

bool sameAction(const Action &a, const Action &b) {
    return a.unit == b.unit && a.kind == b.kind && a.target == b.target &&
           a.subject == b.subject && a.piece == b.piece && a.extra == b.extra &&
           a.amount == b.amount && a.landing == b.landing;
}

bool sameHistoryPrefix(const Game &before, const Game &after) {
    if (before.events().size() > after.events().size())
        return false;
    for (std::size_t i = 0; i < before.events().size(); ++i)
        if (!sameAction(before.events()[i].action, after.events()[i].action))
            return false;
    return true;
}

std::string token() {
    std::random_device r;
    std::ostringstream o;
    o << std::hex;
    for (int n = 0; n < 4; ++n)
        o << std::setw(8) << std::setfill('0') << r();
    return o.str();
}

std::string addressOf(SOCKET fd) {
    sockaddr_in a{};
    SockLength n = sizeof(a);
    char value[INET_ADDRSTRLEN]{};
    if (getpeername(fd, reinterpret_cast<sockaddr *>(&a), &n))
        return "127.0.0.1";
    inet_ntop(AF_INET, &a.sin_addr, value, sizeof(value));
    return value;
}
}

struct Session::Impl {
    struct Peer {
        SOCKET fd = INVALID_SOCKET;
        std::string input, output;
        bool hello = false, connecting = false;
        Clock::time_point last = Clock::now(), ping = Clock::now();
    };

    struct Member {
        std::string ip = "127.0.0.1", key;
        unsigned short port = 0;
    };

    bool initialized = false, dirty = true, pending = false, aborted = false;
    int local = -1, leader = 0, count = 2, slots = 2, round = 1, first = 0, epoch = 0,
        grace = 60000;
    bool team = false;
    Mode mode = Mode::Heroes;
    Map map = defaultMap(Mode::Heroes);
    SOCKET listener = INVALID_SOCKET;
    unsigned short listeningPort = 0;
    std::string selfKey = token();
    std::array<Member, 4> members{};
    std::array<Peer, 4> peers{};
    std::vector<Peer> unknown;
    std::array<bool, 4> ready{};
    std::array<int, 8> starts{};
    std::array<Role, 8> roles{};
    Formations formations{};
    std::shared_ptr<Game> game, completed;
    View own;
    std::vector<View> views;
    std::vector<Event> events;
    Phase stage = Phase::Idle;
    std::wstring message = L"尚未连接。";
    Clock::time_point recovery{}, retry = Clock::now();

    Impl() {
        initialized = socketReady();
    }

    ~Impl() {
        stop();
        if (initialized)
            socketCleanup();
    }

    void stop() {
        closeSocket(listener);
        for (auto &p : peers)
            closeSocket(p.fd);
        for (auto &p : unknown)
            closeSocket(p.fd);
        unknown.clear();
    }

    void error(const wchar_t *why) {
        stop();
        stage = Phase::Error;
        message = why;
        dirty = true;
    }

    bool listenAt(unsigned short port) {
        listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listener == INVALID_SOCKET)
            return false;
        int yes = 1;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&yes),
                   sizeof(yes));
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_ANY);
        a.sin_port = htons(port);
        if (bind(listener, reinterpret_cast<sockaddr *>(&a), sizeof(a)) || listen(listener, 8) ||
            !nonblocking(listener)) {
            closeSocket(listener);
            return false;
        }
        SockLength n = sizeof(a);
        getsockname(listener, reinterpret_cast<sockaddr *>(&a), &n);
        listeningPort = ntohs(a.sin_port);
        return true;
    }

    void queue(Peer &p, const std::string &s) {
        if (s.size() > MaxFrame || p.output.size() + s.size() > MaxFrame * 4) {
            error(L"消息超过会话容量。");
            return;
        }
        p.output += s + '\n';
    }

    void sendTo(int id, const std::string &s) {
        if (id >= 0 && id < 4 && id != local && peers[id].fd != INVALID_SOCKET)
            queue(peers[id], s);
    }

    void broadcast(const std::string &s) {
        for (int i = 0; i < count; ++i)
            sendTo(i, s);
    }

    bool connectedTo(int p) const {
        return p == local ||
               (peers[p].fd != INVALID_SOCKET && peers[p].hello && !peers[p].connecting);
    }

    int joined() const {
        int n = 0;
        for (int p = 0; p < count; ++p)
            if (connectedTo(p))
                ++n;
        return n;
    }

    void update() {
        if (aborted) {
            stage = Phase::Aborted;
            message = L"无法恢复连接，本局已终止，可查看完整复盘。";
        } else if (game) {
            own = game->view(local);
            if (game->finished()) {
                stage = Phase::Finished;
                completed = game;
                message = L"对局结束。";
            } else if (joined() < count) {
                if (recovery == Clock::time_point{})
                    recovery = Clock::now();
                stage = Phase::Recovering;
                message =
                    L"正在恢复连接；房主为玩家" + std::to_wstring(leader + 1) + L"，原对局已保留。";
            } else {
                recovery = {};
                stage = own.turn == local && !pending ? Phase::Playing : Phase::Waiting;
                message = stage == Phase::Playing ? L"轮到你行动。" : L"等待其他玩家。";
            }
        } else {
            stage = local >= 0 && !ready[local] ? Phase::Setup : Phase::Waiting;
            message = L"选择阵容，等待所有玩家就绪。";
        }
        dirty = true;
    }

    std::string roster() const {
        std::ostringstream o;
        o << "ROSTER " << Version << ' ' << leader << ' ' << epoch << ' ' << count << ' ' << slots
          << ' ' << team << ' ' << int(mode) << ' ' << encodeMap(map);
        for (const auto &m : members)
            o << ' ' << m.ip << ' ' << m.port << ' ' << (m.key.empty() ? "-" : m.key);
        return o.str();
    }

    std::string checkpoint() const {
        std::ostringstream o;
        o << "CHECK " << Version << ' ' << epoch << ' ' << leader << ' ' << round << ' ' << first
          << ' ' << count << ' ' << slots << ' ' << team << ' ' << int(mode) << ' '
          << encodeMap(map) << ' ' << aborted;
        put(o, ready);
        put(o, starts);
        for (auto r : roles)
            o << ' ' << int(r);
        for (auto f : formations)
            put(o, f);
        std::size_t n = game ? game->events().size() : 0;
        o << ' ' << n;
        if (game)
            for (const auto &e : game->events())
                actionOut(o, e.action);
        return o.str();
    }

    void rebuildHistory(bool reset = false) {
        if (!game) {
            views.clear();
            events.clear();
            return;
        }
        if (reset || views.size() > game->snapshots().size() ||
            events.size() > game->events().size() ||
            (!views.empty() && views.front().player != local)) {
            views.clear();
            events.clear();
        }
        for (std::size_t i = views.size(); i < game->snapshots().size(); ++i)
            views.push_back(game->historicalView(local, i));
        for (std::size_t i = events.size(); i < game->events().size(); ++i) {
            auto e = game->events()[i];
            e.action = {};
            e.privateText.clear();
            e.playerNotes = {};
            e.feedback.clear();
            events.push_back(e);
        }
    }

    bool acceptCheck(std::istream &in, int sender) {
        int version, ep, host, serial, opening, nplayers, nslots, raw;
        bool teams, final;
        std::string encoded;
        std::array<bool, 4> rr;
        std::array<int, 8> pos;
        std::array<Role, 8> rs;
        Formations ff;
        if (!(in >> version >> ep >> host >> serial >> opening >> nplayers >> nslots >> teams >>
              raw >> encoded >> final) ||
            version != Version || ep < epoch || host < 0 || host >= count || nplayers != count ||
            nslots != slots || teams != team || !validMode(raw) || !get(in, rr) || !get(in, pos))
            return false;
        if (sender >= 0 && sender != host)
            return false;
        for (auto &r : rs) {
            int x;
            if (!(in >> x) || !validRole(x))
                return false;
            r = Role(x);
        }
        for (auto &f : ff)
            if (!get(in, f))
                return false;
        int n;
        if (!(in >> n) || n < 0 || n > 10000)
            return false;
        Map terrain;
        if (!decodeMap(Mode(raw), encoded, terrain))
            return false;
        std::shared_ptr<Game> restored;
        bool all = true;
        for (int p = 0; p < count; ++p)
            all = all && rr[p];
        if (all) {
            restored = std::make_shared<Game>(pos, rs, Mode(raw), opening, terrain, slots, ff,
                                              count, team);
            for (int i = 0; i < n; ++i) {
                Action a;
                if (!actionIn(in, a) || !restored->act(a))
                    return false;
            }
        } else if (n)
            return false;
        if (!complete(in)) {
            return false;
        }
        if (serial == round && ep == epoch && game && restored &&
            restored->state().revision < game->state().revision)
            return true;
        // Reuse only an identical round/action prefix. Recovery or a different
        // formation starts a fresh viewer-filtered history, even at the same revision.
        bool keepHistory = game && restored && serial == round && opening == first &&
                           raw == int(mode) && encodeMap(terrain) == encodeMap(map) &&
                           rr == ready && pos == starts && rs == roles && ff == formations &&
                           sameHistoryPrefix(*game, *restored);
        epoch = ep;
        leader = host;
        round = serial;
        first = opening;
        mode = Mode(raw);
        map = terrain;
        ready = rr;
        starts = pos;
        roles = rs;
        formations = ff;
        game = restored;
        aborted = final;
        pending = false;
        rebuildHistory(!keepHistory);
        update();
        return true;
    }

    void distribute() {
        if (game)
            own = game->view(local);
        rebuildHistory();
        broadcast(checkpoint());
        update();
    }

    void begin() {
        if (local != leader || game || joined() != count)
            return;
        for (int p = 0; p < count; ++p)
            if (!ready[p])
                return;
        game =
            std::make_shared<Game>(starts, roles, mode, first, map, slots, formations, count, team);
        distribute();
    }

    void lost(int id) {
        closeSocket(peers[id].fd);
        peers[id] = Peer{};
        if (local < 0) {
            error(L"尚未加入房间，连接失败。");
            return;
        }
        if (id == leader) {
            int next = local;
            for (int p = 0; p < count; ++p)
                if (connectedTo(p))
                    next = std::min(next, p);
            leader = next;
            ++epoch;
            if (leader == local) {
                broadcast(roster());
                broadcast(checkpoint());
            }
        }
        pending = false;
        update();
    }

    void startConnect(int id, const std::string &ip, unsigned short port) {
        auto &p = peers[id];
        if (p.fd != INVALID_SOCKET)
            return;
        p.fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (p.fd == INVALID_SOCKET || !nonblocking(p.fd)) {
            closeSocket(p.fd);
            return;
        }
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_port = htons(port);
        if (inet_pton(AF_INET, ip.c_str(), &a.sin_addr) != 1) {
            closeSocket(p.fd);
            return;
        }
        int result = connect(p.fd, reinterpret_cast<sockaddr *>(&a), sizeof(a));
        if (result && !wouldBlock()) {
            closeSocket(p.fd);
            return;
        }
        p.connecting = result != 0;
        p.last = Clock::now();
        p.hello = false;
        if (!p.connecting)
            hello(p);
    }

    void hello(Peer &p) {
        queue(p, "HELLO " + std::to_string(Version) + " " + std::to_string(local) + " " +
                     std::to_string(listeningPort) + " " + selfKey + " " + std::to_string(epoch) +
                     " " + std::to_string(leader));
    }

    bool line(int id, const std::string &msg) {
        std::istringstream in(msg);
        std::string cmd;
        in >> cmd;
        if (cmd == "PING") {
            sendTo(id, "PONG");
            return complete(in);
        }
        if (cmd == "PONG")
            return complete(in);
        if (cmd == "WELCOME") {
            int version;
            if (!(in >> version >> local) || version != Version || local < 0 || local > 3)
                return false;
            members[local] = {"127.0.0.1", selfKey, listeningPort};
            peers[id].hello = true;
            return complete(in);
        }
        if (cmd == "ROSTER") {
            int version, raw, host, ep, cnt, sl;
            bool teams;
            std::string encoded;
            if (!(in >> version >> host >> ep >> cnt >> sl >> teams >> raw >> encoded) ||
                version != Version || cnt < 2 || cnt > 4 || sl < 1 || sl > 2 ||
                (teams && cnt != 4) || host < 0 || host >= cnt || ep < epoch || !validMode(raw))
                return false;
            Map next;
            if (!decodeMap(Mode(raw), encoded, next))
                return false;
            std::array<Member, 4> mm;
            for (auto &m : mm) {
                unsigned port;
                if (!(in >> m.ip >> port >> m.key) || port > 65535)
                    return false;
                m.port = static_cast<unsigned short>(port);
                if (m.key == "-")
                    m.key.clear();
            }
            if (!complete(in))
                return false;
            leader = host;
            epoch = ep;
            count = cnt;
            slots = sl;
            team = teams;
            mode = Mode(raw);
            map = next;
            if (local != 0 && mm[0].ip == "127.0.0.1")
                mm[0].ip = members[0].ip;
            members = mm;
            if (local >= 0) {
                members[local].port = listeningPort;
                members[local].key = selfKey;
            }
            peers[id].hello = true;
            update();
            return true;
        }
        if (cmd == "CHECK")
            return acceptCheck(in, id);
        if (cmd == "START") {
            if (local != leader || game || id < 0 || id >= count)
                return false;
            int serial;
            if (!(in >> serial) || serial != round)
                return false;
            for (int k = 0; k < 2; ++k) {
                int slot = id * 2 + k, r;
                if (!(in >> starts[slot] >> r) || !validRole(r) || !get(in, formations[slot]))
                    return false;
                roles[slot] = Role(r);
                if (k < slots && (!valid(mode, starts[slot], map) ||
                                  (roles[slot] == Role::WallGuard &&
                                   !validFormation(mode, map, starts[slot], formations[slot]))))
                    return false;
            }
            if (!complete(in) || (slots == 2 && roles[id * 2] == roles[id * 2 + 1]))
                return false;
            ready[id] = true;
            broadcast(checkpoint());
            begin();
            return true;
        }
        if (cmd == "ACT") {
            if (local != leader || !game || game->finished() || aborted)
                return false;
            int serial, revision, n;
            if (!(in >> serial >> revision >> n) || serial != round || n < 1 || n > 16)
                return false;
            std::vector<Action> batch(n);
            for (auto &a : batch)
                if (!actionIn(in, a) || a.unit / 2 != id)
                    return false;
            if (!complete(in))
                return false;
            if (revision != game->state().revision || stage == Phase::Recovering) {
                sendTo(id, checkpoint());
                return true;
            }
            if (!game->actBatch(batch)) {
                sendTo(id, checkpoint());
                return true;
            }
            distribute();
            return true;
        }
        if (cmd == "SYNC") {
            if (local == leader)
                sendTo(id, checkpoint());
            return complete(in);
        }
        if (cmd == "GIVEUP") {
            if (local != leader)
                return false;
            aborted = true;
            broadcast("ABORT");
            update();
            return complete(in);
        }
        if (cmd == "ABORT") {
            if (id != leader)
                return false;
            aborted = true;
            update();
            return complete(in);
        }
        if (cmd == "ELECT") {
            int ep, host;
            if (!(in >> ep >> host) || host != id || ep < epoch)
                return false;
            epoch = ep;
            leader = host;
            update();
            sendTo(id, "SYNC");
            return complete(in);
        }
        return false;
    }

    bool receiveHello(Peer &p) {
        auto end = p.input.find('\n');
        if (end == std::string::npos)
            return true;
        std::istringstream in(p.input.substr(0, end));
        std::string cmd, key;
        int version, id, port, ep, host;
        if (!(in >> cmd >> version >> id >> port >> key >> ep >> host) || cmd != "HELLO" ||
            version != Version || port < 1 || port > 65535 || key.size() != 32 || !complete(in))
            return false;
        p.input.erase(0, end + 1);
        if (id >= 0 && id < count && members[id].key == key && ep > epoch && host >= 0 &&
            host < count) {
            epoch = ep;
            leader = host;
        }
        if (id < 0) {
            if (local != leader || game)
                return false;
            id = 1;
            while (id < count && !members[id].key.empty())
                ++id;
            if (id >= count)
                return false;
            members[id] = {addressOf(p.fd), key, 0};
            members[id].port = static_cast<unsigned short>(port);
        } else if (id >= count || id == local || members[id].key != key)
            return false;
        closeSocket(peers[id].fd);
        p.hello = true;
        p.connecting = false;
        peers[id] = std::move(p);
        p.fd = INVALID_SOCKET;
        if (local == leader) {
            queue(peers[id], "WELCOME " + std::to_string(Version) + " " + std::to_string(id));
            broadcast(roster());
            sendTo(id, checkpoint());
        } else {
            sendTo(id, roster());
            sendTo(id, "SYNC");
        }
        update();
        return true;
    }

    void flush(Peer &p) {
        while (p.fd != INVALID_SOCKET && !p.output.empty()) {
            int sent = static_cast<int>(send(p.fd, p.output.data(),
                                             std::min<std::size_t>(65536, p.output.size()),
#ifdef _WIN32
                                             0
#else
                                             MSG_NOSIGNAL
#endif
                                             ));
            if (sent > 0)
                p.output.erase(0, sent);
            else if (wouldBlock())
                break;
            else {
                closeSocket(p.fd);
                break;
            }
        }
    }

    void tick() {
        if (stage == Phase::Idle || stage == Phase::Error || aborted)
            return;
        auto now = Clock::now();
        if (listener != INVALID_SOCKET) {
            SOCKET fd = accept(listener, nullptr, nullptr);
            if (fd != INVALID_SOCKET) {
                if (nonblocking(fd) && unknown.size() < 8) {
                    Peer p;
                    p.fd = fd;
                    unknown.push_back(std::move(p));
                } else
                    closeSocket(fd);
            }
        }
        for (auto &p : unknown) {
            if (p.fd != INVALID_SOCKET) {
                char data[8192];
                int n = static_cast<int>(recv(p.fd, data, sizeof(data), 0));
                if (n > 0)
                    p.input.append(data, n);
                else if (n == 0 || (n < 0 && !wouldBlock()))
                    closeSocket(p.fd);
                if (p.input.size() > 4096 || !receiveHello(p) ||
                    now - p.last > std::chrono::seconds(5))
                    closeSocket(p.fd);
            }
        }
        unknown.erase(std::remove_if(unknown.begin(), unknown.end(),
                                     [](const Peer &p) { return p.fd == INVALID_SOCKET; }),
                      unknown.end());
        for (int id = 0; id < 4; ++id) {
            auto &p = peers[id];
            if (p.fd == INVALID_SOCKET || id == local)
                continue;
            if (p.connecting) {
                fd_set writable, errors;
                FD_ZERO(&writable);
                FD_ZERO(&errors);
                FD_SET(p.fd, &writable);
                FD_SET(p.fd, &errors);
                timeval t{};
                select(static_cast<int>(p.fd) + 1, nullptr, &writable, &errors, &t);
                if (FD_ISSET(p.fd, &writable) || FD_ISSET(p.fd, &errors)) {
                    int err = 0;
                    SockLength len = sizeof(err);
                    getsockopt(p.fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&err), &len);
                    if (err) {
                        lost(id);
                        continue;
                    }
                    p.connecting = false;
                    hello(p);
                } else if (now - p.last > std::chrono::seconds(2)) {
                    lost(id);
                    continue;
                } else
                    continue;
            }
            flush(p);
            if (p.fd == INVALID_SOCKET) {
                lost(id);
                continue;
            }
            for (int k = 0; k < 32; ++k) {
                char data[65536];
                int n = static_cast<int>(recv(p.fd, data, sizeof(data), 0));
                if (n == 0) {
                    lost(id);
                    break;
                }
                if (n < 0) {
                    if (!wouldBlock())
                        lost(id);
                    break;
                }
                p.input.append(data, n);
                if (p.input.size() > MaxFrame * 2) {
                    lost(id);
                    break;
                }
                std::size_t e;
                while ((e = p.input.find('\n')) != std::string::npos) {
                    std::string msg = p.input.substr(0, e);
                    p.input.erase(0, e + 1);
                    bool ok = false;
                    try {
                        ok = line(id, msg);
                    } catch (const std::exception &) {
                        ok = false;
                    }
                    if (!ok) {
                        lost(id);
                        break;
                    }
                    p.last = now;
                }
                if (p.fd == INVALID_SOCKET)
                    break;
            }
            if (p.fd == INVALID_SOCKET) {
                continue;
            }
            if (now - p.last > std::chrono::seconds(6)) {
                lost(id);
                continue;
            }
            if (now - p.ping > std::chrono::seconds(1)) {
                sendTo(id, "PING");
                p.ping = now;
            }
            flush(p);
        }
        if (local >= 0 && now >= retry) {
            retry = now + std::chrono::milliseconds(250);
            for (int p = 0; p < count; ++p)
                if ((p < local || p == leader) && p != local && !members[p].key.empty() &&
                    !connectedTo(p))
                    startConnect(p, members[p].ip, members[p].port);
        }
        if (game && !aborted) {
            if (joined() == count) {
                if (stage == Phase::Recovering)
                    update();
            } else {
                if (recovery == Clock::time_point{})
                    update();
                if (now - recovery > std::chrono::milliseconds(grace)) {
                    aborted = true;
                    if (local == leader)
                        broadcast("ABORT");
                    update();
                }
            }
        }
        begin();
    }
};

Session::Session() : impl_(std::make_unique<Impl>()) {}

Session::~Session() = default;

void Session::close() {
    impl_ = std::make_unique<Impl>();
}

bool Session::host(unsigned short port, Mode mode) {
    return host(port, mode, defaultMap(mode));
}

bool Session::host(unsigned short port, Mode mode, const Map &map, int slots, int count,
                   bool teams) {
    close();
    auto &s = *impl_;
    if (!s.initialized || !port || count < 2 || count > 4 || slots < 1 || slots > 2 ||
        (teams && count != 4) || !validMap(mode, map)) {
        s.error(L"房间参数无效。");
        return false;
    }
    s.local = 0;
    s.leader = 0;
    s.count = count;
    s.slots = heroMode(mode) ? slots : 1;
    s.team = teams;
    s.map = map;
    s.mode = heroMode(mode) ? mapMode(map) : mode;
    if (!s.listenAt(port)) {
        s.error(L"无法监听房间端口。");
        return false;
    }
    s.members[0] = {"127.0.0.1", s.selfKey, s.listeningPort};
    s.update();
    return true;
}

bool Session::join(const std::string &ip, unsigned short port) {
    close();
    auto &s = *impl_;
    if (!s.initialized || !port || !s.listenAt(0)) {
        s.error(L"无法创建恢复端口。");
        return false;
    }
    s.members[0].ip = ip;
    s.members[0].port = port;
    s.stage = Phase::Connecting;
    s.startConnect(0, ip, port);
    if (s.peers[0].fd == INVALID_SOCKET) {
        s.error(L"无法连接房主。");
        return false;
    }
    return true;
}

bool Session::poll() {
    impl_->tick();
    bool d = impl_->dirty;
    impl_->dirty = false;
    return d;
}

bool Session::chooseStart(std::array<int, 2> cells, std::array<Role, 2> roles,
                          std::array<std::array<int, 3>, 2> formations) {
    auto &s = *impl_;
    if (s.local < 0 || s.ready[s.local] || s.game)
        return false;
    for (int k = 0; k < s.slots; ++k)
        if (!validRole(int(roles[k])) || !valid(s.mode, cells[k], s.map) ||
            (roles[k] == Role::WallGuard &&
             !validFormation(s.mode, s.map, cells[k], formations[k])))
            return false;
    if (s.slots == 2 && roles[0] == roles[1])
        return false;
    std::ostringstream o;
    o << "START " << s.round;
    for (int k = 0; k < 2; ++k) {
        int id = s.local * 2 + k;
        s.starts[id] = cells[k];
        s.roles[id] = roles[k];
        s.formations[id] = formations[k];
        o << ' ' << cells[k] << ' ' << int(roles[k]);
        put(o, formations[k]);
    }
    s.ready[s.local] = true;
    if (s.local == s.leader) {
        s.broadcast(s.checkpoint());
        s.begin();
    } else
        s.sendTo(s.leader, o.str());
    s.update();
    return true;
}

bool Session::act(const Action &a) {
    return actBatch({a});
}

bool Session::actBatch(const std::vector<Action> &actions) {
    auto &s = *impl_;
    if (s.stage != Phase::Playing || s.pending || actions.empty() || actions.size() > 16)
        return false;
    for (auto &a : actions)
        if (a.unit / 2 != s.local)
            return false;
    if (s.local == s.leader) {
        if (!s.game->actBatch(actions))
            return false;
        s.distribute();
    } else {
        if (!s.game->canActBatch(actions))
            return false;
        std::ostringstream o;
        o << "ACT " << s.round << ' ' << s.own.revision << ' ' << actions.size();
        for (auto a : actions)
            actionOut(o, a);
        s.sendTo(s.leader, o.str());
        s.pending = true;
        s.update();
    }
    return true;
}

bool Session::newRound() {
    auto &s = *impl_;
    if (s.local != s.leader || s.stage != Phase::Finished || s.joined() != s.count)
        return false;
    ++s.round;
    s.first = (s.first + 1) % s.count;
    s.game.reset();
    s.ready = {};
    s.starts = {};
    s.roles = {};
    s.formations = {};
    s.views.clear();
    s.events.clear();
    s.distribute();
    return true;
}

void Session::interrupt() {
    auto &s = *impl_;
    for (int p = 0; p < s.count; ++p)
        if (p != s.local)
            closeSocket(s.peers[p].fd);
    s.recovery = Clock::now();
    s.update();
}

void Session::finishRecovery() {
    auto &s = *impl_;
    if (s.stage != Phase::Recovering)
        return;
    if (s.local != s.leader) {
        s.sendTo(s.leader, "GIVEUP");
        return;
    }
    s.aborted = true;
    s.broadcast("ABORT");
    s.update();
}

void Session::setRecoveryTimeout(int ms) {
    impl_->grace = std::max(10, ms);
}

bool Session::saveRecovery(
    const std::filesystem::path &path) const { // 临时文件落盘后替换，避免进程退出留下半个检查点。
    const auto &s = *impl_;
    if (s.local < 0 || !s.game)
        return false;
    try {
        auto temp = path;
        temp += L".tmp";
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out << s.local << ' ' << s.selfKey << ' ' << s.listeningPort << '\n'
            << s.roster() << '\n'
            << s.checkpoint() << '\n';
        out.flush();
        bool ok = bool(out);
        out.close();
        if (!ok)
            return false;
#ifdef _WIN32
        return MoveFileExW(temp.c_str(), path.c_str(),
                           MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
        std::filesystem::rename(temp, path);
        return true;
#endif
    } catch (const std::exception &) {
        return false;
    }
}

bool Session::loadRecovery(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    int id;
    unsigned port;
    std::string key, roster, checkpoint;
    if (!(in >> id >> key >> port) || id < 0 || id > 3 || key.size() != 32 || port < 1 ||
        port > 65535)
        return false;
    in.ignore();
    std::getline(in, roster);
    std::getline(in, checkpoint);
    close();
    auto &s = *impl_;
    s.local = id;
    s.selfKey = key;
    if (!s.listenAt(static_cast<unsigned short>(port))) {
        s.error(L"恢复端口仍被占用。");
        return false;
    }
    if (!s.line(0, roster))
        return false;
    std::istringstream state(checkpoint);
    std::string cmd;
    state >> cmd;
    if (cmd != "CHECK" || !s.acceptCheck(state, -1))
        return false;
    s.recovery = Clock::now();
    s.update();
    return true;
}

bool Session::previewBatch(const std::vector<Action> &actions, View &result) const {
    if (!impl_->game)
        return false;
    return impl_->game->previewBatch(actions, impl_->local, result);
}

Phase Session::phase() const {
    return impl_->stage;
}

bool Session::isHost() const {
    return impl_->local == impl_->leader;
}

bool Session::connected() const {
    return impl_->joined() == impl_->count;
}

int Session::player() const {
    return std::max(0, impl_->local);
}

Mode Session::mode() const {
    return impl_->mode;
}

int Session::teamSize() const {
    return impl_->slots;
}

int Session::playerCount() const {
    return impl_->count;
}

bool Session::teams() const {
    return impl_->team;
}

int Session::joinedCount() const {
    return impl_->joined();
}

const Map &Session::terrain() const {
    return impl_->map;
}

bool Session::hasView() const {
    return bool(impl_->game);
}

bool Session::hasStart() const {
    return impl_->local >= 0 && impl_->ready[impl_->local];
}

View Session::view() const {
    return impl_->own;
}

const std::vector<Event> &Session::publicEvents() const {
    return impl_->events;
}

const std::wstring &Session::status() const {
    return impl_->message;
}

std::shared_ptr<Game> Session::replay() const {
    return impl_->game && (impl_->game->finished() || impl_->aborted) ? impl_->game : nullptr;
}

std::shared_ptr<Game> Session::takeCompletedReplay() {
    auto r = impl_->completed;
    impl_->completed.reset();
    return r;
}

int Session::authority() const {
    return impl_->leader;
}

const std::vector<View> &Session::history() const {
    return impl_->views;
}

std::vector<std::string> localAddresses() {
    std::vector<std::string> out;
    if (!socketReady())
        return out;
    char host[256]{};
    addrinfo hints{}, *a = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (!gethostname(host, sizeof(host)) && !getaddrinfo(host, nullptr, &hints, &a)) {
        for (auto p = a; p; p = p->ai_next) {
            char ip[INET_ADDRSTRLEN]{};
            inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in *>(p->ai_addr)->sin_addr, ip,
                      sizeof(ip));
            if (std::find(out.begin(), out.end(), ip) == out.end())
                out.emplace_back(ip);
        }
        freeaddrinfo(a);
    }
    socketCleanup();
    return out;
}
}
