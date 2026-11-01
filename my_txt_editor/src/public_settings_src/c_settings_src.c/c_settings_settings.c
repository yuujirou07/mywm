

#include <assert.h>
#include <dlfcn.h>
#include <limits.h>
#include <linux/limits.h>
#include <stddef.h>
#include<stdio.h>
#include<stdlib.h>
#include <sys/stat.h>
#include <wchar.h>


#include "c_settings.h"
#include "editor_log.h"
#include "editor_types.h"
#include "kbd_input_log.h"
#include "path_util.h"
#include "public_data/c_settings/c_settings_setting.h"
#include"txt_editor.h"
#include"string.h"
#include "txt_editor_screen.h"

static void api_save_file(void *userdata);
static void *load_current_settings_obj_c_file(struct editor_input_context *ctx);
static void api_key_mapping(void *userdata,wchar_t key1,wchar_t key2,MY_TXT_EDITOR_SCREEN_MODE mode,EDITOR_ACTION action);
static void add_key_input_event_handle(void *userdata,wchar_t chr,EDITOR_ACTION action);
static int check_input_handle(struct editor_input_context *ctx,wchar_t chr);


// apiへ借用したctxと、保存・キーマップ登録用の関数ポインタを接続する。
// 返り値: なし。apiとctxは非NULLで、API利用中はctxを有効に保つ。
void connect_api_mem_data(MY_TXT_EDITOR_API *const api,struct editor_input_context *ctx){
    api->userdata  = ctx; 
    api->save_file = api_save_file;
    api->key = api_key_mapping;
    api->input_event = add_key_input_event_handle;

    return;
}

// userdataをeditor_input_contextとして扱い、そのstateの本文を保存する。
// 返り値: なし。userdataは有効なctxであること。保存結果の画面遷移はsave_fileに従う。
static void api_save_file(void *userdata){
    struct editor_input_context *tmp_ctx = userdata;
    //編集画面しか機能しないようにする
    if(editor_get_screen_state(tmp_ctx->state) == edit_screen){
        file_save_motion(tmp_ctx);
    }
    return;
}



// userdataのctxへkey1・key2の組とmode(有効にする画面)・actionを登録する。キーマップ配列は事前に確保しておく。
// 返り値: なし。上限到達・再確保失敗はログを残して追加しない。actionは呼出し可能な関数を渡す。
static void api_key_mapping(void *userdata,wchar_t key1,wchar_t key2,MY_TXT_EDITOR_SCREEN_MODE mode,EDITOR_ACTION action){
    struct editor_input_context *ctx = (struct editor_input_context *)userdata;

    //キーマップ最大登録数ガード
    if(ctx->input_manager.key_maps.key_map_num >= KEY_MAPPING_MAX_NUM){
        error_log("over flow key map tabele");
        return;
    }

    //メモリの拡張処理
    if(ctx->input_manager.key_maps.key_map_allocate_num <= ctx->input_manager.key_maps.key_map_num){
        key_map_data *tmp_key_mapps = 
            realloc(ctx->input_manager.key_maps.key_mapp_list,
                sizeof(key_map_data) * (ctx->input_manager.key_maps.key_map_num * 2)); 
            if(tmp_key_mapps == NULL){
                error_log(can not realloc);
                return;
            }
            ctx->input_manager.key_maps.key_mapp_list = tmp_key_mapps;
            ctx->input_manager.key_maps.key_map_allocate_num = ctx->input_manager.key_maps.key_map_num * 2;
    }
    ctx->input_manager.key_maps.key_mapp_list[ctx->input_manager.key_maps.key_map_num].ch[0] = key1;
    ctx->input_manager.key_maps.key_mapp_list[ctx->input_manager.key_maps.key_map_num].ch[1] = key2;
    ctx->input_manager.key_maps.key_mapp_list[ctx->input_manager.key_maps.key_map_num].mode = mode;
    ctx->input_manager.key_maps.key_mapp_list[ctx->input_manager.key_maps.key_map_num].action_func = action;
    ctx->input_manager.key_maps.key_map_num++;
    return;
}


// apiへctxを接続し、editor_settings/so_file/settings.soからST_INITを呼んで設定を登録する。
// 返り値: なし。読込失敗は登録せず戻り、成功したハンドルはctxが保持して終了時にdlcloseする。
void init_settings_src(MY_TXT_EDITOR_API *const api,struct editor_input_context *ctx){
    connect_api_mem_data(api,ctx);
   
    void *handle = load_current_settings_obj_c_file(ctx);
    assert(handle);
    if(handle == NULL){
        return;
    }
    dlerror();
    void (*settings_init)(const MY_TXT_EDITOR_API *) = dlsym(handle,"ST_INIT");
    char *error = dlerror();
    if(error != NULL || settings_init == NULL){
        error_log_write("can not find ST_INIT\n");
        dlclose(handle);
        return;
    }
    ctx->dl_data.loading_dynamic_lib[ctx->dl_data.now_loading_lib_num++] = handle;
    settings_init(api);
    return;
}

// ctxの登録一覧でchrの2文字と一致する全項目を探し、それぞれの処理へapiを渡して実行する。
// 返り値: なし。chrは2要素以上で、登録関数・ctx・apiは有効であること。不一致は何もしない。
void check_key_mapps_entry(struct editor_input_context *ctx,wchar_t chr[2],MY_TXT_EDITOR_API *api){
    if(api == NULL || ctx == NULL)return;
    screen_state editor_screen_state = editor_get_screen_state(ctx->state);

    
    for(int i = 0;i < ctx->input_manager.key_maps.key_map_num;i++){
        //キーが押されているか
        if(ctx->input_manager.key_maps.key_mapp_list[i].ch[0] == chr[0] &&
            ctx->input_manager.key_maps.key_mapp_list[i].ch[1] == chr[1]){
                
            screen_state combed_screen_state = 
                comb_my_txt_editor_screen_mode_To_screen_state(
                    ctx->input_manager.key_maps.key_mapp_list[i].mode
                );
            //設定したスクリーンステートだったら実行する
            if(editor_screen_state == combed_screen_state){
                ctx->input_manager.key_maps.key_mapp_list[i].action_func(api);
            }
        }
    }
    return;
}

// キー入力chrに登録された入力イベントを呼ぶ予定の関数。
// 現実装は引数の確認だけで、イベントは呼ばない(未実装)。引数: ctx=入力context、chr=入力文字、api=ユーザー設定へ渡すAPI。 返り値: 引数不正は-1。

static void add_key_input_event_handle(void *userdata,wchar_t chr,EDITOR_ACTION action){
    if(userdata== NULL){
        error_log("argument error");
        return;
    }

    struct editor_input_context *ctx = userdata;

    if(ctx->input_manager.key_events.key_event_data == NULL){
        ctx->input_manager.key_events.key_map_allocate_num = 32;
        ctx->input_manager.key_events.key_map_num = 0;
        ctx->input_manager.key_events.key_event_data = 
            malloc(sizeof(key_event_data) * ctx->input_manager.key_events.key_map_allocate_num);
    }
    else if(ctx->input_manager.key_events.key_map_allocate_num <= ctx->input_manager.key_events.key_map_num){
        key_event_data *tmp_key_event_data = 
            realloc(
                ctx->input_manager.key_events.key_event_data,
                ctx->input_manager.key_events.key_map_num * 2
            );
        if(tmp_key_event_data == NULL){
            error_log("argument error");
            return;
        }

        ctx->input_manager.key_events.key_event_data = tmp_key_event_data;
        ctx->input_manager.key_events.key_map_allocate_num  = 
            ctx->input_manager.key_events.key_map_num * 2;
    }

    key_event_data *tmp_key_event = 
        &ctx->input_manager.key_events.key_event_data[ctx->input_manager.key_events.key_map_num];
    
    tmp_key_event->chr = chr;
    tmp_key_event->action_func = action;
    ctx->input_manager.key_events.key_map_num++;
    return;
}

// ctxのハンドル格納余地を確認し、editor_settings/so_file/settings.soをdlopenする。
// 返り値: 読込ハンドル、格納余地不足・パス不正・読込失敗はNULL。成功したハンドルは呼び出し側がdlcloseする。
static void *load_current_settings_obj_c_file(struct editor_input_context *ctx){
    if(ctx->dl_data.loading_dynamic_lib == NULL ||
        ctx->dl_data.now_loading_lib_num > ctx->dl_data.now_loading_lib_allocate_num){
        error_log("can not store settings library");
        return NULL;
    }
    char obj_path[PATH_MAX];
    if(editor_path_from_exe_dir(obj_path, sizeof(obj_path),SETTINGS_SO_PATH) == NULL){
        error_log("can not found settings obj file");
        return NULL;
    }

    struct stat st;
    if(stat(obj_path, &st) != 0 || !S_ISREG(st.st_mode)){
        error_log("can not found settings obj file");
        return NULL;
    }

    void *handle = dlopen(obj_path,RTLD_NOW | RTLD_GLOBAL);
    if(handle == NULL){
        error_log("can not load settings library");
        
    }
    return handle;
}



// ctxのキーマップ件数を参照するが、現実装ではイベント処理を行わない。
// 返り値: 常に0。ctxは非NULLとする。
int check_user_settings_event(struct editor_input_context *ctx){
    if(ctx == NULL){
        wrp_stderr_puts("argument error");
        return -1;
    }

    //ショートカットキー処理
    size_t tmp_key_data_arry_num = 0;
    const key_data *tmp_key_data_arry = KL_GetLog(ctx->state,&tmp_key_data_arry_num);
    //ショートカットは2キー以上からなのでそれ以下は除外する
    if(tmp_key_data_arry_num > 0){
        if(tmp_key_data_arry_num > 1){
            wchar_t chr[2] = 
                {
                    tmp_key_data_arry[tmp_key_data_arry_num - 2].key,
                    tmp_key_data_arry[tmp_key_data_arry_num - 1].key
                };

            //同じスクリーンステートのでのショートカットキーに今のとこ限定する
            if(tmp_key_data_arry[tmp_key_data_arry_num - 1].screen_state ==
                tmp_key_data_arry[tmp_key_data_arry_num - 2].screen_state){
                check_key_mapps_entry(ctx,chr,&ctx->editor_api);      
            }  
        }
        check_input_handle(ctx,tmp_key_data_arry[tmp_key_data_arry_num - 1].key);
    }
    
    


    return 0;
}


// key_mapp_dataの先頭キーを0へ書き換える。現実装では登録アクションを呼ばない。
// 返り値: 常に0。key_mapp_dataは非NULLとする。
int call_user_key_mapping_event(key_map_data *key_mapp_data){
    *key_mapp_data->ch = 0;
    return 0;
}


// dl_pathをeditor_settings/直下からの相対パスとして絶対パスにし、ctxの読込パス配列へ追加する。
// 配列が未確保なら32件分を確保し、満杯なら2倍に広げる。追加した文字列はctxが所有し、destroy_dl_path()が解放する。
// 引数: ctx=読込状態を持つ入力context、dl_path=editor_settings/からの相対パス。 返り値: 成功0、引数不正・確保失敗・基準パス取得失敗は-1。
int add_dl_path(struct editor_input_context *ctx,char *dl_path){
    if(ctx == NULL || dl_path == NULL){
        wrp_stderr_puts("argument error");
        return -1;
    }
    //初期化malloc
    if(ctx->dl_data.loading_dynamic_lib == NULL){

        ctx->dl_data.now_loading_lib_allocate_num = 32;
        ctx->dl_data.now_loading_lib_num = 0;
        ctx->dl_data.loading_dynamic_lib = malloc(sizeof(char *) * ctx->dl_data.now_loading_lib_allocate_num);
        if(ctx->dl_data.loading_dynamic_lib == NULL){
            wrp_stderr_puts("malloc error");
            return -1;
        }
    }
    else if(ctx->dl_data.now_loading_lib_allocate_num <= ctx->dl_data.now_loading_lib_num){
        char **tmp_dl_path = realloc(ctx->dl_data.loading_dynamic_lib,sizeof(char *) * ctx->dl_data.now_loading_lib_num * 2);
        if(tmp_dl_path == NULL){
            wrp_stderr_puts("realloc error");
            return -1;
        }
        ctx->dl_data.loading_dynamic_lib = tmp_dl_path;
        ctx->dl_data.now_loading_lib_allocate_num = ctx->dl_data.now_loading_lib_num * 2;
    }

    // 基準ディレクトリはPATH_MAXで必ず収まるので固定配列で取り、結合後の文字列だけ必要な長さで確保する
    char base_dir[PATH_MAX];
    if(editor_path_from_exe_dir(base_dir,sizeof(base_dir),EDITOR_SETTINGS_DIR) == NULL){
        wrp_stderr_puts("can not get settings dir");
        return -1;
    }

    size_t full_path_size = strlen(base_dir) + strlen(dl_path) + 1;
    char *full_path = malloc(full_path_size);
    if(full_path == NULL){
        wrp_stderr_puts("malloc error");
        return -1;
    }
    snprintf(full_path,full_path_size,"%s%s",base_dir,dl_path);
    editor_log_msg(full_path);
    ctx->dl_data.loading_dynamic_lib[ctx->dl_data.now_loading_lib_num] = full_path;
    ctx->dl_data.now_loading_lib_num++;
    return 0;
}

// add_dl_pathで保持したパス文字列と、それを指す配列を開放し、空の状態へ戻す。
// 返り値: 成功0、ctxがNULLなら-1。
int destroy_dl_path(struct editor_input_context *ctx){
    if(ctx == NULL){
        wrp_stderr_puts("argument error");
        return -1;
    }

    for(int i = 0;i < ctx->dl_data.now_loading_lib_num;i++){
        free(ctx->dl_data.loading_dynamic_lib[i]);
    }
    free(ctx->dl_data.loading_dynamic_lib);
    ctx->dl_data.loading_dynamic_lib = NULL;
    ctx->dl_data.now_loading_lib_num = 0;
    ctx->dl_data.now_loading_lib_allocate_num = 0;

    return 0;
}

static int check_input_handle(struct editor_input_context *ctx,wchar_t chr){
    if(ctx == NULL){
        error_log("argument error");
        return -1;
    }

    for(int i = 0; i < ctx->input_manager.key_events.key_map_num;i++){
        if(ctx->input_manager.key_events.key_event_data[i].chr == chr){
            ctx->input_manager.key_events.key_event_data[i].action_func(&ctx->editor_api);
        }
    }
    return 0;
}

