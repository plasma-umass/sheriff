// -*- C++ -*-

#ifndef SHERIFF_XPERSIST_H
#define SHERIFF_XPERSIST_H

#include <set>
#include <list>
#include <vector>

#if !defined(_WIN32)
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "mm.h"
#include "wordchangeinfo.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

#include "atomic.h"
#include "ansiwrapper.h"
#include "freelistheap.h"

#include "stlallocator.h"
#include "privateheap.h"
#include "xplock.h"
#include "xdefines.h"
#include "xpageentry.h"
#include "xpagestore.h"

#ifdef GET_CHARACTERISTICS
#include "xpageprof.h"
#endif


#include "xtracker.h"
#include "xheapcleanup.h"
#include "stats.h"

#if defined(sun)
extern "C" int madvise(caddr_t addr, size_t len, int advice);
#endif

/**
 * @class xpersist
 * @brief Makes a range of memory persistent and consistent.
 *
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 */
template <class Type,
   unsigned long NElts = 1>
class xpersist {
public:
  typedef std::pair<int, void *> objType;
  typedef HL::STLAllocator<objType, privateheap> dirtyListTypeAllocator;
  typedef std::less<int> localComparator;
  /// A map of pointers to objects and their allocated sizes.
  typedef std::map<int, void *, localComparator, dirtyListTypeAllocator> dirtyListType;

  /// @arg startaddr  the optional starting address of local memory.
  /// @arg startsize  the optional size of local memory.
  xpersist (void * startaddr = 0, 
	    size_t startsize = 0)
    : _startaddr (startaddr),
      _startsize (startsize)
  {
    if (_startsize > 0) {
      if (_startsize > NElts * sizeof(Type)) {
        fprintf (stderr, "This persistent region (%d) is too small (%d).\n",
		 NElts * sizeof(Type), _startsize);
        ::abort();
      }
    }
    
    // Get a temporary file name (which had better not be NFS-mounted...).
    char _backingFname[L_tmpnam];
    sprintf (_backingFname, "/tmp/sheriff-backing-XXXXXX");
    _backingFd = mkstemp (_backingFname);
    if (_backingFd == -1) {
      fprintf (stderr, "Failed to make persistent file.\n");
      ::abort();
    }

    // Set the files to the sizes of the desired object.
    if (ftruncate (_backingFd,  NElts * sizeof(Type))) { 
      fprintf (stderr, "Mysterious error with ftruncate.\n");
      ::abort();
    }

    // Get rid of the files when we exit.
    unlink (_backingFname);

    //
    // Establish two maps to the backing file.
    //
    // The persistent map is shared.
    _persistentMemory
      = (Type *) MM::allocateShared (NElts * sizeof(Type), _backingFd);

    if (_persistentMemory == MAP_FAILED) {
      fprintf (stderr, "Failed to allocate memory (%u).\n", NElts * sizeof(Type));
      ::abort();
    }

    // If we specified a start address (globals), copy the contents into the
    // persistent area now because the transient memory map is going
    // to squash it.
    if (_startaddr) {
      memcpy (_persistentMemory, _startaddr, _startsize);
      _startsize = (_startsize / xdefines::PageSize + 1) * xdefines::PageSize;
      _isHeap = false;
    }
    else {
      _isHeap = true;
    }
  
    // The transient map is optionally fixed at the desired start
    // address. If globals, then startaddr is not zero.
    _transientMemory
      = (Type *) MM::allocateShared (NElts * sizeof(Type), _backingFd, startaddr);

    _isProtected = false;
  
#ifndef NDEBUG
    fprintf (stderr, "transient = %p, persistent = %p, size = %lx\n", _transientMemory, _persistentMemory, NElts * sizeof(Type));
#endif
   // fprintf (stderr, "transient = %p, persistent = %p\n", _transientMemory, _persistentMemory);

    _cacheLastthread = (unsigned long *)
      MM::allocateShared (TotalCacheNums * sizeof(unsigned long));
  
    _cacheInvalidates = (unsigned long *)
      MM::allocateShared (TotalCacheNums * sizeof(unsigned long));

    // How many users can be in the same page. We only start to keep track of 
    // wordChanges when there are multiple user in the same page.
    _pageUsers = (unsigned long *)
      MM::allocateShared (TotalPageNums * sizeof(unsigned long));

    // This is used to save all wordchange information about all words.
    // Here, we are trying to allocate the same size as transientMemory.
    // But they won't actually use that much of physical memory. 
    _wordChanges = (wordchangeinfo *)
      MM::allocateShared (NElts * sizeof(Type));

    if ((_transientMemory == MAP_FAILED) ||
	      (_persistentMemory == MAP_FAILED) ) {
      fprintf(stderr, "mmap error with %s\n", strerror(errno));
      // If we couldn't map it, something has seriously gone wrong. Bail.
      ::abort();
    }
  
    if(_isHeap) {
      xheapcleanup::getInstance().storeProtectHeapInfo
	                ((void *)_transientMemory, size(),
	                (void *)_cacheInvalidates, (void *)_cacheLastthread, (void *)_wordChanges);
    }

#ifdef SSE_SUPPORT
    // A string of one bits.
    allones = _mm_setzero_si128();
    allones = _mm_cmpeq_epi32(allones, allones);
#endif 
  }

  virtual ~xpersist (void) {
#if 0
    close (_backingFd);
    // Unmap everything.
    munmap (_transientMemory,  NElts * sizeof(Type));
    munmap (_persistentMemory, NElts * sizeof(Type));
#endif
  }

  void initialize(void) {
    _privatePagesList.clear();
    _savedPagesList.clear();
  }

  void printScopeInformation(unsigned long begin, unsigned long end) {
    unsigned long offset = begin - (intptr_t)base();
    int startCacheNo = offset/xdefines::CACHE_LINE_SIZE;
    int cacheLines = (end - begin)/xdefines::CACHE_LINE_SIZE;
    //if(cacheLines < 1)
    cacheLines += 1;

    wordchangeinfo * wordStart = (wordchangeinfo *)((intptr_t)_wordChanges + offset);

    fprintf(stderr, "Printing word changes from %lx to %lx, cachelines %d, startCacheNo %d\n", begin, end, cacheLines, startCacheNo);
    for(int i = 0; i < cacheLines; i++) {
      // Caculate the first 
      wordchangeinfo * word = (wordchangeinfo *)((intptr_t)wordStart + i * xdefines::CACHE_LINE_SIZE);
      int cacheNo = startCacheNo+i;
      if(_cacheInvalidates[cacheNo] > 0) {
        fprintf(stderr, "addr %lx: changes %d times by thread %d. Cache invalidates %d with cacheNo %d\n", begin + xdefines::CACHE_LINE_SIZE * i, word->version, word->tid, _cacheInvalidates[cacheNo], cacheNo);
       
        // We may print specific word information in this cacheline
        if(_cacheInvalidates[cacheNo] > 1) {
          int j;

          for(j = 0; j < (xdefines::CACHE_LINE_SIZE/sizeof(int)); j++) {
            fprintf(stderr, "\taddr %lx (now %lx): changes %d times by thread %d\n", begin + xdefines::CACHE_LINE_SIZE * i + j * sizeof(int), *((int *)(begin + xdefines::CACHE_LINE_SIZE * i + j * sizeof(int))), word[j].version, word[j].tid);
          }
        }
      }
    }
  }

  void finalize(void *end) {
 
    // Trying to print out some information about this block of memory.
    // Only for debugging purpose. 
    if(_isHeap) {
#if !defined(X86_32BIT) 
      //printScopeInformation(0x2aacbee54d80, 0x2aacbee54f80);
     // printScopeInformation(0x2aacbee54140, 0x2aacbee54340);
      //printScopeInformation(0x2aacbee538f8, 0x2aacbee54698);
#endif
      //printScopeInformation(0x941fd000, 0x941fd040);
     }
     else {
#if !defined(X86_32BIT) 
    //  printScopeInformation(0x804aac0, 0x804ab00);
#endif
     }

    if(!_isHeap) {
      _tracker.checkGlobalObjects(_cacheInvalidates, (int *)base(), size(), _wordChanges); 
    }
    else {
      _tracker.checkHeapObjects(_cacheInvalidates, (int *)base(), (int *)end, _wordChanges);  
    }

    // printf those object information.
    if(_isHeap) {
      _tracker.print_objects_info();
    }
  }

  void sharemem_write_word(void * addr, unsigned long val) {
    unsigned long offset = (intptr_t)addr - (intptr_t)base();
    *((unsigned long *)((intptr_t)_persistentMemory + offset)) = val;
    return;
  }

  unsigned long sharemem_read_word(void * addr) {
    unsigned long offset = (intptr_t)addr - (intptr_t)base();
    return *((unsigned long *)((intptr_t)_persistentMemory + offset));
  }


  /**
   * We need to change mapping for the transient mapping,
   * thus, all following functions are working on _backingFd and 
   * all are working on specified address .
   */
  void * changeMappingToShared(int protInfo, void * start, size_t sz) {
    int  offset = (intptr_t)start - (intptr_t)base();
    return changeMapping(true, protInfo, start, sz, offset);
  }

  void * changeMappingToPrivate(int protInfo, void * start, size_t sz) {
    int  offset = (intptr_t)start - (intptr_t)base();
    return changeMapping(false, protInfo, start, sz, offset);
  }
  
  void * changeMapping (bool isShared, int protInfo, void * start,
            size_t sz, int offset)
  {
    int sharedInfo = isShared ? MAP_SHARED : MAP_PRIVATE;
    sharedInfo     |= MAP_FIXED ;
   
    return mmap (start, sz, protInfo,
                 sharedInfo, _backingFd, offset);
  }
  
  void* mmapRdPrivate(void * start, size_t size) {
    void * ptr;
    // Map to readonly private area.
    ptr = changeMappingToPrivate(PROT_READ, start, size); 
    if(ptr == MAP_FAILED) {
      fprintf(stderr, "Weird, %d can't map to read and private area\n", getpid());
      exit(-1);
    }
    return ptr;
  }

  // Set a page to be read-only but shared
  void * mmapRdShared(int pageNo) {
    void * start = (void *)((intptr_t)base() + pageNo * xdefines::PageSize);
    void * ptr;

    // Map to writable share area. 
    ptr = changeMappingToShared(PROT_READ, start, size); 
    if(ptr == MAP_FAILED) {
      fprintf(stderr, "Weird, %d remove protect failed!!!\n", getpid());
      exit(-1);
    }
    return (ptr);
  }

  // Set a page to be read-only but shared
  void * mmapRdShared(void * start) {
    void * ptr;

    // Map to writable share area. 
    ptr = changeMappingToShared(PROT_READ, start, size); 
    if(ptr == MAP_FAILED) {
      fprintf(stderr, "Weird, %d remove protect failed!!!\n", getpid());
      exit(-1);
    }
    return (ptr);
  }

  /// Set a block of memory to Readable/Writable and shared. 
  void *mmapRwShared(void * start, size_t size) {
    void * ptr;

    // Map to writable share area. 
    ptr = changeMappingToShared(PROT_READ|PROT_WRITE, start, size); 
    if(ptr == MAP_FAILED) {
      fprintf(stderr, "Weird, %d remove protect failed!!!\n", getpid());
      exit(-1);
    }
    return (ptr);
  }

  // We set the attribute to Private and Readable
  void openProtection (void) {
    mmapRdPrivate(base(), size());
    _isProtected = true;
  }

  void closeProtection(void) {
    mmapRwShared(base(), size());
    _isProtected = false;
  }
  
  int getDirtyPages(void) {
    return _privatePagesList.size();
  }
 
  // Cleanup those counter information about one heap object when one object is re-used.
  bool cleanupHeapObject(void * ptr, size_t sz) {
    int offset;
    int cachelines;
    int index;
  
    assert(_isHeap == true);
  
    if(inRange(ptr) == false) {
      return false;
    }
    
    offset = (int)ptr - (int)base();
    index = offset/xdefines::CACHE_LINE_SIZE;
  
    // At least we will check one cache line.
    cachelines = sz/xdefines::CACHE_LINE_SIZE;  
    if(cachelines == 0)
      cachelines = 1;
    
    // Cleanup the cacheinvalidates that are involved in this object.
    for(int i = index; i < index+cachelines; i++) {
      if(_cacheInvalidates[i] >= xdefines::MIN_INVALIDATES_CARE) {
        return false;
      }
      // We don't need atomic operation here.
      _cacheInvalidates[i] = 0; 
    } 
  
    // Cleanup corresponding wordChanges information.
    void * wordptr = (void *)((intptr_t)_wordChanges + offset);
    memset(wordptr, 0, sz);
  
    return true;
  }

  /// @return true iff the address is in this space.
  inline bool inRange (void * addr) {
    if (((size_t) addr >= (size_t) base())
      && ((size_t) addr < (size_t) base() + size())) {
      return true;
    } else {
      return false;
    }
  }

  /// @return the start of the memory region being managed.
  inline Type * base (void) const {
    return _transientMemory;
  }

  /// @return the size in bytes of the underlying object.
  inline size_t size (void) const {
    if(_isHeap) 
      return NElts * sizeof(Type);
    else
      return _startsize;
  }

  inline void addPageEntry(int pageNo, struct pageinfo * curPage, dirtyListType * pageList) {
    pair<dirtyListType::iterator, bool> it;
      
    it = pageList->insert(pair<int, void *>(pageNo, curPage));
    if(it.second == false) {
      // The element is existing in the list now.
      memcpy((void *)it.first->second, curPage, sizeof(struct pageinfo));
    }
    return;                                                                       
  }

  /// @brief Handle the write operation on a page.
  /// For detection, we will try to get a twin page.
  void handleWrite (void * addr) {
    // Compute the page that holds this address.
    unsigned long * pageStart = (unsigned long *) (((intptr_t) addr) & ~(xdefines::PAGE_SIZE_MASK));

    // Unprotect the page and record the write.
    mprotect ((char *) pageStart, xdefines::PageSize, PROT_READ | PROT_WRITE);
  
    // Compute the page number of this item
    int pageNo = computePage ((size_t) addr - (size_t) base());

    //printf("handlePAGEWRITE: addr %p pageNO %d\n", addr, pageNo);
 
    // Get an entry from page store.
    struct pageinfo * curPage = xpageentry::getInstance().alloc();
    curPage->pageNo = pageNo;
    curPage->pageStart = (void *)pageStart;
    curPage->alloced = false;

    // Force the copy-on-write of kernel by writing to this address directly
    // Using assemly language here to avoid the code to be optimized.
    // That is, pageStart[0] = pageStart[0] can be optimized to "nop"
 #if defined(X86_32BIT)
    asm volatile ("movl %0, %1 \n\t"
                  :   // Output, no output 
                  : "r"(pageStart[0]),  // Input 
                    "m"(pageStart[0])
                  : "memory");
 #else
    asm volatile ("movq %0, %1 \n\t"
                  :   // Output, no output 
                  : "r"(pageStart[0]),  // Input 
                    "m"(pageStart[0])
                  : "memory");
  #endif

    // Create the "origTwinPage" from the transient page.
    memcpy(curPage->origTwinPage, pageStart, xdefines::PageSize);

    // We will update the users of this page.
    int origUsers = atomic::increment_and_return(&_pageUsers[pageNo]);
    if(origUsers != 0) {
      // Set this page to be shared by current thread.
      // But we don't need to allocate temporary twin page for this page
      // if current transaction is too short, then there is no need to do that.
      curPage->shared = true;
    }
    else {
      curPage->shared = false;
    }

    // Add this entry to dirtiedPagesList.
    addPageEntry(pageNo, curPage, &_privatePagesList);
  }

  inline void allocResourcesForSharedPage(struct pageinfo * pageinfo) {
    // Alloc those resources for share page.
    pageinfo->wordChanges = (unsigned long *)xpagestore::getInstance().alloc();
    pageinfo->tempTwinPage = xpagestore::getInstance().alloc();
    
    // Cleanup all word change information about one page
    memset(pageinfo->wordChanges, 0, xdefines::PageSize);
    pageinfo->alloced = true;
  }

  // In the periodically checking, we are trying to check all dirty pages  
  inline void periodicCheck(void) {
    struct pageinfo * pageinfo;
    int pageNo;
    bool createTempPage = false;

    for (dirtyListType::iterator i = _privatePagesList.begin(); i != _privatePagesList.end(); i++) {
      pageinfo = (struct pageinfo *)i->second;
      pageNo = pageinfo->pageNo;

      // If the original page is not shared, now we can check again.
      if(pageinfo->shared != true) {
        // Check whether one un-shared page becomes shared now?
        int curUsers = atomic::atomic_read(&_pageUsers[pageNo]);
        if(curUsers == 1) {
          // We don't care those un-shared page.
          continue;
        }
        else {
          pageinfo->shared = true; 
        }
      }

      //printf("%d: period checking on pageNo %d\n", getpid(), pageNo);

      // now all pages should be shared.
      assert(pageinfo->shared == true);
      
      if(pageinfo->shared == true) {
        // Check whether we have allocated the temporary page or not.
        if(pageinfo->alloced == false) {
          allocResourcesForSharedPage(pageinfo);

          // Create the temporary page by copying from the working version.
          // Create the temporary twin page from the local page.
          memcpy(pageinfo->tempTwinPage, pageinfo->origTwinPage, xdefines::PageSize);
        }

        // We will try to record changes for those shared pages
        recordChangesAndUpdate(pageinfo);
      }
    }
  }

  inline int recordCacheInvalidates(int pageNo, int cacheNo) {
    int myTid = getpid();
    int lastTid;
    int interleaving = 0;

    // Try to check the global array about cache last thread id.
    lastTid = atomic::exchange(&_cacheLastthread[cacheNo], myTid);

    //if(cacheNo == 4195014)
    //fprintf(stderr, "%d: CacheNo %d at %lx: lastTid %d and myTid %d, interleavings %d\n", getpid(), cacheNo, (intptr_t)base() + xdefines::CACHE_LINE_SIZE * cacheNo, lastTid, myTid, _cacheInvalidates[cacheNo]);
    if(lastTid != 0 && lastTid != myTid) {
      // If the last thread to invalidate cache is not current thread, then we will update global
      // counter about invalidate numbers.
      atomic::increment(&_cacheInvalidates[cacheNo]);
    //  fprintf(stderr, "Record cache invalidates %p with interleavings %d cacheNo %d\n", &_cacheInvalidates[cacheNo], _cacheInvalidates[cacheNo], cacheNo);
      interleaving = 1;
    }
    //fprintf(stderr, "%d :cacheinterleaves %d\n", getpid(), interleaving); 
    return interleaving;
  }
  
  // Record changes for those shared pages and update those temporary pages, 
  // This is done only in the periodic checking phase.
  inline void recordChangesAndUpdate(struct pageinfo * pageinfo) {
    int myTid = getpid();
    int * local = (int *)pageinfo->pageStart;
    //printf("%d: before record on pageNo %d createTempPage %d\n", getpid(), pageinfo->pageNo, createTempPage);
      
    int * twin = (int *)pageinfo->tempTwinPage;
    
    int * wordChanges;
    int interWrites = 0;
    wordChanges = (int *)pageinfo->wordChanges;
  
    // We will check those modifications by comparing "local" and "twin".
    int cacheNo;
    int startCacheNo = pageinfo->pageNo*xdefines::CACHES_PER_PAGE;
    int recordedCacheNo = 0xFFFFFF00;

    for(int i = 0; i < xdefines::PageSize/sizeof(int); i++) {
      if(local[i] != twin[i]) {
        int lastTid;
    
     //   fprintf(stderr, "%d: difference at %p. Local %x twin %x\n", getpid(), &local[i], local[i], twin[i]);
        // Calculate the cache number for current words.  
        cacheNo = calcCacheNo(i);
        
        // We will update corresponding cache invalidates.
        if(cacheNo != recordedCacheNo) {
          //printf("%d: now recordCacheInvalidates pageNo %d cacheNo %d. Address %p\n", getpid(), pageinfo->pageNo, cacheNo, &local[i]);
//          fprintf(stderr, "%d: now recordCacheInvalidates pageNo %d cacheNo %d. Address %p\n", getpid(), pageinfo->pageNo, cacheNo, &local[i]);
          recordCacheInvalidates(pageinfo->pageNo, 
                       pageinfo->pageNo*xdefines::CACHES_PER_PAGE + cacheNo);
          recordedCacheNo = cacheNo;
        }
        
        // Update words on twin page if we are comparing against temporary twin page.
        // We can't update the original twin page!!! That is a bug.
        twin[i] = local[i];
       
        // Record changes for words in this cache line.
        wordChanges[i]++; 
      }   
    }
  }

  /// @brief Start a transaction.
  inline void begin (void) {
    updateAll();
  }

  // Use vectorization to improve the performance if we can.
  inline void commitPageDiffs (const void * local, const void * twin, int pageNo) {
    void * dest = (void *)((intptr_t)_persistentMemory + xdefines::PageSize * pageNo);
  #ifdef SSE_SUPPORT
    __m128i * localbuf = (__m128i *) local;
    __m128i * twinbuf  = (__m128i *) twin;
    __m128i * destbuf  = (__m128i *) dest;
    for (int i = 0; i < xdefines::PageSize / sizeof(__m128i); i++) {
  
      __m128i localChunk, twinChunk, destChunk;
  
      localChunk = _mm_load_si128 (&localbuf[i]);
      twinChunk  = _mm_load_si128 (&twinbuf[i]);
  
      // Compare the local and twin byte-wise.
      __m128i eqChunk = _mm_cmpeq_epi8 (localChunk, twinChunk);
  
      // Invert the bits by XORing them with ones.
      __m128i neqChunk = _mm_xor_si128 (allones, eqChunk);
  
      // Write local pieces into destbuf everywhere diffs.
      _mm_maskmoveu_si128 (localChunk, neqChunk, (char *) &destbuf[i]);
    }
  #else
    /* If hardware can't support SSE3 instructions, use slow commits as following. */
    unsigned long * mylocal = (unsigned long *)local;
    unsigned long * mytwin = (unsigned long *)twin;
    unsigned long * mydest = (unsigned long *)dest;

    for(int i = 0; i < xdefines::PageSize/sizeof(unsigned long); i++) {
      if(mylocal[i] != mytwin[i]) {
          //if(mytwin[i] != mydest[i] && mylocal[i] != mydest[i])
          //fprintf(stderr, "%d: RACE at %p from %x to %x (dest %x). pageno %d\n", getpid(), &mylocal[i], mytwin[i], mylocal[i], mydest[i], pageno);
    //    mydest[i] = mylocal[i];
        checkCommitWord(mylocal, mytwin, mydest);
      }
    }

  #endif
  }

  inline void checkCommitWord(char * local, char * twin, char * share) {
    int i = 0;
   // fprintf(stderr, "%d: local %lx twin %lx share %lx\n", getpid(), *((unsigned long *)local), *((unsigned long *)twin), *((unsigned long *)share));
    while(i < sizeof(unsigned long)) {
      if(local[i] != twin[i]) {
        share[i] = local[i];
      }
      i++;
    }
   // fprintf(stderr, "after commit %d (at %p): local %lx share %lx\n", getpid(), local, *((unsigned long *)local), *((unsigned long *)share));
  }

  inline void recordWordChanges(void * addr, int changes) {
    wordchangeinfo * word = (wordchangeinfo *)addr;
    unsigned short tid = word->tid;
    unsigned long wordAddr = ((intptr_t)addr - (intptr_t)_wordChanges)+(intptr_t)base();  

    int mine = getpid();
  
    // If this word is not shared, we should set to current thread.
    if(tid == 0) {
      word->tid = mine;
      word->version = 0;
    }
    else if (tid != 0 && tid != mine && tid != 0xFFFF) {
      // This word is shared by different threads. Set to 0xFFFF.
      word->tid = 0xFFFF;
    }
  
    word->version += changes;
  }

  int calcCacheNo(int words) {
    return (words * sizeof(unsigned int))/xdefines::CACHE_LINE_SIZE;
  }

  // Normal commit procedure. All local modifications should be commmitted to the shared mapping so
  // that other threads can see this change. 
  // Also, all wordChanges has be integrated to the global place too.
  inline void checkcommitpage(struct pageinfo * pageinfo) {
    int * twin = (int *) pageinfo->origTwinPage;
    int * local = (int *) pageinfo->pageStart; 
    int * share = (int *) ((intptr_t)_persistentMemory + xdefines::PageSize * pageinfo->pageNo);
    int * tempTwin = (int *) pageinfo->tempTwinPage;
    int * localChanges = (int *) pageinfo->wordChanges;
    // Here we assume sizeof(unsigned long) == 2 * sizeof(unsigned short);
    int * globalChanges = (int *)((intptr_t)_wordChanges + xdefines::PageSize * pageinfo->pageNo);
    unsigned long recordedCacheNo = 0xFFFFFF00;
    unsigned long cacheNo;
    unsigned long interWrites = 0;

    //fprintf(stderr, "%d: pageStart %p twin %p\n", getpid(), local, twin);
    // Now we have the temporary twin page and original twin page.
    // We always commit those changes against the original twin page.
    // But we need to capture the changes since last period by checking against 
    // the temporary twin page.  
    for (int i = 0; i < xdefines::PageSize/sizeof(int); i++) {
      if(local[i] == twin[i]) {
        if(localChanges[i] != 0) {
          //fprintf(stderr, "detect the ABA changes %d, local %x temptwin %x\n", localChanges[i], local[i], tempTwin[i]);
          recordWordChanges((void *)&globalChanges[i], localChanges[i]);
        }
        // It is very unlikely that we have ABA changes, so we don't check
        // against temporary twin page now.
        continue;
      }
      unsigned long wordAddr = (intptr_t)&local[i];

      // Now there are some changes, at least we must commit the word.
      if(local[i] != tempTwin[i]) {
        // Calculate the cache number for current words.    
        cacheNo = calcCacheNo(i);

        // We will update corresponding cache invalidates.
        if(cacheNo != recordedCacheNo) {
          recordCacheInvalidates(pageinfo->pageNo, 
                          pageinfo->pageNo*xdefines::CACHES_PER_PAGE + cacheNo);

          recordedCacheNo = cacheNo;
        }
       
    //    fprintf(stderr, "Check and commit changes at %p, with localChanges %d, now changed too\n", &local[i], localChanges[i]);
        recordWordChanges((void *)&globalChanges[i], localChanges[i] + 1);
      }
      else {
     //   fprintf(stderr, "%d: Check and commit changes at %p, with localChanges %d\n", getpid(), &local[i], localChanges[i]);
        recordWordChanges((void *)&globalChanges[i], localChanges[i]);
      }

      // Now we are doing a byte-by-byte based commit
      checkCommitWord((char *)&local[i], (char *)&twin[i], (char *)&share[i]);
    }
  }

  // Update those continuous pages.
  inline void updateBatchedPages(int batchedPages, void * batchedStart) {
    updatePages(batchedStart, batchedPages * xdefines::PageSize);
  }

  // Update all pages in the beginning of each transaction.
  // By postpone those page updates, we can possibly improve the 
  // parallelism for those critical sections. Now updateAll
  // are done outside the critical section.
  void updateAll(void) {
    // Don't need to commit a page if no pages in the writeset.
    if(_privatePagesList.size() == 0) {
      return;
    }

    // We are trying to batch those system calls
    int    pageNo;
    void * batchedStart = NULL;
    int    batchedPages = 0;
    
    // We are setting lastpage to a impossible page no at first.
    unsigned int lastpage = 0xFFFFFF00;
    struct pageinfo * pageinfo;

    // Check every pages in the private pages list.
    for (dirtyListType::iterator i = _privatePagesList.begin(); i != _privatePagesList.end(); ++i) {
      pageNo = i->first;
      pageinfo = (struct pageinfo *)i->second;
    
    //  fprintf(stderr, "Inside the loop with pageNo %d\n", pageNo); 
      // Check whether current page is continuous with previous page. 
      if(pageNo == lastpage + 1) {
        batchedPages++;
      }
      else {
        if(batchedPages > 0) {
          // We will update batched pages together.
          // By doing this, we may save the overhead of system calls (madvise and mprotect).  
          updateBatchedPages(batchedPages, batchedStart);
        }

        batchedPages = 1;
        batchedStart = pageinfo->pageStart;
     //   fprintf(stderr, "Now pageNo is %d and batchedStart is %p\n", pageNo, batchedStart);
        // Now we set last page to current pageNo.
        lastpage = pageNo;
      }
    }

    // Now we have to commit again since the last part won't be committed.      
    if(batchedPages > 0) {
      updateBatchedPages(batchedPages, batchedStart);
    }

    //fprintf(stderr, "COMMIT-BEGIN at process %d\n", getpid());
    // Now we already finish the commit, let's cleanup the list. 
    // For every transaction, we will restart to capture those writeset
    _privatePagesList.clear();

    // Clean up those page entries.
    xpageentry::getInstance().cleanup();
    xpagestore::getInstance().cleanup();
  }

  // Commit those pages in the end of each transaction. 
  inline void commit(bool doChecking) {
    // Don't need to commit a page if no pages in the writeset.
    if(_privatePagesList.size() == 0) {
      return;
    }

    // Commit those private pages. 
    struct pageinfo * pageinfo = NULL;
    int    pageNo;
    // Check every pages in the private pages list.
    for (dirtyListType::iterator i = _privatePagesList.begin(); i != _privatePagesList.end(); ++i) {
      pageinfo = (struct pageinfo *)i->second;
      pageNo = pageinfo->pageNo;
 
     // fprintf(stderr, "COMMIT: %d on page %d (at %p) on heap %d\n", getpid(), pageNo, pageinfo->pageStart, _isHeap);
 
      // If a page is shared and there are some wordChanges information,
      // We should commit the changes and update wordChanges information too.
      //if((pageinfo->shared == true) && (pageinfo->alloced == true)) { 
      if(pageinfo->alloced == true) { 
        checkcommitpage(pageinfo);
      //  fprintf(stderr, "%d COMMIT: finish on page %d********\n", getpid(), pageNo);
      }
      else {
        // Commit those changes by checking the original twin page.
        commitPageDiffs(pageinfo->pageStart, pageinfo->origTwinPage, pageNo);
      }
    }
  //  fprintf(stderr, "COMMIT: %d finish commits on heap %d\n", getpid(), _isHeap);
  }

  /// @brief Commit all writes.
  inline void memoryBarrier (void) {
    atomic::memoryBarrier();
  }

private:

  //inline int computePage (int index) {
  inline int computePage (size_t index) {
    return (index * sizeof(Type)) / xdefines::PageSize;
  }

  /// @brief Update the given page frame from the backing file.
  void updatePages (void * local, int size) {
    madvise (local, size, MADV_DONTNEED);

  //  fprintf(stderr, "%d: protect page %p size %d\n", getpid(), local, size);
    // Set this page to PROT_READ again.
    mprotect (local, size, PROT_READ);
  }
 
  /// True if current xpersist.h is a heap.
  bool _isHeap;

  /// The starting address of the region.
  void * const _startaddr;

  /// The size of the region.
  size_t _startsize;

  /// A map of dirtied pages.
  dirtyListType _privatePagesList;

  dirtyListType _savedPagesList;
  /// The file descriptor for the backing store.
  int _backingFd;

  /// The transient (not yet backed) memory.
  Type * _transientMemory;

  /// The persistent (backed to disk) memory.
  Type * _persistentMemory;

  bool _isProtected;
  
  /// The file descriptor for the versions.
  int _versionsFd;

  //unsigned long * _globalSharedInfo;
  bool * _globalSharedInfo;
  bool * _localSharedInfo;

 
  /// The length of the version array.
  enum { TotalPageNums = NElts * sizeof(Type)/(xdefines::PageSize) };
  enum { TotalCacheNums = NElts * sizeof(Type)/(xdefines::CACHE_LINE_SIZE) };

  unsigned long * _cacheInvalidates;

  // Last thread to modify current cache
  unsigned long * _cacheLastthread;

#if defined(SSE_SUPPORT)
  // A string of one bits.
  __m128i allones;
#endif
  
  // In order to save space, we will use the higher 16 bit to store the thread id
  // and use the lower 16 bit to store versions.
  wordchangeinfo * _wordChanges;

  // Keeping track of whether multiple users are on the same page.
  // If no multiple users simultaneously, then there is no need to check the word information, 
  // thus we don't need to pay additional physical pages on _wordChanges since
  // _wordChanges will double the physical pages's usage.
  unsigned long * _pageUsers;
 
  xtracker<NElts> _tracker;
};

#endif
