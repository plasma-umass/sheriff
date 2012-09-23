// -*- C++ -*-

/*
  Author: Emery Berger, http://www.cs.umass.edu/~emery
 
  Copyright (c) 2007-8 Emery Berger, University of Massachusetts Amherst.

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

#ifndef _XONEHEAP_H_
#define _XONEHEAP_H_

/**
 * @class xoneheap
 * @brief Wraps a single heap instance.
 *
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 */

template <class SourceHeap>
class xoneheap {
public:
  xoneheap() {
    //fprintf(stderr, "xoneheap constructor.\n");
  }

  void initialize() { getHeap()->initialize(); }
  void finalize (void * end) { getHeap()->finalize(end); }
  void begin() { getHeap()->begin(); }
  void commit (bool doChecking) { getHeap()->commit(doChecking); }
  void cleanup() { getHeap()->cleanup(); }
  void setHeapId (int index) { return getHeap()->setHeapId(index); }

  void * getend() { return getHeap()->getend(); }

  void stats () { getHeap()->stats(); }

  void openProtection() { getHeap()->openProtection(); }
  void closeProtection() { getHeap()->closeProtection(); }
  void setProtectionPeriod() { getHeap()->setProtectionPeriod(); }
  void unprotectNonProfitPages (void *end) { getHeap()->unprotectNonProfitPages(end); }
   
  int getDirtyPages() { return getHeap()->getDirtyPages(); }
 
  bool nop() { return getHeap()->nop(); }
 
  bool inRange (void * ptr) { return getHeap()->inRange(ptr); }
  void handleWrite (void * ptr) { getHeap()->handleWrite(ptr); }
  void periodicCheck() { getHeap()->periodicCheck( ); }

  void * malloc (size_t sz) { return getHeap()->malloc(sz); }
  void free (void * ptr) { getHeap()->free(ptr); }
  size_t getSize (void * ptr) { return getHeap()->getSize(ptr); }

  void sharemem_write_word(void * dest, unsigned long val) {
    getHeap()->sharemem_write_word(dest, val);
  }

  unsigned sharemem_read_word(void * dest) {
    return getHeap()->sharemem_read_word(dest);
  }

private:

  SourceHeap * getHeap (void) {
    static char heapbuf[sizeof(SourceHeap)];
    static SourceHeap * _heap = new (heapbuf) SourceHeap;
    //fprintf (stderr, "heap is in %p\n", _heap);
    return _heap;
  }

};


#endif // _XONEHEAP_H_
