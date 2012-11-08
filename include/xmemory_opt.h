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
  : _init (false),
    _internalheap (InternalHeap::getInstance()),
    _stats   (stats::getInstance())
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
    _bheap.initialize();
    _bheap.setHeapId(0);
    _globals.initialize();
    xpageentry::getInstance().initialize();
    xpagestore::getInstance().initialize();
  
    // In the beginning, we will protect. 
    _protection = true;

#ifdef DETECT_FALSE_SHARING_OPT  
    _lasttrans = 0;
    // To make sure that the first several transaction should be checked.
    _lastema = 0;
    _needChecking = true;
#else
    _lasttrans = 0;
    _lastema = 0;
#endif
    _init = true;
  }

  void setMainId (int tid) {
    _maintid = tid;
  }

  void finalize() {
    _globals.finalize(NULL);
    _bheap.finalize (_bheap.getend());
  }


  inline void *malloc (size_t sz, bool isProtected) {
    void * ptr = NULL;
    bool   checkCallsite = false;

Remalloc_again:
#ifdef DETECT_FALSE_SHARING_OPT
  //fprintf(stderr, "xmemory malloc sz %d\n", sz);
  ptr = _bheap.malloc(_heapid, sz);
   
  // Otherwise, there is a cycle.
  if(_init == true)
    checkCallsite = true;

  // Get callsite information.
  if(checkCallsite) {
    CallSite callsite;
    objectHeader * obj = getObjectHeader(ptr);

    callsite.fetch(CALL_SITE_DEPTH);

    bool sameCallsite = obj->sameCallsite(&callsite);
    // Check whether current callsite is the same as before. If it is
    // Check whether current callsite is the same as before. If it is
    // not the same, we have to cleanup all information about the old
    // object to avoid false positives.
    if(isProtected) {
      bool successCleanup;

      // When the orignal object should be reported, then we are forcing
      // the allocator to pickup another object.
      successCleanup = xheapcleanup::getInstance().cleanupHeapObject(ptr, sz, sameCallsite);
      if(successCleanup != true) {
        goto Remalloc_again;
      }
  #ifdef GET_CHARACTERISTICS
      atomic::add(sz, (unsigned long *)&cleanupSize);
  #endif
      // Save the new callsite information.
      obj->storeCallsite (callsite);
    } else if (!isProtected) {
      // Save callsite to object header.
      obj->storeCallsite (callsite);
    }
  #ifdef GET_CHARACTERISTICS
    atomic::increment((unsigned long *)&allocTimes);
  #endif
  }
#else
  if(sz <= xdefines::LARGE_CHUNK) 
    ptr = _bheap.malloc (_heapid, sz);
  else 
    ptr = _sheap.malloc (_heapid, sz);
#endif
  
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
    size_t s = getSize (ptr);
  
#ifdef DETECT_FALSE_SHARING_OPT
    _bheap.free(_heapid, ptr);
#else
    if (s <= xdefines::LARGE_CHUNK) {
      _bheap.free(_heapid, ptr);
    } else {
      _sheap.free(_heapid, ptr);
    }
#endif
  }

  /// @return the allocated size of a dynamically-allocated object.
  inline size_t getSize (void * ptr) {
    // Just pass the pointer along to the heap.
    return _bheap.getSize (ptr);
  }
 
  void openProtection() {
    _globals.openProtection();
    _bheap.openProtection();
    _protectLargeHeap = true;
    _protection = true;
  }

  void closeProtection() {
    // Only do it when the protection is set.
    if (_protection) {
      // memory spaces (globals and heap).
      _globals.closeProtection();
      _bheap.closeProtection();
      _protectLargeHeap = false;
      _protection = false;
    }
  }

  inline void setThreadIndex (int heapid) {
    _heapid = heapid%xdefines::NUM_HEAPS;
    _bheap.setHeapId(heapid%xdefines::NUM_HEAPS);
  }

  inline void begin (bool startTimer, bool startThread) {
#ifdef DETECT_FALSE_SHARING_OPT
    stopCheckingTimer();
    _globals.begin();
    _bheap.begin();
    if(startTimer) { 
      startCheckingTimer(true);
    }
#else
    if (_protection) {
      // Reset global and heap protection.
      _globals.begin();
      _bheap.begin();
    }
    if (startThread) {
      _lasttrans = _stats.getTrans();
      start(&_lasttime);
    }
#endif
  }

  // Actual page fault handler.
  inline void handleWrite (void * addr) {
    if (_bheap.inRange (addr)) {
      _bheap.handleWrite (addr);
    } else if (_globals.inRange (addr)) {
      _globals.handleWrite (addr);
    } else {
      // Something must be wrong here.
      fprintf(stderr, "address %p is out of range!\n", addr);
    }
  }
  

  // EDB: This comment needs to be rewritten for clarity.

  // How to disable protection periodically for large objects.  Since
  // there are two cases in those benchmarks: One type: fluidanimate,
  // transaction is short, then we may not have much checking times.
  // For this type, we may check the transaction times in order to
  // close protection. In fact, We may close system call to set
  // timers.  Another type: canneal, transaction is long, but checking
  // should take a long time because of large working set.  For this
  // type, we maybe should check the checking times.  So totally, it
  // would be good to check the times of checking times and
  // transaction times.

  inline void commit (bool doChecking, bool update) {
#ifdef DETECT_FALSE_SHARING_OPT
    stopCheckingTimer();
#endif

    // Commit local modifications to the shared mapping.
    _bheap.commit(doChecking);
    _globals.commit(doChecking);
  
    // Update the transaction number.
    _stats.updateTrans();

#ifdef DETECT_FALSE_SHARING_OPT
    evaluateProtection(update, true);
#else 
    evaluateProtection(update);
#endif
} 

  inline int getElapsedMs() {
    return (elapsed2ms(stop(&_lasttime, NULL)));
  }

  // We are using an exponential weighted moving average here, with
  // alpha set to 0.9.
  inline double getAverage (unsigned long elapsed, int trans) {
    double value = 0;
    if(trans > 0) {
      value = 0.9*(double)elapsed/(double)trans + 0.1*(double)_lastema;
    }
    return value;
  }

#ifndef DETECT_FALSE_SHARING_OPT // For Sheriff-Protect only
  void evaluateProtection (bool update) {
    int trans;
    unsigned long elapse = 0;

    if(update) {
      trans = _stats.updateTrans();
      trans++;
    }
    else {
      trans = _stats.getTrans();
    }

    // Check for the transaction length
    if(_protection && (trans-_lasttrans > CHECK_AGAIN_UNDER_PROTECTION)) {
      elapse = getElapsedMs();
      double ema = getAverage(elapse, trans);

      // Don't protect if tran length is shorter than predefined threshold.
      if(ema <= THRESH_TRAN_LENGTH) {
        _globals.cleanup();
        _bheap.cleanup();
          
        // We need to disable protection.
        closeProtection();
      }
    
      _lasttrans = trans;
      _lastema = ema;
      start(&_lasttime);
    }
    else if(!_protection && trans - _lasttrans > CHECK_AGAIN_NO_PROTECTION) {
      // If we are not protected, we check periodically whether transaction
      // length is long enough.
      elapse = getElapsedMs();
      double ema = getAverage(elapse, trans);
      if(ema > THRESH_TRAN_LENGTH) {
        // Open protection again. 
        openProtection();
      }
      _lasttrans = trans;
      _lastema = ema;
      start(&_lasttime);
    }
  }
#endif

#ifdef DETECT_FALSE_SHARING_OPT
  bool checkProtection(int events) {
    bool doProtect = false;

    int remaining = events % xdefines::EVAL_LARGE_HEAP_BASE;
    if(remaining < xdefines::EVAL_LARGE_HEAP_PROTECTION) {
      doProtect = true;
    }

    return(doProtect);
  }
  
  void evaluateProtection(bool update, bool isCommit) {
    if(update == false) 
     return;
  
    // We need to update the global events first and get the total events number.
    int events = _stats.updateEvents();
  
    if(!isCommit) {
      return;
    }
  
    // Whether we need protection status for large heap.
    bool doProtect = checkProtection(events);
  
    // Act only when there is a state change.
    if(doProtect == true && _protectLargeHeap == false) {
      // If we need protection but currently we don't protect at all,
      // Then do protection.
      // Protect those shared pages.
      _globals.setProtectionPeriod();
      _bheap.setProtectionPeriod();
      _protectLargeHeap = true;
    }
    else if (doProtect == false && _protectLargeHeap == true) {
      // Here, we need to switch off the protection.
      // To guarantee the correctness, we need to commit those local changes since
      // later changes should happen on the shared mapping directly.
      // We only need to work on those shared pages.TONGPING  
      _bheap.unprotectNonProfitPages(_bheap.getend());
      _globals.unprotectNonProfitPages(NULL);
      // In order to improve the performance, only close protection for those shared pages
      // but no interleaving writes in the period.
      _protectLargeHeap = false;
    }
  }
#endif

  void evalCheckingTimer(int trans) {
    int elapse = getElapsedMs();
    double ema = getAverage(elapse, trans);

    if(ema <= xdefines::PERIODIC_CHECKING_INTERVAL*2) {
      // Close the protection when transaction is short.
      _needChecking = false;
    }
    else {
      _needChecking = true;
    }
       
    _lasttrans = trans;
    _lastema = ema;
    start(&_lasttime);
  }

  /// @brief Disable checking timer
  inline void stopCheckingTimer() {
    if(_timerStarted)
      ualarm(0, 0);
  } 
 
  // Save some time to set the timer. TONGPING 
  inline void startCheckingTimer (bool evaluate) {
  
    // Check whether we actually need to start checking timer.
    // When the transaction is too short, we don't need to start the 
    // checking timer. TONGPING
    if(!evaluate) {
      ualarm(xdefines::PERIODIC_CHECKING_INTERVAL, 0);
      _timerStarted = true;
      return;
    }
 
    int trans = _stats.getTrans();
 
    if(trans%xdefines::EVAL_CHECKING_PERIOD == 0 && trans > xdefines::EVAL_CHECKING_PERIOD) {
      evalCheckingTimer(trans);     
    }
      
    // Evaluate the checking timer.
    if(_needChecking) {
      ualarm(xdefines::PERIODIC_CHECKING_INTERVAL, 0);
      _timerStarted = true;
    }
    else {
      _timerStarted = false;
    }
  }

  inline void enableCheck() {
    atomic::atomic_set(&_doChecking, 1);
  }

  inline void disableCheck() {
    atomic::atomic_set(&_doChecking, 0);
  } 

  void doPeriodicChecking () {
    if(_doChecking == 1) {
      stopCheckingTimer();
      _globals.periodicCheck();
      _bheap.periodicCheck();
#ifdef DETECT_FALSE_SHARING_OPT
      evaluateProtection(true, false);
#endif
    }

    startCheckingTimer(false); 
  }

  unsigned long sharemem_read_word(void * dest) {
    if(_bheap.inRange(dest)) {
      return _bheap.sharemem_read_word(dest);
    } else if(_globals.inRange(dest)) {
      return _globals.sharemem_read_word(dest);
    }
    return 0;
  }

  void sharemem_write_word(void * dest, unsigned long val) {
    if(_bheap.inRange(dest)) {
      _bheap.sharemem_write_word(dest, val);
    } else if(_globals.inRange(dest)) {
      _globals.sharemem_write_word(dest, val);
    }
  }

private:
  objectHeader * getObjectHeader (void * ptr) {
    objectHeader * o = (objectHeader *) ptr;
    return (o - 1);
  }

public:

  /* Signal-related functions for tracking page accesses. */

  /// @brief Signal handler to trap SEGVs.
  static void segvHandle (int signum,
			  siginfo_t * siginfo,
			  void * context) 
  {
#ifdef DETECT_FALSE_SHARING_OPT
    xmemory::getInstance().disableCheck();
#endif
    void * addr = siginfo->si_addr; // address of access

    // Check if this was a SEGV that we are supposed to trap.
    if (siginfo->si_code == SEGV_ACCERR) {
      // Compute the page that holds this address.
      void * page = (void *) (((size_t) addr) & ~(xdefines::PageSize-1));

      // Unprotect the page and record the write.
      mprotect ((char *) page,
                xdefines::PageSize,
                PROT_READ | PROT_WRITE);

    // It is a write operation. Handle that.
      xmemory::getInstance().handleWrite (addr);
    } else if (siginfo->si_code == SEGV_MAPERR) {
      fprintf (stderr, "%d : map error with addr %p!\n", getpid(), addr);
      ::abort();
    } else {

      fprintf (stderr, "%d : other access error with addr %p.\n", getpid(), addr);
      ::abort();
    }

#ifdef DETECT_FALSE_SHARING_OPT
    xmemory::getInstance().enableCheck();
#endif
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
  warpheap<xdefines::NUM_HEAPS, xdefines::PROTECTEDHEAP_CHUNK, xoneheap<xheap<xdefines::PROTECTEDHEAP_SIZE> > > _bheap;
  
  /// The globals region.
  xglobals          _globals;

#ifndef DETECT_FALSE_SHARING_OPT
  warpheap<xdefines::NUM_HEAPS, xdefines::SHAREDHEAP_CHUNK,xoneheap<SourceSharedHeap<xdefines::SHAREDHEAP_SIZE> > > _sheap;
#endif

  typedef std::set<void *, less<void *>,
		   HL::STLAllocator<void *, privateheap> > // myHeap> >
  pagesetType;

  int _heapid;

  /// Whether globals or heap are empty? 
  bool _heapEmpty;
  bool _globalEmpty;

  bool _init;
  
  /// Internal share heap.
  InternalHeap    _internalheap;

  /// A signal stack, for catching signals.
  stack_t         _sigstk;

  /// A lock that protect the global area.
  int   _maintid;

  stats &     _stats;
  // Do we allow the checking.
  bool _timerStarted;
  unsigned long _doChecking;
  bool _protection;

  unsigned long _lasttrans;
  struct timeinfo _lasttime;
  double _lastema;

  bool _needChecking;
  bool _protectLargeHeap;
};

#endif
