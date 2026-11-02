




#include<stdio.h>




#include"editor_log.h"




int main(){

    set_log_file("/home/yuujirou07/vscode_proj/mywm_proj/my_txt_editor/tests");

    error_log("hello tester");


    editor_log_msg("hello tester");


    close_error_log_file();
    
    
    return 0;

}