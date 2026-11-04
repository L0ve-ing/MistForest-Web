#include "damage.hpp"
#include <iterator>
#include "game.hpp"   // 引入支持自定义地图的游戏规则声明。
#include <algorithm>  // 使用计数、最小值和最大值算法。
#include <cmath>      // 使用绝对值进行距离与线段计算。
#include <functional> // 使用递归函数枚举重骑的一至四步冲锋路线。
#include <iomanip>    // 格式化小数生命。
#include <sstream>    // 显示小数状态。
#include <stdexcept>  // 使用异常拒绝非法开局参数。

namespace mist { // 所有实现属于游戏规则命名空间。

// Cue offsets bind semantics to a public line without interpreting its wording.
void appendPublicCue(Event &e, const std::wstring &text, PublicEffect effect,
                     bool absorbed = false) {
    auto offset = e.publicText.size() + (!text.empty() && text.front() == L'\n' ? 1 : 0);
    e.publicCues.push_back({offset, effect, absorbed});
    e.publicText += text;
}

#include "rules/map_geometry.inc"
#include "rules/role_catalog.inc"
#include "rules/movement.inc"
#include "rules/role_actions.inc"
#include "rules/combat.inc"
#include "rules/validation.inc"
#include "rules/lifecycle.inc"
#include "rules/player_view.inc"
#include "rules/turn_actions.inc"
#include "rules/batch_replay.inc"
#include "rules/legal_actions.inc"
#include "ai/decision.inc"
} // 游戏命名空间结束。
