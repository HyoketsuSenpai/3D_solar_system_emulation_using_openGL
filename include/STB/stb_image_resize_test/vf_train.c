#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define stop() __debugbreak()
#include <windows.h>
#define int64 __int64

#pragma warning(disable:4127)

#define STBIR__WEIGHT_TABLES  
#define STBIR_PROFILE
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"  

/**
 * @brief Reads a binary file into memory with the file size prepended.
 *
 * Allocates a buffer large enough to hold the file contents plus an extra 4 bytes for the file size.
 * The first element of the returned buffer contains the file size in bytes as an integer, followed by the file data.
 *
 * @param filename Path to the binary file to read.
 * @return Pointer to the allocated buffer containing the file size and data, or 0 if the file cannot be opened.
 */
static int * file_read( char const * filename )
{
  size_t s;
  int * m;
  FILE * f = fopen( filename, "rb" );
  if ( f == 0 ) return 0;
  
  fseek( f, 0, SEEK_END);
  s = ftell( f );
  fseek( f, 0, SEEK_SET);
  m = malloc( s + 4 );
  m[0] = (int)s;
  fread( m+1, 1, s, f);
  fclose(f);

  return( m );
}

typedef struct fileinfo
{
  int * timings;
  int timing_count;
  int dimensionx, dimensiony;
  int numtypes;
  int * types;
  int * effective;
  int cpu;
  int simd;
  int numinputrects;
  int * inputrects;
  int outputscalex, outputscaley;
  int milliseconds;
  int64 cycles;
  double scale_time;
  int bitmapx, bitmapy;
  char const * filename;
} fileinfo;

int numfileinfo;
fileinfo fi[256];
unsigned char * bitmap;
int bitmapw, bitmaph, bitmapp;

/**
 * @brief Parses a timing file and populates the fileinfo structure at the specified index.
 *
 * Reads and validates the timing file, extracts metadata and timing data, and fills the corresponding fields in the global fileinfo array at the given index. Returns 1 on success, or 0 if the file cannot be read or is invalid.
 *
 * @param filename Path to the timing file.
 * @param index Index in the fileinfo array to populate.
 * @return int 1 if the file was successfully parsed and loaded, 0 otherwise.
 */
static int use_timing_file( char const * filename, int index )
{
  int * base = file_read( filename );
  int * file = base;

  if ( base == 0 ) return 0;

  ++file; // skip file image size;
  if ( *file++ != 'VFT1' ) return 0;
  fi[index].cpu = *file++;
  fi[index].simd = *file++;
  fi[index].dimensionx = *file++;
  fi[index].dimensiony = *file++;
  fi[index].numtypes = *file++;
  fi[index].types = file; file += fi[index].numtypes;
  fi[index].effective = file; file += fi[index].numtypes;
  fi[index].numinputrects = *file++;
  fi[index].inputrects = file; file += fi[index].numinputrects * 2;
  fi[index].outputscalex = *file++;
  fi[index].outputscaley = *file++;
  fi[index].milliseconds = *file++;
  fi[index].cycles = ((int64*)file)[0]; file += 2;
  fi[index].filename = filename;

  fi[index].timings = file;
  fi[index].timing_count = (int) ( ( base[0] - ( ((char*)file - (char*)base - sizeof(int) ) ) ) / (sizeof(int)*2) );

  fi[index].scale_time = (double)fi[index].milliseconds / (double)fi[index].cycles;

  return 1;
}

/**
 * @brief Determines whether vertical-first filtering should be used for image resizing.
 *
 * Evaluates scaling factors and filter characteristics to decide if vertical-first filtering is preferable for the given input and output dimensions, filter type, and weights. Returns a boolean indicating the decision.
 *
 * @param weights_table Table of weights used for classification.
 * @param ox Output width.
 * @param oy Output height.
 * @param ix Input width.
 * @param iy Input height.
 * @param filter Filter type index.
 * @param v_info Optional pointer to receive additional vertical-first decision info.
 * @return int Nonzero if vertical-first filtering should be used; zero otherwise.
 */
static int vert_first( float weights_table[STBIR_RESIZE_CLASSIFICATIONS][4], int ox, int oy, int ix, int iy, int filter, STBIR__V_FIRST_INFO * v_info )
{
  float h_scale=(float)ox/(float)(ix);
  float v_scale=(float)oy/(float)(iy);
  stbir__support_callback * support = stbir__builtin_supports[filter];
  int vertical_filter_width = stbir__get_filter_pixel_width(support,v_scale,0);
  int vertical_gather = ( v_scale >= ( 1.0f - stbir__small_float ) ) || ( vertical_filter_width <= STBIR_FORCE_GATHER_FILTER_SCANLINES_AMOUNT );

  return stbir__should_do_vertical_first( weights_table, stbir__get_filter_pixel_width(support,h_scale,0), h_scale, ox, vertical_filter_width, v_scale, oy, vertical_gather, v_info );
} 

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

/**
 * @brief Allocates and initializes the bitmap buffer for visualizing timing data.
 *
 * Calculates the required bitmap dimensions to arrange all timing files in a tiled layout with spacing, assigns each file's tile position, and allocates a zero-initialized RGB bitmap buffer sized to fit all tiles.
 */
static void alloc_bitmap()
{
  int findex;
  int x = 0, y = 0;
  int w = 0, h = 0;

  for( findex = 0 ; findex < numfileinfo ; findex++ )
  {
    int nx, ny;
    int thisw, thish;

    thisw = ( fi[findex].dimensionx * fi[findex].numtypes ) + ( fi[findex].numtypes - 1 );
    thish = ( fi[findex].dimensiony * fi[findex].numinputrects ) + ( fi[findex].numinputrects - 1 );

    for(;;)
    {
      nx = x + ((x)?4:0) + thisw;
      ny = y + ((y)?4:0) + thish;
      if ( ( nx <= 3600 ) || ( x == 0 ) )
      { 
        fi[findex].bitmapx = x + ((x)?4:0);
        fi[findex].bitmapy = y + ((y)?4:0);
        x = nx;
        if ( x > w ) w = x;
        if ( ny > h ) h = ny;
        break;
      }
      else
      {
        x = 0;
        y = h;
      }
    }
  }

  w = (w+3) & ~3;
  bitmapw = w;
  bitmaph = h;
  bitmapp = w * 3; // RGB
  bitmap = malloc( bitmapp * bitmaph );

  memset( bitmap, 0, bitmapp * bitmaph );
}

/**
 * @brief Builds a bitmap visualization for a timing file and channel count.
 *
 * Colors each pixel to indicate whether the vertical-first filtering decision, as determined by the provided weights, matches the optimal timing for that configuration. Correct decisions are colored by classification; incorrect decisions are colored by error magnitude and classification.
 *
 * @param weights Table of weights used to determine vertical-first filtering.
 * @param do_channel_count_index Index of the channel count to visualize.
 * @param findex Index of the timing file to visualize.
 */
static void build_bitmap( float weights[STBIR_RESIZE_CLASSIFICATIONS][4], int do_channel_count_index, int findex )
{
  static int colors[STBIR_RESIZE_CLASSIFICATIONS];
  STBIR__V_FIRST_INFO v_info = {0};

  int * ts;
  int ir;
  unsigned char * bitm = bitmap + ( fi[findex].bitmapx*3 ) + ( fi[findex].bitmapy*bitmapp) ;

  for( ir = 0; ir < STBIR_RESIZE_CLASSIFICATIONS ; ir++ ) colors[ ir ] = 127*ir/STBIR_RESIZE_CLASSIFICATIONS+128;

  ts = fi[findex].timings;

  for( ir = 0 ; ir < fi[findex].numinputrects ; ir++ )
  {
    int ix, iy, chanind;
    ix = fi[findex].inputrects[ir*2];
    iy = fi[findex].inputrects[ir*2+1];

    for( chanind = 0 ; chanind < fi[findex].numtypes ; chanind++ )
    {
      int ofs, h, hh;

      // just do the type that we're on
      if ( chanind != do_channel_count_index )
      {
        ts += 2 * fi[findex].dimensionx * fi[findex].dimensiony;
        continue;
      }

      // bitmap offset
      ofs=chanind*(fi[findex].dimensionx+1)*3+ir*(fi[findex].dimensiony+1)*bitmapp;

      h = 1;
      for( hh = 0 ; hh < fi[findex].dimensiony; hh++ )
      {
        int ww, w = 1;
        for( ww = 0 ; ww < fi[findex].dimensionx; ww++ )
        {
          int good, v_first, VF, HF;

          VF = ts[0];
          HF = ts[1];

          v_first = vert_first( weights, w, h, ix, iy, STBIR_FILTER_MITCHELL, &v_info );

          good = ( ((HF<=VF) && (!v_first)) || ((VF<=HF) && (v_first)));

          if ( good )
          {
            bitm[ofs+2] = 0;
            bitm[ofs+1] = (unsigned char)colors[v_info.v_resize_classification];
          }
          else
          {
            double r;

            if ( HF < VF )
              r = (double)(VF-HF)/(double)HF;
            else
              r = (double)(HF-VF)/(double)VF;
            
            if ( r > 0.4f) r = 0.4;
            r *= 1.0f/0.4f;   

            bitm[ofs+2] = (char)(255.0f*r);
            bitm[ofs+1] = (char)(((float)colors[v_info.v_resize_classification])*(1.0f-r));
          }
          bitm[ofs] = 0;

          ofs += 3;
          ts += 2;
          w += fi[findex].outputscalex;
        }
        ofs += bitmapp - fi[findex].dimensionx*3;
        h += fi[findex].outputscaley;
      }
    }
  }
}

/**
 * @brief Builds a comparative bitmap visualizing timing differences between two timing files for a specific channel count.
 *
 * Colors each pixel to indicate which timing file performs better for the vertical-first filtering decision, with color intensity representing the magnitude of the timing difference.
 *
 * @param weights Table of weights used for vertical-first decision classification.
 * @param do_channel_count_index Index of the channel count to compare.
 */
static void build_comp_bitmap( float weights[STBIR_RESIZE_CLASSIFICATIONS][4], int do_channel_count_index )
{
  int * ts0;
  int * ts1;
  int ir;
  unsigned char * bitm = bitmap + ( fi[0].bitmapx*3 ) + ( fi[0].bitmapy*bitmapp) ;

  ts0 = fi[0].timings;
  ts1 = fi[1].timings;

  for( ir = 0 ; ir < fi[0].numinputrects ; ir++ )
  {
    int ix, iy, chanind;
    ix = fi[0].inputrects[ir*2];
    iy = fi[0].inputrects[ir*2+1];

    for( chanind = 0 ; chanind < fi[0].numtypes ; chanind++ )
    {
      int ofs, h, hh;

      // just do the type that we're on
      if ( chanind != do_channel_count_index )
      {
        ts0 += 2 * fi[0].dimensionx * fi[0].dimensiony;
        ts1 += 2 * fi[0].dimensionx * fi[0].dimensiony;
        continue;
      }

      // bitmap offset
      ofs=chanind*(fi[0].dimensionx+1)*3+ir*(fi[0].dimensiony+1)*bitmapp;

      h = 1;
      for( hh = 0 ; hh < fi[0].dimensiony; hh++ )
      {
        int ww, w = 1;
        for( ww = 0 ; ww < fi[0].dimensionx; ww++ )
        {
          int v_first, time0, time1;

          v_first = vert_first( weights, w, h, ix, iy, STBIR_FILTER_MITCHELL, 0 );

          time0 = ( v_first ) ? ts0[0] : ts0[1];
          time1 = ( v_first ) ? ts1[0] : ts1[1];

          if ( time0 < time1 )
          {
            double r = (double)(time1-time0)/(double)time0;
            if ( r > 0.4f) r = 0.4;
            r *= 1.0f/0.4f;   
            bitm[ofs+2] = 0;
            bitm[ofs+1] = (char)(255.0f*r);
            bitm[ofs] = (char)(64.0f*(1.0f-r));
          }
          else
          {
            double r = (double)(time0-time1)/(double)time1;
            if ( r > 0.4f) r = 0.4;
            r *= 1.0f/0.4f;   
            bitm[ofs+2] = (char)(255.0f*r);
            bitm[ofs+1] = 0;
            bitm[ofs] = (char)(64.0f*(1.0f-r));
          }
          ofs += 3;
          ts0 += 2;
          ts1 += 2;
          w += fi[0].outputscalex;
        }
        ofs += bitmapp - fi[0].dimensionx*3;
        h += fi[0].outputscaley;
      }
    }
  }
}

/**
 * @brief Writes the current bitmap visualization to a PNG file named "results.png".
 *
 * The bitmap is saved in BGR format with the current width and height.
 */
static void write_bitmap()
{
  stbi_write_png( "results.png", bitmapp / 3, bitmaph, 3|STB_IMAGE_BGR, bitmap, bitmapp );
}


/**
 * @brief Calculates error metrics for vertical-first filtering decisions across timing files.
 *
 * For a given set of weights and a specific channel count, this function iterates through all loaded timing files and their input rectangles, comparing the actual timing data for vertical-first and horizontal-first filtering. It accumulates the total count and sum of timing errors for each classification where the vertical-first decision does not match the optimal timing.
 *
 * @param weights_table Table of weights used to determine vertical-first filtering decisions.
 * @param curtot Output array to receive the count of errors per classification.
 * @param curerr Output array to receive the sum of timing errors per classification.
 * @param do_channel_count_index Index specifying which channel count to evaluate.
 */
static void calc_errors( float weights_table[STBIR_RESIZE_CLASSIFICATIONS][4], int * curtot, double * curerr, int do_channel_count_index )
{
  int th, findex;
  STBIR__V_FIRST_INFO v_info = {0};

  for(th=0;th<STBIR_RESIZE_CLASSIFICATIONS;th++)
  {
    curerr[th]=0;
    curtot[th]=0;
  }

  for( findex = 0 ; findex < numfileinfo ; findex++ )
  {
    int * ts;
    int ir;
    ts = fi[findex].timings;

    for( ir = 0 ; ir < fi[findex].numinputrects ; ir++ )
    {
      int ix, iy, chanind;
      ix = fi[findex].inputrects[ir*2];
      iy = fi[findex].inputrects[ir*2+1];

      for( chanind = 0 ; chanind < fi[findex].numtypes ; chanind++ )
      {
        int h, hh;

        // just do the type that we're on
        if ( chanind != do_channel_count_index )
        {
          ts += 2 * fi[findex].dimensionx * fi[findex].dimensiony;
          continue;
        }

        h = 1;
        for( hh = 0 ; hh < fi[findex].dimensiony; hh++ )
        {
          int ww, w = 1;
          for( ww = 0 ; ww < fi[findex].dimensionx; ww++ )
          {
            int good, v_first, VF, HF;

            VF = ts[0];
            HF = ts[1];

            v_first = vert_first( weights_table, w, h, ix, iy, STBIR_FILTER_MITCHELL, &v_info );

            good = ( ((HF<=VF) && (!v_first)) || ((VF<=HF) && (v_first)));

            if ( !good )
            {
              double diff;
              if ( VF < HF )
                diff = ((double)HF-(double)VF) * fi[findex].scale_time;
              else
                diff = ((double)VF-(double)HF) * fi[findex].scale_time;

              curtot[v_info.v_resize_classification] += 1;
              curerr[v_info.v_resize_classification] += diff;
            }

            ts += 2;
            w += fi[findex].outputscalex;
          }
          h += fi[findex].outputscaley;
        }
      }
    }
  }
}

#define TRIESPERWEIGHT 32
#define MAXRANGE ((TRIESPERWEIGHT+1) * (TRIESPERWEIGHT+1) * (TRIESPERWEIGHT+1) * (TRIESPERWEIGHT+1) - 1)

/**
 * @brief Converts an integer index into four normalized float weights.
 *
 * Decomposes the given integer range into four components, each mapped to a float in the range [0, 1], and stores them in the provided weights array.
 *
 * @param weights Output array to receive four float weights.
 * @param range Integer index representing a unique combination of four weights.
 */
static void expand_to_floats( float * weights, int range )
{
  weights[0] = (float)( range % (TRIESPERWEIGHT+1) ) / (float)TRIESPERWEIGHT;
  weights[1] = (float)( range/(TRIESPERWEIGHT+1) % (TRIESPERWEIGHT+1) ) / (float)TRIESPERWEIGHT;
  weights[2] = (float)( range/(TRIESPERWEIGHT+1)/(TRIESPERWEIGHT+1) % (TRIESPERWEIGHT+1) ) / (float)TRIESPERWEIGHT;
  weights[3] = (float)( range/(TRIESPERWEIGHT+1)/(TRIESPERWEIGHT+1)/(TRIESPERWEIGHT+1) % (TRIESPERWEIGHT+1) ) / (float)TRIESPERWEIGHT;
}

/**
 * @brief Converts a discrete weight index into a formatted string representation.
 *
 * Decomposes the integer `range` into four weight components and formats them as a string
 * showing their values relative to the maximum allowed (`TRIESPERWEIGHT`).
 *
 * @param range Integer encoding four weight values.
 * @return Pointer to a static string representing the weights in the format "[ w0/max w1/max w2/max w3/max ]".
 */
static char const * expand_to_string( int range )
{
  static char str[128];
  int w0,w1,w2,w3;
  w0 = range % (TRIESPERWEIGHT+1);
  w1 = range/(TRIESPERWEIGHT+1) % (TRIESPERWEIGHT+1);
  w2 = range/(TRIESPERWEIGHT+1)/(TRIESPERWEIGHT+1) % (TRIESPERWEIGHT+1);
  w3 = range/(TRIESPERWEIGHT+1)/(TRIESPERWEIGHT+1)/(TRIESPERWEIGHT+1) % (TRIESPERWEIGHT+1);
  sprintf( str, "[ %2d/%d %2d/%d %2d/%d %2d/%d ]",w0,TRIESPERWEIGHT,w1,TRIESPERWEIGHT,w2,TRIESPERWEIGHT,w3,TRIESPERWEIGHT );
  return str;
}

/**
 * @brief Prints the weights and error statistics for each classification of a given channel count.
 *
 * Outputs the channel index, weight values, error counts, and error sums for each classification to the console.
 *
 * @param weights Array of weight values for each classification.
 * @param channel_count_index Index of the channel count being reported.
 * @param tots Array of error counts per classification.
 * @param errs Array of error sums per classification.
 */
static void print_weights( float weights[STBIR_RESIZE_CLASSIFICATIONS][4], int channel_count_index, int * tots, double * errs )
{
  int th;
  printf("ChInd: %d  Weights:\n",channel_count_index);
  for(th=0;th<STBIR_RESIZE_CLASSIFICATIONS;th++)
  {
    float * w = weights[th];
    printf("  %d: [%1.5f %1.5f %1.5f %1.5f] (%d %.4f)\n",th, w[0], w[1], w[2], w[3], tots[th], errs[th] );
  }
  printf("\n");
}

static int windowranges[ 16 ];
static int windowstatus = 0;
static DWORD trainstart = 0;

/**
 * @brief Optimizes weight parameters for vertical-first resizing for a specific channel count.
 *
 * Performs an exhaustive search over all possible discrete weight combinations for the given channel count, evaluating each combination against timing data to minimize classification errors. Updates the provided output array with the best weights found. Periodically updates the visualization bitmap during the search. The process can be cancelled via a global status flag.
 *
 * @param best_output_weights Output array to receive the best weights for each classification.
 * @param channel_count_index Index specifying which channel count to optimize.
 */
static void opt_channel( float best_output_weights[STBIR_RESIZE_CLASSIFICATIONS][4], int channel_count_index )
{
  int newbest = 0;
  float weights[STBIR_RESIZE_CLASSIFICATIONS][4] = {0};
  double besterr[STBIR_RESIZE_CLASSIFICATIONS];
  int besttot[STBIR_RESIZE_CLASSIFICATIONS];
  int best[STBIR_RESIZE_CLASSIFICATIONS]={0};

  double curerr[STBIR_RESIZE_CLASSIFICATIONS];
  int curtot[STBIR_RESIZE_CLASSIFICATIONS];
  int th, range;
  DWORD lasttick = 0;

  for(th=0;th<STBIR_RESIZE_CLASSIFICATIONS;th++) 
  {
    besterr[th]=1000000000000.0;
    besttot[th]=0x7fffffff;
  }

  newbest = 0;

  // try the whole range  
  range = MAXRANGE;
  do
  {
    for(th=0;th<STBIR_RESIZE_CLASSIFICATIONS;th++)
      expand_to_floats( weights[th], range );

    calc_errors( weights, curtot, curerr, channel_count_index );

    for(th=0;th<STBIR_RESIZE_CLASSIFICATIONS;th++)
    {
      if ( curerr[th] < besterr[th] )
      {
        besterr[th] = curerr[th];
        besttot[th] = curtot[th];
        best[th] = range;
        expand_to_floats( best_output_weights[th], best[th] );
        newbest = 1;
      }
    }

    {
      DWORD t = GetTickCount();
      if ( range == 0 )
        goto do_bitmap;

      if ( newbest )
      {
        if ( ( GetTickCount() - lasttick ) > 200 )
        {
          int findex;
        
         do_bitmap:
          lasttick = t;
          newbest = 0;

          for( findex = 0 ; findex < numfileinfo ; findex++ )
            build_bitmap( best_output_weights, channel_count_index, findex );

          lasttick = GetTickCount();
        }
      }
    }
   
    windowranges[ channel_count_index ] = range;

    // advance all the weights and loop
    --range;
  } while( ( range >= 0 ) && ( !windowstatus ) );

  // if we hit here, then we tried all weights for this opt, so save them 
}

/**
 * @brief Prints a 3D float array as a static C array declaration.
 *
 * Outputs the provided weights array in C source code format, using the specified variable name.
 *
 * @param weight The 3D array of weights to print, indexed by set, classification, and weight component.
 * @param name The variable name to use in the generated C declaration.
 */
static void print_struct( float weight[5][STBIR_RESIZE_CLASSIFICATIONS][4], char const * name )
{
  printf("\n\nstatic float %s[5][STBIR_RESIZE_CLASSIFICATIONS][4]=\n{", name );
  {
    int i;
    for(i=0;i<5;i++) 
    { 
      int th;
      for(th=0;th<STBIR_RESIZE_CLASSIFICATIONS;th++)
      {
        int j;
        printf("\n  "); 
        for(j=0;j<4;j++) 
          printf("%1.5ff, ", weight[i][th][j] ); 
      }
      printf("\n");
    }
    printf("\n};\n");
  }
}

static float retrain_weights[5][STBIR_RESIZE_CLASSIFICATIONS][4];

/**
 * @brief Thread entry point for optimizing weights for a specific channel count.
 *
 * Casts the input parameter to a channel index and calls the optimization routine for that channel.
 *
 * @param p Pointer cast to the channel index to optimize.
 * @return Always returns 0.
 */
static DWORD __stdcall retrain_shim( LPVOID p )
{
  int chanind = (int) (size_t)p;
  opt_channel( retrain_weights[chanind], chanind );
  return 0;
}

/**
 * @brief Formats a duration in milliseconds as a human-readable string.
 *
 * Converts milliseconds to a string in "Xm Ys" format if at least one minute, or "Xs" otherwise.
 *
 * @param ms Duration in milliseconds.
 * @return Pointer to a static string representing the formatted time.
 */
static char const * gettime( int ms )
{
  static char time[32];
  if (ms > 60000)
    sprintf( time, "%dm %ds",ms/60000, (ms/1000)%60 );
  else  
    sprintf( time, "%ds",ms/1000 );
  return time;
}

static BITMAPINFOHEADER bmiHeader;
static DWORD extrawindoww, extrawindowh;
static HINSTANCE instance;
static int curzoom = 1;

/**
 * @brief Handles Windows messages for the training and visualization window.
 *
 * Processes keyboard and window events to manage training cancellation, window closure, bitmap rendering, and interactive display of timing and classification details. On paint events, draws the timing visualization bitmap, overlays training progress and status, and shows detailed information about the pixel under the mouse cursor. Supports user cancellation of training and updates the display in response to timer events.
 *
 * @param window Handle to the window receiving the message.
 * @param message The Windows message identifier.
 * @param wparam Additional message-specific information.
 * @param lparam Additional message-specific information.
 * @return LRESULT Result of message processing, or passes unhandled messages to the default window procedure.
 */
static LRESULT WINAPI WindowProc( HWND   window,
                                  UINT   message,
                                  WPARAM wparam,
                                  LPARAM lparam )
{
  switch( message )
  {
    case WM_CHAR:
      if ( wparam != 27 )
        break;
      // falls through

    case WM_CLOSE:
    {
      int i;
      int max = 0;
      
      for( i = 0 ; i < fi[0].numtypes ; i++ )
        if( windowranges[i] > max ) max = windowranges[i];
   
      if ( ( max == 0 ) || ( MessageBox( window, "Cancel before training is finished?", "Vertical First Training", MB_OKCANCEL|MB_ICONSTOP ) == IDOK ) )
      {
        for( i = 0 ; i < fi[0].numtypes ; i++ )
          if( windowranges[i] > max ) max = windowranges[i];
        if ( max )
          windowstatus = 1;
        DestroyWindow( window );
      }
    }
    return 0;
       
    case WM_PAINT:
    {
      PAINTSTRUCT ps;
      HDC dc;

      dc = BeginPaint( window, &ps );
      StretchDIBits( dc, 
        0, 0, bitmapw*curzoom, bitmaph*curzoom,
        0, 0, bitmapw, bitmaph,
        bitmap, (BITMAPINFO*)&bmiHeader, DIB_RGB_COLORS, SRCCOPY );

      PatBlt( dc, bitmapw*curzoom, 0, 4096, 4096, WHITENESS );
      PatBlt( dc, 0, bitmaph*curzoom, 4096, 4096, WHITENESS );

      SetTextColor( dc, RGB(0,0,0)  );
      SetBkColor( dc, RGB(255,255,255) );
      SetBkMode( dc, OPAQUE );

      {
        int i, l = 0, max = 0;
        char buf[1024];
        RECT rc;
        POINT p;

        for( i = 0 ; i < fi[0].numtypes ; i++ )
        {
          l += sprintf( buf + l, "channels: %d %s\n", fi[0].effective[i], windowranges[i] ? expand_to_string( windowranges[i] ) : "Done." );
          if ( windowranges[i] > max ) max = windowranges[i];
        }

        rc.left = 32; rc.top = bitmaph*curzoom+10;
        rc.right = 512; rc.bottom = rc.top + 512;
        DrawText( dc, buf, -1, &rc, DT_TOP );

        l = 0;
        if ( max == 0 )
        {
          static DWORD traindone = 0;
          if ( traindone == 0 ) traindone = GetTickCount();
          l = sprintf( buf, "Finished in %s.", gettime( traindone - trainstart ) );
        }
        else if ( max != MAXRANGE )
          l = sprintf( buf, "Done in %s...", gettime( (int) ( ( ( (int64)max * ( (int64)GetTickCount() - (int64)trainstart ) ) ) / (int64) ( MAXRANGE - max ) ) ) );

        GetCursorPos( &p );
        ScreenToClient( window, &p );

        if ( ( p.x >= 0 ) && ( p.y >= 0 ) && ( p.x < (bitmapw*curzoom) ) && ( p.y < (bitmaph*curzoom) ) )
        {
          int findex;
          int x, y, w, h, sx, sy, ix, iy, ox, oy;
          int ir, chanind;
          int * ts;
          char badstr[64];
          STBIR__V_FIRST_INFO v_info={0};

          p.x /= curzoom;
          p.y /= curzoom;

          for( findex = 0 ; findex < numfileinfo ; findex++ )
          {
            x = fi[findex].bitmapx;
            y = fi[findex].bitmapy;
            w = x + ( fi[findex].dimensionx + 1 ) * fi[findex].numtypes;
            h = y + ( fi[findex].dimensiony + 1 ) * fi[findex].numinputrects;

            if ( ( p.x >= x ) && ( p.y >= y ) && ( p.x < w ) && ( p.y < h ) )
              goto found;
          }
          goto nope;
         
         found:
            
          ir = ( p.y - y ) / ( fi[findex].dimensiony + 1 );
          sy = ( p.y - y ) % ( fi[findex].dimensiony + 1 );
          if ( sy >= fi[findex].dimensiony ) goto nope;

          chanind = ( p.x - x ) / ( fi[findex].dimensionx + 1 );
          sx = ( p.x - x ) % ( fi[findex].dimensionx + 1 );
          if ( sx >= fi[findex].dimensionx ) goto nope;

          ix = fi[findex].inputrects[ir*2];
          iy = fi[findex].inputrects[ir*2+1];

          ts = fi[findex].timings + ( ( fi[findex].dimensionx * fi[findex].dimensiony * fi[findex].numtypes * ir ) + ( fi[findex].dimensionx * fi[findex].dimensiony * chanind ) + ( fi[findex].dimensionx * sy ) + sx ) * 2;

          ox = 1+fi[findex].outputscalex*sx;
          oy = 1+fi[findex].outputscaley*sy;

          if ( windowstatus != 2 )
          {
            int VF, HF, v_first, good;
            VF = ts[0];
            HF = ts[1];

            v_first = vert_first( retrain_weights[chanind], ox, oy, ix, iy, STBIR_FILTER_MITCHELL, &v_info );

            good = ( ((HF<=VF) && (!v_first)) || ((VF<=HF) && (v_first)));

            if ( good )
              badstr[0] = 0;
            else
            {
              double r;

              if ( HF < VF )
                r = (double)(VF-HF)/(double)HF;
              else
                r = (double)(HF-VF)/(double)VF;
              sprintf( badstr, " %.1f%% off", r*100 );
            }
            sprintf( buf + l, "\n\n%s\nCh: %d Resize: %dx%d to %dx%d\nV: %d H: %d  Order: %c (%s%s)\nClass: %d Scale: %.2f %s", fi[findex].filename,fi[findex].effective[chanind], ix,iy,ox,oy, VF, HF, v_first?'V':'H', good?"Good":"Wrong", badstr, v_info.v_resize_classification, (double)oy/(double)iy, v_info.is_gather ? "Gather" : "Scatter" );
          }
          else
          {
            int v_first, time0, time1;
            float (* weights)[4] = stbir__compute_weights[chanind];
            int * ts1;
            char b0[32], b1[32];

            ts1 = fi[1].timings + ( ts - fi[0].timings );

            v_first = vert_first( weights, ox, oy, ix, iy, STBIR_FILTER_MITCHELL, &v_info );

            time0 = ( v_first ) ? ts[0] : ts[1];
            time1 = ( v_first ) ? ts1[0] : ts1[1];
            
            b0[0] = b1[0] = 0;
            if ( time0 < time1 )
              sprintf( b0," (%.f%% better)", ((double)time1-(double)time0)*100.0f/(double)time0);
            else
              sprintf( b1," (%.f%% better)", ((double)time0-(double)time1)*100.0f/(double)time1);

            sprintf( buf + l, "\n\n0: %s\n1: %s\nCh: %d Resize: %dx%d to %dx%d\nClass: %d Scale: %.2f %s\nTime0: %d%s\nTime1: %d%s", fi[0].filename, fi[1].filename, fi[0].effective[chanind], ix,iy,ox,oy, v_info.v_resize_classification, (double)oy/(double)iy, v_info.is_gather ? "Gather" : "Scatter", time0, b0, time1, b1 );
          }
        }
       nope:

        rc.left = 32+320; rc.right = 512+320; 
        SetTextColor( dc, RGB(0,0,128) );
        DrawText( dc, buf, -1, &rc, DT_TOP );

      }
      EndPaint( window, &ps );
      return 0;
    }

    case WM_TIMER:
      InvalidateRect( window, 0, 0 );
      return 0;

    case WM_DESTROY:
      PostQuitMessage( 0 );
      return 0;
  }
   

  return DefWindowProc( window, message, wparam, lparam );
}

/**
 * @brief Sets the process DPI awareness to improve scaling on high-DPI displays.
 *
 * Attempts to set the process DPI awareness to "system DPI aware" using the SetProcessDpiAwareness function from Shcore.dll, if available.
 */
static void SetHighDPI(void)
{
  typedef HRESULT WINAPI setdpitype(int v);
  HMODULE h=LoadLibrary("Shcore.dll");
  if (h)
  {
    setdpitype * sd = (setdpitype*)GetProcAddress(h,"SetProcessDpiAwareness");
    if (sd )
      sd(1);
  }
} 

/**
 * @brief Creates and displays the main application window for bitmap visualization.
 *
 * Registers a window class, creates a resizable window sized to fit the visualization bitmap, sets up periodic redraw via a timer, and enters the Windows message loop to handle user interaction and rendering.
 */
static void draw_window()
{
  WNDCLASS wc;
  HWND w;
  MSG msg;
  
  instance = GetModuleHandle(NULL);

  wc.style = 0;
  wc.lpfnWndProc = WindowProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = instance;
  wc.hIcon = 0;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = 0;
  wc.lpszMenuName = 0;
  wc.lpszClassName = "WHTrain";

  if ( !RegisterClass( &wc ) )
    exit(1);

  SetHighDPI();

  bmiHeader.biSize          =  sizeof(BITMAPINFOHEADER);
  bmiHeader.biWidth         =  bitmapp/3;
  bmiHeader.biHeight        =  -bitmaph;
  bmiHeader.biPlanes        =  1;
  bmiHeader.biBitCount      =  24;
  bmiHeader.biCompression   =  BI_RGB;

  w = CreateWindow( "WHTrain",
                    "Vertical First Training",
                    WS_CAPTION | WS_POPUP| WS_CLIPCHILDREN |
                    WS_SYSMENU | WS_MINIMIZEBOX | WS_SIZEBOX,
                    CW_USEDEFAULT,CW_USEDEFAULT,
                    CW_USEDEFAULT,CW_USEDEFAULT,
                    0, 0, instance, 0 );

  {
    RECT r, c;
    GetWindowRect( w, &r );
    GetClientRect( w, &c );
    extrawindoww = ( r.right - r.left ) - ( c.right - c.left );
    extrawindowh = ( r.bottom - r.top ) - ( c.bottom - c.top );
    SetWindowPos( w, 0, 0, 0, bitmapw * curzoom + extrawindoww, bitmaph * curzoom + extrawindowh + 164, SWP_NOMOVE );
  }

  ShowWindow( w, SW_SHOWNORMAL );
  SetTimer( w, 1, 250, 0 );

  {
    BOOL ret;
    while( ( ret = GetMessage( &msg, w, 0, 0 ) ) != 0 )
    { 
      if ( ret == -1 )
        break;
      TranslateMessage( &msg ); 
      DispatchMessage( &msg ); 
    }
  }
}

/**
 * @brief Optimizes vertical-first resizing weights for all channel counts using multithreaded training and interactive visualization.
 *
 * Launches a separate training thread for each channel count to exhaustively search for optimal weights that minimize timing errors. Displays an interactive window showing training progress and allows user cancellation. After training, writes the resulting visualization bitmap and prints the optimized weights. Prints "CANCELLED!" if the process was interrupted.
 */
static void retrain()
{
  HANDLE threads[ 16 ];
  int chanind;

  trainstart = GetTickCount();
  for( chanind = 0 ; chanind < fi[0].numtypes ; chanind++ )
    threads[ chanind ] = CreateThread( 0, 2048*1024, retrain_shim, (LPVOID)(size_t)chanind, 0, 0 );

  draw_window();

  for( chanind = 0 ; chanind < fi[0].numtypes ; chanind++ )
  {
    WaitForSingleObject( threads[ chanind ], INFINITE );
    CloseHandle( threads[ chanind ] );
  }

  write_bitmap();

  print_struct( retrain_weights, "retained_weights" );
  if ( windowstatus ) printf( "CANCELLED!\n" );
}

/**
 * @brief Prints detailed information about each loaded timing file.
 *
 * Displays metadata for each timing file, including filename, CPU and SIMD type, total test time, cycles per second, tile and scaling dimensions, sample coordinates, available channel counts, and input rectangle sizes.
 */
static void info()
{
  int findex;

  // display info about each input file
  for( findex = 0 ; findex < numfileinfo ; findex++ )
  {
    int i, h,m,s;
    if ( findex ) printf( "\n" );
    printf( "Timing file: %s\n", fi[findex].filename );
    printf( "CPU type: %d  %s\n", fi[findex].cpu, fi[findex].simd?(fi[findex].simd==2?"SIMD8":"SIMD4"):"Scalar" );
    h = fi[findex].milliseconds/3600000;
    m = (fi[findex].milliseconds-h*3600000)/60000;
    s = (fi[findex].milliseconds-h*3600000-m*60000)/1000;
    printf( "Total time in test: %dh %dm %ds  Cycles/sec: %.f\n", h,m,s, 1000.0/fi[findex].scale_time );
    printf( "Each tile of samples is %dx%d, and is scaled by %dx%d.\n", fi[findex].dimensionx,fi[findex].dimensiony, fi[findex].outputscalex,fi[findex].outputscaley );
    printf( "So the x coords are: " );
    for( i=0; i < fi[findex].dimensionx ; i++ ) printf( "%d ",1+i*fi[findex].outputscalex );
    printf( "\n" );
    printf( "And the y coords are: " );
    for( i=0; i < fi[findex].dimensiony ; i++ ) printf( "%d ",1+i*fi[findex].outputscaley );
    printf( "\n" );
    printf( "There are %d channel counts and they are: ", fi[findex].numtypes );
    for( i=0; i < fi[findex].numtypes ; i++ ) printf( "%d ",fi[findex].effective[i] );
    printf( "\n" );
    printf( "There are %d input rect sizes and they are: ", fi[findex].numinputrects );
    for( i=0; i < fi[findex].numtypes ; i++ ) printf( "%dx%d ",fi[findex].inputrects[i*2],fi[findex].inputrects[i*2+1] );
    printf( "\n" );
  }
}

/**
 * @brief Evaluates and visualizes current weight performance across all channel counts.
 *
 * Calculates error metrics for the current set of weights on all loaded timing files and channel counts. Optionally prints error statistics, builds visualization bitmaps, displays an interactive window, or writes the bitmap to a PNG file depending on the provided flags.
 *
 * @param do_win If nonzero, displays the interactive visualization window.
 * @param do_bitmap If nonzero, writes the bitmap visualization to a PNG file; if zero, prints error statistics to the console.
 */
static void current( int do_win, int do_bitmap )
{
  int i, findex;

  trainstart = GetTickCount();

  // clear progress
  memset( windowranges, 0, sizeof( windowranges ) );
  // copy in appropriate weights
  memcpy( retrain_weights, stbir__compute_weights, sizeof( retrain_weights ) );

  // build and print current errors and build current bitmap
  for( i = 0 ; i < fi[0].numtypes ; i++ )
  {
    double curerr[STBIR_RESIZE_CLASSIFICATIONS];
    int curtot[STBIR_RESIZE_CLASSIFICATIONS];
    float (* weights)[4] = retrain_weights[i];
 
    calc_errors( weights, curtot, curerr, i );
    if ( !do_bitmap )
      print_weights( weights, i, curtot, curerr );

    for( findex = 0 ; findex < numfileinfo ; findex++ )
      build_bitmap( weights, i, findex );
  }

  if ( do_win )
    draw_window();

  if ( do_bitmap )
    write_bitmap();
}

/**
 * @brief Compares two timing files for consistency and visualizes their performance differences.
 *
 * Checks that the two loaded timing files are compatible in terms of types, input rectangles, dimensions, and scaling factors. If they match, builds a comparative bitmap highlighting which file performs better for each configuration and displays the result in an interactive window. Exits with an error if the files are incompatible.
 */
static void compare()
{
  int i;

  trainstart = GetTickCount();
  windowstatus = 2; // comp mode

  // clear progress
  memset( windowranges, 0, sizeof( windowranges ) );

  if ( ( fi[0].numtypes != fi[1].numtypes ) || ( fi[0].numinputrects != fi[1].numinputrects ) ||
       ( fi[0].dimensionx != fi[1].dimensionx ) || ( fi[0].dimensiony != fi[1].dimensiony ) || 
       ( fi[0].outputscalex != fi[1].outputscalex ) || ( fi[0].outputscaley != fi[1].outputscaley ) )
  {
   err:
    printf( "Timing files don't match.\n" );
    exit(5);
  }

  for( i=0; i < fi[0].numtypes ; i++ )
  {
    if ( fi[0].effective[i]      != fi[1].effective[i] ) goto err;
    if ( fi[0].inputrects[i*2]   != fi[1].inputrects[i*2] ) goto err;
    if ( fi[0].inputrects[i*2+1] != fi[1].inputrects[i*2+1] ) goto err;
  }
    
  alloc_bitmap( 1 );
  
  for( i = 0 ; i < fi[0].numtypes ; i++ )
  {
    float (* weights)[4] = stbir__compute_weights[i];
    build_comp_bitmap( weights, i );
  }

  draw_window();
}

/**
 * @brief Loads multiple timing files into the global fileinfo array.
 *
 * Attempts to parse each provided timing file and stores the results. Exits the program if any file is missing or invalid.
 *
 * @param args Array of file paths to timing files.
 * @param count Number of timing files to load.
 */
static void load_files( char ** args, int count )
{
  int i;

  if ( count == 0 )
  {
    printf( "No timing files listed!" );
    exit(3);
  }

  for ( i = 0 ; i < count ; i++ )
  {
    if ( !use_timing_file( args[i], i ) )
    {
      printf( "Bad timing file %s\n", args[i] );
      exit(2);
    }
  }
  numfileinfo = count;
}  

/**
 * @brief Entry point for the vertical-first training and visualization tool.
 *
 * Parses command-line arguments to select and execute a mode: retraining weights, displaying timing file info, checking current weights, generating a bitmap visualization, or comparing two timing files. Loads timing data files as needed and dispatches to the appropriate processing or visualization routines. Prints usage instructions and exits on invalid arguments.
 *
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line argument strings.
 * @return 0 on successful execution; exits with error code on failure or invalid usage.
 */
int main( int argc, char ** argv )
{
  int check;
  if ( argc < 3 )
  {
   err:
    printf( "vf_train retrain [timing_filenames....] - recalcs weights for all the files on the command line.\n");
    printf( "vf_train info [timing_filenames....] - shows info about each timing file.\n");
    printf( "vf_train check [timing_filenames...] - show results for the current weights for all files listed.\n");
    printf( "vf_train compare <timing file1> <timing file2> - compare two timing files (must only be two files and same resolution).\n");
    printf( "vf_train bitmap [timing_filenames...] - write out results.png, comparing against the current weights for all files listed.\n");
    exit(1);
  }
  
  check = ( strcmp( argv[1], "check" ) == 0 );
  if ( ( check ) || ( strcmp( argv[1], "bitmap" ) == 0 ) )
  {
    load_files( argv + 2, argc - 2 );
    alloc_bitmap( numfileinfo );
    current( check, !check );
  }
  else if ( strcmp( argv[1], "info" ) == 0 ) 
  {
    load_files( argv + 2, argc - 2 );
    info();
  }
  else if ( strcmp( argv[1], "compare" ) == 0 ) 
  {
    if ( argc != 4 )
    {
      printf( "You must specify two files to compare.\n" );
      exit(4);
    }

    load_files( argv + 2, argc - 2 );
    compare();
  }
  else if ( strcmp( argv[1], "retrain" ) == 0 ) 
  {
    load_files( argv + 2, argc - 2 );
    alloc_bitmap( numfileinfo );
    retrain();  
  }
  else
  {
    goto err;
  }

  return 0;
}
