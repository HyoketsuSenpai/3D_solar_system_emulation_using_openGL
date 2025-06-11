// stb_leakcheck.h - v0.6 - quick & dirty malloc leak-checking - public domain
// LICENSE
//
//   See end of file.

#ifdef STB_LEAKCHECK_IMPLEMENTATION
#undef STB_LEAKCHECK_IMPLEMENTATION // don't implement more than once

// if we've already included leakcheck before, undefine the macros
#ifdef malloc
#undef malloc
#undef free
#undef realloc
#endif

#ifndef STB_LEAKCHECK_OUTPUT_PIPE
#define STB_LEAKCHECK_OUTPUT_PIPE stdout
#endif

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
typedef struct malloc_info stb_leakcheck_malloc_info;

struct malloc_info
{
   const char *file;
   int line;
   size_t size;
   stb_leakcheck_malloc_info *next,*prev;
};

static stb_leakcheck_malloc_info *mi_head;

/**
 * @brief Allocates memory and tracks allocation metadata for leak detection.
 *
 * Allocates a memory block of the specified size and records the source file and line number where the allocation occurred. The allocation is tracked internally for later leak reporting.
 *
 * @param sz Number of bytes to allocate.
 * @param file Source file name where the allocation is made.
 * @param line Line number in the source file where the allocation is made.
 * @return Pointer to the allocated memory, or NULL if allocation fails.
 */
void *stb_leakcheck_malloc(size_t sz, const char *file, int line)
{
   stb_leakcheck_malloc_info *mi = (stb_leakcheck_malloc_info *) malloc(sz + sizeof(*mi));
   if (mi == NULL) return mi;
   mi->file = file;
   mi->line = line;
   mi->next = mi_head;
   if (mi_head)
      mi->next->prev = mi;
   mi->prev = NULL;
   mi->size = (int) sz;
   mi_head = mi;
   return mi+1;
}

/**
 * @brief Frees memory previously allocated with the leak-checking allocator.
 *
 * Marks the memory block as freed and removes its metadata from the allocation tracking list, unless verbose reporting is enabled. Does nothing if the pointer is NULL.
 */
void stb_leakcheck_free(void *ptr)
{
   if (ptr != NULL) {
      stb_leakcheck_malloc_info *mi = (stb_leakcheck_malloc_info *) ptr - 1;
      mi->size = ~mi->size;
      #ifndef STB_LEAKCHECK_SHOWALL
      if (mi->prev == NULL) {
         assert(mi_head == mi);
         mi_head = mi->next;
      } else
         mi->prev->next = mi->next;
      if (mi->next)
         mi->next->prev = mi->prev;
      free(mi);
      #endif
   }
}

/**
 * @brief Resizes a tracked memory allocation, preserving leak tracking metadata.
 *
 * Behaves like standard `realloc`, but tracks allocation metadata for leak detection.
 * If `ptr` is `NULL`, allocates a new block. If `sz` is zero, frees the memory and returns `NULL`.
 * If the new size is less than or equal to the current allocation, returns the original pointer.
 * Otherwise, allocates a new block, copies the old data, frees the old block, and returns the new pointer.
 *
 * @return Pointer to the resized memory block, or `NULL` if allocation fails or size is zero.
 */
void *stb_leakcheck_realloc(void *ptr, size_t sz, const char *file, int line)
{
   if (ptr == NULL) {
      return stb_leakcheck_malloc(sz, file, line);
   } else if (sz == 0) {
      stb_leakcheck_free(ptr);
      return NULL;
   } else {
      stb_leakcheck_malloc_info *mi = (stb_leakcheck_malloc_info *) ptr - 1;
      if (sz <= mi->size)
         return ptr;
      else {
         #ifdef STB_LEAKCHECK_REALLOC_PRESERVE_MALLOC_FILELINE
         void *q = stb_leakcheck_malloc(sz, mi->file, mi->line);
         #else
         void *q = stb_leakcheck_malloc(sz, file, line);
         #endif
         if (q) {
            memcpy(q, ptr, mi->size);
            stb_leakcheck_free(ptr);
         }
         return q;
      }
   }
}

/**
 * @brief Prints information about a memory allocation or deallocation event.
 *
 * Outputs the reason, source file, line number, allocation size, and memory address for a given allocation record to the configured output stream.
 *
 * @param reason Description of the event (e.g., "LEAKED" or "FREED").
 * @param mi Pointer to the allocation metadata structure.
 */
static void stblkck_internal_print(const char *reason, stb_leakcheck_malloc_info *mi)
{
#if defined(_MSC_VER) && _MSC_VER < 1900 // 1900=VS 2015
   // Compilers that use the old MS C runtime library don't have %zd
   // and the older ones don't even have %lld either... however, the old compilers
   // without "long long" don't support 64-bit targets either, so here's the
   // compromise:
   #if _MSC_VER < 1400 // before VS 2005
      fprintf(STB_LEAKCHECK_OUTPUT_PIPE, "%s: %s (%4d): %8d bytes at %p\n", reason, mi->file, mi->line, (int)mi->size, (void*)(mi+1));
   #else
      fprintf(STB_LEAKCHECK_OUTPUT_PIPE, "%s: %s (%4d): %16lld bytes at %p\n", reason, mi->file, mi->line, (long long)mi->size, (void*)(mi+1));
   #endif
#else
   // Assume we have %zd on other targets.
   #ifdef __MINGW32__
      __mingw_fprintf(STB_LEAKCHECK_OUTPUT_PIPE, "%s: %s (%4d): %zd bytes at %p\n", reason, mi->file, mi->line, mi->size, (void*)(mi+1));
   #else
      fprintf(STB_LEAKCHECK_OUTPUT_PIPE, "%s: %s (%4d): %zd bytes at %p\n", reason, mi->file, mi->line, mi->size, (void*)(mi+1));
   #endif
#endif
}

/**
 * @brief Reports all currently tracked memory allocations and leaks.
 *
 * Prints information about all memory blocks that have not been freed. If `STB_LEAKCHECK_SHOWALL` is defined, also prints information about blocks that have been freed. Output includes the allocation's file, line, size, and pointer address.
 */
void stb_leakcheck_dumpmem(void)
{
   stb_leakcheck_malloc_info *mi = mi_head;
   while (mi) {
      if ((ptrdiff_t) mi->size >= 0)
         stblkck_internal_print("LEAKED", mi);
      mi = mi->next;
   }
   #ifdef STB_LEAKCHECK_SHOWALL
   mi = mi_head;
   while (mi) {
      if ((ptrdiff_t) mi->size < 0)
         stblkck_internal_print("FREED ", mi);
      mi = mi->next;
   }
   #endif
}
#endif // STB_LEAKCHECK_IMPLEMENTATION

#if !defined(INCLUDE_STB_LEAKCHECK_H) || !defined(malloc)
#define INCLUDE_STB_LEAKCHECK_H

#include <stdlib.h> // we want to define the macros *after* stdlib to avoid a slew of errors

#define malloc(sz)    stb_leakcheck_malloc(sz, __FILE__, __LINE__)
#define free(p)       stb_leakcheck_free(p)
#define realloc(p,sz) stb_leakcheck_realloc(p,sz, __FILE__, __LINE__)

extern void * stb_leakcheck_malloc(size_t sz, const char *file, int line);
extern void * stb_leakcheck_realloc(void *ptr, size_t sz, const char *file, int line);
extern void   stb_leakcheck_free(void *ptr);
extern void   stb_leakcheck_dumpmem(void);

#endif // INCLUDE_STB_LEAKCHECK_H


/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2017 Sean Barrett
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
