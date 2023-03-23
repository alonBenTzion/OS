#include <iostream>
#include "osm.h"
int main() {
    std::cout << osm_operation_time(1e6) << std::endl;
    std::cout << osm_function_time(1e6) << std::endl;
    std::cout <<osm_syscall_time(1e6) << std::endl;
    return 0;
}
