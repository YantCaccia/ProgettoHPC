/*************************************************************************
 *                                                                       * 
 *       N  A  S     P A R A L L E L     B E N C H M A R K S  3.4        *
 *                                                                       *
 *                      O p e n M P     V E R S I O N                    *
 *                                                                       * 
 *                                  I S                                  * 
 *                                                                       * 
 ************************************************************************* 
 *                                                                       * 
 *   This benchmark is an OpenMP version of the NPB IS code.             *
 *   It is described in NAS Technical Report 99-011.                     *
 *                                                                       *
 *   Permission to use, copy, distribute and modify this software        *
 *   for any purpose with or without fee is hereby granted.  We          *
 *   request, however, that all derived work reference the NAS           *
 *   Parallel Benchmarks 3.4. This software is provided "as is"          *
 *   without express or implied warranty.                                *
 *                                                                       *
 *   Information on NPB 3.4, including the technical report, the         *
 *   original specifications, source code, results and information       *
 *   on how to submit new results, is available at:                      *
 *                                                                       *
 *          http://www.nas.nasa.gov/Software/NPB/                        *
 *                                                                       *
 *   Send comments or suggestions to  npb@nas.nasa.gov                   *
 *                                                                       *
 *         NAS Parallel Benchmarks Group                                 *
 *         NASA Ames Research Center                                     *
 *         Mail Stop: T27A-1                                             *
 *         Moffett Field, CA   94035-1000                                *
 *                                                                       *
 *         E-mail:  npb@nas.nasa.gov                                     *
 *         Fax:     (650) 604-3957                                       *
 *                                                                       *
 ************************************************************************* 
 *                                                                       * 
 *   Author: M. Yarrow                                                   * 
 *           H. Jin                                                      * 
 *                                                                       * 
 *************************************************************************/

#include "npbparams.h"
#include <stdlib.h>
#include <stdio.h>
#include <sycl/sycl.hpp>
#ifdef _OPENMP
#include <omp.h>
#endif

using namespace sycl;
/*****************************************************************/
/* For serial IS, buckets are not really req'd to solve NPB1 IS  */
/* spec, but their use on some machines improves performance, on */
/* other machines the use of buckets compromises performance,    */
/* probably because it is extra computation which is not req'd.  */
/* (Note: Mechanism not understood, probably cache related)      */
/* Example:  SP2-66MhzWN:  50% speedup with buckets              */
/* Example:  SGI Indy5000: 50% slowdown with buckets             */
/* Example:  SGI O2000:   400% slowdown with buckets (Wow!)      */
/*****************************************************************/
/* To disable the use of buckets, comment out the following line */
   #define USE_BUCKETS

/* Uncomment below for cyclic schedule */
/*#define SCHED_CYCLIC*/


/******************/
/* default values */
/******************/
#ifndef CLASS
#define CLASS 'S'
#endif


/*************/
/*  CLASS S  */
/*************/
#if CLASS == 'S'
#define  TOTAL_KEYS_LOG_2    16
#define  MAX_KEY_LOG_2       11
#define  NUM_BUCKETS_LOG_2   9
#endif


/*************/
/*  CLASS W  */
/*************/
#if CLASS == 'W'
#define  TOTAL_KEYS_LOG_2    20
#define  MAX_KEY_LOG_2       16
#define  NUM_BUCKETS_LOG_2   10
#endif

/*************/
/*  CLASS A  */
/*************/
#if CLASS == 'A'
#define  TOTAL_KEYS_LOG_2    23
#define  MAX_KEY_LOG_2       19
#define  NUM_BUCKETS_LOG_2   10
#endif


/*************/
/*  CLASS B  */
/*************/
#if CLASS == 'B'
#define  TOTAL_KEYS_LOG_2    25
#define  MAX_KEY_LOG_2       21
#define  NUM_BUCKETS_LOG_2   10
#endif


/*************/
/*  CLASS C  */
/*************/
//#if CLASS == 'C'
//#define  TOTAL_KEYS_LOG_2    27
//#define  MAX_KEY_LOG_2       23
//#define  NUM_BUCKETS_LOG_2   10
//#endif
#if CLASS == 'C'
#define  TOTAL_KEYS_LOG_2    27
#define  MAX_KEY_LOG_2       27
#define  NUM_BUCKETS_LOG_2   10
#endif


/*************/
/*  CLASS D  */
/*************/
#if CLASS == 'D'
#define  TOTAL_KEYS_LOG_2    31
#define  MAX_KEY_LOG_2       27
#define  NUM_BUCKETS_LOG_2   10
#endif


/*************/
/*  CLASS E  */
/*************/
#if CLASS == 'E'
#define  TOTAL_KEYS_LOG_2    35
#define  MAX_KEY_LOG_2       31
#define  NUM_BUCKETS_LOG_2   10
#endif


#if (CLASS == 'D' || CLASS == 'E')
#define  TOTAL_KEYS          (1L << TOTAL_KEYS_LOG_2)
#define  TOTAL_KS1           (1 << (TOTAL_KEYS_LOG_2-8))
#define  TOTAL_KS2           (1 << 8)
#define  MAX_KEY             (1L << MAX_KEY_LOG_2)
#else
#define  TOTAL_KEYS          (1 << TOTAL_KEYS_LOG_2)
#define  TOTAL_KS1           TOTAL_KEYS
#define  TOTAL_KS2           1
#define  MAX_KEY             (1 << MAX_KEY_LOG_2)
#endif
#define  NUM_BUCKETS         (1 << NUM_BUCKETS_LOG_2)
#define  NUM_KEYS            TOTAL_KEYS
#define  SIZE_OF_BUFFERS     NUM_KEYS  
                                           

#define  MAX_ITERATIONS      10
#define  TEST_ARRAY_SIZE     5


/*************************************/
/* Typedef: if necessary, change the */
/* size of int here by changing the  */
/* int type to, say, long            */
/*************************************/
#if (CLASS == 'D' || CLASS == 'E')
typedef  long INT_TYPE;
#else
typedef  int  INT_TYPE;
#endif


/********************/
/* Some global info */
/********************/
INT_TYPE *key_buff_ptr_global;         /* used by full_verify to get */
                                       /* copies of rank info        */

int      passed_verification;
                                 

/************************************/
/* These are the three main arrays. */
/* See SIZE_OF_BUFFERS def above    */
/************************************/
INT_TYPE key_array[SIZE_OF_BUFFERS],    
         key_buff1[MAX_KEY],
         key_buff2[SIZE_OF_BUFFERS],
         partial_verify_vals[TEST_ARRAY_SIZE],
         **key_buff1_aptr = NULL;

#ifdef USE_BUCKETS
INT_TYPE **bucket_size, 
         bucket_ptrs[NUM_BUCKETS];
#pragma omp threadprivate(bucket_ptrs)
#endif


/**********************/
/* Partial verif info */
/**********************/
INT_TYPE test_index_array[TEST_ARRAY_SIZE],
         test_rank_array[TEST_ARRAY_SIZE];

int      S_test_index_array[TEST_ARRAY_SIZE] = 
                             {48427,17148,23627,62548,4431},
         S_test_rank_array[TEST_ARRAY_SIZE] = 
                             {0,18,346,64917,65463},

         W_test_index_array[TEST_ARRAY_SIZE] = 
                             {357773,934767,875723,898999,404505},
         W_test_rank_array[TEST_ARRAY_SIZE] = 
                             {1249,11698,1039987,1043896,1048018},

         A_test_index_array[TEST_ARRAY_SIZE] = 
                             {2112377,662041,5336171,3642833,4250760},
         A_test_rank_array[TEST_ARRAY_SIZE] = 
                             {104,17523,123928,8288932,8388264},

         B_test_index_array[TEST_ARRAY_SIZE] = 
                             {41869,812306,5102857,18232239,26860214},
         B_test_rank_array[TEST_ARRAY_SIZE] = 
                             {33422937,10244,59149,33135281,99}, 

         C_test_index_array[TEST_ARRAY_SIZE] = 
                             {44172927,72999161,74326391,129606274,21736814},
         C_test_rank_array[TEST_ARRAY_SIZE] = 
                             {61147,882988,266290,133997595,133525895};

long     D_test_index_array[TEST_ARRAY_SIZE] = 
                             {1317351170,995930646,1157283250,1503301535,1453734525},
         D_test_rank_array[TEST_ARRAY_SIZE] = 
                             {1,36538729,1978098519,2145192618,2147425337},

         E_test_index_array[TEST_ARRAY_SIZE] = 
                             {21492309536L,24606226181L,12608530949L,4065943607L,3324513396L},
         E_test_rank_array[TEST_ARRAY_SIZE] = 
                             {3L,27580354L,3248475153L,30048754302L,31485259697L};


/***********************/
/* function prototypes */
/***********************/
double	randlc( double *X, double *A );

void full_verify( sycl::queue& q );

void c_print_results( char   *name,
                      char   class_,
                      int    n1, 
                      int    n2,
                      int    n3,
                      int    niter,
                      double t,
                      double mops,
		      char   *optype,
                      int    passed_verification,
                      char   *npbversion,
                      char   *compiletime,
                      char   *cc,
                      char   *clink,
                      char   *c_lib,
                      char   *c_inc,
                      char   *cflags,
                      char   *clinkflags );

#include "../common/c_timers.h"

const INT_TYPE toTest = NUM_BUCKETS;

/*
 *    FUNCTION RANDLC (X, A)
 *
 *  This routine returns a uniform pseudorandom double precision number in the
 *  range (0, 1) by using the linear congruential generator
 *
 *  x_{k+1} = a x_k  (mod 2^46)
 *
 *  where 0 < x_k < 2^46 and 0 < a < 2^46.  This scheme generates 2^44 numbers
 *  before repeating.  The argument A is the same as 'a' in the above formula,
 *  and X is the same as x_0.  A and X must be odd double precision integers
 *  in the range (1, 2^46).  The returned value RANDLC is normalized to be
 *  between 0 and 1, i.e. RANDLC = 2^(-46) * x_1.  X is updated to contain
 *  the new seed x_1, so that subsequent calls to RANDLC using the same
 *  arguments will generate a continuous sequence.
 *
 *  This routine should produce the same results on any computer with at least
 *  48 mantissa bits in double precision floating point data.  On Cray systems,
 *  double precision should be disabled.
 *
 *  David H. Bailey     October 26, 1990
 *
 *     IMPLICIT DOUBLE PRECISION (A-H, O-Z)
 *     SAVE KS, R23, R46, T23, T46
 *     DATA KS/0/
 *
 *  If this is the first call to RANDLC, compute R23 = 2 ^ -23, R46 = 2 ^ -46,
 *  T23 = 2 ^ 23, and T46 = 2 ^ 46.  These are computed in loops, rather than
 *  by merely using the ** operator, in order to insure that the results are
 *  exact on all systems.  This code assumes that 0.5D0 is represented exactly.
 */

/*****************************************************************/
/*************           R  A  N  D  L  C             ************/
/*************                                        ************/
/*************    portable random number generator    ************/
/*****************************************************************/

//static int      KS=0;
//static double	R23, R46, T23, T46;
//#pragma omp threadprivate(KS, R23, R46, T23, T46)

SYCL_EXTERNAL double	randlc( double *X, double *A ){
    int KS=0;
    double	R23, R46, T23, T46;

    double		T1, T2, T3, T4;
    double		A1;
    double		A2;
    double		X1;
    double		X2;
    double		Z;
    int     		i, j;

    if (KS == 0) 
    {
    R23 = 1.0;
    R46 = 1.0;
    T23 = 1.0;
    T46 = 1.0;

    for (i=1; i<=23; i++)
    {
        R23 = 0.50 * R23;
        T23 = 2.0 * T23;
    }
    for (i=1; i<=46; i++)
    {
        R46 = 0.50 * R46;
        T46 = 2.0 * T46;
    }
    KS = 1;
    }

/*  Break A into two parts such that A = 2^23 * A1 + A2 and set X = N.  */

      T1 = R23 * *A;
      j  = T1;
      A1 = j;
      A2 = *A - T23 * A1;

/*  Break X into two parts such that X = 2^23 * X1 + X2, compute
    Z = A1 * X2 + A2 * X1  (mod 2^23), and then
    X = 2^23 * Z + A2 * X2  (mod 2^46).                            */

      T1 = R23 * *X;
      j  = T1;
      X1 = j;
      X2 = *X - T23 * X1;
      T1 = A1 * X2 + A2 * X1;
      
      j  = R23 * T1;
      T2 = j;
      Z = T1 - T23 * T2;
      T3 = T23 * Z + A2 * X2;
      j  = R46 * T3;
      T4 = j;
      *X = T3 - T46 * T4;
      return(R46 * *X);
}




/*****************************************************************/
/************   F  I  N  D  _  M  Y  _  S  E  E  D    ************/
/************                                         ************/
/************ returns parallel random number seq seed ************/
/*****************************************************************/

/*
 * Create a random number sequence of total length nn residing
 * on np number of processors.  Each processor will therefore have a
 * subsequence of length nn/np.  This routine returns that random
 * number which is the first random number for the subsequence belonging
 * to processor rank kn, and which is used as seed for proc kn ran # gen.
 */

double   find_my_seed( int kn,        /* my processor rank, 0<=kn<=num procs */
                       int np,        /* np = num procs                      */
                       long nn,       /* total num of ran numbers, all procs */
                       double s,      /* Ran num seed, for ex.: 314159265.00 */
                       double a )     /* Ran num gen mult, try 1220703125.00 */
{

      double t1,t2;
      long   mq,nq,kk,ik;

      if ( kn == 0 ) return s;

      mq = (nn/4 + np - 1) / np;
      nq = mq * 4 * kn;               /* number of rans to be skipped */

      t1 = s;
      t2 = a;
      kk = nq;
      while ( kk > 1 ) {
      	 ik = kk / 2;
         if( 2 * ik ==  kk ) {
            (void)randlc( &t2, &t2 );
	    kk = ik;
	 }
	 else {
            (void)randlc( &t1, &t2 );
	    kk = kk - 1;
	 }
      }
      (void)randlc( &t1, &t2 );

      return( t1 );

}



/*****************************************************************/
/*************      C  R  E  A  T  E  _  S  E  Q      ************/
/*****************************************************************/

void	create_seq( double seed, double a )
{
// 	double x, s;
	INT_TYPE i, k;

// #pragma omp parallel private(x,s,i,k)
//     {
// 	INT_TYPE k1, k2;
// 	double an = a;
// 	int myid = 0, num_threads = 1;
//         INT_TYPE mq;

// #ifdef _OPENMP
// 	myid = omp_get_thread_num();
// 	num_threads = omp_get_num_threads();
// #endif

// 	mq = (NUM_KEYS + num_threads - 1) / num_threads;
// 	k1 = mq * myid;
// 	k2 = k1 + mq;
// 	if ( k2 > NUM_KEYS ) k2 = NUM_KEYS;

// 	KS = 0;
// 	s = find_my_seed( myid, num_threads,
// 			  (long)4*NUM_KEYS, seed, an );

//         k = MAX_KEY/4;

// 	for (i=k1; i<k2; i++)
// 	{
// 	    x = randlc(&s, &an);
// 	    x += randlc(&s, &an);
//         x += randlc(&s, &an);
// 	    x += randlc(&s, &an);  

//         key_array[i] = k*x;
// 	}
//     } /*omp parallel*/
    
    /* --- SYCL CODE --- */
    queue q(cpu_selector_v);

	double *an;

    an = malloc_shared<double>(1,q);
    *an=a;


    k = MAX_KEY/4;
    int *key_array_device = malloc_device<INT_TYPE>(SIZE_OF_BUFFERS, q);
    
    q.submit([&] (handler& h){

        h.parallel_for(range<1>{NUM_KEYS}, [=] (item<1> i) {
            double local_s = find_my_seed( i, NUM_KEYS,
                                    (long)4*NUM_KEYS, seed, *an); 
            double x;
            x = randlc(&local_s, an);
	        x += randlc(&local_s, an);
            x += randlc(&local_s, an);
	        x += randlc(&local_s, an);
            key_array_device[i] = k*x;
        });
    });
    q.wait();

    q.memcpy(key_array, key_array_device, SIZE_OF_BUFFERS * sizeof(INT_TYPE)).wait();

    free(key_array_device, q);
    free(an, q);
    /* END OF SYCL CODE*/
}



/*****************************************************************/
/*****************    Allocate Working Buffer     ****************/
/*****************************************************************/
void *alloc_mem( size_t size )
{
    void *p;

    p = (void *)malloc(size);
    if (!p) {
        perror("Memory allocation error");
        exit(1);
    }
    return p;
}

void alloc_key_buff( sycl::queue& q )
{
    INT_TYPE i;
    int      num_threads = 1;


#ifdef _OPENMP
    num_threads = omp_get_max_threads();
#endif

#ifdef USE_BUCKETS
    bucket_size = (INT_TYPE **)alloc_mem(sizeof(INT_TYPE *) * num_threads);

    for (i = 0; i < num_threads; i++) {
        bucket_size[i] = (INT_TYPE *)alloc_mem(sizeof(INT_TYPE) * NUM_BUCKETS);
    }

    /* #pragma omp parallel for
    for( i=0; i<NUM_KEYS; i++ )
        key_buff2[i] = 0;
    */
   
    /* --- SYCL CODE --- */
    
    INT_TYPE *key_buff2_device = malloc_device<INT_TYPE>(SIZE_OF_BUFFERS, q);
    q.memcpy(key_buff2_device, key_buff2, SIZE_OF_BUFFERS * sizeof(INT_TYPE)).wait();
    
    range<1> num_items(NUM_KEYS);
    q.submit([&] (handler& h){
        h.parallel_for(num_items, [=] (id<1> i){
            key_buff2_device[i[0]]=0;
        });
    });
    q.wait();

    q.memcpy(key_buff2, key_buff2_device, SIZE_OF_BUFFERS * sizeof(INT_TYPE)).wait();
    free(key_buff2_device, q);
    /* END OF SYCL CODE*/
#else /*USE_BUCKETS*/

    key_buff1_aptr = (INT_TYPE **)alloc_mem(sizeof(INT_TYPE *) * num_threads);

    key_buff1_aptr[0] = key_buff1;
    for (i = 1; i < num_threads; i++) {
        key_buff1_aptr[i] = (INT_TYPE *)alloc_mem(sizeof(INT_TYPE) * MAX_KEY);
    }

#endif /*USE_BUCKETS*/
}



/*****************************************************************/
/*************    F  U  L  L  _  V  E  R  I  F  Y     ************/
/*****************************************************************/


void full_verify(sycl::queue& q)
{
    INT_TYPE   i, j;
    INT_TYPE   k, k1, k2;


/*  Now, finally, sort the keys:  */

/*  Copy keys into work array; keys in key_array will be reassigned. */

#ifdef USE_BUCKETS

/* Buckets are already sorted.  Sorting keys within each bucket */

// #ifdef SCHED_CYCLIC
//     #pragma omp parallel for private(i,j,k,k1) schedule(static,1)
// #else
//     #pragma omp parallel for private(i,j,k,k1) schedule(dynamic)
// #endif
//     for( j=0; j< NUM_BUCKETS; j++ ) {

//         k1 = (j > 0)? bucket_ptrs[j-1] : 0;
//         for ( i = k1; i < bucket_ptrs[j]; i++ ) {
//             k = --key_buff_ptr_global[key_buff2[i]];
//             key_array[k] = key_buff2[i];
//         }
//     }

    /* --- SYCL CODE --- */
    
    buffer<INT_TYPE> bucket_ptrs_buf(bucket_ptrs, range<1>(NUM_BUCKETS));
    buffer<INT_TYPE> key_buff2_buf(key_buff2, range<1>(SIZE_OF_BUFFERS));
    buffer<INT_TYPE> key_buff_ptr_global_buf(key_buff_ptr_global, range<1>(MAX_KEY));
    buffer<INT_TYPE> key_array_buf(key_array, range<1>(SIZE_OF_BUFFERS));



    range<1> num_iterations(NUM_BUCKETS);
    q.submit([&] (handler& h){
        
        auto bucket_ptrs_acc = bucket_ptrs_buf.get_access<access::mode::read>(h);
        auto key_buff2_acc = key_buff2_buf.get_access<access::mode::read>(h);
        auto key_buff_ptr_global_acc = key_buff_ptr_global_buf.get_access<access::mode::read_write>(h);
        auto key_array_acc = key_array_buf.get_access<access::mode::read_write>(h);
        
        h.parallel_for(num_iterations, [=] (id<1> j){
            INT_TYPE k1 = (j[0] > 0) ? bucket_ptrs_acc[j[0] - 1] : 0;
            for (INT_TYPE i = k1; i < bucket_ptrs_acc[j]; i++) {
                int index = key_buff2_acc[i];
                auto atomic_obj = atomic_ref<int, memory_order::relaxed, memory_scope::device, access::address_space::global_space>(key_buff_ptr_global_acc[index]);
                atomic_obj.fetch_add(-1);
                int k = atomic_obj;
                auto atomic_obj_key_array = atomic_ref<int, memory_order::relaxed, memory_scope::device, access::address_space::global_space>(key_array_acc[k]);
                atomic_obj_key_array.store(key_buff2_acc[i]);
            }
        });

    });
    q.wait();

    // TODO: this part of copying back to the host can be omitted as subsequent operations (sum computation) is still done on the device
    // sycl::host_accessor key_buff_ptr_global_a(key_buff_ptr_global_buf);
    // sycl::host_accessor key_array_acc_a(key_array_buf);

    // key_buff_ptr_global = &key_buff_ptr_global_a[0];
    // q.memcpy(key_array, &key_array_acc_a[0], SIZE_OF_BUFFERS * sizeof(INT_TYPE)).wait();

    /* END OF SYCL CODE*/

#else /*USE_BUCKETS disabled*/

// #pragma omp parallel private(i,j,k,k1,k2)
//   {
//     #pragma omp for
//     for( i=0; i<NUM_KEYS; i++ )
//         key_buff2[i] = key_array[i];

//     /* This is actual sorting. Each thread is responsible for 
//        a subset of key values */
// #ifdef _OPENMP
//     j = omp_get_num_threads();
//     j = (MAX_KEY + j - 1) / j;
//     k1 = j * omp_get_thread_num();
// #else
//     j = MAX_KEY;
//     k1 = 0;
// #endif
//     k2 = k1 + j;
//     if (k2 > MAX_KEY) k2 = MAX_KEY;

//     for( i=0; i<NUM_KEYS; i++ ) {
//         if (key_buff2[i] >= k1 && key_buff2[i] < k2) {
//             k = --key_buff_ptr_global[key_buff2[i]];
//             if(k==0)
//                 printf("key_buff2:%d, i:%d\n", key_buff2[i], i);
//             key_array[k] = key_buff2[i];
//         }
//     }
//   } /*omp parallel*/    

    
    /* --- SYCL CODE --- */
    // First for
    buffer<INT_TYPE> key_array_buf{key_array, range<1>(SIZE_OF_BUFFERS)};
    buffer<INT_TYPE> key_buff2_buf{key_buff2, range<1>(SIZE_OF_BUFFERS)};
    
    range<1> num_iterations(NUM_KEYS);
    
    q.submit([&] (handler& h){
        auto key_buff2_acc = key_buff2_buf.get_access<access::mode::read_write>(h);
        auto key_array_acc = key_array_buf.get_access<access::mode::read>(h);
        
        h.parallel_for(num_iterations, [=] (item<1> i){
            key_buff2_acc[i[0]]=key_array_acc[i[0]];
        });
    });
    q.wait();   
    
    // Second for
    k1 = 0;
    k2 = MAX_KEY;

    buffer<INT_TYPE> key_buff_ptr_global_buf(key_buff_ptr_global, range<1>(MAX_KEY));

    q.submit([&] (handler& h){
        
        auto key_buff2_acc = key_buff2_buf.get_access<access::mode::read>(h);
        auto key_buff_ptr_global_acc = key_buff_ptr_global_buf.get_access<access::mode::read_write>(h);
        auto key_array_acc = key_array_buf.get_access<access::mode::read_write>(h);
        
        h.parallel_for(num_iterations, [=] (id<1> i){
            if(key_buff2_acc[i[0]] >= k1 && key_buff2_acc[i[0]] < k2){
                int index = key_buff2_acc[i[0]];
                auto atomic_obj = atomic_ref<int, memory_order::relaxed, memory_scope::device, access::address_space::global_space>(key_buff_ptr_global_acc[index]);
                INT_TYPE k = atomic_obj.fetch_add(-1);
                k--;
                auto atomic_obj_key_array = atomic_ref<int, memory_order::relaxed, memory_scope::device, access::address_space::global_space>(key_array_acc[k]);
                atomic_obj_key_array.store(key_buff2_acc[i[0]]);
            }
        });
    });
    q.wait();

    /* END OF SYCL CODE*/
    
#endif


/*  Confirm keys correctly sorted: count incorrectly sorted keys, if any */

    j = 0;
    // #pragma omp parallel for reduction(+:j)
    // for( i=1; i<NUM_KEYS; i++ )
    //     if( key_array[i-1] > key_array[i] ){
    //         printf("i-1:%d, i:%d\nkey_array-1:%d, key_array:%d\n", i-1, i, key_array[i-1], key_array[i]);
    //         j++;
    //     }
    
    /* --- SYCL CODE --- */

   buffer<INT_TYPE> sumBuf { &j, 1 };

    //buffer<INT_TYPE> key_array_buf{key_array, range<1>(NUM_KEYS)};

    q.submit([&](handler& h) {
        auto key_array_acc = key_array_buf.get_access<access_mode::read>(h);

        auto sumReduction = reduction(sumBuf, h, plus<>());

        h.parallel_for(range<1> { NUM_KEYS - 1}, sumReduction, [=](id<1> i, auto& sum) {
            if(key_array_acc[i[0]]>key_array_acc[i[0]+1]){
                sum += 1;
            }
        });

    });
    q.wait();

    j = sumBuf.get_host_access()[0];

    /* END OF SYCL CODE */

    if( j != 0 )
        printf( "Full_verify: number of keys out of sort: %ld\n", (long)j );
    else
        passed_verification++;

}




/*****************************************************************/
/*************             R  A  N  K             ****************/
/*****************************************************************/


void rank( int iteration, sycl::queue& q )
{

    INT_TYPE    i, k;
    INT_TYPE    *key_buff_ptr, *key_buff_ptr2;

#ifdef USE_BUCKETS
    int shift = MAX_KEY_LOG_2 - NUM_BUCKETS_LOG_2;
    INT_TYPE num_bucket_keys = (1L << shift);
#endif


    key_array[iteration] = iteration;
    key_array[iteration+MAX_ITERATIONS] = MAX_KEY - iteration;


/*  Determine where the partial verify test keys are, load into  */
/*  top of array bucket_size                                     */
    for( i=0; i<TEST_ARRAY_SIZE; i++ )
        partial_verify_vals[i] = key_array[test_index_array[i]];


/*  Setup pointers to key buffers  */
#ifdef USE_BUCKETS
    key_buff_ptr2 = key_buff2;
#else
    key_buff_ptr2 = key_array;
#endif
    key_buff_ptr = key_buff1;


#pragma omp parallel private(i, k)
  {
    INT_TYPE *work_buff, m, k1, k2;
    int myid = 0, num_threads = 1;

#ifdef _OPENMP
    myid = omp_get_thread_num();
    num_threads = omp_get_num_threads();
#endif


/*  Bucket sort is known to improve cache performance on some   */
/*  cache based systems.  But the actual performance may depend */
/*  on cache size, problem size. */
#ifdef USE_BUCKETS

    work_buff = bucket_size[myid];

/*  Initialize */
    for( i=0; i<NUM_BUCKETS; i++ )  
        work_buff[i] = 0;

/*  Determine the number of keys in each bucket */
    // #pragma omp for schedule(static)
    // for( i=0; i<NUM_KEYS; i++ )
    //     work_buff[key_array[i] >> shift]++;

    /* --- SYCL CODE --- */

    // Buffers for arrays
    buffer<INT_TYPE> key_array_buf{key_array, range<1>(SIZE_OF_BUFFERS)};
    buffer<INT_TYPE> work_buff_buf{work_buff, range<1>(NUM_BUCKETS)};

    // Create a range for the number of items
    range<1> num_items(NUM_KEYS);

    // Submit the kernel with a parallel for loop that increments the work_buff_device
    q.submit([&] (handler& h){

        // Accessors
        auto key_array_acc = key_array_buf.get_access<access_mode::read>(h);
        auto work_buff_acc = work_buff_buf.get_access<access_mode::read_write>(h);

        h.parallel_for(num_items, [=] (id<1> i){
            auto v = atomic_ref<INT_TYPE, memory_order::relaxed, memory_scope::device, access::address_space::global_space>(work_buff_acc[key_array_acc[i[0]] >> shift]);
            v.fetch_add(1);
        });
    });
    q.wait();

    sycl::host_accessor h_a(work_buff_buf);
    work_buff = &h_a[0];
    q.memcpy(bucket_size[myid], work_buff, NUM_BUCKETS * sizeof(INT_TYPE)).wait();
    
    /* END OF SYCL CODE */


/*  Accumulative bucket sizes are the bucket pointers.
    These are global sizes accumulated upon to each bucket */
    bucket_ptrs[0] = 0;
    for( k=0; k< myid; k++ )  
        bucket_ptrs[0] += bucket_size[k][0];

    for( i=1; i< NUM_BUCKETS; i++ ) { 
        bucket_ptrs[i] = bucket_ptrs[i-1];
        for( k=0; k< myid; k++ )
            bucket_ptrs[i] += bucket_size[k][i];
        for( k=myid; k< num_threads; k++ )
            bucket_ptrs[i] += bucket_size[k][i-1];
    }


/*  Sort into appropriate bucket */
    // #pragma omp for schedule(static)
    // for( i=0; i<NUM_KEYS; i++ )  
    // {
    //     k = key_array[i];
    //     key_buff2[bucket_ptrs[k >> shift]++] = k;
    // }

    /* --- SYCL CODE --- */
    buffer<INT_TYPE> bucket_ptrs_buf(bucket_ptrs, range<1>(NUM_BUCKETS));
    buffer<INT_TYPE> key_buff2_buf(key_buff2, range<1>(SIZE_OF_BUFFERS));

    q.submit([&] (handler& h){
        auto key_array_acc = key_array_buf.get_access<access::mode::read>(h);
        auto bucket_ptrs_acc = bucket_ptrs_buf.get_access<access::mode::read_write>(h);
        auto key_buff2_acc = key_buff2_buf.get_access<access::mode::read_write>(h);

        h.parallel_for(range<1>(NUM_KEYS), [=](id<1> i) {
            INT_TYPE k = key_array_acc[i];
            auto atomic_bucket_ptr = atomic_ref<INT_TYPE, memory_order::relaxed, memory_scope::device, access::address_space::global_space>(bucket_ptrs_acc[k >> shift]);
            INT_TYPE index = atomic_bucket_ptr.fetch_add(1);
        
            auto atomic_key_buff2 = atomic_ref<INT_TYPE, memory_order::relaxed, memory_scope::device, access::address_space::global_space>(key_buff2_acc[index]);
            atomic_key_buff2.store(k);
        });

    }).wait();

    // bucket_ptrs back to the host - {} are needed to determine the scope and destroy the host_accessor after its use
    {
        host_accessor bucket_ptrs_a(bucket_ptrs_buf);
        q.memcpy(bucket_ptrs, &bucket_ptrs_a[0], NUM_BUCKETS * sizeof(INT_TYPE)).wait();
    }

    // key_buff2 back to the host
    {
        host_accessor key_buff2_a(key_buff2_buf);
        q.memcpy(key_buff2, &key_buff2_a[0], SIZE_OF_BUFFERS * sizeof(INT_TYPE)).wait();
    }
    /* END OF SYCL CODE */

/*  The bucket pointers now point to the final accumulated sizes */
    if (myid < num_threads-1) {
        for( i=0; i< NUM_BUCKETS; i++ )
            for( k=myid+1; k< num_threads; k++ )
                bucket_ptrs[i] += bucket_size[k][i];
    }


/*  Now, buckets are sorted.  We only need to sort keys inside
    each bucket, which can be done in parallel.  Because the distribution
    of the number of keys in the buckets is Gaussian, the use of
    a dynamic schedule should improve load balance, thus, performance     */

// #ifdef SCHED_CYCLIC
//     #pragma omp for schedule(static,1)
// #else
//     #pragma omp for schedule(dynamic)
// #endif
//     for( i=0; i< NUM_BUCKETS; i++ ) {

// /*  Clear the work array section associated with each bucket */
//         k1 = i * num_bucket_keys;
//         k2 = k1 + num_bucket_keys;
//         for ( k = k1; k < k2; k++ )
//             key_buff_ptr[k] = 0;

// /*  Ranking of all keys occurs in this section:                 */

// /*  In this section, the keys themselves are used as their 
//     own indexes to determine how many of each there are: their
//     individual population                                       */
//         m = (i > 0)? bucket_ptrs[i-1] : 0;
//         for ( k = m; k < bucket_ptrs[i]; k++ )
//             key_buff_ptr[key_buff_ptr2[k]]++;  /* Now they have individual key   */
//                                        /* population                     */

// /*  To obtain ranks of each key, successively add the individual key
//     population, not forgetting to add m, the total of lesser keys,
//     to the first key population                                          */
//         key_buff_ptr[k1] += m;
//         for ( k = k1+1; k < k2; k++ )
//             key_buff_ptr[k] += key_buff_ptr[k-1];

//     }

    /* --- SYCL CODE --- */
    
    buffer<INT_TYPE> key_buff_ptr_buf(key_buff_ptr, range<1>(MAX_KEY));
    buffer<INT_TYPE> key_buff_ptr2_buf(key_buff_ptr2, range<1>(SIZE_OF_BUFFERS));

    q.submit([&](handler& h) {
        // Accessors
        auto key_buff_ptr_acc = key_buff_ptr_buf.get_access<access::mode::read_write>(h);
        auto key_buff_ptr2_acc = key_buff_ptr2_buf.get_access<access::mode::read>(h);
        auto bucket_ptrs_acc = bucket_ptrs_buf.get_access<access::mode::read>(h);

        h.parallel_for(range<1>(NUM_BUCKETS), [=](id<1> i) {
            // Each work-item handles one bucket
            int k1 = i * num_bucket_keys;
            int k2 = k1 + num_bucket_keys;

            // Clear the work array section associated with each bucket
            for (int k = k1; k < k2; k++) {
                key_buff_ptr_acc[k] = 0;
            }

            // Ranking of all keys occurs in this section
            int m = (i > 0) ? bucket_ptrs_acc[i - 1] : 0;
            for (int k = m; k < bucket_ptrs_acc[i]; k++) {
                key_buff_ptr_acc[key_buff_ptr2_acc[k]]++;
            }

            // Obtain ranks of each key (sequential within each work-item)
            key_buff_ptr_acc[k1] += m;
            for (int k = k1 + 1; k < k2; k++) {
                key_buff_ptr_acc[k] += key_buff_ptr_acc[k - 1];
            }
        });
    }).wait();
    /* END OF SYCL CODE */

#else /*USE_BUCKETS*/


    work_buff = key_buff1_aptr[myid];


/*  Clear the work array */
    for( i=0; i<MAX_KEY; i++ )
        work_buff[i] = 0;


/*  Ranking of all keys occurs in this section:                 */

/*  In this section, the keys themselves are used as their 
    own indexes to determine how many of each there are: their
    individual population                                       */

    // #pragma omp for nowait schedule(static)
    // for( i=0; i<NUM_KEYS; i++ )
    //     work_buff[key_buff_ptr2[i]]++;  /* Now they have individual key   */
                                       /* population                     */
    
    /* --- SYCL CODE --- */
    
    buffer<INT_TYPE> work_buff_buf{work_buff, range<1>(MAX_KEY)};
    buffer<INT_TYPE> key_buff_ptr2_buf{key_buff_ptr2, range<1>(SIZE_OF_BUFFERS)};

    range<1> num_iterations(NUM_KEYS);
    q.submit([&] (handler& h){
        auto work_buff_acc = work_buff_buf.get_access<access_mode::read_write>(h); 
        auto key_buff_ptr2_acc = key_buff_ptr2_buf.get_access<access_mode::read>(h);

        h.parallel_for(num_iterations, [=] (item<1> i){
            auto atomic_work_buff = atomic_ref<INT_TYPE, memory_order::relaxed, memory_scope::device, access::address_space::global_space>(work_buff_acc[key_buff_ptr2_acc[i[0]]]);
            atomic_work_buff.fetch_add(1);
        });
    });
    q.wait();
    sycl::host_accessor h_a(work_buff_buf);
    work_buff = &h_a[0];
    q.memcpy(key_buff1_aptr[myid], work_buff, MAX_KEY * sizeof(INT_TYPE)).wait();
    
    /* END OF SYCL CODE */

/*  To obtain ranks of each key, successively add the individual key
    population                                          */   
     for( i=0; i<MAX_KEY-1; i++ )   
         work_buff[i+1] += work_buff[i];

    #pragma omp barrier

/*  Accumulate the global key population */
// It will never be executed, the number of threads is equal to 1
    for( k=1; k<num_threads; k++ ) {
        #pragma omp for nowait schedule(static)
        for( i=0; i<MAX_KEY; i++ )
            key_buff_ptr[i] += key_buff1_aptr[k][i];
    }

#endif /*USE_BUCKETS*/

  } /*omp parallel*/

/* This is the partial verify test section */
/* Observe that test_rank_array vals are   */
/* shifted differently for different cases */
    for( i=0; i<TEST_ARRAY_SIZE; i++ )
    {                                             
        k = partial_verify_vals[i];          /* test vals were put here */
        if( 0 < k  &&  k <= NUM_KEYS-1 )
        {
            INT_TYPE key_rank = key_buff_ptr[k-1];
            INT_TYPE test_rank = test_rank_array[i];
            int failed = 0;

            switch( CLASS )
            {
                case 'S':
                    if( i <= 2 )
                        test_rank += iteration;
                    else
                        test_rank -= iteration;
                    break;
                case 'W':
                    if( i < 2 )
                        test_rank += iteration - 2;
                    else
                        test_rank -= iteration;
                    break;
                case 'A':
                    if( i <= 2 )
                        test_rank += iteration - 1;
                    else
                        test_rank -= iteration - 1;
                    break;
                case 'B':
                    if( i == 1 || i == 2 || i == 4 )
                        test_rank += iteration;
                    else
                        test_rank -= iteration;
                    break;
                case 'C':
                    if( i <= 2 )
                        test_rank += iteration;
                    else
                        test_rank -= iteration;
                    break;
                case 'D':
                    if( i < 2 )
                        test_rank += iteration;
                    else
                        test_rank -= iteration;
                    break;
                case 'E':
                    if( i < 2 )
                        test_rank += iteration - 2;
                    else if( i == 2 )
                    {
                        test_rank += iteration - 2;
                        if (iteration > 4)
                            test_rank -= 2;
                        else if (iteration > 2)
                            test_rank -= 1;
                    }
                    else
                        test_rank -= iteration - 2;
                    break;
            }
            if( key_rank != test_rank )
                failed = 1;
            else
                passed_verification++;
            if( failed == 1 )
                printf( "Failed partial verification: "
                        "iteration %d, test key %d\n", 
                         iteration, (int)i );
        }
    }




/*  Make copies of rank info for use by full_verify: these variables
    in rank are local; making them global slows down the code, probably
    since they cannot be made register by compiler                        */

    if( iteration == MAX_ITERATIONS ) 
        key_buff_ptr_global = key_buff_ptr;

}      


/*****************************************************************/
/*************             M  A  I  N             ****************/
/*****************************************************************/

int main( int argc, char **argv )
{

    queue q(gpu_selector_v);

    int             i, iteration, timer_on;

    double          timecounter;


/*  Initialize timers  */
    timer_on = check_timer_flag();

    timer_clear( 0 );
    if (timer_on) {
        timer_clear( 1 );
        timer_clear( 2 );
        timer_clear( 3 );
    }

    if (timer_on) timer_start( 3 );


/*  Initialize the verification arrays if a valid class */
    for( i=0; i<TEST_ARRAY_SIZE; i++ )
        switch( CLASS )
        {
            case 'S':
                test_index_array[i] = S_test_index_array[i];
                test_rank_array[i]  = S_test_rank_array[i];
                break;
            case 'A':
                test_index_array[i] = A_test_index_array[i];
                test_rank_array[i]  = A_test_rank_array[i];
                break;
            case 'W':
                test_index_array[i] = W_test_index_array[i];
                test_rank_array[i]  = W_test_rank_array[i];
                break;
            case 'B':
                test_index_array[i] = B_test_index_array[i];
                test_rank_array[i]  = B_test_rank_array[i];
                break;
            case 'C':
                test_index_array[i] = C_test_index_array[i];
                test_rank_array[i]  = C_test_rank_array[i];
                break;
            case 'D':
                test_index_array[i] = D_test_index_array[i];
                test_rank_array[i]  = D_test_rank_array[i];
                break;
            case 'E':
                test_index_array[i] = E_test_index_array[i];
                test_rank_array[i]  = E_test_rank_array[i];
                break;
        };

        

/*  Printout initial NPB info */
    printf
      ( "\n\n NAS Parallel Benchmarks (NPB3.4-OMP) - IS Benchmark\n\n" );
    printf( " Size:  %ld  (class %c)\n", (long)TOTAL_KEYS, CLASS );
    printf( " Iterations:  %d\n", MAX_ITERATIONS );
#ifdef _OPENMP
    printf( " Number of available threads:  %d\n", omp_get_max_threads() );
#endif
    printf( "\n" );

    if (timer_on) timer_start( 1 );

/*  Generate random number sequence and subsequent keys on all procs */
    create_seq( 314159265.00,                    /* Random number gen seed */
                1220703125.00 );                 /* Random number gen mult */

    alloc_key_buff(q);
    if (timer_on) timer_stop( 1 );


/*  Do one interation for free (i.e., untimed) to guarantee initialization of  
    all data and code pages and respective tables */
    rank( 1, q);  

/*  Start verification counter */
    passed_verification = 0;

    if( CLASS != 'S' ) printf( "\n   iteration\n" );

/*  Start timer  */             
    timer_start( 0 );


/*  This is the main iteration */
    for( iteration=1; iteration<=MAX_ITERATIONS; iteration++ )
    {
        if( CLASS != 'S' ) printf( "        %d\n", iteration );
        rank( iteration, q );
    }


/*  End of timing, obtain maximum time of all processors */
    timer_stop( 0 );
    timecounter = timer_read( 0 );


/*  This tests that keys are in sequence: sorting of last ranked key seq
    occurs here, but is an untimed operation                             */
    if (timer_on) timer_start( 2 );
    full_verify(q);
    if (timer_on) timer_stop( 2 );

    if (timer_on) timer_stop( 3 );


/*  The final printout  */
    if( passed_verification != 5*MAX_ITERATIONS + 1 )
        passed_verification = 0;
    c_print_results( "IS",
                     CLASS,
                     TOTAL_KS1,
                     TOTAL_KS2,
                     0,
                     MAX_ITERATIONS,
                     timecounter,
                     1.0e-6*(double)(TOTAL_KEYS)*MAX_ITERATIONS
                                                  /timecounter,
                     "keys ranked", 
                     passed_verification,
                     NPBVERSION,
                     COMPILETIME,
                     CC,
                     CLINK,
                     C_LIB,
                     C_INC,
                     CFLAGS,
                     CLINKFLAGS );


/*  Print additional timers  */
    if (timer_on) {
       double t_total, t_percent;

       t_total = timer_read( 3 );
       printf("\nAdditional timers -\n");
       printf(" Total execution: %8.3f\n", t_total);
       if (t_total == 0.0) t_total = 1.0;
       timecounter = timer_read(1);
       t_percent = timecounter/t_total * 100.;
       printf(" Initialization : %8.3f (%5.2f%%)\n", timecounter, t_percent);
       timecounter = timer_read(0);
       t_percent = timecounter/t_total * 100.;
       printf(" Benchmarking   : %8.3f (%5.2f%%)\n", timecounter, t_percent);
       timecounter = timer_read(2);
       t_percent = timecounter/t_total * 100.;
       printf(" Sorting        : %8.3f (%5.2f%%)\n", timecounter, t_percent);
    }

    return 0;
         /**************************/
}        /*  E N D  P R O G R A M  */
         /**************************/




