#ifndef RUNE_CONSTS_H
#define RUNE_CONSTS_H

namespace rune::consts {

constexpr const char* os_name =
#if defined(_MSC_VER)
    "Windows";
#define OS_WINDOWS
#elif defined(__unix__)
    "Linux";
#define OS_LINUX
#else
    "Unknown";
#endif

constexpr bool is_release =
#ifdef NDEBUG
    true;
#else
    false;
#endif

} // namespace rune::consts

#endif // RUNE_CONSTS_H
