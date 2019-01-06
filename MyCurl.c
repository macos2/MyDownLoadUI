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
	FILE *file;
	guint timeout;
	gchar *uri, *cookie;
	CURLcode code;
	gint64 down_start_time_unix;
	gboolean stop;
	gboolean resume;
	gboolean delete;
} MyCurlThreadData;

typedef struct {
	MyDownloadUi *ui;
	GList *finish_list, *down_list, *del_list; //MyCurlThreadData*
	GAsyncQueue *download_queue; //MyCurlThreadData*
	GMutex *mutex; //sync curl_list
	GThreadPool *pool;
	guint source_id;
	GRegex *uri_regex;
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
	gchar *name, *local, *temp;
	FILE *file;
	gtk_tree_model_get(model, &iter, down_col_thread_data, &data, down_col_name,
			&name, down_col_save_local, &local, -1);
	if (data->stop == TRUE) {
		temp = g_strdup_printf("%s%s%s", local, G_DIR_SEPARATOR_S, name);
		file = fopen(temp, "a");
		data->file = file;
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
	my_curl_add_download(self, uri, NULL, NULL, NULL, local, name, FALSE);
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
}

void my_url_ui_add_uri(MyDownloadUi *ui,gchar *uri,gchar *local,gchar *cookies,gchar *name,gchar *prefix,gchar *suffix, MyCurl *mycurl) {
	my_curl_add_download(mycurl, uri, cookies, prefix, suffix, local, name, FALSE);
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
	g_signal_connect(priv->ui, "down-menu-del", my_curl_down_menu_del,
			mycurl);
	return mycurl;
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

void my_curl_thread(MyCurlThreadData *data, MyCurl *self) {
	CURL *curl;
	GDateTime *now = g_date_time_new_now_local();
	data->down_start_time_unix = g_date_time_to_unix(now);
	g_date_time_unref(now);
	MyCurlPrivate *priv = my_curl_get_instance_private(self);
	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, data->uri);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, data->file);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, my_curl_xferinfo_callback);
	curl_easy_setopt(curl, CURLOPT_XFERINFODATA, data);
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, data->timeout);
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 10 * 1024);
	if (data->cookie != NULL)
		curl_easy_setopt(curl, CURLOPT_COOKIE, data->cookie);
	if (data->resume)
		curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, data->resume_offset);
	data->resume = FALSE;
	data->curl = curl;
	g_mutex_lock(priv->mutex);
	priv->down_list = g_list_append(priv->down_list, data);
	g_mutex_unlock(priv->mutex);
	data->code = curl_easy_perform(curl);
	fclose(data->file);
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
	gchar *uri, *local, *filename, *dfile, *sfile, *err_msg, *start_time;
	FILE *down_file, *summy_file;
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
				down_col_save_local, &local, down_col_name, &filename,
				down_col_start_time, &start_time, -1);
		gtk_tree_path_free(path);
		dfile = g_strdup_printf("%s%s%s", local, G_DIR_SEPARATOR_S, filename);
		if (g_access(dfile, F_OK) == 0) {
			switch (sop) {
			case skip_download:
				g_free(dfile);
				dfile = NULL;
				gtk_list_store_remove(dl_store, &iter);
				gtk_list_store_append(fin_store, &iter);
				gtk_list_store_set(fin_store, &iter, finish_col_finish_time,
						start_time, finish_col_name, filename, finish_col_state,
						Stop, finish_col_state_pixbuf,
						my_download_ui_get_download_state_pixbuf(priv->ui,
								Stop), finish_col_size_format, "Skip",
						finish_col_local, local, finish_col_uri, uri, -1);
				gtk_tree_row_reference_free(data->ref);
				g_free(data->uri);
				g_free(data);
				break;
			case add_suffix:
				i = 0;
				sfile = g_strdup_printf("%s.%02d", dfile, i);
				while (g_access(sfile, F_OK) == 0) {
					i++;
					g_free(sfile);
					sfile = g_strdup_printf("%s.%02u", dfile, i);
				}
				g_free(dfile);
				dfile = sfile;
				sfile = g_strdup_printf("%s.%02u", filename, i);
				g_free(filename);
				filename = sfile;
				break;
			case over_write:
			default:
				break;
			}
		}

		if (dfile != NULL) {
			gtk_list_store_set(dl_store, &iter, down_col_name, filename, -1);
			down_file = fopen(dfile, "w+");
			data->file = down_file;
			data->timeout = timeout;
			data->cookie = NULL;
			data->mutex = g_mutex_new();
			data->down_start_time_unix = g_date_time_to_unix(now);
			g_thread_pool_push(priv->pool, data, NULL);
		}
		g_free(filename);
		g_free(dfile);
		g_free(local);
		g_free(uri);
		g_free(start_time);
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
		gtk_list_store_set(dl_store, &iter, down_col_progress, progress,
				down_col_file_size, data->dltotal + data->resume_offset,
				down_col_size_dlsize, size_totalsize_format, down_col_speed,
				speed_format, down_col_elapsed, elapsed_format, down_col_state,
				Downloading, down_col_state_pixbuf,
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
			gtk_tree_model_get(dl_store, &iter, down_col_name, &filename,
					down_col_save_local, &local, -1);
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
					finish_col_name, filename, finish_col_local, local,
					finish_col_size, data->dltotal+data->resume_offset, finish_col_size_format,
					size_totalsize_format, finish_col_finish_time_unix,
					g_date_time_to_unix(now), finish_col_finish_time,
					elapsed_format, finish_col_uri, data->uri, finish_col_error,
					curl_easy_strerror(data->code), -1);
			g_free(size_totalsize_format);
			g_free(elapsed_format);
			g_free(filename);
			g_free(local);
			g_free(data->uri);
			g_free(data->cookie);
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
		file = g_file_new_for_uri(str);
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
		data = g_malloc(sizeof(MyCurlThreadData));
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
		g_async_queue_push(priv->download_queue, data);
		gtk_tree_path_free(tree_path);
		g_object_unref(file);
		g_free(filename);
		i++;
		str = g_array_index(str_array, gpointer, i);
	}
	my_download_ui_mutex_unlock(priv->ui);
	g_date_time_unref(time);
	g_array_free(str_array, TRUE);
	g_free(time_format);
	g_free(save_local);
	if (priv->source_id == 0)
		priv->source_id = g_timeout_add(250, my_curl_watch_func, mycurl);
}
;
MyDownloadUi *my_curl_get_download_ui(MyCurl *mycurl) {
	MyCurlPrivate *priv = my_curl_get_instance_private(mycurl);
	return priv->ui;
}
;