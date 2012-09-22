// -*- C++ -*-

#ifndef SHERIFF_XTHREAD_H
#define SHERIFF_XTHREAD_H

#include <errno.h>

#if !defined(_WIN32)
#include <sys/wait.h>
#include <sys/types.h>
#endif

#include <stdlib.h>

#include "xdefines.h"


// Heap Layers
#include "freelistheap.h"
#include "mmapheap.h"
#include "util/cpuinfo.h"

#include "internalheap.h"

extern "C" {
  // The type of a pthread function.
  typedef void * threadFunction (void *);
}

class xrun;

class xthread {
private:

  /// @class ThreadStatus
  /// @brief Holds the thread id and the return value.
  class ThreadStatus {
  public:
    ThreadStatus (void * r, bool f)
      : retval (r),
	forked (f)
    {}

    ThreadStatus()
    {}

    /// The thread id.
    int tid;

    /// The return value from the thread.
    void * retval;

    /// Whether this thread was created by a fork or not.
    bool forked;
  };

public:

  xthread()
    : _nestingLevel (0),
      _protected (false)
  {
  }

  void setMaxThreads (unsigned int n)
  {
    //    _throttle.set (n);
    //    printf ("throttle = %d\n", n);
  }

  void * spawn (xrun * runner,
		threadFunction * fn,
		void * arg);

  void join (xrun * runner,
	     void * v,
	     void ** result);

  void cancel (xrun * runner, void * v);
  void thread_kill (xrun * runner, void *v, int sig);

  inline int getId() const {
    return _tid;
  }

  inline void setId (int id) {
    _tid = id;
  }


private:

  void * forkSpawn (xrun * runner,
		    threadFunction * fn,
		    ThreadStatus * t,
		    void * arg);

  static void run_thread (xrun * runner,
			  threadFunction * fn,
			  ThreadStatus * t,
			  void * arg);

  /// @return a chunk of memory shared across processes.
  void * allocateSharedObject (size_t sz) {
#if 0
    // Why we can not use this share heap. FIXME
    return InternalHeap::getInstance().malloc(sz);
#else
    return mmap (NULL,
         sz,
         PROT_READ | PROT_WRITE,
         MAP_SHARED | MAP_ANONYMOUS,
         -1,
         0);
#endif
  }

  void freeSharedObject (void * ptr, size_t sz) {
#if 0
    InternalHeap::getInstance().free(ptr);
#else
    munmap(ptr, sz);
#endif
  }

  // A semaphore that bounds the number of active threads at any time.
  //  xsemaphore	   _throttle;

  /// Current nesting level (i.e., how deep we are in recursive threads).
  unsigned int	   _nestingLevel;

  /// What is this thread's PID?
  int              _tid;

  int              _protected;

//  int              _heapid;
};

#endif
