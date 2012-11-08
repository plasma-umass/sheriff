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
 * @file   xmemory.h
 * @brief  Memory management for all.
 *         This file only includes a simplified logic to detect false sharing problems.
 *         It is slower but more effective.
 * 
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */ 

#ifndef SHERIFF_XMEMORY_H
#define SHERIFF_XMEMORY_H

#include <signal.h>

#if !defined(_WIN32)
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include <set>

#include "xglobals.h"
#include "xrun.h"

// Heap Layers
#include "privateheap.h"
#include "stlallocator.h"
#include "warpheap.h"

#include "sourcesharedheap.h"
#include "internalheap.h"
#include "xoneheap.h"
#include "xheap.h"

#include "xplock.h"
#include "xpageentry.h"
#include "xpagestore.h"
#include "objectheader.h"
#include "xheapcleanup.h"

#include "stats.h"
#include "finetime.h"

class xmemory {
private:

  // Private on purpose. See getInstance(), below.
  xmemory() 
   : _internalheap (InternalHeap::getInstance())
  {
  }

public:

  // Just one accessor.  Why? We don't want more than one (singleton)
  // and we want access to it neatly encapsulated here, for use by the
  // signal handler.
  static xmemory& getInstance() {
    static char buf[sizeof(xmemory)];
    static xmemory * theOneTrueObject = new (buf) xmemory();
    return *theOneTrueObject;
  }

  void initialize() {
    // Intercept SEGV signals (used for trapping initial reads and
    // writes to pages).
    installSignalHandler();
    _heap.initialize();
    _heap.setHeapId(0);
    _globals.initialize();
    xpageentry::getInstance().initialize();
    xpagestore::getInstance().initialize();
  
    // In the beginning, we will protect. 
    _protection = true;
  }

  void setMainId (int tid) {
  }

  void finalize() {
    _globals.finalize(NULL);
    _heap.finalize (_heap.getend());
  }


  inline void *malloc (size_t sz, bool isProtected) {
    void * ptr = NULL;
    bool   checkCallsite = false;

Remalloc_again:
    ptr = _heap.malloc(_heapid, sz);
  
    // Get callsite information.
    CallSite callsite;
    objectHeader * obj = getObjectHeader(ptr);

    callsite.fetch(CALL_SITE_DEPTH);

    // Check whether this malloc are having the same callsite as the existing one.
    bool sameCallsite = obj->sameCallsite(&callsite);
    // Check whether current callsite is the same as before. If it is
    // not the same, we have to cleanup all information about the old
    // object to avoid false positives.
    //if(isProtected && !obj->sameCallsite(&callsite)) {
    if(isProtected) {
      bool successCleanup;
    
     // fprintf(stderr, "Now malloc with ptr %p and size %d 2222!!!!\n", ptr, sz);   

      // When the orignal object should be reported, then we are forcing
      // the allocator to pickup another object.
      successCleanup = xheapcleanup::getInstance().cleanupHeapObject(ptr, sz, sameCallsite);
      if(successCleanup != true) {
    //    fprintf(stderr, "Now malloc with ptr %p and size %d 3333!!!!\n", ptr, sz);   
        goto Remalloc_again;
      }
    
      // Save the new callsite if it is a new callsite.
      if(!sameCallsite) {
        obj->storeCallsite (callsite);
      }
    } else if (!isProtected) {
      // Save callsite to object header.
      obj->storeCallsite (callsite);
    } 

    //fprintf(stderr, "Now malloc with ptr %p and size %d\n", ptr, sz);   
    return ptr;
  }


  inline void * realloc (void * ptr, size_t sz, bool isProtected) {
    size_t s = getSize (ptr);

    void * newptr =  malloc (sz, isProtected);
    if (newptr && s != 0) {
      size_t copySz = (s < sz) ? s : sz;
      memcpy (newptr, ptr, copySz);
    }

    free (ptr);
    return newptr;
  }

  inline void free (void * ptr) {
    size_t s = getSize(ptr);

    //printf("Now free ptr %p with size %d\n", ptr, s);
    _heap.free(_heapid, ptr);
  }

  /// @return the allocated size of a dynamically-allocated object.
  inline size_t getSize (void * ptr) {
    // Just pass the pointer along to the heap.
    return _heap.getSize (ptr);
  }
 
  void openProtection() {
    //fprintf(stderr, "Now %d open the protection\n", getpid());
    _globals.openProtection();
    _heap.openProtection();
    _protection = true;
  }

  void closeProtection() {
    //fprintf(stderr, "Now %d close the protection\n", getpid());
    // Only do it when the protection is set.
    if (_protection) {
      // memory spaces (globals and heap).
      _globals.closeProtection();
      _heap.closeProtection();
      _protection = false;
    }
  }

  inline void setThreadIndex (int heapid) {
    _heapid = heapid%xdefines::NUM_HEAPS;
    _heap.setHeapId(heapid%xdefines::NUM_HEAPS);
  }

  /// Beginning of an atomic transaction.
  inline void begin (bool startTimer, bool startThread) {
    //stopCheckingTimer();
    _globals.begin();
    _heap.begin();

    if(startTimer) { 
      startCheckingTimer();
    }
  }

  // Actual page fault handler.
  inline void handleWrite (void * addr) {
    if (_heap.inRange (addr)) {
      _heap.handleWrite (addr);
    } else if (_globals.inRange (addr)) {
      _globals.handleWrite (addr);
    } else {
      // Something must be wrong here.
      fprintf(stderr, "address %p is out of range!\n", addr);
    }
  }
  
  // Commit those local changes to the shared mapping.
  inline void commit (bool doChecking, bool update) {
    stopCheckingTimer();

    // Commit local modifications to the shared mapping.
    _heap.commit(doChecking);
    _globals.commit(doChecking);
  } 

  /// @brief Disable checking timer
  inline void stopCheckingTimer() {
    if(_timerStarted)
      ualarm(0, 0);
  } 
 
  // Start the timer 
  inline void startCheckingTimer() {
    ualarm(xdefines::PERIODIC_CHECKING_INTERVAL, 0);
    _timerStarted = true;
  }

  inline void enableCheck() {
    atomic::atomic_set(&_doChecking, 1);
  }

  inline void disableCheck() {
    atomic::atomic_set(&_doChecking, 0);
  } 

  void doPeriodicChecking () {
   // if(_doChecking == 1) {
    //  stopCheckingTimer();
    _globals.periodicCheck();
    _heap.periodicCheck();
   // }

    startCheckingTimer(); 
  }

  unsigned long sharemem_read_word(void * dest) {
    if(_heap.inRange(dest)) {
      return _heap.sharemem_read_word(dest);
    } else if(_globals.inRange(dest)) {
      return _globals.sharemem_read_word(dest);
    }
    return 0;
  }

  void sharemem_write_word(void * dest, unsigned long val) {
    if(_heap.inRange(dest)) {
      _heap.sharemem_write_word(dest, val);
    } else if(_globals.inRange(dest)) {
      _globals.sharemem_write_word(dest, val);
    }
  }

private:
  objectHeader * getObjectHeader (void * ptr) {
    objectHeader * o = (objectHeader *) ptr;
    return (o - 1);
  }

  /* Signal-related functions for tracking page accesses. */
  /// @brief Signal handler to trap SEGVs.
  static void segvHandle (int signum,
			  siginfo_t * siginfo,
			  void * context) 
  {
    //xmemory::getInstance().disableCheck();
    xmemory::getInstance().stopCheckingTimer();

    void * addr = siginfo->si_addr; // address of access

    // Check if this was a SEGV that we are supposed to trap.
    if (siginfo->si_code == SEGV_ACCERR) {
      // It is a write operation. Handle that.
      xmemory::getInstance().handleWrite (addr);
    } else if (siginfo->si_code == SEGV_MAPERR) {
      fprintf (stderr, "%d : map error with addr %p!\n", getpid(), addr);
      ::abort();
    } else {

      fprintf (stderr, "%d : other access error with addr %p.\n", getpid(), addr);
      ::abort();
    }

    xmemory::getInstance().startCheckingTimer();
    //xmemory::getInstance().enableCheck();
  }

  /// @brief Handle those timers about checking.
  static void checkingTimerHandle (int signum,
				   siginfo_t * siginfo,
				   void * context) 
  {
    xmemory::getInstance().doPeriodicChecking();
  }

  /// @brief Install a handler for SEGV signals.
  void installSignalHandler() {
    stack_t         _sigstk;
#if defined(linux)
    // Set up an alternate signal stack.
    _sigstk.ss_sp = mmap (NULL, SIGSTKSZ, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    _sigstk.ss_size = SIGSTKSZ;
    _sigstk.ss_flags = 0;
    sigaltstack (&_sigstk, (stack_t *) 0);
#endif
    // Now set up a signal handler for SIGSEGV events.
    struct sigaction siga;
    sigemptyset (&siga.sa_mask);

    // Set the following signals to a set 
    sigaddset (&siga.sa_mask, SIGSEGV);
    sigaddset (&siga.sa_mask, SIGALRM);

    sigprocmask (SIG_BLOCK, &siga.sa_mask, NULL);

    // Point to the handler function.
#if defined(linux)
    siga.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESTART | SA_NODEFER;
#else
    siga.sa_flags = SA_SIGINFO | SA_RESTART;
#endif

    siga.sa_sigaction = xmemory::segvHandle;
    if (sigaction (SIGSEGV, &siga, NULL) == -1) {
      fprintf (stderr, "Signal handler for SIGSEGV failed to install.\n");
      exit (-1);
    }

    // We use the alarm to trigger checking timer.
    siga.sa_sigaction = xmemory::checkingTimerHandle;
    if (sigaction (SIGALRM, &siga, NULL) == -1) {
      fprintf (stderr, "Signal handler for SIGALRM failed to install.\n");
      exit (-1);
    }

    sigprocmask (SIG_UNBLOCK, &siga.sa_mask, NULL);
  }

private:

  /// The protected heap used to satisfy big objects requirement. Less
  /// than 256 bytes now.
  warpheap<xdefines::NUM_HEAPS, xdefines::PROTECTEDHEAP_CHUNK, xoneheap<xheap<xdefines::PROTECTEDHEAP_SIZE> > > _heap;
  
  /// The globals region.
  xglobals          _globals;


  typedef std::set<void *, less<void *>, HL::STLAllocator<void *, privateheap> > pagesetType;

  int _heapid;

  bool _timerStarted;
  /// Internal share heap.
  InternalHeap  _internalheap;
  unsigned long _doChecking;
  bool          _protection;
};

#endif
