Sheriff: : Precise Detection and Automatic Mitigation of False Sharing
-------------------------------------------------

Tongping Liu, Emery D. Berger

<tonyliu@cs.umass.edu>  
<emery@cs.umass.edu>  

<http://plasma.cs.umass.edu>  

Copyright (C) 2011-2012 University of Massachusetts Amherst


### Building Sheriff ###

You can build the sheriff library (`libsheriff.so`) by running `make`.

*NOTE*: if your system does not support the SSE3 instruction set, then
remove `SSE_SUPPORT` from the Makefile.

Also, check other configurations in the Makefile. For example, if you
are on a 64-bit machine, make sure to use `-fno-omit-frame-pointer` if you want to detect a
false sharing problem.   


### Using Sheriff ###

Sheriff currently supports Linux x86 platforms. 

1. Compile your program to object files (here, we use just one, target.o).

2. Link to the sheriff library. There are two options (neither of which
   is particular to sheriff).

  (a) Dynamic linking: this approach requires no environment variables,
      but the sheriff library needs to be in a fixed, known location.
      Place the sheriff library in a directory (`SHERIFF_DIR`).
      
      Then compile your program as follows:

      % g++ target.o -rdynamic SHERIFF_DIR/libdsheriff.so -ldl -o target

  (b) Ordinary dynamic linking: this approach is more flexible (you can
      change the location of the sheriff library), but you must also
      set the `LD_LIBRARY_PATH` environment variable.

      % g++ target.o -LSHERIFF_DIR -lsheriff -dl -o target
      % export LD_LIBRARY_PATH=SHERIFF_DIR:$LD_LIBRARY_PATH

### Citing Sheriff ###

If you use sheriff, we would appreciate hearing about it. To cite
Sheriff, please refer to the following paper, included as
sheriff-oopsla2011.pdf.

@inproceedings{Liu:2011:SPD:2048066.2048070,  
 author = {Liu, Tongping and Berger, Emery D.},  
 title = {SHERIFF: precise detection and automatic mitigation of false sharing},  
 booktitle = {Proceedings of the 2011 ACM International Conference on Object-Oriented Programming Systems, Languages, and Applications},  
 series = {OOPSLA '11},  
 year = {2011},  
 isbn = {978-1-4503-0940-0},  
 location = {Portland, Oregon, USA},  
 pages = {3--18},  
 numpages = {16},  
 url = {http://doi.acm.org/10.1145/2048066.2048070},  
 doi = {http://doi.acm.org/10.1145/2048066.2048070},  
 acmid = {2048070},  
 publisher = {ACM},  
 address = {New York, NY, USA},  
 keywords = {false sharing, multi-threaded},  
}  

