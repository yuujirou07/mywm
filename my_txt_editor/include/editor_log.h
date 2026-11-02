#ifndef ERROR_LOG_H
#define ERROR_LOG_H

#include <stdio.h>

#define ERROR_MSG_SIZE_MAX 1024 // 1件の整形済みエラーメッセージへ確保するバイト数。
#define LOG_MSG_SIZE_MAX 1024


#define EDITOR_LOG_DIR "log_data/"
#define EDITOR_ERROR_LOG_FILE_NAME "editor_error_log.txt"

int set_error_log_file(const char *relative_path);
int close_error_log_file();
void error_log_write(char *error_comment);
void editor_log(const char msg[],const char *date,const char *time,const char *file,int line);
int set_log_file(const char *relative_path);
int close_editor_log();


// 式名、ソースファイル、行番号を整形して現在のエラーログへ書き込む。

#define wrp_stderr_puts(msg)\
do{\
    fprintf(stderr,"MSG: %.*s FILE: %s  LINE: %d　DATE: %s TIME: %s\n",LOG_MSG_SIZE_MAX,(msg),__FILE__,__LINE__,__DATE__,__TIME__);\
    fflush(stderr);\
}while(0)


// editor_logへmsgとビルド日時、呼び出し元のファイル名と行番号を渡す。set_log_file()で出力先を設定済みであること。
#define editor_log_msg(msg) editor_log((msg),__DATE__,__TIME__,__FILE__,__LINE__)

#define error_log(msg) char fin_msg[ERROR_MSG_SIZE_MAX];\
snprintf(fin_msg,sizeof(fin_msg),"ERROR:# %s FILE: %s  LINE: %d DATE: %s TIME: %s\n",#msg,__FILE__,__LINE__,__DATE__,__TIME__);\
error_log_write(fin_msg);\




#endif
