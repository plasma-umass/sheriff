// g++ -g falsesharing.cpp -rdynamic ../libsheriff_protect64.so
#include <stdio.h>
#include <pthread.h>

#include <iostream>
using namespace std;

enum { MAX_THREADS = 8 };
//enum { NUM_ITERATIONS = 800000000 };
//enum { NUM_ITERATIONS = 800000000 };
enum { NUM_ITERATIONS = 800000000 };
//enum { NUM_ITERATIONS = 80000 };

class Item {
public:
  volatile int x[MAX_THREADS];
};

#define SHARE_HEAP_OBJECT 0
//#define SHARE_HEAP_OBJECT 1

#if SHARE_HEAP_OBJECT
Item * theItem;
#else
Item monkey;
#endif

void * worker (void * v) {
  long index = (long) v;
  fprintf(stderr, "%d: in worker %d\n", getpid(), index);

  for (int i = 0; i < NUM_ITERATIONS; i++) {
#if SHARE_HEAP_OBJECT
    theItem->x[index]++;
#else
    monkey.x[index]++;
#endif
//    if(i % 100 == 0)
//    usleep(1);
  }
  //while(1);
#if SHARE_HEAP_OBJECT
    fprintf(stderr, "%d: in worker %d done with item %d\n", getpid(), index, theItem->x[index]);
#else
    fprintf(stderr, "%d: in worker %d done with item %d\n", getpid(), index, monkey.x[index]);
#endif

  return NULL;
}


int
main()
{
#if SHARE_HEAP_OBJECT
  theItem = new Item;
#endif

  pthread_t thread[MAX_THREADS];

  cout << "Starting threads." << endl;

//  cout << "theItem is at " << (void *) &theItem << endl;
#if SHARE_HEAP_OBJECT
  fprintf(stderr, "theItem is at %p pointing to %p\n", &theItem, theItem);
#else 
  fprintf(stderr, "theItem is at %p\n", &monkey);
#endif
  for (int i = 0; i < 8; i++) {
#if SHARE_HEAP_OBJECT
    fprintf(stderr, "theItem[%d] is  %lx\n", i, theItem->x[i]);
#else
    fprintf(stderr, "theItem[%d] is  %d\n", i, monkey.x[i]);
#endif
  }

  for (int i = 0; i < 8; i++) {
    pthread_create (&thread[i], NULL, worker, (void *) i);
  }
 
  for (int i = 0; i < 8; i++) {
    pthread_join (thread[i], NULL);
  }

  cout << "Done." << endl;

  for (int i = 0; i < 8; i++) {
#if SHARE_HEAP_OBJECT
    fprintf(stderr, "theItem[%d] is  %lx\n", i, theItem->x[i]);
#else
    fprintf(stderr, "theItem[%d] is  %d\n", i, monkey.x[i]);
#endif
  }
 
  return 0;
}
