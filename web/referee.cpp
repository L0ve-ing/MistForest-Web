#include "../src/game.hpp"
#include "../src/presentation/skill_fx.hpp"
#include "../src/presentation/actor_motion.hpp"
#include <algorithm>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <tuple>

// Private line protocol: Node owns authentication and serializes each room's requests.
// Only Game::view is serialized. Snapshots, raw Events and setup coordinates never leave here.
namespace {
using namespace mist;
std::unique_ptr<Game> game;
std::string quoted(const std::string &s) {
    std::ostringstream out;
    out << '"';
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32) {
            const char *hex = "0123456789abcdef";
            out << "\\u00" << hex[c >> 4] << hex[c & 15];
        } else out << c;
    }
    out << '"';
    return out.str();
}
std::string quoted(const std::wstring &s) {
    std::string utf8;
    for (std::size_t i=0;i<s.size();++i) {
        unsigned c=static_cast<unsigned>(s[i]);
        if constexpr (sizeof(wchar_t)==2) {
            if(c>=0xd800 && c<=0xdbff && i+1<s.size()) {
                unsigned low=static_cast<unsigned>(s[i+1]);
                if(low>=0xdc00 && low<=0xdfff) {c=0x10000+((c-0xd800)<<10)+(low-0xdc00);++i;}
            }
        }
        if(c>=0xd800 && c<=0xdfff) c=0xfffd;
        if(c<0x80) utf8+=char(c);
        else if(c<0x800) {utf8+=char(0xc0|(c>>6));utf8+=char(0x80|(c&63));}
        else if(c<0x10000) {utf8+=char(0xe0|(c>>12));utf8+=char(0x80|((c>>6)&63));utf8+=char(0x80|(c&63));}
        else {utf8+=char(0xf0|(c>>18));utf8+=char(0x80|((c>>12)&63));utf8+=char(0x80|((c>>6)&63));utf8+=char(0x80|(c&63));}
    }
    return quoted(utf8);
}
template<class T> void array(std::ostream &out, const T &values) {
    out << '[';
    bool comma = false;
    for (const auto &value : values) {
        if (comma) out << ',';
        comma = true;
        out << static_cast<int>(value);
    }
    out << ']';
}
void action(std::ostream &out, const Action &a) {
    out << '[' << a.unit << ',' << int(a.kind) << ',' << a.target << ',' << a.subject << ','
        << a.piece << ',' << a.extra << ',' << a.amount << ',' << a.landing << ']';
}
void mapJson(std::ostream &out, const Map &m) {
    out << "{\"width\":" << m.width << ",\"height\":" << m.height << ",\"encoded\":"
        << quoted(encodeMap(m)) << ",\"cells\":";
    array(out, m.cells);
    out << ",\"horizontal\":"; array(out, m.horizontal);
    out << ",\"vertical\":"; array(out, m.vertical);
    out << '}';
}
std::vector<Action> actions(const View &v) {
    std::vector<Action> result;
    if (inputPlayer(v) != v.player || v.winner != -1) return result;
    std::set<std::tuple<int,int,int,int,int,int,int,int>> seen;
    for (int id = v.player * 2; id < v.player * 2 + v.teamSize; ++id)
        for (auto a : choices(v, id)) {
            // These targets are ignored by the existing rules. Canonicalize duplicates.
            if (a.kind == Kind::Surrender || a.kind == Kind::Follow)
                a.target = 0;
            if (!validate(v, a)) continue;
            auto key = std::make_tuple(a.unit,int(a.kind),a.target,a.subject,a.piece,a.extra,a.amount,a.landing);
            if (seen.insert(key).second) result.push_back(a);
        }
    return result;
}
void cellsJson(std::ostream &out, const Mask &mask) {
    out << '[';
    bool comma = false;
    for (int c = 0; c < 81; ++c) if (mask[c]) {
        if (comma) out << ',';
        comma = true;
        out << c + 1;
    }
    out << ']';
}
// Reuse the native renderer's descriptors, constructed only from this viewer's
// committed Views. Never export Event.action, Snapshot or hidden enemy routes.
void presentationJson(std::ostream &out, const View &v) {
    using namespace mist::presentation;
    if (!game || v.revision <= 0) {out << "null";return;}
    const Notice *line = nullptr;
    std::vector<Mask> phases;
    std::vector<const Notice *> notices;
    for (const auto &n : v.notices) if (n.revision == v.revision) {
        notices.push_back(&n);
        if (!line && n.unit >= 0) line = &n;
        if (fxAny(n.attacks) && (phases.empty() || phases.back() != n.attacks))
            phases.push_back(n.attacks);
    }
    if (!line) {out << "null";return;}
    if (phases.empty()) phases.push_back(Mask{});
    const auto before = game->historicalView(v.player, std::size_t(v.revision - 1));
    out << "{\"revision\":" << v.revision << ",\"kind\":" << int(line->kind)
        << ",\"actor\":" << (line->player == v.player ? line->unit : -1)
        << ",\"lead\":" << (sprites::movesActor(line->kind) ? 550 : 350)
        << ",\"duration\":" << (fxAny(phases.front()) ? 1400 : 850)
        << ",\"notices\":[";
    for (std::size_t i = 0; i < notices.size(); ++i) {
        if (i) out << ',';
        out << "{\"text\":" << quoted(notices[i]->text)
            << ",\"weight\":" << notices[i]->weight
            << ",\"feedback\":" << (notices[i]->feedback ? "true" : "false") << '}';
    }
    out << "],\"phases\":[";
    for (std::size_t i = 0; i < phases.size(); ++i) {
        if (i) out << ',';
        const auto fx = makeSkillFx(before, v, *line, phases[i], v.notices);
        out << "{\"style\":" << int(fx.style) << ",\"role\":" << int(fx.role)
            << ",\"kind\":" << int(fx.kind) << ",\"form\":" << fx.form;
        const std::pair<const char *, const Mask *> masks[] = {
            {"cells", &fx.cells}, {"attacks", &fx.attacks}, {"buds", &fx.buds},
            {"petals", &fx.petals}, {"wither", &fx.wither}, {"health", &fx.health},
            {"shield", &fx.shield}, {"impact", &fx.impact}, {"links", &fx.links}};
        for (const auto &item : masks) {
            out << ",\"" << item.first << "\":";
            cellsJson(out, *item.second);
        }
        out << '}';
    }
    out << "],\"movements\":[";
    bool comma = false;
    for (const auto &motion : sprites::visibleMovements(before, v, line->kind)) {
        if (motion.unit / 2 != v.player) continue;
        if (comma) out << ',';
        comma = true;
        out << "{\"unit\":" << motion.unit << ",\"piece\":" << motion.piece
            << ",\"from\":" << motion.from << ",\"to\":" << motion.to
            << ",\"style\":" << int(motion.style) << ",\"path\":[";
        for (std::size_t i = 0; i < motion.path.size(); ++i) {
            if (i) out << ',';
            out << '[' << motion.path[i].column << ',' << motion.path[i].row << ']';
        }
        out << "]}";
    }
    out << "]}";
}
void viewJson(std::ostream &out, const View &v, bool historical = false) {
    out << "{\"player\":" << v.player << ",\"turn\":" << v.turn << ",\"inputPlayer\":" << inputPlayer(v)
        << ",\"winner\":" << v.winner << ",\"revision\":" << v.revision << ",\"round\":" << v.roundNumber
        << ",\"turns\":" << v.turns << ",\"teamSize\":" << v.teamSize << ",\"playerCount\":" << v.playerCount
        << ",\"teams\":" << (v.teams ? "true" : "false") << ",\"map\":";
    mapJson(out, v.map);
    out << ",\"eliminated\":"; array(out, v.eliminated);
    out << ",\"units\":[";
    for (int id = 0; id < v.playerCount * 2; ++id) {
        if (id) out << ',';
        const auto &u = v.units[id];
        out << "{\"id\":" << id << ",\"role\":" << int(u.role) << ",\"position\":" << u.position
            << ",\"health\":" << u.health / 2 << ",\"armor\":" << u.armor << ",\"shield\":" << u.shield / 2
            << ",\"form\":" << u.form << ",\"cooldown\":" << u.cooldown << ",\"ammo\":" << u.ammo
            << ",\"swordQi\":" << u.swordQi << ",\"report\":" << quoted(u.report) << ",\"soldiers\":";
        array(out, u.soldiers);
        out << ",\"soldierHealth\":[";
        for (int k = 0; k < 3; ++k) {if(k) out << ',';out << u.soldierHealth[k] / 2;}
        out << "],\"silenced\":" << (u.silenced ? "true" : "false") << '}';
    }
    out << "],\"budget\":{\"active\":" << v.budget.active << ",\"moves\":" << v.budget.moves
        << ",\"attacks\":" << v.budget.attacks << ",\"used\":" << v.budget.used
        << ",\"followup\":" << (pendingFollowup(v) ? "true" : "false") << "},\"buildings\":[";
    for (int c = 0; c < 81; ++c) {
        if(c) out << ',';
        const auto &b = v.buildings[c];
        out << '[' << int(b.kind) << ',' << b.owner << ',' << b.health / 2 << ']';
    }
    out << "],\"fires\":[";
    for (int c = 0; c < 81; ++c) {if(c) out << ',';out << v.fires[c].owner;}
    out << "],\"roses\":[";
    for (std::size_t i=0;i<v.roses.size();++i) {
        if(i) out << ',';
        const auto &r=v.roses[i];out << '[' << r.unit << ',' << r.cell << ',' << r.health/2 << ']';
    }
    out << "],\"swords\":[";
    for (std::size_t i=0;i<v.swords.size();++i) {if(i) out << ',';out << '[' << v.swords[i].unit << ',' << v.swords[i].cell << ']';}
    out << "],\"enemyAttacks\":";array(out,v.enemyAttacks);
    out << ",\"result\":" << quoted(v.result) << ",\"feedback\":" << quoted(v.skillFeedback)
        << ",\"turnReport\":" << quoted(v.turnReport) << ",\"notices\":[";
    const auto start=v.notices.size()>100?v.notices.size()-100:0;
    for (std::size_t i=start;i<v.notices.size();++i) {
        if(i!=start) out << ',';
        const auto &n=v.notices[i];
        out << "{\"revision\":" << n.revision << ",\"text\":" << quoted(n.text)
            << ",\"weight\":" << n.weight << ",\"feedback\":" << (n.feedback ? "true" : "false") << '}';
    }
    out << "],\"actions\":[";
    if(!historical) {
        auto legal=actions(v);
        for(std::size_t i=0;i<legal.size();++i) {if(i) out << ',';action(out,legal[i]);}
    }
    out << "],\"presentation\":";
    if (historical) out << "null";
    else presentationJson(out, v);
    out << '}';
}
void require(bool valid) {if (!valid) throw std::runtime_error("invalid request");}
void catalog(std::ostream &out) {
    const char *portraits[]={"jin","wolf","ink","shadow","heavy","hidden-wolf","knight","alpha","armiger","flamer-display","wall-guard","prince","sword"};
    const char *sprites[]={"jin","wolf","ink","shadow","heavy","hidden-wolf","knight","alpha","armiger","flamer","wall-guard","prince","sword"};
    const char *kinds[]={"移动","普通攻击","侦察一行","侦察一列","炫空斗舞","附骨跟随","附骨带动","冲锋","切换形态","日字移动","游猎·倾轧","游猎·沉默","游猎·挥劈","挥劈并突进","游猎·退避","筑堡","筑墙","攻击并突进","嗜血","披甲","卸甲","直刺","离火","火步","攻击后移动","拆除","投降","分担","附骨模式","种植玫瑰","玫瑰之吻","剑刺","飞剑","回剑","攻击后移动","结束回合"};
    out << "{\"ok\":true,\"version\":\"1.1.8-web.2\",\"roles\":[";
    for(int i=0;i<13;++i) {
        if(i) out << ',';
        auto r=Role(i);
        out << "{\"id\":" << i << ",\"name\":" << quoted(roleName(r)) << ",\"skill\":" << quoted(skillName(r))
            << ",\"stats\":" << quoted(roleStats(r)) << ",\"help\":" << quoted(roleHelp(r)) << ",\"tips\":" << quoted(roleTips(r))
            << ",\"portrait\":" << quoted(std::string(portraits[i])) << ",\"sprite\":" << quoted(std::string(sprites[i])) << '}';
    }
    out << "],\"kinds\":[";
    for(int i=0;i<=int(Kind::End);++i) {if(i)out << ',';out << quoted(std::string(kinds[i]));}
    out << "]}";
}
void request(std::istream &in, std::ostream &out) {
    std::string command;in >> command;
    if(command=="catalog") {catalog(out);return;}
    if(command=="map") {
        std::string encoded;in >> encoded;Map m;
        if(encoded=="default") m=defaultMap(Mode::Heroes);
        else if(encoded=="empty") m=Map{};
        else require(decodeMap(Mode::Heroes,encoded,m));
        out << "{\"ok\":true,\"map\":";mapJson(out,m);out << '}';return;
    }
    if(command=="new") {
        int players,slots,teams,first;std::string encoded;
        require(bool(in >> players >> slots >> teams >> first >> encoded));
        Map m;require(decodeMap(Mode::Heroes,encoded,m));
        std::array<int,8> positions{};std::array<Role,8> roles{};Formations formations{};
        for(int i=0;i<8;++i) {int role;require(bool(in >> positions[i] >> role));require(validRole(role));roles[i]=Role(role);}
        for(auto &formation:formations) for(auto &cell:formation) require(bool(in >> cell));
        auto candidate=std::make_unique<Game>(positions,roles,mapMode(m),first,m,slots,formations,players,teams!=0);
        game=std::move(candidate);out << "{\"ok\":true}";return;
    }
    require(bool(game));
    int player;require(bool(in >> player));require(player>=0 && player<game->state().playerCount);
    if(command=="view" || command=="history") {
        out << "{\"ok\":true,\"view\":";
        if(command=="history") {std::size_t step;require(bool(in >> step));require(step<game->snapshots().size());viewJson(out,game->historicalView(player,step),true);}
        else viewJson(out,game->view(player));
        out << '}';return;
    }
    if(command=="act") {
        int revision,kind;Action a;
        require(bool(in >> revision >> a.unit >> kind >> a.target >> a.subject >> a.piece >> a.extra >> a.amount >> a.landing));
        require(kind>=0 && kind<=int(Kind::End));a.kind=Kind(kind);
        auto v=game->view(player);std::wstring why;
        if(revision!=v.revision) {out << "{\"ok\":false,\"error\":\"局面已更新，请重新选择动作。\"}";return;}
        if(inputPlayer(v)!=player || !validate(v,a,&why) || !game->act(a)) {
            out << "{\"ok\":false,\"error\":" << quoted(why.empty()?L"当前无法执行该动作。":why) << '}';return;
        }
        out << "{\"ok\":true,\"revision\":" << game->state().revision << '}';return;
    }
    throw std::runtime_error("unknown command");
}
}
int main() {
    std::ios::sync_with_stdio(false);
    for(std::string line;std::getline(std::cin,line);) {
        try {std::istringstream in(line);std::ostringstream out;request(in,out);std::cout << out.str() << '\n' << std::flush;}
        catch(const std::exception &) {std::cout << "{\"ok\":false,\"error\":\"裁判拒绝了无效的请求或开局配置。\"}\n" << std::flush;}
    }
}
