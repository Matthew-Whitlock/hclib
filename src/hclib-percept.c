#include "hclib-percept.h"
#include "hclib-rt.h"

void perceptron_init(hclib_internal_perceptron_t *percept){
    int i;
    for(i = 0; i < (1<<PERCEPT_TASK_BITS); i++){
        percept->array[i] = 0;
        percept->average_cache_misses[i] = 0;
    }
    for(; i < (1<<PERCEPT_INDEX_BITS); i++){
        percept->array[i] = 0;
    }
    percept->owned = 0;
}

short perceptron_check(hclib_internal_perceptron_t *percept, hclib_task_t *task){
    short to_return = 0;
    for(int f_num = 0; f_num < task->waiting_on_index; f_num++){
        int idx = task->percept_info[f_num] ^ 
            (CURRENT_WS_INTERNAL->id & PERCEPT_RANK_BITMASK);
        to_return += percept->array[idx]; 
    }
    return to_return;
}

void perceptron_update(hclib_internal_perceptron_t *percept, hclib_task_t *task, long long cache_misses){
    /* We need to take ownership of the array before doing a non-atomic operation.
     * consider owned = 1 to be claimed and 0 to be unclaimed, we atomically try
     * to take ownership if unowned until successful.
     */
    while(!__sync_bool_compare_and_swap(&percept->owned, 0, 1));
    
    int prev_val = percept->average_cache_misses[(int)task->percept_info & PERCEPT_TASK_BITMASK];
    if(prev_val == 0){
        //just insert, don't do any other updates since this is (likely) the first task run
        percept->average_cache_misses[(int)task->percept_info & PERCEPT_TASK_BITMASK] = 
            cache_misses;
    } else {
        percept->average_cache_misses[(int)task->percept_info & PERCEPT_TASK_BITMASK] = 
            0.5*(prev_val+cache_misses);

        //Now we need to update the perceptron itself based on the results.
        for(int f_num = 0; f_num < task->waiting_on_index-1; f_num++){
            int idx = task->percept_info[f_num] ^ 
                (CURRENT_WS_INTERNAL->id & PERCEPT_RANK_BITMASK);
            
            //TODO: a better method for updating this, which accounts for only small differences in cache misses and slows raising once the value gets high/low
            short current_val = percept->array[idx];
            if(cache_misses > prev_val){
                //This is bad, lower the preference for this circumstance
                current_val--;
            } else if(cache_misses < prev_val){
                current_val++;
            }
            if(current_val < -16) current_val = -16;
            if(current_val > 15) current_val = 15;

            percept->array[idx] = current_val;
        }
    }
    
    //release ownership
    percept->owned = 0;
}
