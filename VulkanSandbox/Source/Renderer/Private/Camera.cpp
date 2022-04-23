#include "Camera.h"

Camera::Camera()
{
    UpdateMatrices();
}

Camera::Camera(const Vec3& pos, float aspect_ratio, float fov, float near_clip, float far_clip)
    : pos_(pos), aspect_ratio_(aspect_ratio), fov_(fov), near_clip_(near_clip), far_clip_(far_clip)
{
    UpdateMatrices();
}

void Camera::SetNearClip(float z)
{
    near_clip_ = z;
    is_projection_dirty_ = true;
}

void Camera::SetFarClip(float z)
{
    far_clip_ = z;
    is_projection_dirty_ = true;
}

void Camera::SetFov(float fov_in_rad)
{
    fov_ = fov_in_rad;
    is_projection_dirty_ = true;
}

void Camera::LookAt(Vec3 target)
{
    look_at_target_ = target;
    is_view_dirty_ = true;
}

void Camera::SetPosition(const Vec3& position)
{
    pos_ = position;
    is_view_dirty_ = true;
}

const Mat4& Camera::GetProjection()
{
    if (is_projection_dirty_)
    {
        UpdateProjection();
    }

    return projection_;
}

void Camera::SetProjection(const Mat4& projection)
{
    projection_ = projection;
    is_projection_dirty_ = true;
}

const Mat4& Camera::GetView()
{
    if (is_view_dirty_)
    {
        UpdateView();
    }

    return view_;
}

void Camera::SetView(const Mat4& view)
{
    view_ = view;
    is_view_dirty_ = true;
}

const Mat4& Camera::GetViewProjection()
{
    return view_projection_;
}

void Camera::SetViewProjection(const Mat4& vp)
{
    view_projection_ = vp;
}

void Camera::UpdateMatrices()
{
    bool is_vp_dirty = is_view_dirty_ || is_projection_dirty_;

    UpdateProjection();
    UpdateView();

    if (is_vp_dirty)
    {
        UpdateViewProjection();
    }
}

void Camera::UpdateProjection()
{
    projection_ = glm::perspective(fov_, aspect_ratio_, near_clip_, far_clip_);
    is_projection_dirty_ = false;
}

void Camera::UpdateView()
{
    view_ = glm::lookAt(pos_, look_at_target_, up_);
    is_view_dirty_ = false;
}

void Camera::UpdateViewProjection()
{
    view_projection_ = view_ * projection_;
}
