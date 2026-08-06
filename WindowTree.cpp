#include "WindowTree.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <utility>

namespace {

void frame(WINDOW* w, std::string_view title, std::string_view id, bool active) {
    werase(w);
    if (active) wattron(w, COLOR_PAIR(4) | A_BOLD);
    box(w, 0, 0);
    if (active) wattroff(w, COLOR_PAIR(4) | A_BOLD);
    mvwprintw(w, 0, 2, " %.*s:%.*s ", static_cast<int>(title.size()), title.data(),
              static_cast<int>(id.size()), id.data());
}

bool too_small(WINDOW* w) {
    int h, width; getmaxyx(w, h, width);
    if (h >= 6 && width >= 20) return false;
    mvwprintw(w, 1, 1, "Too small");
    return true;
}

struct View { std::size_t first, last; int width, height; };
View view_of(WINDOW* w, std::size_t count, std::size_t offset, int right_margin) {
    int h, width; getmaxyx(w, h, width);
    const int chart_width = std::max(1, width - right_margin);
    const std::size_t last = count - std::min(count, offset);
    const std::size_t shown = std::min(last, static_cast<std::size_t>(chart_width - 1));
    return {last - shown, last, chart_width, std::max(1, h - 3)};
}

} // namespace

WindowNode::~WindowNode() { if (ncurses_window) delwin(ncurses_window); }

std::string_view OhlcWindow::title() const noexcept { return "OHLC"; }
void OhlcWindow::render(WINDOW* w, std::string_view id, const std::vector<RangeBar>& bars,
                        std::size_t offset, bool active) const {
    frame(w, title(), id, active);
    if (too_small(w) || bars.empty()) { if (bars.empty()) mvwprintw(w,2,2,"No bars"); wnoutrefresh(w); return; }
    const View v = view_of(w, bars.size(), offset, 13);
    if (v.first == v.last) { wnoutrefresh(w); return; }

    double lo = std::numeric_limits<double>::max();
    double hi = std::numeric_limits<double>::lowest();
    for (std::size_t i=v.first; i<v.last; ++i) { lo=std::min(lo,bars[i].low); hi=std::max(hi,bars[i].high); }
    const double span = std::max(0.01, hi-lo);
    const auto y_of = [&](double p) { return std::clamp(v.height-static_cast<int>((p-lo)/span*(v.height-1)),1,v.height); };

    wattron(w, COLOR_PAIR(4));
    for (int y=1; y<=v.height; y+=3) {
        const double p=hi-static_cast<double>(y-1)/std::max(1,v.height-1)*span;
        mvwprintw(w,y,v.width+1,"%10.2f",p);
    }
    wattroff(w, COLOR_PAIR(4));
    int x=2;
    for (std::size_t i=v.first; i<v.last; ++i,++x) {
        const auto& b=bars[i]; const int c=b.close>=b.open ? 1 : 2;
        const int yo=y_of(b.open), yc=y_of(b.close), yh=y_of(b.high), yl=y_of(b.low);
        wattron(w,COLOR_PAIR(c));
        for(int y=yh;y<=yl;++y) mvwaddch(w,y,x,ACS_VLINE);
        for(int y=std::min(yo,yc);y<=std::max(yo,yc);++y) mvwaddch(w,y,x,ACS_CKBOARD);
        wattroff(w,COLOR_PAIR(c));
    }
    wnoutrefresh(w);
}

std::string_view DeltaWindow::title() const noexcept { return "DELTA"; }
void DeltaWindow::render(WINDOW* w, std::string_view id, const std::vector<RangeBar>& bars,
                         std::size_t offset, bool active) const {
    frame(w,title(),id,active);
    if (too_small(w) || bars.empty()) { if(bars.empty()) mvwprintw(w,2,2,"No bars"); wnoutrefresh(w); return; }
    const View v=view_of(w,bars.size(),offset,2);
    long max_delta=1; for(std::size_t i=v.first;i<v.last;++i) max_delta=std::max(max_delta,std::labs(bars[i].delta));
    const int base=v.height/2+1, usable=std::max(1,v.height/2-1);
    for(int x=1;x<v.width;++x) mvwaddch(w,base,x,ACS_HLINE);
    int x=2;
    for(std::size_t i=v.first;i<v.last;++i,++x) {
        const long d=bars[i].delta; const int n=std::max(1,static_cast<int>(std::labs(d)*static_cast<double>(usable)/max_delta));
        wattron(w,COLOR_PAIR(d>=0?1:2));
        for(int j=0;j<n;++j) mvwaddch(w,d>=0?base-1-j:base+1+j,x,ACS_CKBOARD);
        wattroff(w,COLOR_PAIR(d>=0?1:2));
    }
    wnoutrefresh(w);
}

std::string_view TpoWindow::title() const noexcept { return "TPO"; }
void TpoWindow::render(WINDOW* w, std::string_view id, const std::vector<RangeBar>&,
                       std::size_t, bool active) const {
    frame(w,title(),id,active); if(too_small(w)) { wnoutrefresh(w); return; }
    mvwprintw(w,2,2,"Session: %s",session_==Session::RTH?"RTH":"ETH");
    mvwprintw(w,4,2,"TPO profile: not implemented");
    mvwprintw(w,6,2,"[r] toggle RTH / ETH");
    wnoutrefresh(w);
}
bool TpoWindow::handle_key(int key) { if(key!='r' && key!='R') return false; session_=session_==Session::RTH?Session::ETH:Session::RTH; return true; }

void layout_tree(WindowNode& n,int y,int x,int h,int w) {
    if(n.is_leaf()) { if(n.ncurses_window) delwin(n.ncurses_window); n.ncurses_window=newwin(std::max(1,h),std::max(1,w),y,x); return; }
    if(n.split==SplitType::Vertical) { const int a=w/2; layout_tree(*n.left,y,x,h,a); layout_tree(*n.right,y,x+a,h,w-a); }
    else { const int a=h/2; layout_tree(*n.left,y,x,a,w); layout_tree(*n.right,y+a,x,h-a,w); }
}
void render_tree(WindowNode& n,const std::vector<RangeBar>& bars,std::size_t offset,const WindowNode* active) {
    if(!n.is_leaf()) { render_tree(*n.left,bars,offset,active); render_tree(*n.right,bars,offset,active); return; }
    n.chart->render(n.ncurses_window,n.id,bars,offset,&n==active);
}
void collect_leaves(WindowNode& n,std::vector<WindowNode*>& leaves) { if(n.is_leaf()) { leaves.push_back(&n); return; } collect_leaves(*n.left,leaves); collect_leaves(*n.right,leaves); }
bool split_leaf(WindowNode& leaf,SplitType split,std::unique_ptr<ChartWindow> chart,std::string id) {
    if(!leaf.is_leaf() || !chart) return false;
    leaf.split=split; leaf.left=std::make_unique<WindowNode>(); leaf.left->id=leaf.id; leaf.left->chart=std::move(leaf.chart);
    leaf.right=std::make_unique<WindowNode>(); leaf.right->id=std::move(id); leaf.right->chart=std::move(chart); return true;
}
namespace {

bool delete_leaf_recursive(
    std::unique_ptr<WindowNode>& node,
    WindowNode* target,
    WindowNode*& new_active_leaf
) {
    if (node == nullptr || node->is_leaf()) {
        return false;
    }

    // Target is the left child:
    if (node->left.get() == target) {
        new_active_leaf = node->right.get();

        // Promote the entire right sibling subtree.
        node = std::move(node->right);

        // If it is itself split, select its first visible pane.
        while (!new_active_leaf->is_leaf()) {
            new_active_leaf = new_active_leaf->left.get();
        }

        return true;
    }

    // Target is the right child:
    if (node->right.get() == target) {
        new_active_leaf = node->left.get();

        // Promote the entire left sibling subtree.
        node = std::move(node->left);

        // If it is itself split, select its first visible pane.
        while (!new_active_leaf->is_leaf()) {
            new_active_leaf = new_active_leaf->left.get();
        }

        return true;
    }

    if (delete_leaf_recursive(node->left, target, new_active_leaf)) {
        return true;
    }

    return delete_leaf_recursive(node->right, target, new_active_leaf);
}

} // namespace

bool delete_leaf(
    std::unique_ptr<WindowNode>& root,
    WindowNode* target,
    WindowNode*& new_active_leaf
) {
    if (root == nullptr || target == nullptr) {
        return false;
    }

    // Do not allow deletion of the final remaining pane.
    if (root.get() == target && root->is_leaf()) {
        return false;
    }

    return delete_leaf_recursive(root, target, new_active_leaf);
}
