// -*- C++ -*-

#ifndef SHERIFF_OBJECTINFO_H
#define SHERIFF_OBJECTINFO_H

#include "wordchangeinfo.h"

class ObjectInfo {
public:
  bool is_heap_object;
  int  access_threads;     // False sharing type, inter-objects or inner-object
  int  times; 
  unsigned long interwrites;
  unsigned long totalwrites;
  unsigned long unitlength;
  unsigned long totallength;
  unsigned long lines;
  unsigned long actuallines;
  unsigned long * start;
  unsigned long * stop;
  void * symbol;     // Used for globals only.
  wordchangeinfo* wordchange_start;
  wordchangeinfo* wordchange_stop;
  unsigned long callsite[CALL_SITE_DEPTH];
};

#endif
