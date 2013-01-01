// -*- C++ -*-

/*
  Author: Emery Berger, http://www.cs.umass.edu/~emery
 
  Copyright (c) 2007-12 Emery Berger, University of Massachusetts Amherst.

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
 * @file   xrun.h
 * @brief  The main engine for consistency management, etc.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */

#ifndef SHERIFF_XRUN_H
#define SHERIFF_XRUN_H

#include "xdefines.h"

// threads
#include "xthread.h"

// memory
#if defined(DETECT_FALSE_SHARING)
#include "xmemory.h"
#else
#include "xmemory_opt.h"
#endif

// Heap Layers
#include "util/sassert.h"

#include "xsync.h"

// Grace utilities
#include "atomic.h"

class xrun {

private:

  xrun()
  : _locksHeld (0),
    _memory (xmemory::getInstance()),
    _isInitialized (false),
    _isProtected (false)
  {
  }

public:

  static xrun& getInstance() {
    static char buf[sizeof(xrun)];
    static xrun * theOneTrueObject = new (buf) xrun();

    return *theOneTrueObject;
  }

  /// @brief Initialize the system.
  void initialize()
  {
    pid_t pid = syscall(SYS_getpid);
 
    if (!_isInitialized) {
      _isInitialized = true;

      // Initialize the memory (install the memory handler)
      _memory.initialize();

      // Set the current _tid to our process id.
      _thread.setId (pid);
      
      _tid = pid;
      _memory.setMainId(pid);
      
      // Set thread to spawn no more threads than number of processors.
      _thread.setMaxThreads (HL::CPUInfo::getNumProcessors());
   } else {
      fprintf(stderr, "%d : OH NOES\n", getpid());
      ::abort();
      // We should only initialize ONCE.
    }
  }

  void openMemoryProtection(void) {
    _memory.openProtection();
    _isProtected = true;
    _hasProtected = true;
  }

  void closeMemoryProtection(void) {
    _memory.closeProtection();
    _isProtected = false;
  }

  void finalize (void)
  {
    // If the tid was set, it means that this instance was
    // initialized: end the transaction (at the end of main()).
    _memory.finalize();
  }

  /* Transaction-related functions. */
  inline void threadRegister (void) {
    int threadindex;
      
    threadindex = atomic::increment_and_return(global_thread_index);
 
    // Since we are a new thread, we need to use the new heap.
    _memory.setThreadIndex(threadindex+1);

    return;
  }   

  inline void resetThreadIndex(void) {
   *global_thread_index = 0;
  }

  /* Thread-related functions. */

  /// @ Return the main thread's id.
  inline int main_id() const {
    return _tid;
  }

  /// @return the "thread" id.
  inline int id() const {
    return _thread.getId();
  }

  /// @brief Spawn a thread.
  /// @return an opaque object used by sync.
  inline void * spawn (threadFunction * fn, void * arg)
  {
    _locksHeld = 0;
    return _thread.spawn (this, fn, arg);
  }

  /// @brief Wait for a thread.
  inline void join (void * v, void ** result) {
  #if defined(DETECT_FALSE_SHARING) || defined(DETECT_FALSE_SHARING_OPT)
    _memory.stopCheckingTimer();
  #endif

    _thread.join (this, v, result);
  }

  /// @brief Do a pthread_cancel
  inline void cancel (void *v) {
    _thread.cancel(this, v);
  }

  inline void thread_kill (void *v, int sig) {
    atomicEnd(true, true);
    _thread.thread_kill(this, v, sig);
    atomicBegin(false, false);
  } 

  /* Heap-related functions. */
  void * malloc (size_t sz) {
    void * ptr = _memory.malloc (sz, _hasProtected);
    return ptr;
  }

  inline void * calloc (size_t nmemb, size_t sz) {
    void * ptr = malloc(nmemb * sz);
    return ptr;
  }

  // In fact, we can delay to open its information about heap.
  inline void free (void * ptr) {
    //fprintf(stderr, "Now free object at ptr %p\n", ptr);
    _memory.free (ptr);
  }

  inline size_t getSize (void * ptr) {
    return _memory.getSize (ptr);
  }

  inline void * realloc (void * ptr, size_t sz) {
    void * newptr;
    if (ptr == NULL) {
      newptr = malloc(sz);
      return newptr;
    }
    if (sz == 0) {
      free (ptr);
      return NULL;
    }

    newptr = _memory.realloc (ptr, sz, _hasProtected);
    return newptr;
  }

  ///// conditional variable functions.
  void cond_init (void * cond) {
    _sync.cond_init(cond, false);
  }

  void cond_destroy (void * cond) {
    _sync.cond_destroy(cond);
  }

  // Barrier support
  int barrier_init(pthread_barrier_t  *barrier, unsigned int count) {
    return _sync.barrier_init(barrier, count);
  }

  int barrier_destroy(pthread_barrier_t *barrier) {
    _sync.barrier_destroy(barrier);
    return 0;
  }

  ///// mutex functions
  /// FIXME: maybe it is better to save those actual mutex address in original mutex.
  int mutex_init(pthread_mutex_t * mutex) {
    _sync.mutex_init(mutex, false);
    return 0;
  }

  // FIXME: if we are trying to remove atomicEnd() before mutex_sync(),
  // we should unlock() this lock if abort(), otherwise, it will
  // cause a deadlock.

  void mutex_lock(pthread_mutex_t * mutex) {
    atomicEnd(true, true);
    _sync.mutex_lock(mutex);
    atomicBegin(false, false);
  }

  void mutex_unlock(pthread_mutex_t * mutex) {
    atomicEnd(false, true);
    _sync.mutex_unlock(mutex);
    atomicBegin(true, false);
  }

  int mutex_destroy(pthread_mutex_t * mutex) {
    _sync.mutex_destroy(mutex);
    return 0;
  }

  int barrier_wait(pthread_barrier_t *barrier) {
    atomicEnd(true, true);
    _sync.barrier_wait(barrier);
    atomicBegin(true, false);
    return 0;
  }

  /// FIXME: whether we can using the order like this.
  void cond_wait(void * cond, void * lock) {
    atomicEnd(false, true);
    _sync.cond_wait (cond, lock);
    atomicBegin(false, false);
  }

  void cond_broadcast (void * cond) {
    if(_locksHeld != 0) {
      atomicEnd(false, true);
      _sync.cond_broadcast (cond);
      atomicBegin(false, false);
    } else {
      atomicEnd(true, true);
      _sync.cond_broadcast (cond);
      atomicBegin(true, false);
    }
  }

  void cond_signal (void * cond) {
    if(_locksHeld != 0) {
      atomicEnd(false, true);
      _sync.cond_signal (cond);
      atomicBegin(false, false);
    } else {
      atomicEnd(true, true);
       _sync.cond_signal (cond);
      atomicBegin(true, false);
    }
  }

  /// @brief Start a transaction.
  void atomicBegin(bool startTimer, bool startThread) {
    if(!_isProtected)
      return;

    // Now start.
    _memory.begin(startTimer, startThread);
  }

  /// @brief End a transaction, aborting it if necessary.
  void atomicEnd(bool doChecking, bool updateTrans) {
    if(!_isProtected)
      return;
  
    // First, attempt to commit.
    _memory.commit(doChecking, updateTrans);

    // Flush the stdout.
    fflush(stdout);
  }

private:


  xthread	_thread;
  xsync  	_sync;

  unsigned int _locksHeld;

  /// The memory manager (for both heap and globals).
  xmemory&     _memory;

  volatile  bool   _isInitialized;
  volatile  bool   _isProtected;
  volatile  bool   _hasProtected;
  int   _tid; //The first process's id.
};


#endif
