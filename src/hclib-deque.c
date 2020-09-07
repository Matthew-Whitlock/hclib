/* Copyright (c) 2015, Rice University

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

1.  Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
2.  Redistributions in binary form must reproduce the above
     copyright notice, this list of conditions and the following
     disclaimer in the documentation and/or other materials provided
     with the distribution.
3.  Neither the name of Rice University
     nor the names of its contributors may be used to endorse or
     promote products derived from this software without specific
     prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */

/*
 * hclib-deque.cpp
 *
 *      Author: Vivek Kumar (vivekk@rice.edu)
 *      Acknowledgments: https://wiki.rice.edu/confluence/display/HABANERO/People
 */

#include "hclib-internal.h"
#include "hclib-atomics.h"

//Only needed for inefficient perceptron testing
extern hclib_context *hc_context;

void deque_init(hclib_internal_deque_t *deq, void *init_value) {
    deq->head = 0;
    deq->tail = 0;
    deq->owned = 0;
}

/*
 * push an entry onto the tail of the deque
 */
int deque_push(hclib_internal_deque_t *deq, void *entry) {
    //try to get ownership until successful before doing this stuff
    //Old value is returned - if old value was 0, we are done.
    while(hc_cas(&deq->owned, 0, 1));
    
    int size = deq->tail - deq->head;
    if (size == INIT_DEQUE_CAPACITY) { /* deque looks full */
        /* may not grow the deque if some interleaving steal occur */
        //be sure to relinquish ownership before returning!
        deq->owned = 0;
        return 0;
    }
    const int n = (deq->tail) % INIT_DEQUE_CAPACITY;
    deq->data[n] = (hclib_task_t *) entry;

    // Required to guarantee ordering of setting data[n] with incrementing tail.
    hc_mfence();

    deq->tail++;
    
    //release ownership
    deq->owned = 0;

    return 1;
}

void deque_destroy(hclib_internal_deque_t *deq) {
    free(deq);
}

/*
 * The steal protocol. Returns the number of tasks stolen, up to
 * STEAL_CHUNK_SIZE. stolen must have enough space to store up to
 * STEAL_CHUNK_SIZE task pointers.
 */
int deque_steal(hclib_internal_deque_t *deq, void **stolen) {
    /* Cannot read deq->data[head] here
     * Can happen that head=tail=0, then the owner of the deq pushes
     * a new task when stealer is here in the code, resulting in head=0, tail=1
     * All other checks down-below will be valid, but the old value of the buffer head
     * would be returned by the steal rather than the new pushed value.
     */

    int nstolen = 0;
    
    //Try to get ownership until successful before doing stuff
    //Old value is returned - if old value was 0, we are done.
    while(hc_cas(&deq->owned, 0, 1));
    
    int success;
    do {
        const int head = deq->head;
        hc_mfence();
        const int tail = deq->tail;

        if ((tail - head) <= 0) {
            success = 0;
        } else {
            hclib_task_t *t = (hclib_task_t *) deq->data[head % INIT_DEQUE_CAPACITY];
            /* compete with other thieves and possibly the owner (if the size == 1) */
            const int old = hc_cas(&deq->head, head, head + 1);
            if (old == head) {
                success = 1;
                stolen[nstolen++] = t;
            }

        }
    } while (success && nstolen < STEAL_CHUNK_SIZE);

    //release ownership
    deq->owned = 0;

    return nstolen;
}

/*
 * pop the task out of the deque from the tail
 */
hclib_task_t *deque_pop(hclib_internal_deque_t *deq) {
    hclib_task_t *t = NULL;

    //try to get ownership until successful before doing this stuff
    //Old value is returned - if old value was 0, we are done.
    while(hc_cas(&deq->owned, 0, 1));
    
    hc_mfence();
    int tail = deq->tail;
    tail--;
    deq->tail = tail;
    hc_mfence();
    int head = deq->head;
    
    int size = tail - head;
    if (size < 0) {
        deq->tail = deq->head;
        deq->owned = 0;
        return NULL;
    }

    hclib_internal_perceptron_t *perceptron = hc_context->perceptron;
    int best_task = tail;
    int best_score = perceptron_check(perceptron, (hclib_task_t*) deq->data[tail % INIT_DEQUE_CAPACITY]);
    for(int task = tail-1; task >= head; task--){
        int task_score = perceptron_check(perceptron, (hclib_task_t*) deq->data[task%INIT_DEQUE_CAPACITY]);
        if(task_score > best_score){
            best_task = task;
            best_score = task_score;
        }
    }
    
    t = (hclib_task_t *) deq->data[best_task % INIT_DEQUE_CAPACITY];

    
    if(best_task != tail){
        //We've taken from the middle, which is a terrible idea for a deque
        //(it'll be fine when implemented in HW)
        //we need to shift the values over to be back within head to tail
        //This is already terribly inefficient, so I'm not so worried about 
        //  optimizing
        for(int task = best_task; task < tail; task++){
            deq->data[task%INIT_DEQUE_CAPACITY] = 
                deq->data[(task+1)%INIT_DEQUE_CAPACITY]; 
        }
    }
    
    //release ownership now that I'm done
    deq->owned = 0;
    
    return t;
}

unsigned deque_size(hclib_internal_deque_t *deq) {
    const int size = deq->tail - deq->head;
    if (size <= 0) return 0;
    else return (unsigned)size;
}
