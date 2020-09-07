#ifndef HCLIB_PERCEPT_H_
#define HCLIB_PERCEPT_H_

#include "hclib-task.h"

//# bits used for task percept_type
#define PERCEPT_TASK_BITS 5
#define PERCEPT_TASK_BITMASK ((1<<PERCEPT_TASK_BITS)-1)
//# bits used for future percept_type
#define PERCEPT_FUTURE_BITS 5
#define PERCEPT_FUTURE_BITMASK ((1<<PERCEPT_FUTURE_BITS)-1)
//# bits used for the worker ID of who finishes a future
#define PERCEPT_RANK_BITS 5
#define PERCEPT_RANK_BITMASK ((1<<PERCEPT_RANK_BITS)-1)

#define PERCEPT_INDEX_BITS (PERCEPT_TASK_BITS + PERCEPT_FUTURE_BITS + PERCEPT_RANK_BITS)

#define PERCEPT_MIN_SIGNIFICANT 10

typedef struct hclib_internal_perceptron_t {
    volatile short array[1<<PERCEPT_INDEX_BITS];

    //owned is used to obtain overship over the average_task_misses array
    //which should be done before any updates.
    volatile short owned;
    //The average number of task misses for a given task percept_type
    volatile int average_cache_misses[1<<PERCEPT_TASK_BITS];
} hclib_internal_perceptron_t;

void perceptron_init(hclib_internal_perceptron_t *percept);
short perceptron_check(hclib_internal_perceptron_t *percept, hclib_task_t *task);
void perceptron_update(hclib_internal_perceptron_t *percept, hclib_task_t *task, long long cache_misses);

#endif //HCLIB_PERCEPT_H_
