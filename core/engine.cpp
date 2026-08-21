#include "engine.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <map>
#include <numeric>
#include <system_error>

namespace
{
    template <typename T>
    bool parse_number(std::string_view text, T &value)
    {
        const auto [end, error] =
            std::from_chars(text.data(), text.data() + text.size(), value);

        return error == std::errc{} && end == text.data() + text.size();
    }

    bool parse_tick_line(std::string_view line, Tick &tick)
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.remove_suffix(1);
        }

        const std::size_t first = line.find(',');
        if (first == std::string_view::npos)
        {
            return false;
        }

        const std::size_t second = line.find(',', first + 1);
        if (second == std::string_view::npos)
        {
            return false;
        }

        const std::size_t third = line.find(',', second + 1);
        if (third == std::string_view::npos)
        {
            tick.time = {};

            return parse_number(line.substr(0, first), tick.price) &&
                   parse_number(
                       line.substr(first + 1, second - first - 1),
                       tick.volume) &&
                   parse_number(line.substr(second + 1), tick.aggressor) &&
                   tick.volume > 0 &&
                   (tick.aggressor == 1 || tick.aggressor == -1);
        }

        tick.time = line.substr(0, first);

        return parse_number(
                   line.substr(first + 1, second - first - 1),
                   tick.price) &&
               parse_number(
                   line.substr(second + 1, third - second - 1),
                   tick.volume) &&
               parse_number(line.substr(third + 1), tick.aggressor) &&
               tick.volume > 0 &&
               (tick.aggressor == 1 || tick.aggressor == -1);
    }

    int seconds_since_midnight(std::string_view time)
    {
        if (time.size() != 8 || time[2] != ':' || time[5] != ':')
        {
            return -1;
        }

        int hour = 0;
        int minute = 0;
        int second = 0;

        if (!parse_number(time.substr(0, 2), hour) ||
            !parse_number(time.substr(3, 2), minute) ||
            !parse_number(time.substr(6, 2), second))
        {
            return -1;
        }

        if (hour < 0 || hour > 23 ||
            minute < 0 || minute > 59 ||
            second < 0 || second > 59)
        {
            return -1;
        }

        return hour * 3600 + minute * 60 + second;
    }

    int64_t price_to_ticks(double price, double tick_size)
    {
        return static_cast<int64_t>(std::llround(price / tick_size));
    }

    double ticks_to_price(int64_t ticks, double tick_size)
    {
        return static_cast<double>(ticks) * tick_size;
    }
}

void parse_ticks(std::string_view file_view, std::vector<Tick> &ticks)
{
    ticks.clear();

    std::size_t begin = 0;

    while (begin < file_view.size())
    {
        const std::size_t end = file_view.find('\n', begin);
        const std::size_t length = end == std::string_view::npos
                                       ? file_view.size() - begin
                                       : end - begin;

        const std::string_view line = file_view.substr(begin, length);

        begin = end == std::string_view::npos
                    ? file_view.size()
                    : end + 1;

        Tick tick;

        if (parse_tick_line(line, tick))
        {
            ticks.push_back(tick);
        }
    }
}

void calculate_range_bars(
    const std::vector<Tick> &ticks,
    std::vector<RangeBar> &bars,
    double range_size)
{
    bars.clear();

    if (range_size <= 0.0)
    {
        return;
    }

    RangeBar current{};
    bool has_current = false;

    for (const Tick &tick : ticks)
    {
        if (!has_current)
        {
            current = RangeBar{
                tick.time,
                tick.price,
                tick.price,
                tick.price,
                tick.price,
                tick.volume,
                tick.aggressor * tick.volume};

            has_current = true;
            continue;
        }

        current.high = std::max(current.high, tick.price);
        current.low = std::min(current.low, tick.price);
        current.close = tick.price;
        current.close_time = tick.time;
        current.volume += tick.volume;
        current.delta += tick.aggressor * tick.volume;

        if (current.high - current.low >= range_size)
        {
            bars.push_back(current);

            current = RangeBar{
                tick.time,
                tick.price,
                tick.price,
                tick.price,
                tick.price,
                0,
                0};
        }
    }

    if (has_current && current.volume > 0)
    {
        bars.push_back(current);
    }
}

void calculate_tpo_profile(
    const std::vector<Tick> &ticks,
    TpoProfile &profile,
    double tick_size,
    int bracket_minutes)
{
    profile = {};

    if (ticks.empty() || tick_size <= 0.0 || bracket_minutes <= 0)
    {
        return;
    }

    const int first_second = seconds_since_midnight(ticks.front().time);

    if (first_second < 0)
    {
        return;
    }

    const int bracket_seconds = bracket_minutes * 60;
    std::map<int64_t, TpoLevel> levels_by_price;

    for (const Tick &tick : ticks)
    {
        const int current_second = seconds_since_midnight(tick.time);

        if (current_second < 0)
        {
            continue;
        }

        int elapsed = current_second - first_second;

        // Unterstützt Daten, die über Mitternacht laufen.
        if (elapsed < 0)
        {
            elapsed += 24 * 60 * 60;
        }

        const int period = elapsed / bracket_seconds;
        const int64_t price_ticks = price_to_ticks(tick.price, tick_size);

        TpoLevel &level = levels_by_price[price_ticks];

        if (level.periods.empty())
        {
            level.price = ticks_to_price(price_ticks, tick_size);
        }

        if (static_cast<int>(level.periods.size()) <= period)
        {
            level.periods.resize(static_cast<std::size_t>(period + 1), false);
        }

        level.periods[static_cast<std::size_t>(period)] = true;
        level.volume += tick.volume;

        profile.period_count = std::max(profile.period_count, period + 1);
    }

    if (levels_by_price.empty())
    {
        return;
    }

    profile.levels.reserve(levels_by_price.size());

    for (auto &[_, level] : levels_by_price)
    {
        if (static_cast<int>(level.periods.size()) < profile.period_count)
        {
            level.periods.resize(
                static_cast<std::size_t>(profile.period_count),
                false);
        }

        profile.levels.push_back(std::move(level));
    }

    std::sort(
        profile.levels.begin(),
        profile.levels.end(),
        [](const TpoLevel &left, const TpoLevel &right)
        {
            return left.price > right.price;
        });

    const auto tpo_count = [](const TpoLevel &level)
    {
        return static_cast<long>(
            std::count(level.periods.begin(), level.periods.end(), true));
    };

    const auto poc_it = std::max_element(
        profile.levels.begin(),
        profile.levels.end(),
        [&](const TpoLevel &left, const TpoLevel &right)
        {
            const long left_count = tpo_count(left);
            const long right_count = tpo_count(right);

            if (left_count != right_count)
            {
                return left_count < right_count;
            }

            // Bei Gleichstand: das nähere Level zur Profilmitte.
            return std::abs(left.price) > std::abs(right.price);
        });

    const std::size_t poc_index =
        static_cast<std::size_t>(poc_it - profile.levels.begin());

    profile.poc = profile.levels[poc_index].price;

    long total_tpos = 0;

    for (const TpoLevel &level : profile.levels)
    {
        total_tpos += tpo_count(level);
    }

    const long target_tpos = static_cast<long>(
        std::ceil(static_cast<double>(total_tpos) * 0.70));

    std::size_t high = poc_index;
    std::size_t low = poc_index;
    long included_tpos = tpo_count(profile.levels[poc_index]);

    while (included_tpos < target_tpos &&
           (high > 0 || low + 1 < profile.levels.size()))
    {
        const long above = high > 0
                               ? tpo_count(profile.levels[high - 1])
                               : -1;

        const long below = low + 1 < profile.levels.size()
                               ? tpo_count(profile.levels[low + 1])
                               : -1;

        if (above >= below && high > 0)
        {
            --high;
            included_tpos += tpo_count(profile.levels[high]);
        }
        else if (low + 1 < profile.levels.size())
        {
            ++low;
            included_tpos += tpo_count(profile.levels[low]);
        }
        else
        {
            break;
        }
    }

    profile.vah = profile.levels[high].price;
    profile.val = profile.levels[low].price;
}