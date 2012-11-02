SOURCE_DIR = source
INCLUDE_DIR = include

SRCS =  $(SOURCE_DIR)/libsheriff.cpp \
	$(SOURCE_DIR)/realfuncs.cpp  \
	$(SOURCE_DIR)/xthread.cpp    \
	$(SOURCE_DIR)/dlmalloc.c     \
	$(SOURCE_DIR)/finetime.c     \
	$(SOURCE_DIR)/gnuwrapper.cpp

INCS =  $(INCLUDE_DIR)/xglobals.h     \
  $(INCLUDE_DIR)/xdefines.h     \
	$(INCLUDE_DIR)/xpersist.h     \
	$(INCLUDE_DIR)/xmemory.h      \
	$(INCLUDE_DIR)/xpersist_opt.h     \
	$(INCLUDE_DIR)/xmemory_opt.h      \
	$(INCLUDE_DIR)/xpageinfo.h    \
	$(INCLUDE_DIR)/xpageprof.h    \
	$(INCLUDE_DIR)/xpagestore.h   \
	$(INCLUDE_DIR)/xrun.h         \
	$(INCLUDE_DIR)/objectheader.h \
	$(INCLUDE_DIR)/objecttable.h  \
	$(INCLUDE_DIR)/realfuncs.h    \
	$(INCLUDE_DIR)/detect/stats.h \
	$(INCLUDE_DIR)/detect/xheapcleanup.h \
	$(INCLUDE_DIR)/detect/callsite.h \
	$(INCLUDE_DIR)/detect/xtracker.h   \
	$(INCLUDE_DIR)/heap/xadaptheap.h   \
	$(INCLUDE_DIR)/heap/xoneheap.h     \
	$(INCLUDE_DIR)/heap/warpheap.h     \
	$(INCLUDE_DIR)/heap/internalheap.h \
	$(INCLUDE_DIR)/heap/privateheap.h  \
	$(INCLUDE_DIR)/heap/sourcesharedheap.h \
	$(INCLUDE_DIR)/sync/xplock.h  \
	$(INCLUDE_DIR)/sync/xsync.h   \
	$(INCLUDE_DIR)/util/atomic.h       \
	$(INCLUDE_DIR)/util/elfinfo.h      \
	$(INCLUDE_DIR)/util/finetime.h     \
	$(INCLUDE_DIR)/util/mm.h

DEPS = $(SRCS) $(INCS)

CXX = g++ -g -I$(INCLUDE_DIR) -I$(INCLUDE_DIR)/detect -I$(INCLUDE_DIR)/heap -I$(INCLUDE_DIR)/util -I$(INCLUDE_DIR)/sync

# Detection on 32bit
# CXX = g++ -DSSE_SUPPORT -m32 -DX86_32BIT -O3 -fno-omit-frame-pointer -DDETECT_FALSE_SHARING
# Detection on 64bit
#CXX = g++ -DSSE_SUPPORT -m64 -fno-omit-frame-pointer -DDETECT_FALSE_SHARING


# -march=core2 -msse3 -DSSE_SUPPORT 
#CFLAGS   = -Wall -msse3 -DSSE_SUPPORT -fno-omit-frame-pointer
CFLAGS   = -msse3 -DSSE_SUPPORT -fno-omit-frame-pointer
CFLAGS32 = $(CFLAGS) -m32 -DX86_32BIT # -O3
CFLAGS64 = $(CFLAGS) #-m64 # -O3
#CFLAGS64 = $(CFLAGS) -m64 # -O3

INCLUDE_DIRS = -I. -I./heaplayers -I./heaplayers/util

#GET_CHARACTERISTICS

TARGETS = libsheriff_protect32.so libsheriff_detect32.so libsheriff_protect64.so libsheriff_detect64.so libsheriff_detect32_opt.so libsheriff_detect64_opt.so

all: $(TARGETS)

libsheriff_protect32.so: $(DEPS)
	$(CXX) $(CFLAGS32) $(INCLUDE_DIRS) -shared -fPIC -D'CUSTOM_PREFIX(x)=sheriff_##x' $(SRCS) -o libsheriff_protect32.so  -ldl -lpthread

libsheriff_detect32.so: $(DEPS)
	$(CXX) -DDETECT_FALSE_SHARING $(CFLAGS32) $(INCLUDE_DIRS) -shared -fPIC -D'CUSTOM_PREFIX(x)=sheriff_##x'  $(SRCS) -o libsheriff_detect32.so  -ldl -lpthread

libsheriff_detect32_opt.so: $(DEPS)
	$(CXX) -DDETECT_FALSE_SHARING_OPT $(CFLAGS32) $(INCLUDE_DIRS) -shared -fPIC -D'CUSTOM_PREFIX(x)=sheriff_##x'  $(SRCS) -o libsheriff_detect32_opt.so  -ldl -lpthread

libsheriff_protect64.so: $(DEPS)
	$(CXX) $(CFLAGS64) $(INCLUDE_DIRS) -shared -fPIC -D'CUSTOM_PREFIX(x)=sheriff_##x' $(SRCS) -o libsheriff_protect64.so  -ldl -lpthread

libsheriff_detect64.so: $(DEPS)
	$(CXX) -DDETECT_FALSE_SHARING $(CFLAGS64) $(INCLUDE_DIRS) -shared -fPIC -D'CUSTOM_PREFIX(x)=sheriff_##x'  $(SRCS) -o libsheriff_detect64.so  -ldl -lpthread

libsheriff_detect64_opt.so: $(DEPS)
	$(CXX) -DDETECT_FALSE_SHARING_OPT $(CFLAGS64) $(INCLUDE_DIRS) -shared -fPIC -D'CUSTOM_PREFIX(x)=sheriff_##x'  $(SRCS) -o libsheriff_detect64_opt.so  -ldl -lpthread

clean:
	rm -f $(TARGETS)

