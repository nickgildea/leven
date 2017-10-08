#ifndef		__CAMERA_H__
#define		__CAMERA_H__

#include <glm/glm.hpp>

struct CameraInput
{
	glm::vec3	speed;
	float		yaw, pitch;
};

void Camera_Initialise();
void Camera_Update(float deltaT, const CameraInput& input);

void Camera_SetPosition(const glm::vec3& pos);
void Camera_SetSpeed(const float speed);
glm::vec3 Camera_GetPosition();
glm::vec3 Camera_GetForward();
void Camera_GetViewAngles(float& x, float& y, float& z);

#endif	//	__CAMERA_H__

