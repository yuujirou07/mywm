#ifndef ASCII_ART_COMB_H_
#define ASCII_ART_COMB_H_

#include<stdio.h>
#include<ncurses.h>
#define ascii_data_h_max_ 256 // ASCIIアートへ保持できる最大行数。
#define ascii_data_w_max_ 256 // 1行へ保持できる最大バイト数（終端NULを除く）。

// 読み込んだASCIIアートと、その有効範囲。
struct ascii_data{
        int w; // 有効な行の最大表示幅。
        int h; // ascii_dataに格納済みの行数。
        char ascii_data[ascii_data_h_max_ ][ascii_data_w_max_ + 1]; // 行ごとのNUL終端文字列。
};


void get_ascii_data(struct ascii_data *ascii_data,FILE *file,int screen_width);


#endif
