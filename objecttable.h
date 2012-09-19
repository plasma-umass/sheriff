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

/*
 * @file   objecttable.h  
 * @brief  One table to capture all false sharing objects.
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */ 
    

#ifndef __OBJECT_TABLE_H__
#define __OBJECT_TABLE_H__

#include <map>
#include <ext/hash_map>
#include <stdio.h>
#include <callsite.h>
#include <internalheap.h>
#include <errno.h>
#include <xdefines.h>

using namespace std;

class ObjectTable 
{
  // We could change this hash value function in the future.
  struct callsite_hash {
    unsigned long operator()( const CallSite & that ) const
    {
      unsigned long first = that._callsite[0];
      return (first & 0x3F);
    }
  };

  struct callsite_compare {
    bool operator()( const CallSite &that1, const CallSite & that2 ) const
    {
      bool result = true;
      int i;
      for(i = 0; i < CALL_SITE_DEPTH; i++) {
        if(that1._callsite[i] != that2._callsite[i]) {
          result = false;
          break;
        }
      } 
      return result;
    }
  };

public:
  typedef std::pair<CallSite, ObjectInfo> objectPair;
  typedef __gnu_cxx::hash_map<CallSite, ObjectInfo, callsite_hash, callsite_compare, HL::STLAllocator<objectPair, InternalHeapAllocator> > callsiteType;
  
  ObjectTable()
  {
  }

  static ObjectTable& getInstance (void) {
    static char buf[sizeof(ObjectTable)];
    static ObjectTable * theOneTrueObject = new (buf) ObjectTable();
    return *theOneTrueObject;
  }
  
  void insertObject(ObjectInfo & object) {
    bool isExist = false;
    callsiteType::iterator i;
    CallSite callsite;
    
    memcpy(&callsite, object.callsite, CALL_SITE_DEPTH * sizeof(unsigned long));

    // Check whether the callsite is there?
    if(object.is_heap_object) {
      i = _callsites.find(callsite);

      if(i != _callsites.end()) {
        ObjectInfo & oldobject = i->second;

        // Update the existing object.
        oldobject.interwrites += object.interwrites;
        oldobject.totalwrites += object.totalwrites;
        oldobject.totallength += object.totallength;
        oldobject.lines += object.lines;
        oldobject.actuallines += object.actuallines;
        oldobject.times++;
      }
      else {
        object.times = 1;
        _callsites.insert(objectPair(callsite, object));
      }
    } 
  }

  int getObjectsNum(void) {
    return _callsites.size();
  }

  void * getCallsites(void) {
    return ((void *)&_callsites);
  }

private:
  callsiteType _callsites;

};
#endif /* __OBJECT_TABLE_H__ */
