#ifndef RUNE_UTILS_H
#define RUNE_UTILS_H

#include "consts.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace rune::utils {

#define rune_assert(core, expr)                                                                      \
    do {                                                                                             \
        if (!(expr)) {                                                                               \
            (core).get_logger().fatal("rune assertion failed at %:%: %", __FILE__, __LINE__, #expr); \
        }                                                                                            \
    } while (0);

#define rune_debug_assert(core, expr)              \
    do {                                           \
        if constexpr (!rune::consts::is_release) { \
            rune_assert(core, expr);               \
        }                                          \
    } while (0);

namespace detail {

static void format_str_process_next(std::ostream& stream, const char* format) {
    const char* current = format;
    while (*current) {
        bool is_escaping_percent = *current == '\\' && *(current + 1) == '%';
        if (is_escaping_percent) {
            stream.write(format, current - format);
            ++current;
            format = current;
        }

        ++current;
    }
    stream << format;
}

template <typename T, typename... Args>
static void
format_str_process_next(std::stringstream& stream, const char* format, const T& value, const Args&... args) {
    const char* current = format;
    while (*current) {
        bool is_escaping_percent = *current == '\\' && *(current + 1) == '%';
        bool is_regular_percent  = *current == '%';

        if (is_escaping_percent) {
            // print out characters up to here, then skip backslash and treat percent like regular character
            stream.write(format, current - format);
            ++current;
            format = current;
        }

        if (is_regular_percent) {
            stream.write(format, current - format);
            ++current;
            format = current;
            break;
        }

        ++current;
    }

    stream << value;

    if (*format) {
        format_str_process_next(stream, format, args...);
    }
}

} // namespace detail

template <typename... Args> static std::string format_str(const char* format, const Args&... args) {
    std::stringstream out;
    detail::format_str_process_next(out, format, args...);
    return out.str();
}

static std::vector<char> load_binary_file(const char* path) {
    std::vector<char> data;

    if (std::ifstream file = std::ifstream(path, std::ios::ate | std::ios::binary)) {
        u32 size = (u32)file.tellg();
        data.resize(size);
        file.seekg(0);
        file.read(data.data(), size);
    }

    return data;
}

} // namespace rune::utils

#endif // RUNE_UTILS_H
