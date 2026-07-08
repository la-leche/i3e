#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <string_view>
#include <charconv>

// Базовая структура тика
struct Tick
{
    double price;
    long volume;
    int aggressor; // 1 = Buy (Ask), -1 = Sell (Bid)
};

// Структура нашего кастомного Range-бара
struct RangeBar
{
    double open;
    double high;
    double low;
    double close;
    long volume = 0;
    long delta = 0;
    bool is_closed = false;
};

// Быстрый парсер строки через std::string_view без выделения памяти в куче
Tick parse_tick_line(std::string_view line)
{
    Tick tick{0.0, 0, 0};

    // Находим запятые
    size_t first_comma = line.find(',');
    size_t second_comma = line.find(',', first_comma + 1);

    if (first_comma == std::string_view::npos || second_comma == std::string_view::npos)
    {
        return tick;
    }

    std::string_view p_str = line.substr(0, first_comma);
    std::string_view v_str = line.substr(first_comma + 1, second_comma - first_comma - 1);
    std::string_view a_str = line.substr(second_comma + 1);

    // Быстрый перевод в числа через std::from_chars (микросекундный уровень)
    std::from_chars(p_str.data(), p_str.data() + p_str.size(), tick.price);
    std::from_chars(v_str.data(), v_str.data() + v_str.size(), tick.volume);
    std::from_chars(a_str.data(), a_str.data() + a_str.size(), tick.aggressor);

    return tick;
}

int main()
{
    const char *filename = "ticks.csv";
    double range_size = 1.00; // Настройка размера Range-бара (например, 1 доллар/пункт)

    // 1. Открываем файл через системный вызов Unix
    int fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        std::cerr << "[-] Ошибка открытия файла!" << std::endl;
        return 1;
    }

    // Получаем размер файла
    struct stat sb;
    if (fstat(fd, &sb) == -1)
    {
        std::cerr << "[-] Ошибка получения размера файла!" << std::endl;
        close(fd);
        return 1;
    }

    // 2. Маппим файл напрямую в память процесса (mmap)
    char *file_in_memory = static_cast<char *>(mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0));
    if (file_in_memory == MAP_FAILED)
    {
        std::cerr << "[-] mmap не удался!" << std::endl;
        close(fd);
        return 1;
    }

    std::string_view file_view(file_in_memory, sb.st_size);
    std::vector<RangeBar> bars;
    RangeBar current_bar;
    bool first_tick = true;

    size_t pos = 0;
    size_t prev = 0;

    std::cout << "[+] Начинаем обработку тиков через mmap..." << std::endl;

    // 3. Потоковый парсер по строкам
    while ((pos = file_view.find('\n', prev)) != std::string_view::npos)
    {
        std::string_view line = file_view.substr(prev, pos - prev);
        prev = pos + 1;

        if (line.empty())
            continue;

        Tick tick = parse_tick_line(line);
        if (tick.volume == 0)
            continue; // Пропускаем битые строки

        // Логика инициализации первого бара
        if (first_tick)
        {
            current_bar.open = tick.price;
            current_bar.high = tick.price;
            current_bar.low = tick.price;
            current_bar.close = tick.price;
            first_tick = false;
        }

        // Обновляем метрики внутри текущего бара
        current_bar.volume += tick.volume;
        current_bar.delta += (tick.aggressor * tick.volume); // Агрессор 1 или -1

        if (tick.price > current_bar.high)
            current_bar.high = tick.price;
        if (tick.price < current_bar.low)
            current_bar.low = tick.price;
        current_bar.close = tick.price;

        // Проверяем условие Range-бара: вышел ли текущий спред за рамки range_size
        if ((current_bar.high - current_bar.low) >= range_size)
        {
            current_bar.is_closed = true;
            bars.push_back(current_bar);

            // Открываем следующий бар с ценой закрытия предыдущего
            RangeBar next_bar;
            next_bar.open = current_bar.close;
            next_bar.high = current_bar.close;
            next_bar.low = current_bar.close;
            next_bar.close = current_bar.close;
            current_bar = next_bar;
        }
    }

    // Добавляем последний открытый бар, если файл закончился
    if (!first_tick && !current_bar.is_closed)
    {
        bars.push_back(current_bar);
    }

    // 4. Демонстрация вывода (Прообраз твоего будущих суб-окон)
    std::cout << "\n[+] Сформированные Range Бары:" << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;
    for (size_t i = 0; i < bars.size(); ++i)
    {
        std::cout << "Bar #" << i + 1
                  << " | O: " << bars[i].open
                  << " | H: " << bars[i].high
                  << " | L: " << bars[i].low
                  << " | C: " << bars[i].close
                  << " | Vol: " << bars[i].volume
                  << " | Delta: " << (bars[i].delta > 0 ? "+" : "") << bars[i].delta
                  << std::endl;
    }

    // Освобождаем ресурсы
    munmap(file_in_memory, sb.st_size);
    close(fd);
    return 0;
}