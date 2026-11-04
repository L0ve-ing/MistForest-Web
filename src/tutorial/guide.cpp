#include "guide.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace mist::tutorial {
namespace {
// Training scenes use ordinary legal actions and copied checkpoints. No live-game
// state is edited, and dummy positions are revealed only in this offline renderer.
LessonTask task(Action action, LessonGoal goal, const wchar_t *title, const wchar_t *explain,
                const wchar_t *result) {
    return {action, goal, title, explain, result};
}

Action teach(Kind kind, int target = 0, int subject = -1, int piece = 0, int extra = 0,
             int amount = 1, int landing = 0) {
    return {0, kind, target, subject, piece, extra, kind == Kind::SwordStab ? 0 : amount, landing};
}

std::vector<Lesson> tutorialCourse(Role role, bool basics) {
    using G = LessonGoal;
    std::vector<Lesson> lessons;
    auto add = [&](const wchar_t *title, std::vector<LessonTask> tasks, int enemy = 14,
                   int start = 13, std::vector<Action> setup = {}, bool reveal = true) {
        lessons.push_back({title, start, enemy, setup, tasks, reveal});
    };
    auto observe = [&](const wchar_t *title, const wchar_t *explain, int control = 0) {
        auto t = task({}, G::Observe, title, explain,
                      L"看清界面后再继续；每一步都有对应的真实操作与反馈。");
        t.observeControl = control;
        return t;
    };
    if (basics) {
        add(L"认识战场与胜负",
            {observe(
                L"你是座狼，陪练是重骑",
                L"绿标是你，红标是练习陪练。消灭敌方全部角色获胜；攻击会先扣护甲，再扣生命。正常对局隐藏敌方站位。")});
        add(L"移动与选格",
            {task(
                teach(Kind::Move, 8), G::Move, L"移动一步",
                L"点「移动 W」，再点金框格（2，3），最后点执行或按空格。选格不消耗行动；座狼每回合有3次行动。",
                L"你的位置已改变，已用行动增加1。普通移动的终点只向自己显示。")});
        add(L"攻击与伤害",
            {task(
                teach(Kind::Attack, 14), G::Hit, L"打中相邻陪练",
                L"点「撕咬 E」并选（3，4），按空格。射程1、伤害1；陪练初始有1层护甲，先观察护甲变化。",
                L"命中已由真实裁判确认。护甲优先承担伤害，空格攻击没有命中不能完成本步。")});
        add(L"阅读行动与反馈",
            {observe(
                 L"先阅读左侧战报",
                 L"绿色行讲谁做了什么，暖色行讲实际结果。点「上一回合 A」查看旧回合，停留期间不会被新播报拉回。",
                 int(Observation::PreviousLog)),
             observe(L"回到当前回合",
                     L"点「当前回合 F」恢复跟随最新反馈；A / D 切换回合，鼠标滚轮在战报区域滚动。",
                     int(Observation::CurrentLog))},
            14, 13, {teach(Kind::Attack, 14), teach(Kind::End), {2, Kind::End}});
        add(L"理解隐藏信息",
            {observe(
                L"红框不是敌人的当前位置",
                L"现在陪练站位隐藏。战报公开的是攻击格或瞬间探测结果，敌人可能已经移动；不要把攻击格当成当前站位。")},
            14, 13, {}, false);
        add(L"完成一个回合",
            {task(teach(Kind::Move, 8), G::Move, L"先走一步",
                  L"先移动到（2，3）。座狼还可以继续行动，也可以主动结束回合。",
                  L"已移动。现在练习把操作权交给下一位玩家。"),
             task(
                 teach(Kind::End), G::End, L"结束并等待交接",
                 L"点「结束回合 S」。陪练会结束自己的回合；全部反馈播放完，才重新轮到你。",
                 L"己方回合已经结束。等待陪练反馈完毕后再继续，教学与正式对局使用相同的播放锁。")});
        add(L"同格反击与对象伤害记录",
            {task(teach(Kind::Attack, 13), G::Hit, L"先攻击同格陪练",
                  L"所有玩家各行动一次为一轮。你与座狼陪练同格，先攻击自身格：伤害1，只伤敌人。",
                  L"陪练已记录1点直接攻击强度，你尚未受击。"),
             task(
                 teach(Kind::End), G::End, L"观察另一玩家的同格攻击",
                 L"结束回合。陪练会在同格反击你，造成1点伤害；双方各记受击强度，在各自回合开始时清空。",
                 L"陪练能够反击；同格其他对象不会因先前攻击获得免伤。")},
            13);
        lessons.back().enemyRole = Role::Wolf;
        lessons.back().enemyStrike = true;
        return lessons;
    }
    switch (role) {
    case Role::Sniper:
        add(L"绝影的距离与必杀",
            {task(teach(Kind::Attack, 16), G::Hit, L"射程3的绝影",
                  L"攻击（4，1）的陪练：射程3，命中即击杀，无视护甲。墙体和中途建筑会挡射线。",
                  L"陪练即使有护甲也被直接击杀。绝影不是普通的1点伤害。")},
            16);
        add(L"移动与攻击二选一",
            {task(teach(Kind::Move, 8), G::Move, L"移动消耗本回合普通行动",
                  L"移速1。移动后不能在本回合再用绝影；与座狼的多次行动不同。",
                  L"本回合普通行动已用；查看禁用的攻击按钮，下一回合才能再次攻击。")});
        break;
    case Role::Wolf:
        add(L"三次自由行动",
            {task(teach(Kind::Move, 14), G::Move, L"靠近目标",
                  L"座狼射程1、伤害1。每回合3次行动，可自由组合移动与攻击，先走到（3，4）。",
                  L"行动1完成，剩余2次。"),
             task(teach(Kind::Attack, 15), G::Hit, L"移动后撕咬",
                  L"攻击（3，5），观察陪练的护甲。近战撕咬不能摧毁建筑。",
                  L"行动2命中，先扣护甲。"),
             task(teach(Kind::Move, 9), G::Move, L"攻击后继续走",
                  L"用最后一次行动移动到（2，4）；每一步看完反馈再决定。",
                  L"本回合3次行动均已使用。")},
            15);
        add(L"同一对象不叠伤",
            {task(teach(Kind::Attack, 14), G::Hit, L"第一次撕咬",
                  L"伤害1。先打中相邻陪练，观察护甲或生命。", L"第一次攻击已造成伤害。"),
             task(
                 teach(Kind::Attack, 14), G::SameCell, L"再次攻击同一格",
                 L"所有玩家各行动一次为一轮；同一轮、同一受击对象只取最高直接攻击强度；同样的1点伤害不会再叠加。再攻击（3，4）验证。",
                 L"第二次仍消耗行动，但没有新增伤害。同一对象的连续直接攻击不能简单相加。")});
        break;
    case Role::Ink:
        add(L"霰弹的实际伤害",
            {task(
                teach(Kind::Attack, 14), G::Hit, L"一发霰弹",
                L"每发0.5远程伤害，射程为自身及周围8格，每回合最多3发；对建筑伤害翻倍。攻击（3，4）。",
                L"霰弹确实命中，生命或护甲减少0.5；伤害还会受目标减伤影响。")});
        add(L"移动穿插与对象最高伤害",
            {task(teach(Kind::Attack, 14), G::Hit, L"先打一发",
                  L"先攻击（3，4），然后换位。三发霰弹逐发结算，不用提前决定全部目标。",
                  L"第1发命中。"),
             task(teach(Kind::Move, 8), G::Move, L"攻击之间移动",
                  L"普通移动每回合最多1格；移动后射程以新的位置为中心。移动到（2，3）。",
                  L"移动完成，霰弹额度保留。"),
             task(
                 teach(Kind::Attack, 14), G::SameCell, L"换位不刷新对象伤害",
                 L"从新位置再攻击（3，4）。陪练当前受击周期已记录0.5直接攻击强度，相同伤害不会再次累加。",
                 L"弹数增加，但陪练没有再受伤；移动不会清除对象最高攻击强度记录。")});
        add(L"命中刷新冲刺",
            {task(
                 teach(Kind::Attack, 12), G::Hit, L"打出真实伤害",
                 L"本步已合法冲刺到（3，1），冲刺在冷却。攻击（3，2）陪练：伤到生命、护甲或临甲都会刷新冲刺。",
                 L"霰弹命中并造成伤害，冲刺冷却已经刷新为0。"),
             task(
                 teach(Kind::Dash, 13), G::Move, L"当回合再次冲刺",
                 L"点「冲刺 R」并选（3，3）。冲刺沿一个方向最多2格，遇障停下，不增加每回合3发的霰弹上限。",
                 L"刷新后成功再次冲刺；冲刺又进入冷却。")},
            12, 13, {teach(Kind::Dash, 11)});
        break;
    case Role::Shadow:
        add(L"附骨带动",
            {task(teach(Kind::ShadowMode, 0, -1, 0, 0, 1), G::Mechanism, L"先选择带动模式",
                  L"回合开始先选主动带动或指定跟随，两者互斥；带动会同步复制自己的移动。",
                  L"本回合已选择主动带动。"),
             task(teach(Kind::Lead, 8, 2), G::Mechanism, L"带动练习陪练",
                  L"指定重骑为对象，移动到（2，3）。随影移速2；复制移动若被墙阻挡会跳过该步。",
                  L"你与陪练均复制了一步移动；正式对局的带动反馈不会泄露敌方站位。")});
        add(L"指定跟随",
            {task(teach(Kind::ShadowMode, 0, -1, 0, 0, 2), G::Mechanism, L"先选跟随模式",
                  L"指定后仍可移动2格；关系在目标移动时生效，不消耗自己的行动。",
                  L"本回合已选择指定跟随。"),
             task(teach(Kind::Follow, 0, 2), G::Mechanism, L"建立跟随关系",
                  L"点「指定跟随」并选择重骑。人形隐狼免疫附骨，沉默会暂停关系。",
                  L"跟随关系已建立。"),
             task(teach(Kind::End), G::End, L"观看目标移动的复制",
                  L"结束回合，陪练会走一步。看战报与己方位置同步变化，再继续。",
                  L"陪练移动会由真实附骨规则复制；受阻时跳过，关系不显示目标的秘密站位。")});
        lessons.back().enemyWalk = true;
        add(L"边界环绕",
            {task(teach(Kind::ShadowMode, 0, -1, 0, 0, 2), G::Mechanism, L"选择本回合模式",
                  L"随影移动与攻击可以穿越地图边界，墙体仍能阻挡。先选择指定跟随模式。",
                  L"模式已选择。"),
             task(teach(Kind::Move, 15), G::Move, L"从左边界走到右边界",
                  L"你在（3，1），移动到（3，5）只需一步；左右边界相连。",
                  L"环绕移动成功，消耗1格移动额度。")},
            14, 11);
        break;
    case Role::Heavy:
        add(L"护甲与普通移动",
            {task(
                teach(Kind::Move, 8), G::Move, L"盾甲重骑",
                L"初始1层护甲，冲锋属于普攻，沉默时仍可使用。普通移动最多2格，与冲锋互斥；先移动到（2，3）。",
                L"普通移动后，本回合不能混入冲锋。")});
        add(L"冲锋分步判定",
            {task(teach(Kind::Charge, 14), G::Move, L"冲锋第一步",
                  L"冲锋最多4格，第一格没有伤害。先进入（3，4），第一步后允许转向一次。",
                  L"第一步只是推进，没有伤害。"),
             task(teach(Kind::Charge, 15), G::Hit, L"第二步造成伤害",
                  L"从第二格起各造成1点近战伤害。继续进入（3，5）撞击陪练，不能反向。",
                  L"第二步已经造成伤害；公开判定格会在冲锋结束时按坐标排序。"),
             task(teach(Kind::End), G::End, L"结束冲锋并阅读判定",
                  L"现在结束回合，查看全部伤害格。公开坐标顺序不代表经过顺序或最终站位。",
                  L"冲锋结束，攻击格公开，秘密路线保留。")},
            15);
        break;
    case Role::HiddenWolf:
        add(L"人形防护",
            {task(teach(Kind::Attack, 15), G::Hit, L"人形远程攻击",
                  L"人形移速1、射程2，远程减伤50%，免探测与附骨；移动和攻击二选一。攻击（3，5）。",
                  L"人形远程攻击命中。减伤与探测免疫属于自己的形态机制。")},
            15);
        add(L"狼形与本能",
            {task(teach(Kind::Transform, 1), G::Mechanism, L"行动前变狼",
                  L"每个己方回合可在行动前变身一次，无跨回合冷却。狼形射程1、移速2，近战减伤50%。",
                  L"狼形已生效，本回合不能再次变身，下个己方回合可再切换。"),
             task(
                 teach(Kind::Attack, 14), G::Hit, L"攻击并获得距离提示",
                 L"攻击（3，4）。狼形攻击后会给出攻击格到最近可探测敌人的距离，并获得额外1格本能移动。",
                 L"攻击命中；查看私人本能距离，不要把距离当成精确的敌人坐标。"),
             task(teach(Kind::Move, 8), G::Move, L"使用攻击后的本能移动",
                  L"攻击后再移动到（2，3）。这一步来自本能额度。", L"本能移动已完成。")});
        add(L"逐回合切换形态",
            {task(teach(Kind::Transform, 1), G::Mechanism, L"本回合先变为狼形",
                  L"每个己方回合开始可切换一次；切换后不能在本回合再次变身。",
                  L"狼形已生效，本回合变身额度已用。"),
             task(teach(Kind::End), G::End, L"交接并等待下个己方回合",
                  L"结束回合，陪练也会结束。下个己方回合开始，变身额度恢复，无需再等待冷却。",
                  L"本回合结束；陪练行动后重新获得变身额度。"),
             task(teach(Kind::Transform, 0), G::Mechanism, L"下一回合切回人形",
                  L"再次点切换形态。切回人形获得0.5临甲，不叠加，持续到下个己方回合开始。",
                  L"人形已恢复，获得0.5临甲；每回合仍只能切换一次。")},
            24);
        break;
    case Role::Knight:
        add(L"基础日字",
            {task(
                teach(Kind::Leap, 6), G::Move, L"检查马腿与落点",
                L"骑士移动为日字跳跃，倾轧属于普通攻击，沉默仍可使用。W选基础日字，检查马腿和落点。跳到（2，1）。",
                L"日字移动完成，消耗本回合基础行动。")});
        add(L"游猎、沉默、挥劈",
            {task(teach(Kind::Hunt, 6), G::Mechanism, L"公开倾轧落点",
                  L"游猎落点公开，倾轧伤害1，可原地施放。跳到（2，1），随后按①②③顺序最多选择两项。",
                  L"游猎落点已公开，后续选项开放。"),
             task(
                 teach(Kind::Silence), G::Mechanism, L"①沉默相邻目标",
                 L"沉默中心及上下左右1格，可穿墙；同时施加受伤＋0.5的易伤，持续至自己下回合开始；仅前两项冷却1回合。",
                 L"相邻陪练已经被沉默；普通移动和基础攻击仍保留。"),
             task(teach(Kind::Cleave, 7), G::Hit, L"②挥劈",
                  L"攻击（2，2），造成1点近战伤害。最多两项，因此本回合不能再选③秘密跳跃。",
                  L"挥劈命中，已使用两项游猎选项。")},
            7);
        add(L"③秘密日字与选项冷却",
            {task(teach(Kind::Hunt, 6), G::Mechanism, L"先游猎落到（2，1）",
                  L"游猎后可按①②③顺序选最多两项；这次跳过前两项，直接练习③。", L"强化游猎已开始。"),
             task(teach(Kind::Retreat, 13), G::Move, L"③跳回中心",
                  L"秘密日字不造成倾轧伤害，没有跨回合冷却。跳到（3，3），落点只向自己显示。",
                  L"秘密跳跃成功，该选项进入冷却；同一选项本回合不能重复使用。")},
            7);
        break;
    case Role::Alpha:
        add(L"嗜血的瞬间情报",
            {task(
                 teach(Kind::Blood), G::Mechanism, L"回合首个动作探测",
                 L"嗜血公开自己的瞬间位置，私下探测最近的可探测敌人。冷却2个己方回合，人形隐狼不会被探测。",
                 L"瞬间探测成功；情报不会持续追踪敌人。"),
             task(teach(Kind::Attack, 14), G::Hit, L"根据情报攻击",
                  L"移速2、近战射程1。攻击（3，4），伤害1，命中回复0.5血量；普通移动与攻击二选一。",
                  L"近战攻击命中，探测不占普通行动额度。")});
        add(L"攻击同时突进",
            {task(teach(Kind::AttackRush, 14), G::Hit, L"开启突进后攻击",
                  L"点「攻击突进 R」开启，再攻击（3，4）。同时进入攻击格；目标格必须可以站立。",
                  L"攻击造成伤害，自己也已进入攻击格。")});
        break;
    case Role::Armiger:
        add(L"负甲的代价",
            {task(teach(Kind::ArmorUp), G::Mechanism, L"增加一层护甲",
                  L"初始无甲。负甲占整个回合，上限2层；0/1/2层甲对应移速2/1/0、刺击距离1/2/3。",
                  L"护甲增加，回合结束；新的移速和刺击长度已生效。")});
        add(L"护甲决定刺击长度",
            {task(teach(Kind::Thrust, 14), G::Hit, L"选择方向，自动延长",
                  L"本步已穿1层甲。点相邻方向（3，4），直线延伸2格，命中（3，5）陪练，伤害1。",
                  L"刺击沿线命中，方向格不必是敌人的站位。")},
            15, 13, {teach(Kind::ArmorUp), {2, Kind::End}});
        add(L"卸甲恢复机动",
            {task(
                 teach(Kind::ArmorDown), G::Mechanism, L"卸掉一层甲",
                 L"回合开始、其他行动前可卸甲。卸掉1层甲后移速恢复2、刺击距离降为1；受击后2个己方回合不能负甲。",
                 L"护甲减少，机动与刺击距离同步改变。"),
             task(teach(Kind::Move, 14), G::Move, L"先移动再刺击",
                  L"玄甲士允许移动后刺击；现在移动到（3，4）。", L"移动完成，刺击额度仍在。")},
            15, 13, {teach(Kind::ArmorUp), {2, Kind::End}});
        break;
    case Role::Flamer:
        add(L"离火与喷射",
            {task(teach(Kind::Flame, 14), G::Hit, L"消耗一枚离火",
                  L"开局3枚离火。选择数量1，朝（3，4）喷火，长度1、每格1点远程伤害。没有普通攻击。",
                  L"火焰命中并消耗1枚离火；燃烧图标同步出现。"),
             task(teach(Kind::FlameAdvance, 14), G::Move, L"看完攻击再选入火",
                  L"攻击反馈完成后，可秘密进入任一合法燃烧格，或留在原地。现在进入（3，4）。",
                  L"进入火焰完成；祝融免疫持续燃烧，每回合最多进入一次。")});
        add(L"火步与点燃代价",
            {task(
                teach(Kind::FireStep, 8, -1, 0, 0, 0), G::Mechanism, L"消耗离火额外走一步",
                L"火步消耗1枚离火，不占普通行动。可开启点燃终点；点燃会取消本回合全部离火回复。先不点燃走到（2，3）。",
                L"火步移动成功，离火减少，普通行动仍在。")});
        add(L"点燃终点",
            {task(
                teach(Kind::FireStep, 8, -1, 0, 0, 1), G::Mechanism, L"开启点燃后火步",
                L"开启「点燃终点 Y」，火步到（2，3）的陪练格，伤害1、不伤自己；代价是本回合末不回复离火。",
                L"终点已点燃，取消本回合全部离火回复；持续燃烧在受影响玩家的回合末结算。")},
            8);
        add(L"离火数量决定火焰长度",
            {task(
                 teach(Kind::Flame, 14, -1, 0, 0, 2), G::Hit, L"消耗2枚直线喷射",
                 L"选数量2，点相邻方向（3，4），喷火延伸2格命中（3，5）。每格伤害1，每枚离火增加一格长度。",
                 L"喷射沿线命中，离火消耗2；攻击反馈完成后再选入火或留在原地。"),
             task(teach(Kind::FlameAdvance, 14), G::Move, L"选择火焰中的落点",
                  L"进入（3，4）；祝融免疫持续燃烧，在火中回合末回复2枚离火，仍受上限3约束。",
                  L"秘密入火完成。")},
            15);
        break;
    case Role::WallGuard:
        add(L"三名士兵独立行动",
            {task(teach(Kind::Move, 8), G::Move, L"士兵1先移动",
                  L"每名士兵0.8生命，每回合各行动一次，移速1、伤害1。先让士兵1移动到（2，3）。",
                  L"士兵1已行动，另外两名仍可行动。"),
             task(teach(Kind::BuildWall, 11, -1, 1), G::Mechanism, L"换士兵2建长城",
                  L"点「士兵2 [2]」，再点「建长城 C」，选（3，1）。只能建一格，消耗该士兵的行动。",
                  L"士兵2建造成功，长城生命1，阻挡移动与攻击；己方可以登墙。")});
        add(L"登墙与列阵",
            {task(
                teach(Kind::Move, 11), G::Move, L"士兵1登己方长城",
                L"本步已由士兵2在（3，1）建墙。移动士兵1到墙上；站墙射程2远程抛射，移速仍为1。",
                L"士兵1已登墙。八方向连通的士兵在不会增加死亡人数时自动平摊；绝影与燃烧不平摊。")},
            14, 12, {teach(Kind::BuildWall, 11, -1, 1)});
        break;
    case Role::Prince:
        add(L"玫瑰之吻",
            {task(
                teach(Kind::PlantRose, 14), G::Hit, L"种玫瑰并造成伤害",
                L"射程2，伤害0.5，玫瑰生命1.5，最多3朵。种植替代移动额度；可与单位或建筑同格。种在（3，4）。",
                L"玫瑰已种下并命中陪练，本回合普通移动额度已消耗。")});
        add(L"盛放的三次选格",
            {task(
                teach(Kind::Bloom, 14, -1, 0, 14, 1, 9), G::Mechanism, L"玫瑰 → 落点 → 攻击格",
                L"依次选（3，4）的玫瑰、落点（2，4）、攻击格（3，4），再执行。伤害1，玫瑰掉0.5生命花瓣；每朵每回合一次。",
                L"瞬移和攻击完成，玫瑰生命减少0.5；同一陪练从0.5提高到1，只补伤害差额。")},
            14, 13, {teach(Kind::PlantRose, 14)});
        break;
    case Role::Sword:
        add(L"剑刺与秘密跟进",
            {task(
                 teach(Kind::SwordStab, 14), G::Hit, L"选择相邻方向格",
                 L"剑气初始9。点（3，4）向右连续刺2格，伤害1、消耗1剑气；U切换包含自身：自身格加方向格，共两格。",
                 L"剑刺命中，剑气减少1。攻击反馈完成后再选择落点。"),
             task(teach(Kind::SwordAdvance, 14), G::Move, L"进入攻击格",
                  L"现在选（3，4）进入，也可用S留在原地。落点只向自己显示，外部只知道选择完成。",
                  L"秘密跟进完成；对手不会从公开战报得知选择的落点。")});
        add(L"包含自身的两格直刺",
            {task(
                 Action{0, Kind::SwordStab, 14, -1, 0, 0, 1}, G::Hit, L"切换包含自身",
                 L"点剑刺E，按U开启包含自身，再点相邻格（3，4）确定方向。攻击自身与方向格，共两格，仍只消耗1剑气。",
                 L"攻击范围包含自身与相邻格，只伤同格敌人，不会刺伤自己。"),
             task(teach(Kind::SwordAdvance, 14), G::Move, L"看完反馈后跟进",
                  L"选择（3，4）进入攻击格；后续落点仍只向己方显示。",
                  L"包含自身的剑刺与秘密跟进已完成。")},
            13);
        add(L"飞剑驻场",
            {task(
                teach(Kind::ThrowSword, 14), G::Mechanism, L"支付剑气并飞剑",
                L"飞剑占整回合，直线距离0—3；按距离支付剑气，原地为0，伤害1；放剑后可立即召回。先支付1飞到（3，4）。每柄驻场剑令剑气上限减1。",
                L"飞剑驻场，剑气上限变为8；回合末回复3，受动态上限约束。")});
        add(L"召回与额外普攻",
            {task(
                 teach(Kind::RecallSword, 14), G::Mechanism, L"召回场上的飞剑",
                 L"本步已在上一回合留剑。召回按当前距离支付剑气，不占回合，并增加一次额外普攻或移动机会。点（3，4）的飞剑。",
                 L"飞剑已召回，额外普攻额度增加1，上限恢复。"),
             task(teach(Kind::Move, 12), G::Move, L"召回后先移动",
                  L"先移动到（3，2）。普通移动会占基础普攻机会，但召回提供的额外普攻仍可以使用。",
                  L"普通移动完成，额外普攻额度保留。"),
             task(teach(Kind::SwordStab, 13), G::Hit, L"使用额外剑刺",
                  L"点相邻方向（3，3），连续刺到（3，4）的陪练。额外普攻仍消耗1剑气。",
                  L"移动后仍成功剑刺，召回提供的额外普攻额度已消耗。"),
             task(teach(Kind::SwordAdvance, 14), G::Move, L"看完反馈再进入",
                  L"进入（3，4），完成攻击后的秘密选择。", L"剑气与落点选择练习完成。")},
            14, 13,
            {teach(Kind::End),
             {2, Kind::ArmorUp},
             teach(Kind::End),
             {2, Kind::ArmorUp},
             teach(Kind::ThrowSword, 14),
             teach(Kind::End),
             {2, Kind::End}});
        lessons.back().enemyRole = Role::Armiger;
        break;
    }
    // A fresh scene repeats a meaningful combination without pointing at a fixed cell.
    Lesson practice = lessons[role == Role::Sword    ? 0
                              : role == Role::Heavy  ? 1
                              : role == Role::Knight ? 1
                              : role == Role::Ink    ? 2
                                                     : 0];
    practice.title = L"独立练习 · 完成组合";
    for (auto &t : practice.tasks) {
        t.independent = true;
        t.explain = L"独立完成「" + t.title +
                    L"」。不再指向具体格子；可随时按H查技能，卡住可看示范或重试。";
    }
    lessons.push_back(practice);
    return lessons;
}

double trainingDurability(const Unit &u) {
    return u.health + u.shield + u.armor * 2;
}

bool trainingOutcome(const LessonTask &t, const Action &a, const Snapshot &before,
                     const Snapshot &after) {
    if (a.kind != t.action.kind || a.unit != 0)
        return false;
    const auto &u = after.units[0];
    const auto &prior = before.units[0];
    double hurt = trainingDurability(before.units[2]) - trainingDurability(after.units[2]);
    switch (t.goal) {
    case LessonGoal::Observe:
        return false;
    case LessonGoal::Move:
        return soldierCell(u, a.piece) != soldierCell(prior, a.piece) &&
               (t.independent || soldierCell(u, a.piece) == t.action.target);
    case LessonGoal::Hit:
        return hurt > 1e-8 && (a.kind != Kind::AttackRush || u.position == a.target) &&
               (u.role != Role::Ink || prior.cooldown == 0 || u.cooldown == 0);
    case LessonGoal::SameCell:
        return a.target == t.action.target &&
               (before.units[2].directDamage > 0 ||
                std::any_of(before.units[2].soldierDirectDamage.begin(),
                            before.units[2].soldierDirectDamage.end(),
                            [](double x) { return x > 0; })) &&
               std::abs(hurt) < 1e-8;
    case LessonGoal::End:
        return after.playerTurns[0] > before.playerTurns[0];
    case LessonGoal::Mechanism:
        break;
    }
    switch (a.kind) {
    case Kind::ShadowMode:
        return after.budget.shadowMode == t.action.amount;
    case Kind::Follow:
        return u.follow == 2 && u.follow != prior.follow;
    case Kind::Lead:
        return u.position != prior.position && after.units[2].position != before.units[2].position;
    case Kind::Transform:
        return u.form != prior.form && u.form == t.action.target && after.budget.transformed &&
               u.cooldown == 0;
    case Kind::Hunt:
        return u.position == a.target && after.budget.phase == 3;
    case Kind::Silence:
        return after.units[2].silenced && !before.units[2].silenced;
    case Kind::Blood:
        return after.intel[0][before.units[2].position - 1] && u.cooldown > 0;
    case Kind::ArmorUp:
        return u.armor > prior.armor;
    case Kind::ArmorDown:
        return u.armor < prior.armor;
    case Kind::FireStep:
        return u.position != prior.position && u.ammo < prior.ammo &&
               (!t.action.amount || (u.ammoBlocked && after.fires[a.target - 1].expires > 0));
    case Kind::BuildWall:
        return a.piece == t.action.piece &&
               after.buildings[a.target - 1].kind == Structure::GreatWall &&
               after.buildings[a.target - 1].health > 0 &&
               before.buildings[a.target - 1].health == 0;
    case Kind::Bloom: {
        bool petal = false;
        for (const auto &rose : after.roses)
            for (const auto &old : before.roses)
                if (rose.id == old.id && rose.cell == a.extra && rose.health < old.health)
                    petal = true;
        return hurt > 0 && u.position == a.landing && petal;
    }
    case Kind::ThrowSword:
        return after.swords.size() > before.swords.size() && after.swords.back().cell == a.target;
    case Kind::RecallSword:
        return after.swords.size() < before.swords.size() &&
               after.budget.extraAttacks > before.budget.extraAttacks;
    default:
        return false;
    }
}

} // namespace

void Guide::recordAction(const Game &game, const Action &action) {
    if (state_.complete || state_.stage != GuideStage::Operate)
        return;
    auto &g = state_;
    const auto &snapshots = game.snapshots();
    if (snapshots.size() < 2)
        return;
    const auto &before = snapshots[snapshots.size() - 2];
    const auto &after = snapshots.back();
    const auto &t = currentTask();
    g.passed = trainingOutcome(t, action, before, after);
    bool continuation = true;
    if (g.passed && t.independent) {
        // Free choices must still leave a playable continuation. Otherwise retry
        // remains at the pre-action checkpoint instead of trapping the next lesson.
        Game trial = game;
        const auto &tasks = g.lessons[g.lesson].tasks;
        for (int i = g.task + 1; i < int(tasks.size()); ++i) {
            if (trial.finished()) {
                continuation = false;
                break;
            }
            while (inputPlayer(trial.state()) != 0) {
                if (!trial.act({inputPlayer(trial.state()) * 2, Kind::End})) {
                    continuation = false;
                    break;
                }
            }
            if (!continuation)
                break;
            Snapshot previous = trial.state();
            if (!trial.act(tasks[i].action) ||
                !trainingOutcome(tasks[i], tasks[i].action, previous, trial.state())) {
                continuation = false;
                break;
            }
        }
        g.passed = continuation;
    }
    g.stage = GuideStage::Feedback;
    g.outcome =
        (g.demo ? L"示范：" : L"") +
        std::wstring(
            g.passed
                ? t.result
                : L"动作已执行，但目标尚未达成。请对照战报查看是否命中、落点是否正确或参数是否匹配，再重试本步。");
    if (!continuation)
        g.outcome =
            L"这一步已执行，但当前位置无法衔接本课后续组合。请重试本步，选择能继续攻击或施放后续技能的位置。";
    double hurt = trainingDurability(before.units[2]) - trainingDurability(after.units[2]);
    if (hurt > 1e-8)
        g.outcome += L"\n本次陪练生命 / 护甲 / 临甲合计减少 " + halfPoints(hurt) + L"。";
    else if (t.goal == LessonGoal::Hit || t.goal == LessonGoal::SameCell)
        g.outcome += L"\n本次新增伤害：0。";
}

const LessonTask &Guide::currentTask() const {
    return state_.lessons.at(state_.lesson).tasks.at(state_.task);
}

int Guide::totalTasks() const {
    int total = 0;
    for (const auto &lesson : state_.lessons)
        total += int(lesson.tasks.size());
    return total;
}

void Guide::reset(Role role, bool basics) {
    state_ = GuideState{};
    role_ = basics ? Role::Wolf : role;
    state_.basics = basics;
    state_.lessons = tutorialCourse(role_, basics);
}

std::shared_ptr<Game> Guide::startLesson(int index) {
    auto &g = state_;
    if (index >= 0) {
        if (index >= int(g.lessons.size()))
            throw std::out_of_range("training lesson index");
        g.lesson = index;
    }
    const auto &lesson = g.lessons.at(g.lesson);
    g.task = 0;
    g.stage = GuideStage::Explain;
    g.passed = g.demo = g.complete = false;
    g.outcome.clear();
    Formations formations{};
    if (role_ == Role::WallGuard)
        formations[0] = {lesson.start, 12, 18};
    auto game = std::make_shared<Game>(
        std::array<int, 8>{lesson.start, lesson.start, lesson.enemy, lesson.enemy},
        std::array<Role, 8>{role_, role_, lesson.enemyRole, lesson.enemyRole}, Mode::Heroes, 0,
        Map{}, 1, formations, 2, false);
    for (const auto &action : lesson.setup)
        if (!game->act(action))
            throw std::logic_error("invalid training preparation");
    g.checkpoint = std::make_shared<Game>(*game);
    return game;
}

bool Guide::inputAllowed() const {
    return state_.stage == GuideStage::Operate && !state_.complete &&
           currentTask().goal != LessonGoal::Observe && state_.actionChosen;
}

void Guide::chooseAction(bool chosen) {
    state_.actionChosen = chosen;
}

void Guide::observe(Observation control, bool selectedAction) {
    if (state_.complete || state_.stage != GuideStage::Operate)
        return;
    const auto &task = currentTask();
    if (task.goal != LessonGoal::Observe && selectedAction)
        state_.actionChosen = true;
    if (task.goal == LessonGoal::Observe && task.observeControl == int(control) &&
        control != Observation::None) {
        state_.passed = true;
        state_.stage = GuideStage::Feedback;
        state_.outcome = task.result;
    }
}

bool Guide::canContinue() const {
    return !state_.lessons.empty() &&
           (state_.complete || state_.stage == GuideStage::Explain ||
            (state_.stage == GuideStage::Operate && currentTask().goal == LessonGoal::Observe &&
             !currentTask().observeControl) ||
            (state_.stage == GuideStage::Feedback && state_.passed));
}

Next Guide::continueWith(const Game &game) {
    auto &g = state_;
    if (!canContinue())
        return Next::None;
    if (g.complete)
        return Next::Leave;
    if (g.stage == GuideStage::Explain) {
        g.stage = GuideStage::Operate;
        return Next::Changed;
    }
    if (g.stage == GuideStage::Operate) {
        g.passed = true;
        g.stage = GuideStage::Feedback;
        g.outcome = currentTask().result;
        return Next::Changed;
    }
    if (g.demo)
        return Next::Retry;
    ++g.completedTasks;
    if (++g.task == int(g.lessons[g.lesson].tasks.size())) {
        g.task = 0;
        if (++g.lesson == int(g.lessons.size())) {
            g.lesson = int(g.lessons.size()) - 1;
            g.complete = true;
            return Next::Complete;
        }
        return Next::Lesson;
    }
    g.checkpoint = std::make_shared<Game>(game);
    g.stage = GuideStage::Explain;
    g.passed = false;
    g.outcome.clear();
    return Next::Task;
}

std::shared_ptr<Game> Guide::retry() {
    if (!state_.checkpoint || state_.complete)
        return {};
    state_.stage = GuideStage::Explain;
    state_.passed = state_.demo = false;
    state_.outcome.clear();
    return std::make_shared<Game>(*state_.checkpoint);
}

bool Guide::beginDemo() {
    if (state_.complete || state_.lessons.empty())
        return false;
    state_.stage = GuideStage::Operate;
    state_.demo = state_.actionChosen = true;
    if (currentTask().goal == LessonGoal::Observe) {
        state_.passed = true;
        state_.stage = GuideStage::Feedback;
        state_.outcome = L"示范：" + currentTask().result;
        return false;
    }
    return true;
}

Action Guide::enemyAction(const View &view) const {
    const auto &lesson = state_.lessons.at(state_.lesson);
    if (lesson.enemyStrike && view.budget.attacks == 0)
        return {2, Kind::Attack, lesson.enemy};
    if (lesson.enemyWalk && view.budget.moves == 0)
        return {2, Kind::Move, lesson.enemy - 5};
    return {2, Kind::End};
}

}
