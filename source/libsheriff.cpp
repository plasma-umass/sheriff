// -*- C++ -*-

/*
  Copyright (c) 2011, University of Massachusetts Amherst.

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

/**
 * @file libsheriff.cpp
 * @brief Interface with outside library.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 *
 */

#if !defined(_WIN32)
#include <dlfcn.h>
#endif

#include <stdarg.h>

#include "xrun.h"

extern "C" {

#if defined(__GNUG__)
  void initializer (void) __attribute__((constructor));
  void finalizer (void)   __attribute__((destructor));
#endif
  unsigned long * global_thread_index; 
  int textStart, textEnd; 
 
  static bool initialized = false;
#ifdef GET_CHARACTERISTICS
  int allocTimes = 0;
  int cleanupSize = 0;
#endif
  #define INITIAL_MALLOC_SIZE 81920
  static char * tempalloced = NULL;
  static int remainning = 0;

  void initializer (void) {
    // Using globals to provide allocation
    // before initialized.
    // We can not use stack variable here since different process
    // may use this to share information. 
    static char tempbuf[INITIAL_MALLOC_SIZE];

    // temprary allocation
    tempalloced = (char *)tempbuf;
    remainning = INITIAL_MALLOC_SIZE;

    init_real_functions();

  	global_thread_index = (unsigned long *)mmap(NULL, xdefines::PageSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *global_thread_index = 0;

    xrun::getInstance().initialize();
    initialized = true;
    
    // Start our first transaction.
#ifndef NDEBUG
    //    printf ("we're gonna begin now.\n"); fflush (stdout);
#endif
  }

  void finalizer (void) {
    initialized = false;
    xrun::getInstance().finalize();
  }


  // Temporary mallocation before initlization has been finished.
  static void * tempmalloc(int size) {
    void * ptr = NULL;
    if(remainning < size) {
      // complaining. Tried to set to larger
      printf("Not enough temporary buffer, size %d remainning %d\n", size, remainning);
      exit(-1);
    }
    else {
      ptr = (void *)tempalloced;
      tempalloced += size;
      remainning -= size;
    }
    return ptr;
  }

  /// Functions related to memory management.
  void * sheriff_malloc (size_t sz) {
    void * ptr;
    if (!initialized) {
      ptr = tempmalloc(sz);
    } else {
      ptr = xrun::getInstance().malloc (sz);
    }
    if (ptr == NULL) {
      fprintf (stderr, "Out of memory!\n");
      ::abort();
    }
    return ptr;
  }
  
  void * sheriff_calloc (size_t nmemb, size_t sz) {
    void * ptr;
    
    ptr = sheriff_malloc (nmemb *sz);
	  memset(ptr, 0, sz*nmemb);
    return ptr;
  }

  void sheriff_free (void * ptr) {
    // We donot free any object if it is before 
    // initializaton has been finished to simplify
    // the logic of tempmalloc
    if(initialized) {
     // printf("Now sheriff_free at %p 111\n", ptr);
      xrun::getInstance().free (ptr);
    }
  }

  size_t sheriff_malloc_usable_size(void * ptr) {
    //assert(initialized);
    if(initialized) {
      return xrun::getInstance().getSize(ptr);
    }
    return 0;
  }

  void * sheriff_memalign (size_t boundary, size_t size) {
	  fprintf(stderr, "%u : Sheriff does not support memalign. boundary %d, size %u\n", 
      getpid(), boundary, size);
    ::abort();
    return NULL;
  }

  void * sheriff_realloc (void * ptr, size_t sz) {
    return xrun::getInstance().realloc (ptr, sz);
  }
 
  void * malloc (size_t sz) throw() {
    return sheriff_malloc(sz);
  }

  void * calloc (size_t nmemb, size_t sz) throw() {
    return sheriff_calloc(nmemb, sz);
  }

  void free(void *ptr) throw () {
    sheriff_free(ptr);
  }
  
  void* realloc(void * ptr, size_t sz) {
    return sheriff_realloc(ptr, sz);
  }

  void * memalign(size_t boundary, size_t sz) { 
    return sheriff_memalign(boundary, sz);
  }
  /// Threads's synchronization functions.
  // Mutex related functions 
  int pthread_mutex_init (pthread_mutex_t * mutex, const pthread_mutexattr_t* attr) {    
    if (initialized) 
      return xrun::getInstance().mutex_init (mutex);
    else 
      return 0;
  }
  
  int pthread_mutex_lock (pthread_mutex_t * mutex) {   
    if (initialized) 
      xrun::getInstance().mutex_lock (mutex);

    return 0;
  }

  // FIXME: add support for trylock
  int pthread_mutex_trylock(pthread_mutex_t * mutex) {
    return 0;
  }
  
  int pthread_mutex_unlock (pthread_mutex_t * mutex) {    
    if (initialized) 
      xrun::getInstance().mutex_unlock (mutex);

    return 0;
  }

  int pthread_mutex_destroy (pthread_mutex_t * mutex) {    
    if (initialized) 
      return xrun::getInstance().mutex_destroy (mutex);
    else
      return 0;
  }
  
  // Condition variable related functions 
  int pthread_cond_init (pthread_cond_t * cond, const pthread_condattr_t* condattr)
  {
    if (initialized) 
      xrun::getInstance().cond_init ((void *)cond);
    return 0;
  }

  int pthread_cond_broadcast (pthread_cond_t * cond)
  {
    if (initialized) 
      xrun::getInstance().cond_broadcast ((void *)cond);
    return 0;
  }

  int pthread_cond_signal (pthread_cond_t * cond) {
    if (initialized) 
      xrun::getInstance().cond_signal ((void *)cond);
    return 0;
  }

  int pthread_cond_wait (pthread_cond_t * cond, pthread_mutex_t * mutex) {
    if (initialized) 
      xrun::getInstance().cond_wait ((void *)cond, (void*) mutex);
    return 0;
  }

  int pthread_cond_destroy (pthread_cond_t * cond) {
	if (initialized) 
    	xrun::getInstance().cond_destroy ((void *)cond);
    return 0;
  }

  // Barrier related functions 
  int pthread_barrier_init(pthread_barrier_t  *barrier,  const pthread_barrierattr_t* attr, unsigned int count) {
    if (initialized) 
      return xrun::getInstance().barrier_init (barrier, count);
    else
      return 0;
  }

  int pthread_barrier_destroy(pthread_barrier_t *barrier) {
    if (initialized) 
      return xrun::getInstance().barrier_destroy (barrier);
    else
      return 0;
  }
  
  int pthread_barrier_wait(pthread_barrier_t *barrier) {
    if (initialized) 
      return xrun::getInstance().barrier_wait (barrier);
    else
      return 0;
  }  

  int pthread_cancel (pthread_t thread) {
    xrun::getInstance().cancel((void*)thread);
    return 0;
  }
  
  int sched_yield (void) 
  {
    return 0;
  }

  void pthread_exit (void * value_ptr) {
    _exit (0);
    // FIX ME?
    // This should probably throw a special exception to be caught in spawn.
  }
 
  int getpid(void) {
    return xrun::getInstance().id();
//  return (syscall(SYS_getpid));
  }
 

  int pthread_setconcurrency (int) {
    return 0;
  }

  int pthread_attr_init (pthread_attr_t *) {
    return 0;
  }

  int pthread_attr_destroy (pthread_attr_t *) {
    return 0;
  }

  pthread_t pthread_self (void) 
  {
    return xrun::getInstance().id();
  }

  int pthread_kill (pthread_t thread, int sig) {
    // FIX ME
    xrun::getInstance().thread_kill((void*)thread, sig);
    return 0;
  }

#if 0
  int pthread_rwlock_destroy (pthread_rwlock_t * rwlock) NOTHROW
  {
    return 0;
  }

  int pthread_rwlock_init (pthread_rwlock_t * rwlock,
			   const pthread_rwlockattr_t * attr) NOTHROW
  {
    return 0;
  }

  int pthread_detach (pthread_t thread) NOTHROW
  {
    return 0;
  }

  int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock) NOTHROW
  {
    return 0;
  }

  int pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock) NOTHROW
  {
    return 0;
  }


  int pthread_rwlock_unlock(pthread_rwlock_t *rwlock) NOTHROW
  {
    return 0;
  }

  int pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock) NOTHROW
  {
    return 0;
  }

  int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock) NOTHROW
  {
    return 0;
  }

#endif
  int pthread_attr_getstacksize (const pthread_attr_t *, size_t * s) {
    *s = 1048576UL; // really? FIX ME
    return 0;
  }

  int pthread_mutexattr_destroy (pthread_mutexattr_t *) { return 0; }
  int pthread_mutexattr_init (pthread_mutexattr_t *)    { return 0; }
  int pthread_mutexattr_settype (pthread_mutexattr_t *, int) { return 0; }
  int pthread_mutexattr_gettype (const pthread_mutexattr_t *, int *) { return 0; }
  int pthread_attr_setstacksize (pthread_attr_t *, size_t) { return 0; }

  int pthread_create (pthread_t * tid,
		      const pthread_attr_t * attr,
		      void *(*start_routine) (void *),
		      void * arg) 
  {
    *tid = (pthread_t)xrun::getInstance().spawn (start_routine, arg);
    return 0;
  }

  int pthread_join (pthread_t tid, void ** val) {
    xrun::getInstance().join ((void *)tid, val);
    return 0;
  }

  // Make sure that all pages are readable and writable by issuing writes on them 
  ssize_t read (int fd, void * buf, size_t count) {
    char * start = (char *)buf;
    long pages = count/xdefines::PageSize;

    if(pages >= 1) {
     // fprintf(stderr, "fd %d buf %p count %d\n", fd, buf, count);
    // Trying to read on those pages, thus there won't be a segmenation fault in 
    // the system call.
      for(long i = 0; i < pages; i++) {
        start[i * xdefines::PageSize] = '\0';
      }
    }
    
    // Make sure that last page are written so the page is readable and writable
    start[count-1] = '\0';
  }

#if 0
  ssize_t write (int fd, const void * buf, size_t count) {
    int * start = (int *)buf;
    long pages = (((intptr_t)buf & xdefines::PAGE_SIZE_MASK)+count)/xdefines::PageSize;
    volatile int temp;

  //  fprintf(stderr, "%d: WWWWWWW buf %p, count %d. temp %p. %p\n", getpid(), buf, count, &temp, &start[count/sizeof(int)-1]);

    // Trying to read on those pages, thus there won't be a segmenation fault in 
    // the system call.
    for(long i = 0; i < pages; i++) {
        temp = start[i * 1024];
    }
    temp = start[count/sizeof(int)-1];
    return WRAP(write)(fd, buf, count);
  }
#endif

  void *NOOOOOmmap(void *addr, size_t length, int prot, 
		     int flags,  int fd, off_t offset) {
	  int newflags = flags;

	  if(initialized == true && (flags & MAP_PRIVATE)) {
//		newflags = (flags & ~MAP_PRIVATE) | MAP_SHARED;
		  printf("flags %x and newflags %x\n", flags, newflags);
	  }
	  return WRAP(mmap)(addr, length, prot, newflags, fd, offset);
  }
}


