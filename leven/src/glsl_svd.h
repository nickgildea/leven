#ifndef		HAS_GLSL_SVD_H_BEEN_INCLUDED
#define		HAS_GLSL_SVD_H_BEEN_INCLUDED

#include	<vector>

#define QEF_USE_GLM
#define USE_GLM
#ifdef QEF_USE_GLM

#include <glm/glm.hpp>
#include <glm/gtx/simd_vec4.hpp>
#include <glm/gtx/simd_mat4.hpp>
typedef glm::mat3 qef_mat3;
typedef glm::vec3 qef_vec3;
typedef glm::vec4 qef_vec4;
#else
typedef float qef_mat3[3][3];
typedef float qef_vec3[3];
typedef float qef_vec4[4];
#endif

typedef float mat_upper_tri[6];

struct QEFData
{
	QEFData()
	{
		ATA[0] = ATA[1] = ATA[2] = ATA[3] = ATA[4] = ATA[5] = 0.f;
		ATb[0] = ATb[1] = ATb[2] = 0.f;
		masspoint[0] = masspoint[1] = masspoint[2] = masspoint[3] = 0.f;
	}

	mat_upper_tri	ATA;
	float			pad[2];
	qef_vec4		ATb;
	qef_vec4		masspoint;
};

QEFData qef_construct(
	const qef_vec3* positions,
	const qef_vec3* normals,
	const size_t count);

void qef_add(QEFData& a, const QEFData& b);

float qef_solve(
	const QEFData& qef, 
	qef_vec3& solved_position);

float qef_solve_from_points(
	const qef_vec3* positions,
	const qef_vec3* normals,
	const size_t count,
	qef_vec3& solved_position);

#endif	//	HAS_GLSL_SVD_H_BEEN_INCLUDED
