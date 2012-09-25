// -*- C++ -*-

#ifndef SHERIFF_OBJECTINFO_H
#define SHERIFF_OBJECTINFO_H

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
  struct wordchangeinfo* wordchange_start;
  struct wordchangeinfo* wordchange_stop;
  unsigned long callsite[CALL_SITE_DEPTH];
};

#endif
