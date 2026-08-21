#include "core/WindowTree.hpp"
#include "core/engine.hpp"

#include <algorithm>
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
        std::vector<Tick> ticks;
        parse_ticks(input.view(), ticks);

        std::vector<RangeBar> bars;
        calculate_range_bars(ticks, bars, 2.0);

        TpoProfile tpo_profile;
        calculate_tpo_profile(
            ticks,
            tpo_profile,
            0.25, // Tick size, für ES/NQ/MES/MNQ passend
            30    // 30-Minuten-TPO-Brackets
        );

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
            render_tree(*root, bars, tpo_profile, offset, active);
            move(LINES - 1, 0);
            clrtoeol();
            mvprintw(
                LINES - 1,
                0,
                "[%s:%s] h/l scroll | +/- TPO zoom | Tab pane | "
                "v Delta | d delete | s TPO | 1/2/3 type | r session | q quit",
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
            if ((key == 'h' || key == 'H') && offset + 1 < bars.size())
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

            if (key == '\t')
            {
                std::vector<WindowNode *> leaves;
                collect_leaves(*root, leaves);
                auto it = std::find(leaves.begin(), leaves.end(), active);
                if (it != leaves.end())
                    active = leaves[(static_cast<std::size_t>(it - leaves.begin()) + 1) % leaves.size()];
                continue;
            }
            if (key == 'v' || key == 'V')
            {
                if (split_leaf(*active, SplitType::Vertical, std::make_unique<DeltaWindow>(), "pane" + std::to_string(++pane_number)))
                {
                    active = active->right.get();
                    dirty = true;
                }
                continue;
            }
            if (key == 's' || key == 'S')
            {
                if (split_leaf(*active, SplitType::Horizontal, std::make_unique<TpoWindow>(), "pane" + std::to_string(++pane_number)))
                {
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
                active->chart = std::make_unique<TpoWindow>();
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
