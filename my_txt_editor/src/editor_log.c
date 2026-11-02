#include "editor_log.h"
#include "path_util.h"

#include <linux/limits.h>
#include <ncurses.h>
#include<stdlib.h>
#include <stddef.h>
#include<stdio.h>
#include <string.h>



static FILE *error_file = NULL;
static FILE *log_file = NULL;


// エラーログの出力先を初回だけ開く。
// 引数: relative_path=my_txt_editor/直下からの相対パス(内部で実行ファイル基準に連結する)。
//  返り値: 成功0、失敗-1。
// 所有権: 開いたFILEはこのモジュールが保持し、close_error_log_file()が閉じる。
int set_error_log_file(const char *relative_path){
    if(relative_path == NULL){
        wrp_stderr_puts("argument error");
        return -1;
    }

    char file_path[PATH_MAX];
    if(editor_path_from_exe_dir(file_path,sizeof(file_path),relative_path) == NULL){
        wrp_stderr_puts("can not resolve error_file path");
        return -1;
    }

    //error_fileがNULLじゃないとfcloseする
    if(error_file != NULL){
        int rt = fclose(error_file);
        error_file = NULL;  // 失敗してもFILEは無効なので必ずNULLへ戻す
        if(rt != 0){
            wrp_stderr_puts("can not close error_file");
            return -1;
        }
    }

    error_file = fopen(file_path,"a");

    //開けなかったことを通知する
    if(error_file == NULL){
        wrp_stderr_puts("can not open error_file");
        return -1;
    }
    return 0;
}

int set_log_file(const char *relative_path){
    if(relative_path == NULL){
        error_log("argument error pal: NULL");
        wrp_stderr_puts("argument error pal: NULL");
        return -1;
    }

    char log_file_path[PATH_MAX];
    if(editor_path_from_exe_dir(log_file_path,sizeof(log_file_path),relative_path) == NULL){
        error_log("can not resolve log file path");
        wrp_stderr_puts("can not resolve log file path");
        return -1;
    }

    if(log_file != NULL){
        int rt = fclose(log_file);
        log_file = NULL;  // 失敗してもFILEは無効なので必ずNULLへ戻す
        if(rt != 0){
            error_log("can not close log_file");
            wrp_stderr_puts("can not close log_file");
            return -1;
        }
    }  

    log_file = fopen(log_file_path,"a");


    if(log_file == NULL){
        error_log("can not open log file");
        wrp_stderr_puts("can not open log file");
        return -1;
    }
    return 0;
}

// モジュールが保持するログFILEを閉じる。未設定なら何もしない。
// 返り値: 成功0、未設定またはfclose()失敗は-1。
int close_error_log_file(){
    if(error_file == NULL){
        wrp_stderr_puts("error_file is NULL so can not close error_file");
        return -1;
    }

    int rt = fclose(error_file);
    error_file = NULL;
    if(rt != 0){
        wrp_stderr_puts("can not close error_file");
        return -1;
    }
    return 0;   
}

// 設定済みログへ最大ERROR_MSG_SIZE_MAXバイトを書き込む。
// 引数: error_comment=NUL終端された出力文字列。 返り値: なし。ログファイルが未設定なら何もしない。
void error_log_write(char *error_comment){
    if(error_file == NULL){
        wrp_stderr_puts("sorry can not open error_log_write ");
        exit(EXIT_FAILURE);
        return;
    } 

    size_t len = strlen(error_comment);
    len = (len > ERROR_MSG_SIZE_MAX)?ERROR_MSG_SIZE_MAX:len;

    // 危険: LSP本文など長さが外部入力に依存する文字列と同じ大きさのVLAを作る。
    // 大きなログ1件だけでスタックオーバーフローする可能性がある。
    char error_log[len + 1];    
    memmove(error_log, error_comment,sizeof(char ) * len);
    error_log[len] = '\0';
    fputs(error_log,error_file);
    fflush(error_file);
    return;
}



void editor_log(const char msg[],const char date[],const char time[],const char file[],int line){
    if(log_file == NULL){
        error_log("can not open editor log file");
        return;
    }

    if(msg == NULL){
        msg = "unknown";
    }
    if(date == NULL){
        date = "unknown";
    }
    if(time == NULL){
        time = "unknown";
    }
    if(file == NULL){
        file = "unknown";
    }

    if(fprintf(log_file," LOG: %s DATE: %s TIME: %s FILE: %s LINE: %d\n",msg,date,time,file,line) < 0){
        error_log("can not puts log data");
        wrp_stderr_puts("can not puts log data");
    }
    else{
        fflush(log_file);
    }

    return;
}



int close_editor_log(){
    if(log_file == NULL){
        wrp_stderr_puts("log_file is NULL so can not close log_file");
        return -1;
    }

    int rt = fclose(log_file);
    if(rt < 0){
        wrp_stderr_puts("can not close log_file");
        return rt;
    }
    log_file = NULL;
    return 0;
}