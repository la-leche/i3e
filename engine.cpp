#include "engine.hpp"

#include <charconv>
#include <system_error>

namespace
{

    struct Tick
    {
        std::string_view time;
        double price = 0.0;
        long volume = 0;
        int aggressor = 0;
    };

    template <typename T>
    bool parse_number(std::string_view text, T &value)
    {
        const auto [end, error] = std::from_chars(
            text.data(), text.data() + text.size(), value);
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
            return false;

        const std::size_t second = line.find(',', first + 1);
        if (second == std::string_view::npos)
            return false;

        const std::size_t third = line.find(',', second + 1);
        if (third == std::string_view::npos)
        {
            // Supports: price,volume,aggressor
            tick.time = {};
            return parse_number(line.substr(0, first), tick.price) && parse_number(line.substr(first + 1, second - first - 1), tick.volume) && parse_number(line.substr(second + 1), tick.aggressor) && tick.volume > 0 && (tick.aggressor == 1 || tick.aggressor == -1);
        }

        // Supports: timestamp,price,volume,aggressor
        tick.time = line.substr(0, first);
        return parse_number(line.substr(first + 1, second - first - 1), tick.price) && parse_number(line.substr(second + 1, third - second - 1), tick.volume) && parse_number(line.substr(third + 1), tick.aggressor) && tick.volume > 0 && (tick.aggressor == 1 || tick.aggressor == -1);
    }

} // namespace

void calculate_range_bars(std::string_view file_view,
                          std::vector<RangeBar> &bars,
                          double range_size)
{
    bars.clear();
    if (range_size <= 0.0)
        return;

    RangeBar current{};
    bool has_current = false;
    std::size_t begin = 0;

    while (begin < file_view.size())
    {
        const std::size_t end = file_view.find('\n', begin);
        const std::size_t length = (end == std::string_view::npos)
                                       ? file_view.size() - begin
                                       : end - begin;
        const std::string_view line = file_view.substr(begin, length);
        begin = (end == std::string_view::npos) ? file_view.size() : end + 1;

        Tick tick;
        if (!parse_tick_line(line, tick))
            continue;

        if (!has_current)
        {
            current = RangeBar{tick.time, tick.price, tick.price, tick.price,
                               tick.price, tick.volume, tick.aggressor * tick.volume};
            has_current = true;
            continue;
        }

        current.high = (tick.price > current.high) ? tick.price : current.high;
        current.low = (tick.price < current.low) ? tick.price : current.low;
        current.close = tick.price;
        current.close_time = tick.time;
        current.volume += tick.volume;
        current.delta += tick.aggressor * tick.volume;

        if (current.high - current.low >= range_size)
        {
            bars.push_back(current);
            current = RangeBar{tick.time, tick.price, tick.price, tick.price,
                               tick.price, 0, 0};
        }
    }

    if (has_current && current.volume > 0)
        bars.push_back(current);
}
