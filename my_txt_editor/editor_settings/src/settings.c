


#include <bits/time.h>
#include<time.h>

#include"public_data/c_settings/c_settings_setting.h"
#include"/home/yuujirou07/vscode_proj/mywm_proj/my_txt_editor/editor_settings/settings_libs/include/settings_helper.h"


// 有効なapiを受け取り、設定キーマップの終了処理としてexit(0)を呼ぶ。
// 返り値: 戻らない。エディタ側の通常の終了処理は経由しない。
void func(const MY_TXT_EDITOR_API *api){
    api->save_file(api->userdata);
}



void input_func(const MY_TXT_EDITOR_API *api){
    static ST_time time = {0};
    static int is_first = 0;
    if(is_first == 0){
        time = ST_gettime_msec();
        is_first = 1;
        return;
    }

    ST_time time2 = ST_gettime_msec();

    double d = ST_getdifftime_msec(time,time2);
        
    if(d < 500){
        api->save_file(api->userdata);
    }
    is_first = 0;
    
    return;    
}

// apiのキー登録関数へ、o・oの2文字に対応する終了処理funcを登録する。
// 返り値: なし。apiとuserdata・keyはホスト側で接続済みであること。
void ST_INIT(const MY_TXT_EDITOR_API *api){
    api->key(api->userdata,L'o',L'o',EDIT_MODE,func);
    api->input_event(api->userdata,L'j',input_func);
}
