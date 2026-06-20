// SPDX-FileCopyrightText: 2024 kwin-outline contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdarg>
#include <cstdio>
#include <ctime>

namespace KWinOutline
{

namespace detail
{
inline void logv(const char *prefix, const char *format, va_list args)
{
    FILE *f = std::fopen("/tmp/kwinoutline-debug.log", "a");
    if (!f) {
        return;
    }
    std::time_t t = std::time(nullptr);
    struct tm tm_info;
    ::localtime_r(&t, &tm_info);
    char timebuf[9];
    std::strftime(timebuf, sizeof(timebuf), "%H:%M:%S", &tm_info);
    std::fprintf(f, "%s %s kwinoutline: ", timebuf, prefix);
    std::vfprintf(f, format, args);
    std::fputc('\n', f);
    std::fclose(f);
}
} // namespace detail

__attribute__((format(printf, 1, 2))) inline void debugLog(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    detail::logv("[DEBUG]", format, args);
    va_end(args);
}

__attribute__((format(printf, 1, 2))) inline void infoLog(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    detail::logv("[INFO] ", format, args);
    va_end(args);
}

__attribute__((format(printf, 1, 2))) inline void warnLog(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    detail::logv("[WARN] ", format, args);
    va_end(args);
}

} // namespace KWinOutline
