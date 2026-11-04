void cover(HDC dc) {
    card(dc, sideRect(-7, 200, 740, 510));
    text(dc, L"秘密视角交接", sideRect(47, 250, 632, 55), 32, INK, true, DT_CENTER);
    text(dc, L"请把屏幕交给" + playerName(s.viewer), sideRect(47, 355, 632, 55), 29, TEAL, true,
         DT_CENTER);
    text(dc, L"其他玩家请暂时回避。", sideRect(47, 446, 632, 40), 21, MUTED, false, DT_CENTER);
    button(dc, Ready, L"我已准备好", sideRect(123, 565, 480, 65), true, true, 24);
} // 不显示上一位玩家的位置或报告。

std::wstring stats(const Unit &u) {
    return roleName(u.role) + (u.role == Role::HiddenWolf ? (u.form ? L"·狼" : L"·人") : L"") +
           L"  " + (u.health > 0 ? L"生命" + halfPoints(u.health) : L"阵亡") +
           (u.armor ? L" 甲" + halfPoints(u.armor * 2) : L"") +
           (u.shield ? L" 临甲" + halfPoints(u.shield) : L"") +
           (u.temporaryHealth ? L" 临时血量" + halfPoints(u.temporaryHealth) : L"");
} // 受伤与公开阵亡状态直接显示兵种。

bool hasView() {
    return s.online ? s.net && s.net->hasView() : s.game != nullptr;
} // 选点阶段不能读不存在的裁判。

bool canKind(const View &v, Kind kind) {
    return s.choice.canKind(v, kind);
}

std::wstring statusText(const View &v) {
    const auto &u = v.units[s.choice.state().unit];
    const auto &b = v.budget;
    std::wstring out =
        L"已移动" + std::to_wstring(b.moves) + L" / 已攻击" + std::to_wstring(b.attacks);
    if (u.silenced || (u.role == Role::WallGuard && u.soldierSilenced[s.choice.state().piece]))
        out += u.role == Role::Shadow ? L" · 附骨暂停至下回合开始" : L" · 沉默";
    if (u.vulnerable || (u.role == Role::WallGuard && u.soldierVulnerable[s.choice.state().piece]))
        out += L" · 易伤＋0.5";
    if (u.cooldown)
        out += L" · 冷却" + std::to_wstring(u.cooldown);
    if (u.role == Role::Knight)
        out += L"\n①②③冷却：" + std::to_wstring(u.optionCooldown[0]) + L" / " +
               std::to_wstring(u.optionCooldown[1]) + L" / " + std::to_wstring(u.optionCooldown[2]);
    if (u.role == Role::Flamer)
        out += L" · 离火" + std::to_wstring(u.ammo) + L"/3";
    if (u.role == Role::Flamer && u.ammoBlocked)
        out += L"\n本回合结束不回复离火";
    if (u.role == Role::HiddenWolf && u.form && u.sense != -2)
        out += u.sense < 0 ? L"\n本能：无可探测目标"
                           : L"\n本能：攻击格距最近敌人" + std::to_wstring(u.sense);
    if (u.role == Role::Ink)
        out += L" · 霰弹" + std::to_wstring(b.pellets) + L"/" + std::to_wstring(inkPelletLimit(b));
    if (u.role == Role::Sword)
        out += L" · 剑气" + std::to_wstring(u.swordQi) + L" · 额外行动" +
               std::to_wstring(b.extraAttacks);
    if (u.role == Role::Armiger)
        out += L" · 移速" + std::to_wstring(2 - int(std::ceil(u.armor - 1e-9))) + L" / 刺击" +
               std::to_wstring(1 + int(std::ceil(u.armor - 1e-9))) + L" / 负甲锁定" +
               std::to_wstring(u.armorLock);
    if (u.role == Role::WallGuard) {
        out = L"士兵" + std::to_wstring(s.choice.state().piece + 1) + L"：" +
              (b.squadDone & (1 << s.choice.state().piece) ? L"已行动" : L"可行动");
        out += L" · 待行动：";
        for (int k = 0; k < 3; ++k)
            if (u.soldierHealth[k] > 0 && !(b.squadDone & (1 << k)))
                out += std::to_wstring(k + 1) + L" ";
        out += L"\n每人选一项行动；建造仅一格长城。";
    }
    if (u.whirlImmune)
        out += L"\n下一轮免疫漩涡";
    return out;
} // 状态面板只显示本人数据。

bool parameterAvailable(const View &v, int amount) {
    return s.choice.parameterAvailable(v, amount);
}

#include "operations.inc"

void battle(HDC dc) { // 双角色、单步动作和大号判定结果共用一页。
    if (s.tutorial && !s.online) {
        tutorialBattle(dc);
        return;
    }
    if (s.phase == Phase::Cover) {
        cover(dc);
        return;
    }
    card(dc, rect(438, 94, 596, 801), RGB(253, 250, 238));
    card(dc, sideRect(638, 94, 446, 801), RGB(253, 250, 238));
    bool setup = s.phase == Phase::Setup, has = hasView();
    View v;
    if (has) {
        syncPresentation(liveView());
        v = displayedView();
    }
    feedPanel(dc);
    Map map = battleMap();
    auto units = deploymentBoardUnits(s.mode, map);
    if (has && !setup)
        units = v.units;
    bool done = s.phase == Phase::Finished && !presentationBusy();
    auto replay = s.online && s.net ? s.net->replay() : s.game;
    if (done && replay)
        units = replay->state().units;
    text(dc,
         setup  ? L"布置你的秘密阵容"
         : done ? L"对局结束 · 全部位置公开"
                : playerName(s.viewer) + L"的战场",
         rect(454, 111, 560, 37), 25, INK, true);
    text(dc, modeName(s.mode) + L" · 坐标为（行，列）", rect(454, 159, 560, 25), 15, MUTED);
    board(dc, s.mode, map, units, done);
    text(dc,
         setup ? L"半透明角色与虚线框：待确认落点；确认后显示完整角色。"
         : has && std::any_of(s.playback.state().enemySeen.begin(),
                              s.playback.state().enemySeen.end(), [](bool x) { return x; })
             ? L"红框：敌方最近回合攻击格，不表示当前站位。"
             : L"直接伤害取最高；在受击方回合开始重置，燃烧另算。",
         rect(454, 803, 560, 44), 14, MUTED); // 棋盘说明明确新交互。
    if (setup) {
        text(dc, L"布置第" + std::to_wstring(s.setupSlot + 1) + L"名角色",
             sideRect(662, 144, 396, 38), 25, TEAL, true);
        if (heroMode(s.mode)) {
            for (int i = 0; i < 13; ++i) {
                bool allowed = s.setupSlot == 0 || s.roles[s.viewer * 2] != Role(i);
                button(dc, RoleBase + i, roleName(Role(i)),
                       sideRect(664 + (i % 3) * 134, 202 + (i / 3) * 47, 125, 38),
                       s.selectedRole == Role(i), allowed, i == 10 ? 15 : 18);
            }
            characterIllustration(dc, sideRect(664, 446, 142, 116), s.selectedRole);
            roleText(dc, roleStats(s.selectedRole) + L"\n" + skillName(s.selectedRole),
                     sideRect(822, 446, 234, 116), 14);
        } else
            text(dc, L"每回合移动、攻击或侦察。\n选择一个秘密起点，允许同格。",
                 sideRect(664, 234, 390, 130), 21, MUTED);
        bool guardSetup = heroMode(s.mode) && s.selectedRole == Role::WallGuard;
        int setupId = s.viewer * 2 + s.setupSlot;
        if (guardSetup) {
            for (int k = 0; k < 3; ++k)
                button(dc, SetupPiece0 + k,
                       L"士兵" + std::to_wstring(k + 1) +
                           (s.formations[setupId][k] ? coordinate(s.mode, s.formations[setupId][k])
                                                     : L"·待选"),
                       sideRect(664 + k * 134, 572, 125, 31), s.setupPiece == k, true, 14);
            text(dc,
                 L"点击地图设置士兵" + std::to_wstring(s.setupPiece + 1) +
                     L"的位置，可点上方切换修改。",
                 sideRect(664, 610, 393, 32), 16, TEAL);
        } else
            text(dc,
                 s.choice.state().selected
                     ? L"起点：" + coordinate(s.mode, s.choice.state().selected)
                     : L"请点击中央起点格。",
                 sideRect(664, 585, 390, 34), 22, INK, true);
        button(dc, Commit, guardSetup ? L"确认三名士兵" : L"确认这个角色",
               sideRect(664, 646, 393, 53), true,
               guardSetup
                   ? validFormation(s.mode, map, s.formations[setupId][0], s.formations[setupId])
                   : s.choice.state().selected > 0,
               23);
    } // 两名角色依次选择且不能同职业。
    else if (s.phase == Phase::Error) {
        text(dc, L"连接中断", sideRect(664, 165, 390, 50), 29, CORAL, true);
        text(dc, s.net ? s.net->status() : L"无法继续。", sideRect(664, 247, 390, 140), 20, MUTED);
        button(dc, Review, L"查看中断记录", sideRect(664, 430, 393, 50), true,
               s.net && !s.net->history().empty());
        button(dc, GiveUp, L"无法重连 · 终止并完整复盘", sideRect(664, 500, 393, 50), false,
               s.net && s.net->phase() == forest::Phase::Recovering, 17);
        button(dc, Online, L"返回联机房间", sideRect(664, 575, 393, 50));
    } // 断线不继续接受本地动作。
    else if (s.phase == Phase::Finished && presentationBusy()) {
        text(dc, L"正在显示最终判定…", sideRect(664, 177, 390, 100), 28, TEAL, true);
    } else if (done) {
        text(dc,
             v.winner == 4 ? L"本局未决"
             : v.teams     ? L"阵营" + std::to_wstring(v.winner + 1) + L"获胜"
                           : playerName(v.winner) + L"获胜",
             sideRect(664, 165, 390, 60), 32, TEAL, true);
        text(dc, L"消灭敌方全部角色才获胜。\n可在赛后复盘查看完整移动与技能结算。",
             sideRect(664, 260, 390, 120), 21, MUTED);
        if (s.series && !s.online && s.playerCount == 2)
            text(dc,
                 L"三局两胜：" + std::to_wstring(s.score[0]) + L" : " + std::to_wstring(s.score[1]),
                 sideRect(664, 399, 393, 42), 23, TEAL, true);
        button(dc, Review, L"逐步复盘", sideRect(664, 463, 393, 58), true);
        button(dc, Again, L"再来一局 · 交换先手", sideRect(664, 548, 393, 58), false,
               !s.online || (s.net->isHost() && s.net->connected()));
    } // 获胜时才揭示敌方位置。
    else if (!has) {
        text(dc, L"等待对方布置阵容…", sideRect(664, 177, 390, 60), 25, INK, true);
    } // 只提交一方不提前公开敌方选角。
    else {
        battleOperations(dc, v);
    }
    footer(
        dc,
        presentationBusy()
            ? L"行动、特效与反馈同步播放 · 完成后才可行动 · H 查看己方技能"
            : L"Q / 角色区滚轮换人 · 参数区滚轮调整 · 空格执行 · A / D / F 浏览战报"); // 三栏布局底部只保留简短说明。
} // 对战页面结束。
