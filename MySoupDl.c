/*
 * MySoupDl.c
 *
 *  Created on: 2019年5月1日
 *      Author: tom
 */

#include "MySoupDl.h"
#define BUF_SIZE 	524288
typedef struct {
	GMutex mux;
	gboolean plused, stop, timeout_reach, reply_reach, self_release;
	guint speed, reply, timeout;
	gchar *suggest_name, *uri, *local, *filename;
	GCancellable *cancle;
	same_name_operation op;
	GError *error;
	time_t start_time_unix;
	MySoupDl *dl;
	gchar buf[BUF_SIZE];
	GOutputStream *out;
	GInputStream *in;
} Watch_data;;

typedef struct {
	gsize len, loaded;
	download_state state;
	gpointer user_data;
	GtkTreeRowReference *row_ref;
	Watch_data *w;
} Thread_data;

typedef struct {
	MyDownloadUi *ui;
	SoupSession *session;
	GThreadPool *pool;
	gint source_id;
	GAsyncQueue *queue;
	GList *working;
} MySoupDlPrivate;

gchar *size_unit[] = { "Byte", "Kib", "Mib", "Gib", "Tib", };

gchar *format_size(gsize size) {
	guint8 i = 0;
	gdouble s = size;
	while (s > 1024. && i < 5) {
		s = s / 1024.;
		i++;
	}
	return g_strdup_printf("%6.2f %s", s, size_unit[i]);
}

G_DEFINE_TYPE_WITH_CODE(MySoupDl, my_soup_dl, G_TYPE_OBJECT,
		G_ADD_PRIVATE(MySoupDl));
void my_soup_dl_thread(Thread_data *data, MySoupDl *self);
gboolean my_soup_dl_watch(MySoupDl *dl);
void my_soup_dl_dispose(MySoupDl *self) {

}

void my_soup_dl_finalize(MySoupDl *self) {
	MySoupDlPrivate *priv = my_soup_dl_get_instance_private(self);
	if (priv->source_id != 0)
		g_source_remove(priv->source_id);
	g_object_unref(priv->session);
	g_object_unref(priv->ui);
	g_thread_pool_free(priv->pool, TRUE, TRUE);
	g_async_queue_unref(priv->queue);
	g_list_free(priv->working);
}

void watch_data_free(Watch_data *data) {
	g_free(data->suggest_name);
	g_free(data->uri);
	g_free(data->local);
	g_free(data->filename);
	g_object_unref(data->cancle);
	if (data->error != NULL)
		g_error_free(data->error);
	if(data->in!=NULL){
		g_input_stream_close(data->in,NULL,NULL);
		g_object_unref(data->in);
	}
	if(data->out!=NULL){
		g_output_stream_flush(data->out,NULL,NULL);
		g_output_stream_close(data->out,NULL,NULL);
		g_object_unref(data->out);
	}
	g_free(data);
}

static void my_soup_dl_class_init(MySoupDlClass *klass) {
	GObjectClass *obj = klass;
	obj->dispose = my_soup_dl_dispose;
	obj->finalize = my_soup_dl_finalize;
	g_signal_new("set_name", MY_TYPE_SOUP_DL, G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET(MySoupDlClass, set_name), NULL, NULL, NULL,
			G_TYPE_STRING, 3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER,
			NULL);
	g_signal_new("download_finish", MY_TYPE_SOUP_DL, G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET(MySoupDlClass, download_finish), NULL, NULL, NULL,
			G_TYPE_NONE, 4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
			G_TYPE_POINTER, NULL);
}

static void my_soup_dl_init(MySoupDl *self) {
	MySoupDlPrivate *priv = my_soup_dl_get_instance_private(self);
	priv->source_id = 0;
	//priv->pool = g_thread_pool_new(my_soup_dl_thread, self, 1, FALSE, NULL);
	priv->queue = g_async_queue_new();
	priv->working = NULL;
}

/*
 void my_soup_dl_thread(Thread_data *data, MySoupDl *self) {
 gchar buf[524288], *temp, *temp2,**strv;
 gsize received;
 GHashTable *head_table;
 gchar *dis_type;
 GFile *file, *parent;
 Watch_data *w = data->w;
 MySoupDlPrivate *priv = my_soup_dl_get_instance_private(self);
 SoupMessage *msg = soup_message_new("GET", w->uri);
 GInputStream *in=NULL;
 GOutputStream *out=NULL;
 if (data->state == Retry){
 if(data->loaded==data->len)data->loaded=0;
 soup_message_headers_set_range(msg->request_headers, data->loaded,
 data->len);
 }
 while(in==NULL&&w->reply_reach==FALSE&&g_cancellable_is_cancelled(w->cancle)==FALSE){
 g_mutex_lock(&w->mux);
 if(w->error!=NULL){
 g_error_free(w->error);
 w->error=NULL;
 }
 g_mutex_unlock(&w->mux);
 in = soup_session_send(priv->session, msg, w->cancle, &w->error);
 g_mutex_lock(&w->mux);
 if(in==NULL)w->reply++;
 g_mutex_unlock(&w->mux);
 }
 //in = soup_session_send(priv->session, msg, w->cancle, &w->error);
 g_mutex_lock(&w->mux);
 w->start_time_unix = time(NULL);
 if (in == NULL) {
 data->state = Error;
 g_object_unref(msg);
 g_mutex_unlock(&w->mux);
 return;
 } else if (data->state == Retry) {
 if (msg->status_code != SOUP_STATUS_PARTIAL_CONTENT) {
 temp=g_strdup_printf("%s%s%s",w->local,G_DIR_SEPARATOR_S,w->filename);
 g_unlink(temp);
 g_free(temp);
 data->loaded = 0;
 }
 } else {
 data->state = Wait;
 data->len = soup_message_headers_get_content_length(
 msg->response_headers);
 soup_message_headers_get_content_disposition(msg->response_headers,
 &dis_type, &head_table);
 if (g_strcmp0(dis_type, "attachment") == 0 && head_table != NULL) { //从文件头中获取推荐文件名
 w->suggest_name = g_strdup(
 g_hash_table_lookup(head_table, "filename"));
 }
 if (head_table != NULL)
 g_hash_table_unref(head_table);
 g_free(dis_type);
 }

 if (w->filename == NULL||g_strcmp0(w->filename, "")==0||g_strcmp0(w->filename, "/")==0) {
 g_signal_emit_by_name(self, "set_name", w->uri, w->suggest_name,
 data->user_data, &w->filename);
 if (w->filename == NULL || g_strcmp0(w->filename, "")==0) {
 g_free(w->filename);
 if (w->suggest_name == NULL||g_strcmp0(w->suggest_name, "")==0) {
 file = g_file_new_for_path(w->uri);
 temp = g_file_get_basename(file);
 strv=g_strsplit(temp,"?",2);
 w->filename=g_strdup(strv[0]);
 g_strfreev(strv);
 g_free(temp);
 g_object_unref(file);
 } else {
 w->filename = g_strdup(w->suggest_name);
 }
 }
 }
 if (w->local == NULL)
 w->local = g_strdup(g_get_home_dir());
 temp = g_strdup_printf("%s%s%s", w->local, G_DIR_SEPARATOR_S, w->filename);
 file = g_file_new_for_path(temp);
 g_free(temp);
 if (g_file_query_exists(file, NULL) && data->state != Retry) { //有同名文件存在
 switch (w->op) {
 case skip_download:
 w->error = g_error_new(g_io_error_quark(), 0, "Skip Existed File");
 g_input_stream_close(in, NULL, NULL);
 g_object_unref(msg);
 w->stop = TRUE;
 data->state = Error;
 g_mutex_unlock(&w->mux);
 return;
 case over_write:
 g_file_delete(file, NULL, NULL);
 break;
 case add_suffix:
 default:
 received = 0;
 temp = NULL;
 while (g_file_query_exists(file, NULL)) {
 g_object_unref(file);
 g_free(temp);
 temp = g_strdup_printf("%s.%02d", w->filename, received++);
 temp2 = g_strdup_printf("%s%s%s", w->local, G_DIR_SEPARATOR_S,
 temp);
 file = g_file_new_for_path(temp2);
 g_free(temp2);
 }
 g_free(w->filename);
 w->filename = temp;
 break;
 }
 }
 g_free(w->filename);
 g_free(w->local);
 w->filename = g_file_get_basename(file);
 parent = g_file_get_parent(file);
 w->local = g_file_get_path(parent);
 g_object_unref(parent);
 g_mutex_unlock(&w->mux);

 out=NULL;
 if(!g_cancellable_is_cancelled(w->cancle)){
 g_mkdir_with_parents(w->local,0777);
 if (data->state == Retry&&data->loaded>0) {
 out = g_file_append_to(file, G_FILE_CREATE_NONE, NULL,  &w->error);
 } else {
 out = g_file_replace(file,NULL,FALSE,G_FILE_CREATE_REPLACE_DESTINATION,NULL,&w->error);
 }
 }
 if(out==NULL){
 g_mutex_lock(&w->mux);
 data->state = Error;
 g_mutex_unlock(&w->mux);
 }
 while (out!=NULL) {
 if(data->len!=0){
 while (data->loaded < data->len) {
 received = 0;
 received = g_input_stream_read(in, buf, 524288, w->cancle, NULL);

 g_mutex_lock(&w->mux);
 if (g_cancellable_is_cancelled(w->cancle)){
 g_mutex_unlock(&w->mux);
 break;
 }else{
 g_output_stream_write(out, buf, received, NULL, NULL);
 data->loaded += received;
 w->speed += received;
 }
 g_mutex_unlock(&w->mux);

 data->state = Downloading;
 }
 }else{
 while (received!=0) {
 received = 0;
 received = g_input_stream_read(in, buf, 524288, w->cancle, NULL);
 g_mutex_lock(&w->mux);
 if (g_cancellable_is_cancelled(w->cancle)){
 g_mutex_unlock(&w->mux);
 break;
 }else{
 g_output_stream_write(out, buf, received, NULL, NULL);
 data->loaded += received;
 w->speed += received;
 }
 g_mutex_unlock(&w->mux);
 data->state = Downloading;
 }
 }
 if (w->timeout_reach && !w->reply_reach) {
 w->timeout = 0;
 w->reply++;
 w->timeout_reach=FALSE;
 g_input_stream_close(in, NULL, NULL);
 g_object_unref(in);
 soup_message_headers_set_range(msg->request_headers, data->loaded,
 data->len);
 in = soup_session_send(priv->session, msg, NULL, &w->error);
 if (in == NULL) {
 g_mutex_lock(&w->mux);
 data->state = Error;
 g_mutex_unlock(&w->mux);
 break;
 } else if (msg->status_code != SOUP_STATUS_PARTIAL_CONTENT) {
 g_mutex_lock(&w->mux);
 data->loaded = 0;
 data->state = Wait;
 g_output_stream_close(out, NULL, NULL);
 g_object_unref(out);
 g_file_delete(file, NULL, NULL);
 out = g_file_create(file, G_FILE_CREATE_REPLACE_DESTINATION,
 NULL,
 NULL);
 g_mutex_unlock(&w->mux);
 }
 g_cancellable_reset(w->cancle);
 } else if (w->reply_reach) {
 g_mutex_lock(&w->mux);
 data->state = Error;
 w->error = g_error_new(g_io_error_quark(), 0, "Max Reply Reach");
 g_mutex_unlock(&w->mux);
 break;
 } else if (w->plused || w->stop) {
 g_mutex_lock(&w->mux);
 data->state = Stop;
 w->error = g_error_new(g_io_error_quark(), 0, "User Plused");
 g_mutex_unlock(&w->mux);
 break;
 } else {
 g_mutex_lock(&w->mux);
 data->state = Finish;
 w->error = g_error_new(g_io_error_quark(), 0, "No Error");
 g_mutex_unlock(&w->mux);
 break;
 }
 }

 g_object_unref(file);
 if (in != NULL) {
 g_input_stream_close(in, NULL, NULL);
 g_object_unref(in);
 }
 g_output_stream_flush(out, NULL, NULL);
 g_output_stream_close(out, NULL, NULL);
 g_object_unref(out);
 g_object_unref(msg);
 if(w->self_release==TRUE){
 watch_data_free(w);
 gtk_tree_row_reference_free(data->row_ref);
 g_free(data);
 }
 }*/

GOutputStream *my_soup_dl_message_open_file(MySoupDl *self, Thread_data *data) {
	GOutputStream *out;
	Watch_data *w = data->w;
	GFile *file, *parent;
	gchar *temp, *temp2, **strv;
	guint index;
	if (w->filename == NULL || g_strcmp0(w->filename, "") == 0
			|| g_strcmp0(w->filename, "/") == 0) {
		g_signal_emit_by_name(self, "set_name", w->uri, w->suggest_name,
				data->user_data, &w->filename);
		if (w->filename == NULL || g_strcmp0(w->filename, "") == 0) {
			g_free(w->filename);
			if (w->suggest_name == NULL
					|| g_strcmp0(w->suggest_name, "") == 0) {
				file = g_file_new_for_path(w->uri);
				temp = g_file_get_basename(file);
				strv = g_strsplit(temp, "?", 2);
				w->filename = g_strdup(strv[0]);
				g_strfreev(strv);
				g_free(temp);
				g_object_unref(file);
			} else {
				w->filename = g_strdup(w->suggest_name);
			}
		}
	}
	if (w->local == NULL)
		w->local = g_strdup(g_get_home_dir());
	temp = g_strdup_printf("%s%s%s", w->local, G_DIR_SEPARATOR_S, w->filename);
	file = g_file_new_for_path(temp);
	g_free(temp);
	if (g_file_query_exists(file, NULL) && data->state != Retry) { //有同名文件存在
		switch (w->op) {
		case skip_download:
			w->error = g_error_new(g_io_error_quark(), 0, "Skip Existed File");
			w->stop = TRUE;
			data->state =Stop;
			break;
		case over_write:
			g_file_delete(file, NULL, NULL);
			break;
		case add_suffix:
		default:
			index = 0;
			temp = NULL;
			while (g_file_query_exists(file, NULL)) {
				g_object_unref(file);
				g_free(temp);
				temp = g_strdup_printf("%s.%02d", w->filename, index++);
				temp2 = g_strdup_printf("%s%s%s", w->local, G_DIR_SEPARATOR_S,
						temp);
				file = g_file_new_for_path(temp2);
				g_free(temp2);
			}
			g_free(w->filename);
			w->filename = temp;
			break;
		}
	}
	g_free(w->filename);
	g_free(w->local);
	w->filename = g_file_get_basename(file);
	parent = g_file_get_parent(file);
	w->local = g_file_get_path(parent);
	g_object_unref(parent);
	if(data->state==Stop){
		g_object_unref(file);
		return NULL;
	}else{
	out = NULL;
	if (!g_cancellable_is_cancelled(w->cancle)) {
		g_mkdir_with_parents(w->local, 0777);
		if (data->loaded > 0) {
			out = g_file_append_to(file, G_FILE_CREATE_NONE, NULL, &w->error);
		} else {
			out = g_file_replace(file, NULL, FALSE,
					G_FILE_CREATE_REPLACE_DESTINATION, NULL, &w->error);
		}
	}
	if(out==NULL)data->state=Error;
	g_object_unref(file);
	}
	return out;
}

void my_soup_dl_message_got_headers(SoupMessage *msg, Thread_data *data) {
	gchar *temp, *dis_type;
	GHashTable *head_table=NULL;
	Watch_data *w = data->w;
	if(w==NULL)return;
	w->start_time_unix = time(NULL);
	if (data->state == Retry) {
		if (msg->status_code == SOUP_STATUS_OK) {
			data->loaded = 0;
		}
	} else {
		data->state = Wait;
		data->len = soup_message_headers_get_content_length(
				msg->response_headers);
		soup_message_headers_get_content_disposition(msg->response_headers,
				&dis_type, &head_table);
		if (g_strcmp0(dis_type, "attachment") == 0 && head_table != NULL) { //从文件头中获取推荐文件名
			w->suggest_name = g_strdup(
					g_hash_table_lookup(head_table, "filename"));
		}
		if (head_table != NULL)
			g_hash_table_unref(head_table);
		g_free(dis_type);
	}

}

void my_soup_dl_message_finished(SoupMessage *msg, Thread_data *data) {
	if ((msg->status_code != SOUP_STATUS_OK	|| msg->status_code != SOUP_STATUS_PARTIAL_CONTENT)&&data->state!=Stop)
		data->state = Error;
	if(data->state==Downloading)data->state=Finish;
	if(data->w!=NULL){
		if(data->w->error!=NULL){
			data->w->error=g_error_new(soup_http_error_quark(),0,"%s",soup_status_get_phrase(msg->status_code));
		}
	}
}

void my_soup_dl_message_read(GInputStream *in, GAsyncResult *res,
		Thread_data *data) {
	GError *err = NULL;
	Watch_data *w = data->w;
	if(w==NULL)return;
	if(g_cancellable_is_cancelled(w->cancle)==TRUE){
		if(w->timeout_reach==TRUE){//速度过低
			g_input_stream_close(w->in,NULL,NULL);
			g_object_unref(in);
			data->state=Retry;
			g_cancellable_reset(w->cancle);
			w->timeout_reach=FALSE;
			my_soup_dl_download_start(w->dl,data);
		}else{//用户停止
		data->state=Stop;
		w->plused=TRUE;
		}
		return;
	}
	gsize size = g_input_stream_read_finish(in, res, &err);
	if (size > 0) {
		if (w->out == NULL)
			w->out = my_soup_dl_message_open_file(w->dl, data);
		if(w->out==NULL){
			return;
		}
		g_output_stream_write(w->out, w->buf, size, NULL, NULL);
		g_output_stream_flush(w->out,NULL,NULL);
		data->loaded += size;
		w->speed += size;
		data->state = Downloading;
		g_input_stream_read_async(in, w->buf, BUF_SIZE, G_PRIORITY_DEFAULT,w->cancle, my_soup_dl_message_read, data);
	} else if (size < 0) {
		if (w->error != NULL)
			g_error_free(w->error);
		w->error = err;
		data->state = Error;
	} else {
		data->state = Finish;
	}
}
;

void my_soup_dl_download_start_cb(SoupSession *session, GAsyncResult *res,
		Thread_data *data) {
	GError *err = NULL;
	Watch_data *w = data->w;
	if(data->state==Stop||data->state==Error)return;//may be skip if the file exist with the same file op apply;
	GInputStream *in = soup_session_send_finish(session, res, &err);
	if (in == NULL && w->reply_reach) {
		data->state = Error;
		w->error = err;
	} else if (in == NULL) {
		w->reply++;
		data->state=Retry;
		my_soup_dl_download_start(w->dl, data);
	} else {
		if(data->state!=Retry)w->reply=0;
		w->in=in;
		g_input_stream_read_async(in, w->buf, BUF_SIZE, G_PRIORITY_DEFAULT,
				w->cancle, my_soup_dl_message_read, data);
	}
}
;

void my_soup_dl_download_start(MySoupDl *self, Thread_data *data) {
	Watch_data *w = data->w;
	MySoupDlPrivate *priv = my_soup_dl_get_instance_private(self);
	SoupMessage *msg = soup_message_new("GET", w->uri);
	g_signal_connect(msg, "got-headers", my_soup_dl_message_got_headers, data);
	g_signal_connect(msg, "finished", my_soup_dl_message_finished, data);
	if (data->state == Retry) {
		if (data->loaded == data->len)
			data->loaded = 0;
		soup_message_headers_set_range(msg->request_headers, data->loaded,
				data->len);
	}
	g_mutex_lock(&w->mux);
	soup_session_send_async(priv->session, msg, w->cancle,
			my_soup_dl_download_start_cb, data);
	g_mutex_unlock(&w->mux);
}

void my_soup_dl_watch_update_download_row(MySoupDl *dl, Thread_data *data) {
	MySoupDlPrivate *priv = my_soup_dl_get_instance_private(dl);
	GtkListStore *dl_store;
	Watch_data *w = data->w;
	GtkTreeIter iter;
	GtkTreePath *path;
	gchar *filename, *dlsize_size, *speed, *t1, *t2, *start_time_format,
			*elapsed_time_format;
	gint progress, reply = 0;

	time_t current = time(NULL);
	time_t elapsed = current - w->start_time_unix;
	GDateTime *s = g_date_time_new_from_unix_local(w->start_time_unix);
	GDateTime *e = g_date_time_new_from_unix_utc(elapsed);
	start_time_format = g_date_time_format(s, "%Y-%m-%d %H:%M:%S");
	elapsed_time_format = g_date_time_format(e, "%H:%M:%S");

	dl_store = my_download_ui_get_download_store(priv->ui);
	path = gtk_tree_row_reference_get_path(data->row_ref);
	gtk_tree_model_get_iter(dl_store, &iter, path);
	gtk_tree_path_free(path);

	if (w->filename == NULL)
		filename = "";
	else
		filename = w->filename;

	t1 = format_size(data->loaded);
	t2 = format_size(data->len);
	dlsize_size = g_strdup_printf("%s/%s", t1, t2);
	g_free(t1);
	g_free(t2);
	t1 = format_size(w->speed * 4);
	speed = g_strdup_printf("%s/s", t1);
	g_free(t1);
	progress = 0;
	if (data->len != 0)
		progress = data->loaded * 100 / data->len;
	gtk_list_store_set(dl_store, &iter, down_col_name, filename,
			down_col_save_local, w->local, down_col_file_size, data->len,
			down_col_size_dlsize, dlsize_size, down_col_state, data->state,
			down_col_state_pixbuf,
			my_download_ui_get_download_state_pixbuf(priv->ui, data->state),
			down_col_progress, progress, down_col_speed, speed, down_col_reply,
			w->reply, down_col_elapsed, elapsed_time_format,
			down_col_start_time, start_time_format, -1);
	g_date_time_unref(s);
	g_date_time_unref(e);
	g_free(start_time_format);
	g_free(elapsed_time_format);
	g_free(dlsize_size);
	g_free(speed);
}

void my_soup_dl_watch_moveto_finish_table(MySoupDl *dl, Thread_data *data) {
	MySoupDlPrivate *priv = my_soup_dl_get_instance_private(dl);
	GtkListStore *dl_store, *fin_store;
	Watch_data *w = data->w;
	GtkTreeIter iter;
	GtkTreePath *path;
	gchar *filename, *dlsize_size, *speed, *t1, *t2, *time_format;
	GDateTime *time = g_date_time_new_now_local();
	gsize size;
	time_format = g_date_time_format(time, "%Y-%m-%d %H:%M:%S");
	dl_store = my_download_ui_get_download_store(priv->ui);
	fin_store = my_download_ui_get_finish_store(priv->ui);

	path = gtk_tree_row_reference_get_path(data->row_ref);
	gtk_tree_model_get_iter(dl_store, &iter, path);
	gtk_tree_path_free(path);
	size = data->len == 0 ? data->loaded : data->len;
	t1 = format_size(size);
	if (w->error != NULL) {
		t2 = g_strdup(w->error->message);
	} else {
		t2 = g_strdup("");
	}
	if (w->filename == NULL)
		filename = "";
	else
		filename = w->filename;

	gtk_list_store_remove(dl_store, &iter);
	gtk_list_store_append(fin_store, &iter);
	gtk_list_store_set(fin_store, &iter, finish_col_uri, w->uri,
			finish_col_state, data->state, finish_col_state_pixbuf,
			my_download_ui_get_download_state_pixbuf(priv->ui, data->state),
			finish_col_name, filename, finish_col_size, size, finish_col_local,
			w->local, finish_col_thread_data, data, finish_col_size_format, t1,
			finish_col_error, t2, finish_col_finish_time, time_format,
			finish_col_finish_time_unix, g_date_time_to_unix(time), -1);
	gtk_tree_row_reference_free(data->row_ref);
	data->row_ref = NULL;
	g_free(t1);
	g_free(t2);
	g_free(time_format);
	g_date_time_unref(time);
}

gboolean my_soup_dl_watch(MySoupDl *dl) {
	MySoupDlPrivate *priv = my_soup_dl_get_instance_private(dl);
	Thread_data *data;
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkListStore *dl_store = my_download_ui_get_download_store(priv->ui);
	GList *l = priv->working, *rl = NULL;
	Watch_data *w;
	gchar *uri;

	while (l != NULL) {
		data = l->data;
		w = data->w;
		switch (data->state) {
		case Error:
		case Finish:
			my_soup_dl_watch_moveto_finish_table(dl, data);
			rl = g_list_append(rl, l->data);
			g_signal_emit_by_name(dl, "download_finish", w->uri, w->filename,
					w->local, &data->user_data, NULL);
			watch_data_free(data->w);
			data->w = NULL;
			break;
		case Stop:
			if (w->stop) {
				g_signal_emit_by_name(dl, "download_finish", w->uri,
						w->filename, w->local, &data->user_data, NULL);
				if(w->self_release==TRUE){
					watch_data_free(data->w);
					gtk_tree_row_reference_free(data->row_ref);
					g_free(data);
				}else{
					my_soup_dl_watch_moveto_finish_table(dl, data);
					watch_data_free(data->w);
					data->w = NULL;
				}
			} else {
				my_soup_dl_watch_update_download_row(dl, data);
				g_output_stream_flush(w->out,NULL,NULL);
				g_output_stream_close(w->out,NULL,NULL);
				g_object_unref(w->out);
				g_input_stream_close(w->in,NULL,NULL);
				g_object_unref(w->in);
				w->out=NULL;
				w->in=NULL;
			}
			rl = g_list_append(rl, l->data);
			break;
		case Downloading:
			if (w->speed * 4 < 20480) {
				w->timeout++;
			} else {
				w->timeout = 0;
			}
			w->speed = 0;
			if ((w->timeout / 4) > my_download_ui_get_timeout(priv->ui)) {
				w->timeout_reach = TRUE;
				g_cancellable_cancel(w->cancle);
			}
			if (w->reply >= my_download_ui_get_reply(priv->ui))
				w->reply_reach = TRUE;
			my_soup_dl_watch_update_download_row(dl, data);
			break;
		case Retry:
		case Wait:
			my_soup_dl_watch_update_download_row(dl, data);
			break;
		default:
			break;
		}
		l = l->next;
	}
	l = rl;
	while (l != NULL) {
		priv->working = g_list_remove(priv->working, l->data);
		l = l->next;
	}
	g_list_free(rl);
	/*
	 if (g_async_queue_length(priv->queue) > 0) {
	 while (g_thread_pool_get_num_threads(priv->pool)
	 < my_download_ui_get_max_count(priv->ui)) {
	 data = g_async_queue_try_pop(priv->queue);
	 if (data != NULL) {
	 path = gtk_tree_row_reference_get_path(data->row_ref);
	 gtk_tree_model_get_iter(dl_store, &iter, path);
	 gtk_tree_path_free(path);
	 if (data->w == NULL) {
	 data->w = g_malloc0(sizeof(Watch_data));
	 gtk_tree_model_get(dl_store, &iter, down_col_uri, &uri, -1);
	 data->w->uri = uri;
	 data->w->local = g_strdup(
	 my_download_ui_get_default_dir(priv->ui));
	 data->w->op = my_download_ui_get_same_name_op(priv->ui);
	 data->w->cancle = g_cancellable_new();
	 g_mutex_init(&data->w->mux);
	 }
	 g_cancellable_reset(data->w->cancle);
	 g_thread_pool_push(priv->pool, data, NULL);
	 priv->working = g_list_append(priv->working, data);
	 } else {
	 break;
	 }
	 }
	 }
	 */
	if (g_async_queue_length(priv->queue) > 0) {
		while (g_list_length(priv->working)
				< my_download_ui_get_max_count(priv->ui)) {
			data = g_async_queue_try_pop(priv->queue);
			if (data != NULL) {
				path = gtk_tree_row_reference_get_path(data->row_ref);
				gtk_tree_model_get_iter(dl_store, &iter, path);
				gtk_tree_path_free(path);
				if (data->w == NULL) {
					data->w = g_malloc0(sizeof(Watch_data));
					gtk_tree_model_get(dl_store, &iter, down_col_uri, &uri, -1);
					data->w->uri = uri;
					data->w->local = g_strdup(
							my_download_ui_get_default_dir(priv->ui));
					data->w->op = my_download_ui_get_same_name_op(priv->ui);
					data->w->cancle = g_cancellable_new();
					g_mutex_init(&data->w->mux);
					data->w->dl = dl;
				}
				g_cancellable_reset(data->w->cancle);
				//g_thread_pool_push(priv->pool, data, NULL);
				my_soup_dl_download_start(dl, data);
				priv->working = g_list_append(priv->working, data);
			} else {
				break;
			}
		}
	}

	if (g_async_queue_length(priv->queue) == 0
			&& g_list_length(priv->working) == 0) {
		priv->source_id = 0;
		return G_SOURCE_REMOVE;
	}
	return G_SOURCE_CONTINUE;
}

void my_soup_dl_ui_add_download_uri(MyDownloadUi *ui, gchar *uri, gchar *local,
		gchar *cookies, gchar *name, gchar *prefix, gchar *suffix, MySoupDl *dl) {
	gchar *str, **strv;
	gint i = 0;
	strv = g_strsplit_set(uri, "\n\r", -1);
	while (strv[i] != NULL) {
		if (g_strcmp0("", strv[i]))
			my_soup_dl_add_download(dl, strv[i], NULL);
		i++;
	}
	g_strfreev(strv);
}
;

void my_soup_dl_ui_finish_menu_restart(MyDownloadUi *ui,
		GtkTreeRowReference *ref, MySoupDl *dl) {
	MySoupDlPrivate *priv = my_soup_dl_get_instance_private(dl);
	Thread_data *data;
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkListStore *finish_store = my_download_ui_get_finish_store(priv->ui);
	GtkListStore *down_store = my_download_ui_get_download_store(priv->ui);
	gchar *filename, *local, *uri;
	path = gtk_tree_row_reference_get_path(ref);
	gtk_tree_model_get_iter(finish_store, &iter, path);
	gtk_tree_path_free(path);
	gtk_tree_model_get(finish_store, &iter, finish_col_thread_data, &data,
			finish_col_name, &filename, finish_col_local, &local,
			finish_col_uri, &uri, -1);
	data->state = Retry;
	data->w = g_malloc0(sizeof(Watch_data));
	data->w->filename = filename;
	data->w->local = local;
	data->w->uri = uri;
	data->w->cancle = g_cancellable_new();
	gtk_list_store_remove(finish_store, &iter);
	gtk_list_store_append(down_store, &iter);
	path = gtk_tree_model_get_path(down_store, &iter);
	data->row_ref = gtk_tree_row_reference_new(down_store, path);
	gtk_tree_path_free(path);
	gtk_list_store_set(down_store, &iter, down_col_uri, uri, down_col_name,
			filename, down_col_save_local, local, down_col_thread_data, data,
			-1);
	g_async_queue_push(priv->queue, data);
	if (priv->source_id == 0)
		priv->source_id = g_timeout_add(250, my_soup_dl_watch, dl);
}
;
//signal "finish-menu-restart"
void my_soup_dl_ui_finish_menu_del(MyDownloadUi *ui, GtkTreeRowReference *ref,
		MySoupDl *dl) {
	MySoupDlPrivate *priv = my_soup_dl_get_instance_private(dl);
	Thread_data *data;
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkListStore *finish_store = my_download_ui_get_finish_store(priv->ui);
	path = gtk_tree_row_reference_get_path(ref);
	gtk_tree_model_get_iter(finish_store, &iter, path);
	gtk_tree_path_free(path);
	gtk_tree_model_get(finish_store, &iter, finish_col_thread_data, &data, -1);
	g_free(data);
}
;
//signal "finish-menu-del"

void my_soup_dl_ui_down_menu_del(MyDownloadUi *ui, GtkTreeRowReference *ref,
		MySoupDl *dl) {
	MySoupDlPrivate *priv = my_soup_dl_get_instance_private(dl);
	Thread_data *data;
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkListStore *down_store = my_download_ui_get_download_store(priv->ui);
	Watch_data *w;
	path = gtk_tree_row_reference_get_path(ref);
	gtk_tree_model_get_iter(down_store, &iter, path);
	gtk_tree_path_free(path);
	gtk_tree_model_get(down_store, &iter, down_col_thread_data, &data, -1);
	if (data->w != NULL) {
		w=data->w;
		g_mutex_lock(&w->mux);
		w->stop = TRUE;
		w->self_release = TRUE;
		g_cancellable_cancel(w->cancle);
		g_mutex_unlock(&w->mux);
	}
}
;
//signal "down-menu-del"

void my_soup_dl_ui_down_menu_stop(MyDownloadUi *ui, GtkTreeRowReference *ref,
		MySoupDl *dl) {
	MySoupDlPrivate *priv = my_soup_dl_get_instance_private(dl);
	Thread_data *data;
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkListStore *down_store = my_download_ui_get_download_store(priv->ui);
	Watch_data *w = NULL;
	path = gtk_tree_row_reference_get_path(ref);
	gtk_tree_model_get_iter(down_store, &iter, path);
	gtk_tree_path_free(path);
	gtk_tree_model_get(down_store, &iter, down_col_thread_data, &data, -1);

	if (data != NULL)
		w = data->w;
	if (w != NULL) {
		g_mutex_lock(&w->mux);
		data->state=Stop;
		w->plused = TRUE;
		g_cancellable_cancel(w->cancle);
		g_mutex_unlock(&w->mux);
	}
}
;
//signal "down-menu-stop"

void my_soup_dl_ui_down_menu_resume(MyDownloadUi *ui, GtkTreeRowReference *ref,
		MySoupDl *dl) {
	MySoupDlPrivate *priv = my_soup_dl_get_instance_private(dl);
	Thread_data *data;
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkListStore *down_store = my_download_ui_get_download_store(priv->ui);
	Watch_data *w;
	path = gtk_tree_row_reference_get_path(ref);
	gtk_tree_model_get_iter(down_store, &iter, path);
	gtk_tree_path_free(path);
	gtk_tree_model_get(down_store, &iter, down_col_thread_data, &data, -1);

	if (data->state == Stop) {
		priv->working = g_list_remove(priv->working, data);
		w = data->w;
		w->stop = FALSE;
		w->plused = FALSE;
		data->state = Retry;
		g_async_queue_push(priv->queue, data);
		gtk_list_store_set(down_store, &iter, down_col_state, Retry,
				down_col_state_pixbuf,
				my_download_ui_get_download_state_pixbuf(priv->ui, Retry), -1);
	}
	if (priv->source_id == 0)
		priv->source_id = g_timeout_add(250, my_soup_dl_watch, dl);
}
;
//signal "down-menu-resume"

MySoupDl *my_soup_dl_new(SoupSession *session, MyDownloadUi *ui) {
	MySoupDl *dl = g_object_new(MY_TYPE_SOUP_DL, NULL);
	MySoupDlPrivate *priv = my_soup_dl_get_instance_private(dl);
	if (session != NULL) {
		priv->session = g_object_ref(session);
	} else {
		priv->session = soup_session_new();
	}
	if (ui != NULL) {
		priv->ui = g_object_ref(ui);
	} else {
		priv->ui = my_download_ui_new();
	}
	g_thread_pool_set_max_threads(priv->pool,
			my_download_ui_get_max_count(priv->ui), NULL);
	g_signal_connect(ui, "add-download-uri", my_soup_dl_ui_add_download_uri,
			dl);
	g_signal_connect(ui, "finish-menu-restart",
			my_soup_dl_ui_finish_menu_restart, dl);
	g_signal_connect(ui, "finish-menu-del", my_soup_dl_ui_finish_menu_del, dl);
	g_signal_connect(ui, "down-menu-del", my_soup_dl_ui_down_menu_del, dl);
	g_signal_connect(ui, "down-menu-stop", my_soup_dl_ui_down_menu_stop, dl);
	g_signal_connect(ui, "down-menu-resume", my_soup_dl_ui_down_menu_resume,
			dl);
	g_object_bind_property(ui, "timeout", session, "timeout",
			G_BINDING_BIDIRECTIONAL);
	return dl;
}
;

gboolean my_soup_dl_add_download(MySoupDl *dl, const gchar *uri,
		gpointer user_data) {
	GtkTreeIter iter;
	MySoupDlPrivate *priv = my_soup_dl_get_instance_private(dl);
	GtkListStore *dl_store = my_download_ui_get_download_store(priv->ui);
	SoupURI *u = soup_uri_new(uri);
	if (u == NULL)
		return FALSE;
	soup_uri_free(u);
	Thread_data *data = g_malloc0(sizeof(Thread_data));
	data->user_data = user_data;
	data->state = Wait;
	gtk_list_store_append(dl_store, &iter);
	gtk_list_store_set(dl_store, &iter, down_col_state, data->state,
			down_col_state_pixbuf,
			my_download_ui_get_download_state_pixbuf(priv->ui, data->state),
			down_col_uri, uri, down_col_thread_data, data, -1);
	GtkTreePath *path = gtk_tree_model_get_path(dl_store, &iter);
	data->row_ref = gtk_tree_row_reference_new(dl_store, path);
	gtk_tree_path_free(path);
	g_async_queue_push(priv->queue, data);
	if (priv->source_id == 0)
		priv->source_id = g_timeout_add(250, my_soup_dl_watch, dl);
	return TRUE;
}
;

guint my_soup_dl_get_downloading_count(MySoupDl *dl, gboolean wait_included) {
	MySoupDlPrivate *priv = my_soup_dl_get_instance_private(dl);
//	guint num = g_thread_pool_get_num_threads(priv->pool);
	guint num=g_list_length(priv->working);
	if (wait_included) {
		num += g_async_queue_length(priv->queue);
	}
	return num;
}
;
