#include <algorithm>
#include <limits>
#include <string_view>
#include "ohlc.hpp"

std::string_view OhlcWindow::title() const noexcept { return "OHLC"; };
void OhlcWindow::render(WINDOW *w, std::string_view id, const std::vector<RangeBar> &bars, const TpoProfile &,
                        std::size_t offset, bool active) const
{
    frame(w, title(), id, active);
    if (too_small(w) || bars.empty())
    {
        if (bars.empty())
            mvwprintw(w, 2, 2, "No bars");
        wnoutrefresh(w);
        return;
    }
    const View v = view_of(w, bars.size(), offset, 13);
    if (v.first == v.last)
    {
        wnoutrefresh(w);
        return;
    }

    double lo = std::numeric_limits<double>::max();
    double hi = std::numeric_limits<double>::lowest();
    for (std::size_t i = v.first; i < v.last; ++i)
    {
        lo = std::min(lo, bars[i].low);
        hi = std::max(hi, bars[i].high);
    }
    const double span = std::max(0.01, hi - lo);
    const auto y_of = [&](double p)
    { return std::clamp(v.height - static_cast<int>((p - lo) / span * (v.height - 1)), 1, v.height); };

    wattron(w, COLOR_PAIR(4));
    for (int y = 1; y <= v.height; y += 3)
    {
        const double p = hi - static_cast<double>(y - 1) / std::max(1, v.height - 1) * span;
        mvwprintw(w, y, v.width + 1, "%10.2f", p);
    }
    wattroff(w, COLOR_PAIR(4));
    int x = 2;
    for (std::size_t i = v.first; i < v.last; ++i, ++x)
    {
        const auto &b = bars[i];
        const int c = b.close >= b.open ? 1 : 2;
        const int yo = y_of(b.open), yc = y_of(b.close), yh = y_of(b.high), yl = y_of(b.low);
        wattron(w, COLOR_PAIR(c));
        for (int y = yh; y <= yl; ++y)
            mvwaddch(w, y, x, ACS_VLINE);
        for (int y = std::min(yo, yc); y <= std::max(yo, yc); ++y)
            mvwaddch(w, y, x, ACS_CKBOARD);
        wattroff(w, COLOR_PAIR(c));
    }
    wnoutrefresh(w);
}