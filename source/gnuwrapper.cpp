/*
  Copyright (C) 2011 University of Massachusetts Amherst.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

/*
 * @file   Gnuwrapper.cpp  
 * @brief
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 */ 
    

#ifndef __GNUC__
#error "This file requires the GNU compiler."
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>


#ifndef CUSTOM_PREFIX
#define CUSTOM_PREFIX
#endif

#define CUSTOM_MALLOC(x)     CUSTOM_PREFIX(malloc)(x)
#define CUSTOM_FREE(x)       CUSTOM_PREFIX(free)(x)
#define CUSTOM_REALLOC(x,y)  CUSTOM_PREFIX(realloc)(x,y)
#define CUSTOM_MEMALIGN(x,y) CUSTOM_PREFIX(memalign)(x,y)


extern "C" {

  void * CUSTOM_MALLOC(size_t);
  void * CUSTOM_CALLOC(size_t, size_t);
  void CUSTOM_FREE(void *);
  void * CUSTOM_REALLOC(void *, size_t);
  void * CUSTOM_MEMALIGN(size_t, size_t);

  static void my_init_hook (void);

  // New hooks for allocation functions.
  static void * my_malloc_hook (size_t, const void *);
  static void my_free_hook (void *, const void *);
  static void * my_realloc_hook (void *, size_t, const void *);
  static void * my_memalign_hook (size_t, size_t, const void *);

  // Store the old hooks just in case.
  static void * (*old_malloc_hook) (size_t, const void *);
  static void (*old_free_hook) (void *, const void *);
  static void *(*old_realloc_hook)(void *ptr, size_t size, const void *caller);
  static void *(*old_memalign_hook)(size_t alignment, size_t size, const void *caller);

#ifndef __MALLOC_HOOK_VOLATILE
#define __MALLOC_HOOK_VOLATILE
#endif

  void (* __MALLOC_HOOK_VOLATILE __malloc_initialize_hook) (void) = my_init_hook;

  static void my_init_hook (void) {
    // Store the old hooks.
    old_malloc_hook = __malloc_hook;
    old_free_hook = __free_hook;
    old_realloc_hook = __realloc_hook;
    old_memalign_hook = __memalign_hook;

    // Point the hooks to the replacement functions.
    __malloc_hook = my_malloc_hook;
    __free_hook = my_free_hook;
    __realloc_hook = my_realloc_hook;
    __memalign_hook = my_memalign_hook;

  }

  static void * my_malloc_hook (size_t size, const void *) {
    void * result = CUSTOM_MALLOC(size);
    return result;
  }

  static void my_free_hook (void * ptr, const void *) {
    CUSTOM_FREE(ptr);
  }

  static void * my_realloc_hook (void * ptr, size_t size, const void *) {
    return CUSTOM_REALLOC(ptr, size);
  }

  static void * my_memalign_hook (size_t size, size_t alignment, const void *) {
    return CUSTOM_MEMALIGN(size, alignment);
  }

#if 0
  void finalizer (void) __attribute__((destructor));

  void finalizer (void) {
    printf ("counter = %d\n", counter);
  }
#endif

}

