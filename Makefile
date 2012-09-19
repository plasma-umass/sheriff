SRCS = libsheriff.cpp realfuncs.cpp xthread.cpp dlmalloc.c finetime.c gnuwrapper.cpp 
DEPS = $(SRCS) xpersist.h xdefines.h xglobals.h xpersist.h xplock.h xrun.h warpheap.h xadaptheap.h xoneheap.h

# Detection on 32bit
CXX = g++ -DSSE_SUPPORT -m32 -DX86_32BIT -O3 -fno-omit-frame-pointer -DDETECT_FALSE_SHARING

# Detection on 64bit
#CXX = g++ -DSSE_SUPPORT -m64 -fno-omit-frame-pointer -DDETECT_FALSE_SHARING

# Only protection on 32bit. 
#CXX = g++ -DSSE_SUPPORT -m32 -O3

# Detection should include -fno-omit-frame-pointer we are on 64bit.
#CXX = g++ -DSSE_SUPPORT -O3 -fno-omit-frame-pointer -DDETECT_FALSE_SHARING

# Only protection on 64bit. 
#CXX = g++ -DSSE_SUPPORT -O3 

INCLUDE_DIRS = -I. -I./heaplayers -I./heaplayers/util

#GET_CHARACTERISTICS
all: 
	$(CXX) -march=core2 -msse3 $(INCLUDE_DIRS) -shared -fPIC -D'CUSTOM_PREFIX(x)=sheriff_##x' $(SRCS) -o libsheriff.so  -ldl -lpthread

clean:
	rm -f libsheriff.so 

