// -*- C++ -*-
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
 * @file   xheapcleanup.h 
 * @brief  Cleanup heap object information when heap object is re-used by other threads.
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */ 

#ifndef _XHEAPCLEANUP_H_
#define _XHEAPCLEANUP_H_

#include <errno.h>

#if !defined(_WIN32)
#include <sys/wait.h>
#include <sys/types.h>
#endif

#include <stdlib.h>
/* This class is used to manage the page entries.
 * Page fault handler will ask for one page entry here.
 * Normally, we will keep 256 pages entries. If the page entry 
 * is not enough, then we can allocate 64 more, but those will be
 * freed in atomicEnd() to avoid unnecessary memory blowup.
 * Since one process will have one own copy of this and it is served
 * for one process only, memory can be allocated from private heap.
 */
class xheapcleanup {
public:

  xheapcleanup() {
	}

	static xheapcleanup& getInstance (void) {
    static char buf[sizeof(xheapcleanup)];
    static xheapcleanup * theOneTrueObject = new (buf) xheapcleanup();
    return *theOneTrueObject;
  }

	void storeProtectHeapInfo(void * start, int size, void * cacheInvalidate, void * cacheLastWriter, void * wordChange) {
		_heapStart = start;
		_heapSize = size;
		_cacheInvalidates = (unsigned long *)cacheInvalidate;
		_cacheLastThread = (unsigned long *)cacheLastWriter;
		_wordChanges = (unsigned long *)wordChange;
	}


	// Cleanup those counter information of one heap object.
  bool cleanupHeapObject(void * ptr, size_t sz, bool sameCallsite) {
    long offset;
    int cachelines;
    int index;

    offset = (intptr_t)ptr - (intptr_t)base();
    index = offset/xdefines::CACHE_LINE_SIZE;

    //fprintf(stderr, "cleanupheap ptr %p with sameCallsite %d\n", ptr, sameCallsite);
    // At least we will check one cache line.
    cachelines = sz/xdefines::CACHE_LINE_SIZE;
    if(cachelines == 0)
      cachelines = 1;

    // Cleanup the cacheinvalidates that are involved in this object.
    if(!sameCallsite) {
      // If we are allocate on a new project, if the existing object has some 
      // interleaving writes, then we must choose a different object. 
      for(int i = index; i < index+cachelines; i++) {
        if(_cacheInvalidates[i] >= xdefines::MIN_INVALIDATES_CARE) {
          return false;
        }
        // We don't need atomic operation here.
        _cacheInvalidates[i] = 0;
      }
    
      // Cleanup the wordChanges 
      void * wordptr = (void *)&_wordChanges[(offset-sizeof(objectHeader))/sizeof(unsigned long)];
      memset(wordptr, 0, sz);
    }
    else {
//    fprintf(stderr, "now cleanupheap ptr %p with sameCallsite %d\n", ptr, sameCallsite);
      // If we reuse a existing callsite, then we must cleanup the last thread.
      // Otherwise, it will introduce an invalid interleaving since it is possible
      // that a new thread is working on the same object.
      for(int i = index; i < index+cachelines; i++) {
        if(_cacheInvalidates[i] >= xdefines::MIN_INVALIDATES_CARE) {
          // We don't need to calculate the first interleaving since it is an
          // unavoidable update. 
 //         fprintf(stderr, "cacheline %d's invalidates %d lastthread %d\n", i, _cacheInvalidates[i], _cacheLastThread[i]);
          _cacheInvalidates[i] -= 1;
        }
        // We are changing to 0, since it is a new object.
        _cacheLastThread[i] = 0;
        // We don't need atomic operation here.
      } 
    }
	  return true;
  }
  	
	inline bool inRange (void * addr) {
    if (((size_t) addr >= (size_t) base())
    && ((size_t) addr < (size_t) base() + size())) {
      return true;
    } else {
      return false;
    }
  }


  /// @return the start of the memory region being managed.
  inline void * base (void) const {
    return _heapStart;
  }
	
	inline int size(void) const{
	  return _heapSize;
	}

private:
	void * _heapStart;
	int    _heapSize;
	unsigned long * _cacheInvalidates;
	unsigned long * _cacheLastThread;
	unsigned long * _wordChanges;
};

#endif
