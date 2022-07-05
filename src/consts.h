#ifndef RUNE_CONSTS_H
#define RUNE_CONSTS_H

#include <glm/glm.hpp>

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

constexpr glm::vec3 up = glm::vec3(0, 1, 0);

} // namespace rune::consts

#endif // RUNE_CONSTS_H
