#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <string_view>
#include <charconv>
#include <ncurses.h>

struct Tick
{
    double price;
    long volume;
    int aggressor;
};

struct RangeBar
{
    double open;
    double high;
    double low;
    double close;
    long volume = 0;
    long delta = 0;
};

// Быстрый парсер строк
Tick parse_tick_line(std::string_view line)
{
    Tick tick{0.0, 0, 0};
    size_t first_comma = line.find(',');
    size_t second_comma = line.find(',', first_comma + 1);
    if (first_comma == std::string_view::npos || second_comma == std::string_view::npos)
        return tick;

    std::string_view p_str = line.substr(0, first_comma);
    std::string_view v_str = line.substr(first_comma + 1, second_comma - first_comma - 1);
    std::string_view a_str = line.substr(second_comma + 1);

    std::from_chars(p_str.data(), p_str.data() + p_str.size(), tick.price);
    std::from_chars(v_str.data(), v_str.data() + v_str.size(), tick.volume);
    std::from_chars(a_str.data(), a_str.data() + a_str.size(), tick.aggressor);
    return tick;
}

int main()
{
    // --- 1. ПАРСИНГ ДАННЫХ ЧЕРЕЗ MMAP ---
    int fd = open("ticks.csv", O_RDONLY);
    if (fd == -1)
    {
        std::cerr << "[-] Сначала запустите python-скрипт для генерации ticks.csv!" << std::endl;
        return 1;
    }
    struct stat sb;
    fstat(fd, &sb);
    char *file_in_memory = static_cast<char *>(mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0));

    std::string_view file_view(file_in_memory, sb.st_size);
    std::vector<RangeBar> bars;
    RangeBar current_bar;
    bool first_tick = true;
    double range_size = 2.00; // Шаг баров

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
            first_tick = false;
        }

        current_bar.volume += tick.volume;
        current_bar.delta += (tick.aggressor * tick.volume);
        if (tick.price > current_bar.high)
            current_bar.high = tick.price;
        if (tick.price < current_bar.low)
            current_bar.low = tick.price;
        current_bar.close = tick.price;

        if ((current_bar.high - current_bar.low) >= range_size)
        {
            bars.push_back(current_bar);
            RangeBar next_bar;
            next_bar.open = next_bar.high = next_bar.low = next_bar.close = current_bar.close;
            current_bar = next_bar;
        }
    }
    munmap(file_in_memory, sb.st_size);
    close(fd);

    // --- 2. ИНИЦИАЛИЗАЦИЯ NCURSES (TUI) ---
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0); // Прячем курсор
    start_color();

    // Настройка сочных цветов (TrueColor / 256)
    init_pair(1, COLOR_GREEN, COLOR_BLACK); // Покупатели
    init_pair(2, COLOR_RED, COLOR_BLACK);   // Продавцы
    init_pair(3, COLOR_CYAN, COLOR_BLACK);  // Границы / Текст

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    // Делим экран: верхнее под графики (75% высоты), нижнее под дельту
    int main_h = (max_y * 3) / 4;
    int sub_h = max_y - main_h;

    WINDOW *main_win = newwin(main_h, max_x, 0, 0);
    WINDOW *sub_win = newwin(sub_h, max_x, main_h, 0);

    size_t current_offset = 0; // Для симуляции скролла истории (H/L)
    int ch;

    while (true)
    {
        // Очищаем окна перед перерисовкой
        werase(main_win);
        werase(sub_win);

        box(main_win, 0, 0);
        box(sub_win, 0, 0);

        mvwprintw(main_win, 0, 2, " i3-Trade Engine | Range Bars: %0.2f | Total Bars: %lu ", range_size, bars.size());
        mvwprintw(sub_win, 0, 2, " Sub-Window: Delta Indicator (H/L to Scroll, Q to Quit) ");

        // Отрисовка баров, которые влезают в ширину терминала
        int available_width = max_x - 4;
        int start_bar = bars.size() - available_width - current_offset;
        if (start_bar < 0)
            start_bar = 0;

        int x_pos = 2;
        for (size_t i = start_bar; i < bars.size() - current_offset && x_pos < max_x - 2; ++i)
        {
            const auto &bar = bars[i];

            // Логика цвета бара
            int color = (bar.close >= bar.open) ? 1 : 2;
            wattron(main_win, COLOR_PAIR(color));

            // Простейший рендеринг: рисуем Close цену в виде вертикального смещения
            int y_render = main_h / 2 + (int)(bar.close - bars[0].open);
            if (y_render > 1 && y_render < main_h - 1)
            {
                mvwaddch(main_win, y_render, x_pos, 'X');
            }
            wattroff(main_win, COLOR_PAIR(color));

            // Рендеринг Дельты в нижнем суб-окне
            int sub_color = (bar.delta >= 0) ? 1 : 2;
            wattron(sub_win, COLOR_PAIR(sub_color));

            // Если дельта положительная — палочка вверх, отрицательная — вниз
            if (bar.delta >= 0)
            {
                mvwaddch(sub_win, sub_h / 2 - 1, x_pos, '^');
            }
            else
            {
                mvwaddch(sub_win, sub_h / 2 + 1, x_pos, 'v');
            }
            wattroff(sub_win, COLOR_PAIR(sub_color));

            x_pos++;
        }

        wrefresh(main_win);
        wrefresh(sub_win);

        // Обработка Vim-style нажатий
        ch = getch();
        if (ch == 'q' || ch == 'Q')
            break;
        if (ch == 'h' || ch == 'H')
        { // Скролл назад в историю
            if (bars.size() - current_offset > (size_t)available_width)
                current_offset++;
        }
        if (ch == 'l' || ch == 'L')
        { // Скролл вперед к текущим тикам
            if (current_offset > 0)
                current_offset--;
        }
    }

    // Закрываем ncurses
    delwin(main_win);
    delwin(sub_win);
    endwin();
    return 0;
}