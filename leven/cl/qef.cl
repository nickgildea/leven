// minimal SVD implementation for calculating feature points from hermite data
// public domain

typedef float mat3x3[3][3];
typedef float mat3x3_tri[6];

typedef struct QEFData_s
{
	mat3x3_tri	ATA;
	float		pad[2];
	float4		ATb;
	float4		masspoint;
}
QEFData;

#define SVD_NUM_SWEEPS (10)

// SVD
////////////////////////////////////////////////////////////////////////////////

#define PSUEDO_INVERSE_THRESHOLD (0.1f)

void svd_mul_matrix_vec(float4* result, mat3x3 a, float4 b)
{
	(*result).x = dot((float4)(a[0][0], a[0][1], a[0][2], 0.f), b);
	(*result).y = dot((float4)(a[1][0], a[1][1], a[1][2], 0.f), b);
	(*result).z = dot((float4)(a[2][0], a[2][1], a[2][2], 0.f), b);
	(*result).w = 0.f;
}

void givens_coeffs_sym(float a_pp, float a_pq, float a_qq, float* c, float* s) {
	if (a_pq == 0.f) {
		*c = 1.f;
		*s = 0.f;
		return;
	}
	float tau = (a_qq - a_pp) / (2.f * a_pq);
	float stt = sqrt(1.f + tau * tau);
	float tan = 1.f / ((tau >= 0.f) ? (tau + stt) : (tau - stt));
	*c = rsqrt(1.f + tan * tan);
	*s = tan * (*c);
}

void svd_rotate_xy(float* x, float* y, float c, float s) {
	float u = *x; float v = *y;
	*x = c * u - s * v;
	*y = s * u + c * v;
}

void svd_rotateq_xy(float* x, float* y, float* a, float c, float s) {
	float cc = c * c; float ss = s * s;
	float mx = 2.0 * c * s * (*a);
	float u = *x; float v = *y;
	*x = cc * u - mx + ss * v;
	*y = ss * u + mx + cc * v;
}

void svd_rotate(mat3x3 vtav, mat3x3 v, int a, int b) {
	if (vtav[a][b] == 0.0) return;
	
	float c, s;
	givens_coeffs_sym(vtav[a][a], vtav[a][b], vtav[b][b], &c, &s);

	float x, y, z;
	x = vtav[a][a]; y = vtav[b][b]; z = vtav[a][b];
	svd_rotateq_xy(&x,&y,&z,c,s);
	vtav[a][a] = x; vtav[b][b] = y; vtav[a][b] = z;

	x = vtav[0][3-b]; y = vtav[1-a][2];
	svd_rotate_xy(&x, &y, c, s);
	vtav[0][3-b] = x; vtav[1-a][2] = y;

	vtav[a][b] = 0.0;
	
	x = v[0][a]; y = v[0][b];
	svd_rotate_xy(&x, &y, c, s);
	v[0][a] = x; v[0][b] = y;

	x = v[1][a]; y = v[1][b];
	svd_rotate_xy(&x, &y, c, s);
	v[1][a] = x; v[1][b] = y;

	x = v[2][a]; y = v[2][b];
	svd_rotate_xy(&x, &y, c, s);
	v[2][a] = x; v[2][b] = y;
}

void svd_solve_sym(mat3x3_tri a, float4* sigma, mat3x3 v) {
	// assuming that A is symmetric: can optimize all operations for 
	// the upper right triagonal
	mat3x3 vtav;
	vtav[0][0] = a[0]; vtav[0][1] = a[1]; vtav[0][2] = a[2]; 
	vtav[1][0] = 0.f;  vtav[1][1] = a[3]; vtav[1][2] = a[4]; 
	vtav[2][0] = 0.f;  vtav[2][1] = 0.f;  vtav[2][2] = a[5]; 

	// assuming V is identity: you can also pass a matrix the rotations
	// should be applied to. (U is not computed)
	for (int i = 0; i < SVD_NUM_SWEEPS; ++i) {
		svd_rotate(vtav, v, 0, 1);
		svd_rotate(vtav, v, 0, 2);
		svd_rotate(vtav, v, 1, 2);
	}

	*sigma = (float4)(vtav[0][0], vtav[1][1], vtav[2][2], 0.f);
}

float svd_invdet(float x, float tol) {
	return (fabs(x) < tol || fabs(1.0 / x) < tol) ? 0.0 : (1.0 / x);
}

void svd_pseudoinverse(mat3x3 o, float4 sigma, mat3x3 v) {
	float d0 = svd_invdet(sigma.x, PSUEDO_INVERSE_THRESHOLD);
	float d1 = svd_invdet(sigma.y, PSUEDO_INVERSE_THRESHOLD);
	float d2 = svd_invdet(sigma.z, PSUEDO_INVERSE_THRESHOLD);

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
	mat3x3_tri ATA, 
	float4 ATb, 
	float4* x) 
{
	mat3x3 V;
	V[0][0] = 1.f; V[0][1] = 0.f; V[0][2] = 0.f;
	V[1][0] = 0.f; V[1][1] = 1.f; V[1][2] = 0.f;
	V[2][0] = 0.f; V[2][1] = 0.f; V[2][2] = 1.f;

	float4 sigma = { 0.f, 0.f, 0.f, 0.f };
	svd_solve_sym(ATA, &sigma, V);
	
	// A = UEV^T; U = A / (E*V^T)
	mat3x3 Vinv;
	svd_pseudoinverse(Vinv, sigma, V);
	svd_mul_matrix_vec(x, Vinv, ATb);
}

void svd_vmul_sym(float4* result, mat3x3_tri A, float4 v) {
	float4 A_row_x = { A[0], A[1], A[2], 0.f };

	(*result).x = dot(A_row_x, v);
	(*result).y = A[1] * v.x + A[3] * v.y + A[4] * v.z;
	(*result).z = A[2] * v.x + A[4] * v.y + A[5] * v.z;
}

// QEF
////////////////////////////////////////////////////////////////////////////////

void qef_initialise(QEFData* qef)
{
	qef->ATA[0] = 0.f;
	qef->ATA[1] = 0.f;
	qef->ATA[2] = 0.f;
	qef->ATA[3] = 0.f;
	qef->ATA[4] = 0.f;
	qef->ATA[5] = 0.f;

	qef->ATb = (float4)(0.f, 0.f, 0.f, 0.f);
	qef->masspoint = (float4)(0.f, 0.f, 0.f, 0.f);
}

void qef_add_point(
	QEFData* qef,
	float4 n, 
	float4 p)
{
	qef->ATA[0] += n.x * n.x;
	qef->ATA[1] += n.x * n.y;
	qef->ATA[2] += n.x * n.z;
	qef->ATA[3] += n.y * n.y;
	qef->ATA[4] += n.y * n.z;
	qef->ATA[5] += n.z * n.z;

	float b = dot(p, n);
	qef->ATb.x += n.x * b;
	qef->ATb.y += n.y * b;
	qef->ATb.z += n.z * b;

	qef->masspoint.x += p.x;
	qef->masspoint.y += p.y;
	qef->masspoint.z += p.z;
	qef->masspoint.w += 1.f;
}

void qef_add(
	QEFData* result,
	const QEFData* a,
	const QEFData* b)
{
	result->ATA[0] = a->ATA[0] + b->ATA[0];
	result->ATA[1] = a->ATA[1] + b->ATA[1];
	result->ATA[2] = a->ATA[2] + b->ATA[2];
	result->ATA[3] = a->ATA[3] + b->ATA[3];
	result->ATA[4] = a->ATA[4] + b->ATA[4];
	result->ATA[5] = a->ATA[5] + b->ATA[5];

	result->ATb = a->ATb + b->ATb;
	result->masspoint = a->masspoint + b->masspoint;
	result->masspoint /= result->masspoint.w;
}

float qef_calc_error(mat3x3_tri A, float4 x, float4 b) {
	float4 tmp;

	svd_vmul_sym(&tmp, A, x);
	tmp = b - tmp;

	return dot(tmp, tmp);
}

float qef_solve_(
	mat3x3_tri ATA, 
	float4 ATb,
	float4 pointaccum,
	float4* x) 
{
	float4 masspoint = pointaccum / pointaccum.w;

	float4 A_mp = { 0.f, 0.f, 0.f, 0.f };
	svd_vmul_sym(&A_mp, ATA, masspoint);
	A_mp = ATb - A_mp;

	svd_solve_ATA_ATb(ATA, A_mp, x);

	float error = qef_calc_error(ATA, *x, ATb);
	(*x) += masspoint;
		
	return error;
}

float qef_solve(
	QEFData* qef,
	float4* solvedPosition)
{
	// prevent a div-by-zero exception
	qef->masspoint /= max(qef->masspoint.w, 1.f);

	float4 A_mp = { 0.f, 0.f, 0.f, 0.f };
	svd_vmul_sym(&A_mp, qef->ATA, qef->masspoint);
	A_mp = qef->ATb - A_mp;

	svd_solve_ATA_ATb(qef->ATA, A_mp, solvedPosition);

	float error = qef_calc_error(qef->ATA, *solvedPosition, qef->ATb);
	(*solvedPosition) += qef->masspoint;
		
	return error;
}

float4 qef_solve_from_points(
	const float4* positions,
	const float4* normals,
	const size_t count,
	float* error)
{
	QEFData qef;
	qef.ATA[0] = 0.f;
	qef.ATA[1] = 0.f;
	qef.ATA[2] = 0.f;
	qef.ATA[3] = 0.f;
	qef.ATA[4] = 0.f;
	qef.ATA[5] = 0.f;
	qef.ATb = (float4)(0.f, 0.f, 0.f, 0.f);
	qef.masspoint = (float4)(0.f, 0.f, 0.f, 0.f);
	
	for (int i= 0; i < count; ++i) {
		qef_add_point(&qef, normals[i], positions[i]);
	}
	
	float4 solved_position = { 0.f, 0.f, 0.f, 0.f };
	*error = qef_solve(&qef, &solved_position);
	return solved_position;
}

void qef_create_from_points(
	const float4* positions,
	const float4* normals,
	const size_t count,
	QEFData* qef)
{
	qef->ATA[0] = 0.f;
	qef->ATA[1] = 0.f;
	qef->ATA[2] = 0.f;
	qef->ATA[3] = 0.f;
	qef->ATA[4] = 0.f;
	qef->ATA[5] = 0.f;
	qef->ATb = (float4)(0.f, 0.f, 0.f, 0.f);
	qef->masspoint = (float4)(0.f, 0.f, 0.f, 0.f);

	for (int i= 0; i < count; ++i) {
		qef_add_point(qef, normals[i], positions[i]);
	}

	qef->masspoint /= qef->masspoint.w;
}


