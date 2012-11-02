//#define NUMCPUS 24
//#define NUMCPUS 8
#define NUMCPUS 24
#define NUMTHREADS NUMCPUS
#define NUMROWS 1000000
//#define ITER 10
#define ITER 10
//#define MEMSIZE (200 * 1024 * 1024)
//#define MEMSIZE (20 * 1024 * 1024)
#define MEMSIZE (20 * 1024 * 1024)

struct CMPEXECOP  {
         char       der;         /* execute derivative code         */
         char       quiet;       /* ignor errors & missing values   */
         char       errsave;     /* save (^ print) execution errors */
         } ;

struct CMPEXECS {
         double   * _error_;      /* the _ERROR_ variable            */
         double   * _list_;       /* the _LIST_ variable             */
         double   * _n_;          /* the _N_ variable                */
         double   * _delta_;      /* _DELTA_ var for numeric derivs  */
         double   * _der_;        /* _DER_ var flag                  */
         double   * _i_;          /* _I_ default index variable      */
         double   * _epsilon_;    /* _EPSILON_ for bound stmt        */
         double   * _epsilon_m1_; /* _EPSILON_M1_ for bound stmt     */

         int        outputflag;   /* flag for default OUTPUT;        */
         int        skipflag;     /* flag for PUT stmt trailing "@"  */
         int        selectpend;   /* flag SELECT looking for WHEN    */
         int      * missflag;     /* used in optimization ---------  */

         double     _epsilon_buf;
         double     _epsilon_m1_buf;

         } ;

 struct CMPEXECMEM {

      void *       pool;        /* storage pool                    */

      int           rc;          /* execution return code           */

                                 /* function user exit stmts call   */
      void       *userexit;

      void *     errorlist;   /* list of execution errors        */
      void *     errorlast;   /* last execution error            */
      void *      errorfree;   /* free execution error structures */
      long          nerror;      /* number of execution errors      */

      void *    misslist;    /* list of missing value items     */
      void *    misslast;    /* last missing value item         */
      void *    missfree;    /* free missgen structures         */
      long          nmissgen;    /* number of generated missing val */

      void *         xst;         /* current executing statement     */

      void *           fid;         /* file id pointer for OUTPUT      */
      void *          xvp;         /* xvput pointer for OUTPUT        */
      };

struct TKCMPEXEC {

  int seg;                 // code segment to run
  int der;                 // execute derivative code
  int quiet;               // ignore execution errors and missing values
  int errsave;             // queue up execution errors

  char** updvs;            // array of user-defined pdvs
  int    threadn;          // current thread number
  int    init_updv_chars;  // should cmp update X_Strings in updvs?

  // return information
  int nexecerr;            // num execution errors
  int nmissgen;            // num missing values generated

  void *      jnlLog;      // log jnl used at run-time
  void *      jnlLst;      // lst jnl used at run-time

};

 struct CMPTHREAD {
   void *     *  pdvs;          /* array a PDVs                        */
   void *        stack_pdv;     /* current PDV on the stack            */
   void *    tk;            /* threaded kernal instance handle     */

   void *   tktExHandler;   // our Exception Handler
   int   exceptionCode;  // set by Exception handler
   void    ** recovery_code;  // TKG recovery code
   void     * tktjumpbuf;     // TKT jump buf
   void *  tkthread;

   void *      st;          /* current stmt                 */
   void *      oplist;      /* current operation            */

   double   * _der_;
   int        errflag;
   int64_t  memuse;

   struct CMPEXECS   * s;  /* execution symbol values      */
   struct CMPEXECMEM * m;  /* execution memory information */
   int  * op; /* execution options            */

   struct CMPTKGCONTEXT * ctx;     /* CMP TKG context      */
   long                   threadn; /* thread number        */

   /*- run-time link handling ----------------------------*/
   unsigned int  linkstack[256];
   int           linkdepth;


   char             putdest;      /* current put destination  */
   char             incodestream; /* in the TKG codestream?   */
   int              fbrg_rc;      /* rc set by bridge funcs   */
   int              funcdepth;
   struct CMPFUNC * func;
   char           * funcname;

   void *      bestTkFmth;

   int         buf[256+1];    /* print buffer */
   long      bufl;

   void *         jrnlst;      /* jnl for listing    */
   void *         jrnlog;      /* jnl for logging    */
   char           jrnlstUsed;
   char           jrnlogUsed;

   char * cResultBuffer;
   char   cResultBufferUsed;

   char   runInitialized;

   char   upcase[256]; // moved from cmppar for threading

   void *         jrnlst_orig;
   void *         jrnlog_orig;

};

struct THREADINFO {
    struct CMPTHREAD *ti;
    int tid;
    pthread_mutex_t mut;
    pthread_cond_t cond;
    int go;
    int *errflag;
    void (*entry)(struct THREADINFO *t, struct TKCMPEXEC * exech);

};


