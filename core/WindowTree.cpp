#include "WindowTree.hpp"
#include "config.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <utility>

void frame(WINDOW *w, std::string_view title, std::string_view id, bool active)
{
    werase(w);
    if (active)
        wattron(w, COLOR_PAIR(4) | A_BOLD);
    box(w, 0, 0);
    if (active)
        wattroff(w, COLOR_PAIR(4) | A_BOLD);
    mvwprintw(w, 0, 2, " %.*s:%.*s ", static_cast<int>(title.size()), title.data(),
              static_cast<int>(id.size()), id.data());
}

bool too_small(WINDOW *w)
{
    int h, width;
    getmaxyx(w, h, width);
    if (h >= 6 && width >= 20)
        return false;
    mvwprintw(w, 1, 1, "Too small");
    return true;
}

View view_of(WINDOW *w, std::size_t count, std::size_t offset, int right_margin)
{
    int h, width;
    getmaxyx(w, h, width);
    const int chart_width = std::max(1, width - right_margin);
    const std::size_t last = count - std::min(count, offset);
    const std::size_t shown = std::min(last, static_cast<std::size_t>(chart_width - 1));
    return {last - shown, last, chart_width, std::max(1, h - 3)};
}

WindowNode::~WindowNode()
{
    if (ncurses_window)
        delwin(ncurses_window);
}

void layout_tree(WindowNode &n, int y, int x, int h, int w)
{
    if (n.is_leaf())
    {
        if (n.ncurses_window)
            delwin(n.ncurses_window);
        n.ncurses_window = newwin(std::max(1, h), std::max(1, w), y, x);
        return;
    }
    if (n.split == SplitType::Vertical)
    {
        const int a = w / 2;
        layout_tree(*n.left, y, x, h, a);
        layout_tree(*n.right, y, x + a, h, w - a);
    }
    else
    {
        const int a = h / 2;
        layout_tree(*n.left, y, x, a, w);
        layout_tree(*n.right, y + a, x, h - a, w);
    }
}
void render_tree(
    WindowNode &node,
    const std::array<std::vector<RangeBar>, 3> &bars,
    const std::array<TpoProfile, 3> &profiles,
    std::size_t offset,
    const WindowNode *active)
{
    if (!node.is_leaf())
    {
        render_tree(*node.left, bars, profiles, offset, active);
        render_tree(*node.right, bars, profiles, offset, active);
        return;
    }

    node.chart->render(
        node.ncurses_window,
        node.id,
        bars[session_index(node.session)],
        profiles[session_index(node.session)],
        offset,
        &node == active);
}
void collect_leaves(WindowNode &n, std::vector<WindowNode *> &leaves)
{
    if (n.is_leaf())
    {
        leaves.push_back(&n);
        return;
    }
    collect_leaves(*n.left, leaves);
    collect_leaves(*n.right, leaves);
}
bool split_leaf(WindowNode &leaf, SplitType split, std::unique_ptr<ChartWindow> chart, std::string id)
{
    if (!leaf.is_leaf() || !chart)
        return false;
    leaf.split = split;
    leaf.left = std::make_unique<WindowNode>();
    leaf.left->id = leaf.id;
    leaf.left->chart = std::move(leaf.chart);
    leaf.right = std::make_unique<WindowNode>();
    leaf.right->id = std::move(id);
    leaf.right->chart = std::move(chart);
    return true;
}
namespace
{

    bool delete_leaf_recursive(
        std::unique_ptr<WindowNode> &node,
        WindowNode *target,
        WindowNode *&new_active_leaf)
    {
        if (node == nullptr || node->is_leaf())
        {
            return false;
        }

        // Target is the left child:
        if (node->left.get() == target)
        {
            new_active_leaf = node->right.get();

            // Promote the entire right sibling subtree.
            node = std::move(node->right);

            // If it is itself split, select its first visible pane.
            while (!new_active_leaf->is_leaf())
            {
                new_active_leaf = new_active_leaf->left.get();
            }

            return true;
        }

        // Target is the right child:
        if (node->right.get() == target)
        {
            new_active_leaf = node->left.get();

            // Promote the entire left sibling subtree.
            node = std::move(node->left);

            // If it is itself split, select its first visible pane.
            while (!new_active_leaf->is_leaf())
            {
                new_active_leaf = new_active_leaf->left.get();
            }

            return true;
        }

        if (delete_leaf_recursive(node->left, target, new_active_leaf))
        {
            return true;
        }

        return delete_leaf_recursive(node->right, target, new_active_leaf);
    }

} // namespace

bool delete_leaf(
    std::unique_ptr<WindowNode> &root,
    WindowNode *target,
    WindowNode *&new_active_leaf)
{
    if (root == nullptr || target == nullptr)
    {
        return false;
    }

    // Do not allow deletion of the final remaining pane.
    if (root.get() == target && root->is_leaf())
    {
        return false;
    }

    return delete_leaf_recursive(root, target, new_active_leaf);
}
