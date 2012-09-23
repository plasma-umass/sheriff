// -*- C++ -*-

/*
  Copyright (C) 2012 University of Massachusetts Amherst.

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
 * @file   mm.h
 * @brief  Low-level memory management routines.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 */ 

#ifndef SHERIFF_MM_H
#define SHERIFF_MM_H

#include <sys/mman.h>

#include "realfuncs.h"

class MM {
public:

  static void   deallocate (void * ptr, size_t sz) {
    munmap(ptr, sz);
  }

  static void * allocateShared (size_t sz,
				int fd = -1,
				void * startaddr = NULL) 
  {
    return allocate (true, sz, fd, startaddr);
  }

  static void * allocatePrivate (size_t sz,
				 int fd = -1,
				 void * startaddr = NULL) 
  {
    return allocate (false, sz, fd, startaddr);
  }

private:

  static void * allocate (bool isShared,
			  size_t sz,
			  int fd,
			  void * startaddr) 
  {
    int protInfo   = PROT_READ | PROT_WRITE;
    int sharedInfo = isShared ? MAP_SHARED : MAP_PRIVATE;
    sharedInfo     |= ((fd == -1) ? MAP_ANONYMOUS : 0);
    sharedInfo     |= ((startaddr != NULL) ? MAP_FIXED : 0);
    
    return mmap (startaddr,
		 sz,
		 protInfo,
		 sharedInfo,
		 fd,
		 0);
  }

};

#endif

