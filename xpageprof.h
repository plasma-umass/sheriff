// -*- C++ -*-
// This class will provide the page profiling information. 
// We will have another class to do cacheline profiling.
#ifndef _XPAGEPROF_H_
#define _XPAGEPROF_H_

#include <set>

#if !defined(_WIN32)
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef TRACK_RACE
#include "xtracker.h"
#endif

#include "xdefines.h"
// This class try to provide the following information:
// (a) How many pages are written totally --> pages written per transaction.
// (b) Total number of commits.
// (c) Total number of rollbacks.
// (d) How many pages are written by one process only. We need per page user information for this.

template <class Type,
      int NElts = 1>
class xpageprof {
public:

  xpageprof ()
	: _totalpages(0)
  {
    char * start = NULL;
    // Allocate one share page to store statistics
    start = (char *)mmap(
                      NULL,
                      xdefines::PageSize,
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_ANONYMOUS,
                      -1,
                      0);
	  if(start == NULL) {
      fprintf(stderr, "Failed to mmap pages to store page user info %s\n", strerror(errno));
      ::abort();
    }
   
	  memset(start, 0, xdefines::PageSize);
 
    // Initialize those data.
    _commits = (unsigned long *)start;
    _rollbacks = (unsigned long *)(start + sizeof(unsigned long)*1);
    _dirtypages = (unsigned long *)(start + sizeof(unsigned long)*2);
	  _shorttrans = (unsigned long *)(start + sizeof(unsigned long)*3);
	  _singlepages = (unsigned long *)(start + sizeof(unsigned long)*4);
	 
	  // Calculate total pages.
	  _totalpages = NElts * sizeof(Type)/xdefines::PageSize;

    _pageInfo = (struct pageUserInfo *)mmap(
                NULL,
                _totalpages * sizeof(pageUserInfo),
                PROT_READ | PROT_WRITE,
                MAP_SHARED | MAP_ANONYMOUS,
                -1,
                0);
    if(_pageInfo == MAP_FAILED) {
      fprintf(stderr, "Failed to mmap pages to store page user info %s\n", strerror(errno));
      ::abort();
    }

    // Initialize that before using.
    memset(_pageInfo, 0, _totalpages * sizeof(pageUserInfo));
  }

  virtual ~xpageprof (void) {
  }

  void finalize(char * start) {
    int pages = 0;
    int usages = 0;
    int totalpages = 0;
	  double overhead = 0.0;
	  double improvement = 0.0;
    int  * stopPos = (int *)start;
 
    fprintf(stderr, "\n\nStatistics DATA:\n");
	  fprintf(stderr, "%ld commits, %ld rollbacks, %ld dirtypages, %ld short transactions\n", *_commits, *_rollbacks, *_dirtypages, *_shorttrans);
    // Calculate the pages that are accessed by one process only.
    for(unsigned long i = 0; i < _totalpages; i++) {
      if(_pageInfo[i].numUser != 0 && _pageInfo[i].numUser == 1) {
  			pages++;
  			usages += _pageInfo[i].usages;
  		}
      
      if(_pageInfo[i].usages != 0)
			  totalpages++; 
    }
	
    overhead = (double)(*_dirtypages) * 20.0 /1000000.00;
	  improvement = (double)(usages) * 20.0 /1000000.00;

    fprintf(stderr, "Total pages %ld, single-process pages %ld use %d times, estimated time %f s, room for improvement %f s. Single pages %ld\n", totalpages,  pages,  usages, overhead, improvement, *_singlepages);
  }

  void updateSinglepages(int count) {
	  (*_singlepages) += count;
  }

  void updateCommitInfo(int pages) {
    (*_commits) += 1;
	  (*_dirtypages) += pages;
  	//fprintf(stderr, "dirty pages %x\n", *_dirtypages);
  	if(pages == 1)
  	  (*_shorttrans) += 1;
  }

  void updateRollbackInfo(void) {
    (*_commits) += 1;
	  (*_rollbacks) += 1;
  }

  void updatePageInfo(int pageNo) {
    int pid = getpid();
    struct pageUserInfo * pageInfo = &_pageInfo[pageNo];

    if(pid != pageInfo->firstPid) {
      pageInfo->numUser++;
      if(pageInfo->firstPid == 0) {
        pageInfo->firstPid = pid;
	    }
    }
    pageInfo->usages++;
  }

  
private:

  struct pageUserInfo {
    int      firstPid;
    unsigned long numUser;
    unsigned long usages;
  };

  struct pageUserInfo * _pageInfo;
  unsigned long _totalpages;
  unsigned long *_commits;
  unsigned long *_rollbacks;
  unsigned long *_dirtypages;
  unsigned long *_shorttrans;
  unsigned long *_singlepages;  //Used to keep track of how many pages are using by one thread at a time.
#ifdef TRACK_RACE
  xtracker<NElts> _tracker;
#endif
};

#endif /* _XPAGEPROF_H_ */
