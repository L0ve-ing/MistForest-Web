#pragma once
#include "../game.hpp"

namespace mist::editor {
enum class Brush { Ground, Wall, Lake, Whirlpool, Line, Erase };

// Platform-independent document operations. UI/file dialogs only present the result.
class MapEditor {
  public:
    const Map &map() const { return map_; }
    Brush brush() const { return brush_; }
    bool started() const { return started_; }
    std::size_t undoDepth() const { return undo_.size(); }
    unsigned seed() const { return seed_; }

    void begin(const Map &initial);
    void selectBrush(Brush brush) { brush_ = brush; }
    void replace(const Map &map);
    bool paintCell(int cell);
    bool paintEdge(bool horizontal, int index);
    bool undo();
    void clear();
    void reset();
    bool resize(int width, int height);
    void generate(unsigned seed);

  private:
    Map map_ = defaultMap(Mode::Heroes);
    Brush brush_ = Brush::Wall;
    bool started_ = false;
    unsigned seed_ = 1;
    std::vector<Map> undo_;
    void remember();
};
} // namespace mist::editor
