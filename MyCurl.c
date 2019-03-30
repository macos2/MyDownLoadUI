/*
 * MyCurl.c
 *
 *  Created on: 2018年12月26日
 *      Author: tom
 */

#include "MyCurl.h"

static gchar *SIZE_UNIT[] = { "Byte", "KiB", "MiB", "Gib", "Tib" };
static gchar *empty_str = "";

void size_format_long(glong *size, gint *i) {
	*i = 0;
	while (*size > 1024 && *i < 4) {
		*size = *size / 1024;
		*i = *i + 1;
	}
}

void size_format_double(gdouble *size, gint *i) {
	*i = 0;
	while (*size > 1024. && *i < 4) {
		*size = *size / 1024.;
		*i = *i + 1;
	}
}

typedef struct {
	CURL *curl;
	MyCurl *mycurl;
	GtkTreeRowReference *ref;
	curl_off_t dlnow, dltotal, ulnow, ultotal;
	curl_off_t pdlnow, pulnow, resume_offset;
	GMutex *mutex;
	GIOChannel *file;
	guint timeout;
	gchar *uri, *cookie, *suggest_filename, *filename, *local;
	CURLcode code;
	gint64 down_start_time_unix;
	gboolean stop;
	gboolean resume;
	gboolean delete;
	void *set_filename_data;
	void *set_cookies_data;
	void *get_proxy_data;
} MyCurlThreadData;

typedef struct {
	MyDownloadUi *ui;
	GList *finish_list, *down_list, *del_list; //MyCurlThreadData*
	GAsyncQueue *download_queue; //MyCurlThreadData*
	GMutex *mutex; //sync curl_list
	GThreadPool *pool;
	guint source_id;
	GRegex *uri_regex;
	my_curl_set_cookies_callback set_cookies_callback;
	my_curl_set_filename_callback set_filename_callback;
	my_curl_get_cookies_callback get_cookies_callback;
	my_curl_get_proxy_callback get_proxy_callback;
	GFreeFunc free_filename_data, free_cookies_data, free_proxy_data;

} MyCurlPrivate;

G_DEFINE_TYPE_WITH_CODE(MyCurl, my_curl, G_TYPE_OBJECT, G_ADD_PRIVATE(MyCurl));
gboolean my_curl_watch_func(MyCurl *mycurl);
void my_curl_thread(MyCurlThreadData *data, MyCurl *self);

void my_curl_dispose(MyCurl *self) {
	MyCurlPrivate *priv = my_curl_get_instance_private(self);
	g_object_unref(priv->ui);
}
;
void my_curl_finalize(MyCurl *self) {
	MyCurlPrivate *priv = my_curl_get_instance_private(self);
	g_async_queue_unref(priv->download_queue);
	g_mutex_free(priv->mutex);
	g_regex_unref(priv->uri_regex);
}
;

static void my_curl_class_init(MyCurlClass *klass) {
	GObjectClass *obj_class = klass;
	obj_class->finalize = my_curl_finalize;
	obj_class->dispose = my_curl_dispose;
}

void my_curl_down_menu_del(MyDownloadUi *ui, GtkTreeRowReference *ref,
		MyCurl *self) {
	GtkTreeIter iter;
	GtkTreePath *path = gtk_tree_row_reference_get_path(ref);
	GtkTreeModel *model = gtk_tree_row_reference_get_model(ref);
	MyCurlPrivate *priv = my_curl_get_instance_private(self);
	gtk_tree_model_get_iter(model, &iter, path);
	MyCurlThreadData *data;
	gtk_tree_model_get(model, &iter, down_col_thread_data, &data, -1);
	data->delete = TRUE;
	data->stop = TRUE;
	gtk_tree_path_free(path);
}
;

void my_curl_down_menu_stop(MyDownloadUi *ui, GtkTreeRowReference *ref,
		MyCurl *self) {
	GtkTreeIter iter;
	GtkTreePath *path = gtk_tree_row_reference_get_path(ref);
	GtkTreeModel *model = gtk_tree_row_reference_get_model(ref);
	gtk_tree_model_get_iter(model, &iter, path);
	MyCurlThreadData *data;
	gtk_tree_model_get(model, &iter, down_col_thread_data, &data, -1);
	data->stop = TRUE;
	gtk_tree_path_free(path);
}
;
void my_curl_down_menu_resume(MyDownloadUi *ui, GtkTreeRowReference *ref,
		MyCurl *self) {
	MyCurlPrivate *priv = my_curl_get_instance_private(self);
	GtkTreeIter iter;
	GtkTreePath *path = gtk_tree_row_reference_get_path(ref);
	GtkTreeModel *model = gtk_tree_row_reference_get_model(ref);
	gtk_tree_model_get_iter(model, &iter, path);
	MyCurlThreadData *data;
	gchar *name, *local, *temp,*temp2;
	GFile *file;
	gtk_tree_model_get(model, &iter, down_col_thread_data, &data, down_col_name,
			&name, down_col_save_local, &local, -1);
	if (data->stop == TRUE) {
		temp = g_strdup_printf("%s%s%s", local, G_DIR_SEPARATOR_S, name);
#ifdef G_OS_WIN32
		temp2 = g_convert(temp, -1, "GB2312", "UTF-8", NULL, NULL, NULL);
		g_free(temp);
		temp = temp2;
#endif
		data->file = g_io_channel_new_file(temp, "a", NULL);
		g_io_channel_set_encoding(data->file, NULL, NULL);
		data->stop = FALSE;
		data->resume = TRUE;
		data->resume_offset += data->dlnow;
		g_thread_pool_push(priv->pool, data, NULL);
		g_free(temp);
	}
	g_free(name);
	g_free(local);
	gtk_tree_path_free(path);
}
;
void my_curl_finish_menu_restart(MyDownloadUi *ui, GtkTreeRowReference *ref,
		MyCurl *self) {
	GtkTreeIter iter;
	GtkTreePath *path = gtk_tree_row_reference_get_path(ref);
	GtkTreeModel *model = gtk_tree_row_reference_get_model(ref);
	gchar *uri, *name, *local, *temp;
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_path_free(path);
	gtk_tree_model_get(model, &iter, finish_col_uri, &uri, finish_col_name,
			&name, finish_col_local, &local, -1);
	temp = g_strdup_printf("%s%s%s", local, G_DIR_SEPARATOR_S, name);
	g_unlink(temp);
	my_curl_add_download(self, uri, NULL, NULL, NULL, local, name, NULL, NULL,
			NULL,
			FALSE);
	g_free(temp);
	g_free(local);
	g_free(name);
	g_free(uri);
	gtk_list_store_remove(model, &iter);
}
;

static void my_curl_init(MyCurl *self) {
	MyCurlPrivate *priv = my_curl_get_instance_private(self);
	priv->ui = NULL;
	priv->finish_list = NULL;
	priv->down_list = NULL;
	priv->del_list = NULL;
	priv->download_queue = g_async_queue_new();
	priv->mutex = g_mutex_new();
	priv->pool = g_thread_pool_new(my_curl_thread, self, 5, FALSE, NULL);
	priv->uri_regex = g_regex_new("[\\w]+://[^\\s^\\n^,^\\\"]+", 0, 0, NULL);
	priv->set_cookies_callback = NULL;
	priv->set_filename_callback = NULL;
	priv->get_cookies_callback = NULL;
	priv->get_proxy_callback = NULL;
	priv->free_cookies_data = NULL;
	priv->free_filename_data = NULL;
	priv->free_proxy_data = NULL;
}

void my_url_ui_add_uri(MyDownloadUi *ui, gchar *uri, gchar *local,
		gchar *cookies, gchar *name, gchar *prefix, gchar *suffix,
		MyCurl *mycurl) {
	my_curl_add_download(mycurl, uri, cookies, prefix, suffix, local, name,
			NULL,
			NULL, NULL,
			FALSE);
}

MyCurl *my_curl_new(MyDownloadUi *ui) {
	MyCurl *mycurl = g_object_new(MY_TYPE_CURL, NULL);
	MyCurlPrivate *priv = my_curl_get_instance_private(mycurl);
	if (MY_IS_DOWNLOAD_UI(ui)) {
		priv->ui = g_object_ref(ui);
	} else {
		priv->ui = my_download_ui_new();
	}
	g_signal_connect(priv->ui, "add-download-uri", my_url_ui_add_uri, mycurl);
	g_signal_connect(priv->ui, "finish-menu-restart",
			my_curl_finish_menu_restart, mycurl); //my_curl_down_menu_stop
	g_signal_connect(priv->ui, "down-menu-stop", my_curl_down_menu_stop,
			mycurl);
	g_signal_connect(priv->ui, "down-menu-resume", my_curl_down_menu_resume,
			mycurl);
	g_signal_connect(priv->ui, "down-menu-del", my_curl_down_menu_del, mycurl);
	return mycurl;
}
;

size_t my_curl_write_callback(char *ptr, size_t size, size_t nmemb,
		MyCurlThreadData *data) {
	gchar *filename, *dfile, *t = NULL, *temp;
	MyCurlPrivate *priv = my_curl_get_instance_private(data->mycurl);
	same_name_operation sop;
	gint i = 0;
	gsize res;
	if (data->file == NULL) {
		if (priv->set_filename_callback != NULL) {
			t = priv->set_filename_callback(data->suggest_filename,
					data->set_filename_data);
			if (t == NULL /*|| g_strcmp0(t, "") == 0*/) {
				filename = g_strdup(data->filename);
				g_free(t);
			} else {
				g_free(filename);
				filename = t;
			}
		} else {
			if (data->suggest_filename != NULL) {
				filename = g_strdup(data->suggest_filename);
			} else {
				filename = g_strdup(data->filename);
			}
		}

		dfile = g_strdup_printf("%s%s%s", data->local, G_DIR_SEPARATOR_S,
				filename);
		//Windows 下文件名编码不是utf-8，
#ifdef G_OS_WIN32
		temp = g_convert(dfile, -1, "GB2312", "UTF-8", NULL, NULL, NULL);
		g_free(dfile);
		dfile = temp;
#endif
		GFile *file = g_file_new_for_path(dfile);
		GFile *parent = g_file_get_parent(file);
		gchar *path = g_file_get_path(parent);
		if (g_access(path, F_OK) != 0) {
			g_mkdir_with_parents(path, 0775);
		} else if (g_access(dfile, F_OK) == 0) {
			g_object_get(priv->ui, "same-name-operation", &sop, NULL);
			switch (sop) {
			case skip_download:
				data->stop = TRUE;
				break;
			case add_suffix:
				while (g_access(dfile, F_OK) == 0) {
					g_free(dfile);
					dfile = g_strdup_printf("%s%s%s.%02d", data->local,
					G_DIR_SEPARATOR_S, filename, i);
				}
				break;
			case over_write:
			default:
				break;
			}
		}
		g_free(path);
		g_object_unref(parent);
		g_object_unref(file);
		data->file = g_io_channel_new_file(dfile, "w+", NULL);
		g_io_channel_set_encoding(data->file, NULL, NULL);
		g_free(dfile);
		g_free(data->filename);
//文件名转回utf-8，防止下载列表显示乱码
#ifdef G_OS_WIN32
		data->filename = g_convert(filename, -1, "GB2312", "UTF-8", NULL, NULL, NULL);
#else
		data->filename = filename;
#endif
	}
	g_io_channel_write(data->file, ptr, size * nmemb, &res);
	return res;
	/*
	 res = nmemb * size;
	 fwrite(ptr, res, 1, data->file);
	 return res;*/
}
;

int my_curl_xferinfo_callback(MyCurlThreadData *data, curl_off_t dltotal,
		curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
	g_mutex_lock(data->mutex);
	data->dlnow = dlnow;
	data->dltotal = dltotal;
	data->ulnow = ulnow;
	data->ultotal = ultotal;
	g_mutex_unlock(data->mutex);
	return data->stop;
}

size_t my_curl_header_callback(char *ptr, size_t size, size_t nmemb,
		MyCurlThreadData *data) {
	size_t i = 0;
	gchar *temp1, *temp2;
	gchar *buf = g_strndup(ptr, size * nmemb);
	MyCurlPrivate *priv = my_curl_get_instance_private(data->mycurl);
	if (g_strstr_len(buf, size * nmemb, "Content-Disposition:") != NULL) {
		temp1 = g_strstr_len(buf, -1, "filename=");
		if (temp1 != NULL) {
			temp1 += 9;
			if (*temp1 == '"') {
				temp1 += 1;
			}
			temp2 = temp1;
			while (*temp2 != '"' && *temp2 != '\n' && *temp2 != ';'
					&& *temp2 != ' ') {
				temp2++;
			}
			data->suggest_filename = g_strndup(temp1, temp2 - temp1);
		}
	};
	if (g_strstr_len(buf, -1, "Set-Cookie:") != NULL) {
		g_mutex_lock(priv->mutex);
		temp1 = buf + 11;
		if (priv->set_cookies_callback != NULL) {
			priv->set_cookies_callback(temp1, data->set_cookies_data);
		}
		g_mutex_unlock(priv->mutex);
	};
	g_free(buf);
	return size * nmemb;
}
;

void my_curl_thread(MyCurlThreadData *data, MyCurl *self) {
	gchar *cookie = NULL;
	CURL *curl;
	gchar **proxy;
	GDateTime *now = g_date_time_new_now_local();
	data->down_start_time_unix = g_date_time_to_unix(now);
	g_date_time_unref(now);
	MyCurlPrivate *priv = my_curl_get_instance_private(self);
	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, data->uri);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, my_curl_write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, data);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, my_curl_xferinfo_callback);
	curl_easy_setopt(curl, CURLOPT_XFERINFODATA, data);
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, data->timeout);
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 10 * 1024);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, my_curl_header_callback);
	curl_easy_setopt(curl, CURLOPT_HEADERDATA, data);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	if (data->cookie != NULL)
		curl_easy_setopt(curl, CURLOPT_COOKIE, data->cookie);
	if (priv->get_cookies_callback != NULL && data->set_cookies_data != NULL) {
		cookie = priv->get_cookies_callback(data->set_cookies_data);
		if (cookie != NULL)
			curl_easy_setopt(curl, CURLOPT_COOKIE, cookie);
		g_free(cookie);
	}
	if (priv->get_proxy_callback != NULL) {
		proxy = priv->get_proxy_callback(data->uri, data->get_proxy_data);
		if (proxy != NULL) {
			if (g_strcmp0("direct://", proxy[0]) != 0)
				curl_easy_setopt(curl, CURLOPT_PROXY, proxy[0]);
			g_strfreev(proxy);
		}
	}
	if (data->resume)
		curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, data->resume_offset);
	data->resume = FALSE;
	data->curl = curl;
	g_mutex_lock(priv->mutex);
	priv->down_list = g_list_append(priv->down_list, data);
	g_mutex_unlock(priv->mutex);
	data->code = curl_easy_perform(curl);
	if (data->file != NULL) {
		g_io_channel_shutdown(data->file, TRUE, NULL);
		g_io_channel_unref(data->file);
		data->file = NULL;
	}
	g_mutex_lock(priv->mutex);
	priv->down_list = g_list_remove(priv->down_list, data);
	priv->finish_list = g_list_append(priv->finish_list, data);
	g_mutex_unlock(priv->mutex);
}

gboolean my_curl_watch_func(MyCurl *mycurl) {
	CURLcode code;
	CURL *curl;
	CURLMsg *curl_msg;
	curl_off_t size = 0;
	GtkListStore *dl_store, *fin_store;
	GtkTreeIter iter;
	GtkTreePath *path;
	gchar *uri, *local, *filename, *sfile, *err_msg;
	FILE *summy_file;
	GList *list;
	MyCurlThreadData *data = NULL;
	GDateTime *now, *elapsed_time;
	MyCurlPrivate *priv = my_curl_get_instance_private(mycurl);
	same_name_operation sop;
	gint i, j;
	guint timeout = 0, max_count, runing_count;
	now = g_date_time_new_now_local();
	g_object_get(priv->ui, "same-name-operation", &sop, "timeout", &timeout,
			"max-count", &max_count, NULL);
	g_thread_pool_set_max_threads(priv->pool, max_count, NULL);
	data = g_async_queue_try_pop_unlocked(priv->download_queue);
	dl_store = my_download_ui_get_download_store(priv->ui);
	fin_store = my_download_ui_get_finish_store(priv->ui);
	while (data != NULL) {
		my_download_ui_mutex_lock(priv->ui);
		path = gtk_tree_row_reference_get_path(data->ref);
		gtk_tree_model_get_iter(dl_store, &iter, path);
		gtk_tree_model_get(dl_store, &iter, down_col_uri, &uri,
				down_col_save_local, &local, down_col_name, &filename, -1);
		gtk_tree_path_free(path);
		data->file = NULL;
		data->timeout = timeout;
		data->mutex = g_mutex_new();
		data->down_start_time_unix = g_date_time_to_unix(now);
		data->filename = filename;
		data->local = local;
		g_thread_pool_push(priv->pool, data, NULL);
		g_free(uri);
		my_download_ui_mutex_unlock(priv->ui);
		data = g_async_queue_try_pop_unlocked(priv->download_queue);
	}
	g_mutex_lock(priv->mutex);
	list = priv->del_list;
	while (list != NULL) {
		data = list->data;
		g_free(data->uri);
		g_free(data->cookie);
		g_mutex_free(data->mutex);
		gtk_tree_row_reference_free(data->ref);
		curl_easy_cleanup(data->curl);
		g_free(data);
		list = list->next;
	}
	g_list_free(priv->del_list);
	priv->del_list = NULL;

	gint progress;
	gdouble speed;
	gchar *size_totalsize_format, *speed_format, *elapsed_format;
	gdouble temp1, temp2;
	list = priv->down_list;
	my_download_ui_mutex_lock(priv->ui);
	while (list != NULL) {
		data = list->data;
		g_mutex_lock(data->mutex);
		path = gtk_tree_row_reference_get_path(data->ref);
		gtk_tree_model_get_iter(dl_store, &iter, path);
		gtk_tree_path_free(path);
		speed = (data->dlnow - data->pdlnow) * 4.;
		data->pdlnow = data->dlnow;
		progress = 0;
		if (data->dltotal > 0)
			progress = ((data->dlnow + data->resume_offset) * 100)
					/ (data->dltotal + data->resume_offset);
		temp1 = data->dlnow + data->resume_offset;
		temp2 = data->dltotal + data->resume_offset;
		elapsed_time = g_date_time_new_from_unix_utc(
				g_date_time_to_unix(now) - data->down_start_time_unix);
		elapsed_format = g_date_time_format(elapsed_time, " %H:%M:%S ");
		size_format_double(&temp1, &i);
		size_format_double(&temp2, &j);
		size_totalsize_format = g_strdup_printf("%6.2f %s/%6.2f %s", temp1,
				SIZE_UNIT[i], temp2, SIZE_UNIT[j]);
		size_format_double(&speed, &i);
		speed_format = g_strdup_printf(" %6.2f %s/s ", speed, SIZE_UNIT[i]);
		gtk_list_store_set(dl_store, &iter, down_col_name, data->filename,
				down_col_progress, progress, down_col_file_size,
				data->dltotal + data->resume_offset, down_col_size_dlsize,
				size_totalsize_format, down_col_speed, speed_format,
				down_col_elapsed, elapsed_format, down_col_state, Downloading,
				down_col_state_pixbuf,
				my_download_ui_get_download_state_pixbuf(priv->ui, Downloading),
				-1);
		g_free(elapsed_format);
		g_free(speed_format);
		g_free(size_totalsize_format);
		g_date_time_unref(elapsed_time);
		g_mutex_unlock(data->mutex);
		list = list->next;
	}

	list = priv->finish_list;
	download_state state;
	while (list != NULL) {
		data = list->data;
		path = gtk_tree_row_reference_get_path(data->ref);
		gtk_tree_model_get_iter(dl_store, &iter, path);
		gtk_tree_path_free(path);
		if (data->stop) {
			gtk_list_store_set(dl_store, &iter, down_col_state, Stop,
					down_col_state_pixbuf,
					my_download_ui_get_download_state_pixbuf(priv->ui, Stop),
					down_col_speed, "---", -1);
			curl_easy_cleanup(data->curl);
		} else if (data->delete) {
			g_free(data->uri);
			g_free(data->cookie);
			g_mutex_free(data->mutex);
			gtk_tree_row_reference_free(data->ref);
			curl_easy_cleanup(data->curl);
			g_free(data);
		} else {
			gtk_tree_model_get(dl_store, &iter, down_col_save_local, &local,
					-1);
			gtk_list_store_remove(dl_store, &iter);
			gtk_list_store_append(fin_store, &iter);
			state = Finish;
			if (data->code != CURLE_OK)
				state = Error;
			temp1 = data->dltotal + data->resume_offset;
			size_format_double(&temp1, &i);
			size_totalsize_format = g_strdup_printf(" %6.2f %s ", temp1,
					SIZE_UNIT[i]);
			elapsed_format = g_date_time_format(now, " %Y-%m-%d %H:%M:%S ");
			gtk_list_store_set(fin_store, &iter, finish_col_state, state,
					finish_col_state_pixbuf,
					my_download_ui_get_download_state_pixbuf(priv->ui, state),
					finish_col_name, data->filename, finish_col_local, local,
					finish_col_size, data->dltotal + data->resume_offset,
					finish_col_size_format, size_totalsize_format,
					finish_col_finish_time_unix, g_date_time_to_unix(now),
					finish_col_finish_time, elapsed_format, finish_col_uri,
					data->uri, finish_col_error, curl_easy_strerror(data->code),
					-1);
			g_free(size_totalsize_format);
			g_free(elapsed_format);
			g_free(local);
			g_free(data->uri);
			g_free(data->cookie);
			g_free(data->suggest_filename);
			g_free(data->filename);
			g_free(data->local);
			if (priv->free_cookies_data != NULL
					&& data->set_cookies_data != NULL)
				priv->free_cookies_data(data->set_cookies_data);
			if (priv->free_filename_data != NULL
					&& data->set_filename_data != NULL)
				priv->free_filename_data(data->set_filename_data);
			if (priv->free_proxy_data != NULL && data->get_proxy_data != NULL)
				priv->free_proxy_data(data->get_proxy_data);
			g_mutex_free(data->mutex);
			gtk_tree_row_reference_free(data->ref);
			curl_easy_cleanup(data->curl);
			g_free(data);
		}
		list = list->next;
	}
	g_list_free(priv->finish_list);
	priv->finish_list = NULL;
	my_download_ui_mutex_unlock(priv->ui);
	g_mutex_unlock(priv->mutex);
	g_date_time_unref(now);
	return G_SOURCE_CONTINUE;
}

void my_curl_add_download(MyCurl *mycurl, gchar *uri, gchar *cookie,
		gchar *prefix, gchar *suffix, gchar *save_dir, gchar *f_name,
		void *cookies_cb_data, void *filename_cb_data, void *proxy_cb_data,
		gboolean force_uri_as_filename) {
	guint i = 0;
	gboolean uri_as_filename = force_uri_as_filename;
	gchar *save_local, *filename, **filename_op, *temp, *str;
	GtkListStore *dl_store;
	GtkTreeIter iter;
	GtkTreeRowReference *row_ref;
	GtkTreePath *tree_path;
	GFile *file;
	MyCurlThreadData *data;
	GArray *str_array = g_array_new(TRUE, TRUE, sizeof(gpointer));
	g_ptr_array_set_free_func(str_array, g_free);
	MyCurlPrivate *priv = my_curl_get_instance_private(mycurl);
	GMatchInfo *info;
	g_regex_match(priv->uri_regex, uri, 0, &info);
	while (g_match_info_matches(info)) {
		str = g_match_info_fetch(info, 0);
		g_array_append_val(str_array, str);
		g_match_info_next(info, NULL);
	}
	g_match_info_free(info);
	if (str_array->len == 0) {
		g_array_free(str_array, TRUE);
		return;
	}
	GDateTime *time = g_date_time_new_now_local();
	gchar *time_format = g_date_time_format(time, "%Y-%m-%d %H:%M:%S");
	dl_store = my_download_ui_get_download_store(priv->ui);
	my_download_ui_mutex_lock(priv->ui);
	if (save_dir == NULL) {
		g_object_get(priv->ui, "save-dir", &save_local, NULL);
	} else {
		save_local = g_strdup(save_dir);
	}
	if (force_uri_as_filename) {
		uri_as_filename = TRUE;
	} else {
		g_object_get(priv->ui, "uri-as-filename", &uri_as_filename, NULL);
	}
	if (prefix == NULL)
		prefix = empty_str;
	if (suffix == NULL)
		suffix = empty_str;
	if (cookie == NULL)
		cookie = empty_str;
	if (f_name == NULL)
		f_name = empty_str;
	i = 0;
	str = g_array_index(str_array, gpointer, i);
	while (str != NULL) {
		file = g_file_new_for_path(str);
		if (file == NULL) {
			i++;
			str = g_array_index(str_array, gpointer, i);
			continue;
		}
		if (g_strcmp0("", f_name) == 0) {
			filename = g_file_get_basename(file);
		} else {
			filename = g_strdup(f_name);
		}
		if (uri_as_filename || g_strcmp0("/", filename) == 0) {
			filename_op = g_strsplit(str, "/", -1);
			g_free(filename);
			filename = g_strjoinv("|", filename_op);
			g_strfreev(filename_op);
		}
		temp = g_strdup_printf("%s%s%s", prefix, filename, suffix);
		g_free(filename);
		filename = temp;
		data = g_malloc0(sizeof(MyCurlThreadData) + 1);
		gtk_list_store_append(dl_store, &iter);
		gtk_list_store_set(dl_store, &iter, down_col_name, filename,
				down_col_save_local, save_local, down_col_uri, str,
				down_col_state, Wait, down_col_state_pixbuf,
				my_download_ui_get_download_state_pixbuf(priv->ui, Wait),
				down_col_start_time, time_format, down_col_thread_data, data,
				-1);
		tree_path = gtk_tree_model_get_path(dl_store, &iter);
		row_ref = gtk_tree_row_reference_new(dl_store, tree_path);
		data->curl = NULL;
		data->mycurl = mycurl;
		data->ref = row_ref;
		data->uri = g_strdup(str);
		data->cookie = g_strdup(cookie);
		data->stop = FALSE;
		data->resume = FALSE;
		data->resume_offset = 0;
		data->delete = FALSE;
		data->set_cookies_data = cookies_cb_data;
		data->set_filename_data = filename_cb_data;
		data->get_proxy_data = proxy_cb_data;
		data->filename = filename;
		g_async_queue_push(priv->download_queue, data);
		gtk_tree_path_free(tree_path);
		g_object_unref(file);
		i++;
		str = g_array_index(str_array, gpointer, i);
	}
	my_download_ui_mutex_unlock(priv->ui);
	g_date_time_unref(time);
	g_ptr_array_set_free_func(str_array, g_free);
	g_array_free(str_array, TRUE);
	g_free(time_format);
	g_free(save_local);
	if (priv->source_id == 0)
		priv->source_id = g_timeout_add(250, my_curl_watch_func, mycurl);
}
;

void my_curl_set_set_cookies_callback(MyCurl *mycurl,
		my_curl_set_cookies_callback *cb, GFreeFunc *free_data_func) {
	MyCurlPrivate *priv = my_curl_get_instance_private(mycurl);
	priv->set_cookies_callback = cb;
	priv->free_cookies_data = free_data_func;
}
;

void my_curl_set_get_cookies_callback(MyCurl *mycurl,
		my_curl_get_cookies_callback *cb) {
	MyCurlPrivate *priv = my_curl_get_instance_private(mycurl);
	priv->get_cookies_callback = cb;
}
;

void my_curl_set_set_filename_callback(MyCurl *mycurl,
		my_curl_set_filename_callback *cb, GFreeFunc *free_data_func) {
	MyCurlPrivate *priv = my_curl_get_instance_private(mycurl);
	priv->set_filename_callback = cb;
	priv->free_filename_data = free_data_func;
}
;

void my_curl_set_get_proxy_callback(MyCurl *mycurl,
		my_curl_get_proxy_callback *cb, GFreeFunc *free_data_func) {
	MyCurlPrivate *priv = my_curl_get_instance_private(mycurl);
	priv->get_proxy_callback = cb;
	priv->free_proxy_data = free_data_func;
}
;

MyDownloadUi *my_curl_get_download_ui(MyCurl *mycurl) {
	MyCurlPrivate *priv = my_curl_get_instance_private(mycurl);
	return priv->ui;
}
;

guint my_curl_get_downloading_count(MyCurl *mycurl, gboolean wait_included) {
	guint res = 0;
	MyCurlPrivate *priv = my_curl_get_instance_private(mycurl);
	res = g_thread_pool_get_num_threads(priv->pool);
	if (wait_included) {
		res += g_thread_pool_unprocessed(priv->pool);
	}
	return res;
}
;
