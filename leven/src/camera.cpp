#include	"camera.h"

#include	"physics.h"

#include	<memory>
#include	<glm/ext.hpp>

using		glm::vec3;
using		glm::vec4;
using		glm::mat4;

struct FirstPersonCamera 
{
	void update(const vec3& speed, float rotateX, float rotateY)
	{
		rotateX_ += rotateX;
		rotateX_ = glm::clamp(rotateX_, -85.f, 85.f);

		rotateY_ += rotateY;

		const mat4 pitchMatrix = glm::rotate(mat4(1.f), rotateX_, vec3(1, 0, 0));
		const mat4 yawMatrix = glm::rotate(mat4(1.f), rotateY_, vec3(0, 1, 0));

		vec4 forward(0.f, 0.f, 1.f, 0.f);
		forward = pitchMatrix * forward;
		forward = yawMatrix * forward;
		forward_ = vec3(forward);

		vec4 right(1.f, 0.f, 0.f, 0.f);
		right = pitchMatrix * right;
		right = yawMatrix * right;
		right_ = vec3(right);

		vec4 up(0.f, 1.f, 0.f, 0.f);
		up = pitchMatrix * up;
		up = yawMatrix * up;
		up_ = vec3(up);

		position_ = Physics_GetPlayerPosition();

		vec3 velocity;
		velocity += speed.z * forward_;
		velocity += speed.y * up_;
		velocity += speed.x * right_;
		Physics_SetPlayerVelocity(velocity);
	}

	void getViewAngles(float& x, float& y, float& z)
	{
		x = rotateX_;
		y = rotateY_;
		z = 0.f;
	}

	vec3		position_ { 0.f };
	vec3		forward_ { 0.f, 0.f, 1.f };
	vec3		up_ { 0.f, 1.f, 0.f };
	vec3		right_ { 1.f, 0.f, 0.f };
	float		rotateX_ = 0.f;
	float		rotateY_ = 180.f;
	float		speed = 1.f;
};

FirstPersonCamera g_camera;

void Camera_Initialise()
{
	g_camera = FirstPersonCamera();
}

void Camera_Update(float deltaT, const CameraInput& input)
{
	const float DEGREES_PER_SEC = 60.f;
	const float rotateX = deltaT * (input.pitch * DEGREES_PER_SEC);
	const float rotateY = deltaT * (input.yaw * DEGREES_PER_SEC);
	const vec3 adjustedSpeed = deltaT * (input.speed * g_camera.speed);

	g_camera.update(adjustedSpeed, rotateX, rotateY);
}

void Camera_SetPosition(const vec3& pos)
{
	g_camera.position_ = pos;
}

void Camera_SetMoveSpeed(const float speed)
{
	g_camera.speed = speed;
}

vec3 Camera_GetPosition()
{
	return g_camera.position_;
}

vec3 Camera_GetForward()
{
	return g_camera.forward_;
}

void Camera_GetViewAngles(float& x, float& y, float& z)
{
	g_camera.getViewAngles(x, y, z);
}


