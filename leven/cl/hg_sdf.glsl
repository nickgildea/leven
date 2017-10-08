////////////////////////////////////////////////////////////////
//
//                           HG_SDF
//
//     GLSL LIBRARY FOR BUILDING SIGNED DISTANCE BOUNDS
//
//     version 2015-12-15 (initial release)
//
//     Check http://mercury.sexy/hg_sdf for updates
//     and usage examples. Send feedback to spheretracing@mercury.sexy.
//
//     Brought to you by MERCURY http://mercury.sexy
//
//
//
// Released as Creative Commons Attribution-NonCommercial (CC BY-NC)
//
////////////////////////////////////////////////////////////////
//
// How to use this:
//
// 1. Build some system to #include glsl files in each other.
//   Include this one at the very start. Or just paste everywhere.
// 2. Build a sphere tracer. See those papers:
//   * "Sphere Tracing" http://graphics.cs.illinois.edu/sites/default/files/zeno.pdf
//   * "Enhanced Sphere Tracing" http://lgdv.cs.fau.de/get/2234
//   The Raymnarching Toolbox Thread on pouet can be helpful as well
//   http://www.pouet.net/topic.php?which=7931&page=1
//   and contains links to many more resources.
// 3. Use the tools in this library to build your distance bound f().
// 4. ???
// 5. Win a compo.
// 
// (6. Buy us a beer or a good vodka or something, if you like.)
//
////////////////////////////////////////////////////////////////
//
// Table of Contents:
//
// * Helper functions and macros
// * Collection of some primitive objects
// * Domain Manipulation operators
// * Object combination operators
//
////////////////////////////////////////////////////////////////
//
// Why use this?
//
// The point of this lib is that everything is structured according
// to patterns that we ended up using when building geometry.
// It makes it more easy to write code that is reusable and that somebody
// else can actually understand. Especially code on Shadertoy (which seems
// to be what everybody else is looking at for "inspiration") tends to be
// really ugly. So we were forced to do something about the situation and
// release this lib ;)
//
// Everything in here can probably be done in some better way.
// Please experiment. We'd love some feedback, especially if you
// use it in a scene production.
//
// The main patterns for building geometry this way are:
// * Stay Lipschitz continuous. That means: don't have any distance
//   gradient larger than 1. Try to be as close to 1 as possible -
//   Distances are euclidean distances, don't fudge around.
//   Underestimating distances will happen. That's why calling
//   it a "distance bound" is more correct. Don't ever multiply
//   distances by some value to "fix" a Lipschitz continuity
//   violation. The invariant is: each fSomething() function returns
//   a correct distance bound.
// * Use very few primitives and combine them as building blocks
//   using combine opertors that preserve the invariant.
// * Multiply objects by repeating the domain (space).
//   If you are using a loop inside your distance function, you are
//   probably doing it wrong (or you are building boring fractals).
// * At right-angle intersections between objects, build a new local
//   coordinate system from the two distances to combine them in
//   interesting ways.
// * As usual, there are always times when it is best to not follow
//   specific patterns.
//
////////////////////////////////////////////////////////////////
//
// FAQ
//
// Q: Why is there no sphere tracing code in this lib?
// A: Because our system is way too complex and always changing.
//    This is the constant part. Also we'd like everyone to
//    explore for themselves.
//
// Q: This does not work when I paste it into Shadertoy!!!!
// A: Yes. It is GLSL, not GLSL ES. We like real OpenGL
//    because it has way more features and is more likely
//    to work compared to browser-based WebGL. We recommend
//    you consider using OpenGL for your productions. Most
//    of this can be ported easily though.
//
// Q: How do I material?
// A: We recommend something like this:
//    Write a material ID, the distance and the local coordinate
//    p into some global variables whenever an object's distance is
//    smaller than the stored distance. Then, at the end, evaluate
//    the material to get color, roughness, etc., and do the shading.
//
// Q: I found an error. Or I made some function that would fit in
//    in this lib. Or I have some suggestion.
// A: Awesome! Drop us a mail at spheretracing@mercury.sexy.
//
// Q: Why is this not on github?
// A: Because we were too lazy. If we get bugged about it enough,
//    we'll do it.
//
// Q: Your license sucks for me.
// A: Oh. What should we change it to?
//
// Q: I have trouble understanding what is going on with my distances.
// A: Some visualization of the distance field helps. Try drawing a
//    plane that you can sweep through your scene with some color
//    representation of the distance field at each point and/or iso
//    lines at regular intervals. Visualizing the length of the
//    gradient (or better: how much it deviates from being equal to 1)
//    is immensely helpful for understanding which parts of the
//    distance field are broken.
//
////////////////////////////////////////////////////////////////






////////////////////////////////////////////////////////////////
//
//             HELPER FUNCTIONS/MACROS
//
////////////////////////////////////////////////////////////////

#define PI 3.14159265f
#define TAU (2.f*PI)
#define PHI (2.2360679f*0.5f + 0.5f)

// Clamp to [0,1] - this operation is free under certain circumstances.
// For further information see
// http://www.humus.name/Articles/Persson_LowLevelThinking.pdf and
// http://www.humus.name/Articles/Persson_LowlevelShaderOptimization.pdf
#define saturate(x) clamp(x, 0.f, 1.f)

// Sign function that doesn't return 0
float sgn(float x) {
	return (x<0)?-1:1;
}

#define square(x) ((x)*(x))

float lengthSqr(float3 x) {
	return dot(x, x);
}


// Maximum/minumum elements of a vector
float vmax2(float2 v) {
	return max(v.x, v.y);
}

float vmax3(float3 v) {
	return max(max(v.x, v.y), v.z);
}

float vmax4(float4 v) {
	return max(max(v.x, v.y), max(v.z, v.w));
}

float vmin2(float2 v) {
	return min(v.x, v.y);
}

float vmin3(float3 v) {
	return min(min(v.x, v.y), v.z);
}

float vmin4(float4 v) {
	return min(min(v.x, v.y), min(v.z, v.w));
}




////////////////////////////////////////////////////////////////
//
//             PRIMITIVE DISTANCE FUNCTIONS
//
////////////////////////////////////////////////////////////////
//
// Conventions:
//
// Everything that is a distance function is called fSomething.
// The first argument is always a point in 2 or 3-space called <p>.
// Unless otherwise noted, (if the object has an intrinsic "up"
// side or direction) the y axis is "up" and the object is
// centered at the origin.
//
////////////////////////////////////////////////////////////////

float fSphere(float3 p, float r) {
	return length(p) - r;
}

// Plane with normal n (n is normalized) at some distance from the origin
float fPlane(float3 p, float3 n, float distanceFromOrigin) {
	return dot(p, n) + distanceFromOrigin;
}

// Cheap Box: distance to corners is overestimated
float fBoxCheap(float3 p, float3 b) { //cheap box
	return vmax3(fabs(p) - b);
}

// Box: correct distance to corners
float fBox(float3 p, float3 b) {
	float3 d = fabs(p) - b;
	return length(max(d, (float3)(0))) + vmax3(min(d, (float3)(0)));
}

// Same as above, but in two dimensions (an endless box)
float fBox2Cheap(float2 p, float2 b) {
	return vmax2(fabs(p)-b);
}

float fBox2(float2 p, float2 b) {
	float2 d = fabs(p) - b;
	return length(max(d, (float2)(0))) + vmax2(min(d, (float2)(0)));
}


// Endless "corner"
float fCorner (float2 p) {
	return length(max(p, (float2)(0))) + vmax2(min(p, (float2)(0)));
}

// Blobby ball object. You've probably seen it somewhere. This is not a correct distance bound, beware.
float fBlob(float3 p) {
	p = fabs(p);
	if (p.x < max(p.y, p.z)) p = p.yzx;
	if (p.x < max(p.y, p.z)) p = p.yzx;
	float b = max(max(max(
		dot(p, normalize((float3)(1, 1, 1))),
		dot(p.xz, normalize((float2)(PHI+1, 1)))),
		dot(p.yx, normalize((float2)(1, PHI)))),
		dot(p.xz, normalize((float2)(1, PHI))));
	float l = length(p);
	return l - 1.5f - 0.2f * (1.5f / 2.f)* cos(min(sqrt(1.01f - b / l)*(PI / 0.25f), PI));
}

// Cylinder standing upright on the xz plane
float fCylinder(float3 p, float r, float height) {
	float d = length(p.xz) - r;
	d = max(d, fabs(p.y) - height);
	return d;
}

// Capsule: A Cylinder with round caps on both sides
float fCapsule(float3 p, float r, float c) {
	return mix(length(p.xz) - r, length((float3)(p.x, fabs(p.y) - c, p.z)) - r, step(c, fabs(p.y)));
}

// Distance to line segment between <a> and <b>, used for fCapsule() version 2below
float fLineSegment(float3 p, float3 a, float3 b) {
	float3 ab = b - a;
	float t = saturate(dot(p - a, ab) / dot(ab, ab));
	return length((ab*t + a) - p);
}

// Capsule version 2: between two end points <a> and <b> with radius r 
/*
float fCapsule(float3 p, float3 a, float3 b, float r) {
	return fLineSegment(p, a, b) - r;
}
*/

// Torus in the XZ-plane
float fTorus(float3 p, float smallRadius, float largeRadius) {
	return length((float2)(length(p.xz) - largeRadius, p.y)) - smallRadius;
}

// A circle line. Can also be used to make a torus by subtracting the smaller radius of the torus.
float fCircle(float3 p, float r) {
	float l = length(p.xz) - r;
	return length((float2)(p.y, l));
}

// A circular disc with no thickness (i.e. a cylinder with no height).
// Subtract some value to make a flat disc with rounded edge.
float fDisc(float3 p, float r) {
	float l = length(p.xz) - r;
	return l < 0 ? fabs(p.y) : length((float2)(p.y, l));
}

// Hexagonal prism, circumcircle variant
float fHexagonCircumcircle(float3 p, float2 h) {
	float3 q = fabs(p);
	float x = q.x*sqrt(3.0)*0.5 + q.z*0.5;
	return max(q.y - h.y, max(x, q.z) - h.x);
	//this is mathematically equivalent to this line, but less efficient:
	//return max(q.y - h.y, max(dot((float2)(cos(PI/3), sin(PI/3)), q.zx), q.z) - h.x);
}

// Hexagonal prism, incircle variant
float fHexagonIncircle(float3 p, float2 h) {
	return fHexagonCircumcircle(p, (float2)(h.x*sqrt(3.0)*0.5, h.y));
}

// Cone with correct distances to tip and base circle. Y is up, 0 is in the middle of the base.
float fCone(float3 p, float radius, float height) {
	float2 q = (float2)(length(p.xz), p.y);
	float2 tip = q - (float2)(0, height);
	float2 mantleDir = normalize((float2)(height, radius));
	float mantle = dot(tip, mantleDir);
	float d = max(mantle, -q.y);
	float projected = dot(tip, (float2)(mantleDir.y, -mantleDir.x));
	
	// distance to tip
	if ((q.y > height) && (projected < 0)) {
		d = max(d, length(tip));
	}
	
	// distance to base ring
	if ((q.x > radius) && (projected > length((float2)(height, radius)))) {
		d = max(d, length(q - (float2)(radius, 0)));
	}
	return d;
}

//
// "Generalized Distance Functions" by Akleman and Chen.
// see the Paper at https://www.viz.tamu.edu/faculty/ergun/research/implicitmodeling/papers/sm99.pdf
//
// This set of constants is used to construct a large variety of geometric primitives.
// Indices are shifted by 1 compared to the paper because we start counting at Zero.
// Some of those are slow whenever a driver decides to not unroll the loop,
// which seems to happen for fIcosahedron und fTruncatedIcosahedron on nvidia 350.12 at least.
// Specialized implementations can well be faster in all cases.
//

constant float3 GDFVectors[19] = {
	{1, 0, 0},
	{0, 1, 0},
	{0, 0, 1},

	{1, 1, 1 },
	{-1, 1, 1},
	{1, -1, 1},
	{1, 1, -1},

	{0, 1, PHI+1},
	{0, -1, PHI+1},
	{PHI+1, 0, 1},
	{-PHI-1, 0, 1},
	{1, PHI+1, 0},
	{-1, PHI+1, 0},

	{0, PHI, 1},
	{0, -PHI, 1},
	{1, 0, PHI},
	{-1, 0, PHI},
	{PHI, 1, 0},
	{-PHI, 1, 0}
};

// Version with variable exponent.
// This is slow and does not produce correct distances, but allows for bulging of objects.
float fGDF(float3 p, float r, float e, int begin, int end) {
	float d = 0;
	for (int i = begin; i <= end; ++i)
		d += pow(fabs(dot(p, normalize(GDFVectors[i]))), e);
	return pow(d, 1/e) - r;
}

// Version with without exponent, creates objects with sharp edges and flat faces
float fGDF_sharp(float3 p, float r, int begin, int end) {
	float d = 0;
	for (int i = begin; i <= end; ++i)
		d = max(d, fabs(dot(p, normalize(GDFVectors[i]))));
	return d - r;
}

// Primitives follow:

float fOctahedron(float3 p, float r, float e) {
	return fGDF(p, r, e, 3, 6);
}

float fDodecahedron(float3 p, float r, float e) {
	return fGDF(p, r, e, 13, 18);
}

float fIcosahedron(float3 p, float r, float e) {
	return fGDF(p, r, e, 3, 12);
}

float fTruncatedOctahedron(float3 p, float r, float e) {
	return fGDF(p, r, e, 0, 6);
}

float fTruncatedIcosahedron(float3 p, float r, float e) {
	return fGDF(p, r, e, 3, 18);
}

float fOctahedron_sharp(float3 p, float r) {
	return fGDF_sharp(p, r, 3, 6);
}

float fDodecahedron_sharp(float3 p, float r) {
	return fGDF_sharp(p, r, 13, 18);
}

float fIcosahedron_sharp(float3 p, float r) {
	return fGDF_sharp(p, r, 3, 12);
}

float fTruncatedOctahedron_sharp(float3 p, float r) {
	return fGDF_sharp(p, r, 0, 6);
}

float fTruncatedIcosahedron_sharp(float3 p, float r) {
	return fGDF_sharp(p, r, 3, 18);
}


////////////////////////////////////////////////////////////////
//
//                DOMAIN MANIPULATION OPERATORS
//
////////////////////////////////////////////////////////////////
//
// Conventions:
//
// Everything that modifies the domain is named pSomething.
//
// Many operate only on a subset of the three dimensions. For those,
// you must choose the dimensions that you want manipulated
// by supplying e.g. <p.x> or <p.zx>
//
// <inout p> is always the first argument and modified in place.
//
// Many of the operators partition space into cells. An identifier
// or cell index is returned, if possible. This return value is
// intended to be optionally used e.g. as a random seed to change
// parameters of the distance functions inside the cells.
//
// Unless stated otherwise, for cell index 0, <p> is unchanged and cells
// are centered on the origin so objects don't have to be moved to fit.
//
//
////////////////////////////////////////////////////////////////



// Rotate around a coordinate axis (i.e. in a plane perpendicular to that axis) by angle <a>.
// Read like this: R(p.xz, a) rotates "x towards z".
// This is fast if <a> is a compile-time constant and slower (but still practical) if not.
void pR(float2* p_ptr, float a) {
	float2 p = *p_ptr;
	*p_ptr = cos(a)*p + sin(a)*(float2)(p.y, -p.x);
}

// Shortcut for 45-degrees rotation
void pR45(float2* p_ptr) {
	float2 p = *p_ptr;
	*p_ptr = (p + (float2)(p.y, -p.x))*(float2)(sqrt(0.5f));
}

// Repeat space along one axis. Use like this to repeat along the x axis:
// <float cell = pMod1(p.x,5);> - using the return value is optional.
float pMod1(float* p_ptr, float size) {
	float p = *p_ptr;
	float halfsize = size*0.5f;
	float c = floor((p + halfsize)/size);
	*p_ptr = fmod(p + halfsize, size) - halfsize;
	return c;
}

// Same, but mirror every second cell so they match at the boundaries
float pModMirror1(float* p_ptr, float size) {
	float p = *p_ptr;
	float halfsize = size*0.5f;
	float c = floor((p + halfsize)/size);
	p = fmod(p + halfsize,size) - halfsize;
	*p_ptr = p * fmod(c, 2.0f)*2.f - 1.f;
	return c;
}

// Repeat the domain only in positive direction. Everything in the negative half-space is unchanged.
float pModSingle1(float* p_ptr, float size) {
	float p = *p_ptr;
	float halfsize = size*0.5f;
	float c = floor((p + halfsize)/size);
	if (p >= 0)
		p = fmod(p + halfsize, size) - halfsize;
	*p_ptr = p;
	return c;
}

// Repeat only a few times: from indices <start> to <stop> (similar to above, but more flexible)
float pModInterval1(float* p_ptr, float size, float start, float stop) {
	float p = *p_ptr;
	float halfsize = size*0.5f;
	float c = floor((p + halfsize)/size);
	p = fmod(p+halfsize, size) - halfsize;
	if (c > stop) { //yes, this might not be the best thing numerically.
		p += size*(c - stop);
		c = stop;
	}
	if (c <start) {
		p += size*(c - start);
		c = start;
	}
	*p_ptr = p;
	return c;
}

#if 0
// Repeat around the origin by a fixed angle.
// For easier use, num of repetitions is use to specify the angle.
float pModPolar(float2* p_ptr, float repetitions) {
	float2 p = *p_ptr;
	float angle = 2*PI/repetitions;
	float a = atan(p.yx) + (float2)(angle/2.f, angle/2.f);
	float r = length(p);
	float c = floor(a/angle);
	a = fmod(a,angle) - angle/2.;
	p = (float2)(cos(a), sin(a))*r;
	// For an odd number of repetitions, fix cell index of the cell in -x direction
	// (cell index would be e.g. -5 and 5 in the two halves of the cell):
	if (fabs(c) >= (repetitions/2)) c = fabs(c);
	*p_ptr = p;
	return c;
}
#endif

// Repeat in two dimensions
float2 pMod2(float2* p_ptr, float2 size) {
	float2 p = *p_ptr;
	float2 c = floor((p + size*0.5f)/size);
	p = fmod(p + size*0.5f,size) - size*0.5f;
	*p_ptr = p;
	return c;
}

// Same, but mirror every second cell so all boundaries match
float2 pModMirror2(float2* p_ptr, float2 size) {
	float2 p = *p_ptr;
	float2 halfsize = size*0.5f;
	float2 c = floor((p + halfsize)/size);
	p = fmod(p + halfsize, size) - halfsize;
	p *= fmod(c,(float2)(2.f))*2.f - (float2)(1.f);
	*p_ptr = p;
	return c;
}

// Same, but mirror every second cell at the diagonal as well
float2 pModGrid2(float2* p_ptr, float2 size) {
	float2 p = *p_ptr;
	float2 c = floor((p + size*0.5f)/size);
	p = fmod(p + size*0.5f, size) - size*0.5f;
	p *= fmod(c,(float2)(2))*2 - (float2)(1);
	p -= size/2;
	if (p.x > p.y) p.xy = p.yx;
	*p_ptr = p;
	return floor(c/2);
}

// Repeat in three dimensions
float3 pMod3(float3* p_ptr, float3 size) {
	float3 p = *p_ptr;
	float3 c = floor((p + size*0.5f)/size);
	p = fmod(p + size*0.5f, size) - size*0.5f;
	*p_ptr = p;
	return c;
}

// Mirror at an axis-aligned plane which is at a specified distance <dist> from the origin.
float pMirror (float* p_ptr, float dist) {
	float p = *p_ptr;
	float s = sign(p);
	p = fabs(p)-dist;
	*p_ptr = p;
	return s;
}

// Mirror in both dimensions and at the diagonal, yielding one eighth of the space.
// translate by dist before mirroring.
float2 pMirrorOctant (float2* p_ptr, float2 dist) {
	float2 p = *p_ptr;
	float2 s = sign(p);
	float x = p.x;
	float y = p.y;
	pMirror(&x, dist.x);
	pMirror(&y, dist.y);
	p.x = x;
	p.y = y;
	if (p.y > p.x)
		p.xy = p.yx;
	*p_ptr = p;
	return s;
}

// Reflect space at a plane
float pReflect(float3* p_ptr, float3 planeNormal, float offset) {
	float3 p = *p_ptr;
	float t = dot(p, planeNormal)+offset;
	if (t < 0) {
		p = p - (2*t)*planeNormal;
	}
	*p_ptr = p;
	return sign(t);
}


////////////////////////////////////////////////////////////////
//
//             OBJECT COMBINATION OPERATORS
//
////////////////////////////////////////////////////////////////
//
// We usually need the following boolean operators to combine two objects:
// Union: OR(a,b)
// Intersection: AND(a,b)
// Difference: AND(a,!b)
// (a and b being the distances to the objects).
//
// The trivial implementations are min(a,b) for union, max(a,b) for intersection
// and max(a,-b) for difference. To combine objects in more interesting ways to
// produce rounded edges, chamfers, stairs, etc. instead of plain sharp edges we
// can use combination operators. It is common to use some kind of "smooth minimum"
// instead of min(), but we don't like that because it does not preserve Lipschitz
// continuity in many cases.
//
// Naming convention: since they return a distance, they are called fOpSomething.
// The different flavours usually implement all the boolean operators above
// and are called fOpUnionRound, fOpIntersectionRound, etc.
//
// The basic idea: Assume the object surfaces intersect at a right angle. The two
// distances <a> and <b> constitute a new local two-dimensional coordinate system
// with the actual intersection as the origin. In this coordinate system, we can
// evaluate any 2D distance function we want in order to shape the edge.
//
// The operators below are just those that we found useful or interesting and should
// be seen as examples. There are infinitely more possible operators.
//
// They are designed to actually produce correct distances or distance bounds, unlike
// popular "smooth minimum" operators, on the condition that the gradients of the two
// SDFs are at right angles. When they are off by more than 30 degrees or so, the
// Lipschitz condition will no longer hold (i.e. you might get artifacts). The worst
// case is parallel surfaces that are close to each other.
//
// Most have a float argument <r> to specify the radius of the feature they represent.
// This should be much smaller than the object size.
//
// Some of them have checks like "if ((-a < r) && (-b < r))" that restrict
// their influence (and computation cost) to a certain area. You might
// want to lift that restriction or enforce it. We have left it as comments
// in some cases.
//
// usage example:
//
// float fTwoBoxes(float3 p) {
//   float box0 = fBox(p, float3(1));
//   float box1 = fBox(p-float3(1), float3(1));
//   return fOpUnionChamfer(box0, box1, 0.2);
// }
//
////////////////////////////////////////////////////////////////


// The "Chamfer" flavour makes a 45-degree chamfered edge (the diagonal of a square of size <r>):
float fOpUnionChamfer(float a, float b, float r) {
	float m = min(a, b);
	//if ((a < r) && (b < r)) {
		return min(m, (a - r + b)*sqrt(0.5f));
	//} else {
		return m;
	//}
}

// Intersection has to deal with what is normally the inside of the resulting object
// when using union, which we normally don't care about too much. Thus, intersection
// implementations sometimes differ from union implementations.
float fOpIntersectionChamfer(float a, float b, float r) {
	float m = max(a, b);
	if (r <= 0) return m;
	if (((-a < r) && (-b < r)) || (m < 0)) {
		return max(m, (a + r + b)*sqrt(0.5f));
	} else {
		return m;
	}
}

// Difference can be built from Intersection or Union:
float fOpDifferenceChamfer (float a, float b, float r) {
	return fOpIntersectionChamfer(a, -b, r);
}

// The "Round" variant uses a quarter-circle to join the two objects smoothly:
float fOpUnionRound(float a, float b, float r) {
	float m = min(a, b);
	if ((a < r) && (b < r) ) {
		return min(m, r - sqrt((r-a)*(r-a) + (r-b)*(r-b)));
	} else {
	 return m;
	}
}

float fOpIntersectionRound(float a, float b, float r) {
	float m = max(a, b);
	if ((-a < r) && (-b < r)) {
		return max(m, -(r - sqrt((r+a)*(r+a) + (r+b)*(r+b))));
	} else {
		return m;
	}
}

float fOpDifferenceRound (float a, float b, float r) {
	return fOpIntersectionRound(a, -b, r);
}


// The "Columns" flavour makes n-1 circular columns at a 45 degree angle:
float fOpUnionColumns(float a, float b, float r, float n) {
	if ((a < r) && (b < r)) {
		float2 p = (float2)(a, b);
		float columnradius = r*sqrt(2.0)/((n-1)*2+sqrt(2.0));
		pR45(&p);
		p.x -= sqrt(2.0)/2*r;
		p.x += columnradius*sqrt(2.0);
		if (fmod(n,2) == 1) {
			p.y += columnradius;
		}
		// At this point, we have turned 45 degrees and moved at a point on the
		// diagonal that we want to place the columns on.
		// Now, repeat the domain along this direction and place a circle.
		float y = p.y;
		pMod1(&y, columnradius*2);
		p.y = y;
		float result = length(p) - columnradius;
		result = min(result, p.x);
		result = min(result, a);
		return min(result, b);
	} else {
		return min(a, b);
	}
}

float fOpDifferenceColumns(float a, float b, float r, float n) {
	a = -a;
	float m = min(a, b);
	//avoid the expensive computation where not needed (produces discontinuity though)
	if ((a < r) && (b < r)) {
		float2 p = (float2)(a, b);
		float columnradius = r*sqrt(2.0)/n/2.0;
		columnradius = r*sqrt(2.0)/((n-1)*2+sqrt(2.0));

		pR45(&p);
		p.y += columnradius;
		p.x -= sqrt(2.0)/2*r;
		p.x += -columnradius*sqrt(2.0)/2;

		if (fmod(n,2) == 1) {
			p.y += columnradius;
		}
		float y = p.y;
		pMod1(&y,columnradius*2);
		p.y = y;

		float result = -length(p) + columnradius;
		result = max(result, p.x);
		result = min(result, a);
		return -min(result, b);
	} else {
		return -m;
	}
}

float fOpIntersectionColumns(float a, float b, float r, float n) {
	return fOpDifferenceColumns(a,-b,r, n);
}

// The "Stairs" flavour produces n-1 steps of a staircase:
float fOpUnionStairs(float a, float b, float r, float n) {
	float d = min(a, b);
	float2 p = (float2)(a, b);
	pR45(&p);
	p = p.yx - (float2)((r-r/n)*0.5*sqrt(2.0));
	p.x += 0.5*sqrt(2.0)*r/n;
	float x = r*sqrt(2.0)/n;
	float px = p.x;
	pMod1(&px, x);
	p.x = px;
	d = min(d, p.y);
	pR45(&p);
	return min(d, vmax2(p -(float2)(0.5*r/n)));
}

// We can just call Union since stairs are symmetric.
float fOpIntersectionStairs(float a, float b, float r, float n) {
	return -fOpUnionStairs(-a, -b, r, n);
}

float fOpDifferenceStairs(float a, float b, float r, float n) {
	return -fOpUnionStairs(-a, b, r, n);
}

// This produces a cylindical pipe that runs along the intersection.
// No objects remain, only the pipe. This is not a boolean operator.
float fOpPipe(float a, float b, float r) {
	return length((float2)(a, b)) - r;
}

