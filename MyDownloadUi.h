/*
 * MyDownload.h
 *
 *  Created on: 2018年12月26日
 *      Author: tom
 */

#ifndef MYDOWNLOADUI_H_
#define MYDOWNLOADUI_H_

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS
#define DEC_GET_SET_PROP(TN,t_n,gobj_prop_name,prop,prop_type) prop_type t_n##_##get_##prop(TN *object); prop_type t_n##_##set_##prop(TN *object,prop_type set);

typedef enum{
add_suffix,
over_write,
skip_download
}same_name_operation;

typedef enum {
	Downloading, Wait, Retry, Stop, Error, Finish
} download_state;

typedef enum {
	down_col_name,
	down_col_uri,
	down_col_thread_data,//not display ,use for assocate the thread data
	down_col_progress,
	down_col_size_dlsize,
	down_col_state,
	down_col_state_pixbuf,
	down_col_save_local,
	down_col_start_time,
	down_col_elapsed,
	down_col_speed,
	down_col_file_size,//not display ,use for sort the size
} down_col;

typedef enum {
	finish_col_name,
	finish_col_size,//not display ,use for sort the state
	finish_col_local,
	finish_col_state,//not display ,use for sort the state
	finish_col_state_pixbuf,
	finish_col_finish_time_unix,//not display,use for sort the time
	finish_col_finish_time,
	finish_col_size_format,
	finish_col_uri,
	finish_col_error,
} finish_col;

#define MY_TYPE_DOWNLOAD_UI my_download_ui_get_type()
G_DECLARE_DERIVABLE_TYPE(MyDownloadUi, my_download_ui, MY, DOWNLOAD_UI, GtkBox);
typedef struct _MyDownloadUiClass {
	GtkBoxClass parent_class;
	void (*add_download_uri)(MyDownloadUi *ui,gchar *uri,gchar *local,gchar *cookies,gchar *name,gchar *prefix,gchar *suffix);		//signal "add-download-uri"
	void (*finish_menu_restart)(MyDownloadUi *ui,GtkTreeRowReference *ref);		//signal "finish-menu-restart"
	void (*finish_menu_del)(MyDownloadUi *ui,GtkTreeRowReference *ref); 	//signal "finish-menu-del"
	void (*down_menu_del)(MyDownloadUi *ui,GtkTreeRowReference *ref);		//signal "down-menu-del"
	void (*down_menu_stop)(MyDownloadUi *ui,GtkTreeRowReference *ref);		//signal "down-menu-stop"
	void (*down_menu_resume)(MyDownloadUi *ui,GtkTreeRowReference *ref);		//signal "down-menu-resume"
};

MyDownloadUi *my_download_ui_new();
GtkListStore *my_download_ui_get_download_store(MyDownloadUi *down);
GdkPixbuf *my_download_ui_get_download_state_pixbuf(MyDownloadUi *down,download_state state);
GtkListStore *my_download_ui_get_finish_store(MyDownloadUi *down);
/*
guint my_download_ui_get_timeout(MyDownloadUi *down);
guint my_download_ui_set_timeout(MyDownloadUi *down,guint timeout);
same_name_operation my_download_ui_get_same_name_op(MyDownloadUi *down);
same_name_operation my_download_ui_set_same_name_op(MyDownloadUi *down,same_name_operation opt);
gboolean my_download_ui_force_uri_as_name(MyDownloadUi *down);
gboolean my_download_ui_set_force_uri_as_name(MyDownloadUi *down,gboolean setting);
guint my_download_ui_get_max_count(MyDownloadUi *down);
guint my_download_ui_set_max_count(MyDownloadUi *down,guint max_count);
*/

DEC_GET_SET_PROP(MyDownloadUi,my_download_ui,"max count",max_count,guint);
DEC_GET_SET_PROP(MyDownloadUi,my_download_ui,"uri as filename",force_uri_as_name,gboolean);
DEC_GET_SET_PROP(MyDownloadUi,my_download_ui,"same-name-operation",same_name_op,same_name_operation);
DEC_GET_SET_PROP(MyDownloadUi,my_download_ui,"timeout",timeout,guint);
DEC_GET_SET_PROP(MyDownloadUi,my_download_ui,"save-dir",default_dir,gchar*);

void my_download_ui_mutex_lock(MyDownloadUi *down);
void my_download_ui_mutex_unlock(MyDownloadUi *down);

G_END_DECLS

#endif /* MYDOWNLOADUI_H_ */
