#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct Tick
{
    std::chrono::system_clock::time_point timestamp;
    int64_t priceTicks;
    int64_t volume;
};

struct TPOConfig
{
    int64_t tickSize = 1; // z. B. 25 = 0.25 bei price * 100
    std::chrono::minutes bracket{30};
    double valueAreaFraction = 0.70;
};

struct TPOLevel
{
    int64_t priceTicks{};
    std::vector<bool> periods;
    int64_t volume{};
};

class TPOProfile
{
public:
    explicit TPOProfile(TPOConfig config = {});

    void build(const std::vector<Tick> &ticks);

    [[nodiscard]] const std::map<int64_t, TPOLevel> &levels() const;
    [[nodiscard]] int64_t poc() const;
    [[nodiscard]] int64_t valueAreaHigh() const;
    [[nodiscard]] int64_t valueAreaLow() const;
    [[nodiscard]] std::string lettersAt(int64_t priceTicks) const;

private:
    TPOConfig config_;
    std::map<int64_t, TPOLevel> levels_;

    int64_t poc_ = 0;
    int64_t vah_ = 0;
    int64_t val_ = 0;

    void calculatePoc();
    void calculateValueArea();
};