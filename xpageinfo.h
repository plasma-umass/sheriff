#ifndef __XPAGEINFO_H__
#define __XPAGEINFO_H__

#define MAX_DIFF_ENTRY 4 

struct pageinfo {
	int pageNo;	

	int version;
   
	// Used to save start address for this page. 
	void * pageStart;
	
	// Following two fields are different in functionality.
    // origTwinPage will be created in the page handler, that can be used
    // in commit phase to detect those modifications in current transaction.
    // tempTwinPage is used to keep temporary modification of currrent page.
    // That will be made itentical to local version everytime when we enter checking timer handler. 
    // Then we can detect local updates in current transaction.
	void * origTwinPage;
	void * tempTwinPage;

	unsigned long * wordChanges;
	bool shared;
	bool alloced;
	bool hasTwinPage;
};

#endif /* __XPAGEINFO_H__ */
