#pragma once
#include "../../core/WindowTree.hpp"

class DeltaWindow final : public ChartWindow
{
public:
    [[nodiscard]] std::string_view title() const noexcept override;
    void render(
        WINDOW *,
        std::string_view,
        const std::vector<RangeBar> &,
        const TpoProfile &,
        std::size_t,
        bool) const override;
};