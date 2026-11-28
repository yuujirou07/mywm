#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/epoll.h>


#include "c_settings.h"
#include "ftj.h"
#include "editor_screen.h"
#include"editor_log.h"
#include"path_util.h"
#include"editor_color.h"
#include"lsp_settings.h"

static void end_process(struct editor_state *state);


// argc/argvの起動指定を読み、ncursesと編集状態を初期化して入力・描画ループを実行する。
// 返り値: 正常終了0、初期化・確保失敗1。終了時に保持資源を解放する。
int main(int argc, char *argv[])
{


    char startuptime_log_file_path_name[PATH_MAX] = {0};
    

    struct timespec startup_start_time;
    clock_gettime(CLOCK_MONOTONIC, &startup_start_time);
    bool startup_timer = 0;


    //エラーログ履歴ファイルのパス設定(my_txt_editor/直下からの相対パス)
    //エラーログが作れない場合終了する
    if(set_error_log_file(EDITOR_LOG_DIR_ EDITOR_ERROR_LOG_FILE_NAME_) < 0){
        wrp_stderr_puts_("can not set error log file");
        return EXIT_SUCCESS;
    }
    if(set_log_file(EDITOR_LOG_DATA_) != 0){
        return EXIT_SUCCESS;
    }
    
    bool mylsp = false;
    //引数解析
    if(argc > startuptime_log_file_argument_num_){
        int result = strcmp(argv[startuptime_log_file_argument_num_],"sttime");

        if(result == 0){
            if(editor_path_from_exe_dir(startuptime_log_file_path_name,sizeof(startuptime_log_file_path_name),
                        EDITOR_LOG_DIR_ "startuptime_log.log") != NULL){
                startup_timer = 1;
            }
            else{
                error_log_write("path to long");
            }
        }
        else if(strcmp(argv[startuptime_log_file_argument_num_],"mylsp") == 0){
            mylsp = true;
        }

    }

    struct editor_settings settings_data = {0};
    struct editor_state state      = {0};
    struct ascii_data   ascii_data = {0};

    state.settings_data = &settings_data;
    state.mylsp_use     = mylsp;

    struct editor_input_context input_context;
    struct box status_bar;
    WINDOW *win = NULL;

    // 設定読み込みがctx->stateを参照するため、呼び出し前に借用先を設定する。
    if(init_input_context(&input_context,&state) != 0){
        return 1;
    }

   //カスタム設定のバグになる値を検出する関数の設置
    init_settings_data(&state);
    
    setlocale(LC_ALL, "");
    // 危険: initscr()以降の失敗returnでendwin()を呼ばない経路が複数ある。
    // その経路では端末がraw/noecho相当の状態に残る可能性がある。
    win = initscr();


    cbreak();
    noecho();
    nodelay(stdscr,false);
    keypad(win, TRUE); 
    my_cur_set(&state,false);

    //cursesカーラー初期化
    init_curses_color(&state);


    // 固定の色ペアはinit_curses_colorが登録済み。構文色はPAIR_SYNTAX_BASE以降へ登録する。
    if(state.settings_data->built_in_syntax){
        init_syntax_colors();
    }
    

    state.scr.scr_start_num = 0;

    // 編集カーソルはここが唯一の初期化場所。以降、位置はstate.cursorだけが持ち、
    // 端末側へはeditor_sync_cursor()で反映する。
    state.cursor.file_pos = (struct pos){0,0};

    getmaxyx(win, state.scr.scr_size.y, state.scr.scr_size.x);
    clear();

    int line_cap = state.settings_data->default_load_line_size;
    // ファイルを開くまでは各行へ画面幅分の容量を割り当てておく。
    // 足りなくなった行はeditor_ensure_line_cap()が伸ばす。
    int default_line_cols = (state.scr.scr_size.x > 0)
        ? state.scr.scr_size.x : EDITOR_LINE_COL_SLACK_;
    if(line_cap < 1 || default_line_cols > LONG_MAX / line_cap){
        printf("line buffer size is too large");
        return 1;
    }
    if(!editor_alloc_text_buffer(&state, line_cap, (long)line_cap * default_line_cols)){
        printf("state.str.wint_line_str_data calloc error");
        return 1;
    }
    for(int i = 0; i < line_cap; i++){
        state.str.line_offset[i] = (long)i * default_line_cols;
        state.str.line_cap[i]    = default_line_cols;
    }
    state.str.chr_file_all_str_data = NULL;


    state.str.input_data_list.input_data_allocate_num = 64;
    state.str.input_data_list.input_data_num = 0;
    state.str.input_data_list.input_data = NULL;
    



    input_context.ask_make_file_mode.screen_center_y =  state.scr.scr_size.y / 2;

    state.file_browse.box.w = state.scr.scr_size.x / 3;
    state.file_browse.box.h = input_context.ask_make_file_mode.screen_center_y;
    state.file_browse.box.pos.x = (state.scr.scr_size.x / 2) - state.file_browse.box.w / 2;
    state.file_browse.box.pos.y = state.scr.scr_size.y / 4;

    ft_init_file_tree(&state.file_tree_data);

    state.edit_input_complete_data.box.pos = (struct pos){0,0};
    state.edit_input_complete_data.box.h = 
        settings_data.auto_complete_settings_data.auto_complete_window_size.y;
        
    state.edit_input_complete_data.box.w = 
        settings_data.auto_complete_settings_data.auto_complete_window_size.x;

    
    my_cur_set(&state,true);
    raw();
    scrollok(win, TRUE);
    mouseinterval(0); // クリックの判定待ちを最小にして、ダブルクリックを待たず即反応させる。
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);  
    printf("\033[?1002h");
    fflush(stdout);
    

    // ファイルブラウザ初期ディレクトリの絶対パスを取得する。
    if(getcwd(state.file_browse.path_name, sizeof(state.file_browse.path_name)) == NULL) {
        perror("getcwd");
        return 1;
    }


    struct dir_table now_path = {.path_name = state.file_browse.path_name};
    if(now_open_path_name(&now_path,set) == NULL)return 0;
    state.file_data.description_line_end = 0;
    state.write_area.x_start = state.settings_data->line_number_space + 1;
    state.write_area.y_start = 0;
    state.write_area.x_end   = state.scr.scr_size.x-1;
    state.write_area.y_end   = state.scr.scr_size.y;
    if(state.settings_data->show_status_bar){
        status_bar.w = state.scr.scr_size.x;
        status_bar.h = 1;
        if(state.settings_data->bar_side_state == top){
            status_bar.pos = (struct pos){0, 1};
            state.write_area.y_start = status_bar.pos.y + status_bar.h;
        }
        else{
            status_bar.pos = (struct pos){0, state.scr.scr_size.y - 1};
            state.write_area.y_end = status_bar.pos.y;
        }
    }
    state.status_bar   = &status_bar;
    editor_apply_write_area(&state);
    state.write_area.h = state.write_area.y_end - state.write_area.y_start;
    state.file_data.file_str_line_end = 0;

    state.file_browse.area.pos.x = state.file_browse.box.pos.x + 1;
    state.file_browse.area.pos.y = state.file_browse.box.pos.y + 1;
    state.file_browse.area.h = state.file_browse.box.h - 2; // 上下の枠線を除く。
    state.file_browse.area.w = state.file_browse.box.w - 2; // 左右の枠線を除く。

    state.make_file_mode_status.is_input_scene        = false;
    state.make_file_mode_status.new_file_name_counter = 0;

    snprintf(state.settings_data->lsp.lsp_language,
         sizeof(state.settings_data->lsp.lsp_language),
         "%s", "c");

    memset(&state.write_file_name_area,0,sizeof(struct box));

    state.file_browse.select_line.now_line         = 0;
    state.file_browse.select_line.previous_line    = 0;
    state.file_browse.select_line.now_logical_line = 0;
    state.file_data.now_open_file  = NULL;
    state.file_data.is_open_file   = 0;
    state.file_data.file_line_start_num_counter = 0;
    state.file_data.file_line_start_num = calloc(state.settings_data->default_load_line_size, sizeof(long));
    

    if(state.file_data.file_line_start_num == NULL){
        editor_free_text_buffer(&state);
        return 1;
    }
    state.settings_screen_data.box = (struct box){(struct pos){0,0},0,0};
    state.settings_screen_data.item_data = NULL;
    state.settings_screen_data.settings_item_data_num = 0;
    state.settings_screen_data.settings_item_data_allocate_num = 0;
    state.settings_screen_data.select_line = 0;
    if(load_settings_screen_items(&state.settings_screen_data) < 0){
        error_log_write("settings item load error\n");
        end_process(&state);
        return 1;
    }
    state.jump_mode_data.jump_line_num_counter = 0;
    


    editor_set_screen_state(&state, state.settings_data->show_start_menu ? start_menu_screen_mode : edit_screen_mode);
    // 一覧テーブルは各エントリの名前と種別を保持する。
    // 行数は表示できる件数分だけ確保し、幅の変化では作り直さない。
    state.file_browse.dir_name_table_rows = (state.file_browse.area.h > 0)
        ? state.file_browse.area.h : 1;
    state.file_browse.dir_name_table =
        calloc((size_t)state.file_browse.dir_name_table_rows,sizeof(*state.file_browse.dir_name_table));
    int allocate_total_str_size     = state.settings_data->load_buffer_lines;
    state.file_data.file_str_data   = calloc(allocate_total_str_size,sizeof(char*));
    if(state.file_browse.dir_name_table == NULL || state.file_data.file_str_data == NULL){
        free(state.file_browse.dir_name_table);
        free(state.file_data.file_str_data);
        free(state.file_data.file_line_start_num);
        editor_free_text_buffer(&state);
        return 1;
    }
    state.file_browse.dir_num = 0;
    state.file_browse.dir_name_table_num = 0;
    load_dir_table(&state,&state.file_browse.dir_name_table,&state.file_browse.dir_name_table_rows,state.file_browse.path_name,0,
                   &state.file_browse.dir_num,&state.file_browse.dir_name_table_num);

    input_context.ask_make_file_mode.screen_center_pos.x = state.scr.scr_size.x / 2;
    input_context.ask_make_file_mode.screen_center_pos.y = input_context.ask_make_file_mode.screen_center_y;
    
    bkgd(COLOR_PAIR(PAIR_TEXT_));
    editor_sync_cursor(&state);
    set_cur_pos(&state);
    refresh();


    void *handle = NULL;
    
    //スタートメニュー表示判定
    Start_Menu start_menu = NULL;
    bool open_start_menu = state.settings_data->show_start_menu;
    // スタートメニューは共有ライブラリ(プラグイン)として実行時に読み込む。
    if(state.settings_data->show_start_menu){
        char plugin_path[PATH_MAX];
        if(editor_config_path(plugin_path, sizeof(plugin_path), START_MENU_SO_PATH_) != NULL){
            handle = dlopen(plugin_path, RTLD_NOW);
        }
        //スタートメニューsoファイルが見つからないならエラーを出す
        if(handle == NULL){
            editor_error_screen(&state,"can not load start menu data");
            error_log_write("sry can not open so file :(\n"); 
        }
        else{
            start_menu = (Start_Menu)dlsym(handle,"draw_start_menu");
            if(start_menu == NULL){
                error_log_write("dlsym failed\n");
                dlclose(handle);
                end_process(&state);
                return 1;
            }
        }
    }
    
    input_context.win = win;

    // 起動時にLSPを起動する設定なら、カレントディレクトリをルートとして起動する。
    if(is_lsp_enabled(&state) && state.settings_data->lsp.lsp_launch_startup_editor){
        struct lsp_process *lsp = lsp_run(state.file_browse.path_name,state.settings_data->lsp.lsp_language);
        if(lsp != NULL && lsp_server_data_add(&input_context.lsp_server_data,lsp) != 0){
            lsp_close_server(lsp);
            free(lsp->lsp_root_uri);
            free(lsp);
        }
    }
    
    editor_set_env_lang(&input_context.syntax_data, input_context.state,C);
    input_context.edit_screen.line_start_pos.x = state.write_area.x_start - 1;
    input_context.edit_screen.line_start_pos.y = state.write_area.y_start;
    input_context.edit_screen.line_end_pos.x = state.write_area.x_start - 1;
    input_context.edit_screen.line_end_pos.y = state.write_area.y_end;

    state.file_browse.search_box.pos.x = state.file_browse.box.pos.x;
    state.file_browse.search_box.pos.y = state.file_browse.box.pos.y + state.file_browse.box.h - 1;
    state.file_browse.search_box.w = state.file_browse.box.w;
    state.file_browse.search_box.h = 3;
    state.file_browse.path_input_mode = false;

    state.cursor.CurMode = CURSOR_INPUT_MODE;
    state.cursor.Color   = CURSOR_COLOR_WHITE;

    input_context.start_menu_screen.open = &open_start_menu;
    input_context.start_menu_screen.has_plugin = (start_menu != NULL);
    input_context.start_menu_screen.plugin = start_menu;
    input_context.start_menu_screen.ascii_data = &ascii_data;
    input_context.start_menu_screen.startup_start_time = startup_timer ? &startup_start_time : NULL;
    input_context.start_menu_screen.startup_log_path = startup_timer ? startuptime_log_file_path_name : NULL;

    input_context.state->key_board_data.key_log_data.key_allocate_num = 0;
    input_context.state->key_board_data.key_log_data.key_count  = 0;
    input_context.state->key_board_data.key_log_data.key_log  = NULL;

    if(init_command_mode(&state) != 0){
        error_log_("command mode init error");
        wrp_stderr_puts_("command mode init error");
        return 0;
    }

    KL_open_key_log_file(KEY_EVENT_LOG_PATH_);
    

    //設定プログラムファイルロード　
    if(state.settings_data->editor_settings_program.enabled){
        load_settings_program(
            &input_context,
            state.settings_data->editor_settings_program.root_src_path
        );
    }
    
    
    int load_icon_rt = 
        load_icon_data(
            ICON_SETTINGS_PATH_,
            &settings_data.icon_data
        );

    if(load_icon_rt != 0){
        error_log_("can not load icon data");
    }


    char *now_dir = getcwd(NULL,0);
    get_root_file_tree_data(&state.file_tree_data,now_dir);
    free(now_dir);

    // メインループ: LSP受信 → 補完枠の表示判定 → 描画 → 構文色の重ね塗り → キー入力待ち。
    int running = true;
    
    wint_t ch = 0;
    int input_result = 0;
        
    while (running) {
        
        //もしlspを使用する設定だったら
        if(is_lsp_enabled(&state) && is_lsp_running(&input_context)){
            lsp_poll_all(&input_context.lsp_server_data,settings_data.lsp.lsp_epoll_timeout_ms);
        }

        //スタートメニュー実行判定
        if(open_start_menu && start_menu != NULL){
            editor_set_screen_state(&state, start_menu_screen_mode);
            running = editor_handle_screen_input(&input_context, OK, 0);
            if(running == false)break;
            continue;
        }
        
        //補完候補があるか確認し、あれば補完ウィンドウを有効にする
        //補完機能が有効か
        if(state.settings_data->auto_complete_settings_data.auto_complete_enabled){
            //補完ウィンドウの表示フラグがたっているか
            if(state.edit_input_complete_data.show &&
                editor_get_screen_state(&state) == edit_screen_mode){
                //補完ウィンドウ表示フラグがたっているのに補完候補が0ならバグとして認識し、補完ウィンドウ表示フラグをfalseに戻す
                if(state.edit_input_complete_data.
                    comp_world_candidacy_part_data.
                        comp_w_cand_part_d_count <= 0){
                    state.edit_input_complete_data.show = false;
                }
                else{
                    state.render_flags |= RENDER_EDIT_COMPLETE_WINDOW;
                }
            }
        }
    
        //デバッグ用
        set_complete_str(&state,state.edit_input_complete_data.
                comp_world_candidacy_part_data.complete_world_candidacy_part_str,1);

        // refresh中に端末カーソルが途中位置で点滅しないよう、一度隠してから戻す。
        bool is_cur_hide = false;
        if(input_context.state->cursor.is_cur_show){
            my_cur_set(&state,false);
            is_cur_hide = true;
            refresh();
        }
    
        //画面更新元(この関数外では画面を更新しない)
        update_screen(&input_context);
        
        if(editor_get_screen_state(&state) == edit_screen_mode){ 
            if(state.settings_data->built_in_syntax){
                apply_syntax_color(&state,input_context.syntax_data);
            }
            editor_sync_cursor(&state);
            set_cur_pos(&state);
        }

        refresh();


        if(is_cur_hide){
            my_cur_set(&state,true);
        }


        input_result = get_wch(&ch);
        
        //キーログスタックに追加する
        if(state.settings_data->key_log_settings.use_key_log){
            KL_add(
                &state,
                ch,
                input_result,
                editor_get_screen_state(&state)
            );
        }

        //ユーザイベント確認
        if(state.settings_data->editor_settings_program.enabled){
            check_user_settings_event(&input_context);
        }
        
        if (input_result == ERR)continue;

        //リサイズイベント
        if (input_result == KEY_CODE_YES && ch == KEY_RESIZE) {
            handle_resize(win, &input_context);
            // 画面寸法が変わると表示範囲が変わるため、構文情報を全再計算する。
            set_syntax_data(&input_context.syntax_data,&state);
            continue;
        }

        running = editor_handle_screen_input(&input_context, input_result, ch);
        continue;
    }
    // 終了処理。確保した順の逆に解放し、最後にend_processでncursesを終了する。
    // resize_file_browser()がreallocした場合、最新のポインタはstate側にある。
    free(state.file_browse.dir_name_table);
    free(input_context.syntax_data.syntax_list_data.syntax_data);
    if(state.file_tree_data.root_node != NULL){
        close_tree(state.file_tree_data.root_node);
        state.file_tree_data.root_node = NULL;
    }


    if(state.file_tree_data.item_view_data != NULL){
        free(state.file_tree_data.item_view_data);
        state.file_tree_data.item_view_data = NULL;
    }
    if(state.edit_input_complete_data.word_data.world_allocate_num > 0){
        for(int i = 0;i < state.edit_input_complete_data.word_data.world_allocate_num;i++){
            free(state.edit_input_complete_data.word_data.world[i]);
        }
    }

    if(state.edit_input_complete_data.word_data.world != NULL){
        free(state.edit_input_complete_data.word_data.world);
        state.edit_input_complete_data.word_data.world_allocate_num = 0;
        state.edit_input_complete_data.word_data.world_num = 0;
    }
    if(handle != NULL){
        dlclose(handle);
    }


    if(state.settings_data->editor_settings_program.root_src_path != NULL){
        free(state.settings_data->editor_settings_program.root_src_path);
    }

    key_map_list_destroy(&input_context.input_manager.key_maps);
    destroy_dl_path(&input_context);
    destroy_key_input_event_handle(&input_context);
    destory_command_mode(&state);

    lsp_server_data_destroy(&input_context.lsp_server_data);
    end_process(&state);

    fputs(CURSOR_SHAPE[CURSOR_DEFAULT_MODE],stdout);
    fflush(stdout);


    return 0;
}


// stateの行・本文・設定項目・アイコン用メモリとログを解放し、端末表示を終了する。
// 返り値: なし。stateとその参照先は初期化済みであること。
static void end_process(struct editor_state *state){
    free(state->file_data.file_str_data);
    free(state->file_data.file_line_start_num);
    free(state->str.chr_file_all_str_data);

    //キーログ解放
    if(state->settings_data->key_log_settings.use_key_log){
        if(state->key_board_data.key_log_data.key_allocate_num > 0){
            free(state->key_board_data.key_log_data.key_log);
        }
    }
    
    for(int i = 0;i < state->settings_screen_data.settings_item_data_num;i++){
        free((char *)state->settings_screen_data.item_data[i].name);
        free((char *)state->settings_screen_data.item_data[i].explanation);
    }
    free(state->settings_screen_data.item_data);
    destroy_icon_data(&state->settings_data->icon_data);
    editor_free_text_buffer(state);

    close_error_log_file();
    clear();
    refresh();
    endwin();
}

// 指定した画面座標へNUL終端文字列を描画する。
// 引数: pos=描画開始座標、str=描画する文字列。 返り値: なし。ncursesの描画失敗は呼び出し元へ通知しない。
void my_mvaddstr(struct pos pos,char * str){
    mvaddstr(pos.y,pos.x,str);
}

