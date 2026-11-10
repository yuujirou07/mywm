#include <limits.h>
#include <linux/limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "path_util.h"
#include "editor_log.h"

// 実行ファイル(build/main)の1つ上=プロジェクト直下を基準に、relativeを連結してbuf_sizeバイトのbufへ書く。
// 返り値: 成功時buf、引数不正・位置取得失敗・長さ超過はNULL。bufは呼び出し側所有で、プラグイン内でも本体が基準。
char *editor_path_from_exe_dir(char *buf, size_t buf_size, const char *relative){
    if(buf == NULL || buf_size == 0 || relative == NULL){
        return NULL;
    }


    char exe_path[PATH_MAX];
    ssize_t len = readlink(SELF_SIM_LINK, exe_path, sizeof(exe_path) - 1);
    if(len <= 0){
        wrp_stderr_puts("failed readlink");
        error_log("failed readlink");
        return NULL;
    }

    exe_path[len] = '\0';

    // 実行ファイル名とbuildディレクトリ名を落として、プロジェクト直下(my_txt_editor/)にする
    for(int i = 0;i < 2;i++){
        char *slash = strrchr(exe_path, '/');
        if(slash == NULL){
            return NULL;
        }
        *slash = '\0';
    }

    int written = snprintf(buf, buf_size, "%s/%s", exe_path, relative);
    if(written < 0 || (size_t)written >= buf_size){
        return NULL;
    }
    return buf;
}



// pathへ自分の実行ファイルのパスを書く。
// 返り値: 成功0、s不足なら必要バイト数(終端含む・正)、失敗-1。
int get_self_path(char *path,size_t s){
    if(path == NULL || s == 0){
        return -1;
    }

    char tmp_path[PATH_MAX];
    ssize_t path_len = readlink(SELF_SIM_LINK,tmp_path,sizeof(tmp_path) - 1);


    if(path_len < 0){
        return -1;
    }

    tmp_path[path_len] = '\0';

    size_t need_len = (size_t)path_len + 1;

    if(need_len > s){
        return (int)need_len;
    }

    memcpy(path,tmp_path,need_len);
    return 0;
}


// pathの最後の'/'の直後で文字列を切る(末尾の'/'は残す)。pathは終端済みで書き換え可能なこと。
// 返り値: '/'があればpath、なければNULL(pathは変更しない)。
char *get_path_dirname(char *path){
    if(path == NULL){
        return NULL;
    }

    char *slash = strrchr(path,'/');
    if(slash == NULL){
        return NULL;
    }
    
    slash[1] = '\0';
    return path;
}


