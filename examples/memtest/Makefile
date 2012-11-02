CC = gcc
CXX= g++
CFLAGS = -m32 -Wall -g
CFLAGS = -Wall -g -O0
LIBS = -lm -lrt -ldl

SRCS := $(wildcard *.c)
TESTS := $(SRCS:.c=)

#SRCS := $(wildcard *.cpp)
#TESTS := $(SRCS:.cpp=)

PTHREAD_LIBS += $(LIBS) -lpthread
PTHREAD_OBJS := $(addsuffix -pthread, $(TESTS))

#DTHREAD_LIBS = $(LIBS) -rdynamic ../../libsheriff_detect32.so
DTHREAD_LIBS = $(LIBS) -rdynamic ../../libsheriff_detect64.so
#DTHREAD_LIBS = $(LIBS) -rdynamic ../../../sheriff-bak-beforesharedstack/libsheriff_detect64.so
#DTHREAD_LIBS = $(LIBS) -rdynamic /nfs/cm/scratch1/tonyliu/grace/branches/sheriffrelease/sheriff/libsheriff_detect64.so
#DTHREAD_LIBS = $(LIBS) -rdynamic /nfs/cm/scratch1/tonyliu/grace/branches/sheriff/libsheriff.so
#DTHREAD_LIBS = $(LIBS) -rdynamic sheriff/libsheriff.so
DTHREAD_OBJS := $(addsuffix -dthread, $(TESTS))

COMPILE = $(CC) $(CFLAGS) 
COMPILE = $(CXX) $(CFLAGS) 

.PHONY : default all clean
default: all
all:$(PTHREAD_OBJS) $(DTHREAD_OBJS)
clean:
	rm -f $(PTHREAD_OBJS) $(DTHREAD_OBJS) 

%-pthread:%.c
	$(COMPILE) -o $@ $< $(PTHREAD_LIBS)

%-dthread:%.c
	$(COMPILE) -o $@ $< $(DTHREAD_LIBS)

%-pthread:%.cpp
	$(COMPILE) -o $@ $< $(PTHREAD_LIBS)

%-dthread:%.cpp
	$(COMPILE) -o $@ $< $(DTHREAD_LIBS)

