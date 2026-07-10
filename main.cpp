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

// Быстрый парсер строк (теперь с поддержкой таймстампа)
// Предполагаемый формат CSV: HH:MM:SS,Price,Volume,Aggressor
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
        current_bar.close_time = tick.time_str; // Время последнего тика в баре

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
    {
        bars.push_back(current_bar);
    }
}

int main()
{
    setlocale(LC_ALL, "");

    int fd = open("ticks.csv", O_RDONLY);
    if (fd == -1)
    {
        std::cerr << "[-] Ошибка: ticks.csv не найден. Запустите сначала скрипт генерации." << std::endl;
        return 1;
    }
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
    init_pair(4, COLOR_CYAN, COLOR_BLACK); // Для шкал

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    int main_h = ((max_y - 1) * 3) / 4;
    int sub_h = (max_y - 1) - main_h;

    WINDOW *main_win = newwin(main_h, max_x, 0, 0);
    WINDOW *sub_win = newwin(sub_h, max_x, main_h, 0);
    WINDOW *cmd_win = newwin(1, max_x, max_y - 1, 0);

    size_t current_offset = 0;
    int ch;
    char cmd_buf[128];

    // Ширина шкалы цен справа (в символах)
    const int PRICE_SCALE_WIDTH = 12;
    // Высота шкалы времени снизу основного окна (в строках)
    const int TIME_SCALE_HEIGHT = 2;

    while (true)
    {
        werase(main_win);
        werase(sub_win);
        werase(cmd_win);

        box(main_win, 0, 0);
        box(sub_win, 0, 0);

        mvwprintw(main_win, 0, 2, " i3-Trade [OHLC] | Range: %0.2f ", range_size);
        mvwprintw(cmd_win, 0, 0, ":_ (Press ':' for commands)");

        // Рабочая зона для свечей (учитываем границы и шкалы)
        int chart_width = max_x - 2 - PRICE_SCALE_WIDTH;
        int chart_height = main_h - 2 - TIME_SCALE_HEIGHT;

        int start_bar = bars.size() - chart_width - current_offset;
        if (start_bar < 0)
            start_bar = 0;

        // Поиск локальных экстремумов для масштабирования цены
        double min_p = 999999.0, max_p = -999999.0;
        for (size_t i = start_bar; i < bars.size() - current_offset; ++i)
        {
            if (bars[i].high > max_p)
                max_p = bars[i].high;
            if (bars[i].low < min_p)
                min_p = bars[i].low;
        }
        double price_range = (max_p - min_p == 0) ? 1.0 : (max_p - min_p);

        // --- 1. ОТРИСОВКА ШКАЛЫ ЦЕН (СПРАВА) ---
        wattron(main_win, COLOR_PAIR(4));
        for (int y = 1; y <= chart_height; y += 3)
        { // Печатаем цену каждые 3 строки
            double price_at_y = max_p - ((double)(y - 1) / (chart_height - 1)) * price_range;
            mvwprintw(main_win, y, max_x - PRICE_SCALE_WIDTH + 1, "│ %0.2f", price_at_y);
        }
        // Вертикальный разделитель шкалы цен
        for (int y = 1; y <= chart_height + 1; ++y)
        {
            mvwaddstr(main_win, y, max_x - PRICE_SCALE_WIDTH, "│");
        }
        wattroff(main_win, COLOR_PAIR(4));

        // --- 2. ОТРИСОВКА СВЕЧЕЙ И ВРЕМЕНИ ---
        int x_pos = 2;
        for (size_t i = start_bar; i < bars.size() - current_offset && x_pos < chart_width + 2; ++i)
        {
            const auto &bar = bars[i];
            bool is_bull = (bar.close >= bar.open);
            int color = is_bull ? 1 : 2;

            // Определение Y-координат с учетом TIME_SCALE_HEIGHT
            int y_open = chart_height - (int)(((bar.open - min_p) / price_range) * (chart_height - 1)) + 1;
            int y_close = chart_height - (int)(((bar.close - min_p) / price_range) * (chart_height - 1)) + 1;
            int y_high = chart_height - (int)(((bar.high - min_p) / price_range) * (chart_height - 1)) + 1;
            int y_low = chart_height - (int)(((bar.low - min_p) / price_range) * (chart_height - 1)) + 1;

            int body_top = std::min(y_open, y_close);
            int body_bottom = std::max(y_open, y_close);

            wattron(main_win, COLOR_PAIR(color));
            // Фитили
            for (int y = y_high; y <= y_low; ++y)
            {
                if (y > 0 && y <= chart_height)
                    mvwaddstr(main_win, y, x_pos, "│");
            }
            // Тело
            for (int y = body_top; y <= body_bottom; ++y)
            {
                if (y > 0 && y <= chart_height)
                    mvwaddstr(main_win, y, x_pos, "█");
            }
            wattroff(main_win, COLOR_PAIR(color));

            // --- ОТРИСОВКА ВРЕМЕНИ (КАЖДЫЕ 15 БАРОВ) ---
            if (i % 15 == 0)
            {
                wattron(main_win, COLOR_PAIR(4));
                // Рисуем маленькую засечку
                mvwaddstr(main_win, chart_height + 1, x_pos, "┼");
                // Печатаем HH:MM (первые 5 символов строки времени)
                std::string time_short(bar.close_time.substr(0, 5));
                mvwprintw(main_win, chart_height + 2, x_pos - 2, "%s", time_short.c_str());
                wattroff(main_win, COLOR_PAIR(4));
            }

            // --- ДЕЛЬТА В СУБ-ОКНЕ ---
            int sub_color = (bar.delta >= 0) ? 1 : 2;
            wattron(sub_win, COLOR_PAIR(sub_color));
            int delta_height = std::min((int)std::abs(bar.delta) / 100 + 1, sub_h - 2);
            if (bar.delta >= 0)
            {
                for (int y = 0; y < delta_height; ++y)
                    mvwaddstr(sub_win, sub_h / 2 - y, x_pos, "▪");
            }
            else
            {
                for (int y = 0; y < delta_height; ++y)
                    mvwaddstr(sub_win, sub_h / 2 + y, x_pos, "▪");
            }
            wattroff(sub_win, COLOR_PAIR(sub_color));

            x_pos++;
        }

        wrefresh(main_win);
        wrefresh(sub_win);
        wrefresh(cmd_win);

        ch = getch();
        if (ch == 'q' || ch == 'Q')
            break;
        if (ch == 'h' || ch == 'H')
        {
            if (bars.size() - current_offset > (size_t)chart_width)
                current_offset++;
        }
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

            if (cmd.rfind("set_range ", 0) == 0)
            {
                std::string_view val_str = cmd.substr(10);
                double new_range = range_size;
                auto [ptr, ec] = std::from_chars(val_str.data(), val_str.data() + val_str.size(), new_range);
                if (ec == std::errc() && new_range > 0.0)
                {
                    range_size = new_range;
                    current_offset = 0;
                    calculate_range_bars(file_view, bars, range_size);
                }
            }
            noecho();
            curs_set(0);
        }
    }

    delwin(main_win);
    delwin(sub_win);
    delwin(cmd_win);
    endwin();
    munmap(file_in_memory, sb.st_size);
    close(fd);
    return 0;
}