#pragma once

#include <string_view>
#include <vector>

struct RangeBar {
    std::string_view close_time;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    long volume = 0;
    long delta = 0;
};

void calculate_range_bars(std::string_view file_view,
                          std::vector<RangeBar>& bars,
                          double range_size);
