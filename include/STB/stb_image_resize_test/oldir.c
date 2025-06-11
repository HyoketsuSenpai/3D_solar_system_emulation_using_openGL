#include <stdio.h>
#include <stdlib.h>

#ifdef _MSC_VER
#define stop() __debugbreak()
#else
#define stop() __builtin_trap()
#endif

//#define HEAVYTM
#include "tm.h"

#define STBIR_SATURATE_INT
#define STB_IMAGE_RESIZE_STATIC
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "old_image_resize.h"  


static int types[4] =    { STBIR_TYPE_UINT8, STBIR_TYPE_UINT8, STBIR_TYPE_UINT16, STBIR_TYPE_FLOAT };
static int edges[4] =    { STBIR_EDGE_CLAMP, STBIR_EDGE_REFLECT, STBIR_EDGE_ZERO, STBIR_EDGE_WRAP };
static int flts[5] =     { STBIR_FILTER_BOX, STBIR_FILTER_TRIANGLE, STBIR_FILTER_CUBICBSPLINE, STBIR_FILTER_CATMULLROM, STBIR_FILTER_MITCHELL };
static int channels[20] = { 1, 2, 3, 4,      4,4,  2,2,  4,4, 2,2,  4,4, 2,2,  4,4, 2,2 }; 
static int alphapos[20] = { -1, -1, -1, -1,  3,0,  1,0,   3,0,  1,0,   3,0,  1,0,   3,0,  1,0 }; 


/**
 * @brief Resizes an image buffer using stb_image_resize with profiling and debugging support.
 *
 * Maps buffer format, data type, edge handling, and filter parameters to stb_image_resize options, and performs the resizing operation from the input buffer to the output buffer. Triggers a debug trap if resizing fails. Emits detailed profiling zones when enabled.
 *
 * @param o Pointer to the output image buffer.
 * @param ox Output image width in pixels.
 * @param oy Output image height in pixels.
 * @param op Output image pixel stride (bytes per row).
 * @param i Pointer to the input image buffer.
 * @param ix Input image width in pixels.
 * @param iy Input image height in pixels.
 * @param ip Input image pixel stride (bytes per row).
 * @param buf Buffer format index, selects channel count and alpha position.
 * @param type Data type index for pixel values.
 * @param edg Edge handling mode index.
 * @param flt Filter type index.
 */
void oresize( void * o, int ox, int oy, int op, void * i, int ix, int iy, int ip, int buf, int type, int edg, int flt )
{
  int t = types[type];
  int ic = channels[buf];
  int alpha = alphapos[buf];
  int e = edges[edg];
  int f = flts[flt];
  int space = ( type == 1 ) ? STBIR_COLORSPACE_SRGB : 0;
  int flags = ( buf >= 16 ) ? STBIR_FLAG_ALPHA_PREMULTIPLIED : ( ( buf >= 12 ) ? STBIR_FLAG_ALPHA_OUT_PREMULTIPLIED : ( ( buf >= 8 ) ? (STBIR_FLAG_ALPHA_PREMULTIPLIED|STBIR_FLAG_ALPHA_OUT_PREMULTIPLIED) : 0 ) );
  stbir_uint64 start;

  ENTER( "Resize (old)" );
  start = tmGetAccumulationStart( tm_mask );

  if(!stbir_resize( i, ix, iy, ip, o, ox, oy, op, t, ic, alpha, flags, e, e, f, f, space, 0 ) )
    stop();

  #ifdef STBIR_PROFILE
  tmEmitAccumulationZone( 0, 0, (tm_uint64 *)&start, 0, oldprofile.named.setup, "Setup (old)" );
  tmEmitAccumulationZone( 0, 0, (tm_uint64 *)&start, 0, oldprofile.named.filters, "Filters (old)" );
  tmEmitAccumulationZone( 0, 0, (tm_uint64 *)&start, 0, oldprofile.named.looping, "Looping (old)" );
  tmEmitAccumulationZone( 0, 0, (tm_uint64 *)&start, 0, oldprofile.named.vertical, "Vertical (old)" );
  tmEmitAccumulationZone( 0, 0, (tm_uint64 *)&start, 0, oldprofile.named.horizontal, "Horizontal (old)" );
  tmEmitAccumulationZone( 0, 0, (tm_uint64 *)&start, 0, oldprofile.named.decode, "Scanline input (old)" );
  tmEmitAccumulationZone( 0, 0, (tm_uint64 *)&start, 0, oldprofile.named.encode, "Scanline output (old)" );
  tmEmitAccumulationZone( 0, 0, (tm_uint64 *)&start, 0, oldprofile.named.alpha, "Alpha weighting (old)" );
  tmEmitAccumulationZone( 0, 0, (tm_uint64 *)&start, 0, oldprofile.named.unalpha, "Alpha unweighting (old)" );
  #endif

  LEAVE();
}
