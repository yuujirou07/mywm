#ifndef KEY_MAP_H_
#define KEY_MAP_H_

#include <wchar.h>
#include "public_data/c_settings/c_settings_setting.h"

// ユーザー設定が登録したキーマップ1件。ch[0]・ch[1]の2キー列をmodeの画面で入力すると、action_funcを呼ぶ。
typedef struct{
    wchar_t ch[2]; // 2キーのショートカット。ch[0]が先に押したキー。
    MY_TXT_EDITOR_SCREEN_MODE mode; // このキーマップを有効にする画面。
    EDITOR_ACTION action_func; // 一致したときに呼ぶ処理。呼出し可能な関数であること。
}key_map_data;

// key_map_dataの動的配列。確保・追加・解放はkey_map_list_init/add/destroyで行う。
typedef struct{
    key_map_data *key_map_list; // 登録済みキーマップの配列。未確保ならNULL。
    int key_map_num; // 登録済みの件数。KEY_MAPPING_MAX_NUMを超えない。
    int key_map_allocate_num; // 配列へ確保済みの要素数。
}key_map_list_data;

// listを空にして、初期容量分の配列を確保する。
// 返り値: 成功0、listがNULLまたは確保失敗-1(失敗時は空・未確保の状態)。
int key_map_list_init(key_map_list_data *list);
// key1・key2の2キー列にmodeとactionを登録する。配列が満杯なら2倍に拡張する。
// 返り値: 成功0、引数不正・登録上限(KEY_MAPPING_MAX_NUM_)到達・拡張時の確保失敗-1(追加しない)。
int key_map_list_add(key_map_list_data *list,wchar_t key1,wchar_t key2,
                     MY_TXT_EDITOR_SCREEN_MODE mode,EDITOR_ACTION action);
// 配列を解放して空・未確保の状態へ戻す。listがNULLなら何もしない。
void key_map_list_destroy(key_map_list_data *list);

#endif
