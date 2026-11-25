#ifndef LSP_SETTINGS_H
#define LSP_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>
#include "lsp_communication.h"

struct editor_state;
struct editor_input_context;

// 設定でLSPを使う指定になっていればtrue。stateがNULLでないこと。
bool is_lsp_enabled(struct editor_state *state);

// root_pathをルートとして、languageの言語サーバー(clangd)を起動しinitializeを送る。
// 返り値: 成功時は呼び出し側(lsp_server_data_add)が所有するlsp_process、失敗時NULL。epoll登録に失敗してもサーバーは返す(epfdは-1)。
struct lsp_process *lsp_run(const char *root_path,const char *language);

// lsp_server_dataを空の状態にする。
void lsp_server_data_init(lsp_server_data *data);
// procをdataの末尾へ追加し、所有権をdataへ移す。返り値: 成功0、引数不正・確保失敗-1(失敗時procは移らない)。
int lsp_server_data_add(lsp_server_data *data,struct lsp_process *proc);
// dataが持つ全サーバーを閉じて解放し、空の状態に戻す。
void lsp_server_data_destroy(lsp_server_data *data);

// ctxで現在使う言語サーバーを返す。現在は先頭の1つ。未起動・ctxがNULLならNULL。借用で解放しない。
struct lsp_process *lsp_get_active(struct editor_input_context *ctx);
// epollで監視できる言語サーバーが1つでもあればtrue。
bool is_lsp_running(struct editor_input_context *ctx);
// 監視中の全サーバーをtimeout_msずつ待ち、受信メッセージを処理する。
void lsp_poll_all(lsp_server_data *data,int timeout_ms);

#endif
