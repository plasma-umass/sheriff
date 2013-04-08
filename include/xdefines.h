// -*- C++ -*-

/*
 
  Copyright (c) 2007-2012 Emery Berger, University of Massachusetts Amherst.

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

#ifndef SHERIFF_XDEFINES_H
#define SHERIFF_XDEFINES_H

#include <sys/types.h>
#include <syscall.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "realfuncs.h"

/*
 * @file   xdefines.h   
 * @brief  Global definitions for Sheriff-Detect and Sheriff-Protect.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */ 


#ifdef __cplusplus

extern "C"
{

  extern int textStart, textEnd;

  // If one transaction is less than 5ms, then we will close the protection.
  enum { THRESH_TRAN_LENGTH = 5000 };

  enum { CHECK_AGAIN_NO_PROTECTION = 50 };

  enum { CHECK_AGAIN_UNDER_PROTECTION = 1 };

  enum { CALL_SITE_DEPTH = 2 };

#ifdef GET_CHARACTERISTICS
  extern int allocTimes;
  extern int cleanupSize;
#endif

  void * malloc(size_t) throw ();
  void * calloc(size_t, size_t) throw ();
  void free(void *) throw ();
  void * realloc(void *, size_t) throw ();
  void * memalign(size_t, size_t) throw ();

// maybe it is better to differentiate the false sharing type. 
// Two cases: 
// Streamcluster: times of interleaving writes are large and also there are only several times totally.
//                we better provide information for each object. 
// Kmeans: each object is small. Then we need aggregate information in order to make things noticable.
// Reverseindex: different threads are falsely shared the same cache line. But multiple threads can 
//              work on the same address later. We might freeze the information after this false sharing 
//              problem are serious. Thus, this information are correct in some sense.
// Wordcount: only false sharing. That is the same as reverseindex.
// Linear_regression:
// String match: 

  extern unsigned long * global_thread_index;
};

class xdefines {
public:
  enum { STACK_SIZE = 1024 * 1024 };
#ifdef X86_32BIT
  enum { PROTECTEDHEAP_SIZE = 1048576UL * 800 };
#else
  enum { PROTECTEDHEAP_SIZE = 1048576UL * 8192 };
#endif
  enum { SHAREDHEAP_SIZE = 1048576UL * 100 };

#if defined(DETECT_FALSE_SHARING) || defined(DETECT_FALSE_SHARING_OPT)
  enum { PROTECTEDHEAP_CHUNK = 1048576 };
  enum { LARGE_CHUNK = 1024 };
  enum { EVAL_LARGE_HEAP_BASE = 1000 };
  enum { EVAL_LARGE_HEAP_PROTECTION = 1000 }; 
#else
  enum { PROTECTEDHEAP_CHUNK = 40960 };
  enum { SHAREDHEAP_CHUNK = 1048576};
  enum { LARGE_CHUNK = 40960 };
  enum { EVAL_TRANS_PER_THREAD = 2 }; // Evaluate whether we need to separate memory.
  enum { EVAL_TOTAL_TRANS = 8 };
  enum { EVAL_LEAST_INVALIDATES = 1000 };
#endif

  enum { EVAL_CHECKING_PERIOD = 20 };
  enum { MAX_GLOBALS_SIZE = 1048576UL * 20 };
  enum { INTERNALHEAP_SIZE = 1048576UL * 20 };
  enum { PageSize = 4096UL };
  enum { PAGE_SIZE_MASK = (PageSize-1) };
  enum { NUM_HEAPS = 32 };
  //enum { PERIODIC_CHECKING_INTERVAL = 10000};
  enum { PERIODIC_CHECKING_INTERVAL = 1000};
  enum { CACHE_LINE_SIZE = 64};
  enum { CACHES_PER_PAGE = 64};
  enum { CACHELINE_SIZE_MASK = 0x3F};
  enum { MIN_PAGE_INTERWRITES_CARE = 20};
  enum { MIN_INTERWRITES_OUTPUT = 2};
  enum { MIN_INTERWRITES_CARE = 10};
  enum { MIN_CONWRITES_CARE = 5};
  enum { MIN_INVALIDATES_CARE = MIN_INTERWRITES_CARE};
  enum { MIN_WRITES_CARE = 100000};
  enum { CPU_CORES = 8 };
};

#endif

#endif
