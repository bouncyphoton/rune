#ifndef RUNE_CAMERA_H
#define RUNE_CAMERA_H

#include "consts.h"
#include "types.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace rune {

class Camera {
  public:
    Camera(f32       fov_radians  = glm::half_pi<f32>(),
           f32       aspect_ratio = 1.0f,
           f32       near         = 0.1f,
           f32       far          = 100.0f,
           glm::vec3 position     = glm::vec3(0),
           f32       pitch        = 0.0f,
           f32       yaw          = 0.0f)
        : fov_radians_(fov_radians), aspect_ratio_(aspect_ratio), near_(near), far_(far), position_(position) {
        set_pitch(pitch);
        set_yaw(yaw);
    }

    [[nodiscard]] glm::vec3 get_position() const {
        return position_;
    }

    [[nodiscard]] glm::mat4 get_view_matrix() const {
        return glm::lookAt(position_, position_ + get_forward(), consts::up);
    }

    [[nodiscard]] glm::mat4 get_projection_matrix() const {
        return glm::perspective(fov_radians_, aspect_ratio_, near_, far_);
    }

    [[nodiscard]] glm::mat4 get_view_projection_matrix() const {
        return get_projection_matrix() * get_view_matrix();
    }

    [[nodiscard]] glm::vec3 get_forward() const {
        return glm::vec3(cosf(yaw_radians_) * cosf(pitch_radians_),
                         sinf(pitch_radians_),
                         sinf(yaw_radians_) * cos(pitch_radians_));
    }

    [[nodiscard]] glm::vec3 get_right() const {
        return normalize(glm::cross(get_forward(), consts::up));
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

    void add_position(glm::vec3 position) {
        position_ += position;
    }

    /**
     * Get the pitch of the camera
     * @return The pitch in radians
     */
    [[nodiscard]] f32 get_pitch() const {
        return pitch_radians_;
    }

    /**
     * Set the pitch of the camera
     * @param pitch The pitch in radians
     */
    void set_pitch(f32 pitch) {
        pitch_radians_ = pitch;
    }

    void add_pitch(f32 pitch) {
        set_pitch(get_pitch() + pitch);
    }

    [[nodiscard]] f32 get_yaw() const {
        return yaw_radians_;
    }

    void set_yaw(f32 yaw) {
        yaw_radians_ = yaw;
    }

    void add_yaw(f32 yaw) {
        set_yaw(get_yaw() + yaw);
    }

  private:
    f32       fov_radians_;
    f32       aspect_ratio_;
    f32       near_;
    f32       far_;
    glm::vec3 position_;
    f32       pitch_radians_;
    f32       yaw_radians_;
};

} // namespace rune

#endif // RUNE_CAMERA_H
