#include "core/WindowTree.hpp"
#include "core/config.hpp"
#include "core/engine.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <ncurses.h>
#include "charts/ohlc/ohlc.hpp"
#include "charts/delta/delta.hpp"
#include "charts/tpo/tpo.hpp"

class MappedFile
{
public:
    explicit MappedFile(const char *path)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);

        if (!file)
        {
            throw std::runtime_error("Cannot open ticks.csv");
        }

        const std::streamsize file_size = file.tellg();

        if (file_size <= 0)
        {
            throw std::runtime_error("Invalid ticks.csv");
        }

        data_.resize(static_cast<std::size_t>(file_size));

        file.seekg(0, std::ios::beg);

        if (!file.read(data_.data(), file_size))
        {
            throw std::runtime_error("Cannot read ticks.csv");
        }
    }

    [[nodiscard]] std::string_view view() const
    {
        return {data_.data(), data_.size()};
    }

private:
    std::vector<char> data_;
};

bool confirm_quit()
{
    move(LINES - 1, 0);
    clrtoeol();

    wattron(stdscr, COLOR_PAIR(2) | A_BOLD);
    mvprintw(
        LINES - 1,
        0,
        "Quit i3trade? [y] yes, any other key cancels");
    wattroff(stdscr, COLOR_PAIR(2) | A_BOLD);

    refresh();

    const int answer = getch();

    return answer == 'y' || answer == 'Y';
}

bool confirm_delete(const WindowNode &pane)
{
    move(LINES - 1, 0);
    clrtoeol();

    wattron(stdscr, COLOR_PAIR(2) | A_BOLD);

    mvprintw(
        LINES - 1,
        0,
        "Delete %s:%s? [y] yes, any other key cancels",
        pane.chart->title().data(),
        pane.id.c_str());

    wattroff(stdscr, COLOR_PAIR(2) | A_BOLD);

    refresh();

    const int answer = getch();

    return answer == 'y' || answer == 'Y';
}

int main()
{
    try
    {
        MappedFile input{"ticks.csv"};
        const AppConfig config = load_config("config.ini");
        std::vector<Tick> ticks;
        parse_ticks(input.view(), ticks);

        const std::array<SessionWindow, 3> session_windows{
            config.rth,
            config.overnight,
            config.eth};
        std::array<std::vector<RangeBar>, 3> bars;
        std::array<TpoProfile, 3> tpo_profiles;
        for (std::size_t index = 0; index < session_windows.size(); ++index)
        {
            std::vector<Tick> session_ticks;
            for (const Tick &tick : ticks)
            {
                if (tick_in_session(tick, session_windows[index]))
                    session_ticks.push_back(tick);
            }
            calculate_range_bars(session_ticks, bars[index], 2.0);
            calculate_tpo_profile(session_ticks, tpo_profiles[index], 0.25, 30);
        }

        initscr();
        cbreak();
        noecho();
        keypad(stdscr, TRUE);
        curs_set(0);
        start_color();
        use_default_colors();
        init_pair(1, COLOR_GREEN, -1);
        init_pair(2, COLOR_RED, -1);
        init_pair(4, COLOR_CYAN, -1);

        auto root = std::make_unique<WindowNode>();
        root->id = "main";
        root->session = config.active_session;
        root->chart = std::make_unique<OhlcWindow>();
        WindowNode *active = root.get();
        std::size_t offset = 0;
        int pane_number = 1;
        bool dirty = true;

        while (true)
        {
            if (dirty)
            {
                int h, w;
                getmaxyx(stdscr, h, w);
                layout_tree(*root, 0, 0, h - 1, w);
                dirty = false;
            }
            render_tree(*root, bars, tpo_profiles, offset, active);
            move(LINES - 1, 0);
            clrtoeol();
            mvprintw(
                LINES - 1,
                0,
                "[%s:%s] h/l scroll | +/- TPO zoom | Tab pane | "
                "r Right | d delete | b Below | 1/2/3 type | s session | q quit",
                active->chart->title().data(),
                active->id.c_str());
            wnoutrefresh(stdscr);
            doupdate();
            const int key = getch();

            if (key == 'q' || key == 'Q')
            {
                if (confirm_quit())
                {
                    break;
                }

                continue;
            }
            if (key == KEY_RESIZE)
            {
                dirty = true;
                continue;
            }
            const auto &active_bars = bars[session_index(active->session)];
            if ((key == 'h' || key == 'H') && offset + 1 < active_bars.size())
            {
                ++offset;
                continue;
            }
            if ((key == 'l' || key == 'L') && offset > 0)
            {
                --offset;
                continue;
            }

            if (active->chart->handle_key(key))
            {
                continue;
            }

            if (key == 's' || key == 'S')
            {
                active->session = next_session(active->session);
                offset = 0;
                if (auto *tpo = dynamic_cast<TpoWindow *>(active->chart.get()))
                    tpo->set_session(active->session);
                continue;
            }

            if (key == '\t')
            {
                std::vector<WindowNode *> leaves;
                collect_leaves(*root, leaves);
                auto it = std::find(leaves.begin(), leaves.end(), active);
                if (it != leaves.end())
                    active = leaves[(static_cast<std::size_t>(it - leaves.begin()) + 1) % leaves.size()];
                continue;
            }
            if (key == 'r' || key == 'R')
            {
                if (split_leaf(*active, SplitType::Vertical, std::make_unique<DeltaWindow>(), "pane" + std::to_string(++pane_number)))
                {
                    active->right->session = active->session;
                    active = active->right.get();
                    dirty = true;
                }
                continue;
            }
            if (key == 'b' || key == 'B')
            {
                if (split_leaf(*active, SplitType::Horizontal, std::make_unique<TpoWindow>(config.active_session), "pane" + std::to_string(++pane_number)))
                {
                    active->right->session = active->session;
                    active->right->chart = std::make_unique<TpoWindow>(active->right->session);
                    active = active->right.get();
                    dirty = true;
                }
                continue;
            }
            if (key == '1')
            {
                active->chart = std::make_unique<OhlcWindow>();
                continue;
            }
            if (key == '2')
            {
                active->chart = std::make_unique<DeltaWindow>();
                continue;
            }
            if (key == '3')
            {
                active->chart = std::make_unique<TpoWindow>(active->session);
                continue;
            }
            if (key == 'd' || key == 'D')
            {
                if (confirm_delete(*active))
                {
                    WindowNode *next_active = nullptr;

                    if (delete_leaf(root, active, next_active))
                    {
                        active = next_active;
                        dirty = true;
                    }
                }

                continue;
            }
            active->chart->handle_key(key);
        }
        endwin();
    }
    catch (const std::exception &e)
    {
        std::fprintf(stderr, "Error: %s\n", e.what());
        return EXIT_FAILURE;
    }
}
