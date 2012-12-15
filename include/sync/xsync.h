// -*- C++ -*-

#ifndef SHERIFF_XSYNC_H
#define SHERIFF_XSYNC_H

#include <map>

#if !defined(_WIN32)
#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#include "xdefines.h"
#include "internalheap.h"
#if defined(DETECT_FALSE_SHARING)
#include "xmemory.h"
#else
#include "xmemory_opt.h"
#endif
/**
 * @class xbarrier
 * @brief Manage the cross-process barrier.
 *  Here, we will do some tricks since we can not use the passed barrier because it is a global variable(protected by us). 
 *  We don't want to introduce the additional read/write set. That will cause some confuse. 
 *
 */
class xsync {
public:

  xsync()
  {
    WRAP(pthread_mutexattr_init)(&_mutex_attr);
    pthread_mutexattr_setpshared (&_mutex_attr, PTHREAD_PROCESS_SHARED);

    WRAP(pthread_condattr_init)(&_cond_attr);
    pthread_condattr_setpshared (&_cond_attr, PTHREAD_PROCESS_SHARED);

    pthread_barrierattr_init(&_barrier_attr);
    pthread_barrierattr_setpshared (&_barrier_attr, PTHREAD_PROCESS_SHARED);
  }

  /// @brief Initialize the lock.
  inline pthread_mutex_t * mutex_init(pthread_mutex_t * lck, bool needProtect) {
    pthread_mutex_t * realMutex = NULL;

    if(needProtect) {
      lock();
      realMutex = (pthread_mutex_t *)getSyncEntry_sharemem(lck); 
    }
   
    if(!realMutex) {
      realMutex = (pthread_mutex_t *)allocSyncEntry(lck, sizeof(pthread_mutex_t));
    
      // Initialize the mutex that shared by different processes
      WRAP(pthread_mutex_init)(realMutex, &_mutex_attr);
    }
    
    if(needProtect) {
      unlock();
    }
    
    return realMutex;
  }

  /// @brief Lock the lock.
  inline int mutex_lock (pthread_mutex_t * lck) {
    pthread_mutex_t * realMutex = getRealMutex(lck);
  
    assert(realMutex != NULL);
    // Now lock it.
    return WRAP(pthread_mutex_lock) (realMutex);
  }

  /// @brief Unlock the lock.
  inline int mutex_unlock (pthread_mutex_t * lck) {
    pthread_mutex_t * realMutex = getRealMutex(lck);
    assert(realMutex != NULL);
    return WRAP(pthread_mutex_unlock) (realMutex);
  }
  
  /// @brief Destroy the lock.
  inline void mutex_destroy (pthread_mutex_t * lck) {
    deallocSyncEntry(lck);
  }

  pthread_cond_t * cond_init(void * cond, bool needProtect) {
    pthread_cond_t * realCond = NULL;

    if(needProtect) {
      lock();
      realCond = (pthread_cond_t *)getSyncEntry_sharemem(cond); 
    }
   
    if(realCond == NULL) {
      realCond = (pthread_cond_t *)allocSyncEntry(cond, sizeof(pthread_cond_t));
    // Initialize the mutex that shared by different processes
      WRAP(pthread_cond_init)(realCond, &_cond_attr);
    }

    if(needProtect) {
      unlock();
    }

    return realCond;
  }

  void cond_destroy(void * cond) {
    deallocSyncEntry(cond);
  }

  int cond_wait (void * cond, void * lck) {
    // Look for this cond in the map of initialized condes.
    pthread_cond_t * realCond = getRealCond(cond);
    pthread_mutex_t * realMutex = getRealMutex(lck);

    return WRAP(pthread_cond_wait) (realCond, realMutex);
  }

  /// @brief Unblock at least one thread waiting on the cond.
  int cond_signal (void * cond) {
    pthread_cond_t * realCond = getRealCond(cond);
  
    return WRAP(pthread_cond_signal)(realCond);
  }

  /// @brief Unblock all threads waiting on the cond.
  int cond_broadcast (void * cond) {
    pthread_cond_t * realCond = getRealCond(cond);
    return WRAP(pthread_cond_broadcast)(realCond);
  }
 
  /// @brief Initialize the barrier.
  int barrier_init(void * barrier, unsigned int count) {

    // Look for this barrier in the map of initialized barrieres.
    pthread_barrier_t * realBarrier=(pthread_barrier_t *)allocSyncEntry(barrier,sizeof(pthread_barrier_t));

    // Set this entry to be process-shared.
    WRAP(pthread_barrier_init)(realBarrier, &_barrier_attr, count);

    // Initialize the barrier that shared by different processes
    return 0;
  }

  int barrier_wait (void * barrier) {
    // Look for this barrier in the map of initialized barrieres.
    pthread_barrier_t * realBarrier = (pthread_barrier_t *)getSyncEntry(barrier);

    // barrier must be initialized explicitly.
    assert(realBarrier);  

    return WRAP(pthread_barrier_wait)(realBarrier);
  }

  /// @brief Destroy the barrier.
  int barrier_destroy (void * barrier) {
    // Release this part of memory.
    deallocSyncEntry(barrier);
    return 0;
  }

private:

  inline void * allocSyncEntry(void *origentry, int size) {
    void * entry = ((void *)InternalHeap::getInstance().malloc(size));
    setSyncEntry(origentry, entry);

    return entry; 
  }

  inline void deallocSyncEntry(void *ptr) {
    void * realentry = getSyncEntry(ptr);

    assert(realentry);
    InternalHeap::getInstance().free(realentry);
  }

  inline pthread_cond_t * getRealCond(void * cond) {
    pthread_cond_t * realcond = (pthread_cond_t *)getSyncEntry(cond);

    if(!realcond) {
      // Whenever it is not allocated, then we will try to allocate one.
      realcond = cond_init(cond, true);
    } 
    return realcond;
  }

  void clearSyncEntry(void * origentry) {
    void **dest = (void**)origentry;

    *dest = NULL;

    // Update the shared copy in the same time. 
    xmemory::getInstance().sharemem_write_word(origentry, 0);
  }

  void setSyncEntry(void * origentry, void * newentry) {
    void **dest = (void**)origentry;

    // FIXME, is it correct to do this???
    // We will create a copy of this page in current thread.
    //*dest = newentry;

    // Update the shared copy in the same time. 
    xmemory::getInstance().sharemem_write_word(origentry, (unsigned long)newentry);
  }

  void * getSyncEntry(void * entry) {
    void ** ptr = (void **)entry;
//    fprintf(stderr, "%d: entry %p and synentry 0x%x. ptr %p\n", getpid(), entry, *((int *)entry), *ptr);   
    return(*ptr);
  }

  void * getSyncEntry_sharemem(void * entry) {
    void * result = (void *)xmemory::getInstance().sharemem_read_word(entry);
    if(result) {
      // Now some one has already initialized this entry, we should set this one.
      void **dest = (void**)entry;
      *dest = result;
    }
    return result;
  }

  inline pthread_mutex_t * getRealMutex(void * lck) {
    pthread_mutex_t * mutex = (pthread_mutex_t *)getSyncEntry(lck);

    if(mutex ==NULL) {
      // Whenever it is not allocated, then we will try to allocate one.
      mutex = mutex_init((pthread_mutex_t *)lck, true);
    }
    return mutex; 
  }

  void lock(void) {
    _global_sync_lock.lock();
  }

  void unlock(void) {
    _global_sync_lock.unlock();
  }

  /// The barrier's attributes.
  pthread_barrierattr_t _barrier_attr;
  pthread_condattr_t _cond_attr;
  pthread_mutexattr_t _mutex_attr;

  xplock _global_sync_lock;
};


#endif
