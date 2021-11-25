#include "logger.h"

int main() {
    using namespace rune;

    Logger::info("operating system: %", consts::os_name);
    Logger::info("is release build: %", consts::is_release);

    return 0;
}
