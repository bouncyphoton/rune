#ifndef RUNE_CAMERA_H
#define RUNE_CAMERA_H

#include "constants.h"
#include "types.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace rune {

class Camera {
  public:
    Camera() : Camera(glm::half_pi<f32>(), 1.0f, 0.1f, 100.0f, glm::vec3(0), glm::vec3(0, 0, -1)) {}
    Camera(f32 fov_radians, f32 aspect_ratio, f32 near, f32 far, glm::vec3 position, glm::vec3 look_position)
        : fov_radians_(fov_radians), aspect_ratio_(aspect_ratio), near_(near), far_(far), position_(position),
          look_position_(look_position) {}

    [[nodiscard]] glm::vec3 get_position() const {
        return position_;
    }

    [[nodiscard]] glm::mat4 get_view_matrix() const {
        return glm::lookAt(position_, look_position_, consts::UP);
    }

    [[nodiscard]] glm::mat4 get_projection_matrix() const {
        return glm::perspective(fov_radians_, aspect_ratio_, near_, far_);
    }

    [[nodiscard]] glm::mat4 get_view_projection_matrix() const {
        return get_projection_matrix() * get_view_matrix();
    }

    [[nodiscard]] glm::vec3 get_forward() const {
        return glm::normalize(look_position_ - position_);
    }

    [[nodiscard]] glm::vec3 get_right() const {
        return glm::cross(get_forward(), consts::UP);
    }

    [[nodiscard]] glm::vec3 get_up() const {
        return glm::cross(get_right(), get_forward());
    }

    void set_aspect_ratio(f32 aspect_ratio) {
        aspect_ratio_ = aspect_ratio;
    }

    void set_position(glm::vec3 position) {
        position_ = position;
    }

  private:
    f32       fov_radians_;
    f32       aspect_ratio_;
    f32       near_;
    f32       far_;
    glm::vec3 position_;
    glm::vec3 look_position_;
};

} // namespace rune

#endif // RUNE_CAMERA_H
