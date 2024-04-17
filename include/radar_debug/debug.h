#ifndef DEBUG_H
#define DEBUG_H

#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

void inline mem_usage(double &vm_usage, double &resident_set) {
    vm_usage = 0.0;
    resident_set = 0.0;

    std::ifstream stat_stream("/proc/self/stat", std::ios_base::in);
    if (stat_stream) {
        std::string line;
        getline(stat_stream, line);

        // Extract relevant fields from the stat file
        unsigned long utime, stime, cutime, cstime;
        long rss_pages;
        sscanf(line.c_str(), "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %lu %lu %lu %lu %*d %*d %*d %*d %*d %ld", &utime,
            &stime, &cutime, &cstime, &rss_pages);

        // Convert page counts to kilobytes
        long page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024;
        resident_set = rss_pages * page_size_kb;

        // Calculate total virtual memory usage
        vm_usage = utime + stime + cutime + cstime;
    }
}

double inline mem_usage() {
    double vm_usage = 0.0;
    double resident_set = 0.0;

    mem_usage(vm_usage, resident_set);
    return vm_usage;
}

#endif