#include <sys/time.h>
#include <iostream>
#include "osm.h"

/* Time measurement function for a simple arithmetic operation.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_operation_time(unsigned int iterations){
    if(iterations==0){return -1;}
    struct timeval start_time, end_time;
    gettimeofday(&start_time, nullptr);
    int i;
    for (i = 0; i < iterations; i+=10) {
        2 + 2;
        2 + 2;
        2 + 2;
        2 + 2;
        2 + 2;
        2 + 2;
        2 + 2;
        2 + 2;
        2 + 2;
        2 + 2;
    }
    gettimeofday(&end_time, nullptr);
    double seconds = end_time.tv_sec - start_time.tv_sec;
    double microseconds = end_time.tv_usec - start_time.tv_usec;
    if(i==iterations){return (seconds * 1e9 + microseconds * 1e3) / iterations;}
    return -1;

}

void empty() { return; }

/* Time measurement function for an empty function call.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_function_time(unsigned int iterations){
    if(iterations==0){return -1;}

    struct timeval start_time, end_time;
    gettimeofday(&start_time, nullptr);
    int i;
    for (i = 0; i < iterations; i+=10) {
        empty();
        empty();
        empty();
        empty();
        empty();
        empty();
        empty();
        empty();
        empty();
        empty();
    }
    gettimeofday(&end_time, nullptr);
    double seconds = end_time.tv_sec - start_time.tv_sec;
    double microseconds = end_time.tv_usec - start_time.tv_usec;
    if(i==iterations){return (seconds * 1e9 + microseconds * 1e3) / iterations;}
    return -1;
}


/* Time measurement function for an empty trap into the operating system.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_syscall_time(unsigned int iterations){
    if(iterations==0){return -1.0;}

    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);
    int i;
    for (i = 0; i < iterations; i+=10) {
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
    }
    gettimeofday(&end_time, NULL);
    double seconds = end_time.tv_sec - start_time.tv_sec;
    double microseconds = end_time.tv_usec - start_time.tv_usec;
    if(i==iterations){return (seconds * 1e9 + microseconds * 1e3) / iterations;}
    return -1.0;
}