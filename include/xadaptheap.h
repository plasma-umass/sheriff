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

#ifndef _XADAPTHEAP_H_
#define _XADAPTHEAP_H_

/**
 * @class xadaptheap
 * @brief Manages a heap whose metadata is allocated from a given source.
 *
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */

template <template <class S, int Size> class Heap,
	  class Source, int ChunkSize>
class xadaptheap : public Source {
public:

  xadaptheap (void)
  {
	  int  metasize = sizeof(Heap<Source, ChunkSize>);

#if 0
    void * buf = Source::malloc (metasize);
    _heap = new (buf) Heap<Source, ChunkSize>;
#else
	  char * base;

    // Allocate a share page to hold all heap metadata.
    base = (char *)mmap (NULL, metasize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	
	  if(base == NULL) {
		  fprintf(stderr, "Failed to allocate the memory to hold the heap metadata.");
		  exit(0);
	  }
	  _heap = new (base) Heap<Source, ChunkSize>;
#endif

  }

  virtual ~xadaptheap (void) {
    Source::free (_heap);
  }

  void * malloc (int heapid, size_t sz) {
	//fprintf(stderr, "%d: malloc in xadapteheap using heapid %d _heapid %d, size %d\n", getpid(), heapid, _heapid, sz);
    return _heap->malloc (heapid, sz);
  }

  void free (int heapid, void * ptr) {
	//fprintf(stderr, "%d: free in xadapteheap heapid %d _heapid %d, ptr %d\n", getpid(), heapid, _heapid, ptr);
    _heap->free (heapid, ptr);
  }

  size_t getSize (void * ptr) {
    return _heap->getSize (ptr);
  }

  void * nextPage(void) {
	return _heap->nextPage(); 
  }

  void setHeapId(int index) {
    _heapid = index;
  }

private:

  Heap<Source, ChunkSize> * _heap;
  int _heapid;
};


#endif // _XADAPTHEAP_H_

