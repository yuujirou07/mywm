#include <ncurses.h>
#include <stdlib.h>
#include <math.h>
#include <wctype.h>

#include "editor.h"
#include "editor_log.h"
#include "filetree.h"
#include "ftj.h"
#include "editor_screen.h"
#include "editor_color.h"

static struct pos ft_get_item_name_pos(struct box ft_box,int file_h,int layer);

// tree_tableの子の名前を、filetree_box左上にlayer列・pos_y行を加えた位置へ順に描く。
// 返り値: 常に0。現実装は各項目の座標を進めず、同じ場所へ重ねて描く。
int set_tree_item(struct box filetree_box,int layer,int pos_y,struct table *tree_table){
    int x = filetree_box.pos.x + layer;
    int y = filetree_box.pos.y + pos_y;

    for(int i = 0; i < tree_table->c_table_num;i++){
        mvaddstr(y,x,tree_table->c_table[i].s_data.d_name);
    }
    return 0;
}


// filetree_dataのtable_num件のtableに表示座標と階層を再帰的に割り当てる(描画はしない)。layerは深さ、*file_hは次の相対行。
// 返り値: なし。*file_h・各項目の画面座標・drawing_item_numを更新し、枠の下端・項目情報の確保失敗で打ち切る。
static void ft_update_filetree_data(struct editor_input_context *ctx,
                                file_tree_data *filetree_data,
                                struct table *table,int table_num,
                                int layer,int *file_h){
                                    
    struct box box = filetree_data->ft_item_box;

    for(int i = 0;i < table_num;i++){
        // スクロール分ずらした枠内の相対行
        int view_h = *file_h - filetree_data->scroll_y;
        //ファイルツリー外描画ガード
        if(view_h >= box.h - 1)return;
        //現在のレイヤーのi番目のテーブルのデータを取得する
        ft_item_view_data *item_data =
            get_filetree_item_data(filetree_data,&table[i]);
        if(item_data == NULL)return;

        item_data->layer = layer;
        // 上へスクロールアウトした項目はscreen_y=-1のまま残す
        if(view_h >= 1){
            // i番目の要素の名前を描く画面座標
            struct pos item_name_pos = ft_get_item_name_pos(box,view_h,layer);
            //取得した名前の座標をitemに代入する
            item_data->screen_y = item_name_pos.y;
            item_data->screen_x = item_name_pos.x;
        }   
        (*file_h)++;
        //iの要素がディレクトリかつ、is_openで展開判定になっていたら
        //i番目のテーブルにそのディレクトリ内の要素のデータをつける
        if(table[i].s_data.d_type == DT_DIR && item_data->is_open){
            if(load_child_dir(&table[i]) == 0){
                ft_update_filetree_data(ctx,filetree_data,table[i].c_table,
                                    table[i].c_table_num,layer + 1,file_h);
            }
        }
    }
}


// filetree_dataの枠内を消して境界線を描き、項目の画面座標を振り直してからft_draw_filetree_dataで項目を描く。
// 返り値: 常に0。枠が小さい、またはルート未設定なら描画しない。
int draw_filetree(struct editor_input_context *ctx,file_tree_data *filetree_data){
    struct box box = filetree_data->ft_box;

    if(box.w <= 2 || box.h <= 2 || filetree_data->root_node == NULL){
        return 0;
    }

    //前に表示されていたツリーをクリアする
    clear_box_area(box);


    //開発用
    filetree_data->ft_side = LEFT;

    //ファイルツリーの位置が左右だった場合位置におおじて枠線を設定する
    // (ファイルツリーが左側にあった場合右に線を設定する)
    if(filetree_data->ft_side == LEFT || filetree_data->ft_side == RIGHT){
        int wall_pos_x = (filetree_data->ft_side == LEFT)
            ?(box.pos.x + box.w):box.pos.x;  

        for(int i = box.pos.y;i < box.pos.y + box.h;i++){
            mvaddch(i,wall_pos_x,ACS_VLINE);
        }
    }
    
    // 画面行は描画のたびに振り直す。-1のまま残った項目は画面外で、クリック判定に当たらない。
    for(int i = 0;i < filetree_data->item_view_count_num;i++){
        filetree_data->item_view_data[i].screen_y = -1;
        filetree_data->item_view_data[i].screen_x = -1;
    }

    int file_h = 1;
    ft_update_filetree_data(
        ctx,filetree_data,
        filetree_data->root_node->c_table,
        filetree_data->root_node->c_table_num,
        0,
        &file_h
    );

    ft_draw_filetree_data(ctx,filetree_data);
    
    return 0;
}


// ft_boxの枠内で、相対行file_h・階層layerの項目名を描く画面座標を求める(階層ごとに2列字下げ)。
// 返り値: 画面上の座標。範囲検査はしない。
static struct pos ft_get_item_name_pos(struct box ft_box,int file_h,int layer){
    struct pos tmp_pos;
    tmp_pos.x = ft_box.pos.x + 1 + (layer * 2);
    tmp_pos.y = ft_box.pos.y + file_h;
    return tmp_pos;
}


// 項目1件を、記録済みのscreen_y/screen_xへアイコン(ディレクトリは開閉、ファイルは拡張子別)と名前の順に描く。
// 引数: ctx=アイコン設定を持つcontext、ft_box=名前を切り詰める基準の枠、view_data=描画する項目(screen_y/xは設定済み)。 返り値: なし。名前の描画幅が残っていなければ名前は描かない。
static void ft_draw_item_data(struct editor_input_context *ctx,struct box ft_box,
                         ft_item_view_data *view_data){

    const char *item_name = view_data->table_ptr->s_data.d_name;
    const char *icon;

    //ディレクトリは開閉状態、それ以外は拡張子に応じたアイコンを使う
    if(view_data->table_ptr->s_data.d_type == DT_DIR){
        icon = view_data->is_open ? FILE_TREE_ICON_OPEN : FILE_TREE_ICON_CLOSE;
    }
    else{
        icon = get_file_ext_code(item_name,&ctx->state->settings_data->icon_data);
    }

    //アイコンの使用セル数
    const int item_icon_len = str_display_width(icon);
    mvaddstr(view_data->screen_y,view_data->screen_x,icon);

    //アイテム名の描画文字数制限。0以下はmvaddnstrで「全文字」の意味になるため描かない。
    int name_len_max = (ft_box.w - 1) - (view_data->screen_x + 1 + item_icon_len);
    if(name_len_max <= 0)return;
    
    mvaddnstr(view_data->screen_y,view_data->screen_x + item_icon_len + 1,
              item_name,name_len_max);
}


// file_tree_dataの先頭から枠の高さ-1件までの項目を、記録済みscreen_y/screen_xへアイコン付きで描く。
// 返り値: 常に0。座標は事前にft_update_filetree_dataで設定しておく。
int ft_draw_filetree_data(struct editor_input_context *ctx,file_tree_data *file_tree_data){
       
    struct box ft_box = file_tree_data->ft_box;
    ft_item_view_data **sorted_item_view_data;
    //y座標の昇順にソートした配列を取得する
    int sorted_num = ft_get_sorted_screen_y_item_view_data(
            file_tree_data->item_view_data,
            file_tree_data->item_view_count_num,
            &sorted_item_view_data
        );

    if(sorted_num <= 0){
        return -1;
    }

    int i = 0;
    //実際に表示するアイテム個数カウンター
    int drawing_item_count = 0;
    while(i < sorted_num){
        ft_item_view_data *tmp_view_data = sorted_item_view_data[i];

        if(tmp_view_data->screen_y < 1){
            i++;
            continue;
        }
        
        // screen_y=-1は画面外の項目。表示行はft_update_filetree_dataで枠内に制限済み。
        if(tmp_view_data->screen_y < 0){
            i++;
            continue;
        }
        ft_draw_item_data(ctx,ft_box,tmp_view_data);
        //選択中の項目は、その行のツリー幅いっぱい(枠線は除く)を薄い白背景にして目立たせる
        // 選択行は項目領域内の0始まりの行なので、項目のscreen_yを同じ基準に直して比べる。
        const int item_row = tmp_view_data->screen_y - (file_tree_data->ft_item_box.pos.y + 1);
        //ファイルツリーにフォーカスしているときだけ色を変える
        if(item_row == file_tree_data->ft_item_select_line && 
            editor_get_screen_state(ctx->state) == filetree_screen_mode){
            set_cell_color_pair(
                PAIR_FILETREE_SELECT,
                ft_box.pos.x,
                tmp_view_data->screen_y,
                ft_box.w
            );
        }

        
        drawing_item_count++;
        i++;
    }
    
    file_tree_data->drawing_item_num = drawing_item_count;

    free(sorted_item_view_data);
    
    return 0;
}
