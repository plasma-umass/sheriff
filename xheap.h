// -*- C++ -*-

/*
  Copyright (c) 2011, University of Massachusetts Amherst.

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
 * @file   xheap.h
 * @brief  A basic bump pointer heap, used as a source for other heap layers.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */


#ifndef _XHEAP_H_
#define _XHEAP_H_

#include "xpersist.h"
#include "xdefines.h"

template <long Size>
class xheap : public xpersist<char,Size>
{
  typedef xpersist<char,Size> parent;

public:

  // It is very very important to put the first page in a separate area since it will invoke a 
  // lot of un-necessary rollbacks, which can affect the performance greately and also can affect
  // the correctness of race detection, since different threads can end up getting the same memory 
  // block from class "xheap". 
  // Although we can rely on "rollback" mechanism to avoid the problem,
  // but we still need to differentiate those races caused by this and actual races in user application. 
  // It is not good to do this.
  // It is possible that we don't need sanityCheck any more, but we definitely need a lock to avoid the race on "metatdata".
  xheap (void)
  {
	  // Since we don't know whether shareheap has been initialized here, just use mmap to assign
    // one page to hold all data. We are pretty sure that one page is enough to hold all contents.
  	char * base;

	  // Allocate a share page to hold all heap metadata.
    base = (char *)mmap (NULL, xdefines::PageSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	  // Put all "heap metadata" in this page.
    _position   = (char **)base;
    _remaining  = (size_t *)(base + 1 * sizeof(void *));
    _magic      = (size_t *)(base + 2 * sizeof(void *));
	  _lock       = new (base + 3*sizeof(void *)) xplock;
	
	  // Initialize the following content according the values of xpersist class.
    _start      = parent::base();
    _end        = _start + parent::size();
    *_position  = (char *)_start;
    *_remaining = parent::size();
    *_magic     = 0xCAFEBABE;
//    fprintf(stderr, "PROTECTEDHEAP:_start %p end is %p remaining %p-%x, position %p-%x. OFFSET %x\n", _start, _end, _remaining, *_remaining, _position, *_position, (intptr_t)*_position-(intptr_t)_start);
  }

  inline void * getend(void) {
    return  *_position;
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

    return p;
  }

  void initialize(void) {
    //fprintf(stderr, "xheap initialize~~~\n");
    parent::initialize();
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
