

#ifndef C_SETTINGS_SETTING_H
#define C_SETTINGS_SETTING_H



#include <stddef.h>
#include <wchar.h>


#define KEY_MAPPING_MAX_NUM 512


typedef enum{
    EDIT_MODE = 0,
    FILE_SEARCH_MODE = 1,
    FILE_TREE_MODE   = 2,
    HOME_SCREEN_MODE = 3,
    UNKNOWN_MODE = 4,
}MY_TXT_EDITOR_SCREEN_MODE;

typedef enum{
    API_OK = 0,
    API_ERR_WRONG_MODE = 1,
    API_ERR_INVALID_ARG = 2,
    API_ERR_NULL = 3,
    API_ERR_NO_MEMORY = 4,
}MY_TXT_EDITOR_RESULT;



typedef struct MY_TXT_EDITOR_API MY_TXT_EDITOR_API;

typedef void (*EDITOR_ACTION)(const MY_TXT_EDITOR_API *);
typedef MY_TXT_EDITOR_RESULT (*KEY_SET)(void *userdata,wchar_t key1,wchar_t key2,MY_TXT_EDITOR_SCREEN_MODE mode,EDITOR_ACTION action);
typedef MY_TXT_EDITOR_RESULT (*ADD_INPUT_CALL_BACK)(void *userdata,wchar_t input,MY_TXT_EDITOR_SCREEN_MODE mode,EDITOR_ACTION action);
typedef MY_TXT_EDITOR_RESULT (*GET_SCREEN_MODE)(const MY_TXT_EDITOR_API *,MY_TXT_EDITOR_SCREEN_MODE *rt);
typedef MY_TXT_EDITOR_RESULT (*SET_TEXT_CURSOR_ROW)(const MY_TXT_EDITOR_API *,int row);
typedef MY_TXT_EDITOR_RESULT (*SET_TEXT_CURSOR_COL)(const MY_TXT_EDITOR_API *,int col);

typedef MY_TXT_EDITOR_RESULT (*INPUT_CHAR)(const MY_TXT_EDITOR_API *api,wchar_t chr);

struct MY_TXT_EDITOR_API{
    void * userdata;
    void *screen_state;
    MY_TXT_EDITOR_RESULT (* save_file)(void *userdata);
    KEY_SET key;
    ADD_INPUT_CALL_BACK input_event;
    GET_SCREEN_MODE get_screen_mode;
    SET_TEXT_CURSOR_ROW set_text_cursor_row;
    SET_TEXT_CURSOR_COL set_text_cursor_col;
    INPUT_CHAR input_char;
};


void ST_INIT(const MY_TXT_EDITOR_API *api);

void ST_on_key(wchar_t ch,const MY_TXT_EDITOR_API *const api);


#endif