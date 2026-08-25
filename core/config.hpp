#pragma once

#include "WindowTree.hpp"

#include <string>

struct SessionWindow
{
    int start_seconds = 0;
    int end_seconds = 0;
};

struct AppConfig
{
    Session active_session = Session::ETH;
    SessionWindow rth{9 * 3600 + 30 * 60, 16 * 3600};
    SessionWindow overnight{18 * 3600, 9 * 3600 + 30 * 60};
    SessionWindow eth{18 * 3600, 16 * 3600};
};

AppConfig load_config(const std::string &path);
bool tick_in_session(const Tick &tick, const SessionWindow &window);
const char *session_name(Session session) noexcept;
Session next_session(Session session) noexcept;
std::size_t session_index(Session session) noexcept;