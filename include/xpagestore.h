// -*- C++ -*-

#ifndef SHERIFF_PAGESTORE_H
#define SHERIFF_PAGESTORE_H

#include <errno.h>

#if !defined(_WIN32)
#include <sys/wait.h>
#include <sys/types.h>
#endif

#include <stdlib.h>

#include "xplock.h"
#include "xdefines.h"


class xpagestore {

  // If we are used this to detect false sharing, since there is much
  // consumption about memory usage. Then there is a big problem since
  // kernel can't reserve the memory usage for stack. Then the memory
  // returned by this may overlapped with the stack memory. Then there
  // are a lot of strange problems.
  
  enum { INITIAL_PAGESTORE_PAGES = 20000 };

public:
  xpagestore()
    : _cur (0),
      _start (NULL)
  { }

  static xpagestore& getInstance (void) {
    static char buf[sizeof(xpagestore)];
    static xpagestore * theOneTrueObject = new (buf) xpagestore();
    return *theOneTrueObject;
  }

  void initialize(void) {
    _start = mmap (NULL,
		   xdefines::PageSize * INITIAL_PAGESTORE_PAGES,
		   PROT_READ | PROT_WRITE,
		   MAP_PRIVATE | MAP_ANONYMOUS,
		   -1,
		   0);
    
    if (_start == MAP_FAILED)  {
      fprintf(stderr, "%d failed to initialize page store: %s\n", getpid(), strerror(errno));
      ::abort();
    }
    
    _cur = 0;
    _total = INITIAL_PAGESTORE_PAGES;
  }

  void * alloc() {
    void * pageStart;
    
    if (_cur < _total) {
      pageStart = (void *)((intptr_t)_start + _cur * xdefines::PageSize);
      _cur++;
    } else {
      // There are not enough entries now; re-allocate new entries now.

      // EDB: This seems broken. FIX ME.
      fprintf (stderr, "%d: NOT enough pages in page store, _cur %x _total %x!!!\n", getpid(), _cur, _total);
    }
    return pageStart;
  }

  void cleanup() {
    //fprintf(stderr, "%d : cleaning up _cur\n", getpid());
    _cur = 0;
  }
  
private:

  // Current index of entry that need to be allocated.
  int _cur;

  int _total;
  void *_start;
};

#endif
