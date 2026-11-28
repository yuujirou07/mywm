#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "editor_log.h"
#include "path_util.h"

#define TEST_ERROR_LOG_PATH_ EDITOR_LOG_DIR_ "editor_log_test_error.txt"
#define TEST_LOG_PATH_ EDITOR_LOG_DIR_ "editor_log_test.log"

static int fail_count = 0;

#define CHECK_(cond)\
do{\
    if(!(cond)){\
        printf("  FAIL: %s (line %d)\n",#cond,__LINE__);\
        fail_count++;\
    }\
}while(0)

// pathの内容をbufへ読み込む。返り値: 読んだバイト数、開けなければ-1。bufは必ずNUL終端する。
static long read_text(const char *path,char *buf,size_t buf_size){
    FILE *fp = fopen(path,"rb");
    if(fp == NULL){
        return -1;
    }

    size_t len = fread(buf,1,buf_size - 1,fp);
    buf[len] = '\0';
    fclose(fp);
    return (long)len;
}

// 前回のログを消し、set_log_fileで開き直して空のログから始める。
static void reset_log(const char *log_abs_path){
    remove(log_abs_path);
    CHECK_(set_log_file(TEST_LOG_PATH_) == 0);
}

int main(void){
    char error_abs_path[PATH_MAX];
    char log_abs_path[PATH_MAX];
    char read_buf[1024];

    CHECK_(editor_path_from_config_home(error_abs_path,sizeof(error_abs_path),TEST_ERROR_LOG_PATH_) != NULL);
    CHECK_(editor_path_from_config_home(log_abs_path,sizeof(log_abs_path),TEST_LOG_PATH_) != NULL);
    remove(error_abs_path);

    // editor_log内のerror_logはerror_fileが無いとexitするので、先に設定する。
    CHECK_(set_error_log_file(TEST_ERROR_LOG_PATH_) == 0);

    printf("case1: 通常の書き込み\n");
    fflush(stdout);
    reset_log(log_abs_path);
    editor_log("hello","Nov  1 2026","02:40:12","f.c",7);
    CHECK_(read_text(log_abs_path,read_buf,sizeof(read_buf)) > 0);
    CHECK_(strcmp(read_buf," LOG: hello DATE: Nov  1 2026 TIME: 02:40:12 FILE: f.c LINE: 7\n") == 0);

    printf("case2: NULLはunknownになる\n");
    fflush(stdout);
    reset_log(log_abs_path);
    editor_log(NULL,NULL,NULL,NULL,0);
    CHECK_(read_text(log_abs_path,read_buf,sizeof(read_buf)) > 0);
    CHECK_(strcmp(read_buf," LOG: unknown DATE: unknown TIME: unknown FILE: unknown LINE: 0\n") == 0);

    printf("case3: editor_log_msgマクロ\n");
    fflush(stdout);
    reset_log(log_abs_path);
    editor_log_msg_("macro");
    CHECK_(read_text(log_abs_path,read_buf,sizeof(read_buf)) > 0);
    CHECK_(strncmp(read_buf," LOG: macro DATE: ",strlen(" LOG: macro DATE: ")) == 0);
    CHECK_(strstr(read_buf,__DATE__) != NULL);
    CHECK_(strstr(read_buf,__TIME__) != NULL);
    CHECK_(strstr(read_buf,__FILE__) != NULL);

    printf("case4: 2回書くと両方残る\n");
    fflush(stdout);
    reset_log(log_abs_path);
    editor_log("first","d1","t1","f.c",1);
    editor_log("second","d2","t2","f.c",2);
    CHECK_(read_text(log_abs_path,read_buf,sizeof(read_buf)) > 0);
    CHECK_(strstr(read_buf,"first") != NULL);
    CHECK_(strstr(read_buf,"second") != NULL);

    CHECK_(close_editor_log() == 0);
    CHECK_(close_editor_log() == -1);
    CHECK_(close_error_log_file() == 0);

    if(fail_count == 0){
        printf("ALL PASS\n");
        return 0;
    }
    printf("%d check(s) failed\n",fail_count);
    return 1;
}
