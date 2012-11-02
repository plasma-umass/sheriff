// -*- C++ -*-

/*
  Copyright (C) 2011 University of Massachusetts Amherst.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#ifndef SHERIFF_OBJECTHEADER_H
#define SHERIFF_OBJECTHEADER_H

/*
 * @file   objectheader.h
 * @brief  Heap object header, keeping track of size and callsite.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */

#if defined(DETECT_FALSE_SHARING) || defined(DETECT_FALSE_SHARING_OPT)
#include "callsite.h"
#endif

class objectHeader {
public:
  enum { MAGIC = 0xCAFEBABE };

  objectHeader (size_t sz)
    : _size (sz),
      _magic (MAGIC)
  {
  }

  size_t getSize () { sanityCheck(); return _size; }

#if defined(DETECT_FALSE_SHARING) || defined(DETECT_FALSE_SHARING_OPT)

  int getCallsiteLength() const {
    return sizeof(CallSite);
  }

  CallSite* getCallsiteRef() {
    return &_callsites;
  }

  void storeCallsite (CallSite& callsite) {
    for (int i = 0; i < callsite.getDepth(); i++) {
      _callsites._callsite[i] = callsite._callsite[i];
    }
  }
 
  bool sameCallsite(CallSite *callsite) const {
    bool ret = true;

    for(int i = 0; i < CALL_SITE_DEPTH; i++) {
      if (_callsites._callsite[i] != callsite->_callsite[i]) {
	ret = false; 
	break;
      }
    }
        
    return ret;
  }
#endif

  bool isValidObject() {
    bool ret = false;
    ret = sanityCheck();
    if (ret) {
      // FIXME: _size should be multiple of 8s according to our heap implementation.
      ret = (_size % 8 == 0)? true : false;
    }
    return ret;		
  }

  bool verifyMagic() {
    return (_magic == MAGIC);
  }

private:

  bool sanityCheck (void) {
#ifndef NDEBUG
    if (_magic != MAGIC) {
      fprintf (stderr, "Sanity check failed in process %d. Current _magic %x at %p\n", getpid(), _magic, &_magic);
      ::abort();
    }
#endif
    return true;
  }

  size_t _magic;
  size_t _size;
#if defined(DETECT_FALSE_SHARING) || defined(DETECT_FALSE_SHARING_OPT)
  CallSite _callsites;
#endif
};

#endif /* SHERIFF_OBJECTHEADER_H */
