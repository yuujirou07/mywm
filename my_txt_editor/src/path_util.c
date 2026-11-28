#include <linux/limits.h>
#include <string.h>
#include <unistd.h>
#include<stdlib.h>
#include <sys/stat.h>
#include<sys/stat.h>
#include<pwd.h>


#include "path_util.h"
#include "editor_log.h"
#include "editor_types.h"

static int resolve_dir(const char **buf, size_t *buf_size);

// resolve_dir()が返すディレクトリを基準に、relativeを連結してbuf_sizeバイトのbufへ書く。
// 基準は、EDITOR_DEV_MODEなら実行ファイル(build/main)の1つ上=プロジェクト直下、通常ビルドなら設定ディレクトリ。
// 返り値: 成功時buf、引数不正・基準の取得失敗・長さ超過はNULL。bufは呼び出し側所有で、プラグイン内でも本体が基準。
char *editor_path_from_config_home(char *buf, size_t buf_size, const char *relative){
    if(buf == NULL || buf_size == 0 || relative == NULL){
        return NULL;
    }

    const char *base_dir = NULL;
    size_t base_dir_len = 0;
    if(resolve_dir(&base_dir,&base_dir_len) != 0){
        wrp_stderr_puts_("failed resolve dir");
        error_log_("failed resolve dir");
        return NULL;
    }

    int written = snprintf(buf, buf_size, "%s/%s", base_dir, relative);
    free((void *)base_dir);
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
    ssize_t path_len = readlink(SELF_SIM_LINK_,tmp_path,sizeof(tmp_path) - 1);


    if(path_len < 0){
        return -1;
    }

    tmp_path[path_len] = NUL_;

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
    
    slash[1] = NUL_;
    return path;
}


// pathの種別を判定する。シンボリックリンクはリンク先を辿らず、PATH_TYPE_LNKとして返す。
// 返り値: 対応する種別。pathがNULL・存在しない・lstat失敗はPATH_TYPE_ERROR、判定できない種別はPATH_TYPE_UNKOWN。
enum path_type get_path_type(const char *path){
    if(path == NULL){
        return PATH_TYPE_ERROR;
    }

    struct stat path_state = {0};
    if(lstat(path,&path_state) != 0){
        return PATH_TYPE_ERROR;
    }

    if(S_ISDIR(path_state.st_mode)){
        return PATH_TYPE_DIR;
    }
    if(S_ISREG(path_state.st_mode)){
        return PATH_TYPE_FILE;
    }
    if(S_ISLNK(path_state.st_mode)){
        return PATH_TYPE_LNK;
    }
    if(S_ISCHR(path_state.st_mode)){
        return PATH_TYPE_CHR;
    }
    if(S_ISBLK(path_state.st_mode)){
        return PATH_TYPE_BLK;
    }
    if(S_ISFIFO(path_state.st_mode)){
        return PATH_TYPE_FIFO;
    }
    if(S_ISSOCK(path_state.st_mode)){
        return PATH_TYPE_SOCK;
    }
    return PATH_TYPE_UNKOWN;
}




#ifdef EDITOR_DEV_MODE
// 開発用: 実行ファイル(build/main)の1つ上=プロジェクト直下のパスを返す(末尾に/は付けない)。
// 返り値: 成功時0(*bufはstrdupした文字列で呼び出し側がfree)、取得失敗・確保失敗は-1。
static int resolve_dir(const char **buf, size_t *buf_size){
    char exe_path[PATH_MAX];
    ssize_t len = readlink(SELF_SIM_LINK_, exe_path, sizeof(exe_path) - 1);
    if(len <= 0){
        return -1;
    }
    exe_path[len] = NUL_;

    // 実行ファイル名とbuildディレクトリ名を落とす
    for(int i = 0;i < 2;i++){
        char *slash = strrchr(exe_path, '/');
        if(slash == NULL){
            return -1;
        }
        *slash = NUL_;
    }

    char *dup_path = strdup(exe_path);
    if(dup_path == NULL){
        return -1;
    }
    *buf = dup_path;
    *buf_size = strlen(dup_path);
    return 0;
}

#else
// 通常用: $XDG_CONFIG_HOME、無ければホームディレクトリ配下の設定ディレクトリを解決する。
static int resolve_dir(const char **buf, size_t *buf_size){
    const char *tmp_config_path = NULL;
    
    tmp_config_path = getenv("XDG_CONFIG_HOME");
    if(tmp_config_path != NULL && get_path_type(tmp_config_path) == PATH_TYPE_DIR){
        *buf = strdup(tmp_config_path);
        *buf_size = strlen(tmp_config_path);
        return 0;
    }

    struct passwd *paswd = getpwuid(getuid());
    if(paswd != NULL && paswd->pw_dir != NULL &&
        get_path_type(paswd->pw_dir) == PATH_TYPE_DIR){

        size_t pw_dir_len = strlen(paswd->pw_dir);
        size_t global_config_name_len = strlen(GLOBAL_CONFIG_NAME);

        char tmp_config_path[pw_dir_len + global_config_name_len + 2];
        snprintf(
            tmp_config_path,
            sizeof(tmp_config_path),
            "%s/%s",
            paswd->pw_dir,GLOBAL_CONFIG_NAME
        );

        if(get_path_type(tmp_config_path) == PATH_TYPE_DIR)
        {
            *buf = strdup(tmp_config_path);
            *buf_size = strlen(tmp_config_path);
            return 0;
        }
    }
    return -1;
}
#endif


int get_editor_config_home(const char **buf, size_t *buf_size){
    return resolve_dir(buf, buf_size);
}