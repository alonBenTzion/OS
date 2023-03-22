#include <stdio.h>
#include "osm.h"

int main()
{
    printf("%f\n", osm_operation_time(1e6));
    printf("%f\n", osm_function_time(1e6));
    printf("%f\n", osm_syscall_time(1e6));

    return 0;
}
