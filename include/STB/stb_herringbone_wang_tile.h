/* stbhw - v0.7 -  http://nothings.org/gamedev/herringbone
   Herringbone Wang Tile Generator - Sean Barrett 2014 - public domain

== LICENSE ==============================

This software is dual-licensed to the public domain and under the following
license: you are granted a perpetual, irrevocable license to copy, modify,
publish, and distribute this file as you see fit.

== WHAT IT IS ===========================

 This library is an SDK for Herringbone Wang Tile generation:

      http://nothings.org/gamedev/herringbone

 The core design is that you use this library offline to generate a
 "template" of the tiles you'll create. You then edit those tiles, then
 load the created tile image file back into this library and use it at
 runtime to generate "maps".

 You cannot load arbitrary tile image files with this library; it is
 only designed to load image files made from the template it created.
 It stores a binary description of the tile sizes & constraints in a
 few pixels, and uses those to recover the rules, rather than trying
 to parse the tiles themselves.

 You *can* use this library to generate from arbitrary tile sets, but
 only by loading the tile set and specifying the constraints explicitly
 yourself.

== COMPILING ============================

 1. #define STB_HERRINGBONE_WANG_TILE_IMPLEMENTATION before including this
    header file in *one* source file to create the implementation
    in that source file.

 2. optionally #define STB_HBWANG_RAND() to be a random number
    generator. if you don't define it, it will use rand(),
    and you need to seed srand() yourself.

 3. optionally #define STB_HBWANG_ASSERT(x), otherwise
    it will use assert()

 4. optionally #define STB_HBWANG_STATIC to force all symbols to be
    static instead of public, so they are only accesible
    in the source file that creates the implementation

 5. optionally #define STB_HBWANG_NO_REPITITION_REDUCTION to disable
    the code that tries to reduce having the same tile appear
    adjacent to itself in wang-corner-tile mode (e.g. imagine
    if you were doing something where 90% of things should be
    the same grass tile, you need to disable this system)

 6. optionally define STB_HBWANG_MAX_X and STB_HBWANG_MAX_Y
    to be the max dimensions of the generated map in multiples
    of the wang tile's short side's length (e.g. if you
    have 20x10 wang tiles, so short_side_len=10, and you
    have MAX_X is 17, then the largest map you can generate
    is 170 pixels wide). The defaults are 100x100. This
    is used to define static arrays which affect memory
    usage.

== USING ================================

  To use the map generator, you need a tileset. You can download
  some sample tilesets from http://nothings.org/gamedev/herringbone

  Then see the "sample application" below.

  You can also use this file to generate templates for
  tilesets which you then hand-edit to create the data.


== MEMORY MANAGEMENT ====================

  The tileset loader allocates memory with malloc(). The map
  generator does no memory allocation, so e.g. you can load
  tilesets at startup and never free them and never do any
  further allocation.


== SAMPLE APPLICATION ===================

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"        // http://nothings.org/stb_image.c

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"  // http://nothings.org/stb/stb_image_write.h

#define STB_HBWANG_IMPLEMENTATION
#include "stb_hbwang.h"

int main(int argc, char **argv)
{
   unsigned char *data;
   int xs,ys, w,h;
   stbhw_tileset ts;

   if (argc != 4) {
      fprintf(stderr, "Usage: mapgen {tile-file} {xsize} {ysize}\n"
                      "generates file named 'test_map.png'\n");
      exit(1);
   }
   data = stbi_load(argv[1], &w, &h, NULL, 3);
   xs = atoi(argv[2]);
   ys = atoi(argv[3]);
   if (data == NULL) {
      fprintf(stderr, "Error opening or parsing '%s' as an image file\n", argv[1]);
      exit(1);
   }
   if (xs < 1 || xs > 1000) {
      fprintf(stderr, "xsize invalid or out of range\n");
      exit(1);
   }
   if (ys < 1 || ys > 1000) {
      fprintf(stderr, "ysize invalid or out of range\n");
      exit(1);
   }

   stbhw_build_tileset_from_image(&ts, data, w*3, w, h);
   free(data);

   // allocate a buffer to create the final image to
   data = malloc(3 * xs * ys);

   srand(time(NULL));
   stbhw_generate_image(&ts, NULL, data, xs*3, xs, ys);

   stbi_write_png("test_map.png", xs, ys, 3, data, xs*3);

   stbhw_free_tileset(&ts);
   free(data);

   return 0;
}

== VERSION HISTORY ===================

   0.7   2019-03-04   - fix warnings
	0.6   2014-08-17   - fix broken map-maker
	0.5   2014-07-07   - initial release

*/

//////////////////////////////////////////////////////////////////////////////
//                                                                          //
//                         HEADER FILE SECTION                              //
//                                                                          //

#ifndef INCLUDE_STB_HWANG_H
#define INCLUDE_STB_HWANG_H

#ifdef STB_HBWANG_STATIC
#define STBHW_EXTERN static
#else
#ifdef __cplusplus
#define STBHW_EXTERN extern "C"
#else
#define STBHW_EXTERN extern
#endif
#endif

typedef struct stbhw_tileset stbhw_tileset;

// returns description of last error produced by any function (not thread-safe)
STBHW_EXTERN const char *stbhw_get_last_error(void);

// build a tileset from an image that conforms to a template created by this
// library. (you allocate storage for stbhw_tileset and function fills it out;
// memory for individual tiles are malloc()ed).
// returns non-zero on success, 0 on error
STBHW_EXTERN int stbhw_build_tileset_from_image(stbhw_tileset *ts,
                     unsigned char *pixels, int stride_in_bytes, int w, int h);

// free a tileset built by stbhw_build_tileset_from_image
STBHW_EXTERN void stbhw_free_tileset(stbhw_tileset *ts);

// generate a map that is w * h pixels (3-bytes each)
// returns non-zero on success, 0 on error
// not thread-safe (uses a global data structure to avoid memory management)
// weighting should be NULL, as non-NULL weighting is currently untested
STBHW_EXTERN int stbhw_generate_image(stbhw_tileset *ts, int **weighting,
                     unsigned char *pixels, int stride_in_bytes, int w, int h);

//////////////////////////////////////
//
// TILESET DATA STRUCTURE
//
// if you use the image-to-tileset system from this file, you
// don't need to worry about these data structures. but if you
// want to build/load a tileset yourself, you'll need to fill
// these out.

typedef struct
{
   // the edge or vertex constraints, according to diagram below
   signed char a,b,c,d,e,f;

   // The herringbone wang tile data; it is a bitmap which is either
   // w=2*short_sidelen,h=short_sidelen, or w=short_sidelen,h=2*short_sidelen.
   // it is always RGB, stored row-major, with no padding between rows.
   // (allocate stbhw_tile structure to be large enough for the pixel data)
   unsigned char pixels[1];
} stbhw_tile;

struct stbhw_tileset
{
   int is_corner;
   int num_color[6];  // number of colors for each of 6 edge types or 4 corner types
   int short_side_len;
   stbhw_tile **h_tiles;
   stbhw_tile **v_tiles;
   int num_h_tiles, max_h_tiles;
   int num_v_tiles, max_v_tiles;
};

///////////////  TEMPLATE GENERATOR  //////////////////////////

// when requesting a template, you fill out this data
typedef struct
{
   int is_corner;      // using corner colors or edge colors?
   int short_side_len; // rectangles is 2n x n, n = short_side_len
   int num_color[6];   // see below diagram for meaning of the index to this;
                       // 6 values if edge (!is_corner), 4 values if is_corner
                       // legal numbers: 1..8 if edge, 1..4 if is_corner
   int num_vary_x;     // additional number of variations along x axis in the template
   int num_vary_y;     // additional number of variations along y axis in the template
   int corner_type_color_template[4][4];
      // if corner_type_color_template[s][t] is non-zero, then any
      // corner of type s generated as color t will get a little
      // corner sample markup in the template image data

} stbhw_config;

// computes the size needed for the template image
STBHW_EXTERN void stbhw_get_template_size(stbhw_config *c, int *w, int *h);

// generates a template image, assuming data is 3*w*h bytes long, RGB format
STBHW_EXTERN int stbhw_make_template(stbhw_config *c, unsigned char *data, int w, int h, int stride_in_bytes);

#endif//INCLUDE_STB_HWANG_H


// TILE CONSTRAINT TYPES
//
// there are 4 "types" of corners and 6 types of edges.
// you can configure the tileset to have different numbers
// of colors for each type of color or edge.
//
// corner types:
//
//                     0---*---1---*---2---*---3
//                     |       |               |
//                     *       *               *
//                     |       |               |
//     1---*---2---*---3       0---*---1---*---2
//     |               |       |
//     *               *       *
//     |               |       |
//     0---*---1---*---2---*---3
//
//
//  edge types:
//
//     *---2---*---3---*      *---0---*
//     |               |      |       |
//     1               4      5       1
//     |               |      |       |
//     *---0---*---2---*      *       *
//                            |       |
//                            4       5
//                            |       |
//                            *---3---*
//
// TILE CONSTRAINTS
//
// each corner/edge has a color; this shows the name
// of the variable containing the color
//
// corner constraints:
//
//                        a---*---d
//                        |       |
//                        *       *
//                        |       |
//     a---*---b---*---c  b       e
//     |               |  |       |
//     *               *  *       *
//     |               |  |       |
//     d---*---e---*---f  c---*---f
//
//
//  edge constraints:
//
//     *---a---*---b---*      *---a---*
//     |               |      |       |
//     c               d      b       c
//     |               |      |       |
//     *---e---*---f---*      *       *
//                            |       |
//                            d       e
//                            |       |
//                            *---f---*
//


//////////////////////////////////////////////////////////////////////////////
//                                                                          //
//                       IMPLEMENTATION SECTION                             //
//                                                                          //

#ifdef STB_HERRINGBONE_WANG_TILE_IMPLEMENTATION


#include <string.h> // memcpy
#include <stdlib.h> // malloc

#ifndef STB_HBWANG_RAND
#include <stdlib.h>
#define STB_HBWANG_RAND()  (rand() >> 4)
#endif

#ifndef STB_HBWANG_ASSERT
#include <assert.h>
#define STB_HBWANG_ASSERT(x)  assert(x)
#endif

// map size
#ifndef STB_HBWANG_MAX_X
#define STB_HBWANG_MAX_X  100
#endif

#ifndef STB_HBWANG_MAX_Y
#define STB_HBWANG_MAX_Y  100
#endif

// global variables for color assignments
// @MEMORY change these to just store last two/three rows
//         and keep them on the stack
static signed char c_color[STB_HBWANG_MAX_Y+6][STB_HBWANG_MAX_X+6];
static signed char v_color[STB_HBWANG_MAX_Y+6][STB_HBWANG_MAX_X+5];
static signed char h_color[STB_HBWANG_MAX_Y+5][STB_HBWANG_MAX_X+6];

static const char *stbhw_error;
/**
 * @brief Retrieves and clears the last error message.
 *
 * Returns a pointer to the most recent error message generated by the library, or NULL if no error has occurred. The error message is cleared after retrieval.
 *
 * @return const char* The last error message, or NULL if none.
 */
STBHW_EXTERN const char *stbhw_get_last_error(void)
{
   const char *temp = stbhw_error;
   stbhw_error = 0;
   return temp;
}




/////////////////////////////////////////////////////////////
//
//  SHARED TEMPLATE-DESCRIPTION CODE
//
//  Used by both template generator and tileset parser; by
//  using the same code, they are locked in sync and we don't
//  need to try to do more sophisticated parsing of edge color
//  markup or something.

typedef void stbhw__process_rect(struct stbhw__process *p, int xpos, int ypos,
                                 int a, int b, int c, int d, int e, int f);

typedef struct stbhw__process
{
   stbhw_tileset *ts;
   stbhw_config *c;
   stbhw__process_rect *process_h_rect;
   stbhw__process_rect *process_v_rect;
   unsigned char *data;
   int stride,w,h;
} stbhw__process;

/**
 * @brief Iterates over all combinations of constraint values for a horizontal tile row and processes each tile.
 *
 * Calls the provided callback for each horizontal tile rectangle in a row, varying the constraint values (a, b, c, d, e, f) and the number of variants.
 *
 * @param p Pointer to the processing context containing configuration and callback.
 * @param xpos Starting x-coordinate for the row.
 * @param ypos Y-coordinate for the row.
 * @param a0,a1 Range for constraint 'a'.
 * @param b0,b1 Range for constraint 'b'.
 * @param c0,c1 Range for constraint 'c'.
 * @param d0,d1 Range for constraint 'd'.
 * @param e0,e1 Range for constraint 'e'.
 * @param f0,f1 Range for constraint 'f'.
 * @param variants Number of tile variants to process for each constraint combination.
 */
static void stbhw__process_h_row(stbhw__process *p,
                           int xpos, int ypos,
                           int a0, int a1,
                           int b0, int b1,
                           int c0, int c1,
                           int d0, int d1,
                           int e0, int e1,
                           int f0, int f1,
                           int variants)
{
   int a,b,c,d,e,f,v;

   for (v=0; v < variants; ++v)
      for (f=f0; f <= f1; ++f)
         for (e=e0; e <= e1; ++e)
            for (d=d0; d <= d1; ++d)
               for (c=c0; c <= c1; ++c)
                  for (b=b0; b <= b1; ++b)
                     for (a=a0; a <= a1; ++a) {
                        p->process_h_rect(p, xpos, ypos, a,b,c,d,e,f);
                        xpos += 2*p->c->short_side_len + 3;
                     }
}

/**
 * @brief Iterates over all combinations of vertical tile constraints and invokes a callback for each.
 *
 * For each combination of constraint values (a, b, c, d, e, f) within the specified ranges and for each variant, calls the process_v_rect callback to process a vertical tile rectangle at the given position.
 *
 * @param p Pointer to the processing context containing configuration and callback.
 * @param xpos Starting x-coordinate for tile placement.
 * @param ypos Starting y-coordinate for tile placement.
 * @param a0 Minimum value for constraint a.
 * @param a1 Maximum value for constraint a.
 * @param b0 Minimum value for constraint b.
 * @param b1 Maximum value for constraint b.
 * @param c0 Minimum value for constraint c.
 * @param c1 Maximum value for constraint c.
 * @param d0 Minimum value for constraint d.
 * @param d1 Maximum value for constraint d.
 * @param e0 Minimum value for constraint e.
 * @param e1 Maximum value for constraint e.
 * @param f0 Minimum value for constraint f.
 * @param f1 Maximum value for constraint f.
 * @param variants Number of tile variants to process for each constraint combination.
 */
static void stbhw__process_v_row(stbhw__process *p,
                           int xpos, int ypos,
                           int a0, int a1,
                           int b0, int b1,
                           int c0, int c1,
                           int d0, int d1,
                           int e0, int e1,
                           int f0, int f1,
                           int variants)
{
   int a,b,c,d,e,f,v;

   for (v=0; v < variants; ++v)
      for (f=f0; f <= f1; ++f)
         for (e=e0; e <= e1; ++e)
            for (d=d0; d <= d1; ++d)
               for (c=c0; c <= c1; ++c)
                  for (b=b0; b <= b1; ++b)
                     for (a=a0; a <= a1; ++a) {
                        p->process_v_rect(p, xpos, ypos, a,b,c,d,e,f);
                        xpos += p->c->short_side_len+3;
                     }
}

/**
 * @brief Calculates the template image size and tile counts for a given configuration.
 *
 * Determines the required width and height of the template image, as well as the number of horizontal and vertical tiles, based on the provided tile configuration.
 *
 * @param c Pointer to the tile configuration structure.
 * @param w Output pointer for the computed template image width (may be NULL).
 * @param h Output pointer for the computed template image height (may be NULL).
 * @param h_count Output pointer for the number of horizontal tiles (may be NULL).
 * @param v_count Output pointer for the number of vertical tiles (may be NULL).
 */
static void stbhw__get_template_info(stbhw_config *c, int *w, int *h, int *h_count, int *v_count)
{
   int size_x,size_y;
   int horz_count,vert_count;

   if (c->is_corner) {
      int horz_w = c->num_color[1] * c->num_color[2] * c->num_color[3] * c->num_vary_x;
      int horz_h = c->num_color[0] * c->num_color[1] * c->num_color[2] * c->num_vary_y;

      int vert_w = c->num_color[0] * c->num_color[3] * c->num_color[2] * c->num_vary_y;
      int vert_h = c->num_color[1] * c->num_color[0] * c->num_color[3] * c->num_vary_x;

      int horz_x = horz_w * (2*c->short_side_len + 3);
      int horz_y = horz_h * (  c->short_side_len + 3);

      int vert_x = vert_w * (  c->short_side_len + 3);
      int vert_y = vert_h * (2*c->short_side_len + 3);

      horz_count = horz_w * horz_h;
      vert_count = vert_w * vert_h;

      size_x = horz_x > vert_x ? horz_x : vert_x;
      size_y = 2 + horz_y + 2 + vert_y;
   } else {
      int horz_w = c->num_color[0] * c->num_color[1] * c->num_color[2] * c->num_vary_x;
      int horz_h = c->num_color[3] * c->num_color[4] * c->num_color[2] * c->num_vary_y;

      int vert_w = c->num_color[0] * c->num_color[5] * c->num_color[1] * c->num_vary_y;
      int vert_h = c->num_color[3] * c->num_color[4] * c->num_color[5] * c->num_vary_x;

      int horz_x = horz_w * (2*c->short_side_len + 3);
      int horz_y = horz_h * (  c->short_side_len + 3);

      int vert_x = vert_w * (  c->short_side_len + 3);
      int vert_y = vert_h * (2*c->short_side_len + 3);

      horz_count = horz_w * horz_h;
      vert_count = vert_w * vert_h;

      size_x = horz_x > vert_x ? horz_x : vert_x;
      size_y = 2 + horz_y + 2 + vert_y;
   }
   if (w) *w = size_x;
   if (h) *h = size_y;
   if (h_count) *h_count = horz_count;
   if (v_count) *v_count = vert_count;
}

/**
 * @brief Computes the required width and height for a tile template image based on the given configuration.
 *
 * @param c Pointer to the tile template configuration.
 * @param w Output pointer to receive the computed image width.
 * @param h Output pointer to receive the computed image height.
 */
STBHW_EXTERN void stbhw_get_template_size(stbhw_config *c, int *w, int *h)
{
   stbhw__get_template_info(c, w, h, NULL, NULL);
}

/**
 * @brief Iterates over all tile rectangles in a template image and processes them using provided callbacks.
 *
 * Depending on the configuration, traverses the template image layout for either corner-based or edge-based Wang tiles, invoking the appropriate processing function for each horizontal and vertical tile rectangle. Validates that the image is large enough for the specified configuration.
 *
 * @param p Pointer to a processing context containing configuration and callback functions.
 * @return int Returns 1 on success, or 0 if the image is too small for the configuration.
 */
static int stbhw__process_template(stbhw__process *p)
{
   int i,j,k,q, ypos;
   int size_x, size_y;
   stbhw_config *c = p->c;

   stbhw__get_template_info(c, &size_x, &size_y, NULL, NULL);

   if (p->w < size_x || p->h < size_y) {
      stbhw_error = "image too small for configuration";
      return 0;
   }

   if (c->is_corner) {
      ypos = 2;
      for (k=0; k < c->num_color[2]; ++k) {
         for (j=0; j < c->num_color[1]; ++j) {
            for (i=0; i < c->num_color[0]; ++i) {
               for (q=0; q < c->num_vary_y; ++q) {
                  stbhw__process_h_row(p, 0,ypos,
                     0,c->num_color[1]-1, 0,c->num_color[2]-1, 0,c->num_color[3]-1,
                     i,i, j,j, k,k,
                     c->num_vary_x);
                  ypos += c->short_side_len + 3;
               }
            }
         }
      }
      ypos += 2;
      for (k=0; k < c->num_color[3]; ++k) {
         for (j=0; j < c->num_color[0]; ++j) {
            for (i=0; i < c->num_color[1]; ++i) {
               for (q=0; q < c->num_vary_x; ++q) {
                  stbhw__process_v_row(p, 0,ypos,
                     0,c->num_color[0]-1, 0,c->num_color[3]-1, 0,c->num_color[2]-1,
                     i,i, j,j, k,k,
                     c->num_vary_y);
                  ypos += (c->short_side_len*2) + 3;
               }
            }
         }
      }
      assert(ypos == size_y);
   } else {
      ypos = 2;
      for (k=0; k < c->num_color[3]; ++k) {
         for (j=0; j < c->num_color[4]; ++j) {
            for (i=0; i < c->num_color[2]; ++i) {
               for (q=0; q < c->num_vary_y; ++q) {
                  stbhw__process_h_row(p, 0,ypos,
                     0,c->num_color[2]-1, k,k,
                     0,c->num_color[1]-1, j,j,
                     0,c->num_color[0]-1, i,i,
                     c->num_vary_x);
                  ypos += c->short_side_len + 3;
               }
            }
         }
      }
      ypos += 2;
      for (k=0; k < c->num_color[3]; ++k) {
         for (j=0; j < c->num_color[4]; ++j) {
            for (i=0; i < c->num_color[5]; ++i) {
               for (q=0; q < c->num_vary_x; ++q) {
                  stbhw__process_v_row(p, 0,ypos,
                     0,c->num_color[0]-1, i,i,
                     0,c->num_color[1]-1, j,j,
                     0,c->num_color[5]-1, k,k,
                     c->num_vary_y);
                  ypos += (c->short_side_len*2) + 3;
               }
            }
         }
      }
      assert(ypos == size_y);
   }
   return 1;
}


/////////////////////////////////////////////////////////////
//
//  MAP GENERATOR
/**
 * @brief Sets the RGB value of a single pixel in an image buffer.
 *
 * @param output Pointer to the start of the image buffer.
 * @param stride Number of bytes per image row.
 * @param x X-coordinate of the pixel.
 * @param y Y-coordinate of the pixel.
 * @param c Array of 3 bytes representing the RGB color to set.
 */

static void stbhw__draw_pixel(unsigned char *output, int stride, int x, int y, unsigned char c[3])
{
   memcpy(output + y*stride + x*3, c, 3);
}

/**
 * @brief Draws a horizontal tile into the output image buffer.
 *
 * Copies the pixel data of a horizontal tile into the specified position of the output image, ensuring drawing stays within image bounds.
 *
 * @param output Pointer to the output image buffer (RGB).
 * @param stride Number of bytes per row in the output image.
 * @param xmax Width of the output image in pixels.
 * @param ymax Height of the output image in pixels.
 * @param x X-coordinate of the top-left corner where the tile will be drawn.
 * @param y Y-coordinate of the top-left corner where the tile will be drawn.
 * @param h Pointer to the horizontal tile to draw.
 * @param sz Short side length of the tile.
 */
static void stbhw__draw_h_tile(unsigned char *output, int stride, int xmax, int ymax, int x, int y, stbhw_tile *h, int sz)
{
   int i,j;
   for (j=0; j < sz; ++j)
      if (y+j >= 0 && y+j < ymax)
         for (i=0; i < sz*2; ++i)
            if (x+i >= 0 && x+i < xmax)
               stbhw__draw_pixel(output,stride, x+i,y+j, &h->pixels[(j*sz*2 + i)*3]);
}

/**
 * @brief Draws a vertical tile into the output image buffer at the specified position.
 *
 * Copies the pixel data from the given vertical tile into the output image, ensuring drawing stays within image bounds.
 *
 * @param output Pointer to the output image buffer.
 * @param stride Number of bytes per row in the output image.
 * @param xmax Width of the output image in pixels.
 * @param ymax Height of the output image in pixels.
 * @param x X-coordinate of the top-left corner where the tile will be drawn.
 * @param y Y-coordinate of the top-left corner where the tile will be drawn.
 * @param h Pointer to the vertical tile to draw.
 * @param sz Short side length of the tile.
 */
static void stbhw__draw_v_tile(unsigned char *output, int stride, int xmax, int ymax, int x, int y, stbhw_tile *h, int sz)
{
   int i,j;
   for (j=0; j < sz*2; ++j)
      if (y+j >= 0 && y+j < ymax)
         for (i=0; i < sz; ++i)
            if (x+i >= 0 && x+i < xmax)
               stbhw__draw_pixel(output,stride, x+i,y+j, &h->pixels[(j*sz + i)*3]);
}


/**
 * @brief Selects a tile from a list that matches specified constraints, updating the constraints to match the chosen tile.
 *
 * Randomly chooses a tile from the provided list that satisfies the given constraint values. If a weighting array is provided, selection is weighted accordingly. The constraint values are updated to those of the selected tile. Returns NULL and sets an error message if no matching tile is found.
 *
 * @return Pointer to the selected stbhw_tile, or NULL if no match is found.
 */
static stbhw_tile * stbhw__choose_tile(stbhw_tile **list, int numlist,
                                      signed char *a, signed char *b, signed char *c,
                                      signed char *d, signed char *e, signed char *f,
                                      int **weighting)
{
   int i,n,m = 1<<30,pass;
   for (pass=0; pass < 2; ++pass) {
      n=0;
      // pass #1:
      //   count number of variants that match this partial set of constraints
      // pass #2:
      //   stop on randomly selected match
      for (i=0; i < numlist; ++i) {
         stbhw_tile *h = list[i];
         if ((*a < 0 || *a == h->a) &&
             (*b < 0 || *b == h->b) &&
             (*c < 0 || *c == h->c) &&
             (*d < 0 || *d == h->d) &&
             (*e < 0 || *e == h->e) &&
             (*f < 0 || *f == h->f)) {
            if (weighting)
               n += weighting[0][i];
            else
               n += 1;
            if (n > m) {
               // use list[i]
               // update constraints to reflect what we placed
               *a = h->a;
               *b = h->b;
               *c = h->c;
               *d = h->d;
               *e = h->e;
               *f = h->f;
               return h;
            }
         }
      }
      if (n == 0) {
         stbhw_error = "couldn't find tile matching constraints";
         return NULL;
      }
      m = STB_HBWANG_RAND() % n;
   }
   STB_HBWANG_ASSERT(0);
   return NULL;
}

/**
 * @brief Checks if the corner colors at two adjacent positions match.
 *
 * Compares the color at position (x, y) with the color at position (x+1, y+1) in the corner color array.
 *
 * @param x X-coordinate of the first corner.
 * @param y Y-coordinate of the first corner.
 * @return 1 if the colors match, 0 otherwise.
 */
static int stbhw__match(int x, int y)
{
   return c_color[y][x] == c_color[y+1][x+1];
}

/**
 * @brief Selects an index randomly from a weighted list.
 *
 * Chooses an index from 0 to num_options-1, where each index's probability is proportional to its corresponding weight.
 *
 * @param num_options Number of available options.
 * @param weights Array of integer weights for each option.
 * @return int The selected index based on the provided weights.
 */
static int stbhw__weighted(int num_options, int *weights)
{
   int k, total, choice;
   total = 0;
   for (k=0; k < num_options; ++k)
      total += weights[k];
   choice = STB_HBWANG_RAND() % total;
   total = 0;
   for (k=0; k < num_options; ++k) {
      total += weights[k];
      if (choice < total)
         break;
   }
   STB_HBWANG_ASSERT(k < num_options);
   return k;
}

/**
 * @brief Selects a new color index different from the given old color, optionally using weights.
 *
 * If weights are provided, selects a new color index (not equal to old_color) with probability proportional to the corresponding weight. If weights are NULL, selects a new color index uniformly at random from all options except old_color.
 *
 * @param old_color The color index to avoid.
 * @param num_options The total number of color options.
 * @param weights Optional array of weights for each color index; may be NULL for uniform selection.
 * @return int The selected color index, guaranteed to differ from old_color.
 */
static int stbhw__change_color(int old_color, int num_options, int *weights)
{
   if (weights) {
      int k, total, choice;
      total = 0;
      for (k=0; k < num_options; ++k)
         if (k != old_color)
            total += weights[k];
      choice = STB_HBWANG_RAND() % total;
      total = 0;
      for (k=0; k < num_options; ++k) {
         if (k != old_color) {
            total += weights[k];
            if (choice < total)
               break;
         }
      }
      STB_HBWANG_ASSERT(k < num_options);
      return k;
   } else {
      int offset = 1+STB_HBWANG_RAND() % (num_options-1);
      return (old_color+offset) % num_options;
   }
}



// generate a map that is w * h pixels (3-bytes each)
/**
 * @brief Generates a herringbone Wang tile map image using the provided tileset.
 *
 * Fills the output image buffer with a procedurally generated herringbone Wang tile map, selecting and placing tiles from the given tileset according to their edge or corner constraints. Optionally applies weighting to influence tile selection. In corner mode, reduces obvious repetition of identical tile blocks.
 *
 * @param ts Pointer to a tileset previously built from a template image.
 * @param weighting Optional array of weighting tables for each constraint type; may be NULL for uniform selection.
 * @param output Pointer to the output image buffer (RGB, row-major).
 * @param stride Number of bytes per row in the output image.
 * @param w Width of the output image in pixels.
 * @param h Height of the output image in pixels.
 * @return 1 on success, 0 on error (with error message retrievable via stbhw_get_last_error()).
 */
STBHW_EXTERN int stbhw_generate_image(stbhw_tileset *ts, int **weighting, unsigned char *output, int stride, int w, int h)
{
   int sidelen = ts->short_side_len;
   int xmax = (w / sidelen) + 6;
   int ymax = (h / sidelen) + 6;
   if (xmax > STB_HBWANG_MAX_X+6 || ymax > STB_HBWANG_MAX_Y+6) {
      stbhw_error = "increase STB_HBWANG_MAX_X/Y";
      return 0;
   }

   if (ts->is_corner) {
      int i,j, ypos;
      int *cc = ts->num_color;

      for (j=0; j < ymax; ++j) {
         for (i=0; i < xmax; ++i) {
            int p = (i-j+1)&3; // corner type
            if (weighting==NULL || weighting[p]==0 || cc[p] == 1)
               c_color[j][i] = STB_HBWANG_RAND() % cc[p];
            else
               c_color[j][i] = stbhw__weighted(cc[p], weighting[p]);
         }
      }
      #ifndef STB_HBWANG_NO_REPITITION_REDUCTION
      // now go back through and make sure we don't have adjancent 3x2 vertices that are identical,
      // to avoid really obvious repetition (which happens easily with extreme weights)
      for (j=0; j < ymax-3; ++j) {
         for (i=0; i < xmax-3; ++i) {
            //int p = (i-j+1) & 3; // corner type   // unused, not sure what the intent was so commenting it out
            STB_HBWANG_ASSERT(i+3 < STB_HBWANG_MAX_X+6);
            STB_HBWANG_ASSERT(j+3 < STB_HBWANG_MAX_Y+6);
            if (stbhw__match(i,j) && stbhw__match(i,j+1) && stbhw__match(i,j+2)
                && stbhw__match(i+1,j) && stbhw__match(i+1,j+1) && stbhw__match(i+1,j+2)) {
               int p = ((i+1)-(j+1)+1) & 3;
               if (cc[p] > 1)
                  c_color[j+1][i+1] = stbhw__change_color(c_color[j+1][i+1], cc[p], weighting ? weighting[p] : NULL);
            }
            if (stbhw__match(i,j) && stbhw__match(i+1,j) && stbhw__match(i+2,j)
                && stbhw__match(i,j+1) && stbhw__match(i+1,j+1) && stbhw__match(i+2,j+1)) {
               int p = ((i+2)-(j+1)+1) & 3;
               if (cc[p] > 1)
                  c_color[j+1][i+2] = stbhw__change_color(c_color[j+1][i+2], cc[p], weighting ? weighting[p] : NULL);
            }
         }
      }
      #endif

      ypos = -1 * sidelen;
      for (j = -1; ypos < h; ++j) {
         // a general herringbone row consists of:
         //    horizontal left block, the bottom of a previous vertical, the top of a new vertical
         int phase = (j & 3);
         // displace horizontally according to pattern
         if (phase == 0) {
            i = 0;
         } else {
            i = phase-4;
         }
         for (;; i += 4) {
            int xpos = i * sidelen;
            if (xpos >= w)
               break;
            // horizontal left-block
            if (xpos + sidelen*2 >= 0 && ypos >= 0) {
               stbhw_tile *t = stbhw__choose_tile(
                  ts->h_tiles, ts->num_h_tiles,
                  &c_color[j+2][i+2], &c_color[j+2][i+3], &c_color[j+2][i+4],
                  &c_color[j+3][i+2], &c_color[j+3][i+3], &c_color[j+3][i+4],
                  weighting
               );
               if (t == NULL)
                  return 0;
               stbhw__draw_h_tile(output,stride,w,h, xpos, ypos, t, sidelen);
            }
            xpos += sidelen * 2;
            // now we're at the end of a previous vertical one
            xpos += sidelen;
            // now we're at the start of a new vertical one
            if (xpos < w) {
               stbhw_tile *t = stbhw__choose_tile(
                  ts->v_tiles, ts->num_v_tiles,
                  &c_color[j+2][i+5], &c_color[j+3][i+5], &c_color[j+4][i+5],
                  &c_color[j+2][i+6], &c_color[j+3][i+6], &c_color[j+4][i+6],
                  weighting
               );
               if (t == NULL)
                  return 0;
               stbhw__draw_v_tile(output,stride,w,h, xpos, ypos,  t, sidelen);
            }
         }
         ypos += sidelen;
      }
   } else {
      // @TODO edge-color repetition reduction
      int i,j, ypos;
      memset(v_color, -1, sizeof(v_color));
      memset(h_color, -1, sizeof(h_color));

      ypos = -1 * sidelen;
      for (j = -1; ypos<h; ++j) {
         // a general herringbone row consists of:
         //    horizontal left block, the bottom of a previous vertical, the top of a new vertical
         int phase = (j & 3);
         // displace horizontally according to pattern
         if (phase == 0) {
            i = 0;
         } else {
            i = phase-4;
         }
         for (;; i += 4) {
            int xpos = i * sidelen;
            if (xpos >= w)
               break;
            // horizontal left-block
            if (xpos + sidelen*2 >= 0 && ypos >= 0) {
               stbhw_tile *t = stbhw__choose_tile(
                  ts->h_tiles, ts->num_h_tiles,
                  &h_color[j+2][i+2], &h_color[j+2][i+3],
                  &v_color[j+2][i+2], &v_color[j+2][i+4],
                  &h_color[j+3][i+2], &h_color[j+3][i+3],
                  weighting
               );
               if (t == NULL) return 0;
               stbhw__draw_h_tile(output,stride,w,h, xpos, ypos, t, sidelen);
            }
            xpos += sidelen * 2;
            // now we're at the end of a previous vertical one
            xpos += sidelen;
            // now we're at the start of a new vertical one
            if (xpos < w) {
               stbhw_tile *t = stbhw__choose_tile(
                  ts->v_tiles, ts->num_v_tiles,
                  &h_color[j+2][i+5],
                  &v_color[j+2][i+5], &v_color[j+2][i+6],
                  &v_color[j+3][i+5], &v_color[j+3][i+6],
                  &h_color[j+4][i+5],
                  weighting
               );
               if (t == NULL) return 0;
               stbhw__draw_v_tile(output,stride,w,h, xpos, ypos,  t, sidelen);
            }
         }
         ypos += sidelen;
      }
   }
   return 1;
}

/**
 * @brief Extracts a horizontal tile from the template image and adds it to the tileset.
 *
 * Copies the pixel data and constraint values for a horizontal tile located at the specified position in the template image, then appends the new tile to the tileset's horizontal tile array.
 *
 * @param xpos X-coordinate of the tile's top-left corner in the template image.
 * @param ypos Y-coordinate of the tile's top-left corner in the template image.
 * @param a Constraint value for the first corner or edge.
 * @param b Constraint value for the second corner or edge.
 * @param c Constraint value for the third corner or edge.
 * @param d Constraint value for the fourth corner or edge.
 * @param e Constraint value for the fifth corner or edge.
 * @param f Constraint value for the sixth corner or edge.
 */
static void stbhw__parse_h_rect(stbhw__process *p, int xpos, int ypos,
                            int a, int b, int c, int d, int e, int f)
{
   int len = p->c->short_side_len;
   stbhw_tile *h = (stbhw_tile *) malloc(sizeof(*h)-1 + 3 * (len*2) * len);
   int i,j;
   ++xpos;
   ++ypos;
   h->a = a, h->b = b, h->c = c, h->d = d, h->e = e, h->f = f;
   for (j=0; j < len; ++j)
      for (i=0; i < len*2; ++i)
         memcpy(h->pixels + j*(3*len*2) + i*3, p->data+(ypos+j)*p->stride+(xpos+i)*3, 3);
   STB_HBWANG_ASSERT(p->ts->num_h_tiles < p->ts->max_h_tiles);
   p->ts->h_tiles[p->ts->num_h_tiles++] = h;
}

/**
 * @brief Extracts a vertical tile from the template image and adds it to the tileset.
 *
 * Copies pixel data and constraint values for a vertical tile located at the specified position in the template image, then appends the tile to the tileset's vertical tile array.
 *
 * @param p Internal processing context containing configuration, image data, and tileset.
 * @param xpos X-coordinate of the tile's top-left corner in the template image.
 * @param ypos Y-coordinate of the tile's top-left corner in the template image.
 * @param a Constraint value for the tile (meaning depends on mode).
 * @param b Constraint value for the tile.
 * @param c Constraint value for the tile.
 * @param d Constraint value for the tile.
 * @param e Constraint value for the tile.
 * @param f Constraint value for the tile.
 */
static void stbhw__parse_v_rect(stbhw__process *p, int xpos, int ypos,
                            int a, int b, int c, int d, int e, int f)
{
   int len = p->c->short_side_len;
   stbhw_tile *h = (stbhw_tile *) malloc(sizeof(*h)-1 + 3 * (len*2) * len);
   int i,j;
   ++xpos;
   ++ypos;
   h->a = a, h->b = b, h->c = c, h->d = d, h->e = e, h->f = f;
   for (j=0; j < len*2; ++j)
      for (i=0; i < len; ++i)
         memcpy(h->pixels + j*(3*len) + i*3, p->data+(ypos+j)*p->stride+(xpos+i)*3, 3);
   STB_HBWANG_ASSERT(p->ts->num_v_tiles < p->ts->max_v_tiles);
   p->ts->v_tiles[p->ts->num_v_tiles++] = h;
}

/**
 * @brief Builds a tileset from a template image.
 *
 * Parses a specially formatted template image to extract all horizontal and vertical tiles, their constraint data, and configuration parameters, populating the provided tileset structure for use in map generation.
 *
 * @param ts Pointer to the tileset structure to populate.
 * @param data Pointer to the image pixel data (RGB, row-major).
 * @param stride Number of bytes per image row.
 * @param w Image width in pixels.
 * @param h Image height in pixels.
 * @return Non-zero on success, 0 on failure (invalid image or configuration).
 */
STBHW_EXTERN int stbhw_build_tileset_from_image(stbhw_tileset *ts, unsigned char *data, int stride, int w, int h)
{
   int i, h_count, v_count;
   unsigned char header[9];
   stbhw_config c = { 0 };
   stbhw__process p = { 0 };

   // extract binary header

   // remove encoding that makes it more visually obvious it encodes actual data
   for (i=0; i < 9; ++i)
      header[i] = data[w*3 - 1 - i] ^ (i*55);

   // extract header info
   if (header[7] == 0xc0) {
      // corner-type
      c.is_corner = 1;
      for (i=0; i < 4; ++i)
         c.num_color[i] = header[i];
      c.num_vary_x = header[4];
      c.num_vary_y = header[5];
      c.short_side_len = header[6];
   } else {
      c.is_corner = 0;
      // edge-type
      for (i=0; i < 6; ++i)
         c.num_color[i] = header[i];
      c.num_vary_x = header[6];
      c.num_vary_y = header[7];
      c.short_side_len = header[8];
   }

   if (c.num_vary_x < 0 || c.num_vary_x > 64 || c.num_vary_y < 0 || c.num_vary_y > 64)
      return 0;
   if (c.short_side_len == 0)
      return 0;
   if (c.num_color[0] > 32 || c.num_color[1] > 32 || c.num_color[2] > 32 || c.num_color[3] > 32)
      return 0;

   stbhw__get_template_info(&c, NULL, NULL, &h_count, &v_count);

   ts->is_corner = c.is_corner;
   ts->short_side_len = c.short_side_len;
   memcpy(ts->num_color, c.num_color, sizeof(ts->num_color));

   ts->max_h_tiles = h_count;
   ts->max_v_tiles = v_count;

   ts->num_h_tiles = ts->num_v_tiles = 0;

   ts->h_tiles = (stbhw_tile **) malloc(sizeof(*ts->h_tiles) * h_count);
   ts->v_tiles = (stbhw_tile **) malloc(sizeof(*ts->v_tiles) * v_count);

   p.ts = ts;
   p.data = data;
   p.stride = stride;
   p.process_h_rect = stbhw__parse_h_rect;
   p.process_v_rect = stbhw__parse_v_rect;
   p.w = w;
   p.h = h;
   p.c = &c;

   // load all the tiles out of the image
   return stbhw__process_template(&p);
}

/**
 * @brief Frees all memory associated with a tileset.
 *
 * Releases memory allocated for the tiles and tile arrays in the given tileset and resets its fields.
 */
STBHW_EXTERN void stbhw_free_tileset(stbhw_tileset *ts)
{
   int i;
   for (i=0; i < ts->num_h_tiles; ++i)
      free(ts->h_tiles[i]);
   for (i=0; i < ts->num_v_tiles; ++i)
      free(ts->v_tiles[i]);
   free(ts->h_tiles);
   free(ts->v_tiles);
   ts->h_tiles = NULL;
   ts->v_tiles = NULL;
   ts->num_h_tiles = ts->max_h_tiles = 0;
   ts->num_v_tiles = ts->max_v_tiles = 0;
}

//////////////////////////////////////////////////////////////////////////////
//
//               GENERATOR
//
//


/**
 * @brief Sets the RGB value of a single pixel in an image buffer.
 *
 * @param data Pointer to the start of the image buffer.
 * @param stride Number of bytes per image row.
 * @param xpos X-coordinate of the pixel.
 * @param ypos Y-coordinate of the pixel.
 * @param color Array of 3 bytes representing the RGB color to set.
 */

static void stbhw__set_pixel(unsigned char *data, int stride, int xpos, int ypos, unsigned char color[3])
{
   memcpy(data + ypos*stride + xpos*3, color, 3);
}

/**
 * @brief Sets a pixel to a whitened version of the specified color in an image buffer.
 *
 * Computes a color that is a blend of the input color and white, then writes it to the specified (x, y) position in the image buffer.
 *
 * @param data Pointer to the image buffer (RGB, row-major).
 * @param stride Number of bytes per image row.
 * @param xpos X-coordinate of the pixel.
 * @param ypos Y-coordinate of the pixel.
 * @param color Input RGB color to be whitened.
 */
static void stbhw__stbhw__set_pixel_whiten(unsigned char *data, int stride, int xpos, int ypos, unsigned char color[3])
{
   unsigned char c2[3];
   int i;
   for (i=0; i < 3; ++i)
      c2[i] = (color[i]*2 + 255)/3;
   memcpy(data + ypos*stride + xpos*3, c2, 3);
}


static unsigned char stbhw__black[3] = { 0,0,0 };

// each edge set gets its own unique color variants
// used http://phrogz.net/css/distinct-colors.html to generate this set,
// but it's not very good and needs to be revised

static unsigned char stbhw__color[7][8][3] =
{
   { {255,51,51}  , {143,143,29}, {0,199,199}, {159,119,199},     {0,149,199}  , {143, 0,143}, {255,128,0}, {64,255,0},  },
   { {235,255,30 }, {255,0,255},  {199,139,119},  {29,143, 57},    {143,0,71}   , { 0,143,143}, {0,99,199}, {143,71,0},  },
   { {0,149,199}  , {143, 0,143}, {255,128,0}, {64,255,0},        {255,191,0}  , {51,255,153}, {0,0,143}, {199,119,159},},
   { {143,0,71}   , { 0,143,143}, {0,99,199}, {143,71,0},         {255,190,153}, { 0,255,255}, {128,0,255}, {255,51,102},},
   { {255,191,0}  , {51,255,153}, {0,0,143}, {199,119,159},       {255,51,51}  , {143,143,29}, {0,199,199}, {159,119,199},},
   { {255,190,153}, { 0,255,255}, {128,0,255}, {255,51,102},      {235,255,30 }, {255,0,255}, {199,139,119},  {29,143, 57}, },

   { {40,40,40 },  { 90,90,90 }, { 150,150,150 }, { 200,200,200 },
     { 255,90,90 }, { 160,160,80}, { 50,150,150 }, { 200,50,200 } },
};

/**
 * @brief Draws a horizontal line with a colored segment for template visualization.
 *
 * Draws a black horizontal line of specified length, with a central segment highlighted in a given color to indicate a constraint slot in a tile template image.
 *
 * @param data Pointer to the image buffer.
 * @param stride Number of bytes per image row.
 * @param xpos X-coordinate of the line's starting point.
 * @param ypos Y-coordinate of the line.
 * @param color Index of the color to use for the highlighted segment.
 * @param len Length of the line in pixels.
 * @param slot Index of the constraint slot for color selection.
 */
static void stbhw__draw_hline(unsigned char *data, int stride, int xpos, int ypos, int color, int len, int slot)
{
   int i;
   int j = len * 6 / 16;
   int k = len * 10 / 16;
   for (i=0; i < len; ++i)
      stbhw__set_pixel(data, stride, xpos+i, ypos, stbhw__black);
   if (k-j < 2) {
      j = len/2 - 1;
      k = j+2;
      if (len & 1)
         ++k;
   }
   for (i=j; i < k; ++i)
      stbhw__stbhw__set_pixel_whiten(data, stride, xpos+i, ypos, stbhw__color[slot][color]);
}

/**
 * @brief Draws a vertical line with a colored highlight segment in an image buffer.
 *
 * Draws a black vertical line of specified length at the given position, then overlays a colored segment within the line to indicate a constraint or marker.
 *
 * @param data Pointer to the image buffer.
 * @param stride Number of bytes per image row.
 * @param xpos X-coordinate of the line.
 * @param ypos Y-coordinate of the line's starting point.
 * @param color Index of the color to use for the highlight segment.
 * @param len Length of the vertical line.
 * @param slot Color slot index for selecting the highlight color.
 */
static void stbhw__draw_vline(unsigned char *data, int stride, int xpos, int ypos, int color, int len, int slot)
{
   int i;
   int j = len * 6 / 16;
   int k = len * 10 / 16;
   for (i=0; i < len; ++i)
      stbhw__set_pixel(data, stride, xpos, ypos+i, stbhw__black);
   if (k-j < 2) {
      j = len/2 - 1;
      k = j+2;
      if (len & 1)
         ++k;
   }
   for (i=j; i < k; ++i)
      stbhw__stbhw__set_pixel_whiten(data, stride, xpos, ypos+i, stbhw__color[slot][color]);
}

//                 0--*--1--*--2--*--3
//                 |     |           |
//                 *     *           *
//                 |     |           |
//     1--*--2--*--3     0--*--1--*--2
//     |           |     |
//     *           *     *
//     |           |     |
//     0--*--1--*--2--*--3
//
// variables while enumerating (no correspondence between corners
// of the types is implied by these variables)
//
//     a-----b-----c      a-----d
//     |           |      |     |
//     |           |      |     |
//     |           |      |     |
//     d-----e-----f      b     e
//                        |     |
//                        |     |
//                        |     |
//                        c-----f
//

unsigned char stbhw__corner_colors[4][4][3] =
{
   { { 255,0,0 }, { 200,200,200 }, { 100,100,200 }, { 255,200,150 }, },
   { { 0,0,255 }, { 255,255,0 },   { 100,200,100 }, { 150,255,200 }, },
   { { 255,0,255 }, { 80,80,80 },  { 200,100,100 }, { 200,150,255 }, },
   { { 0,255,255 }, { 0,255,0 },   { 200,120,200 }, { 255,200,200 }, },
};

int stbhw__corner_colors_to_edge_color[4][4] =
{
   // 0   1   2   3
   {  0,  1,  4,  9, }, // 0
   {  2,  3,  5, 10, }, // 1
   {  6,  7,  8, 11, }, // 2
   { 12, 13, 14, 15, }, // 3
};

#define stbhw__c2e stbhw__corner_colors_to_edge_color

/**
 * @brief Draws a clipped corner marker in the template image at a specified position.
 *
 * Draws a colored marker representing a corner constraint, clipped to avoid the extreme corners, within the bounds of the template image.
 *
 * @param data Pointer to the image pixel buffer.
 * @param stride Number of bytes per image row.
 * @param xpos X offset of the tile within the image.
 * @param ypos Y offset of the tile within the image.
 * @param w Width of the tile area.
 * @param h Height of the tile area.
 * @param x X coordinate of the corner within the tile.
 * @param y Y coordinate of the corner within the tile.
 */
static void stbhw__draw_clipped_corner(unsigned char *data, int stride, int xpos, int ypos, int w, int h, int x, int y)
{
   static unsigned char template_color[3] = { 167,204,204 };
   int i,j;
   for (j = -2; j <= 1; ++j) {
      for (i = -2; i <= 1; ++i) {
         if ((i == -2 || i == 1) && (j == -2 || j == 1))
            continue;
         else {
            if (x+i < 1 || x+i > w) continue;
            if (y+j < 1 || y+j > h) continue;
            stbhw__set_pixel(data, stride, xpos+x+i, ypos+y+j, template_color);

         }
      }
   }
}

/**
 * @brief Draws the edge constraint markers for a horizontal tile rectangle in edge-based Wang tile mode.
 *
 * Draws colored lines representing the six edge constraints (a–f) for a horizontal tile at the specified position in the template image buffer.
 *
 * @param p Pointer to the processing context containing template configuration and image buffer.
 * @param xpos X-coordinate of the top-left corner of the tile rectangle.
 * @param ypos Y-coordinate of the top-left corner of the tile rectangle.
 * @param a–f Edge constraint color indices for the tile's edges.
 */
static void stbhw__edge_process_h_rect(stbhw__process *p, int xpos, int ypos,
                            int a, int b, int c, int d, int e, int f)
{
   int len = p->c->short_side_len;
   stbhw__draw_hline(p->data, p->stride, xpos+1        , ypos        , a, len, 2);
   stbhw__draw_hline(p->data, p->stride, xpos+  len+1  , ypos        , b, len, 3);
   stbhw__draw_vline(p->data, p->stride, xpos          , ypos+1      , c, len, 1);
   stbhw__draw_vline(p->data, p->stride, xpos+2*len+1  , ypos+1      , d, len, 4);
   stbhw__draw_hline(p->data, p->stride, xpos+1        , ypos + len+1, e, len, 0);
   stbhw__draw_hline(p->data, p->stride, xpos + len+1  , ypos + len+1, f, len, 2);
}

/**
 * @brief Draws a vertical tile rectangle for edge-based Wang tile templates.
 *
 * Marks the edges of a vertical tile in the template image with color codes representing edge constraints, according to the specified parameters and configuration.
 *
 * @param p Processing context containing template configuration and image data.
 * @param xpos X-coordinate of the rectangle's top-left corner.
 * @param ypos Y-coordinate of the rectangle's top-left corner.
 * @param a-f Edge constraint color indices for the tile's six edges.
 */
static void stbhw__edge_process_v_rect(stbhw__process *p, int xpos, int ypos,
                            int a, int b, int c, int d, int e, int f)
{
   int len = p->c->short_side_len;
   stbhw__draw_hline(p->data, p->stride, xpos+1      , ypos          , a, len, 0);
   stbhw__draw_vline(p->data, p->stride, xpos        , ypos+1        , b, len, 5);
   stbhw__draw_vline(p->data, p->stride, xpos + len+1, ypos+1        , c, len, 1);
   stbhw__draw_vline(p->data, p->stride, xpos        , ypos +   len+1, d, len, 4);
   stbhw__draw_vline(p->data, p->stride, xpos + len+1, ypos +   len+1, e, len, 5);
   stbhw__draw_hline(p->data, p->stride, xpos+1      , ypos + 2*len+1, f, len, 3);
}

/**
 * @brief Draws a horizontal tile rectangle for a corner-based Wang tile template.
 *
 * Marks the edges and corners of a horizontal tile rectangle in the template image buffer, using color codes to encode constraint values for each corner and edge. This function is used during template generation in corner mode to visually encode tile constraints.
 *
 * @param p Pointer to the processing context containing template configuration and image buffer.
 * @param xpos X-coordinate of the rectangle's top-left corner in the image.
 * @param ypos Y-coordinate of the rectangle's top-left corner in the image.
 * @param a Constraint value for the top-left corner.
 * @param b Constraint value for the top-center corner.
 * @param c Constraint value for the top-right corner.
 * @param d Constraint value for the bottom-left corner.
 * @param e Constraint value for the bottom-center corner.
 * @param f Constraint value for the bottom-right corner.
 */
static void stbhw__corner_process_h_rect(stbhw__process *p, int xpos, int ypos,
                            int a, int b, int c, int d, int e, int f)
{
   int len = p->c->short_side_len;

   stbhw__draw_hline(p->data, p->stride, xpos+1        , ypos        , stbhw__c2e[a][b], len, 2);
   stbhw__draw_hline(p->data, p->stride, xpos+  len+1  , ypos        , stbhw__c2e[b][c], len, 3);
   stbhw__draw_vline(p->data, p->stride, xpos          , ypos+1      , stbhw__c2e[a][d], len, 1);
   stbhw__draw_vline(p->data, p->stride, xpos+2*len+1  , ypos+1      , stbhw__c2e[c][f], len, 4);
   stbhw__draw_hline(p->data, p->stride, xpos+1        , ypos + len+1, stbhw__c2e[d][e], len, 0);
   stbhw__draw_hline(p->data, p->stride, xpos + len+1  , ypos + len+1, stbhw__c2e[e][f], len, 2);

   if (p->c->corner_type_color_template[1][a]) stbhw__draw_clipped_corner(p->data,p->stride, xpos,ypos, len*2,len, 1,1);
   if (p->c->corner_type_color_template[2][b]) stbhw__draw_clipped_corner(p->data,p->stride, xpos,ypos, len*2,len, len+1,1);
   if (p->c->corner_type_color_template[3][c]) stbhw__draw_clipped_corner(p->data,p->stride, xpos,ypos, len*2,len, len*2+1,1);

   if (p->c->corner_type_color_template[0][d]) stbhw__draw_clipped_corner(p->data,p->stride, xpos,ypos, len*2,len, 1,len+1);
   if (p->c->corner_type_color_template[1][e]) stbhw__draw_clipped_corner(p->data,p->stride, xpos,ypos, len*2,len, len+1,len+1);
   if (p->c->corner_type_color_template[2][f]) stbhw__draw_clipped_corner(p->data,p->stride, xpos,ypos, len*2,len, len*2+1,len+1);

   stbhw__set_pixel(p->data, p->stride, xpos        , ypos, stbhw__corner_colors[1][a]);
   stbhw__set_pixel(p->data, p->stride, xpos+len    , ypos, stbhw__corner_colors[2][b]);
   stbhw__set_pixel(p->data, p->stride, xpos+2*len+1, ypos, stbhw__corner_colors[3][c]);
   stbhw__set_pixel(p->data, p->stride, xpos        , ypos+len+1, stbhw__corner_colors[0][d]);
   stbhw__set_pixel(p->data, p->stride, xpos+len    , ypos+len+1, stbhw__corner_colors[1][e]);
   stbhw__set_pixel(p->data, p->stride, xpos+2*len+1, ypos+len+1, stbhw__corner_colors[2][f]);
}

/**
 * @brief Draws a vertical tile rectangle for a corner-based Wang tile template.
 *
 * Marks the edges and corners of a vertical tile rectangle in the template image buffer, using the specified corner constraint values and color templates.
 */
static void stbhw__corner_process_v_rect(stbhw__process *p, int xpos, int ypos,
                            int a, int b, int c, int d, int e, int f)
{
   int len = p->c->short_side_len;

   stbhw__draw_hline(p->data, p->stride, xpos+1      , ypos          , stbhw__c2e[a][d], len, 0);
   stbhw__draw_vline(p->data, p->stride, xpos        , ypos+1        , stbhw__c2e[a][b], len, 5);
   stbhw__draw_vline(p->data, p->stride, xpos + len+1, ypos+1        , stbhw__c2e[d][e], len, 1);
   stbhw__draw_vline(p->data, p->stride, xpos        , ypos +   len+1, stbhw__c2e[b][c], len, 4);
   stbhw__draw_vline(p->data, p->stride, xpos + len+1, ypos +   len+1, stbhw__c2e[e][f], len, 5);
   stbhw__draw_hline(p->data, p->stride, xpos+1      , ypos + 2*len+1, stbhw__c2e[c][f], len, 3);

   if (p->c->corner_type_color_template[0][a]) stbhw__draw_clipped_corner(p->data,p->stride, xpos,ypos, len,len*2, 1,1);
   if (p->c->corner_type_color_template[3][b]) stbhw__draw_clipped_corner(p->data,p->stride, xpos,ypos, len,len*2, 1,len+1);
   if (p->c->corner_type_color_template[2][c]) stbhw__draw_clipped_corner(p->data,p->stride, xpos,ypos, len,len*2, 1,len*2+1);

   if (p->c->corner_type_color_template[1][d]) stbhw__draw_clipped_corner(p->data,p->stride, xpos,ypos, len,len*2, len+1,1);
   if (p->c->corner_type_color_template[0][e]) stbhw__draw_clipped_corner(p->data,p->stride, xpos,ypos, len,len*2, len+1,len+1);
   if (p->c->corner_type_color_template[3][f]) stbhw__draw_clipped_corner(p->data,p->stride, xpos,ypos, len,len*2, len+1,len*2+1);

   stbhw__set_pixel(p->data, p->stride, xpos      , ypos        , stbhw__corner_colors[0][a]);
   stbhw__set_pixel(p->data, p->stride, xpos      , ypos+len    , stbhw__corner_colors[3][b]);
   stbhw__set_pixel(p->data, p->stride, xpos      , ypos+2*len+1, stbhw__corner_colors[2][c]);
   stbhw__set_pixel(p->data, p->stride, xpos+len+1, ypos        , stbhw__corner_colors[1][d]);
   stbhw__set_pixel(p->data, p->stride, xpos+len+1, ypos+len    , stbhw__corner_colors[0][e]);
   stbhw__set_pixel(p->data, p->stride, xpos+len+1, ypos+2*len+1, stbhw__corner_colors[3][f]);
}

/**
 * @brief Generates a Wang tile template image based on the provided configuration.
 *
 * Fills the given RGB image buffer with a visual template encoding all tile constraints and variations as specified in the configuration. The template includes color-coded markers for tile edges or corners and embeds binary configuration data in the image for later parsing.
 *
 * @param c Pointer to the configuration specifying tile mode, color counts, variations, and size.
 * @param data Pointer to the output image buffer (must be at least 3*w*h bytes, RGB format).
 * @param w Width of the output image in pixels.
 * @param h Height of the output image in pixels.
 * @param stride_in_bytes Number of bytes per image row.
 * @return int Returns 1 on success, 0 on failure (invalid configuration or buffer too small).
 */
STBHW_EXTERN int stbhw_make_template(stbhw_config *c, unsigned char *data, int w, int h, int stride_in_bytes)
{
   stbhw__process p;
   int i;

   p.data = data;
   p.w = w;
   p.h = h;
   p.stride = stride_in_bytes;
   p.ts = 0;
   p.c = c;

   if (c->is_corner) {
      p.process_h_rect = stbhw__corner_process_h_rect;
      p.process_v_rect = stbhw__corner_process_v_rect;
   } else {
      p.process_h_rect = stbhw__edge_process_h_rect;
      p.process_v_rect = stbhw__edge_process_v_rect;
   }

   for (i=0; i < p.h; ++i)
      memset(p.data + i*p.stride, 255, 3*p.w);

   if (!stbhw__process_template(&p))
      return 0;

   if (c->is_corner) {
      // write out binary information in first line of image
      for (i=0; i < 4; ++i)
         data[w*3-1-i] = c->num_color[i];
      data[w*3-1-i] = c->num_vary_x;
      data[w*3-2-i] = c->num_vary_y;
      data[w*3-3-i] = c->short_side_len;
      data[w*3-4-i] = 0xc0;
   } else {
      for (i=0; i < 6; ++i)
         data[w*3-1-i] = c->num_color[i];
      data[w*3-1-i] = c->num_vary_x;
      data[w*3-2-i] = c->num_vary_y;
      data[w*3-3-i] = c->short_side_len;
   }

   // make it more obvious it encodes actual data
   for (i=0; i < 9; ++i)
      p.data[p.w*3 - 1 - i] ^= i*55;

   return 1;
}
#endif // STB_HBWANG_IMPLEMENTATION
