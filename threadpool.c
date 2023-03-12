
#include <pthread.h>
#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include "threadpool.h"
threadpool* create_threadpool(int num_threads_in_pool);
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg);
void* do_work(void* p);
void destroy_threadpool(threadpool* destroyme);
threadpool* create_threadpool(int num_threads_in_pool){
    threadpool *thread= (threadpool*) malloc(num_threads_in_pool* sizeof(threadpool));
    thread->num_threads=num_threads_in_pool;
    thread->qsize=0;
    thread->threads=(pthread_t *) malloc( num_threads_in_pool*sizeof(pthread_t));
    thread->qhead=NULL;
    thread->qtail=NULL;
    thread->shutdown=0;
    thread->dont_accept=0;
    if (pthread_cond_init(&(thread->q_empty), NULL))
    {
        perror("error q_empty\n");
        pthread_mutex_destroy(&(thread->qlock));
        free(thread->threads);
        free(thread);
        return NULL;
    }
    if (pthread_cond_init(&(thread->q_not_empty), NULL)) {
        perror("error q_not_empty\n");
        pthread_mutex_destroy(&(thread->qlock));
        pthread_cond_destroy(&(thread->q_empty));
        free(thread->threads);
        free(thread);
        return NULL;
    }
    int j;
    for ( j = 0; j <num_threads_in_pool ; j++) {
        if(pthread_create(&(thread->threads[j]), NULL, do_work, thread) != 0)
        {
            perror("Error during threadpool creation!\n");
            return NULL;
        }
    }



    return thread;
    }
/**
* dispatch enter a "job" of type work_t into the queue.
* when an available thread takes a job from the queue, it will
* call the function "dispatch_to_here" with argument "arg".
* this function should:
* 1. create and init work_t element
* 2. lock the mutex
* 3. add the work_t element to the queue
* 4. unlock mutex
*/

void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg){
    if(from_me==NULL){
        perror("thread pool is empty\n");
        return;
    }
    work_t *work=(work_t*) malloc(sizeof(work_t));
    if(work==NULL){
        perror("error allocation \n");
    }
    work->arg=arg;
    work->routine=dispatch_to_here;
    work->next=NULL;
    pthread_mutex_lock(&from_me->qlock);
    if(from_me->qhead==NULL){
        from_me->qhead=work;
        from_me->qtail=work;
        from_me->qsize++;
        pthread_cond_signal(&(from_me->q_not_empty));
    }
    else if(from_me->qhead==from_me->qtail){
        from_me->qtail=work;
        from_me->qhead->next=work;
        from_me->qsize++;
        pthread_cond_signal(&(from_me->q_not_empty));
    }
    else{
        work_t *temp=from_me->qhead;
        while (temp->next!=NULL){
            temp=temp->next;
        }
        temp->next=work;
        from_me->qtail=work;
        from_me->qsize++;
        pthread_cond_signal(&(from_me->q_not_empty));
    }
    pthread_mutex_unlock(&from_me->qlock);
}
/*
 * The work function of the thread
 * this function should:
 * 1. lock mutex
 * 2. if the queue is empty, wait
 * 3. take the first element from the queue (work_t)
 * 4. unlock mutex
 * 5. call the thread routine
 *
*/

void* do_work(void* p) {

    threadpool* thread = (threadpool*)p;
    while (1) {
        work_t* work = NULL;
        pthread_mutex_lock(&thread->qlock);
        // Wait for work to become available or for the thread pool to be shut down
        while (!thread->shutdown && thread->qsize == 0) {
            pthread_cond_wait(&thread->q_not_empty, &thread->qlock);
        }
        // Check if the thread pool has been shut down
        if (thread->shutdown) {
            pthread_mutex_unlock(&thread->qlock);
            return NULL;
        }
        // Dequeue the next work item
        work = thread->qhead;
        thread->qhead = work->next;
        thread->qsize--;
        // Signal that the queue is empty if necessary
        if (thread->qsize == 0 && thread->dont_accept) {
            pthread_cond_signal(&thread->q_empty);
        }
        pthread_mutex_unlock(&thread->qlock);
        // Execute the work item
        work->routine(work->arg);
        // Free the memory for the work item
        free(work);
    }


}

/**
 * destroy_threadpool kills the threadpool, causing
 * all threads in it to commit suicide, and then
 * frees all the memory associated with the threadpool.
 */
void destroy_threadpool(threadpool* destroyme) {
    // Lock the mutex to ensure exclusive access to the task queue
    pthread_mutex_lock(&destroyme->qlock);

    // Set the flag to indicate that the threadpool should not accept new tasks
    destroyme->dont_accept = 1;

    // Wait for the task queue to be empty
    while (destroyme->qsize > 0) {
        pthread_cond_wait(&destroyme->q_empty, &destroyme->qlock);
    }
    // Set the shutdown flag to indicate that the threadpool should shut down
    destroyme->shutdown = 1;
    // Unlock the mutex
    pthread_mutex_unlock(&destroyme->qlock);
    // Wake up all threads
    pthread_cond_broadcast(&destroyme->q_not_empty);
    // Join all threads
    for (int i = 0; i < destroyme->num_threads; i++) {
        pthread_join(destroyme->threads[i], NULL);
    }
    // Free the memory associated with the threads array and the threadpool object
    free(destroyme->threads);
    free(destroyme);
}

