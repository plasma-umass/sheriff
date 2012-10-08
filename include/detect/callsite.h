// -*- C++ -*-

/*
  Copyright (C) 2011 University of Massachusetts Amherst.

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
 * @file   callsite.h
 * @brief  Management about callsite for heap objects.
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */ 
    
#ifndef SHERIFF_CALLSITE_H
#define SHERIFF_CALLSITE_H

#include <link.h>
#include <stdio.h>
#include <cassert>
#include <setjmp.h>
#include <execinfo.h>

class CallSite {
public:
  CallSite() {
    for (int i = 0; i < CALL_SITE_DEPTH; i++) {
      _callsite[i] = 0;
    }
  }

  unsigned long getItem(unsigned int index)
  {
    assert (index < CALL_SITE_DEPTH);
    return _callsite[index];
  }

  unsigned long getDepth() 
  {
    return CALL_SITE_DEPTH;
  }

  void print()
  {
    printf("CALL SITE: ");
    for(int i = 0; i < CALL_SITE_DEPTH; i++) {
      printf("%lx\t", _callsite[i]);
    }
    printf("\n");
  }

#if 0
  unsigned short fetch(int skip) {
    void * length[10];
    
    //    fprintf(stderr, "fetch : before backtrace total %d\n", skip+CALL_SITE_DEPTH);
    int frames = backtrace((void **)&length[0], skip+CALL_SITE_DEPTH);
    //    fprintf(stderr, "fetch : after backtrace with frames %d\n", frames);

    int minframes = (skip+CALL_SITE_DEPTH) > frames ? frames : (skip+CALL_SITE_DEPTH);

#if 0
    int i;
    for(i = skip; i < minframes; i++) {
      _callsite[i-skip] = (unsigned long)length[i];
      fprintf(stderr, "%d: %p at callsite %p\n", i, length[i], &_callsite[i]);
    }
#endif
  }
  
#else
  // Check and store callsite
  inline void storeCallsite(unsigned long tmp, unsigned long * frames) {
    //fprintf(stderr, "try to store callsite with tmp %lx and frames %d\n", tmp, *frames);
    if ((tmp > textStart) && (tmp < textEnd)) {
      _callsite[*frames] = tmp-5;
      //fprintf(stderr, "Saving callsite %p\n", _callsite[*frames]);
      (*frames)++;
    }
    else if((*frames) >= 1) {
      // We should stop getting callsites if we already jump out of text segment. 
      // Otherwise, some application will crash, fixed by Tongping Liu (09/28/1012).
      (*frames) = CALL_SITE_DEPTH;
    }
  }

  /// The following function is borrowed from plug project that is written by Gene Nowark.
  unsigned short fetch(int skip) 
  {  
    sigjmp_buf __backtrace_jump;
#ifdef X86_32BIT 
#define RA(a)								\
    __builtin_frame_address(a) ? /*fprintf(stderr,"%d: 0x%x\n", a, __builtin_return_address(a)),*/ (unsigned long)__builtin_return_address(a) : 0UL;
#else 
    // On 64bit machine, we can use -O0 for application and sheriff, 
    // but some library (glibc) may use other different optimization level, 
    // which makes it unable to get callsite. 
    // For example, __bultin_frame_adress(a) can return "5", in this case,
    // we have to stop to get callsite information otherwise the program can crash.
#define RA(a)								\
    ((unsigned long)__builtin_frame_address(a) > 0x7fffff000 && (unsigned long)__builtin_frame_address(a) < 0x7fffffffffff)  ? /*fprintf(stderr,"%d: 0x%x\n", a, __builtin_return_address(a)),*/ (unsigned long)__builtin_return_address(a) : 0UL;
#endif 


#define STORE_CALLSITE_AND_CHECK_EXIT		\
    if(tmp == 0) break;				\
    storeCallsite(tmp, &frames);		\
    if(frames == CALL_SITE_DEPTH) break;  

    unsigned long tmp;
    unsigned long frames = 0;
	
    // setjmp should be OK here because SEGV can't be masked.
    // And sigsetjmp requires a kernel crossing and is fscking expensive in any case.
    if(setjmp(__backtrace_jump) == 0) {
    
      switch(skip) {
      case 1:
	tmp = RA(1);
	STORE_CALLSITE_AND_CHECK_EXIT;
      case 2:
	tmp = RA(2);
	STORE_CALLSITE_AND_CHECK_EXIT;
      case 3:
	tmp = RA(3);
	STORE_CALLSITE_AND_CHECK_EXIT;
      case 4:
	tmp = RA(4);
	STORE_CALLSITE_AND_CHECK_EXIT;
      case 5:
	tmp = RA(5);
	STORE_CALLSITE_AND_CHECK_EXIT;
      case 6:
	tmp = RA(6);
	STORE_CALLSITE_AND_CHECK_EXIT;
      case 7:
	tmp = RA(7);
	STORE_CALLSITE_AND_CHECK_EXIT;
      case 8:
	tmp = RA(8);
	STORE_CALLSITE_AND_CHECK_EXIT;
      default:
	break;
      }
    }
      
    //fprintf(stderr, "FETCH: skip %d now exit\n", skip);

    return frames;
  }
#endif

  unsigned long _callsite[CALL_SITE_DEPTH];
};

#endif
