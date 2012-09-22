// -*- C++ -*-

#ifndef SHERIFF_PAGEINFO_H
#define SHERIFF_PAGEINFO_H

struct pageinfo {
  int pageNo;	
  
  int version;
  
  // Used to save start address for this page. 
  void * pageStart;
  
  // Following two fields are different in functionality.

  // origTwinPage will be created in the page handler, that can be
  // used in commit phase to detect those modifications in current
  // transaction.
  void * origTwinPage;

  // tempTwinPage is used to keep temporary modification of currrent
  // page. That will be made itentical to local version everytime when
  // we enter checking timer handler. Then we can detect local updates
  // in current transaction.
  void * tempTwinPage;
  
  unsigned long * wordChanges;
  bool shared;
  bool alloced;
  bool hasTwinPage;
};

#endif /* SHERIFF_PAGEINFO_H */
