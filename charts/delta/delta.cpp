#include <algorithm>
#include <cstdlib>
#include <string_view>
#include "delta.hpp"

std::string_view DeltaWindow::title() const noexcept { return "DELTA"; }
void DeltaWindow::render(WINDOW *w, std::string_view id, const std::vector<RangeBar> &bars, const TpoProfile &,
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
    const View v = view_of(w, bars.size(), offset, 2);
    long max_delta = 1;
    for (std::size_t i = v.first; i < v.last; ++i)
        max_delta = std::max(max_delta, std::labs(bars[i].delta));
    const int base = v.height / 2 + 1, usable = std::max(1, v.height / 2 - 1);
    for (int x = 1; x < v.width; ++x)
        mvwaddch(w, base, x, ACS_HLINE);
    int x = 2;
    for (std::size_t i = v.first; i < v.last; ++i, ++x)
    {
        const long d = bars[i].delta;
        const int n = std::max(1, static_cast<int>(std::labs(d) * static_cast<double>(usable) / max_delta));
        wattron(w, COLOR_PAIR(d >= 0 ? 1 : 2));
        for (int j = 0; j < n; ++j)
            mvwaddch(w, d >= 0 ? base - 1 - j : base + 1 + j, x, ACS_CKBOARD);
        wattroff(w, COLOR_PAIR(d >= 0 ? 1 : 2));
    }
    wnoutrefresh(w);
}