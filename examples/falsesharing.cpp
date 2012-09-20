// g++ -g falsesharing.cpp -rdynamic ../libsheriff_protect64.so

#include <pthread.h>

#include <iostream>
using namespace std;

enum { MAX_THREADS = 256 };
enum { NUM_ITERATIONS = 80000000 };

class Item {
public:
  volatile int x[MAX_THREADS];
};

#define SHARE_HEAP_OBJECT 1

#if SHARE_HEAP_OBJECT
Item * theItem;
#else
Item monkey;
#endif

void * worker (void * v) {
  long index = (long) v;
  for (int i = 0; i < NUM_ITERATIONS; i++) {
#if SHARE_HEAP_OBJECT
    theItem->x[index]++;
#else
    monkey.x[index]++;
#endif
  }
  return NULL;
}


int
main()
{
  theItem = new Item;

  pthread_t thread[MAX_THREADS];

  cout << "Starting threads." << endl;
  cout << "theItem is at " << (void *) &theItem << endl;

  for (int i = 0; i < 8; i++) {
    pthread_create (&thread[i], NULL, worker, (void *) i);
  }
 
  for (int i = 0; i < 8; i++) {
    pthread_join (thread[i], NULL);
  }

  cout << "Done." << endl;
 
  return 0;
}
