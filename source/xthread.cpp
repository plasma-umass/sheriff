#include <pthread.h>
#include <syscall.h>
#include "xthread.h"
#include "xrun.h"

void * xthread::spawn (xrun * runner,
		       threadFunction * fn,
		       void * arg)
{

	if(!_protected) {
		runner->openMemoryProtection();
  	runner->atomicBegin(false, false);
		_protected = true;
	}
    
	runner->atomicEnd(true, false);
 
  // Allocate an object to hold the thread's return value.
  void * buf = allocateSharedObject (4096);
  HL::sassert<(4096 > sizeof(ThreadStatus))> checkSize;
  ThreadStatus * t = new (buf) ThreadStatus;

	runner->atomicBegin(false, false);
  return forkSpawn (runner, fn, t, arg);
}


/// @brief Do pthread_join.
void xthread::join (xrun * runner,
            void * v,
            void ** result)
{

  // Return immediately if the thread argument is NULL.
  if (v == NULL) {
    return;
  }

  ThreadStatus * t = (ThreadStatus *) v;
 
  runner->atomicEnd(true, false);
//  fprintf(stderr, "%d: joining thread %d\n", getpid(), t->tid);
  
  // FIXME: I should wait until the child thread has been finished.
  int status;
  waitpid(t->tid, &status, 0);

  runner->atomicBegin(false, false);
#if 0
  while(!WIFEXITED(status)) {
  //  fprintf(stderr, "%d: Now waitAGAIN!!!!!\n", getpid());
     waitpid(t->tid, &status, 0);
  } 
#endif
 
  // Grab the thread result from the status structure (set by the thread),
  // reclaim the memory, and return that result.
  if (result != NULL) {
    *result = t->retval;
  }
  
  // Free the shared object held by this thread.
  freeSharedObject(t, 4096);

  // JOIN means one child is closed here. If no other threads, we can close
  // the memory protection to improve the performance.
  if(getpid() == runner->main_id()) {
	// Check whether main thread is the only alive one. If it is, we maybe don't 
    // need protection anymore.
	  if(waitpid(-1, NULL, WNOHANG) == -1 && errno == ECHILD) {
		  runner->closeMemoryProtection();
		  runner->resetThreadIndex();
		  _protected = false;
		//fprintf(stderr, "return %d and errno %s", ret, strerror(errno));
	    return;
	  }
  }

  runner->atomicBegin(false, false);
}

/// @brief Cancel one thread. We just send out a SIGKILL signal to that thread
void xthread::cancel (xrun * runner, void *v)
{
  ThreadStatus * t = (ThreadStatus *) v;
  //fprintf(stderr, "KILL thread %d\n", t->tid);
  kill(t->tid, SIGKILL); 
  
  // Free the shared object held by this thread.
  freeSharedObject(t, 4096);
}

void xthread::thread_kill (xrun * runner, void *v, int sig)
{ 
  ThreadStatus * t = (ThreadStatus *) v;
  //fprintf(stderr, "KILL thread %d\n", t->tid);
  kill(t->tid, sig); 
}


int forkWithFS (void) {
  return syscall(SYS_clone, CLONE_FS|CLONE_FILES|SIGCHLD, (void*) 0 );
 // return fork();
}


void * xthread::forkSpawn (xrun * runner,
			   threadFunction * fn,
			   ThreadStatus * t,
			   void * arg) 
{
  // Use fork to create the effect of a thread spawn.
  // FIXME:: For current process, we should close share. 
  // children to use MAP_PRIVATE mapping. Or just let child to do that in the beginning.
  int child = forkWithFS();
  
  if (child) {
    // I'm the parent (caller of spawn).

    // Store the tid so I can later sync on this thread.
#ifndef NDEBUG
//	fprintf(stderr, "%d : Creating CHILD %d\n", getpid(), child);
#endif
    t->tid = child;
  
    // Start a new atomic section and return the thread info.
    runner->atomicBegin(true, false);
    return (void *) t;
      
  } else {
    // I'm the spawned child who will run the thread function.
    // Astoundingly, glibc caches getpid(), and that value is invalid
    // now (it always refers to the parent), so we have to explicitly
    // make the system call here. See man clone(2).
    pid_t mypid = syscall(SYS_getpid);
    
    // Set "thread_self".
    setId (mypid);

	  // Register to the system, we will set the heapid for myself.
	  runner->threadRegister();

    // We're in...
    _nestingLevel++;

 // fprintf(stderr, "Create thread %d\n", mypid);
#ifdef MULTITHREAD_SUPPORT
	  // Create those helpers.
	  runner->creatHelpers();
#endif

#ifdef EXCLUSIVE_HEAP_USAGE
	  // Close those parent's share blocks since child need to protect those areas.
    runner->closeParentSharedBlocks();
#endif

    //while(1); 
    // Run the thread...
    run_thread (runner, fn, t, arg);

//	fprintf(stderr, "%d : EXIT thread\n", mypid);
    // and we're out.
    _nestingLevel--;

//	fprintf(stderr, "%d : EXIT thread\n", mypid);
    // And that's the end of this "thread".
    _exit(0);

    // Avoid complaints.
    return NULL;
  }
}

// @brief Execute the thread.
void xthread::run_thread (xrun * runner,
			  threadFunction * fn,
			  ThreadStatus * t,
			  void * arg) 
{
  // Run the thread inside a transaction.
//  fprintf(stderr, "%d : trying to run atomicBegin\n", getpid());
  runner->atomicBegin(true, true);
//  fprintf(stderr, "%d : after atomicBegin and fn %p\n", getpid(), fn);
  void * result = fn (arg);

  runner->atomicEnd(true, false);
  // We're done. Write the return value.
  t->retval = result;
}
