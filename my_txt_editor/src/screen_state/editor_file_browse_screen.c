#include <dirent.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
#include "editor_log.h"
#include "editor.h"
#include "editor_screen.h"
#include "editor_syntax.h"
#include "editor_color.h"
#include "editor_icon.h"

// ファイルブラウザ画面の移動・選択・復帰を処理する。
// 引数: ctx=入力context(state->file_browseにファイル一覧・現在パスが入っている)、input_result=get_wch()の結果、ch=入力文字またはKEY_*。 返り値: 入力ループを続けるならtrue。
bool handle_file_browse_screen_input(struct editor_input_context *ctx, int input_result, wint_t ch){
    struct editor_state *state = ctx->state;
    struct file_browse_state *file_browse = &state->file_browse;


    //もしインプットモードなら英数字のキーには反応させない
    bool input_mode = get_file_browse_path_input_mode(file_browse);

    if (ch == CTRL('f')) {
        restore_edit_screen(state);
        return true;
    }


    //パスインプットモード判定
    if(ch == 'i' && !input_mode){
        set_file_browse_path_input_mode(
            file_browse,
            true
            );
        file_browse->select_line.now_logical_line = 0;


        size_t now_open_path_len = strlen(file_browse->path_name);
        char tmp_now_open_path[now_open_path_len + 2];
        strcpy(tmp_now_open_path,file_browse->path_name);
        char joint_str[2] = {'/','\0'};
        strcat(tmp_now_open_path,joint_str);
        struct dir_table now_path = {.path_name = tmp_now_open_path};
        now_open_path_name(&now_path,set);
        load_dir_table(
            state,&file_browse->dir_name_table,
            &file_browse->dir_name_table_rows,
            file_browse->path_name,
            0,
            &file_browse->dir_num,
            &file_browse->dir_name_table_num
        );
        state->render_flags |= RENDER_FILE_BROWSE;

        return true;
    }

    if(input_mode == false){
        if(state->settings_data->file_select_scene_lighting){

            if(ch == '\t') {
                if(editor_get_screen_state_log(state,1) == start_menu_screen_mode){
                    editor_set_screen_state(state, start_menu_screen_mode);
                    *ctx->start_menu_screen.open = true;

                    set_file_browse_path_input_mode(
                        file_browse,
                        false
                    );
                }
                else{
                    editor_set_screen_state(state, edit_screen_mode);
                    restore_edit_screen(state);
                    set_file_browse_path_input_mode(file_browse,
                        false);
                }

                return true;
            }

            if(ch == KEY_MOUSE){
                handle_mouse(ctx,file_browse->dir_name_table_num);
            }


            //親ディレクトリに戻る
            if(ch == 'h'&& !input_mode){

                char *child_dir = NULL;
                if((child_dir = strrchr(file_browse->path_name,'/'))==NULL){
                    return 1;
                }
                if(strchr(file_browse->path_name,'/') == child_dir){
                    *(child_dir+1) = '\0';
                }
                else{
                    *(child_dir) = '\0';
                }
                

                file_browse->select_line.now_logical_line = 0;
                load_dir_table(state, &file_browse->dir_name_table,
                    &file_browse->dir_name_table_rows,
                    file_browse->path_name,
                    0,
                    &file_browse->dir_num,
                    &file_browse->dir_name_table_num);
                state->render_flags |= RENDER_FILE_BROWSE;
            }

        }
        if(ch == KEY_ENTER || ch == '\n' || ch == '\r' || ch == ' ' || (ch == 'l' && input_mode)) {
        // select_state.select_nameが空でなければディレクトリ選択、空ならファイル読み込み完了側を見る。
            struct file_browse_select_state select_state;
            load_file(state, file_browse->dir_name_table,
                    file_browse->dir_name_table_num,
                    file_browse->path_name, &select_state);
            
            if(select_state.select_name[0] != '\0' && select_state.select_state == folder){
                struct dir_table now_path = {
                    .path_name = file_browse->path_name,
                };
                now_open_path_name(&now_path,set);
                char now_path_name[PATH_MAX] = {0};
                const char *path_name = file_browse->path_name;
                const char *separator = strcmp(path_name, "/") == 0 ? "" : "/";
                int path_len = snprintf(now_path_name, sizeof(now_path_name), "%s%s%s",
                                        path_name, separator, select_state.select_name);
                if(path_len < 0 || (size_t)path_len >= sizeof(now_path_name)){
                    editor_error_screen(state, "path too long");
                    return true;
                }
                memcpy(file_browse->path_name, now_path_name, (size_t)path_len + 1);
                file_browse->select_line.now_logical_line = 0;
                load_dir_table(state, &file_browse->dir_name_table,
                    &file_browse->dir_name_table_rows,
                        file_browse->path_name,
                        0,
                        &file_browse->dir_num,
                        &file_browse->dir_name_table_num);

                
                state->render_flags |= RENDER_FILE_BROWSE;
                file_browse->path_input_mode = false;

            }
            if(state->file_data.now_open_file != NULL && select_state.select_state == file){
                load_screen_size(state);
                char uri[(PATH_MAX * 3) + sizeof("file://")];

                if(state->settings_data->lsp.lsp_use && ctx->lsp_data != NULL &&
                    ctx->lsp_data->initialized && ctx->lsp_data->to_server_fd >= 0 &&
                    lsp_path_to_file_uri(uri, sizeof(uri), state->file_data.now_open_path_name) == 0 &&
                    lsp_send_did_open(ctx->lsp_data->to_server_fd, uri,
                        ctx->lsp_data->lsp_language_id,
                            state->str.chr_file_all_str_data) == 0){

                    snprintf(ctx->lsp_data->update_data.path_name,
                            sizeof(ctx->lsp_data->update_data.path_name), "%s",
                            state->file_data.now_open_path_name);
                    ctx->lsp_data->update_data.file_update_counter = 1;
                }
                //ここでディレクトリ移動
                chdir(file_browse->path_name);
                // 読み込み直後は先頭行の行頭から編集を始める。
                editor_set_cursor(state, 0, 0);
                restore_edit_screen(state);
                set_syntax_data(&ctx->syntax_data,ctx);
                
            }
        }
    }
    else{
        const wchar_t *path = now_open_path_name(NULL,get);
        size_t path_len = wcslen(path);

        if(ch == KEY_BACKSPACE){
            
            wchar_t  tmp_path[path_len + 1];
            memcpy(tmp_path,path,sizeof(wchar_t) * (path_len+1));
            char mb_path[PATH_MAX];
            if(path_len > 0){
                tmp_path[path_len-1] = L'\0';
                size_t char_path_len = 0;
                if((char_path_len = wcstombs(mb_path, tmp_path, sizeof(mb_path))) != (size_t)-1){
                    mb_path[char_path_len] = '\0';
                    struct dir_table now_path = {.path_name = mb_path};
                    now_open_path_name(&now_path,set);
                    state->render_flags |= RENDER_FILE_BROWSE;
                }
            }
        }
        else if(input_result == OK && iswprint(ch)){
            wchar_t tmp_path[path_len + 2];
            memcpy(tmp_path,path,sizeof(wchar_t) * (path_len+1));
            tmp_path[path_len] = ch;
            tmp_path[path_len + 1] = L'\0';
            char mb_path[PATH_MAX];
            if(wcstombs(mb_path, tmp_path, sizeof(mb_path)) != (size_t)-1){
                struct dir_table now_path = {.path_name = mb_path};
                now_open_path_name(&now_path,set);
                state->render_flags |= RENDER_FILE_BROWSE;
            }
        }
        else if(ch == '\t' || ch == '\0' || ch == '\r' || ch == '\n' || ch == KEY_ENTER){
            
            const wchar_t *path = now_open_path_name(NULL,get);
            if(path == NULL)return true;
            char char_old_path[PATH_MAX];
            size_t converted = wcstombs(char_old_path,path,sizeof(char_old_path) - 1);
            if(converted == (size_t)-1){
                error_log("wcstombs argument error");
                char *err = strerror(errno);
                fputs(err,stderr);
                return true;
            }
            char_old_path[converted] = '\0';

            //現在入力されているパスが存在したら開く
            enum select_state path_state = get_path_state(char_old_path);
            if(path_state == file){
                now_input_path_open(state,ctx);
                return true;
            }
            

            int selected_line = file_browse->select_line.now_line;
            if(selected_line < 0 || selected_line >= file_browse->dir_name_table_num)return true;

            int candidate_rows = selected_line + 1;
            struct dir_table candidates[candidate_rows];
            int candidate_count = check_dir_mem(candidates,candidate_rows);
            //もし選択行よりメンバが少なかったらバグっているのでエラー画面を出す
            if(candidate_count <= selected_line){
                editor_error_screen(state,"directory error");
                return true;
            }
            struct dir_table *candidate = &candidates[selected_line];
            if(path == NULL){
                error_log("can not get open path name");
                return true;
            }
        
            char *last_slash = strrchr(char_old_path,'/');
            if(last_slash != NULL){
                *(last_slash+1) = '\0';
            }

            char char_path[PATH_MAX];
            const char *separator = (candidate->d_type == DT_DIR) ? "/" : "";
            int path_size = snprintf(char_path,sizeof(char_path),"%s%s%s",
                char_old_path,candidate->d_name,separator);

            editor_log(char_path,NULL,NULL, NULL,0);

            if(path_size >= 0 && (size_t)path_size < sizeof(char_path)){
                struct dir_table now_path = {
                    .path_name = char_path,
                };
                now_open_path_name(&now_path,set);
                if(candidate->d_type != DT_REG)
                    state->render_flags |= RENDER_FILE_BROWSE;
            }
            if(candidate->d_type == DT_REG){
                now_input_path_open(state,ctx);
                return true;
            }
        }


        if(ch == 'q'){
            return false;
        }

        //親ディレクトリ内の候補の更新
        if(ch == KEY_BACKSPACE || (input_result == OK && iswprint(ch)) || ch == '\n' || ch == '\t'){
            int table_rows = file_browse->dir_name_table_rows;
            struct dir_table table[table_rows];
            int men_num = check_dir_mem(table,table_rows);
            if(men_num > 0){
                file_browse->dir_num = men_num;
                file_browse->dir_name_table_num = men_num;
                file_browse->select_line.now_logical_line = 0;
                
                //入力文字列の更新時に、選択ラインを一番上に戻す
                if((file_browse->select_line.now_line < 0 ||
                    file_browse->select_line.now_line >= men_num)){
                    set_file_select_line(state,file_browse->dir_name_table_num,0);
                }

                for(int i = 0; i < table_rows;i++){
                    if(i < men_num){
                        snprintf(file_browse->dir_name_table[i].name,
                            sizeof(file_browse->dir_name_table[i].name),
                            "%s",table[i].d_name);
                        file_browse->dir_name_table[i].d_type = table[i].d_type;
                    }
                    else{
                        file_browse->dir_name_table[i].name[0] = '\0';
                        file_browse->dir_name_table[i].d_type = DT_UNKNOWN;
                    }
                }

                set_clear_box(&state->clear_box_data,file_browse->box);
                state->render_flags |= RENDER_FILE_BROWSE;
            }
        }
    }
    // now_lineは画面内の位置、now_logical_lineは全件テーブルの開始位置として別に動かす。
    if(ch == KEY_UP || (ch == 'k' && !input_mode)){
        int visible_num = file_browse->dir_num -
            file_browse->select_line.now_logical_line;
        if(visible_num > file_browse->area.h)visible_num = file_browse->area.h;
        int next_line = file_browse->select_line.now_line;
        if(next_line > 0){
            next_line--;
        }
        else if(file_browse->select_line.now_logical_line > 0){
            file_browse->select_line.now_logical_line--;
        }
        set_file_select_line(state, visible_num, next_line);
        state->render_flags |= RENDER_FILE_BROWSE;

    } else if(ch == KEY_DOWN || (ch == 'j' && !input_mode)){
        int visible_num = file_browse->dir_num -
            file_browse->select_line.now_logical_line;
        if(visible_num > file_browse->area.h)visible_num = file_browse->area.h;
        int next_line = file_browse->select_line.now_line;
        if(next_line + 1 < visible_num){
            next_line++;
        }
        else if(file_browse->select_line.now_logical_line + visible_num <
                file_browse->dir_num){
            file_browse->select_line.now_logical_line++;
        }
        set_file_select_line(state, visible_num, next_line);
        state->render_flags |= RENDER_FILE_BROWSE;
    }

    return true;
}

// ファイルブラウザ上部に現在ディレクトリのパスを表示する。
// 引数: file_browse_box=表示位置と幅、path_name=表示するパス文字列。 返り値: なし。
void draw_now_path_name(struct box file_browse_box,char *path_name){
    int x = file_browse_box.pos.x;
    int y = file_browse_box.pos.y;
    int w = file_browse_box.w;
    if(w <= 1 || y < 2){
        return;
    }

    mvaddch(y - 2, x, ACS_ULCORNER);
    for (int i = 1; i < w - 1; i++)
        mvaddch(y - 2, x + i, ACS_HLINE);
    mvaddch(y - 2, x + w - 1, ACS_URCORNER);

    mvaddch(y - 1, x, ACS_VLINE);
    int inner_w = w - 2;
    int len = (int)strlen(path_name);
    if (len > inner_w && inner_w > 3) {
        // 長いパスは先頭を"..."に置き換え、末尾側を残す(ファイル名側のほうが重要なため)。
        // 残す先頭はなるべく'/'で区切り、ディレクトリ名が途中で切れないようにする。
        addstr("...");
        int diff = len - inner_w+strlen("...");
        char *path_start_ptr = strchr(&path_name[diff],'/');
        if(path_start_ptr == NULL){
            path_start_ptr = &path_name[diff];
        }
        mvaddnstr(y - 1, x +4,path_start_ptr, inner_w - 3);

    } else if(len > inner_w) {
        mvaddnstr(y - 1, x + 1, path_name, inner_w);
    } else {
        mvprintw(y - 1, x + 1, "%-*s", inner_w, path_name);
    }
    mvaddch(y - 1, x + w - 1, ACS_VLINE);
    mvaddch(y, x, ACS_LTEE);
    mvaddch(y, x + w - 1, ACS_RTEE);
}

// load_dir_table()が作ったディレクトリエントリ一覧をファイルブラウザの内側へ描画する。
// 引数: state=ファイルブラウザ領域、table=名前と種別を持つディレクトリエントリ一覧。 返り値: なし。
void draw_box_inside_dir(struct editor_state *state,struct dir_entry *table){
    
    if(table == NULL || state->file_browse.area.w <= 0 || state->file_browse.area.h <= 0){return;}
    char clear[state->file_browse.area.w + 1];
    memset(clear,' ',state->file_browse.area.w * sizeof(char));
    clear[state->file_browse.area.w] = '\0';

    // エントリ名は幅に関係なく丸ごと保持しているため、はみ出す分はここで詰める。
    // 描き始めがpos.x+1なので、使える幅は内側幅から1引いた分。
    int max_len = state->file_browse.area.w - 3;

    // パス入力中のテーブルはcheck_dir_mem()が入力パスの親から作るので、同じディレクトリを前に付ける。
    const char *base_path = state->file_browse.path_name;
    char input_dir[PATH_MAX];
    if(get_file_browse_path_input_mode(&state->file_browse)){
        struct dir_table now_path = {0};
        now_open_path_name(&now_path,get);
        char *last_slash = (now_path.path_name != NULL) ? strrchr(now_path.path_name,'/') : NULL;
        if(last_slash != NULL){
            size_t dir_len = (last_slash == now_path.path_name) ? 1 : (size_t)(last_slash - now_path.path_name);
            memcpy(input_dir,now_path.path_name,dir_len);
            input_dir[dir_len] = '\0';
            base_path = input_dir;
        }
    }

    // now_logical_line以降の全件テーブルを、i番目の画面行へ対応付ける。
    for(int i = 0;i < state->file_browse.area.h;i++){
        mvaddstr(state->file_browse.area.pos.y + i, state->file_browse.area.pos.x,clear);
        if(max_len <= 0) continue;

        struct dir_entry *entry = &table[state->file_browse.select_line.now_logical_line + i];
        if(entry->name[0] == '\0') continue;

        int draw_y = state->file_browse.area.pos.y + i;
        int draw_x = state->file_browse.area.pos.x + 3;
        int len = (int)strlen(entry->name);

        char entry_path[sizeof(state->file_browse.path_name) + sizeof(entry->name) + 1];
        snprintf(entry_path,sizeof(entry_path),"%s%s%s",base_path,
            strcmp(base_path,"/") == 0 ? "" : "/",entry->name);
        editor_log(entry_path, NULL, NULL, NULL, 0);
        const char *icon_code =
            get_file_ext_code(entry_path,&state->settings_data->icon_data);
      
        mvaddstr(draw_y,draw_x - 2,icon_code);
        if(len <= max_len){
            mvaddstr(draw_y, draw_x, entry->name);
        }
        else if(max_len > 3){
            // 末尾を"..."にして省略したことが分かるようにする。
            mvaddnstr(draw_y, draw_x, entry->name, max_len - 3);
            mvaddstr(draw_y, draw_x + max_len - 3, "...");
        }
        else{
            mvaddnstr(draw_y, draw_x, entry->name, max_len);
        }
    }
}

// stateのブラウザ選択行へ色ペアnumを適用し、前の選択行を通常色へ戻す。dir_numは表示項目数。
// 返り値: なし。強調無効・幅不足・件数0以下・選択範囲外なら描かず、描画後は端末カーソルを復元する。
void draw_select_dir_scene_color(struct editor_state *state,int dir_num,int num){
    int cur_x;
    int cur_y;
    getyx(stdscr,cur_y,cur_x);
    if(state->settings_data->file_select_scene_lighting == false)
        return;

    if(state->file_browse.area.w <= 0 || dir_num <= 0 ||
       state->file_browse.select_line.now_line < 0 || state->file_browse.select_line.now_line >= dir_num){
        return;
    }
    int lighting_line = state->file_browse.area.pos.y + state->file_browse.select_line.now_line;
    mvchgat(lighting_line,state->file_browse.area.pos.x,state->file_browse.area.w,A_NORMAL,num,NULL);

    if(state->file_browse.select_line.previous_line != state->file_browse.select_line.now_line){
        int previous_line = state->file_browse.area.pos.y + state->file_browse.select_line.previous_line;
        // 選択が移った前の行を通常の本文色へ戻す。
        mvchgat(previous_line,state->file_browse.area.pos.x,state->file_browse.area.w,A_NORMAL,get_color_pair(state->input_ctx,COLOR_WHITE,COLOR_BLACK),NULL);
    }
    move(cur_y,cur_x);
}

// ファイルブラウザ全体の再描画を要求する。
// 引数: state=描画要求の保存先。枠やパスはstate->file_browseから参照する。 返り値: なし。
void show_file_browse(struct editor_state *state){
    state->render_flags |= RENDER_FILE_BROWSE;
}

// stateのブラウザ選択行をlineへ更新し、dir_num件の範囲へ丸めて強調表示を再描画要求する。
// 返り値: なし。dir_numが0以下、または強調表示が無効なら変更しない。
void set_file_select_line(struct editor_state *state,int dir_num,int line){
    if(dir_num <= 0 || state->settings_data->file_select_scene_lighting == false){
        return;
    }
    if(line < 0){
        line = 0;
    }
    if(line >= dir_num){
        line = dir_num - 1;
    }
    
    file_select_line_update(&state->file_browse.select_line, line);
    state->render_flags |= RENDER_SELECT_DIR_SCENE_COLOR;
}

// ファイルブラウザのパス入力枠を描画する。
// 引数: search_box=枠線を含む描画領域、win=描画先ウィンドウ。 返り値: なし。winがNULLなら何もしない。
static void draw_search_box(struct box search_box,WINDOW *win){
    if(win == NULL)return;
    draw_box(search_box,win);
    mvaddch(search_box.pos.y,search_box.pos.x,ACS_LTEE);
    mvaddch(search_box.pos.y,search_box.pos.x + search_box.w -1,ACS_RTEE);
}

// ファイルブラウザの枠・パス・一覧・選択行と、パス入力中はサーチボックスを描画する。
// 引数: ctx=状態と描画先を持つ入力context。 返り値: なし。
void draw_file_browse(struct editor_input_context *ctx){
    struct editor_state *state = ctx->state;
    WINDOW *win = ctx->win;
    //ブラウザ画面を後ろのコードが見えないように消す
    set_clear_box(&state->clear_box_data,state->file_browse.box);
    clear_box(&state->clear_box_data);
    draw_box(state->file_browse.box, win);
    draw_now_path_name(state->file_browse.box,
                       state->file_browse.path_name);
    draw_box_inside_dir(state, state->file_browse.dir_name_table);
    draw_select_dir_scene_color(state,state->file_browse.dir_name_table_num,get_color_pair(ctx,COLOR_BLACK,COLOR_WHITE));

    //サーチボックスの描画とサーチボックス内のパス描画
    if(get_file_browse_path_input_mode(&state->file_browse)){
        my_cur_set(state,true);

        struct box search_box = state->file_browse.search_box;
        draw_search_box(search_box,ctx->win);

        int cur_line = search_box.pos.y + 1;
        int col = search_box.pos.x + 1;

        const wchar_t *path = now_open_path_name(NULL,get);
        //マウスカーソル分を確保するため両サイド合わせて-3する
        int show_path_size = search_box.w - 3;
        // 入力欄に収まらない長いパスは、入力中の末尾が見えるよう後ろから表示幅分だけ残す。
        // 全角はセル幅2なので文字数ではなくwcwidthの合計で数える。
        size_t path_start = wcslen(path);
        int path_width = 0;
        while(path_start > 0){
            int char_width = wcwidth(path[path_start - 1]);
            if(char_width < 0 || path_width + char_width > show_path_size)break;
            path_width += char_width;
            path_start--;
        }

        mvhline(cur_line,col,' ',show_path_size);
        mvaddwstr(cur_line,col,&path[path_start]);
        int x;
        int y;
        getyx(ctx->win,y,x);
        cur_pos_push((struct pos){x,y},state);
    }
}
