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
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */


#ifndef SHERIFF_SOURCESHAREDHEAP_H
#define SHERIFF_SOURCESHAREDHEAP_H

#include "xdefines.h"
#include "xplock.h"

template <unsigned long Size>
class SourceSharedHeap
{
public:

  SourceSharedHeap (void)
  {
    char * base;
    
    // Call mmap to allocate a shared map.
    base = (char *)mmap (NULL, xdefines::PageSize+Size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    // Put all heap metadata on this page.
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
  }

  inline void * malloc (size_t sz) {
    sanityCheck();

    // We need to page-align size, since we don't want two different
    // threads using the same page.

    // Round up the size to page aligned.
    sz = xdefines::PageSize * ((sz + xdefines::PageSize - 1) / xdefines::PageSize);
    
    _lock->lock();
    
    if (*_remaining < sz) {
      fprintf (stderr, "Out of memory error: available = %u, requested = %u, thread = %d.\n",
       *_remaining, sz, (int) pthread_self());
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
