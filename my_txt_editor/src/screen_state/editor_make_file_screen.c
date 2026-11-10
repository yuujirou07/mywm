#include <ncurses.h>
#include <string.h>
#include <wctype.h>
#include "editor.h"
#include "editor_color.h"
#include "editor_screen.h"

// 未保存ファイル作成確認と新規ファイル名入力を処理する。
// 引数: ctx=確認ダイアログと編集画面復帰に必要なcontext、input_result=get_wch()の結果、ch=入力文字またはKEY_*。 返り値: 入力ループを続けるならtrue。
bool handle_ask_make_file_mode_input(struct editor_input_context *ctx, int input_result, wint_t ch){
    struct editor_state *state = ctx->state;
    //入力画面時処理
    if(state->make_file_mode_status.is_input_scene){
        int input_limit = state->write_file_name_area.w - 2;
        if(input_result == OK && iswprint((wint_t)ch) &&
           state->make_file_mode_status.new_file_name_counter < input_limit &&
           state->make_file_mode_status.new_file_name_counter <
               (int)sizeof(state->make_file_mode_status.new_file_name) - 1){

            int index = state->make_file_mode_status.new_file_name_counter++;
            state->make_file_mode_status.new_file_name[index] = (char)ch;
            state->make_file_mode_status.new_file_name[index + 1] = '\0';
            state->render_flags |= RENDER_MAKE_FILE;
            
        }
        else if (input_result == KEY_CODE_YES && ch == KEY_BACKSPACE && state->cursor.is_cur_show
            && state->make_file_mode_status.new_file_name_counter > 0){
            state->make_file_mode_status.new_file_name_counter--;
            state->make_file_mode_status.new_file_name[
                state->make_file_mode_status.new_file_name_counter] = '\0';
            state->render_flags |= RENDER_MAKE_FILE;
        }
        else if(ch == KEY_ENTER || ch == '\n' || ch == '\r' || ch == ' '){
            //もし何も入力されていなかったら
            if(state->make_file_mode_status.new_file_name_counter <= 0){
                return true;
            }
            state->make_file_mode_status.
                new_file_name[state->make_file_mode_status.new_file_name_counter] = '\0';

            editor_set_screen_state(state, edit_screen_mode);
            memcpy(state->file_data.now_open_path_name,
                state->make_file_mode_status.new_file_name,
                sizeof(state->file_data.now_open_path_name));

            save_file(state);

            if(editor_get_screen_state(state) == error_screen_mode)return true;

            state->make_file_mode_status.new_file_name_counter = 0;
            state->make_file_mode_status.is_input_scene = false;
            memset(state->make_file_mode_status.new_file_name,
                0,
                sizeof(state->make_file_mode_status.new_file_name));

            clear();
            editor_set_screen_state(state,edit_screen_mode);
            state->render_flags |= RENDER_EDIT_SCREEN_BASE;
            state->render_flags |= RENDER_FILE_DATA;
            editor_sync_cursor(state);
        }
    }
    else{
        if(ch == 'y'){
            state->make_file_mode_status.is_input_scene = true;
            state->cursor.is_cur_show = true;
            state->render_flags |= RENDER_MAKE_FILE;
            return true;

        }
        else if(ch == 'n'){
            clear();
            state->make_file_mode_status.is_input_scene = false;
            state->render_flags |= RENDER_EDIT_SCREEN_BASE;
            state->render_flags |= RENDER_FILE_DATA;
            editor_set_screen_state(state, edit_screen_mode);
            editor_sync_cursor(state);
            my_cur_set(state,true);
        }

    }
    return true;
}

// stateと画面中央の基準screen_center_y/screen_center_posから作成確認枠を計算し、*file_boxへ保存する。
// 返り値: なし。描画は予約し、カーソル表示フラグを下げる。winは未使用で、file_boxは非NULLとする。
void show_make_file_prompt(WINDOW *win, struct editor_state *state, struct box *file_box,
                                  int screen_center_y, struct pos screen_center_pos){
    (void)win;
    char comment_str[] = "The file cannot be found; would you like to create it?";
    int comment_str_len = strlen(comment_str);
    int box_h = (comment_str_len / state->scr.scr_size.x + 1) + 3;
    int box_w = (state->scr.scr_size.x > comment_str_len + 2)
        ? comment_str_len + 2 : state->scr.scr_size.x;
    struct box make_file_box;
    make_file_box.pos.y = screen_center_y - (screen_center_y / 2);
    make_file_box.pos.x = screen_center_pos.x - (comment_str_len / 2);
    make_file_box.h = box_h;
    make_file_box.w = box_w;

    state->render_flags |= RENDER_MAKE_FILE;
    state->cursor.is_cur_show = false;
    *file_box = make_file_box;
}


//編集画面でファイルを保存するときのモーション
int file_save_motion(struct editor_input_context *ctx){

    struct editor_state *state = ctx->state;
    WINDOW *win = ctx->win;
      // 編集位置はstate->cursorが持ち続けるため、画面を離れる前の退避は不要。
    save_file(state);
    flushinp();
    if(editor_get_screen_state(state) == ask_make_file_mode){
        show_make_file_prompt(
            win,
            state,
            &state->ask_make_file_box,
            ctx->ask_make_file_mode.screen_center_y,
            ctx->ask_make_file_mode.screen_center_pos
        );
    }
    return 0;
}

// 新規作成の確認画面またはファイル名入力画面を描画する。
// 引数: ctx=画面状態・配置基準・描画先を持つ入力context。 返り値: なし。入力画面ではカーソル反映位置と入力欄の矩形も更新する。
void draw_make_file_dialog(struct editor_input_context *ctx){
    struct editor_state *state = ctx->state;
    WINDOW *win = ctx->win;

    if(!state->make_file_mode_status.is_input_scene){
        char comment[] = "The file cannot be found; would you like to create it?";
        int comment_len = strlen(comment);
        int box_h = comment_len / state->scr.scr_size.x + 4;
        int box_w = (state->scr.scr_size.x > comment_len + 2)
            ? comment_len + 2 : state->scr.scr_size.x;
        struct box box = {
            .pos = {ctx->ask_make_file_mode.screen_center_pos.x - comment_len / 2,
                    ctx->ask_make_file_mode.screen_center_y -
                    ctx->ask_make_file_mode.screen_center_y / 2},
            .w = box_w,
            .h = box_h,
        };
        struct pos text_pos = {box.pos.x + 1, box.pos.y + 1};

        state->ask_make_file_box = box;
        draw_box(box, win);
        my_mvaddstr(text_pos, comment);
        mvaddstr(text_pos.y + 1, text_pos.x + box.w / 2 - (int)strlen("YES[y]") - 2, "YES[y]");
        mvaddstr(text_pos.y + 1, text_pos.x + box.w / 2 + 2, "NO[n]");
        my_cur_set(state,false);
        editor_sync_cursor(state);
        return;
    }

    struct box write_box = {
        .pos = state->ask_make_file_box.pos,
        .w = state->ask_make_file_box.w,
        .h = state->ask_make_file_box.h + 2,
    };
    struct box input_box = {
        .pos = {write_box.pos.x + 1, write_box.pos.y + write_box.h - 4},
        .w = write_box.w - 2,
        .h = 3,
    };
    char *label = "write a new file name";
    int label_x = write_box.pos.x + (write_box.w - (int)strlen(label)) / 2;

    struct clear_box_data clear_data = {
        .clear_box = {write_box},
        .clear_box_counter = 1,
    };

    clear_box(&clear_data);
    draw_box(write_box, win);
    draw_box(input_box, win);
    mvaddstr(write_box.pos.y + 1, label_x, label);
    mvaddnstr(input_box.pos.y + 1, input_box.pos.x + 1,
               state->make_file_mode_status.new_file_name,
               state->make_file_mode_status.new_file_name_counter);
    state->write_file_name_area = input_box;
    cur_pos_push((struct pos){input_box.pos.x + 1 + state->make_file_mode_status.new_file_name_counter,
                 input_box.pos.y + 1},state);
    my_cur_set(state,true);
}
