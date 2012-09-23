#ifndef SHERIFF_XPLOCK_H
#define SHERIFF_XPLOCK_H

#if !defined(_WIN32)
#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include "internalheap.h"

/**
 * @class xplock
 * @brief A cross-process lock.
 *
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 */

// Use secret hidden APIs to avoid mallocs...

class xplock {
public:

  xplock (void) {
    /// The lock's attributes.
    pthread_mutexattr_t attr;

    // Set up the lock with a shared attribute.
    WRAP(pthread_mutexattr_init) (&attr);
    pthread_mutexattr_setpshared (&attr, PTHREAD_PROCESS_SHARED);

    // Instantiate the lock structure inside a shared mmap.
    _lock = (pthread_mutex_t *)
      (char *) mmap (NULL, xdefines::PageSize, 
		     PROT_READ | PROT_WRITE,
		     MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    WRAP(pthread_mutex_init) (_lock, &attr);
  }

  /// @brief Lock the lock.
  void lock() {
    WRAP(pthread_mutex_lock) (_lock);
  }

  /// @brief Unlock the lock.
  void unlock() {
    WRAP(pthread_mutex_unlock)(_lock);
  }

private:

  /// A pointer to the lock.
  pthread_mutex_t * _lock;
};

#endif
