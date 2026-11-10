

#include <ncurses.h>
#include<stdio.h>
#include<stdlib.h>

#include"editor.h"
#include"editor_color.h"

int init_curses_color(struct editor_input_context *ctx){

    if(ctx == NULL){
        return -1;
    }
    start_color();
    
    //カラー非対応か対応色が64色以下ならカラーをつけない
    if (!has_colors() || COLOR_PAIRS < 64) {
        return 0;
    }
    

    ctx->color_list.col_id_data = malloc(sizeof(color_id_data) * SUPPORT_COLOR_NUM);
    if(ctx->color_list.col_id_data == NULL){
        return 0;
    }
    // 確保に成功してから件数を入れる。失敗時はget_color_idが-1を返す。
    ctx->color_list.col_data_allocate_num = SUPPORT_COLOR_NUM;
    ctx->color_list.col_data_num = SUPPORT_COLOR_NUM;

    for(int i = 0;i < 8;i++){
        for(int j = 0;j < 8;j++){
            int idx = (i * 8) + j;
            // ペア0は端末既定色で変更できないため登録しない。
            if(idx != 0)init_pair(idx,i,j);
            ctx->color_list.col_id_data[idx].id = idx;
            ctx->color_list.col_id_data[idx].fg_col = i;
            ctx->color_list.col_id_data[idx].bg_col = j;
        }
    }
    attrset(COLOR_PAIR(get_color_pair(ctx,COLOR_WHITE,COLOR_BLACK)));
    return 0;
}

int get_color_id(struct editor_input_context *ctx,int fg,int bg){
    if(ctx == NULL){
        return -1;
    }
    
    if(ctx->color_list.col_data_allocate_num <= 0){
        return -1;
    }

    int tmp_id = -1;
    for(int i = 0;i < SUPPORT_COLOR_NUM;i++){
        if(ctx->color_list.col_id_data[i].fg_col == fg && 
            ctx->color_list.col_id_data[i].bg_col == bg){
            tmp_id = i;
            break;
        }
    }
    return tmp_id;
}

// get_color_idの結果をCOLOR_PAIR・mvchgatへそのまま渡せる形で返す。
// 返り値: 色ペア番号。色非対応や未登録の組み合わせなら端末既定色の0。
short get_color_pair(struct editor_input_context *ctx,int fg,int bg){
    int id = get_color_id(ctx,fg,bg);
    return (id < 0) ? 0 : (short)id;
}
