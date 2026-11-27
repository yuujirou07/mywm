#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>

#include "lsp_settings.h"
#include "editor.h"
#include "editor_log.h"

// 設定でLSPを使う指定になっているかを返す。
// 返り値: 有効ならtrue、stateがNULLまたは無効ならfalse。
bool is_lsp_enabled(struct editor_state *state){
    return state != NULL && state->settings_data->lsp.lsp_use;
}

// root_pathをルートとして言語サーバー(clangd)を起動し、initialize送信とepoll登録まで行う。languageはdidOpenで送る言語IDになる。
// 返り値: 成功時は新規確保したlsp_process(呼び出し側がlsp_server_data_addで渡して所有権を移す)、失敗時NULL。epoll登録だけ失敗した場合もサーバーは返し、epfdは-1になる。
struct lsp_process *lsp_run(const char *root_path,const char *language){
    if(root_path == NULL || language == NULL){
        return NULL;
    }

    struct lsp_process *tmp_lsp_process = malloc(sizeof(struct lsp_process));
    if(tmp_lsp_process == NULL){
        perror("malloc");
        return NULL;
    }
    lsp_process_init(tmp_lsp_process);

    char root_uri[(PATH_MAX * 3) + sizeof("file://")];
    char *lsp_argv[] = {"clangd", NULL};

    initialize_id(&tmp_lsp_process->id_data);

    if(lsp_path_to_file_uri(root_uri, sizeof(root_uri),root_path) == -1){
        error_log_write("cant make lsp root uri :(");
        free(tmp_lsp_process);
        return NULL;
    }
    if(lsp_start_server(tmp_lsp_process, "clangd", lsp_argv) == -1){
        error_log_write("cant start lsp server :(");
        free(tmp_lsp_process);
        return NULL;
    }
    if(lsp_send_initialize(
        tmp_lsp_process->to_server_fd,
        tmp_lsp_process->id_data.used_id_history[tmp_lsp_process->id_data.id_storage_counter++],
        getpid(),root_uri ) == -1){

        error_log_write("cant send lsp initialize :(");
        lsp_close_server(tmp_lsp_process);
        free(tmp_lsp_process);
        return NULL;
    }

    tmp_lsp_process->lsp_root_uri = strdup(root_uri);
    set_lsp_use_language(tmp_lsp_process,(char *)language);

    /* epollインスタンスを作成 */
    tmp_lsp_process->epfd = epoll_create(FDS_N_);
    if(tmp_lsp_process->epfd < 0){
        error_log_write("cant create lsp epoll\n");
        tmp_lsp_process->epfd = -1;
        return tmp_lsp_process;
    }

    /* from_server_fdの入力待ちイベントを設定してepollへ追加する */
    memset(&tmp_lsp_process->ev, 0, sizeof(struct epoll_event));
    tmp_lsp_process->ev.events = EPOLLIN;
    tmp_lsp_process->ev.data.fd = tmp_lsp_process->from_server_fd;
    if(epoll_ctl(
        tmp_lsp_process->epfd,
        EPOLL_CTL_ADD,
        tmp_lsp_process->from_server_fd,
        &tmp_lsp_process->ev) == -1){

        error_log_write("cant add lsp fd to epoll\n");
        close(tmp_lsp_process->epfd);
        tmp_lsp_process->epfd = -1;
    }

    return tmp_lsp_process;
}

// dataを空(要素0・確保0)の状態にする。既存の要素は解放しないので、初期化済みのdataには使わない。
// 返り値: なし。dataがNULLなら何もしない。
void lsp_server_data_init(lsp_server_data *data){
    if(data == NULL){
        return;
    }
    data->lsp_proc_list = NULL;
    data->lsp_proc_num = 0;
    data->lsp_proc_allocate_num = 0;
}

// procをdataの末尾へ追加し、procの所有権をdataへ移す。配列が満杯なら2倍に拡張する。
// 返り値: 成功0、引数がNULLまたは拡張時の確保失敗-1(失敗時procの所有権は呼び出し側に残る)。
int lsp_server_data_add(lsp_server_data *data,struct lsp_process *proc){
    if(data == NULL || proc == NULL){
        return -1;
    }

    if(data->lsp_proc_num >= data->lsp_proc_allocate_num){
        size_t new_num = (data->lsp_proc_allocate_num == 0) ? 2 : data->lsp_proc_allocate_num * 2;
        struct lsp_process **tmp_list =
            realloc(data->lsp_proc_list,sizeof(struct lsp_process *) * new_num);
        if(tmp_list == NULL){
            return -1;
        }
        data->lsp_proc_list = tmp_list;
        data->lsp_proc_allocate_num = new_num;
    }

    data->lsp_proc_list[data->lsp_proc_num++] = proc;
    return 0;
}

// dataが持つ全サーバーを閉じ、lsp_processとlsp_root_uriを解放して、dataを空の状態へ戻す。
// 返り値: なし。dataがNULLなら何もしない。
void lsp_server_data_destroy(lsp_server_data *data){
    if(data == NULL){
        return;
    }

    for(size_t i = 0;i < data->lsp_proc_num;i++){
        struct lsp_process *proc = data->lsp_proc_list[i];
        if(proc == NULL){
            continue;
        }
        lsp_close_server(proc);
        free(proc->lsp_root_uri);
        free(proc);
    }
    free(data->lsp_proc_list);
    lsp_server_data_init(data);
}

// ctxで現在使う言語サーバーを返す。今は言語を選別せず、起動順で先頭の1つを返す。
// 返り値: 先頭のlsp_process。未起動・ctxがNULLならNULL。返り値は借用で解放しない。
struct lsp_process *lsp_get_active(struct editor_input_context *ctx){
    if(ctx == NULL || ctx->lsp_server_data.lsp_proc_num == 0){
        return NULL;
    }
    return ctx->lsp_server_data.lsp_proc_list[0];
}

// epollで監視できる(epfdが有効な)言語サーバーが1つでもあるかを返す。
// 返り値: あればtrue、ctxがNULLまたは1つも無ければfalse。
bool is_lsp_running(struct editor_input_context *ctx){
    if(ctx == NULL){
        return false;
    }

    lsp_server_data *data = &ctx->lsp_server_data;
    for(size_t i = 0;i < data->lsp_proc_num;i++){
        if(data->lsp_proc_list[i] != NULL && data->lsp_proc_list[i]->epfd >= 0){
            return true;
        }
    }
    return false;
}

// lsp->epfdでtimeout_msミリ秒待機し、受信メッセージを処理・解放する。
// epoll_wait失敗時はfdを閉じてlsp->epfdを-1にする。
static void lsp_poll_events(struct lsp_process *lsp,int timeout_ms){
    struct epoll_event events[FDS_N_];

    int n_events = epoll_wait(lsp->epfd, events, FDS_N_, timeout_ms);
    if(n_events < 0){
        error_log_write("lsp epoll_wait error\n");
        close(lsp->epfd);
        lsp->epfd = -1;
        return;
    }

    for(int i = 0; i < n_events; i++){
        if(events[i].data.fd < 0)continue;
        if(events[i].data.fd != lsp->from_server_fd)continue;

        char *msg = lsp_read_message(lsp->from_server_fd);
        if(msg == NULL){
            // 1件分の読み取りに失敗したら、壊れたストリームを読み続けないよう残りのイベントも打ち切る。
            break;
        }
        lsp_handle_message(lsp, msg);
        error_log_write(lsp->from_server_msg_buff);
        free(msg);
    }
}

// dataの各サーバーのうちepfdが有効なものを、順にtimeout_msずつ待って受信メッセージを処理する。
// 返り値: なし。サーバーがN個だと最大でN×timeout_msブロックする。dataがNULLなら何もしない。
void lsp_poll_all(lsp_server_data *data,int timeout_ms){
    if(data == NULL){
        return;
    }

    for(size_t i = 0;i < data->lsp_proc_num;i++){
        struct lsp_process *lsp = data->lsp_proc_list[i];
        if(lsp != NULL && lsp->epfd >= 0){
            lsp_poll_events(lsp,timeout_ms);
        }
    }
}
