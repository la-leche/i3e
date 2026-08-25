#include <algorithm>
#include <cmath>
#include <string_view>
#include "tpo.hpp"
#include "../../core/config.hpp"

std::string_view TpoWindow::title() const noexcept
{
    return "TPO";
}

void TpoWindow::render(
    WINDOW *w,
    std::string_view id,
    const std::vector<RangeBar> &,
    const TpoProfile &profile,
    std::size_t,
    bool active) const
{
    frame(w, title(), id, active);

    if (too_small(w))
    {
        wnoutrefresh(w);
        return;
    }

    if (profile.levels.empty())
    {
        mvwprintw(w, 2, 2, "No valid timestamped ticks");
        wnoutrefresh(w);
        return;
    }

    int height = 0;
    int width = 0;
    getmaxyx(w, height, width);

    constexpr int left = 2;
    constexpr int price_width = 11;

    const int available_rows = std::max(1, height - 5);
    const int available_letters =
        std::max(1, width - left - price_width - 2);

    const std::size_t group_size =
        static_cast<std::size_t>(ticks_per_row_);

    const std::size_t total_rows =
        (profile.levels.size() + group_size - 1) / group_size;

    const std::size_t maximum_offset =
        total_rows > static_cast<std::size_t>(available_rows)
            ? total_rows - static_cast<std::size_t>(available_rows)
            : 0;

    const std::size_t offset =
        std::min(scroll_offset_, maximum_offset);

    wattron(w, COLOR_PAIR(4) | A_BOLD);

    mvwprintw(
        w,
        1,
        left,
        "%s | POC %.2f | VAH %.2f | VAL %.2f",
        session_name(session_),
        profile.poc,
        profile.vah,
        profile.val);

    wattroff(w, COLOR_PAIR(4) | A_BOLD);

    const std::size_t first_row = offset;
    const std::size_t last_row = std::min(
        total_rows,
        first_row + static_cast<std::size_t>(available_rows));

    for (std::size_t row = first_row; row < last_row; ++row)
    {
        const std::size_t first_level = row * group_size;
        const std::size_t last_level = std::min(
            profile.levels.size(),
            first_level + group_size);

        const int y = 2 + static_cast<int>(row - first_row);

        const TpoLevel &top = profile.levels[first_level];
        const TpoLevel &bottom = profile.levels[last_level - 1];

        bool contains_poc = false;
        bool contains_vah = false;
        bool contains_val = false;

        std::vector<bool> merged_periods(
            static_cast<std::size_t>(profile.period_count),
            false);

        for (std::size_t level_index = first_level;
             level_index < last_level;
             ++level_index)
        {
            const TpoLevel &level = profile.levels[level_index];

            contains_poc = contains_poc ||
                           std::abs(level.price - profile.poc) < 0.0001;

            contains_vah = contains_vah ||
                           std::abs(level.price - profile.vah) < 0.0001;

            contains_val = contains_val ||
                           std::abs(level.price - profile.val) < 0.0001;

            for (std::size_t period = 0;
                 period < level.periods.size();
                 ++period)
            {
                merged_periods[period] =
                    merged_periods[period] || level.periods[period];
            }
        }

        if (contains_poc)
        {
            wattron(w, COLOR_PAIR(1) | A_BOLD);
        }
        else if (contains_vah || contains_val)
        {
            wattron(w, COLOR_PAIR(4) | A_BOLD);
        }

        if (group_size == 1)
        {
            mvwprintw(w, y, left, "%10.2f ", top.price);
        }
        else
        {
            mvwprintw(
                w,
                y,
                left,
                "%5.2f-%5.2f ",
                top.price,
                bottom.price);
        }

        int x = left + price_width;

        for (std::size_t period = 0;
             period < merged_periods.size() &&
             x < left + price_width + available_letters;
             ++period)
        {
            if (!merged_periods[period])
            {
                continue;
            }

            const char tpo_letter = period < 26
                                        ? static_cast<char>('A' + period)
                                        : '+';

            mvwaddch(w, y, x++, tpo_letter);
        }

        if (contains_poc)
        {
            wattroff(w, COLOR_PAIR(1) | A_BOLD);
        }
        else if (contains_vah || contains_val)
        {
            wattroff(w, COLOR_PAIR(4) | A_BOLD);
        }
    }

    mvwprintw(
        w,
        height - 2,
        left,
        "h/l scroll | +/- zoom | %d ticks/row | %zu/%zu",
        ticks_per_row_,
        offset + 1,
        maximum_offset + 1);

    wnoutrefresh(w);
}

bool TpoWindow::handle_key(int key)
{
    switch (key)
    {
    case 'h':
    case 'H':
        ++scroll_offset_;
        return true;

    case 'l':
    case 'L':
        if (scroll_offset_ > 0)
        {
            --scroll_offset_;
        }
        return true;

    case '+':
    case '=': // Auf deutscher Tastatur oft nötig für Plus ohne Shift-Probleme.
        if (ticks_per_row_ > 1)
        {
            ticks_per_row_ /= 2;
        }
        return true;

    case '-':
    case '_':
        if (ticks_per_row_ < 32)
        {
            ticks_per_row_ *= 2;
        }
        return true;

    case 's':
    case 'S':
        session_ = session_ == Session::RTH
                       ? Session::ETH
                       : Session::RTH;
        return true;

    default:
        return false;
    }
}