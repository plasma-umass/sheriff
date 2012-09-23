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

#ifndef SHERIFF_STATS_H
#define SHERIFF_STATS_H

/*
 * @file   stats.h   
 * @brief  statistics information, shared across multiple threads.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */ 

#include "xdefines.h"
#include "xplock.h"

class stats {
public:

  stats()
  {
    // Allocate a shared page to hold all heap metadata.
    char * base = (char *) allocateShared (xdefines::PageSize);
    _trans       = (int *)base;
    _interwrites = (int *)(base + 1 * sizeof(int));
    _events      = (int *)(base + 2 * sizeof(int));
    _pages       = (int *)(base + 3 * sizeof(int));
    _caches      = (int *)(base + 4 * sizeof(int));
    _prots       = (int *)(base + 5 * sizeof(int));
  
    // EDB NOTE: In theory, this is unnecessary, since these pages should
    // be demand-zero.

    *_trans = 0;
    *_interwrites = 0;  
    *_events = 0;
    *_pages = 0;
    *_caches = 0;
    *_prots = 0;
  }
 
  virtual ~stats() {}
  
  static stats& getInstance() {
    static void * buf = allocateShared (sizeof(stats));
    static stats * theOneTrueObject = new (buf) stats();
    return *theOneTrueObject;
  }

  // This function will be called after one page's checking has been
  // finished.  So it is possible that we will have multiple
  // interleaving invalidates in one page.
  void updateInvalidates(void * addr, int num) {
    atomic::add(num, (volatile unsigned long *)_interwrites);
  }

  int getTrans() {
    return *_trans;
  }

  int updateTrans() {
    return atomic::increment_and_return((volatile unsigned long *)_trans);
  }

  int updateEvents() {
    return atomic::increment_and_return((volatile unsigned long *)_events);
  }

  int updateCaches() {
    return atomic::increment_and_return((volatile unsigned long *)_caches);
  }

 int updateDirtyPage() {
    return atomic::increment_and_return((volatile unsigned long *)_pages);
  }

  int updateProtects() {
    return atomic::increment_and_return((volatile unsigned long *)_prots);
  }

  int getCaches() {
    return *_caches;
  }

  int getProtects() {
    return *_prots;
  }

  int getDirtyPages() {
    return *_pages;
  }

private:

  static void * allocateShared (size_t sz) {
    return WRAP(mmap) (NULL,
		       sizeof(stats),
		       PROT_READ | PROT_WRITE, 
		       MAP_SHARED | MAP_ANONYMOUS,
		       -1,
		       0);
  }

  int * _trans;
  int * _interwrites;
  int * _events;
  int * _pages;
  int * _prots;
  int * _caches;
};

#endif
