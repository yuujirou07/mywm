


#include"settings_helper.h"

#include <sys/time.h>
#include<time.h>




ST_time ST_gettime_msec(){
    ST_time t;
    timespec_get(&t.timespec,CLOCK_MONOTONIC);
    return t;
}

double ST_getdifftime_msec(ST_time t1,ST_time t2){
    return ((t2.timespec.tv_sec - t1.timespec.tv_sec) * 1000) + 
        ((t2.timespec.tv_nsec - t1.timespec.tv_nsec)/1000000.0);
}