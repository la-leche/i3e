#pragma once
#include "../../core/WindowTree.hpp"

class TpoWindow final : public ChartWindow
{
public:
    explicit TpoWindow(Session session = Session::ETH)
        : session_(session)
    {
    }

    [[nodiscard]] std::string_view title() const noexcept override;
    void set_session(Session session) noexcept { session_ = session; }

    void render(
        WINDOW *,
        std::string_view,
        const std::vector<RangeBar> &,
        const TpoProfile &,
        std::size_t,
        bool) const override;

    bool handle_key(int key) override;

private:
    Session session_;
    std::size_t scroll_offset_ = 0;

    // Anzahl originaler Tick-Levels, die in einer Display-Zeile landen.
    // 1 = maximale Detailansicht; 2, 4, 8 ... = Zoom-out.
    int ticks_per_row_ = 1;
};