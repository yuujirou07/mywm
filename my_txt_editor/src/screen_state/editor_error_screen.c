#include <string.h>
#include "editor_log.h"
#include "editor_screen.h"
#include "editor_color.h"

// エラー画面でEnterが押されたら遷移元に応じて画面へ戻す。
// 引数: ctx=編集画面復帰に必要なcontext、ch=入力文字またはKEY_*。 返り値: 入力ループを続けるならtrue。
bool handle_error_screen_input(struct editor_input_context *ctx, wint_t ch){
    struct editor_state *state = ctx->state;

    if(ch == KEY_ENTER || ch == '\n' || ch == '\r'){
        clear();
        enum screen_state previous_state = editor_get_screen_state_log(state,1);
        // start menu由来のfile browserで発生したエラーでは、2つ前が戻り先になる。
        enum screen_state previous_previous_state =
            editor_get_screen_state_log(state,2);

        if(previous_state == start_menu_screen_mode ||
           (previous_state == file_browse_screen_mode &&
            previous_previous_state == start_menu_screen_mode)){
            my_cur_set(state,false);
            editor_set_screen_state(state, start_menu_screen_mode);
        }
        else{
            my_cur_set(state,true);
            editor_set_screen_state(state, edit_screen_mode);
            state->render_flags |= RENDER_EDIT_SCREEN_BASE;
            state->render_flags |= RENDER_FILE_DATA;
        }
        
    }
    return true;
}

// エラー表示用の画面へ切り替え、中央にメッセージを表示する。
// 引数: state=画面状態と表示領域、error_comment=表示するエラーメッセージ。 返り値: なし。
void editor_error_screen(struct editor_state *state,char *error_comment){
    my_cur_set(state,false);
    clear();
    editor_set_screen_state(state, error_screen_mode);

    int screen_center_x     = state->file_browse.area.pos.x + (state->file_browse.area.w/2);
    int error_comment_size  = strlen(error_comment);
    int error_size          = sizeof("error");
    int press_enter_comment = sizeof("press enter to back");
    int comment_start_pos_x = screen_center_x - (error_comment_size/2);
    int error_start_pos_x   = screen_center_x - (error_size/2);
    int press_enter_comment_start_pos_x = screen_center_x - (press_enter_comment/2);
    int screen_center_y        =  state->file_browse.area.pos.y + (state->file_browse.area.h/2);

    attr_t error_attr = COLOR_PAIR(get_color_pair(state->input_ctx,COLOR_BLACK,COLOR_RED));
    attron(error_attr);
    mvaddstr(screen_center_y-10,error_start_pos_x,"error");
    attroff(error_attr);
    mvaddstr(screen_center_y-9,comment_start_pos_x,error_comment);
    mvaddstr(screen_center_y-8,press_enter_comment_start_pos_x,"press enter to back");
    error_log(error_comment);//ファイル出力
    refresh();
    flushinp();
}
