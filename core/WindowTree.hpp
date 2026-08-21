#pragma once

#include "engine.hpp"

#include <cstddef>
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
    ETH
};

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

class OhlcWindow final : public ChartWindow
{
public:
    [[nodiscard]] std::string_view title() const noexcept override;
    void render(
        WINDOW *,
        std::string_view,
        const std::vector<RangeBar> &,
        const TpoProfile &,
        std::size_t,
        bool) const override;
};

class DeltaWindow final : public ChartWindow
{
public:
    [[nodiscard]] std::string_view title() const noexcept override;
    void render(
        WINDOW *,
        std::string_view,
        const std::vector<RangeBar> &,
        const TpoProfile &,
        std::size_t,
        bool) const override;
};

class TpoWindow final : public ChartWindow
{
public:
    explicit TpoWindow(Session session = Session::ETH)
        : session_(session)
    {
    }

    [[nodiscard]] std::string_view title() const noexcept override;

    void render(
        WINDOW *,
        std::string_view,
        const std::vector<RangeBar> &,
        const TpoProfile &,
        std::size_t,
        bool) const override;

    bool handle_key(int key) override;

private:
    Session session_;
    std::size_t scroll_offset_ = 0;

    // Anzahl originaler Tick-Levels, die in einer Display-Zeile landen.
    // 1 = maximale Detailansicht; 2, 4, 8 ... = Zoom-out.
    int ticks_per_row_ = 1;
};

struct WindowNode
{
    std::string id;
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
    const std::vector<RangeBar> &bars,
    const TpoProfile &profile,
    std::size_t offset,
    const WindowNode *active_leaf);
void collect_leaves(WindowNode &node, std::vector<WindowNode *> &leaves);
bool split_leaf(WindowNode &leaf, SplitType split,
                std::unique_ptr<ChartWindow> new_chart, std::string new_id);
bool delete_leaf(
    std::unique_ptr<WindowNode> &root,
    WindowNode *target,
    WindowNode *&new_active_leaf);
