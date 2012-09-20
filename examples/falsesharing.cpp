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

Item monkey;

Item * theItem = &monkey;

void * worker (void * v) {
  long index = (long) v;
  for (int i = 0; i < NUM_ITERATIONS; i++) {
    monkey.x[index]++;
    //    theItem->x[index]++;
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
