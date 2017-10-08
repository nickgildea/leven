/*
 * 2D, 3D and 4D Perlin noise, classic and simplex, in a GLSL fragment shader.
 *
 * Classic noise is implemented by the functions:
 * float noise(float2 P)
 * float noise(float3 P)
 * float noise(float4 P)
 *
 * Simplex noise is implemented by the functions:
 * float snoise(float2 P)
 * float snoise(float3 P)
 * float snoise(float4 P)
 *
 * Author: Stefan Gustavson ITN-LiTH (stegu@itn.liu.se) 2004-12-05
 * Simplex indexing functions by Bill Licea-Kane, ATI
 */
 
/*
This code was irrevocably released into the public domain
by its original author, Stefan Gustavson, in January 2011.
Please feel free to use it for whatever you want.
Credit is appreciated where appropriate, and I also
appreciate being told where this code finds any use,
but you may do as you like. Alternatively, if you want
to have a familiar OSI-approved license, you may use
This code under the terms of the MIT license:

Copyright (C) 2004 by Stefan Gustavson. All rights reserved.
This code is licensed to you under the terms of the MIT license:

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

/*
 * To create offsets of one texel and one half texel in the
 * texture lookup, we need to know the texture image size.
 */
#define ONE 0.00390625f
#define ONEHALF 0.001953125f
// The numbers above are 1/256 and 0.5/256, change accordingly
// if you change the code to use another perm/grad texture size.

__constant sampler_t permSampler = CLK_FILTER_NEAREST | CLK_ADDRESS_REPEAT | CLK_NORMALIZED_COORDS_TRUE;

/*
 * The 5th degree smooth interpolation function for Perlin "improved noise".
 */
float fade(const float t) 
{
	// return t*t*(3.f-2.f*t); // Old fade, yields discontinuous second derivative
	return t*t*t*(t*(t*6.f-15.f)+10.f); // Improved fade, yields C2-continuous noise
}

/*
 * Efficient simplex indexing functions by Bill Licea-Kane, ATI. Thanks!
 * (This was originally implemented as a texture lookup. Nice to avoid that.)
 */
void simplex(const float3 P, float3* offset1, float3* offset2)
{
	float3 offset0;
 
	float2 isX = step( P.yz, P.xx );				 // P.x >= P.y ? 1.f : 0.f;	P.x >= P.z ? 1.f : 0.f;
	offset0.x	= dot( isX, (float2)( 1.f ) );	// Accumulate all P.x >= other channels in offset.x
	offset0.yz = 1.f - isX;								// Accumulate all P.x <	other channels in offset.yz

	float isY = step( P.z, P.y );					// P.y >= P.z ? 1.f : 0.f;
	offset0.y += isY;											// Accumulate P.y >= P.z in offset.y
	offset0.z += 1.f - isY;								// Accumulate P.y <	P.z in offset.z
 
	// offset0 now contains the unique values 0,1,2 in each channel
	// 2 for the channel greater than other channels
	// 1 for the channel that is less than one but greater than another
	// 0 for the channel less than other channels
	// Equality ties are broken in favor of first x, then y
	// (z always loses ties)

	*offset2 = clamp(	 offset0, 0.f, 1.f );
	// offset2 contains 1 in each channel that was 1 or 2
	*offset1 = clamp( offset0 - 1.f, 0.f, 1.f );
	// offset1 contains 1 in the single channel that was 1
}

float snoise2(const float2 P, read_only image2d_t permTexture) 
{
// Skew and unskew factors are a bit hairy for 2D, so define them as constants
// This is (sqrt(3.f)-1.f)/2.f
#define F2 0.366025403784f
// This is (3.f-sqrt(3.f))/6.f
#define G2 0.211324865405f

	// Skew the (x,y) space to determine which cell of 2 simplices we're in
	float s = (P.x + P.y) * F2;	 // Hairy factor for 2D skewing
	float2 Pi = floor(P + s);
	float t = (Pi.x + Pi.y) * G2; // Hairy factor for unskewing
	float2 P0 = Pi - t; // Unskew the cell origin back to (x,y) space
	Pi = Pi * ONE + ONEHALF; // Integer part, scaled and offset for texture lookup

	float2 Pf0 = P - P0;	// The x,y distances from the cell origin

	// For the 2D case, the simplex shape is an equilateral triangle.
	// Find out whether we are above or below the x=y diagonal to
	// determine which of the two triangles we're in.
	float2 o1;
	if(Pf0.x > Pf0.y) o1 = (float2)(1.f, 0.f);	// +x, +y traversal order
	else o1 = (float2)(0.f, 1.f);					// +y, +x traversal order

	// Noise contribution from simplex origin
	float2 grad0 = read_imagef(permTexture, permSampler, Pi).xy * 4.f - 1.f;
	float t0 = 0.5 - dot(Pf0, Pf0);
	float n0;
	if (t0 < 0.f) n0 = 0.f;
	else {
		t0 *= t0;
		n0 = t0 * t0 * dot(grad0, Pf0);
	}

	// Noise contribution from middle corner
	float2 Pf1 = Pf0 - o1 + G2;
	float2 grad1 = read_imagef(permTexture, permSampler, Pi + o1*ONE).xy * 4.f - 1.f;
	float t1 = 0.5 - dot(Pf1, Pf1);
	float n1;
	if (t1 < 0.f) n1 = 0.f;
	else {
		t1 *= t1;
		n1 = t1 * t1 * dot(grad1, Pf1);
	}
	
	// Noise contribution from last corner
	float2 Pf2 = Pf0 - (float2)(1.f-2.f*G2);
	float2 grad2 = read_imagef(permTexture, permSampler, Pi + (float2)(ONE, ONE)).xy * 4.f - 1.f;
	float t2 = 0.5 - dot(Pf2, Pf2);
	float n2;
	if(t2 < 0.f) n2 = 0.f;
	else {
		t2 *= t2;
		n2 = t2 * t2 * dot(grad2, Pf2);
	}

	// Sum up and scale the result to cover the range [-1,1]
	return 70.f * (n0 + n1 + n2);
}

float snoise3(const float3 P, read_only image2d_t permTexture)
{
// The skewing and unskewing factors are much simpler for the 3D case
#define F3 0.333333333333f
#define G3 0.166666666667f

	// Skew the (x,y,z) space to determine which cell of 6 simplices we're in
 	float s = (P.x + P.y + P.z) * F3; // Factor for 3D skewing
	float3 Pi = floor(P + s);
	float t = (Pi.x + Pi.y + Pi.z) * G3;
	float3 P0 = Pi - t; // Unskew the cell origin back to (x,y,z) space
	Pi = Pi * ONE + ONEHALF; // Integer part, scaled and offset for texture lookup

	float3 Pf0 = P - P0;	// The x,y distances from the cell origin

	// For the 3D case, the simplex shape is a slightly irregular tetrahedron.
	// To find out which of the six possible tetrahedra we're in, we need to
	// determine the magnitude ordering of x, y and z components of Pf0.
	float3 o1;
	float3 o2;
	simplex(Pf0, &o1, &o2);

	// Noise contribution from simplex origin
	float perm0 = read_imagef(permTexture, permSampler, Pi.xy).w;
	float3	grad0 = read_imagef(permTexture, permSampler, (float2)(perm0, Pi.z)).xyz * 4.f - 1.f;
	float t0 = 0.6 - dot(Pf0, Pf0);
	float n0;
	if (t0 < 0.f) n0 = 0.f;
	else {
		t0 *= t0;
		n0 = t0 * t0 * dot(grad0, Pf0);
	}

	// Noise contribution from second corner
	float3 Pf1 = Pf0 - o1 + G3;
	float perm1 = read_imagef(permTexture, permSampler, Pi.xy + o1.xy*ONE).w;
	float3	grad1 = read_imagef(permTexture, permSampler, (float2)(perm1, Pi.z + o1.z*ONE)).xyz * 4.f - 1.f;
	float t1 = 0.6 - dot(Pf1, Pf1);
	float n1;
	if (t1 < 0.f) n1 = 0.f;
	else {
		t1 *= t1;
		n1 = t1 * t1 * dot(grad1, Pf1);
	}
	
	// Noise contribution from third corner
	float3 Pf2 = Pf0 - o2 + 2.f * G3;
	float perm2 = read_imagef(permTexture, permSampler, Pi.xy + o2.xy*ONE).w;
	float3	grad2 = read_imagef(permTexture, permSampler, (float2)(perm2, Pi.z + o2.z*ONE)).xyz * 4.f - 1.f;
	float t2 = 0.6 - dot(Pf2, Pf2);
	float n2;
	if (t2 < 0.f) n2 = 0.f;
	else {
		t2 *= t2;
		n2 = t2 * t2 * dot(grad2, Pf2);
	}
	
	// Noise contribution from last corner
	float3 Pf3 = Pf0 - (float3)(1.f-3.f*G3);
	float perm3 = read_imagef(permTexture, permSampler, Pi.xy + (float2)(ONE, ONE)).w;
	float3	grad3 = read_imagef(permTexture, permSampler, (float2)(perm3, Pi.z + ONE)).xyz * 4.f - 1.f;
	float t3 = 0.6 - dot(Pf3, Pf3);
	float n3;
	if(t3 < 0.f) n3 = 0.f;
	else {
		t3 *= t3;
		n3 = t3 * t3 * dot(grad3, Pf3);
	}

	// Sum up and scale the result to cover the range [-1,1]
	return 32.f * (n0 + n1 + n2 + n3);
}



