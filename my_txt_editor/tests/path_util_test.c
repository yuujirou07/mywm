


#include <assert.h>
#include <stddef.h>
#include<stdio.h>


#include"path_util.h"

int main(){


    char self_path[256];

    
    get_self_path(self_path,sizeof(self_path));
    puts(self_path);
    get_path_dirname(self_path);
    puts(self_path);



    


    return 0;
}