// -*- C++ -*-

// This class provides page profiling information.  We will have
// another class to do cacheline profiling.

#ifndef SHERIFF_PAGEPROF_H
#define SHERIFF_PAGEPROF_H

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

#include "mm.h"

#ifdef TRACK_RACE
#include "xtracker.h"
#endif

#include "xdefines.h"

// This class provides the following information:
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
    // Allocate one shared page to store statistics.
    char * start
      = (char *) MM::allocateShared (xdefines::PageSize);

    if (start == MAP_FAILED) {
      fprintf(stderr, "Failed to mmap pages to store page user info %s\n", strerror(errno));
      ::abort();
    }
    
    // EDB FIX ME Isn't this definitely unnecessary?
    memset (start, 0, xdefines::PageSize);
    
    // Initialize the data.
    _commits     = (unsigned long *)start;
    _rollbacks   = (unsigned long *)(start + sizeof(unsigned long)*1);
    _dirtypages  = (unsigned long *)(start + sizeof(unsigned long)*2);
    _shorttrans  = (unsigned long *)(start + sizeof(unsigned long)*3);
    _singlepages = (unsigned long *)(start + sizeof(unsigned long)*4);
    
    // Calculate total pages.
    _totalpages = NElts * sizeof(Type)/xdefines::PageSize;

    _pageInfo = (struct pageUserInfo *)
      MM::allocateShared (_totalpages * sizeof(pageUserInfo));

    if(_pageInfo == MAP_FAILED) {
      fprintf(stderr, "Failed to mmap pages to store page user info %s\n", strerror(errno));
      ::abort();
    }

    // Zero out data. FIX ME EDB See above comment on zeroing.
    memset(_pageInfo, 0, _totalpages * sizeof(pageUserInfo));
  }


  virtual ~xpageprof () {}

  void finalize (char * start) {
    int pages = 0;
    int usages = 0;
    int totalpages = 0;
    double overhead = 0.0;
    double improvement = 0.0;
    int  * stopPos = (int *)start;
    
    fprintf(stderr, "\n\nStatistics DATA:\n");
    fprintf(stderr, "%ld commits, %ld rollbacks, %ld dirtypages, %ld short transactions\n", *_commits, *_rollbacks, *_dirtypages, *_shorttrans);

    // Calculate the pages that are accessed by one process only.
    for (unsigned long i = 0; i < _totalpages; i++) {
      if(_pageInfo[i].numUser != 0 && _pageInfo[i].numUser == 1) {
	pages++;
	usages += _pageInfo[i].usages;
      }
      
      if (_pageInfo[i].usages != 0)
	totalpages++; 
    }
	
    // EDB FIX ME What's with the magic numbers here?
    overhead = (double) *_dirtypages * 20.0 /1000000.00;
    improvement = (double) usages * 20.0 /1000000.00;
    
    fprintf(stderr, "Total pages %ld, single-process pages %ld use %d times, estimated time %f s, room for improvement %f s. Single pages %ld\n", totalpages,  pages,  usages, overhead, improvement, *_singlepages);
  }

private:

  void updateSinglepages (int count) {
    *_singlepages += count;
  }
  
  void updateCommitInfo (int pages) {
    *_commits += 1;
    *_dirtypages += pages;
    //fprintf(stderr, "dirty pages %x\n", *_dirtypages);
    if (pages == 1)
      *_shorttrans += 1;
  }

  void updateRollbackInfo() {
    *_commits += 1;
    *_rollbacks += 1;
  }

  void updatePageInfo (int pageNo) {
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

#endif /* SHERIFF_PAGEPROF_H */
