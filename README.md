Sheriff: Precise Detection and Automatic Mitigation of False Sharing
--------------------------------------------------------------------

Tongping Liu, Emery D. Berger

<tonyliu@cs.umass.edu>  
<emery@cs.umass.edu>  

<http://plasma.cs.umass.edu>

Copyright (C) 2011-2012 University of Massachusetts Amherst


### Building Sheriff ###

Running `make` builds two variants of the Sheriff library, in 32-bit and 64-bit versions:

1. *Sheriff_Protect*: Use either `libsheriff_protect32.so` or `libsheriff_protect64.so` as a replacement for the `pthreads` library to automatically eliminate false sharing problems.

2. *Sheriff_Detect*: Use either `libsheriff_detect32.so` or `libsheriff_detect64.so` to find false sharing problems (reported after the program finishes execution).

***NOTE: You may need to install the 32-bit libraries in order to build 32-bit executables. On Debian, for example, type `sudo yum install glibc-devel.i686` and `sudo yum install libstdc++.i686`.


### Using Sheriff ###

Sheriff currently supports Linux x86 platforms.

1. Compile your program to object files (here, we use just one, `target.o`).

2. Link to the appropriate Sheriff library. There are two options (neither of which is particular to Sheriff).

  (a) Dynamic linking: this approach requires no environment variables,
      but the Sheriff library needs to be in a fixed, known location.
      Place the Sheriff library in a directory, e.g., `SHERIFF_DIR`.
      Then compile your program as follows:

      % g++ target.o -rdynamic SHERIFF_DIR/libsheriff_variant.so -ldl -o target

  (b) Ordinary dynamic linking: this approach is more flexible (you can
      change the location of the Sheriff library), but you must also
      set the `LD_LIBRARY_PATH` environment variable.

      % g++ target.o -LSHERIFF_DIR -lsheriff_variant -dl -o target
      % export LD_LIBRARY_PATH=SHERIFF_DIR:$LD_LIBRARY_PATH

When using Sheriff_Detect, all reports of any discovered false sharing
instances are printed out after the program finishes execution.

### Citing Sheriff ###

If you use Sheriff, we would appreciate hearing about it. To cite
Sheriff, please refer to the following paper, included as
[`sheriff-oopsla2011.pdf`](https://github.com/plasma-umass/sheriff/blob/master/sheriff-oopsla2011.pdf?raw=true).

```latex
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
```

