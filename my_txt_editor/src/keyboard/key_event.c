#include <stdlib.h>
#include "keyboard/key_event.h"
#include "editor_log.h"

#define KEY_EVENT_INIT_ALLOCATE_NUM 32 // 入力イベント配列の初期確保数。

int key_event_list_add(key_event_list_data *list,wchar_t chr,
                       MY_TXT_EDITOR_SCREEN_MODE mode,EDITOR_ACTION action){
    if(list == NULL || action == NULL){
        error_log("argument error");
        return -1;
    }

    if(list->key_event_allocate_num <= list->key_event_num){
        int new_num = (list->key_event_allocate_num == 0) ?
            KEY_EVENT_INIT_ALLOCATE_NUM : list->key_event_allocate_num * 2;
        key_event_data *tmp_list = realloc(list->key_event_list,sizeof(key_event_data) * new_num);
        if(tmp_list == NULL){
            error_log("can not realloc");
            return -1;
        }
        list->key_event_list = tmp_list;
        list->key_event_allocate_num = new_num;
    }

    key_event_data *tmp_key_event = &list->key_event_list[list->key_event_num];
    tmp_key_event->chr = chr;
    tmp_key_event->mode = mode;
    tmp_key_event->action_func = action;
    list->key_event_num++;
    return 0;
}

void key_event_list_destroy(key_event_list_data *list){
    if(list == NULL){
        return;
    }
    free(list->key_event_list);
    list->key_event_list = NULL;
    list->key_event_num = 0;
    list->key_event_allocate_num = 0;
}
