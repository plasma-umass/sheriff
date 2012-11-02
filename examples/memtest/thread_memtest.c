/* $Id: memtest.c,v 1.1.2.1 2010/05/14 13:44:25 sassek Exp $ */
/* support: sassek */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <fcntl.h>
#include <pthread.h>


#include <unistd.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <errno.h>
#include <dlfcn.h>

#include "memt.h"
int 		numrows;
int		align	= 0;
struct MEMINFO
{
	char	*real;
	char	*alngd;
} mem_data[50] = {{0,0}};



char *my_malloc( int size )
{
	char 	*pt;
	int		i, rem;

	size += 64;
	pt	= (char *)malloc( size );
	for( i = 0; ; i++ )
		if ( mem_data[i].alngd == NULL )
			break;
	mem_data[i].real = pt;
	if ( align == 2 )
	{
		rem	= (unsigned long)pt % 64;
		pt += ( 64 - rem );
	}
	mem_data[i].alngd	= pt;
	return(pt);
}

void my_free( void * pt )
{
	int i;

	for( i = 0; ; i++ )
	{
		if ( mem_data[i].alngd == pt )
			break;
		if ( mem_data[i].real == NULL )
			i++;
	}
	free( mem_data[i].real );
	mem_data[i].alngd	= NULL;
}


static void use_mem(struct THREADINFO *t, struct TKCMPEXEC * exech)
{
    int i;
    int j;
    struct CMPTHREAD *ti;
    int retstatus;
	struct CMPEXECS *s;

    for (j = 0; j < 1; j++)
    {
        for (i = 0; i < numrows; i++)
        {
#if 1
            exech->nexecerr = 0;
            exech->nmissgen = 0;

            if (*t->errflag) return;


            ti = &t->ti[t->tid];

            ti->m->rc = 0;
            ti->m->nerror = 0l;

			s = ti->s;
            s->selectpend = 0;
            s->skipflag = 2;
            *s->_error_ = 0.;
            if (s->_list_!=NULL) *s->_list_ = 0.;
            *s->_delta_ = 1e-8;
            *s->_der_   = (double) exech->der;
            *s->_epsilon_ = 2.0;
            *s->_epsilon_m1_ = 1.0;

            if (s->_n_)
              (*s->_n_)++;

            *(ti->op) = *(ti->op) + 1; /* FALSE_SHARING */

        //    if(ti->op == (int *)0x2aabbe4511f0)
        //      fprintf(stderr, "%d: changing ti->op %p\n", getpid(), ti->op);
            if (exech->jnlLog == NULL)  exech->jnlLog = ti->jrnlog_orig;
            else if (exech->jnlLog != ti->jrnlog) ti->jrnlog = exech->jnlLog;
            if (exech->jnlLst == NULL) exech->jnlLst = ti->jrnlst_orig;
            else if (exech->jnlLst != ti->jrnlst) ti->jrnlst = exech->jnlLst;

            ti->incodestream = 1;
            retstatus = 0;
            ti->incodestream = 0;
            ti->m->rc = retstatus;
#else
            t->entry(t, exech);
#endif
        }
    }
}

static pthread_t CreateThread(int (*ThreadFn)(void *args), void  *args)
{
    pthread_t thread;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    pthread_create(&thread, &attr, (void *(*)(void *))ThreadFn, args);

    pthread_attr_destroy(&attr);

    return thread;

}


static int doit(void *p)
{
    struct THREADINFO *t = (struct THREADINFO *)p;
    struct TKCMPEXEC exech;

	memset( &exech, 0, sizeof(struct TKCMPEXEC) );
    while(t->go == 0)
    {
        pthread_mutex_lock(&t->mut);
        if (t->go == 0)
            pthread_cond_wait(&t->cond, &t->mut);
        pthread_mutex_unlock(&t->mut);
    }

    exech.threadn = t->tid;
//    pi = my_malloc(64*sizeof(int));
//    for (i = 0; i < 64; i++)
//       pi[i] = i;

    use_mem(t, &exech);

//    my_free(pi);

    return 0;
}


int main(int argc, char *argv[])
{
    int i,j;
    int iter = ITER;
    pthread_t   *th;
    char	*mem, *mem2;
	char	*op, *op2;
	char	*ex, *ex2;
//    struct CMPEXECS *ex;
    struct CMPTHREAD *ti;
    struct PDV {
        double _error_;
        double _delta_;
        double _der_;
        double _list_;
        double _n_;
        double _epsilon_;
        double _epsilon_m1_;
        double pad;
    } *pdv;
    int *junk1;
    int *junk2;
    struct THREADINFO *t, *nt;
    int *errflag;
    int		size, msize, osize, esize;
    char	*var;
    int		nthreads = NUMTHREADS;

//    handle = dlopen("./memt.so", RTLD_NOW);

	var = getenv( "GRID_ALIGN" );
	if (var )
	{
		sscanf( var, "%d", &align );
		printf( "Alignment = %d \n", align );
	}

	var = getenv( "GRID_NTHREADS" );
	if (var )
	{
		sscanf( var, "%d", &nthreads );
		printf( "Running with %d threads\n", nthreads );
	}

	var = getenv( "GRID_ITER" );
	if (var )
	{
		sscanf( var, "%d", &iter );
		printf( "Running with %d iterations\n", iter );
	}

	var = getenv( "GRID_LENGTH" );
	i = 3;;
	if (var )
	{
		sscanf( var, "%d", &i );
		printf( "Running with length = %d\n", i );
	}
	numrows = NUMROWS * i;

    junk1 = (int *) my_malloc(MEMSIZE*sizeof(int));
    for (i = 0; i < MEMSIZE; i++)
        junk1[i] = i;

    junk2 = (int *) my_malloc(MEMSIZE*sizeof(int));
    for (i = 0; i < MEMSIZE; i++)
        junk2[i] = i;

	t	= (struct THREADINFO *) my_malloc( nthreads * sizeof(struct THREADINFO) );
	th  = (pthread_t *) my_malloc( nthreads * sizeof( pthread_t) );
	ti	= ( struct CMPTHREAD *) my_malloc( nthreads * sizeof( struct CMPTHREAD ) );

    for ( j = 0; j < iter; j++)
    {
        printf("iteration %d\n", j);

		// not only do the structures that make up the array need to be set on
		// a cache boundary, but we need to make sure the real memory is too.
		msize	= sizeof( struct CMPEXECMEM );
		if ( align )
			msize = msize + 63 - ((msize-1) % 64);
        mem = mem2 = my_malloc(nthreads * msize );

		osize	= sizeof( int );
		if ( align )
			osize = osize + 63 - ((osize-1) % 64);
        op = op2 = my_malloc(nthreads * osize ); /* FALSE_SHARING */
        memset(op, 0, nthreads*osize);

      fprintf(stderr, "%d: nthreads %d size %d, op address from %p to 0x%lx\n", getpid(), nthreads, osize, op, (intptr_t)op+nthreads*osize);

    //while(1) ;
		esize	= sizeof( struct CMPEXECS );
		if ( align )
			esize = esize + 63 - ((esize-1) % 64);
        ex = ex2 = my_malloc(nthreads * esize);
      
      fprintf(stderr, "%d: nthreads %d size %d, ex address from %p to 0x%lx\n", getpid(), nthreads,nthreads * esize, ex, (intptr_t)ex+nthreads*esize);

		size	= sizeof( *pdv);
		if ( align )
			size = size + 63 - ((size-1) % 64);
        pdv = (struct PDV *) my_malloc((nthreads * size) + 64);
        memset(pdv, 0, nthreads*size);

        errflag = (int *)my_malloc(sizeof(int));
        *errflag = 0;



        for (i = 0; i < nthreads; i++)
        {
            ti[i].m = (struct CMPEXECMEM *)&mem[i*msize];
            ti[i].op = (int *)&op[i*osize]; /* FALSE_SHARING */
            ti[i].s = (struct CMPEXECS *) &ex[i*esize];
//            printf( "%d\n", i );
//            printf( "mem  %x  %x\n", ti[i].m, (((char*)ti[i].m) + msize ));
//            printf( "op  %x %x\n", ti[i].op, ((char*)ti[i].op) + osize );
//            printf( "s  %x %x\n", ti[i].s, ((char*)ti[i].s) + esize );
//            printf( "pdv %x %x\n", (char *)&pdv[i], (char *)&pdv[i+1] );
            ti[i].s->_error_ = &pdv[i]._error_;
            ti[i].s->_delta_ = &pdv[i]._delta_;
            ti[i].s->_list_ = &pdv[i]._list_;
            ti[i].s->_der_ = &pdv[i]._der_;
            ti[i].s->_n_ = &pdv[i]._n_;
            ti[i].s->_epsilon_ = &pdv[i]._epsilon_;
            ti[i].s->_epsilon_m1_ = &pdv[i]._epsilon_m1_;

            ti[i].jrnlog_orig = &ti[i].jrnlog_orig;
            ti[i].jrnlog      = &ti[i].jrnlog_orig;
            ti[i].jrnlst_orig = &ti[i].jrnlst_orig;
            ti[i].jrnlst      = &ti[i].jrnlst_orig;

			nt	= &t[i];
            nt->ti = ti;
            nt->tid = i;
            nt->errflag = errflag;

            pthread_mutex_init(&nt->mut, NULL);
            pthread_cond_init(&nt->cond, NULL);
            nt->go = 0;

//            nt->entry = dlsym(handle, "lib_use_mem");
		}

        for (i = 0; i < nthreads; i++)
            th[i] = CreateThread(doit, &t[i]);

        for (i = 0; i < nthreads; i++)
        {
			nt	= &t[i];
            pthread_mutex_lock(&nt->mut);
            nt->go = 1;
            pthread_cond_signal(&nt->cond);
            pthread_mutex_unlock(&nt->mut);
        }

        for (i = 0; i < nthreads; i++)
        {
            void *ret;
            pthread_join(th[i], &ret);
        }

        for (i = 0; i < nthreads; i++)
        {
			nt	= &t[i];
            pthread_mutex_destroy(&nt->mut);
            pthread_cond_destroy(&nt->cond);
        }

        for (i = 0; i < MEMSIZE; i+=64)
           if (junk1[i] != junk2[i])
           {
                printf("bad\n");
                while(1) sleep(1);
           }


		    my_free( (void*)errflag );
        my_free((void*)pdv);
        my_free((void*)op2);
        my_free((void*)ex2);
        my_free((void*)mem2);
    }
    return 0;
}

