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
 * @file   sourceshareheap.h
 * @brief  It is the source of internalheap. Basically, it is almost the same as 
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */


#ifndef _XSOURCESHAREHEAP_H_
#define _XSOURCESHAREHEAP_H_

#include "xdefines.h"
#include "xplock.h"

template <int Size>
class SourceShareHeap
{
public:

  SourceShareHeap (void)
  {
	char * base;

	// Call mmap to allocate share map.
    base = (char *)mmap (NULL, xdefines::PageSize+Size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	// Put all "heap metadata" in this page.
    _position   = (char **)base;
    _remaining  = (size_t *)(base + 1 * sizeof(void *));
    _magic      = (size_t *)(base + 2 * sizeof(void *));
  	_lock       = new (base + 3*sizeof(void *)) xplock;
	
	// Initialize the following content according the values of xpersist class.
    _start      = base + xdefines::PageSize;
    _end        = _start + Size;
    *_position  = (char *)_start;
    *_remaining = Size;
    *_magic     = 0xCAFEBABE;
//    fprintf(stderr, "SHAREHEAP:_start %p end is %p remaining %p-%x, position %p-%x. OFFSET %x\n", _start, _end, _remaining, *_remaining, _position, *_position, (int)*_position-(int)_start);
  }

  // We need to page-aligned size, we don't need two different threads are using the same page here.
  inline void * malloc (size_t sz) {
    sanityCheck();

    // Roud up the size to page aligned.
	sz = xdefines::PageSize * ((sz + xdefines::PageSize - 1) / xdefines::PageSize);

	_lock->lock();

    //fprintf (stderr, "%d : xheap malloc size %x, remainning %p-%x and position %p-%x: OFFSET %x\n", getpid(), sz, _remaining, *_remaining, _position, *_position, (int)*_position-(int)_start);
    if (*_remaining == 0) { 
      fprintf (stderr, "FOOP: something very bad has happened: _start = %x, _position = %x, _remaining = %x.\n", *_start, *_position, *_remaining);
    }
   
    if (*_remaining < sz) {
      fprintf (stderr, "CRAP: remaining[%x], sz[%x] thread[%d]\n", *_remaining, sz, pthread_self());
	  exit(-1);
    }
    void * p = *_position;


    // Increment the bump pointer and drop the amount of memory.
    *_remaining -= sz;
    *_position += sz;

	_lock->unlock();

    //fprintf (stderr, "%d : shareheapmalloc %p with size %x, remaining %x\n", getpid(), p, sz, *_remaining);
    return p;
  }

  // These should never be used.
  inline void free (void * ptr) { sanityCheck(); }
  inline size_t getSize (void * ptr) { sanityCheck(); return 0; } // FIXME

private:

  void sanityCheck (void) {
    if (*_magic != 0xCAFEBABE) {
      fprintf (stderr, "%d : WTF!\n", getpid());
      ::abort();
    }
  }

  /// The start of the heap area.
  volatile char *  _start;

  /// The end of the heap area.
  volatile char *  _end;

  /// Pointer to the current bump pointer.
  char **  _position;

  /// Pointer to the amount of memory remaining.
  size_t*  _remaining;

  size_t*  _magic;

  // We will use a lock to protect the allocation request from different threads.
  xplock* _lock;

};

#endif
