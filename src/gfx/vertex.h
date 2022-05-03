#ifndef RUNE_VERTEX_H
#define RUNE_VERTEX_H

#include <glm/glm.hpp>

namespace rune {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

} // namespace rune

#endif // RUNE_VERTEX_H
