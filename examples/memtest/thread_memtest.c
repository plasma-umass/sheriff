
/* deliberately silly code */
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

static void use_mem(struct THREADINFO *t, struct TKCMPEXEC * exech)
{
    int i;
    int j;
    struct CMPTHREAD *ti;
    int retstatus;
    
    for (j = 0; j < 1; j++)
    {
        for (i = 0; i < NUMROWS; i++)
        {
#if 1
            exech->nexecerr = 0;
            exech->nmissgen = 0;
            
            if (*t->errflag) return;


            ti = &t->ti[t->tid];
            
            ti->m->rc = 0;
            ti->m->nerror = 0l;

            ti->s->selectpend = 0;
            ti->s->skipflag = 2;
            *ti->s->_error_ = 0.;
            if (ti->s->_list_!=NULL) *ti->s->_list_ = 0.;
            *ti->s->_delta_ = 1e-8;
            *ti->s->_der_   = (double) exech->der;
            *ti->s->_epsilon_ = 2.0;
            *ti->s->_epsilon_m1_ = 1.0;

            if (ti->s->_n_)
              (*ti->s->_n_)++;

            ti->op->der = exech->der;
            ti->op->quiet = exech->quiet;
            ti->op->errsave = exech->errsave;
            
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
    struct THREADINFO *t = p;
    struct TKCMPEXEC exech = {0};
    int *pi;
    int i;

    while(t->go == 0)
    {
        pthread_mutex_lock(&t->mut);
        if (t->go == 0)
            pthread_cond_wait(&t->cond, &t->mut);
        pthread_mutex_unlock(&t->mut);
    }
    
    exech.threadn = t->tid;
    pi = malloc(64*sizeof(int));
    for (i = 0; i < 64; i++)
       pi[i] = i;
    
    use_mem(t, &exech);

    free(pi);
    
    return 0;
}

int main(int argc, char *argv[])
{
    int i,j;
    char *p;
    int fd;
    int iters = 10;
    pthread_t   th[NUMTHREADS];
    double check, check0;
    int failures;
    struct CMPEXECMEM *mem;
    struct CMPEXECOP *op;
    struct CMPEXECS *ex;
    struct CMPTHREAD ti[NUMTHREADS];
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
    struct THREADINFO * t;
    int *errflag;
    void *handle;
    
    t = malloc (sizeof(struct THREADINFO) * NUMTHREADS);

//    handle = dlopen("./memt.so", RTLD_NOW);
    
    printf ("MOOOO 1!\n");

    printf ("memsize = %d\n", MEMSIZE * sizeof(int));

    junk1 = malloc(MEMSIZE*sizeof(int));
    for (i = 0; i < MEMSIZE; i++)
        junk1[i] = i;

    printf ("MOOOO 2!\n");
    junk2 = malloc(MEMSIZE*sizeof(int));
    for (i = 0; i < MEMSIZE; i++)
        junk2[i] = i;

    printf ("MOOOO 3!\n");
    
    for ( j = 0; j < 10; j++)
    {
        printf("iteration %d\n", j);
        
        mem = malloc(NUMTHREADS * sizeof(*mem));
        op = malloc(NUMTHREADS * sizeof(*op));
        ex = malloc(NUMTHREADS * sizeof(*ex));
        
        pdv = malloc(NUMTHREADS * sizeof(*pdv));
        memset(pdv, 0, NUMTHREADS*sizeof(*pdv));
        
        errflag = malloc(sizeof(int));
        *errflag = 0;


        memset(op, 0, NUMTHREADS*sizeof(sizeof(*op)));
    
        for (i = 0; i < NUMTHREADS; i++)
        {
            ti[i].m = &mem[i];
            ti[i].op = &op[i];
            ti[i].s = &ex[i];
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

            t[i].ti = ti;
            t[i].tid = i;
            t[i].errflag = errflag;
            
            pthread_mutex_init(&t[i].mut, NULL);
            pthread_cond_init(&t[i].cond, NULL);
            t[i].go = 0;

//            t[i].entry = dlsym(handle, "lib_use_mem");
        }

        for (i = 0; i < NUMTHREADS; i++)
            th[i] = CreateThread(doit, &t[i]);

        for (i = 0; i < NUMTHREADS; i++)
        {
            pthread_mutex_lock(&t[i].mut);
            t[i].go = 1;
            pthread_cond_signal(&t[i].cond);
            pthread_mutex_unlock(&t[i].mut);
        }
    
        for (i = 0; i < NUMTHREADS; i++)
        {
            void *ret;
            pthread_join(th[i], &ret);
        }

        for (i = 0; i < NUMTHREADS; i++)
        {
            pthread_mutex_destroy(&t[i].mut);
            pthread_cond_destroy(&t[i].cond);
        }
        
        for (i = 0; i < MEMSIZE; i+=64)
           if (junk1[i] != junk2[i]) 
           {
                printf("bad\n");
                while(1) sleep(1);
           }

            
        
        free(pdv);
        free(op);
        free(ex);
        free(mem);
    }
    return 0;
}
