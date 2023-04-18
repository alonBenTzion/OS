//
// Created by Roy Abend on 15/04/2023.
//

#include "uthreads.h"
#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdbool.h>
#include <iostream>
#include <deque>
#include "set"

#define LIB_ERROR "thread library error: "
#define SYS_ERROR "system error: "
#define INVALID_INPUT "invalid input"
#define INVALID_CALL "invalid sleep call from main thread"
#define INVALID_BLOCK "invalid block call"
#define INVALID_RESUME "invalid resume call"
#define INVALID_SPAWN "invalid spawn"
#define INVALID_TERM "invalid termination"
#define INVALID_TID "invalid thread id"
#define FAILED_ALLOC "failed allocation"

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
sigjmp_buf env[MAX_THREAD_NUM];
int threads_quantums[MAX_THREAD_NUM];
int sleep_counters[MAX_THREAD_NUM];
std::deque<int> ready_queue;
int running_thread_tid;
std::set<int> blocked_threads;
struct itimerval timer;
struct sigaction sa = {0};
sigset_t sig_set;

void yield(bool insert_to_ready);

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

void jump_to_thread(int tid) {
    running_thread_tid = tid;
    siglongjmp(env[tid], 1);
}

void time_up_handler(int sig) {
    yield(true);
}

void free_before_exit() {
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        if (stacks[i] != nullptr) {
            delete[] stacks[i];
            stacks[i] = nullptr;
        }
    }
    delete[] stacks;

}

void update_sleeping_counters() {
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        if (sleep_counters[i] > 0) {
            // if finished sleeping and not blocked
            if ((sleep_counters[i] == 1) && (blocked_threads.find(i) == blocked_threads.end())) {
                ready_queue.push_back(i);
            }
            sleep_counters[i] -= 1;
        }
    }
}

/**
 * @brief Saves the current thread state, and jumps to the other thread.
 */
void yield(bool insert_to_ready) {
    if (insert_to_ready) {
        ready_queue.push_back(running_thread_tid);
    }
    int tid = ready_queue.front();
    ready_queue.pop_front();
    sigsetjmp(env[running_thread_tid], 1);
    threads_quantums[tid] += 1;
    update_sleeping_counters();
    jump_to_thread(tid);
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
    ready_queue.push_back(tid);

}

void init_timer(int quantum_usecs) {
    sa.sa_handler = &time_up_handler;
    if (sigaction(SIGVTALRM, &sa, nullptr) < 0) {
        printf("sigaction error.");
    }

    // Configure the timer to expire after quantum_usecs ... */
    timer.it_value.tv_sec = quantum_usecs / 1000000;        // first time interval, seconds part
    timer.it_value.tv_usec = quantum_usecs % 1000000;        // first time interval, microseconds part

    // configure the timer to expire every quantum_usecs after that.
    timer.it_interval.tv_sec = quantum_usecs / 1000000;    // following time intervals, seconds part
    timer.it_interval.tv_usec = quantum_usecs % 1000000;    // following time intervals, microseconds part

    if (setitimer(ITIMER_VIRTUAL, &timer, nullptr)) {
        printf("setitimer error.");
    }
}

void init_ds() {
    stacks = new char *[MAX_THREAD_NUM];
    if (stacks == nullptr) {
        std::cerr << SYS_ERROR << FAILED_ALLOC << std::endl;
        free_before_exit();
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        stacks[i] = nullptr;
        threads_quantums[i] = 0;
        sleep_counters[i] = 0;
    }
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
        std::cerr << LIB_ERROR << INVALID_INPUT << std::endl;
        return -1;
    }
    init_ds();

    sigsetjmp(env[0], 1);
    sigemptyset(&env[0]->__saved_mask);

    sigemptyset(&sig_set);
    sigaddset(&sig_set, SIGVTALRM);

    init_timer(quantum_usecs);


    running_thread_tid = 0;
    threads_quantums[0] += 1;
    return 0;
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
    sigprocmask(SIG_BLOCK, &sig_set, nullptr);
    if (entry_point == nullptr) {
        std::cerr << "Error message: Invalid entry_point function" << std::endl;
        sigprocmask(SIG_UNBLOCK, &sig_set, nullptr);
        return -1;
    }
    // check the limit, allocate stack and setup the new thread
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        if (stacks[i] == nullptr) {
            stacks[i] = new char[STACK_SIZE];
            if (stacks[i] == nullptr) {
                std::cerr << SYS_ERROR << FAILED_ALLOC << std::endl;
                free_before_exit();
                exit(EXIT_FAILURE);
            }
            setup_thread(i, stacks[i], entry_point);
            sigprocmask(SIG_UNBLOCK, &sig_set, nullptr);
            return i;
        }
    }
    sigprocmask(SIG_UNBLOCK, &sig_set, nullptr);
    std::cerr << "Error message: Maximum number of threads are exists" << std::endl;
    return -1;
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
    sigprocmask(SIG_BLOCK, &sig_set, nullptr);
    if (!is_valid_thread(tid)) {
        sigprocmask(SIG_UNBLOCK, &sig_set, nullptr);
        return -1;
    }
    if (tid == 0) {
        free_before_exit();
        // #TODO err
        exit(EXIT_SUCCESS);
    }
    // if running
    if (running_thread_tid == tid) {
        yield(false);
    } else {
        // if in ready - remove from queue
        remove_from_ready_queue(tid);
        // if blocked
        blocked_threads.erase(tid);
    }

    delete[] stacks[tid];
    stacks[tid] = nullptr;
    threads_quantums[tid] = 0;
    sigprocmask(SIG_UNBLOCK, &sig_set, nullptr);
    return 0;
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
    sigprocmask(SIG_BLOCK, &sig_set, nullptr);
    if (!is_valid_thread(tid)) {
        sigprocmask(SIG_UNBLOCK, &sig_set, nullptr);
        return -1;
    }
    if (tid == 0) {
        // TODO error main thread
        sigprocmask(SIG_UNBLOCK, &sig_set, nullptr);
        return -1;
    }
    if (running_thread_tid == tid) {
        yield(false);
    }
    // if not in ready state nothing will happen
    remove_from_ready_queue(tid);
    // if already in set nothing will happen
    blocked_threads.insert(tid);
    sigprocmask(SIG_UNBLOCK, &sig_set, nullptr);
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
    sigprocmask(SIG_BLOCK, &sig_set, nullptr);
    if (!is_valid_thread(tid)) {
        sigprocmask(SIG_UNBLOCK, &sig_set, nullptr);
        return -1;
    }
    //if blocked and not sleep
    if (blocked_threads.erase(tid) == 1 && sleep_counters[tid] == 0) {
        ready_queue.push_back(tid); //#todo check if succeeded??
    }
    sigprocmask(SIG_UNBLOCK, &sig_set, nullptr);
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
int uthread_sleep(int num_quantums) {
    sigprocmask(SIG_BLOCK, &sig_set, nullptr);
    if (running_thread_tid == 0) {
        // error
        return -1;
    }
    sleep_counters[running_thread_tid] = num_quantums;
    yield(false);
    sigprocmask(SIG_UNBLOCK, &sig_set, nullptr);
    return 0;

}


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
    sigprocmask(SIG_BLOCK, &sig_set, nullptr);
    int total_quantum = 0;
    for (auto i: threads_quantums) {
        total_quantum += i;
    }
    sigprocmask(SIG_UNBLOCK, &sig_set, nullptr);
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
        std::cerr << "Error message: Invalid tid" << std::endl;
        return -1;
    }
    return threads_quantums[tid];
}
