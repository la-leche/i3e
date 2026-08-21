#pragma once

#include <array>
#include <string_view>
#include <vector>

struct Tick
{
    std::string_view time; // Erwartet HH:MM:SS
    double price = 0.0;
    long volume = 0;
    int aggressor = 0;
};

struct RangeBar
{
    std::string_view close_time;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    long volume = 0;
    long delta = 0;
};

struct TpoLevel
{
    double price = 0.0;
    std::vector<bool> periods;
    long volume = 0;
};

struct TpoProfile
{
    std::vector<TpoLevel> levels; // Absteigend: höchster Preis zuerst
    double poc = 0.0;
    double vah = 0.0;
    double val = 0.0;
    int period_count = 0;
};

void parse_ticks(std::string_view file_view, std::vector<Tick> &ticks);

void calculate_range_bars(
    const std::vector<Tick> &ticks,
    std::vector<RangeBar> &bars,
    double range_size);

void calculate_tpo_profile(
    const std::vector<Tick> &ticks,
    TpoProfile &profile,
    double tick_size = 0.25,
    int bracket_minutes = 30);