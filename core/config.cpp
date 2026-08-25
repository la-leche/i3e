#include "config.hpp"

#include <cctype>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace
{
    std::string trim(std::string value)
    {
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
            value.erase(value.begin());
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
            value.pop_back();
        return value;
    }

    int parse_time(const std::string &value)
    {
        if (value.size() != 8 || value[2] != ':' || value[5] != ':')
            throw std::runtime_error("Invalid session time: " + value);

        const int hour = std::stoi(value.substr(0, 2));
        const int minute = std::stoi(value.substr(3, 2));
        const int second = std::stoi(value.substr(6, 2));
        if (hour > 23 || minute > 59 || second > 59)
            throw std::runtime_error("Invalid session time: " + value);
        return hour * 3600 + minute * 60 + second;
    }

    Session parse_session(const std::string &value)
    {
        if (value == "rth")
            return Session::RTH;
        if (value == "overnight")
            return Session::Overnight;
        if (value == "eth")
            return Session::ETH;
        throw std::runtime_error("Invalid active_session: " + value);
    }

    void set_window(SessionWindow &window, const std::string &key, const std::string &value)
    {
        if (key == "start")
            window.start_seconds = parse_time(value);
        else if (key == "end")
            window.end_seconds = parse_time(value);
        else
            throw std::runtime_error("Unknown session setting: " + key);
    }
}

AppConfig load_config(const std::string &path)
{
    AppConfig config;
    std::ifstream file(path);
    if (!file)
        return config;

    std::string section;
    std::string line;
    while (std::getline(file, line))
    {
        line = trim(line);
        if (line.empty() || line.front() == '#')
            continue;

        if (line.front() == '[' && line.back() == ']')
        {
            section = line.substr(1, line.size() - 2);
            continue;
        }

        const std::size_t separator = line.find('=');
        if (separator == std::string::npos)
            throw std::runtime_error("Invalid config line: " + line);

        const std::string key = trim(line.substr(0, separator));
        const std::string value = trim(line.substr(separator + 1));
        if (section == "session")
        {
            if (key != "active")
                throw std::runtime_error("Unknown session setting: " + key);
            config.active_session = parse_session(value);
        }
        else if (section == "rth")
            set_window(config.rth, key, value);
        else if (section == "overnight")
            set_window(config.overnight, key, value);
        else if (section == "eth")
            set_window(config.eth, key, value);
        else
            throw std::runtime_error("Unknown config section: " + section);
    }
    return config;
}

bool tick_in_session(const Tick &tick, const SessionWindow &window)
{
    if (tick.time.size() != 8)
        return false;

    const int seconds = (tick.time[0] - '0') * 10 * 3600 +
                        (tick.time[1] - '0') * 3600 +
                        (tick.time[3] - '0') * 10 * 60 +
                        (tick.time[4] - '0') * 60 +
                        (tick.time[6] - '0') * 10 +
                        (tick.time[7] - '0');
    if (window.start_seconds <= window.end_seconds)
        return seconds >= window.start_seconds && seconds < window.end_seconds;
    return seconds >= window.start_seconds || seconds < window.end_seconds;
}

const char *session_name(Session session) noexcept
{
    switch (session)
    {
    case Session::RTH:
        return "RTH";
    case Session::Overnight:
        return "OVERNIGHT";
    case Session::ETH:
        return "ETH";
    }
    return "UNKNOWN";
}

Session next_session(Session session) noexcept
{
    switch (session)
    {
    case Session::RTH:
        return Session::ETH;
    case Session::ETH:
        return Session::Overnight;
    case Session::Overnight:
        return Session::RTH;
    }
    return Session::ETH;
}

std::size_t session_index(Session session) noexcept
{
    switch (session)
    {
    case Session::RTH:
        return 0;
    case Session::Overnight:
        return 1;
    case Session::ETH:
        return 2;
    }
    return 2;
}