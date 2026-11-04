#include "map_editor.hpp"
#include <algorithm>

namespace mist::editor {
void MapEditor::begin(const Map &initial) {
    if (!started_) {
        map_ = initial;
        started_ = true;
    }
}

void MapEditor::remember() {
    if (undo_.size() >= 100)
        undo_.erase(undo_.begin());
    undo_.push_back(map_);
}

void MapEditor::replace(const Map &map) {
    remember();
    map_ = map;
}

bool MapEditor::paintCell(int cell) {
    if (cell < 1 || cell > map_.width * map_.height || brush_ == Brush::Line)
        return false;
    Terrain terrain = brush_ == Brush::Wall        ? Terrain::Wall
                      : brush_ == Brush::Lake      ? Terrain::Lake
                      : brush_ == Brush::Whirlpool ? Terrain::Whirlpool
                                                  : Terrain::Ground;
    if (map_.cells[cell - 1] == terrain)
        return false;
    remember();
    map_.cells[cell - 1] = terrain;
    return true;
}

bool MapEditor::paintEdge(bool horizontal, int index) {
    if (index < 0 || index >= 90 || (brush_ != Brush::Line && brush_ != Brush::Erase))
        return false;
    remember();
    auto &edge = horizontal ? map_.horizontal[index] : map_.vertical[index];
    edge = brush_ == Brush::Line ? !edge : false;
    return true;
}

bool MapEditor::undo() {
    if (undo_.empty())
        return false;
    map_ = undo_.back();
    undo_.pop_back();
    return true;
}

void MapEditor::clear() {
    Map next;
    next.width = map_.width;
    next.height = map_.height;
    replace(next);
}

void MapEditor::reset() {
    replace(defaultMap(Mode::Heroes));
}

bool MapEditor::resize(int width, int height) {
    if (width < 1 || width > 9 || height < 1 || height > 9)
        return false;
    Map next;
    next.width = width;
    next.height = height;
    int overlapWidth = std::min(width, map_.width), overlapHeight = std::min(height, map_.height);
    for (int row = 0; row < overlapHeight; ++row)
        for (int column = 0; column < overlapWidth; ++column)
            next.cells[row * width + column] = map_.cells[row * map_.width + column];
    for (int row = 0; row <= overlapHeight; ++row)
        for (int column = 0; column < overlapWidth; ++column)
            next.horizontal[row * width + column] = map_.horizontal[row * map_.width + column];
    for (int row = 0; row < overlapHeight; ++row)
        for (int column = 0; column <= overlapWidth; ++column)
            next.vertical[row * (width + 1) + column] = map_.vertical[row * (map_.width + 1) + column];
    replace(next);
    return true;
}

void MapEditor::generate(unsigned seed) {
    replace(randomMap(map_.width, map_.height, seed));
    seed_ = seed;
}
} // namespace mist::editor
