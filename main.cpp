#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <string_view>
#include <charconv>
#include <ncurses.h>
#include <cmath>
#include <memory>
#include <string>

// --- БАЗОВЫЕ СТРУКТУРЫ ДАННЫХ ---
struct Tick
{
    std::string_view time_str;
    double price;
    long volume;
    int aggressor;
};

struct RangeBar
{
    std::string_view close_time;
    double open;
    double high;
    double low;
    double close;
    long volume = 0;
    long delta = 0;
};

enum class SplitType
{
    NONE,
    HORIZONTAL,
    VERTICAL
};
enum class WinType
{
    OHLC,
    DELTA,
    TPO
};

struct WindowNode
{
    std::string id; // Уникальное имя среды для переключения контекста
    SplitType split = SplitType::NONE;
    WinType type = WinType::OHLC;
    WINDOW *ncurses_win = nullptr;

    int y = 0, x = 0, h = 0, w = 0;

    std::shared_ptr<WindowNode> left = nullptr;
    std::shared_ptr<WindowNode> right = nullptr;

    ~WindowNode()
    {
        if (ncurses_win)
            delwin(ncurses_win);
    }
};

// Специфичные настройки контекстов (для демонстрации концепта ETH/RTH)
std::string current_session = "ETH";

Tick parse_tick_line(std::string_view line)
{
    Tick tick{"00:00:00", 0.0, 0, 0};
    size_t first_comma = line.find(',');
    if (first_comma == std::string_view::npos)
        return tick;
    tick.time_str = line.substr(0, first_comma);
    size_t second_comma = line.find(',', first_comma + 1);
    size_t third_comma = line.find(',', second_comma + 1);
    if (second_comma == std::string_view::npos || third_comma == std::string_view::npos)
        return tick;

    std::string_view p_str = line.substr(first_comma + 1, second_comma - first_comma - 1);
    std::string_view v_str = line.substr(second_comma + 1, third_comma - second_comma - 1);
    std::string_view a_str = line.substr(third_comma + 1);

    std::from_chars(p_str.data(), p_str.data() + p_str.size(), tick.price);
    std::from_chars(v_str.data(), v_str.data() + v_str.size(), tick.volume);
    std::from_chars(a_str.data(), a_str.data() + a_str.size(), tick.aggressor);
    return tick;
}

void calculate_range_bars(std::string_view file_view, std::vector<RangeBar> &bars, double range_size)
{
    bars.clear();
    RangeBar current_bar;
    bool first_tick = true;
    size_t pos = 0, prev = 0;

    while ((pos = file_view.find('\n', prev)) != std::string_view::npos)
    {
        std::string_view line = file_view.substr(prev, pos - prev);
        prev = pos + 1;
        if (line.empty())
            continue;

        Tick tick = parse_tick_line(line);
        if (tick.volume == 0)
            continue;

        if (first_tick)
        {
            current_bar.open = current_bar.high = current_bar.low = current_bar.close = tick.price;
            current_bar.close_time = tick.time_str;
            first_tick = false;
        }

        current_bar.volume += tick.volume;
        current_bar.delta += (tick.aggressor * tick.volume);
        if (tick.price > current_bar.high)
            current_bar.high = tick.price;
        if (tick.price < current_bar.low)
            current_bar.low = tick.price;
        current_bar.close = tick.price;
        current_bar.close_time = tick.time_str;

        if ((current_bar.high - current_bar.low) >= range_size)
        {
            bars.push_back(current_bar);
            RangeBar next_bar;
            next_bar.open = next_bar.high = next_bar.low = next_bar.close = current_bar.close;
            next_bar.close_time = tick.time_str;
            current_bar = next_bar;
        }
    }
    if (!first_tick)
        bars.push_back(current_bar);
}

// --- ОТРИСОВКА С УЧЕТОМ АКТИВНОГО КОНТЕНТА И КУРСОРНОЙ ПОДСВЕТКИ ---
void render_window_content(std::shared_ptr<WindowNode> node, std::shared_ptr<WindowNode> active_node, const std::vector<RangeBar> &bars, size_t offset)
{
    if (node->split != SplitType::NONE)
    {
        render_window_content(node->left, active_node, bars, offset);
        render_window_content(node->right, active_node, bars, offset);
        return;
    }

    WINDOW *w = node->ncurses_win;
    werase(w);

    // Если окно активно — подсвечиваем рамку (Цвет кубиков/шкал, например Cyan, или кастомный)
    bool is_active = (node == active_node);
    if (is_active)
    {
        wattron(w, COLOR_PAIR(4) | A_BOLD); // Синий/Циан бокс для активного контекста
    }
    box(w, 0, 0);
    if (is_active)
    {
        wattroff(w, COLOR_PAIR(4) | A_BOLD);
    }

    int chart_width = node->w - 14;
    int chart_height = node->h - 3;

    if (chart_width <= 5 || chart_height <= 3)
    {
        mvwprintw(w, 1, 1, "Too small");
        wrefresh(w);
        return;
    }

    int start_bar = bars.size() - chart_width - offset;
    if (start_bar < 0)
        start_bar = 0;

    if (node->type == WinType::OHLC)
    {
        mvwprintw(w, 0, 2, " [OHLC: %s] ", node->id.c_str());
        if (is_active)
            mvwprintw(w, 0, node->w - 12, " *ACTIVE* ");

        double min_p = 999999.0, max_p = -999999.0;
        for (size_t i = start_bar; i < bars.size() - offset; ++i)
        {
            if (bars[i].high > max_p)
                max_p = bars[i].high;
            if (bars[i].low < min_p)
                min_p = bars[i].low;
        }
        double price_range = (max_p - min_p == 0) ? 1.0 : (max_p - min_p);

        wattron(w, COLOR_PAIR(4));
        for (int y = 1; y <= chart_height; y += 3)
        {
            double price_at_y = max_p - ((double)(y - 1) / (chart_height - 1)) * price_range;
            mvwprintw(w, y, node->w - 12, "│ %0.2f", price_at_y);
        }
        wattroff(w, COLOR_PAIR(4));

        int x_pos = 2;
        for (size_t i = start_bar; i < bars.size() - offset && x_pos < chart_width + 2; ++i)
        {
            const auto &bar = bars[i];
            int color = (bar.close >= bar.open) ? 1 : 2;

            int y_open = chart_height - (int)(((bar.open - min_p) / price_range) * (chart_height - 1)) + 1;
            int y_close = chart_height - (int)(((bar.close - min_p) / price_range) * (chart_height - 1)) + 1;
            int y_high = chart_height - (int)(((bar.high - min_p) / price_range) * (chart_height - 1)) + 1;
            int y_low = chart_height - (int)(((bar.low - min_p) / price_range) * (chart_height - 1)) + 1;

            wattron(w, COLOR_PAIR(color));
            for (int y = y_high; y <= y_low; ++y)
                mvwaddstr(w, y, x_pos, "│");
            for (int y = std::min(y_open, y_close); y <= std::max(y_open, y_close); ++y)
                mvwaddstr(w, y, x_pos, "█");
            wattroff(w, COLOR_PAIR(color));
            x_pos++;
        }
    }
    else if (node->type == WinType::DELTA)
    {
        mvwprintw(w, 0, 2, " [Delta: %s] ", node->id.c_str());
        if (is_active)
            mvwprintw(w, 0, node->w - 12, " *ACTIVE* ");

        int x_pos = 2;
        for (size_t i = start_bar; i < bars.size() - offset && x_pos < node->w - 2; ++i)
        {
            const auto &bar = bars[i];
            int color = (bar.delta >= 0) ? 1 : 2;
            wattron(w, COLOR_PAIR(color));
            int d_h = std::min((int)std::abs(bar.delta) / 100 + 1, node->h - 2);
            if (bar.delta >= 0)
            {
                for (int y = 0; y < d_h; ++y)
                    mvwaddstr(w, node->h / 2 - y, x_pos, "▪");
            }
            else
            {
                for (int y = 0; y < d_h; ++y)
                    mvwaddstr(w, node->h / 2 + y, x_pos, "▪");
            }
            wattroff(w, COLOR_PAIR(color));
            x_pos++;
        }
    }
    else if (node->type == WinType::TPO)
    {
        mvwprintw(w, 0, 2, " [TPO: %s] %s | IB <xx>", node->id.c_str(), current_session.c_str());
        if (is_active)
            mvwprintw(w, 0, node->w - 12, " *ACTIVE* ");

        // Показываем уникальный контекст TPO (выбранную сессию RTH/ETH)
        mvwprintw(w, 2, 2, "Session context: %s", current_session.c_str());
        mvwprintw(w, 4, 2, "AABBBCCCCDDD (IB Range)");
        mvwprintw(w, 5, 2, "AABBBCCCC (POC)");
    }

    wrefresh(w);
}

void refresh_tree_layouts(std::shared_ptr<WindowNode> node, int y, int x, int h, int w)
{
    node->y = y;
    node->x = x;
    node->h = h;
    node->w = w;
    if (node->split == SplitType::NONE)
    {
        if (node->ncurses_win)
            delwin(node->ncurses_win);
        node->ncurses_win = newwin(h, w, y, x);
        return;
    }
    if (node->split == SplitType::HORIZONTAL)
    {
        int half_h = h / 2;
        refresh_tree_layouts(node->left, y, x, half_h, w);
        refresh_tree_layouts(node->right, y + half_h, x, h - half_h, w);
    }
    else if (node->split == SplitType::VERTICAL)
    {
        int half_w = w / 2;
        refresh_tree_layouts(node->left, y, x, h, half_w);
        refresh_tree_layouts(node->right, y, x + half_w, h, w - half_w);
    }
}

// Рекурсивный поиск окна по его строковому ID среды
std::shared_ptr<WindowNode> find_node_by_id(std::shared_ptr<WindowNode> node, std::string_view target_id)
{
    if (!node)
        return nullptr;
    if (node->split == SplitType::NONE)
    {
        if (node->id == target_id)
            return node;
        return nullptr;
    }
    auto left_res = find_node_by_id(node->left, target_id);
    if (left_res)
        return left_res;
    return find_node_by_id(node->right, target_id);
}

int main()
{
    setlocale(LC_ALL, "");

    int fd = open("ticks.csv", O_RDONLY);
    if (fd == -1)
        return 1;
    struct stat sb;
    fstat(fd, &sb);
    char *file_in_memory = static_cast<char *>(mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0));
    std::string_view file_view(file_in_memory, sb.st_size);

    std::vector<RangeBar> bars;
    double range_size = 2.00;
    calculate_range_bars(file_view, bars, range_size);

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    start_color();
    init_pair(1, COLOR_GREEN, COLOR_BLACK);
    init_pair(2, COLOR_RED, COLOR_BLACK);
    init_pair(3, COLOR_WHITE, COLOR_BLACK);
    init_pair(4, COLOR_CYAN, COLOR_BLACK); // Цвет рамки активной среды

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    // Базовое окно при старте
    auto root = std::make_shared<WindowNode>();
    root->id = "main_chart";
    root->type = WinType::OHLC;

    std::shared_ptr<WindowNode> active_leaf = root; // Курсор/Фокус на старте
    refresh_tree_layouts(root, 0, 0, max_y - 1, max_x);

    WINDOW *cmd_win = newwin(1, max_x, max_y - 1, 0);

    size_t current_offset = 0;
    int ch = 0;
    char cmd_buf[128];
    bool first_render = true;
    int env_counter = 1; // Счетчик сред для уникальных ID

    while (true)
    {
        werase(cmd_win);
        mvwprintw(cmd_win, 0, 0, "[Active Env: %s] :_ (switch <id>, add <type> <pos> <id>, set_session <rth/eth>)", active_leaf->id.c_str());
        wrefresh(cmd_win);

        render_window_content(root, active_leaf, bars, current_offset);

        if (first_render)
        {
            first_render = false;
            continue;
        }

        ch = getch();
        if (ch == 'q' || ch == 'Q')
            break;
        if (ch == 'h' || ch == 'H')
            current_offset++;
        if (ch == 'l' || ch == 'L')
        {
            if (current_offset > 0)
                current_offset--;
        }

        if (ch == ':')
        {
            echo();
            curs_set(1);
            mvwprintw(cmd_win, 0, 0, ":");
            wrefresh(cmd_win);
            wgetnstr(cmd_win, cmd_buf, sizeof(cmd_buf) - 1);
            std::string_view cmd(cmd_buf);

            // 1. КОМАНДА ДОБАВЛЕНИЯ ОКНА С ИМЕНЕМ СРЕДЫ: add <type> <pos> <new_id>
            // Пример: add tpo right my_tpo
            if (cmd.rfind("add ", 0) == 0)
            {
                std::vector<std::string_view> tokens;
                size_t start = 4, end = 0;
                while ((end = cmd.find(' ', start)) != std::string_view::npos)
                {
                    tokens.push_back(cmd.substr(start, end - start));
                    start = end + 1;
                }
                tokens.push_back(cmd.substr(start));

                if (tokens.size() >= 3)
                {
                    std::string_view w_type = tokens[0];
                    std::string_view w_pos = tokens[1];
                    std::string new_id(tokens[2]);

                    auto leaf = active_leaf; // Делим именно то окно, в котором стоим!

                    WinType new_type = WinType::TPO;
                    if (w_type == "delta")
                        new_type = WinType::DELTA;
                    if (w_type == "ohlc")
                        new_type = WinType::OHLC;

                    if (w_pos == "right")
                        leaf->split = SplitType::VERTICAL;
                    if (w_pos == "down")
                        leaf->split = SplitType::HORIZONTAL;

                    if (leaf->split != SplitType::NONE)
                    {
                        leaf->left = std::make_shared<WindowNode>();
                        leaf->left->id = leaf->id; // Старое окно сохраняет свое имя
                        leaf->left->type = leaf->type;

                        leaf->right = std::make_shared<WindowNode>();
                        leaf->right->id = new_id; // Новое окно получает твой ID среды
                        leaf->right->type = new_type;

                        active_leaf = leaf->right; // Сразу прыгаем курсором в созданную среду
                        refresh_tree_layouts(root, 0, 0, max_y - 1, max_x);
                    }
                }
            }
            // 2. КОМАНДА ПЕРЕКЛЮЧЕНИЯ КОНТЕКСТА: switch <id>
            else if (cmd.rfind("switch ", 0) == 0)
            {
                std::string_view target_id = cmd.substr(7);
                auto target_node = find_node_by_id(root, target_id);
                if (target_node)
                {
                    active_leaf = target_node; // Переключаем фокус курсора!
                }
            }
            // 3. КОМАНДА СПЕЦИФИЧНАЯ ТОЛЬКО ДЛЯ АКТИВНОГО ТРО КОНТЕКСТА
            else if (cmd.rfind("set_session ", 0) == 0)
            {
                if (active_leaf->type == WinType::TPO)
                {
                    std::string_view sess = cmd.substr(12);
                    if (sess == "rth" || sess == "RTH")
                        current_session = "RTH";
                    if (sess == "eth" || sess == "ETH")
                        current_session = "ETH";
                }
            }

            noecho();
            curs_set(0);
        }
    }

    endwin();
    munmap(file_in_memory, sb.st_size);
    close(fd);
    return 0;
}