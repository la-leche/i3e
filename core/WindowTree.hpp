#pragma once

#include "engine.hpp"

#include <cstddef>
#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <ncurses.h>

enum class SplitType
{
    None,
    Horizontal,
    Vertical
};
enum class Session
{
    RTH,
    Overnight,
    ETH
};

struct View
{
    std::size_t first;
    std::size_t last;
    int width;
    int height;
};

void frame(WINDOW *window, std::string_view title, std::string_view id, bool active);
bool too_small(WINDOW *window);
View view_of(WINDOW *window, std::size_t count, std::size_t offset, int right_margin);

class ChartWindow
{
public:
    virtual ~ChartWindow() = default;
    [[nodiscard]] virtual std::string_view title() const noexcept = 0;
    virtual void render(
        WINDOW *window,
        std::string_view pane_id,
        const std::vector<RangeBar> &bars,
        const TpoProfile &profile,
        std::size_t offset,
        bool active) const = 0;
    virtual bool handle_key(int) { return false; }
};

struct WindowNode
{
    std::string id;
    Session session = Session::ETH;
    SplitType split = SplitType::None;
    WINDOW *ncurses_window = nullptr;
    std::unique_ptr<ChartWindow> chart;
    std::unique_ptr<WindowNode> left;
    std::unique_ptr<WindowNode> right;

    [[nodiscard]] bool is_leaf() const noexcept { return split == SplitType::None; }
    ~WindowNode();
};

void layout_tree(WindowNode &node, int y, int x, int height, int width);
void render_tree(
    WindowNode &node,
    const std::array<std::vector<RangeBar>, 3> &bars,
    const std::array<TpoProfile, 3> &profiles,
    std::size_t offset,
    const WindowNode *active_leaf);
void collect_leaves(WindowNode &node, std::vector<WindowNode *> &leaves);
bool split_leaf(WindowNode &leaf, SplitType split,
                std::unique_ptr<ChartWindow> new_chart, std::string new_id);
bool delete_leaf(
    std::unique_ptr<WindowNode> &root,
    WindowNode *target,
    WindowNode *&new_active_leaf);
