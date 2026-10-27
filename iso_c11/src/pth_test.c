#include <stdio.h>
#include <pthread.h>
#include <errno.h>

static const int NTHREADS = 7;

void *thread_function(void *);

int
main()
{
   pthread_t thread_id[NTHREADS];

   for(int i = 0; i < NTHREADS; i++)
      pthread_create( &thread_id[i], NULL, thread_function, NULL );

   for(int i = 0; i < NTHREADS; i++)
      pthread_join( thread_id[i], NULL);

   return 0;
}

void *thread_function(void *dummyPtr)
{
    (void) dummyPtr;
    printf("Thread number %ld addr(errno):%p, errno = %d\n", pthread_self(), &errno, errno);
    return dummyPtr;
}

