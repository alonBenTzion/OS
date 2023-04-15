//
// Created by Roy Abend on 15/04/2023.
//

#include "uthreads.h"

/*
 * sigsetjmp/siglongjmp demo program.
 * Hebrew University OS course.
 * Author: OS, os@cs.huji.ac.il
 */

#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdbool.h>
#include "iostream"
#include <deque>
#include "set"

#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;


#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr) {
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
            : "=g" (ret)
            : "0" (addr));
    return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5


/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
                 "rol    $0x9,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}


#endif

char **stacks;
//char stack1[STACK_SIZE];
sigjmp_buf env[MAX_THREAD_NUM];
int threads_quantums[MAX_THREAD_NUM];
//Thread threads[MAX_THREAD_NUM];
std::deque<int> ready_queue;
int QUANTUM_USECS;
int running_thread_tid;
std::set<int> blocked_threads;
int total_quantum;
struct itimerval timer;
struct sigaction sa = {0};
void yield();
void jump_to_thread(int tid) {
    running_thread_tid = tid;
    siglongjmp(env[tid], 1);
}

void reset_timer(){
    timer.it_value.tv_usec = QUANTUM_USECS;
}

void time_up_handler(int sig){
    if(sig==SIGVTALRM){
        yield();
    }
}

/**
 * @brief Saves the current thread state, and jumps to the other thread.
 */
void yield() {
    int tid = ready_queue.front();
    ready_queue.pop_front();
    sigsetjmp(env[running_thread_tid], 1);
    jump_to_thread(tid);
    threads_quantums[tid] += 1;
    total_quantum += 1;
    reset_timer();
    setitimer(ITIMER_VIRTUAL, &timer, NULL);
    if (sigaction(SIGVTALRM, &sa, NULL) < 0)
    {
        printf("sigaction error.");
    }

}

void setup_thread(int tid, char *stack, thread_entry_point entry_point) {
    // initializes env[tid] to use the right stack, and to run from the function 'entry_point', when we'll use
    // siglongjmp to jump into the thread.
    address_t sp = (address_t) stack + STACK_SIZE - sizeof(address_t);
    address_t pc = (address_t) entry_point;
    sigsetjmp(env[tid], 1);
    (env[tid]->__jmpbuf)[JB_SP] = translate_address(sp);
    (env[tid]->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&env[tid]->__saved_mask);
}


/**
 * @brief initializes the thread library.
 *
 * Once this function returns, the main thread (tid == 0) will be set as RUNNING. There is no need to
 * provide an entry_point or to create a stack for the main thread - it will be using the "regular" stack and PC.
 * You may assume that this function is called before any other thread library function, and that it is called
 * exactly once.
 * The input to the function is the length of a quantum in micro-seconds.
 * It is an error to call this function with non-positive quantum_usecs.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs) {
    if (quantum_usecs < 1) {
        std::cerr << "error message" << std::endl;
        return -1;
    }
    stacks = new char *[MAX_THREAD_NUM];
    if (stacks == nullptr) {
        std::cerr << "error message" << std::endl;
        return -1;
    }
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        stacks[i] = nullptr;
        threads_quantums[i] = 0;
    }
    setup_thread(0, stacks[0],)
    QUANTUM_USECS = quantum_usecs;
    running_thread_tid = 0;
    total_quantum = 1;
    sa.sa_handler = &time_up_handler;

}


/**
 * @brief Creates a new thread, whose entry point is the function entry_point with the signature
 * void entry_point(void).
 *
 * The thread is added to the end of the READY threads list.
 * The uthread_spawn function should fail if it would cause the number of concurrent threads to exceed the
 * limit (MAX_THREAD_NUM).
 * Each thread should be allocated with a stack of size STACK_SIZE bytes.
 * It is an error to call this function with a null entry_point.
 *
 * @return On success, return the ID of the created thread. On failure, return -1.
*/
int uthread_spawn(thread_entry_point entry_point) {
    if (entry_point == nullptr) {
        //#TODO error message
        return -1;
    }
    // check the limit
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        if (stacks[i] == nullptr) {
            stacks[i] = new char[STACK_SIZE];
            if (stacks[i] == nullptr) {
                //#TODO allocation error message
                return -1;
            }
            setup_thread(i, stacks[i], entry_point);
            ready_queue.push_back(i);
            return i;
        }
    }
    // #todo message limit num of threads
    return -1;
}

bool is_valid_thread(int tid) {
    if (tid < 0 || tid > MAX_THREAD_NUM) {
        // error message
        return false;
    }
    if (stacks[tid] == nullptr) {
        // error message
        return false;
    }
    return true;
}

void remove_from_ready_queue(int tid) {
    ready_queue.erase(std::remove(ready_queue.begin(), ready_queue.end(), tid), ready_queue.end());
}

/**
 * @brief Terminates the thread with ID tid and deletes it from all relevant control structures.
 *
 * All the resources allocated by the library for this thread should be released. If no thread with ID tid exists it
 * is considered an error. Terminating the main thread (tid == 0) will result in the termination of the entire
 * process using exit(0) (after releasing the assigned library memory).
 *
 * @return The function returns 0 if the thread was successfully terminated and -1 otherwise. If a thread terminates
 * itself or the main thread is terminated, the function does not return.
*/
int uthread_terminate(int tid) {
    if (!is_valid_thread(tid)) {
        return -1;
    };
    // if running
    if (running_thread_tid == tid) {
        yield();
    } else {
        // if in ready - remove from queue
        remove_from_ready_queue(tid);
        // if blocked
        blocked_threads.erase(tid);
    }


    delete[] stacks[tid];
    stacks[tid] = nullptr;
    threads_quantums[tid] = 0;
    // #TODO sigprocmask???


}


/**
 * @brief Blocks the thread with ID tid. The thread may be resumed later using uthread_resume.
 *
 * If no thread with ID tid exists it is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision should be made. Blocking a thread in
 * BLOCKED state has no effect and is not considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_block(int tid) {
    if (!is_valid_thread(tid)) {
        return -1;
    }
    if (tid == 0) {
        // error main thread
        return -1;
    }
    if (running_thread_tid == tid) {
        // a scheduling decision should be made
    }
    // if not in ready state nothing will happen
    remove_from_ready_queue(tid);
    // if already in set nothing will happen
    blocked_threads.insert(tid);
    return 0;


}


/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid) {
    if (!is_valid_thread(tid)) {
        return -1;
    }
    if (blocked_threads.erase(tid) == 1) {
        ready_queue.push_back(tid); //#todo check if succeeded??
    }
    return 0;
}


/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * After the sleeping time is over, the thread should go back to the end of the READY queue.
 * If the thread which was just RUNNING should also be added to the READY queue, or if multiple threads wake up
 * at the same time, the order in which they're added to the end of the READY queue doesn't matter.
 * The number of quantums refers to the number of times a new quantum starts, regardless of the reason. Specifically,
 * the quantum of the thread which has made the call to uthread_sleep isn’t counted.
 * It is considered an error if the main thread (tid == 0) calls this function.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_sleep(int num_quantums);


/**
 * @brief Returns the thread ID of the calling thread.
 *
 * @return The ID of the calling thread.
*/
int uthread_get_tid() {
    return running_thread_tid;
}


/**
 * @brief Returns the total number of quantums since the library was initialized, including the current quantum.
 *
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number should be increased by 1.
 *
 * @return The total number of quantums.
*/
int uthread_get_total_quantums() {
    return total_quantum;
}


/**
 * @brief Returns the number of quantums the thread with ID tid was in RUNNING state.
 *
 * On the first time a thread runs, the function should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state when this function is called, include
 * also the current quantum). If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return the number of quantums of the thread with ID tid. On failure, return -1.
*/
int uthread_get_quantums(int tid) {
    if (!is_valid_thread(tid)) {
        return -1;
    }
    return threads_quantums[tid];
}
