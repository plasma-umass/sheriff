// -*- C++ -*-

#ifndef SHERIFF_XTRACKER_H
#define SHERIFF_XTRACKER_H

#include <set>

#if !defined(_WIN32)
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "mm.h"
#include "wordchangeinfo.h"
#include "objectinfo.h"
#include "objecttable.h"
#include "objectheader.h"
#include "elfinfo.h"
#include "callsite.h"
#include "stats.h"

template <unsigned long NElts = 1>
class xtracker {
  
  enum { PAGE_SIZE = 4096 };
  enum { MAXBUFSIZE = 1024 };

public:

  xtracker()
  {
    int    count;
    void * ptr;

    if (NElts == xdefines::PROTECTEDHEAP_SIZE)
      _isHeap = true;

    /* Get current executable file name. */
    count=readlink("/proc/self/exe", _exec_filename, MAXBUFSIZE);
    if (count <= 0 || count >= MAXBUFSIZE)
    {
      fprintf(stderr, "Failed to get current executable file name\n" );
      exit(1);
    }
    _exec_filename[count] = '\0';

    /* Get text segmentations information. */
    _elf_info.hdr = (Elf_Ehdr*)grab_file(_exec_filename, &_elf_info.size);
    if (!_elf_info.hdr) {
      printf("Can't grab file %s\n", _exec_filename);
      exit(1);
    }
  
    /* Get the text segment information and saved to global _textinfo */
    get_textseg_info(&_elf_info);
    parse_elf();
  }

  virtual ~xtracker() {
    release_file(_elf_info.hdr, _elf_info.size);
  }
 
  void print_objects_info() {

    char base[MAXBUFSIZE];
    int k = 0;      

    sprintf (base, "addr2line -e %s", _exec_filename);
  
    if (ObjectTable::getInstance().getObjectsNum() > 0) {
      fprintf(stderr, "Sheriff-Detect: false sharing detected.\n");
    }
    else {
      fprintf(stderr, "Sheriff-Detect: no false sharing found.\n");
      return;
    }
  
    // We sort those objects according to the number of interleaving writes.
    typedef ObjectTable::callsiteType ObjectType;

    ObjectType * objects = (ObjectType *)ObjectTable::getInstance().getCallsites();

    typedef std::greater<const int> localComparator;
    typedef HL::STLAllocator<ObjectType, privateheap> Allocator;    
    typedef std::multimap<const int, ObjectInfo, localComparator, Allocator> objectListType;
    objectListType objectlist;

    // Get all objects to this list.
    for (ObjectType::iterator i = objects->begin(); i != objects->end(); ++i) {
       ObjectInfo & object = i->second;
       objectlist.insert(pair<int, ObjectInfo>(object.interwrites, object));
    }

    for(objectListType::iterator i = objectlist.begin(); i != objectlist.end(); i++) {
      ObjectInfo & object = i->second;
      k++;
        
      if(object.interwrites < xdefines::MIN_INTERWRITES_OUTPUT) {
        continue;
      }
      //fprintf(stderr, "Object %d: cache interleaving writes %d (%d per cache line, %d times on %d actual line(s), object writes = %d)\n\tObject start = %lx; length = %d.\n", k, object.interwrites, object.interwrites/object.lines, object.interwrites/object.actuallines, object.actuallines, object.totalwrites, object.start, object.totallength);
      
      fprintf(stderr, "Object %d: cache interleaving writes %d on %d cache lines:\n  Object start = %lx; length = %d.\n", k, object.interwrites, object.actuallines, object.start, object.totallength);
      if (object.is_heap_object == true) {
      //  fprintf(stderr, "\tHeap object accumulated by %d, unit length = %d, total length = %d, cache lines = %d.\n", object.times, object.unitlength, object.totallength, object.totallength/xdefines::CACHE_LINE_SIZE);

        // Print callsite information.
	      fprintf (stderr, "    Object allocation call site information:\n");
        CallSite * callsite = (CallSite *) &object.callsite[0];
        for(int j = 0; j < callsite->getDepth(); j++) {
          unsigned long ipaddr = callsite->getItem(j);
          char command[MAXBUFSIZE];
            
         fprintf(stderr, "\tCall site %d %lx: ", j, ipaddr);
          if(ipaddr >= textStart && ipaddr <= textEnd) {
            sprintf(command, "%s %x", base, ipaddr);
            system(command);
          }
        }
        fprintf(stderr, "\n\n");
      }
      else {
        // Print object information about globals.
        Elf_Sym *symbol = find_symbol(&_elf_info, (intptr_t)object.start);
        if(symbol != NULL) {
          const char * symname = _elf_info.strtab + symbol->st_name;
          fprintf(stderr, "\tGlobal object: name \"%s\", start %lx, size %d\n", symname, symbol->st_value, symbol->st_size);
        }
      }
    }
  }

  void *grab_file(const char *filename, unsigned long *size) {
    struct stat st;
    void *map;
    int fd;

    fd = open(filename, O_RDONLY);
    if (fd < 0 || fstat(fd, &st) != 0)
      return NULL;

    *size = st.st_size;
    map = MM::allocatePrivate (*size, fd);
    close(fd);

    if (map == MAP_FAILED)
      return NULL;
    return map;
  }

  int check_elf(struct elf_info * _elf_info) 
  {
    Elf_Ehdr *hdr = _elf_info->hdr;

    if (_elf_info->size < sizeof(*hdr)) {
        /* file too small, assume this is an empty .o file */
        return -1;
    }
    
    /* Is this a valid ELF file? */
    if ((hdr->e_ident[EI_MAG0] != ELFMAG0) ||
       (hdr->e_ident[EI_MAG1] != ELFMAG1) ||
       (hdr->e_ident[EI_MAG2] != ELFMAG2) ||
       (hdr->e_ident[EI_MAG3] != ELFMAG3)) {
      /* Not an ELF file - silently ignore it */
      return -1;
   }

   return 0;
  }

  void get_textseg_info(struct elf_info * _elf_info) 
  {
    Elf_Ehdr *hdr = _elf_info->hdr;
    Elf_Shdr *sechdrs;
    int i;
  
    if(check_elf(_elf_info)) {
      printf("errorneous elf file.\n");
      exit(1);
    }
    
    sechdrs =(Elf_Shdr *)((char *)hdr + hdr->e_shoff);
    _elf_info->sechdrs = sechdrs;

    /* Fix endianness in section headers */
    for (i = 0; i < hdr->e_shnum; i++) {
      char *secstrings=(char*)((unsigned long)hdr + sechdrs[hdr->e_shstrndx].sh_offset);
      char *secname;
      secname = secstrings + sechdrs[i].sh_name;

      if (strncmp(secname, ".text", 5) != 0) {
        continue;
      }
   
      textStart = hdr->e_entry;
      textEnd   = hdr->e_entry + sechdrs[i].sh_size;
      assert(textEnd >= textStart);
    }

    fprintf(stderr, "textStart 0x%lx textEnd 0x%lx\n", textStart, textEnd);
    return; 
  }

  int parse_elf()
  {
    unsigned int i;
    Elf_Ehdr *hdr = _elf_info.hdr;
    Elf_Shdr *sechdrs;
  
    if (check_elf(&_elf_info)) {
      fprintf (stderr, "Erroneous ELF file.\n");
      exit(-1);
    }
  

    sechdrs =(Elf_Shdr *)((char *)hdr + hdr->e_shoff);
    _elf_info.sechdrs = sechdrs;

    /* Fix endianness in section headers */
    for (i = 0; i < hdr->e_shnum; i++) {
      if (sechdrs[i].sh_type != SHT_SYMTAB)
        continue;
  
      if(sechdrs[i].sh_size == 0) 
        continue;
  
      _elf_info.symtab_start = (Elf_Sym*)((unsigned long)hdr + sechdrs[i].sh_offset);
      _elf_info.symtab_stop  = (Elf_Sym*)((unsigned long)hdr + sechdrs[i].sh_offset + sechdrs[i].sh_size);
      _elf_info.strtab       = (char *)((unsigned long)hdr + sechdrs[sechdrs[i].sh_link].sh_offset);
      break;
    }
  
    return 0; 
  }

  /*
   * Find symbols according to specific address.  
   */
  Elf_Sym * find_symbol(struct elf_info *elf, Elf_Addr addr)
  {
    int bFound = 0;
    Elf_Sym *symbol;
    Elf_Ehdr *hdr = elf->hdr;
  
    for (symbol = elf->symtab_start; symbol < elf->symtab_stop; symbol++) {
      if (symbol->st_shndx >= SHN_LORESERVE)
        continue;
      
      /* Checked only when it is a global variable. */
      if(ELF_ST_BIND(symbol->st_info) != STB_GLOBAL || ELF_ST_TYPE(symbol->st_info) != STT_OBJECT)
        continue;
  
      /* If the addr is in the range of current symbol, that get it. */
      if(addr >= symbol->st_value && addr < (symbol->st_value + symbol->st_size)) {
        bFound = 1;
        break;
      }
    }
  
    if(bFound)
      return symbol;
    else
      return NULL;
  }

  void release_file(void *file, unsigned long size)
  {
    munmap(file, size);
  }

  void finalize() {
  }
  
  void printCacheline(int * start) {
    int i = 0;
    int * cachestart;
    cachestart = (int *)(((int)start) & 0xFFFFFFC0);
  
    while(i < 16) {
      fprintf(stderr, "%d-%x\t", i++, cachestart[i]);  
    }
  }

  objectHeader * getHeapObject(int * pos, int * stop) {
    objectHeader * obj = NULL;
    int size = 0;
    int * start = pos;
  
    fprintf(stderr,"pos is %p and stop is %p\n\n", pos, stop);

    obj = NULL;
  
    // Do reverse checking to find the start of object.
    while(pos > stop) {
      if(*pos == objectHeader::MAGIC) //Magic number for an object
      {
        obj = (objectHeader *)pos;
        fprintf(stderr, "obj is got %p size %d\n", obj, obj->getSize());
      
        /* Check whether it is valid object header. */
        if(obj->isValidObject()) {
          break;
        }
        obj = NULL;
      }
      pos--;
      size+=4;
    }

#if 0
  if(obj == NULL) {
    printCacheline(start);
  }
#endif
    return obj;
  }

  int calcCacheWrites(wordchangeinfo * start, int count) {
    wordchangeinfo * cur = start;
    wordchangeinfo * stop = &start[count];
    int    writes = 0;
    
    while(cur < stop) {
      writes += cur->version;
      cur++;
    }
  
    return writes;
  }

  // It is simple for us, we just use the forward search.
  void checkWrites(int * base, int size, wordchangeinfo * wordchange) {
    int * pos;
    int * end;
  
    int i = 0;
    pos = base;
    end = (int *)((intptr_t)base + size);
  
    fprintf(stderr, "Checking writes on %p\n", base); 
    while(pos < end) {
      // First, we should calculate the writes on this cacheline.   
      int writes = 0;
  
      writes = calcCacheWrites(&wordchange[pos-base], xdefines::CACHE_LINE_SIZE);
  
      fprintf(stderr, "%d: cache writes %d on %p\n", i++, writes, pos);
   
   //   if(writes > xdefines::MIN_WRITES_CARE) {
      {
        fprintf(stderr, "%d: cache writes %d on %p\n", i++, writes, pos);
      }
  
      pos += 16;
    }
  }

  // Get how many cache invadidations happen for specified cache line. 
  long getCacheInvalidates(int cacheStart, long lines, unsigned long * cacheInvalidates, long * actuallines){
    long writes = 0;
    //fprintf(stderr, "Now check start %p cacheStart %ld line %ld\n", cacheInvalidates, cacheStart, lines);
    for(long i = 0; i < lines; i++) {
      writes += cacheInvalidates[cacheStart + i];
      //fprintf(stderr, "Now i %d check cacheInvalidates at %p\n", i, &cacheInvalidates[cacheStart+i]);
      if(cacheInvalidates[cacheStart + i] > 1) {
        (*actuallines)++;
      } 
  #ifdef GET_CHARACTERISTICS
      if(cacheInvalidates[cacheStart + i] > 1) { 
        stats::getInstance().updateCaches();
      }
  #endif
    }

    return writes;  
  }

  bool sameCallsite(CallSite * that1, CallSite * that2) {
    bool result = true;
    int i;
    for(i = 0; i < CALL_SITE_DEPTH; i++) {
      if(that1->_callsite[i] != that2->_callsite[i]) {
        result = false;
        break;
      }
    }

  return result;
  }

  // We will return the start address of next different object. 
  int * getNextDiffObject(int * pos, int * memend, CallSite * callsite, int unitsize) {
    int * address = memend;
  
    // We should check object according to the length of one object.
    while(pos < memend) {
      if(*pos == objectHeader::MAGIC) {
        objectHeader * object = (objectHeader *)pos;
        
        if(sameCallsite(callsite, object->getCallsiteRef())) {
          pos = (int *)((intptr_t)&object[1] + object->getSize());
          continue;
        }
        else {
          address = pos;
          break;        
        } 
      }
      else {
        address = pos;
        break;
      }
    }
  
    return address;
  }

  int getObjectWrites(int * start, int * stop, int * memstart, wordchangeinfo * wordchange) {
    int offset;
    offset = ((intptr_t)start - (intptr_t)memstart)/sizeof(unsigned long);
      
    wordchangeinfo * cur = &wordchange[offset];
    int    writes = 0;
    int * pos = start;
  
    //fprintf(stderr, "pos %p offset %d\n", pos, offset);
    while(pos < stop) {
      writes += cur->version;
      cur++;
      pos++;
    }

    return writes;
  }

  int getAccessThreads(unsigned long * start, int unitsize, wordchangeinfo * cur) {
    int   offset;
    int   threads = 1;
    int   threadid = cur->tid;
    unsigned long * pos = start;
    unsigned long * stop = (unsigned long *)((intptr_t)start + unitsize);
  
    pos++;
  
    if(threadid == 0xFFFF) {
      return 0xFFFF;
    }
  
    while(pos < stop) {
      if(threadid != cur->tid) {
        threads++;
        threadid = cur->tid;
        if(threads > 2) {
          break;
        }
      }
      if(cur->tid == 0xFFFF) {
        return 0xFFFF;
      }
        
      cur++;
      pos++;
    }

    return threads;
  }


  void checkHeapObjects(unsigned long * cacheInvalidates, int * memstart, int * memend, wordchangeinfo * wordchange) {
    int i;
  
    int * pos = memstart;
 
    while(pos < memend) {
      // We are tracking word-by-word to find the objec theader.  
      if(*pos == objectHeader::MAGIC) {
        objectHeader * object = (objectHeader *)pos;
        unsigned long  objectStart = (unsigned long)&object[1];
        unsigned long   objectOffset = objectStart - (intptr_t)memstart;
        int   writes;
        int   cacheStart = objectOffset/xdefines::CACHE_LINE_SIZE;
        int   unitsize = object->getSize();
      
        // Check the memory until we met a different callsite.
        int * nextobject = getNextDiffObject((int *)(objectStart + object->getSize()), memend, object->getCallsiteRef(), object->getSize());
        
        // Calculate how many cache lines are occupied by this object.
        int   lines = getCachelines(objectStart, unitsize);
        long  actuallines = 0;
 
        // Check whether there are some interleaving writes on this object. 
        writes = getCacheInvalidates(cacheStart, lines, cacheInvalidates, &actuallines);

#if 0
        if(writes > 5) {
          fprintf(stderr, "writes %ld on pos %p nextobject %p. lines %ld\n", writes, pos, nextobject, lines);
        } 
#endif   
        // Whenever interleaved writes is larger than the specified threshold
        // We are trying to report it. 
        if(writes > xdefines::MIN_INTERWRITES_CARE) {
          // Check whether the current object causes enough invalidations or not.
          // We only need to check two ends for continuous memory allocation for same callsite.
          long units = ((intptr_t)nextobject - (intptr_t)pos)/(unitsize+sizeof(objectHeader));
          long objectwrites;
      
          // Check how many objects are located in the first cache line.
          objectwrites = getObjectWrites((int *)objectStart, (int *)(objectStart+unitsize), memstart, wordchange);
  
          // Check how many objects are located in the last cache line.
          ObjectInfo objectinfo;
          
          // Save object information.
          objectinfo.is_heap_object = true;
          objectinfo.interwrites = writes;
          objectinfo.totalwrites = objectwrites;
          objectinfo.unitlength = unitsize;
          objectinfo.lines = lines;
          objectinfo.actuallines = actuallines;
          objectinfo.totallength = unitsize;
          //objectinfo.totallength = (intptr_t)nextobject - (intptr_t)objectStart;
          objectinfo.start = (unsigned long *)objectStart;
       
          objectinfo.stop = (unsigned long *)nextobject;
          objectinfo.wordchange_start = (wordchangeinfo *)((intptr_t)wordchange + objectOffset);
          objectinfo.wordchange_stop = (wordchangeinfo *)((intptr_t)wordchange + objectOffset + unitsize);
         
          memcpy((void *)&objectinfo.callsite, (void *)(object->getCallsiteRef()), object->getCallsiteLength());
          
          // Now add this object into the global ObjectTable.
          objectinfo.access_threads = getAccessThreads((unsigned long *)&object, object->getSize(), (wordchangeinfo *)objectinfo.wordchange_start);
          ObjectTable::getInstance().insertObject(objectinfo);        
        }
          
        pos = (int *)nextobject;
        continue;
      } 
      else {
        pos++;
      }
    }

  }

  // Caculate how many cache lines are occupied by specified address and size.
  int getCachelines(unsigned long start, size_t size) {
    return ((start & xdefines::CACHELINE_SIZE_MASK) + size + xdefines::CACHE_LINE_SIZE - 1)/xdefines::CACHE_LINE_SIZE;
  }

  void checkGlobalObjects(unsigned long *cacheInvalidates, int * memBase, unsigned long size, wordchangeinfo * wordchange) {
    struct elf_info *elf = &_elf_info;  
    Elf_Ehdr *hdr = elf->hdr;
    Elf_Sym *symbol;

 //   fprintf(stderr, "symbol table start %x and stop %x \n", elf->symtab_start, elf->symtab_stop);
    for (symbol = elf->symtab_start; symbol < elf->symtab_stop; symbol++) {
      if (symbol->st_shndx >= SHN_LORESERVE)
        continue;

      /* Checked only when it is a global variable. */
      if(ELF_ST_BIND(symbol->st_info) != STB_GLOBAL || ELF_ST_TYPE(symbol->st_info) != STT_OBJECT)
        continue;

      // Now current symbol is one normal object. 
      long objectStart = symbol->st_value;
      long objectSize = symbol->st_size;
      long objectOffset = objectStart - (intptr_t)memBase;
      long lines = getCachelines(objectStart, symbol->st_size);
      long actuallines = 0;
      long interwrites = getCacheInvalidates(objectOffset/xdefines::CACHE_LINE_SIZE, lines, cacheInvalidates, &actuallines);
   
      long totalwrites = getObjectWrites((int *)objectStart, (int *)(objectStart + objectSize), memBase, wordchange);
//      fprintf(stderr, "memBase %p cacheInvalidates %p, start %lx size %d, Now lines %d global with interwrites %d totalWrites %d\n", memBase, cacheInvalidates, objectStart, objectSize, lines, interwrites, totalwrites); 
      // For globals, only when we need to output this object then we need to store that.
      // Since there is no accumulation for global objects.
      if (interwrites > xdefines::MIN_INTERWRITES_OUTPUT && totalwrites >= (xdefines::MIN_INTERWRITES_OUTPUT)) {
        // Save the object information
        ObjectInfo objectinfo;
        objectinfo.is_heap_object = false;
        //objectinfo.is_heap_object = true;
        objectinfo.interwrites = interwrites;
        objectinfo.totalwrites = totalwrites;
        objectinfo.unitlength = symbol->st_size;
        //fprintf(stderr, "get globals with interwirtes larger than 0, interwrites %d\n", interwrites); 
        objectinfo.lines = lines;
        objectinfo.actuallines = actuallines;
        objectinfo.totallength = symbol->st_size;
        objectinfo.symbol = (void *)symbol;
        objectinfo.start = (unsigned long *)objectStart;
        objectinfo.stop = (unsigned long *)(objectStart + objectSize);
        objectinfo.wordchange_start = &wordchange[objectOffset/sizeof(unsigned long)];
        objectinfo.wordchange_stop = (wordchangeinfo *)((intptr_t)&wordchange[objectOffset/sizeof(unsigned long)] + objectinfo.totallength);

        // Check the first object for share type.
        objectinfo.access_threads = getAccessThreads((unsigned long *)objectStart, objectSize, (wordchangeinfo *)objectinfo.wordchange_start);
        ObjectTable::getInstance().insertObject(objectinfo);
      }
    }
  } 
 
private:

  struct elf_info _elf_info;

  // Profiling type.
  bool _isHeap;
  
  char _exec_filename[MAXBUFSIZE];
};


#endif
