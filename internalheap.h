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

#ifndef _INTERNALHEAP_H_
#define _INTERNALHEAP_H_

#include "xdefines.h"
#include "objectheader.h"
#include "ansiwrapper.h"
#include "kingsleyheap.h"
#include "adapt.h"
#include "sllist.h"
#include "dllist.h"
#include "sanitycheckheap.h"
#include "zoneheap.h"
/**
 * @file InternalHeap.h
 * @brief A share heap for internal allocation needs.
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 *
 */
class SourceInternalHeap {
public:
  SourceInternalHeap (void)
    : _alreadyMalloced (false)
  {}

  void * malloc (size_t) {
    if (_alreadyMalloced) {
      return NULL;
    } else {
      void * start;
#if defined(__SVR4)
      start = WRAP(mmap) (NULL, xdefines::INTERNALHEAP_SIZE, PROT_READ | PROT_WRITE, 
      MAP_SHARED | MAP_ANON, -1, 0);
#else
      // Create a MAP_SHARED memory
      start = WRAP(mmap)(NULL, xdefines::INTERNALHEAP_SIZE, PROT_READ | PROT_WRITE, 
      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
#endif
      if(start == NULL) {
        fprintf(stderr, "Failed to create a internal share heap.\n");
        exit(1);
      }
      
//      fprintf(stderr, "Internal heap start %p to %lx\n", start, (intptr_t)start + xdefines::INTERNALHEAP_SIZE);  
      _alreadyMalloced = true;
      return(start);
    }
  }
  
  void free (void * addr) {}

private:

  bool _alreadyMalloced;  

};

template <class SourceHeap>
class NewAdaptHeap : public SourceHeap {
public:
  void * malloc (size_t sz) {
    void * ptr = SourceHeap::malloc (sz + sizeof(objectHeader));
    if (!ptr) {
      return NULL;
    }

    // There is no need to get callsite information here.
    objectHeader * o = new (ptr) objectHeader (sz);
    void * newptr = getPointer(o);
	
    assert (getSize(newptr) >= sz);
    return newptr;
  }

  void free (void * ptr) {
    SourceHeap::free ((void *) getObject(ptr));
  }

  size_t getSize (void * ptr) {
    objectHeader * o = getObject(ptr);
    size_t sz = o->getSize();
    if (sz == 0) {
      printf ("error!\n");
    }
    return sz;
  }
private:

  static objectHeader * getObject (void * ptr) {
    objectHeader * o = (objectHeader *) ptr;
    return (o - 1);
  }

  static void * getPointer (objectHeader * o) {
    return (void *) (o + 1);
  }
};

class InternalHeap : 
  public 
  HL::ANSIWrapper<
  HL::StrictSegHeap<Kingsley::NUMBINS,
        Kingsley::size2Class,
        Kingsley::class2Size,
        HL::AdaptHeap<HL::SLList, NewAdaptHeap<SourceInternalHeap> >,
        NewAdaptHeap<HL::ZoneHeap<SourceInternalHeap, xdefines::INTERNALHEAP_SIZE> > > >

{
  typedef   HL::ANSIWrapper<
  HL::StrictSegHeap<Kingsley::NUMBINS,
        Kingsley::size2Class,
        Kingsley::class2Size,
        HL::AdaptHeap<HL::SLList, NewAdaptHeap<SourceInternalHeap> >,
        NewAdaptHeap<HL::ZoneHeap<SourceInternalHeap, xdefines::INTERNALHEAP_SIZE> > > >
  SuperHeap;

public:

  InternalHeap()
  {
    pthread_mutexattr_t attr;

    // Set up the lock with a shared attribute.
    WRAP(pthread_mutexattr_init)(&attr);
    pthread_mutexattr_setpshared (&attr, PTHREAD_PROCESS_SHARED);

    // Allocate a lock to use internally
    _lock = new (mmap (NULL, sizeof(pthread_mutex_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0)) pthread_mutex_t;
    if(_lock == NULL) {
      printf("Fail to initialize an internal lock for monitor map\n");
      _exit(-1);
    }

    WRAP(pthread_mutex_init) (_lock, &attr);
    
    // Do an initialized malloc in order to do allocation before spawning.
    malloc(8);
  }
 
  virtual ~InternalHeap (void) {}
  
  // Just one accessor.  Why? We don't want more than one (singleton)
  // and we want access to it neatly encapsulated here, for use by the
  // signal handler.
  static InternalHeap& getInstance (void) {
    static void * buf = WRAP(mmap) (NULL, sizeof(InternalHeap), PROT_READ | PROT_WRITE, 
          MAP_SHARED | MAP_ANON, -1, 0);
    static InternalHeap * theOneTrueObject = new (buf) InternalHeap();
    return *theOneTrueObject;
  }
  
  void * malloc (size_t sz) {
    void * ptr = NULL;
    lock(); 
    ptr = SuperHeap::malloc (sz);
    unlock();
  
    if(!ptr) {
      fprintf(stderr, "%d : SHAREHEAP is exhausted, exit now!!!\n", getpid());
      assert(ptr != NULL);
    }
    //fprintf(stderr, "%d : SHAREHEAP allocate %p with sz %x\n", getpid(), ptr, sz);
  
    return ptr;
  }
  
  void free (void * ptr) {
    lock(); 
    SuperHeap::free (ptr);
    unlock();
  }
  
private:
  
  // The lock is used to protect the update on global _monitors
  void lock(void) {
    WRAP(pthread_mutex_lock) (_lock);
  }
  
  void unlock(void) {
    WRAP(pthread_mutex_unlock) (_lock);
  }
  
  // Internal lock
  pthread_mutex_t * _lock; 
};


class InternalHeapAllocator {
public:
  void * malloc (size_t sz) {
    return InternalHeap::getInstance().malloc(sz);
  }
  
  void free (void * ptr) {
    return InternalHeap::getInstance().free(ptr);
  }
};

#endif
