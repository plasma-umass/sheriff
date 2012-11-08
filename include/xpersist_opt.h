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


#ifdef DETECT_FALSE_SHARING_OPT
#include "xtracker.h"
#include "xheapcleanup.h"
#include "stats.h"
#endif

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

  enum {
    PAGE_TYPE_UPDATE = 0, 
    PAGE_TYPE_PRIVATE, 
    PAGE_TYPE_READONLY,
    PAGE_TYPE_INVALID
  };

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
      = (Type *) MM::allocateShared (NElts * sizeof(Type),
				     _backingFd);

    if (_persistentMemory == MAP_FAILED) {
      char buf[255];
      sprintf (buf, "Failed to allocate memory (%u).\n", NElts * sizeof(Type));
      fprintf (stderr, buf);
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
      if(NElts == xdefines::PROTECTEDHEAP_SIZE)
        _isBasicHeap = true;
      else
        _isBasicHeap = false; 
    }
  
    // The transient map is optionally fixed at the desired start
    // address.

    _transientMemory
      = (Type *) MM::allocateShared (NElts * sizeof(Type),
				     _backingFd,
				     startaddr);

    _isProtected = false;
  
#ifndef NDEBUG
    //fprintf (stderr, "transient = %p, persistent = %p, size = %lx\n", _transientMemory, _persistentMemory, NElts * sizeof(Type));
#endif

    _pageUsers = (unsigned long *)
      MM::allocateShared (TotalPageNums * sizeof(unsigned long));
      
    _cacheLastthread = (unsigned long *)
      MM::allocateShared (TotalCacheNums * sizeof(unsigned long));
  
#if defined(DETECT_FALSE_SHARING_OPT) 
    // Finally, map the version numbers.
    _globalSharedInfo = (bool *)
      MM::allocateShared (TotalPageNums * sizeof(bool));
    
    _localSharedInfo = (bool *)
      MM::allocatePrivate (TotalPageNums * sizeof(bool));

    // We need to preset those shared information.
    // In the beginning, everything are set to NON_SHARED.
    if (_globalSharedInfo == MAP_FAILED || _localSharedInfo == MAP_FAILED) {
      fprintf(stderr, "Failed to initialize shared info.\n");
      ::abort();
    }
//  memset(_globalSharedInfo, 0, TotalPageNums * sizeof(bool));
//  memset(_localSharedInfo, 0, TotalPageNums * sizeof(bool));
    _cacheInvalidates = (unsigned long *)
      MM::allocateShared (TotalCacheNums * sizeof(unsigned long));

    // FIXME: Didn't make sense to have this since most pages are not shared.
    _wordChanges = (wordchangeinfo *)
      MM::allocateShared (TotalWordNums * sizeof(wordchangeinfo));

    if ((_transientMemory == MAP_FAILED) ||
	(_globalSharedInfo == MAP_FAILED) ||
	(_pageUsers == MAP_FAILED) ||
	(_persistentMemory == MAP_FAILED) ) {
      fprintf(stderr, "mmap error with %s\n", strerror(errno));
      // If we couldn't map it, something has seriously gone wrong. Bail.
      ::abort();
    }
  
    if(_isHeap) {
      xheapcleanup::getInstance().storeProtectHeapInfo
	((void *)_transientMemory, 
	 size(),
	 (void *)_cacheInvalidates, 
	 (void *)_cacheLastthread, 
	 (void *)_wordChanges);
    }
#endif
    // A string of one bits.
    allones = _mm_setzero_si128();
    allones = _mm_cmpeq_epi32(allones, allones); 
  }

  virtual ~xpersist (void) {
#if 0
    close (_backingFd);
    // Unmap everything.
    munmap (_transientMemory,  NElts * sizeof(Type));
    munmap (_persistentMemory, NElts * sizeof(Type));
    munmap (_globalSharedInfo, TotalPageNums * sizeof(unsigned long));
    munmap (_pageUsers, TotalPageNums * sizeof(unsigned long));
#endif
  }

  void initialize(void) {
    _privatePagesList.clear();
#ifdef DETECT_FALSE_SHARING_OPT
    _savedPagesList.clear();
#endif
  }

  void finalize(void *end) {
#ifdef GET_CHARACTERISTICS
    _pageprof.finalize((char *)_persistentMemory);
#endif

#ifdef DETECT_FALSE_SHARING_OPT
    closeProtection();
  #ifdef GET_CHARACTERISTICS
    if(_isHeap) 
      fprintf(stderr, "allocTimes %d cleanupSize %d\n", allocTimes, cleanupSize);
  #endif

  #ifdef TRACK_ALL_WRITES
    // We will check those memory writes from the beginning, if one callsite are captured to 
    // have one bigger updates, then report that.
    _tracker.checkWrites((int *)base(), size(),  _wordChanges); 
  #endif

    if(!_isHeap) {
      _tracker.checkGlobalObjects(_cacheInvalidates, (int *)base(), size(), _wordChanges); 
    }
    else {
      _tracker.checkHeapObjects(_cacheInvalidates, (int *)base(), (int *)end, _wordChanges);  
  }

  // printf those object information.
  if(_isBasicHeap) {
    _tracker.print_objects_info();
  }

#ifdef GET_CHARACTERISTICS
  fprintf(stderr, "\n\n Statistics information at heap: %d\n", _isHeap);
  fprintf(stderr, "trans %d, dirtypages %d, protects %d, cachelines %d\n", 
  stats::getInstance().getTrans(), stats::getInstance().getDirtyPages(), stats::getInstance().getProtects(), stats::getInstance().getCaches());
#endif
#endif
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

#ifdef DETECT_FALSE_SHARING_OPT
  void writeProtect(void * start, unsigned long size) {
   //if(_isHeap) //FIXME
     mprotect (start, size, PROT_READ);
    return;
  }

  void removeProtect(void * start, unsigned long size) {
    mprotect (start, size, PROT_READ|PROT_WRITE);
    return;
  }
  
  void* mapRdPrivate(void * start, unsigned long size) {
    void * area;
    int  offset = (intptr_t)start - (intptr_t)base();

    // Map to readonly private area. 
    area = (Type *) mmap (start,
                      size,
                      PROT_READ,
                      MAP_PRIVATE | MAP_FIXED,
                      _backingFd,
                      offset);
    if(area == MAP_FAILED) {
      fprintf(stderr, "Weird, %d writeProtect failed with error %s!!!\n", getpid(), strerror(errno));
      fprintf(stderr, "start %p size %d!!!\n", start, size);
      exit(-1);
    }
    return(area);
  }

  void * setPageRdShared(int pageNo) {
    void * area;
    int offset = pageNo * xdefines::PageSize;
    void * start =(void *)((intptr_t)base() + offset);

    //fprintf(stderr, "%d : remove protect %x size 0x%x, offset 0x%x\n", getpid(), start, size, offset);
    // Map to writable share area. 
    area = (void *) mmap (start,
                    xdefines::PageSize,
                    PROT_READ,
                    MAP_SHARED | MAP_FIXED,
                    _backingFd,
                    offset);

    if(area == MAP_FAILED) {
      fprintf(stderr, "Weird, %d remove protect failed!!!\n", getpid());
      exit(-1);
    }
    return (area);
  }

  void *mapRwShared(void * start, unsigned long size) {
    void * area;
    int  offset = (intptr_t)start - (intptr_t)base();

    //fprintf(stderr, "%d : remove protect %x size 0x%x, offset 0x%x\n", getpid(), start, size, offset);
    // Map to writable share area. 
    area = (Type *) mmap (start,
                    size,
                    PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_FIXED,
                    _backingFd,
                    offset);

    if(area == MAP_FAILED) {
      fprintf(stderr, "Weird, %d remove protect failed!!!\n", getpid());
      exit(-1);
    }
    return (area);
 }
#else
  void *writeProtect(void * start, unsigned long size) {
    void * area;
    int  offset = (intptr_t)start - (intptr_t)base();

    // Map to readonly private area. 
    area = (Type *) mmap (start,
                      size,
                      PROT_READ,
                      MAP_PRIVATE | MAP_FIXED,
                      _backingFd,
                      offset);
    if(area == MAP_FAILED) {
      fprintf(stderr, "Weird, %d writeProtect failed with error %s!!!\n", getpid(), strerror(errno));
      fprintf(stderr, "start %p size %d!!!\n", start, size);
      exit(-1);
    }
    return(area);
  }


  void * removeProtect(void * start, unsigned long size) {
    void * area;
    int  offset = (intptr_t)start - (intptr_t)base();

    // Map to writable share area. 
    area = (Type *) mmap (start,
                    size,
                    PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_FIXED,
                    _backingFd,
                    offset);

    if(area == MAP_FAILED) {
      fprintf(stderr, "Weird, %d remove protect failed!!!\n", getpid());
      exit(-1);
    }
    return (area);
 }
#endif

  void openProtection (void) {
    writeProtect(base(), size());
    _detectPeriod = true;
    _isProtected = true;
  }

  void closeProtection(void) {
    removeProtect(base(), size());
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
   
    // Calculate the offset of this object. 
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
  
    // Cleanup the wordChanges
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
  inline unsigned long size (void) const {
    if(_isHeap) 
      return NElts * sizeof(Type);
    else
      return _startsize;
  }

  inline void addPageEntry(int pageNo, struct pageinfo * curr, dirtyListType * pageList) {
    pair<dirtyListType::iterator, bool> it;
      
    it = pageList->insert(pair<int, void *>(pageNo, curr));
    if(it.second == false) {
      // The element is existing in the list now.
      memcpy((void *)it.first->second, curr, sizeof(struct pageinfo));
    }
    return;                                                                       
  }

  /// @brief Record a write to this location.
  void handleWrite (void * addr) {
    // Compute the page number of this item
    int pageNo = computePage ((size_t) addr - (size_t) base());
    int * pageStart = (int *)((intptr_t)_transientMemory + xdefines::PageSize * pageNo);
    int origUsers = 0;
 
    // Get an entry from page store.
    struct pageinfo * curr = xpageentry::getInstance().alloc();
    curr->pageNo = pageNo;
    curr->pageStart = (void *)pageStart;
    curr->alloced = false;

    // Get current page's version number. 
    // Trick here: we have to get version number before the force of copy-on-write.
    // Getting the old version number is safer than getting of a new version number.
    // Since we use the version number checking to determine whether there is a need to do word-by-word commit.
     
    // Force the copy-on-write of kernel by writing to this address directly
#ifndef DETECT_FALSE_SHARING_OPT
    asm volatile ("movl %0, %1 \n\t"
            :   // Output, no output 
            : "r"(pageStart[0]),  // Input 
              "m"(pageStart[0])
            : "memory");

    // Create the "origTwinPage" from _transientMemory.
    memcpy(curr->origTwinPage, pageStart, xdefines::PageSize);
#else
    if(_localSharedInfo[pageNo] == true) {
      asm volatile ("movl %0, %1 \n\t"
            :   // Output, no output 
            : "r"(pageStart[0]),  // Input 
              "m"(pageStart[0])
            : "memory");

      // Create the "origTwinPage" from _transientMemory.
      memcpy(curr->origTwinPage, pageStart, xdefines::PageSize);
      curr->hasTwinPage = true;
    }
    else {
      curr->hasTwinPage = false;
    }
#endif
    // We will update the users of this page.
    origUsers = atomic::increment_and_return(&_pageUsers[pageNo]);
    if(origUsers != 0) {
      curr->shared = true;
    }
    else {
      curr->shared = false;
    }

    
    // Add this entry to dirtiedPagesList.
    addPageEntry(pageNo, curr, &_privatePagesList);
  }

  inline void allocResourcesForSharePage(struct pageinfo * pageinfo) {
    // Alloc those resources for share page.
    pageinfo->wordChanges = (unsigned long *)xpagestore::getInstance().alloc();
    pageinfo->tempTwinPage = xpagestore::getInstance().alloc();
    
    // Clean these two pages
    memset(pageinfo->wordChanges, 0, xdefines::PageSize);
    pageinfo->alloced = true;
  }

  // In the periodically checking, we are trying to check all dirty pages  
  inline void checkDirtiedPages(void) {
    struct pageinfo * pageinfo;
    int pageNo;

    for (dirtyListType::iterator i = _privatePagesList.begin(); i != _privatePagesList.end(); i++) {
      pageinfo = (struct pageinfo *)i->second;
      pageNo = pageinfo->pageNo;
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

      // Try to record those changes on the shared pages if there is 
      // a private copy, otherwise, the information is incorrect, do
      // nothing now.
      if(_localSharedInfo[pageNo] == true) {
        recordChangesAndUpdate(pageinfo);
      }
  #ifdef DETECT_FALSE_SHARING_OPT
      // We don't have a private copy for this page.
      // BUG here, how we can differentiate this, when the page is touched again,
      // Then one part we have some data.
      // If this page is not touched again, then the twinPage is trash here.
      // How we can avoid that in the future, we don't want to commit this trash to
      // the shared copy.
      else if(_detectPeriod) {
        // Change the local and global shared info.
        _localSharedInfo[pageNo] = true;  
        _globalSharedInfo[pageNo] = true; 
        
        // Change the mapping to Readonly and MAP_PRIVATE.
        mapRdPrivate(pageinfo->pageStart, xdefines::PageSize);
      }
  #endif
    }
  }

  inline int recordCacheInvalidates(int pageNo, int cacheNo) {
    int myTid = getpid();
    int lastTid;
    int interleaving = 0;

    // Try to check the global array about cache last thread id.
    lastTid = atomic::exchange(&_cacheLastthread[cacheNo], myTid);

    //fprintf(stderr, "Record cache interleavings, lastTid %d and myTid %d\n", lastTid, myTid);
    if(lastTid != 0 && lastTid != myTid) {
      // If the last thread to invalidate cache is not current thread, then we will update global
      // counter about invalidate numbers.
      atomic::increment(&_cacheInvalidates[cacheNo]);
     // fprintf(stderr, "Record cache invalidates %p with interleavings %d\n", &_cacheInvalidates[cacheNo], _cacheInvalidates[cacheNo]);
      interleaving = 1;
    }
    return interleaving;
  }
  
  // Record changes for those shared pages.
  inline void recordChangesAndUpdate(struct pageinfo * pageinfo) {
    unsigned long cacheNo;
    int myTid = getpid();
    unsigned long * local = (unsigned long *)pageinfo->pageStart;
    int startCacheNo = pageinfo->pageNo*xdefines::CACHES_PER_PAGE;
    unsigned long recordedCacheNo = 0xFFFFFFFF;
    unsigned long * twin;
    unsigned long * wordChanges;
  #if defined(DETECT_FALSE_SHARING_OPT)
    unsigned long interWrites = 0;
  #endif
  
    // We do nothing for this page. If this page is touched again,
    // then it can create one entry again.
    if(pageinfo->hasTwinPage == false) 
      return;
    
    twin = (unsigned long *)pageinfo->tempTwinPage;
    if(pageinfo->alloced == false) {
      allocResourcesForSharePage(pageinfo);
  
      //printf(stderr, "%d try to get a new twin page\n", getpid());  
      // We will create the tempTwinPage by copying from the original twin page now.
      twin = (unsigned long *)pageinfo->tempTwinPage;
      memcpy(twin, pageinfo->origTwinPage, xdefines::PageSize);
    }
  
    wordChanges = (unsigned long *)pageinfo->wordChanges;
  
    // We will check those modifications by comparing "local" and "twin".
    for(int i = 0; i < xdefines::PageSize/sizeof(unsigned long); i++) {
      if(local[i] != twin[i]) {
        int lastTid;
    
        // Calculate the cache number for current words.  
        cacheNo = calcCacheNo(i);
        
        // We will update corresponding cache invalidates.
        if(cacheNo != recordedCacheNo) {
      #if defined(DETECT_FALSE_SHARING_OPT)
          interWrites += recordCacheInvalidates(pageinfo->pageNo, pageinfo->pageNo*xdefines::CACHES_PER_PAGE + cacheNo);
      #endif
          recordedCacheNo = cacheNo;
        }
        
        // Update corresponding words on twin page and record changes for words in this cache line.
        twin[i] = local[i];
        wordChanges[i]++; 
      }   
    }
  }

  inline void periodicCheck(void) {
    // Scan those shared pages to record some modifications in the past period.
    checkDirtiedPages();
  }

  // Invalid now.
  bool nop (void) {
#ifdef GET_CHARACTERISTICS
    if(_privatePagesList.empty())
      _pageprof.updateCommitInfo(0);
#endif
    return(_privatePagesList.empty());
  }

  /// @brief Start a transaction.
  inline void begin (void) {
    // Update all pages related in this dirty page list
    updateAll();
  }

  void stats (void) {
    fprintf (stderr, "xpersist stats: %d dirtied\n", _privatePagesList.size());
  }

  // Here, we are trying to use vectorization to improve the performance.
  inline void writePageDiffs (const void * local, const void * twin, void * dest) {
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
        mydest[i] = mylocal[i];
      }
    }

  #endif
  }

  inline void checkCommitWord(char * local, char * twin, char * share) {
    int i = 0;
    while(i < sizeof(unsigned long)) {
      if(local[i] != twin[i]) {
        share[i] = local[i];
      }
      i++;
    }
  }

  inline void recordWordChanges(void * addr, unsigned long changes) {
    wordchangeinfo * word = (wordchangeinfo *)addr;
    unsigned short tid = word->tid;
  
    int mine = getpid();
  
    // If this word is not shared, we should set to current thread.
    if(tid == 0) {
      word->tid = mine;
    }
    else if (tid != 0 && tid != mine && tid != 0xFFFF) {
      // This word is shared by different threads. Set to 0xFFFF.
      word->tid = 0xFFFF;
    }
  
    word->version += changes;
  }

  int calcCacheNo(unsigned long words) {
    return (words * sizeof(unsigned long))/xdefines::CACHE_LINE_SIZE;
  }

  // Normal commit procedure. All local modifications should be commmitted to the shared mapping so
  // that other threads can see this change. 
  inline void checkcommitpage(struct pageinfo * pageinfo) {
    unsigned long * twin = (unsigned long *) pageinfo->origTwinPage;
    unsigned long * tempTwin = (unsigned long *) pageinfo->tempTwinPage;
    unsigned long * local = (unsigned long *) pageinfo->pageStart; 
    unsigned long * share = (unsigned long *) ((intptr_t)_persistentMemory + xdefines::PageSize * pageinfo->pageNo);
    unsigned long * localChanges = (unsigned long *) pageinfo->wordChanges;
    // Here we assume sizeof(unsigned long) == 2 * sizeof(unsigned short);
    unsigned long * globalChange = (unsigned long *)((intptr_t)_wordChanges + xdefines::PageSize * pageinfo->pageNo);
    unsigned long recordedCacheNo = 0xFFFFFFFF;
    unsigned long cacheNo;
  #if defined(DETECT_FALSE_SHARING_OPT)
    unsigned long interWrites = 0;
  #endif
  
    // Also, it is possible to change the global version number about invalidates too. 
    // Iterate through the page a word at a time.
    if(localChanges == NULL) {
      for (int i = 0; i < xdefines::PageSize/sizeof(unsigned long); i++) {
        if(local[i] != twin[i]) {
          // Calculate the cache number for current words.    
          cacheNo = calcCacheNo(i);

          // We will update corresponding cache invalidates.
          if(cacheNo != recordedCacheNo) {
        #if defined(DETECT_FALSE_SHARING_OPT)
            interWrites += recordCacheInvalidates(pageinfo->pageNo, pageinfo->pageNo*xdefines::CACHES_PER_PAGE + cacheNo);
        #endif
          }
          checkCommitWord((char *)&local[i], (char *)&twin[i], (char *)&share[i]);
          recordWordChanges((void *)&globalChange[i], 1);
        }
      }
    }
    else {
      for (int i = 0; i < xdefines::PageSize/sizeof(unsigned long); i++) {
        unsigned long cacheNo;
        unsigned long recordedCacheNo = 0xFFFFFFFF; 
  
        if(local[i] == twin[i] && localChanges[i] == 0) {
          // There is no need to commit
          continue;
        }
        else if(local[i] == twin[i] && localChanges[i] != 0) {
          // There is ABA change, we just update the global version directly.
          //fprintf(stderr, "detect the ABA changes %d\n", localChanges[i]);
          recordWordChanges((void *)&globalChange[i], localChanges[i]);
          continue;
        }
        
        // Here, we find some modification. 
        if(local[i] != tempTwin[i]) {
          // Calculate the cache number for current words.    
          cacheNo = calcCacheNo(i);

          // We will update corresponding cache invalidates.
          if(cacheNo != recordedCacheNo) {
        #if defined(DETECT_FALSE_SHARING_OPT)
            interWrites += recordCacheInvalidates(pageinfo->pageNo, pageinfo->pageNo*xdefines::CACHES_PER_PAGE + cacheNo);
        #endif

            recordedCacheNo = cacheNo;
          }

          recordWordChanges((void *)&globalChange[i], 1);
        }
      
        checkCommitWord((char *)&local[i], (char *)&twin[i], (char *)&share[i]);
        recordWordChanges((void *)&globalChange[i], localChanges[i]);
      }
    }
  }

#ifdef DETECT_FALSE_SHARING_OPT
  inline void issueBatchedSystemcalls(int pagetype, int batched, void * batchedStart) {
    if(batched == 0) {
      return;
    }

    switch(pagetype) {
    case PAGE_TYPE_UPDATE:
      updatePage(batchedStart, batched * xdefines::PageSize);
      break; 
        
    case PAGE_TYPE_PRIVATE:
      mapRdPrivate(batchedStart, batched * xdefines::PageSize);
      break;

    case PAGE_TYPE_READONLY: 
      writeProtect(batchedStart, batched * xdefines::PageSize);
      break;
        
    default:
      break;
    }
  }
#endif
 
  inline void commit(bool doChecking) {
    struct pageinfo * pageinfo = NULL;
    int pageNo;
    unsigned long * persistent;
    void * batchedStart;
    int    batched = 0;
    int    lastpage;
    int    lastpagetype = PAGE_TYPE_INVALID;
    int    pagetype = PAGE_TYPE_INVALID;


#ifdef GET_CHARACTERISTICS
    _pageprof.updateCommitInfo(_privatePagesList.size());
#endif
    if(_privatePagesList.size() == 0) {
      return;
    }

    // Commit those private pages if _localSharedInfo is set to true since that means current page
    // are using the private copy.
    for (dirtyListType::iterator i = _privatePagesList.begin(); i != _privatePagesList.end(); ++i) {
      pageinfo = (struct pageinfo *)i->second;
      pageNo = pageinfo->pageNo;
      persistent = (unsigned long *) ((intptr_t)_persistentMemory + xdefines::PageSize * pageNo);
    
    #ifdef DETECT_FALSE_SHARING_OPT
      if(_localSharedInfo[pageNo] == true && pageinfo->hasTwinPage == true) {
      #ifdef GET_CHARACTERISTICS
        stats::getInstance().updateDirtyPage();
      #endif

        // We have to do slower checking in order to commit all information to the global changes.
        if(doChecking && (pageinfo->wordChanges != NULL || pageinfo->shared == true)) 
          checkcommitpage(pageinfo);
        else  
          writePageDiffs(pageinfo->pageStart, pageinfo->origTwinPage, persistent);

        pagetype = PAGE_TYPE_UPDATE;
      }
      else if(_detectPeriod) {
        // We don't need the commit if no private copy. Then we may change the mapping.
        if(doChecking && (pageinfo->shared == true || _pageUsers[pageNo] > 1 || _globalSharedInfo[pageNo] == 1) && _localSharedInfo[pageNo] == false) {
          // Change the local and global shared info.
          _globalSharedInfo[pageNo] = true; 
    
          // Change the mapping to Readonly and MAP_PRIVATE.
          _localSharedInfo[pageNo] = true;  
        #ifdef GET_CHARACTERISTICS
          stats::getInstance().updateProtects();
        #endif
          pagetype = PAGE_TYPE_PRIVATE;
        }
        else {
          pagetype = PAGE_TYPE_READONLY;
        }
      }
      else {
        pagetype = PAGE_TYPE_INVALID;
        _savedPagesList.insert(pair<int, void *>(pageNo, pageinfo));
      }
  
      // FIXME: Put the system calls stuff in one funtion call  
      if(pagetype == lastpagetype && pageNo == lastpage + 1) {
        batched++;
      }
      else {
        // Issue batched system calls.  
        issueBatchedSystemcalls(lastpagetype, batched, batchedStart);

        batched = 1;
        batchedStart = pageinfo->pageStart;
        lastpage = pageNo;
        lastpagetype = pagetype;
      }
    #else
      // It is possible that one thread are accessing the same page directly when I am trying to access,
      // It is safer to commit the changes only. Memcpy can compromise the changes by the thread directly working on that.
      writePageDiffs(pageinfo->pageStart, pageinfo->origTwinPage, persistent);
    #endif
      atomic::decrement(&_pageUsers[pageinfo->pageNo]);
    }

  #ifdef DETECT_FALSE_SHARING_OPT
    issueBatchedSystemcalls(lastpagetype, batched, batchedStart);
  
    if(_detectPeriod && _savedPagesList.size() > 0) {
      lastpagetype = PAGE_TYPE_READONLY;
      lastpage = -1;
      batched = 0;
    
      for (dirtyListType::iterator i = _savedPagesList.begin(); i != _savedPagesList.end(); ++i) {
        pageinfo = (struct pageinfo *)i->second;
        pageNo = pageinfo->pageNo;
      
        if(pageNo == lastpage + 1) {
          batched++;
        }
        else {
         // Issue batched system calls.  
          issueBatchedSystemcalls(lastpagetype, batched, batchedStart);

          batched = 1;
          batchedStart = pageinfo->pageStart;
          lastpage = pageNo;
        }
      }
      _savedPagesList.clear();
    }

    _privatePagesList.clear();

    // Clean up those page entries.
    xpageentry::getInstance().cleanup();
    xpagestore::getInstance().cleanup();
  #endif
  }

#ifndef DETECT_FALSE_SHARING_OPT
  /// @brief Update every page frame from the backing file.
  /// Change to this function so that it will deallocate those backup pages. Previous way
  /// will have a memory leakage here without deallocation of Backup Pages.
  /// Also, re-protect those block in the list.
  void updateAll (void) {
    // Dump the now-unnecessary page frames, reducing space overhead.
    dirtyListType::iterator i;
    for (i = _privatePagesList.begin(); i != _privatePagesList.end(); ++i) {
      struct pageinfo * pageinfo = (struct pageinfo *)i->second;
      updatePage(pageinfo->pageStart, xdefines::PageSize);
    }
    
    _privatePagesList.clear();
    
    // Clean up those page entries.
    xpageentry::getInstance().cleanup();
    xpagestore::getInstance().cleanup();
  }
#else
  void updateAll(void) {
  // Do nothing.  
  }
#endif

  void protectPage(int pageNo) {
    void * area;
    int offset = pageNo * xdefines::PageSize;
    void * start =(void *)((int)base() + offset);

    //fprintf(stderr, "%d : remove protect %x size 0x%x, offset 0x%x\n", getpid(), start, size, offset);
    // Map to writable share area. 
    area = mmap (start,
                 xdefines::PageSize,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_FIXED,
                 _backingFd,
                 offset);

    if(area == MAP_FAILED) {
      fprintf(stderr, "Weird, %d remove protect failed!!!\n", getpid());
      exit(-1);
    }
  }

  void setProtectionPeriod(void) {
    writeProtect(base(), size());
    _detectPeriod = true; 
  }

  void unsetProtectionPeriod(void) {
    _detectPeriod = false;  
  }

#ifdef DETECT_FALSE_SHARING_OPT
  void *setPagesRwShared(int start, int stop) {
    int  offset = start*xdefines::PageSize;
    void * startAddr = (void *)((int)base() + offset);
    int size = (stop - start + 1)*xdefines::PageSize;
    mprotect(startAddr, size, PROT_READ|PROT_WRITE); 
 }

  void unprotectNonProfitPages(void * end) {
    int totalpages;
    if(end == NULL) {
      totalpages = size()/xdefines::PageSize;
    }
    else {
      totalpages = ((intptr_t)end - (intptr_t)base())/xdefines::PageSize;
    }

    int start = 0;
    bool unprotect = false;

    unsetProtectionPeriod(); 
  }
#endif

  // We don't need to set the page protection.
  void cleanup (void) {
    // Dump the now-unnecessary page frames, reducing space overhead.
    dirtyListType::iterator i;
    for (i = _privatePagesList.begin(); i != _privatePagesList.end(); ++i) {
      struct pageinfo * pageinfo = (struct pageinfo *)i->second;
      madvise (pageinfo->pageStart, xdefines::PageSize, MADV_DONTNEED);
    }

    _privatePagesList.clear();

    // Clean up those page entries.
    xpageentry::getInstance().cleanup();
    xpagestore::getInstance().cleanup();
  }


  /// @brief Commit all writes.
  inline void memoryBarrier (void) {
    atomic::memoryBarrier();
  }

private:

  inline int computePage (int index) {
    return (index * sizeof(Type)) / xdefines::PageSize;
  }

  /// @brief Update the given page frame from the backing file.
  void updatePage (void * local, int size) {
    madvise (local, size, MADV_DONTNEED);

    // Set this page to PROT_READ again.
    mprotect (local, size, PROT_READ);
  }
 
  /// True if current xpersist.h is a heap.
  bool _isHeap;
  bool _isBasicHeap;

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

  unsigned long * _pageUsers;
 
  /// The length of the version array.
  enum { TotalPageNums = NElts * sizeof(Type)/(xdefines::PageSize) };
  enum { TotalCacheNums = NElts * sizeof(Type)/(xdefines::CACHE_LINE_SIZE) };

  unsigned long * _cacheInvalidates;

  // Last thread to modify current cache
  unsigned long * _cacheLastthread;

  // A string of one bits.
  __m128i allones;
  
  enum { TotalWordNums = NElts * sizeof(Type)/sizeof(unsigned long) };
  
  // In order to save space, we will use the higher 16 bit to store the thread id
  // and use the lower 16 bit to store versions.
  wordchangeinfo * _wordChanges;

  bool _detectPeriod;
 
#ifdef GET_CHARACTERISTICS
  xpageprof<Type, NElts>  _pageprof;
#endif

#ifdef DETECT_FALSE_SHARING_OPT
  xtracker<NElts> _tracker;
#endif
};

#endif
