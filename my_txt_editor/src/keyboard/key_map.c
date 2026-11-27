#include <stdlib.h>
#include "keyboard/key_map.h"
#include "editor_log.h"

#define KEY_MAP_INIT_ALLOCATE_NUM_ 16 // キーマップ配列の初期確保数。

int key_map_list_init(key_map_list_data *list){
    if(list == NULL){
        return -1;
    }

    list->key_map_num = 0;
    list->key_map_allocate_num = 0;
    list->key_map_list = malloc(sizeof(key_map_data) * KEY_MAP_INIT_ALLOCATE_NUM_);
    if(list->key_map_list == NULL){
        error_log_("malloc");
        return -1;
    }
    list->key_map_allocate_num = KEY_MAP_INIT_ALLOCATE_NUM_;
    return 0;
}

int key_map_list_add(key_map_list_data *list,wchar_t key1,wchar_t key2,
                     MY_TXT_EDITOR_SCREEN_MODE mode,EDITOR_ACTION action){
    if(list == NULL || action == NULL){
        return -1;
    }

    //キーマップ最大登録数ガード
    if(list->key_map_num >= KEY_MAPPING_MAX_NUM_){
        error_log_("over flow key map tabele");
        return -1;
    }

    //メモリの拡張処理
    if(list->key_map_allocate_num <= list->key_map_num){
        int new_num = (list->key_map_allocate_num == 0) ?
            KEY_MAP_INIT_ALLOCATE_NUM_ : list->key_map_allocate_num * 2;
        key_map_data *tmp_list = realloc(list->key_map_list,sizeof(key_map_data) * new_num);
        if(tmp_list == NULL){
            error_log_("can not realloc");
            return -1;
        }
        list->key_map_list = tmp_list;
        list->key_map_allocate_num = new_num;
    }

    key_map_data *tmp_key_map = &list->key_map_list[list->key_map_num];
    tmp_key_map->ch[0] = key1;
    tmp_key_map->ch[1] = key2;
    tmp_key_map->mode = mode;
    tmp_key_map->action_func = action;
    list->key_map_num++;
    return 0;
}

void key_map_list_destroy(key_map_list_data *list){
    if(list == NULL){
        return;
    }
    free(list->key_map_list);
    list->key_map_list = NULL;
    list->key_map_num = 0;
    list->key_map_allocate_num = 0;
}
