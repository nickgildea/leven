#define QEF_INCLUDE_IMPL
#include "qef_simd.h"

// old interface
#if 0

// ----------------------------------------------------------------------------

QEFSIMDData qef_simd_construct(
	const __m128* positions,
	const __m128* normals,
	const size_t count)
{
	QEFSIMDData data;

	for (size_t i = 0; i < count; i++)
	{
		qef_simd_add(positions[i], normals[i], data.ATA, data.ATb, data.masspoint);
	}

	data.masspoint = _mm_div_ps(data.masspoint.Data, _mm_set1_ps(data.masspoint.Data.m128_f32[3]));
	return data;
}

// ----------------------------------------------------------------------------

void qef_simd_add(QEFSIMDData& a, const QEFSIMDData& b)
{
	a.ATA += b.ATA;
	a.ATb += b.ATb;

	const __m128 m = _mm_set_ps(
		1.f,
		a.masspoint.Data.m128_f32[2],
		a.masspoint.Data.m128_f32[1],
		a.masspoint.Data.m128_f32[0]);
	a.masspoint = _mm_add_ps(a.masspoint.Data, m);
}

// ----------------------------------------------------------------------------

float qef_simd_solve(
	const QEFSIMDData& data,
	__m128& solved_position)
{
	return qef_simd_solve(data.ATA, data.ATb, data.masspoint, solved_position);
}

// ----------------------------------------------------------------------------

#endif

