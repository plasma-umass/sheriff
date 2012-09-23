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

#ifndef SHERIFF_FSHAREINFO_H
#define SHERIFF_FSHAREINFO_H

/*
 * @file   fshareinfo.h   
 * @brief  share information accross different threads.
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */ 

/* NOTES:

The threads-as-processes framework is helpful to improve
the performance for the following cases:
   a. Long transactions.
   b. False sharing inside.

To work as a runtime system, we want to be passive. That is, we only
protect pages when there is some benefit to do so.

1. Since we have to pay some overhead for transaction end and begin,
   which should be avoided as much as possible since the benefit of
   tolerating false sharing can be ''dixiao'' easily by the overhead
   of transaction overhead.

   [EDB: Please translate 'dixiao' FIX ME.]

2. When one page has been protected before, which we can't find some
   benefit, we tend to un-protect this page forever even if there is
   memory re-usage in this page.  So protection can happen only on one
   address once.

3. If we have a chance to detect some callsite which can cause the
   false sharing problem.  We are trying to remember that for the
   future runs.

4. Sometimes we need to reprotect the memory if we find that is useful
   to do so. Thus, we are keeping track of the transaction size.  If
   the transaction length is larger than one interval, we are
   protecting those new memory again. Then we can evaluate performance
   again later.

*/

#include "xdefines.h"
#include "xplock.h"

// In order to check whether there is one page that we need to protect
// quickly, we borrow the multi-level page table idea from linux.  We
// always update multi-level information at the same time.

class fshareinfo {
public:

  fshareinfo()
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
 
  virtual ~fshareinfo() {}
  
  static fshareinfo& getInstance() {
    static void * buf = allocateShared (sizeof(fshareinfo));
    static fshareinfo * theOneTrueObject = new (buf) fshareinfo();
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
		       sizeof(fshareinfo),
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
