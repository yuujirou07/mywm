

#ifndef SETTINGS_HELPER_H
#define SETTINGS_HELPER_H



#include <time.h>

typedef struct{
   struct timespec timespec;
}ST_time;


ST_time ST_gettime_msec();
double ST_getdifftime_msec(ST_time t1,ST_time t2);











#endif