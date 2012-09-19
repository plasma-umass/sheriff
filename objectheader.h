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

#ifndef __OBJECTHEADER_H__
#define __OBJECTHEADER_H__

/*
 * @file   objectheader.h
 * @brief  Heap object header, keeping track of size and callsite.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */

#ifdef DETECT_FALSE_SHARING
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

	size_t getSize (void) { sanityCheck(); return _size; }

#ifdef DETECT_FALSE_SHARING
	size_t getCallsiteOffset(void) {
		return (sizeof(size_t) * 2);
	}

	int getCallsiteLenth(void) {
		return (CALL_SITE_DEPTH * sizeof(unsigned long));
	}

	void * getCallsite(void) {
		return((void *)&_callsites);
	}
	
	
	CallSite & getCallsiteRef(void) {
		return((CallSite &)_callsites);
	}

	void store_callsite(CallSite & callsite) {
		for(int i = 0; i < callsite.get_depth(); i++) {
			_callsites._callsite[i] = callsite._callsite[i];
		}
	}
 
 bool sameCallsite(CallSite *callsite) {
		bool ret = true;

		for(int i = 0; i < CALL_SITE_DEPTH; i++) {
			if(_callsites._callsite[i] != callsite->_callsite[i]) {
				ret = false; 
				break;
			}
		}
        
    return (ret);
  }
#endif

	bool isValidObject(void) {
		bool ret = false;
		ret = sanityCheck();
		if(ret == true) {
			// FIXME: _size should be multiple of 8s according to our heap implementation.
			ret = (_size % 8 == 0)? true : false;
		}
		return ret;		
	}

	bool verifyMagic(void) {
		if(_magic == MAGIC)
			return (true);
		else
			return (false);
	}
private:

	bool sanityCheck (void) {
#ifndef NDEBUG
		if (_magic != MAGIC) {
			fprintf (stderr, "HLY FK in %d. Current _magic %x at %p\n", getpid(), _magic, &_magic);
			::abort();
		}
#endif
		return true;
	}

	size_t _magic;
	size_t _size;
#ifdef DETECT_FALSE_SHARING
	CallSite _callsites;
#endif
};

#endif /* __OBJECTHEADER_H__ */
