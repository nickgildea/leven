// minimal SVD implementation for calculating feature points from hermite data
// works in C++ and GLSL

// public domain

#include	"glsl_svd.h"

#ifndef USE_GLSL
#define USE_GLSL 0
#endif

#ifndef USE_WEBGL
#define USE_WEBGL 0
#endif

#ifndef DEBUG_SVD
//#define DEBUG_SVD (!USE_GLSL)
#endif

#ifndef SVD_COMPARE_REFERENCE
//#define SVD_COMPARE_REFERENCE 1
#endif

#define SVD_NUM_SWEEPS 5

#if USE_GLSL

// GLSL prerequisites

#define IN(t,x) in t x
#define OUT(t, x) out t x
#define INOUT(t, x) inout t x
#define rsqrt inversesqrt

#define SWIZZLE_XYZ(v) v.xyz

#else

// C++ prerequisites

#include <math.h>

//#include <glm/glm.hpp>
//#include <glm/gtc/swizzle.hpp>
using namespace glm;


#define abs fabs
#define sqrt sqrtf
#define max(x,y) (((x)>(y))?(x):(y))
#define IN(t,x) const t& x
#define OUT(t, x) t &x
#define INOUT(t, x) t &x

float rsqrt(float x) {
	return 1.0f / sqrt(x);
}

//#define SWIZZLE_XYZ(v) vec3(swizzle<X,Y,Z>(v))

#endif

#if DEBUG_SVD

// Debugging
////////////////////////////////////////////////////////////////////////////////

void dump_vec3(vec3 v) {
	printf("(%.5f %.5f %.5f)\n", v[0], v[1], v[2]);
}

void dump_vec4(vec4 v) {
	printf("(%.5f %.5f %.5f %.5f)\n", v[0], v[1], v[2], v[3]);
}

void dump_mat3(mat3 m) {
	printf("(%.5f %.5f %.5f\n %.5f %.5f %.5f\n %.5f %.5f %.5f)\n",
		m[0][0], m[0][1], m[0][2],
		m[1][0], m[1][1], m[1][2],
		m[2][0], m[2][1], m[2][2]);
}

#endif

// SVD
////////////////////////////////////////////////////////////////////////////////

const float PSUEDO_INVERSE_THRESHOLD = 0.1f;

float svd_dot(IN(qef_vec3,a), IN(qef_vec3,b))
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void svd_vec_sub(OUT(qef_vec3,result), IN(qef_vec3,a), IN(qef_vec3,b))
{
	result[0] = a[0] - b[0];
	result[1] = a[1] - b[1];
	result[2] = a[2] - b[2];
}

void svd_mul_matrix_vec(OUT(qef_vec3,result), IN(qef_mat3,a), IN(qef_vec3,b))
{
	result[0] = a[0][0] * b[0] + a[0][1] * b[1] + a[0][2] * b[2];
	result[1] = a[1][0] * b[0] + a[1][1] * b[1] + a[1][2] * b[2];
	result[2] = a[2][0] * b[0] + a[2][1] * b[1] + a[2][2] * b[2];
}

void givens_coeffs_sym(float a_pp, float a_pq, float a_qq, OUT(float,c), OUT(float,s)) {
	if (a_pq == 0.f) {
		c = 1.f;
		s = 0.f;
		return;
	}
	float tau = (a_qq - a_pp) / (2.f * a_pq);
	float stt = sqrt(1.f + tau * tau);
	float tan = 1.f / ((tau >= 0.f) ? (tau + stt) : (tau - stt));
	c = rsqrt(1.f + tan * tan);
	s = tan * c;
}

void svd_rotate_xy(INOUT(float,x), INOUT(float,y), IN(float,c), IN(float,s)) {
	float u = x; float v = y;
	x = c * u - s * v;
	y = s * u + c * v;
}

void svd_rotateq_xy(INOUT(float,x), INOUT(float,y), INOUT(float,a), IN(float,c), IN(float,s)) {
	float cc = c * c; float ss = s * s;
	float mx = 2.0 * c * s * a;
	float u = x; float v = y;
	x = cc * u - mx + ss * v;
	y = ss * u + mx + cc * v;
}

#if USE_WEBGL

void svd_rotate01(INOUT(mat3,vtav), INOUT(mat3,v)) {
	if (vtav[0][1] == 0.0) return;
	
	float c, s;
	givens_coeffs_sym(vtav[0][0], vtav[0][1], vtav[1][1], c, s);
	svd_rotateq_xy(vtav[0][0],vtav[1][1],vtav[0][1],c,s);
	svd_rotate_xy(vtav[0][2], vtav[1][2], c, s);
	vtav[0][1] = 0.0;
	
	svd_rotate_xy(v[0][0], v[0][1], c, s);
	svd_rotate_xy(v[1][0], v[1][1], c, s);
	svd_rotate_xy(v[2][0], v[2][1], c, s);
}

void svd_rotate02(INOUT(mat3,vtav), INOUT(mat3,v)) {
	if (vtav[0][2] == 0.0) return;
	
	float c, s;
	givens_coeffs_sym(vtav[0][0], vtav[0][2], vtav[2][2], c, s);
	svd_rotateq_xy(vtav[0][0],vtav[2][2],vtav[0][2],c,s);
	svd_rotate_xy(vtav[0][1], vtav[1][2], c, s);
	vtav[0][2] = 0.0;
	
	svd_rotate_xy(v[0][0], v[0][2], c, s);
	svd_rotate_xy(v[1][0], v[1][2], c, s);
	svd_rotate_xy(v[2][0], v[2][2], c, s);
}

void svd_rotate12(INOUT(mat3,vtav), INOUT(mat3,v)) {
	if (vtav[1][2] == 0.0) return;
	
	float c, s;
	givens_coeffs_sym(vtav[1][1], vtav[1][2], vtav[2][2], c, s);
	svd_rotateq_xy(vtav[1][1],vtav[2][2],vtav[1][2],c,s);
	svd_rotate_xy(vtav[0][1], vtav[0][2], c, s);
	vtav[1][2] = 0.0;
	
	svd_rotate_xy(v[0][1], v[0][2], c, s);
	svd_rotate_xy(v[1][1], v[1][2], c, s);
	svd_rotate_xy(v[2][1], v[2][2], c, s);
}

#else

void svd_rotate(INOUT(qef_mat3,vtav), INOUT(qef_mat3,v), IN(int,a), IN(int,b)) {
	if (vtav[a][b] == 0.0) return;
	
	float c, s;
	givens_coeffs_sym(vtav[a][a], vtav[a][b], vtav[b][b], c, s);
	svd_rotateq_xy(vtav[a][a],vtav[b][b],vtav[a][b],c,s);
	svd_rotate_xy(vtav[0][3-b], vtav[1-a][2], c, s);
	vtav[a][b] = 0.0;
	
	svd_rotate_xy(v[0][a], v[0][b], c, s);
	svd_rotate_xy(v[1][a], v[1][b], c, s);
	svd_rotate_xy(v[2][a], v[2][b], c, s);
}


#endif

void svd_solve_sym(IN(mat_upper_tri,a), OUT(qef_vec3,sigma), INOUT(qef_mat3,v)) {
	// assuming that A is symmetric: can optimize all operations for 
	// the upper right triagonal
	qef_mat3 vtav;
	vtav[0][0] = a[0]; vtav[0][1] = a[1]; vtav[0][2] = a[2]; 
	vtav[1][0] = 0.f;  vtav[1][1] = a[3]; vtav[1][2] = a[4]; 
	vtav[2][0] = 0.f;  vtav[2][1] = 0.f;  vtav[2][2] = a[5]; 

	// assuming V is identity: you can also pass a matrix the rotations
	// should be applied to
	// U is not computed

	for (int i = 0; i < SVD_NUM_SWEEPS; ++i) {
		svd_rotate(vtav, v, 0, 1);
		svd_rotate(vtav, v, 0, 2);
		svd_rotate(vtav, v, 1, 2);
	}
	sigma[0] = vtav[0][0];
	sigma[1] = vtav[1][1];
	sigma[2] = vtav[2][2];
}

float svd_invdet(float x, float tol) {
	return (abs(x) < tol || abs(1.0 / x) < tol) ? 0.0 : (1.0 / x);
}

void svd_pseudoinverse(OUT(qef_mat3,o), IN(qef_vec3,sigma), IN(qef_mat3,v)) {
	float d0 = svd_invdet(sigma[0], PSUEDO_INVERSE_THRESHOLD);
	float d1 = svd_invdet(sigma[1], PSUEDO_INVERSE_THRESHOLD);
	float d2 = svd_invdet(sigma[2], PSUEDO_INVERSE_THRESHOLD);

	o[0][0] = v[0][0] * d0 * v[0][0] + v[0][1] * d1 * v[0][1] + v[0][2] * d2 * v[0][2];
	o[0][1] = v[0][0] * d0 * v[1][0] + v[0][1] * d1 * v[1][1] + v[0][2] * d2 * v[1][2];
	o[0][2] = v[0][0] * d0 * v[2][0] + v[0][1] * d1 * v[2][1] + v[0][2] * d2 * v[2][2];
	o[1][0] = v[1][0] * d0 * v[0][0] + v[1][1] * d1 * v[0][1] + v[1][2] * d2 * v[0][2];
	o[1][1] = v[1][0] * d0 * v[1][0] + v[1][1] * d1 * v[1][1] + v[1][2] * d2 * v[1][2];
	o[1][2] = v[1][0] * d0 * v[2][0] + v[1][1] * d1 * v[2][1] + v[1][2] * d2 * v[2][2];
	o[2][0] = v[2][0] * d0 * v[0][0] + v[2][1] * d1 * v[0][1] + v[2][2] * d2 * v[0][2];
	o[2][1] = v[2][0] * d0 * v[1][0] + v[2][1] * d1 * v[1][1] + v[2][2] * d2 * v[1][2];
	o[2][2] = v[2][0] * d0 * v[2][0] + v[2][1] * d1 * v[2][1] + v[2][2] * d2 * v[2][2];
}

void svd_solve_ATA_ATb(
//	IN(mat3,ATA), IN(vec3,ATb), OUT(vec3,x)
	IN(mat_upper_tri,ATA), IN(qef_vec3,ATb), OUT(qef_vec3,x)
) {
	qef_mat3 V;
	V[0][0] = 1.f; V[0][1] = 0.f; V[0][2] = 0.f;
	V[1][0] = 0.f; V[1][1] = 1.f; V[1][2] = 0.f;
	V[2][0] = 0.f; V[2][1] = 0.f; V[2][2] = 1.f;

	qef_vec3 sigma;
	svd_solve_sym(ATA, sigma, V);
	
	// A = UEV^T; U = A / (E*V^T)
#if DEBUG_SVD
	
	printf("ATA="); dump_mat3(ATA);
	printf("ATb="); dump_vec3(ATb);
	printf("V="); dump_mat3(V);
	printf("sigma="); dump_vec3(sigma);
#endif
	qef_mat3 Vinv;
	svd_pseudoinverse(Vinv, sigma, V);
	svd_mul_matrix_vec(x, Vinv, ATb);

#if DEBUG_SVD
	printf("Vinv="); dump_mat3(Vinv);
#endif
}

//vec3 svd_vmul_sym(IN(mat_upper_tri,a), IN(vec3,v)) {
void svd_vmul_sym(OUT(qef_vec3,result), IN(mat_upper_tri,A), IN(qef_vec3,v)) {
	qef_vec3 A_row_x = qef_vec3(0.f);
	A_row_x[0] = A[0]; A_row_x[1] = A[1]; A_row_x[2] = A[2]; 

	result[0] = svd_dot(A_row_x, v);
	result[1] = A[1] * v[0] + A[3] * v[1] + A[4] * v[2];
	result[2] = A[2] * v[0] + A[4] * v[1] + A[5] * v[2];
}

#if 0
void svd_mul_ata_sym(OUT(mat3,o), IN(mat3,a))
{
	o[0][0] = a[0][0] * a[0][0] + a[1][0] * a[1][0] + a[2][0] * a[2][0];
	o[0][1] = a[0][0] * a[0][1] + a[1][0] * a[1][1] + a[2][0] * a[2][1];
	o[0][2] = a[0][0] * a[0][2] + a[1][0] * a[1][2] + a[2][0] * a[2][2];
	o[1][1] = a[0][1] * a[0][1] + a[1][1] * a[1][1] + a[2][1] * a[2][1];
	o[1][2] = a[0][1] * a[0][2] + a[1][1] * a[1][2] + a[2][1] * a[2][2];
	o[2][2] = a[0][2] * a[0][2] + a[1][2] * a[1][2] + a[2][2] * a[2][2];
}
	
void svd_solve_Ax_b(IN(mat3,a), IN(vec3,b), OUT(mat3,ATA), OUT(vec3,ATb), OUT(vec3,x)) {
	svd_mul_ata_sym(ATA, a);
	ATb = b * a; // transpose(a) * b;
	svd_solve_ATA_ATb(ATA, ATb, x);
}
#endif

// QEF
////////////////////////////////////////////////////////////////////////////////

void qef_add(
	IN(qef_vec3,n), IN(qef_vec3,p),
	INOUT(mat_upper_tri,ATA), 
	INOUT(qef_vec3,ATb),
	INOUT(qef_vec4,pointaccum))
{
#if DEBUG_SVD
	printf("+plane=");dump_vec4(vec4(n, dot(-p,n)));
#endif	
	ATA[0] += n[0] * n[0];
	ATA[1] += n[0] * n[1];
	ATA[2] += n[0] * n[2];
	ATA[3] += n[1] * n[1];
	ATA[4] += n[1] * n[2];
	ATA[5] += n[2] * n[2];

	float b = svd_dot(p, n);
	ATb[0] += n[0] * b;
	ATb[1] += n[1] * b;
	ATb[2] += n[2] * b;

	pointaccum[0] += p[0];
	pointaccum[1] += p[1];
	pointaccum[2] += p[2];
	pointaccum[3] += 1.f;
}

float qef_calc_error(IN(mat_upper_tri,A), IN(qef_vec3, x), IN(qef_vec3, b)) {
	qef_vec3 tmp;

	svd_vmul_sym(tmp, A, x);
	svd_vec_sub(tmp, b, tmp);

	return svd_dot(tmp, tmp);
}

float qef_solve(
	IN(mat_upper_tri,ATA), 
	IN(qef_vec3,ATb),
	IN(qef_vec4,pointaccum),
	OUT(qef_vec3,x)
) {
	qef_vec3 masspoint;
	masspoint[0] = pointaccum[0] / pointaccum[3];
	masspoint[1] = pointaccum[1] / pointaccum[3];
	masspoint[2] = pointaccum[2] / pointaccum[3];

	qef_vec3 A_mp;
	svd_vmul_sym(A_mp, ATA, masspoint);
	svd_vec_sub(A_mp, ATb, A_mp);

	svd_solve_ATA_ATb(ATA, A_mp, x);

	float result = qef_calc_error(ATA, x, ATb);
	x[0] += masspoint[0];
	x[1] += masspoint[1];
	x[2] += masspoint[2];
		
	return result;
}

QEFData qef_construct(
	const qef_vec3* positions,
	const qef_vec3* normals,
	const size_t count)
{
	QEFData data;

	for (size_t i = 0; i < count; i++)
	{
		const qef_vec3& p = positions[i];
		const qef_vec3& n = normals[i];

		data.ATA[0] += n[0] * n[0];
		data.ATA[1] += n[0] * n[1];
		data.ATA[2] += n[0] * n[2];
		data.ATA[3] += n[1] * n[1];
		data.ATA[4] += n[1] * n[2];
		data.ATA[5] += n[2] * n[2];

		const float b = svd_dot(p, n);
		data.ATb[0] += n[0] * b;
		data.ATb[1] += n[1] * b;
		data.ATb[2] += n[2] * b;
		
		data.masspoint[0] += p[0];
		data.masspoint[1] += p[1];
		data.masspoint[2] += p[2];
		data.masspoint[3] += 1.f;
	}

	data.masspoint[0] = data.masspoint[0] / data.masspoint[3];
	data.masspoint[1] = data.masspoint[1] / data.masspoint[3];
	data.masspoint[2] = data.masspoint[2] / data.masspoint[3];
	data.masspoint[3] = 1.f;
	return data;
}

void qef_add(QEFData& a, const QEFData& b)
{
	a.ATA[0] += b.ATA[0];
	a.ATA[1] += b.ATA[1];
	a.ATA[2] += b.ATA[2];
	a.ATA[3] += b.ATA[3];
	a.ATA[4] += b.ATA[4];
	a.ATA[5] += b.ATA[5];

	a.ATb[0] += b.ATb[0];
	a.ATb[1] += b.ATb[1];
	a.ATb[2] += b.ATb[2];

	a.masspoint[0] += b.masspoint[0];
	a.masspoint[1] += b.masspoint[1];
	a.masspoint[2] += b.masspoint[2];
	a.masspoint[3] += 1.f;
}

float qef_solve(
	const QEFData& data,
	qef_vec3& solved_position)
{
	qef_vec3 ATb, solved_p;
	qef_vec4 masspoint;

	ATb[0] = data.ATb[0];
	ATb[1] = data.ATb[1];
	ATb[2] = data.ATb[2];

	masspoint[0] = data.masspoint[0];
	masspoint[1] = data.masspoint[1];
	masspoint[2] = data.masspoint[2];
	masspoint[3] = data.masspoint[3];

	auto error = qef_solve(data.ATA, ATb, masspoint, solved_p);
	solved_position[0] = solved_p[0];
	solved_position[1] = solved_p[1];
	solved_position[2] = solved_p[2];
	return error;
}

float qef_solve_from_points(
	const qef_vec3* positions,
	const qef_vec3* normals,
	const size_t count,
	qef_vec3& solved_position) 
{
	qef_vec4 pointaccum(0.f);
	qef_vec3 ATb(0.f);
	mat_upper_tri ATA = {0.f};
	
	qef_vec3 n[12], p[12];
	for (int i = 0; i < count; i++)	{
		p[i][0] = positions[i][0];
		p[i][1] = positions[i][1];
		p[i][2] = positions[i][2];

		n[i][0] = normals[i][0];
		n[i][1] = normals[i][1];
		n[i][2] = normals[i][2];
	}

	for (int i= 0; i < count; ++i) {
		qef_add(n[i],p[i],ATA,ATb,pointaccum);
	}
	
	return qef_solve(ATA,ATb,pointaccum,solved_position);
}



// Test
////////////////////////////////////////////////////////////////////////////////

#if USE_GLSL

void main(void) {
}

#else

#if DEBUG_SVD
#undef sqrt
#undef abs
#undef rsqrt
#undef max

#if SVD_COMPARE_REFERENCE
#include "qr_solve.h"
#include "r8lib.h"
#include "qr_solve.c"
#include "r8lib.c"
#endif

int main(void) {
	vec4 pointaccum = vec4(0.0);
	mat3 ATA = mat3(0.0);
	vec3 ATb = vec3(0.0);
	
	#define COUNT 5
	vec3 normals[COUNT] = {
		normalize(vec3( 1.0,1.0,0.0)),
		normalize(vec3( 1.0,1.0,0.0)),
		normalize(vec3(-1.0,1.0,0.0)),
		normalize(vec3(-1.0,2.0,1.0)),
		//normalize(vec3(-1.0,1.0,0.0)),
		normalize(vec3(-1.0,1.0,0.0)),
	};
	vec3 points[COUNT] = {
		vec3(  1.0,0.0,0.3),
		vec3(  0.9,0.1,-0.5),
		vec3( -0.8,0.2,0.6),
		vec3( -1.0,0.0,0.01),
		vec3( -1.1,-0.1,-0.5),
	};
	
	for (int i= 0; i < COUNT; ++i) {
		qef_add(normals[i],points[i],ATA,ATb,pointaccum);
	}
	vec3 com = SWIZZLE_XYZ(pointaccum) / pointaccum.w;
	
	vec3 x;
	float error = qef_solve(ATA,ATb,pointaccum,x);

	printf("masspoint = (%.5f %.5f %.5f)\n", com.x, com.y, com.z);
	printf("point = (%.5f %.5f %.5f)\n", x.x, x.y, x.z);
	printf("error = %.5f\n", error);

#if SVD_COMPARE_REFERENCE
	double a[COUNT*3];
	double b[COUNT];
	
	for (int i = 0; i < COUNT; ++i) {
		b[i] = (points[i].x - com.x)*normals[i].x
			 + (points[i].y - com.y)*normals[i].y
			 + (points[i].z - com.z)*normals[i].z;
		a[i] = normals[i].x;
		a[i+COUNT] = normals[i].y;
		a[i+2*COUNT] = normals[i].z;
	}
	
	double *c = svd_solve(5,3,a,b,0.1);
	
	vec3 result = com + vec3(c[0], c[1], c[2]);
	r8_free(c);
	printf("reference="); dump_vec3(result);
#endif
	
	return 0;
}
#endif

#endif
