#ifndef KEY_EVENT_H
#define KEY_EVENT_H

#include <wchar.h>
#include "public_data/c_settings/c_settings_setting.h"
#include "keyboard/key_map.h"

// キー1文字の入力イベントへ登録する処理1件。
typedef struct{
    wchar_t chr; // 対象の入力文字。
    MY_TXT_EDITOR_SCREEN_MODE mode; // このイベントを有効にする画面。
    EDITOR_ACTION action_func; // 入力されたときに呼ぶ処理。
}key_event_data;

// key_event_dataの動的配列。確保・追加・解放はkey_event_list_add/destroyで行う。
typedef struct{
    key_event_data *key_event_list; // 登録された入力イベント。未確保ならNULL。
    int key_event_num; // 登録済みの件数。
    int key_event_allocate_num; // 確保済みの要素数。
}key_event_list_data;

// ユーザー設定の入力(キーマップ、今後はマウスイベントも)をまとめて管理する。
typedef struct{
    key_map_list_data key_maps; // ユーザーが登録したキーマップの一覧。
    key_event_list_data key_events; // ユーザーが登録した入力イベントの一覧。
}editor_input_manager;

// chrの入力イベントへmodeの画面でactionを呼ぶ処理を登録する。初回追加時に配列を確保し、満杯なら2倍に拡張する。
// 返り値: 成功0、引数不正・確保失敗-1(追加しない)。
int key_event_list_add(key_event_list_data *list,wchar_t chr,
                       MY_TXT_EDITOR_SCREEN_MODE mode,EDITOR_ACTION action);
// 配列を解放して空・未確保の状態へ戻す。listがNULLなら何もしない。
void key_event_list_destroy(key_event_list_data *list);

#endif
