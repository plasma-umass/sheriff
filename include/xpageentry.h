// -*- C++ -*-
#ifndef _XPAGEENTRY_H_
#define _XPAGEENTRY_H_

#include <errno.h>

#if !defined(_WIN32)
#include <sys/wait.h>
#include <sys/types.h>
#endif

#include <stdlib.h>

#include "xdefines.h"
#include "xpageinfo.h"


/* This class is used to manage the page entries.
 * Page fault handler will ask for one page entry here.
 * Normally, we will keep 256 pages entries. If the page entry 
 * is not enough, then we can allocate 64 more, but those will be
 * freed in atomicEnd() to avoid unnecessary memory blowup.
 * Since one process will have one own copy of this and it is served
 * for one process only, memory can be allocated from private heap.
 */
class xpageentry {
#if defined(DETECT_FALSE_SHARING) || defined(DETECT_FALSE_SHARING_OPT)
	enum {PAGE_ENTRY_NUM = 100000 };
#else
	enum {PAGE_ENTRY_NUM = 5120 };
#endif
public:
	xpageentry()
	{
		_start = NULL;
		_cur = 0;
        _total = 0;
	}

	static xpageentry& getInstance (void) {
      static char buf[sizeof(xpageentry)];
      static xpageentry * theOneTrueObject = new (buf) xpageentry();
      return *theOneTrueObject;
    }

	void initialize(void) {
		void * start;
        struct pageinfo * cur;
        int i = 0;
        unsigned long pagestart;

		// We don't need to allocate all pages, only the difference between newnum and oldnum.
		start = mmap (NULL,
         			  PAGE_ENTRY_NUM * sizeof(pageinfo),
         			  PROT_READ | PROT_WRITE,
         			  MAP_PRIVATE | MAP_ANONYMOUS,
         			  -1,
         			  0);
         pagestart = (unsigned long)mmap (NULL,
         			 xdefines::PageSize * PAGE_ENTRY_NUM,
         			 PROT_READ | PROT_WRITE,
         			 MAP_PRIVATE | MAP_ANONYMOUS,
         			 -1,
         			 0);

		if(start == NULL || pagestart == 0)  {
			fprintf(stderr, "%d fail to allocate page entries : %s\n", getpid(), strerror(errno));
			::abort();
		}
		
		// start to initialize it.
        cur = (struct pageinfo *)start;
        i = 0;
        while(i < PAGE_ENTRY_NUM) {
            cur->origTwinPage = (void *)(pagestart + i * xdefines::PageSize);
            cur++;
            i++;
        }

		_cur = 0;
		_total = PAGE_ENTRY_NUM;
		_start = (struct pageinfo *)start;
		return;
	}

	struct pageinfo * alloc(void) {
		struct pageinfo * entry = NULL;
		if(_cur < _total) {
			entry = &_start[_cur];
			_cur++;
		}
 		else {
			// There is no enough entry now, re-allocate new entries now.
			fprintf(stderr, "NO enough page entry, now _cur %x, _total %x!!!\n", _cur, _total);
			::abort();
		}
		return entry;
    }

	void cleanup(void) {
		_cur = 0;
	}

private:
	// How many entries in total.
	int _total;

	// Current index of entry that need to be allocated.
	int _cur;
	
	struct pageinfo * _start;
};

#endif
