#ifndef		HAS_SHARED_CONSTANTS_CL_BEEN_INCLUDED
#define		HAS_SHARED_CONSTANTS_CL_BEEN_INCLUDED

constant int EDGE_MAP[12][2] = 
{
	{0,4},{1,5},{2,6},{3,7},	// x-axis 
	{0,2},{1,3},{4,6},{5,7},	// y-axis
	{0,1},{2,3},{4,5},{6,7}		// z-axis
};

constant int4 CHILD_MIN_OFFSETS[8] =
{
	// needs to match the vertMap from Dual Contouring impl
	(int4)( 0, 0, 0, 0 ),
	(int4)( 0, 0, 1, 0 ),
	(int4)( 0, 1, 0, 0 ),
	(int4)( 0, 1, 1, 0 ),
	(int4)( 1, 0, 0, 0 ),
	(int4)( 1, 0, 1, 0 ),
	(int4)( 1, 1, 0, 0 ),
	(int4)( 1, 1, 1, 0 ),
};

inline int field_index(const int4 pos)
{
	return pos.x + (pos.y * FIELD_DIM) + (pos.z * FIELD_DIM * FIELD_DIM);
}



// "Insert" a 0 bit after each of the 16 low bits of x
uint Part1By1(uint x)
{
  x &= 0x0000ffff;                  // x = ---- ---- ---- ---- fedc ba98 7654 3210
  x = (x ^ (x <<  8)) & 0x00ff00ff; // x = ---- ---- fedc ba98 ---- ---- 7654 3210
  x = (x ^ (x <<  4)) & 0x0f0f0f0f; // x = ---- fedc ---- ba98 ---- 7654 ---- 3210
  x = (x ^ (x <<  2)) & 0x33333333; // x = --fe --dc --ba --98 --76 --54 --32 --10
  x = (x ^ (x <<  1)) & 0x55555555; // x = -f-e -d-c -b-a -9-8 -7-6 -5-4 -3-2 -1-0
  return x;
}

// "Insert" two 0 bits after each of the 10 low bits of x
uint Part1By2(uint x)
{
  x &= 0x000003ff;                  // x = ---- ---- ---- ---- ---- --98 7654 3210
  x = (x ^ (x << 16)) & 0xff0000ff; // x = ---- --98 ---- ---- ---- ---- 7654 3210
  x = (x ^ (x <<  8)) & 0x0300f00f; // x = ---- --98 ---- ---- 7654 ---- ---- 3210
  x = (x ^ (x <<  4)) & 0x030c30c3; // x = ---- --98 ---- 76-- --54 ---- 32-- --10
  x = (x ^ (x <<  2)) & 0x09249249; // x = ---- 9--8 --7- -6-- 5--4 --3- -2-- 1--0
  return x;
}

uint EncodeMorton3(uint x, uint y, uint z)
{
  return (Part1By2(z) << 2) + (Part1By2(y) << 1) + Part1By2(x);
}

#endif	//	HAS_SHARED_CONSTANTS_CL_BEEN_INCLUDED

