// Buttons, shortcuts and wheel input all arrive through command(id).
bool actionSelectionCommand(int id) {
    Kind kind;
    if (commandKind(id, kind)) {
        s.choice.selectKind(hasView() ? currentView() : View{}, kind);
        return true;
    }
    using mist::selection::Modifier;
    switch (id) {
    case BatchToggle:
        s.choice.setBatching(!s.choice.state().batching);
        break;
    case BatchClear:
        s.choice.clearPlan();
        break;
    case IncludeSelf:
        s.choice.toggle(Modifier::IncludeSelf);
        break;
    case RushToggle:
        s.choice.toggle(Modifier::Rush);
        break;
    case IgniteToggle:
        s.choice.toggle(Modifier::Ignite);
        break;
    case Lunge:
        s.choice.toggle(Modifier::Lunge);
        break;
    case EnemyPiece:
        s.choice.cycleSubjectPiece();
        break;
    case CostDown:
        s.choice.selectAmount(std::max(1, s.choice.state().amount - 1));
        break;
    case CostUp:
        s.choice.selectAmount(std::min(9, s.choice.state().amount + 1));
        break;
    case Amount1:
    case Amount2:
    case Amount3:
        s.choice.selectAmount(s.choice.amountForSlot(currentView(), id - Amount1));
        break;
    case Piece0:
    case Piece1:
    case Piece2:
        s.choice.selectPiece(id - Piece0);
        break;
    case Enemy0:
    case Enemy1:
        s.choice.selectSubject((1 - s.viewer) * 2 + id - Enemy0);
        break;
    default:
        return false;
    }
    return true;
}
