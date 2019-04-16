/*
 * MyDownload.c
 *
 *  Created on: 2018年12月26日
 *      Author: tom
 */

#define DEF_GET_SET_PROP(TN,t_n,gobj_prop_name,prop,prop_type) \
	prop_type t_n##_##get_##prop(TN *object){\
	prop_type res;\
	g_object_get(object,gobj_prop_name,&res,NULL);\
	return res;};\
	prop_type t_n##_##set_##prop(TN *object,prop_type set){\
	prop_type res;\
	g_object_set(object,gobj_prop_name,set,NULL);\
	g_object_get(object,gobj_prop_name,&res,NULL);\
	return res;};\

#include "MyDownloadUi.h"
#include "gresources.h"

enum {
	prop_max_count = 1,
	prop_timeout,
	prop_same_name_operation,
	prop_downloading_count,
	prop_finish_count,
	prop_error_count,
	prop_save_dir,
	prop_uri_as_filename,
	prop_reply,
	n_prop
};

GParamSpec *PROP[n_prop] = { NULL, };

typedef struct {
	GList *download_list;
	GtkListStore *download_liststore, *finish_liststore;
	GtkTextBuffer *add_uri_buffer;
	GtkDialog *add_dialog, *set_dialog;
	guint downloading;
	GtkAdjustment *max_count, *timeout,*reply;
	GtkFileChooser *save_dir_chooser,*add_dialog_local;
	GtkImage *state_download, *state_stop, *state_error, *state_retry,
			*state_finish, *state_wait;
	GtkRadioButton *add_suffix, *over_write, *skip_download;
	GtkCheckButton *uri_as_filename;
	GtkMenu *finish_menu, *down_menu;
	GMutex *mutex;
	GtkTreeView *finish_tree_view, *down_tree_view;
	GtkEntry *add_dialog_name,*add_dialog_prefix,*add_dialog_suffix,*add_dialog_cookies;
	GtkToggleButton *add_dialog_special_local;
} MyDownloadUiPrivate;

G_DEFINE_TYPE_WITH_CODE(MyDownloadUi, my_download_ui, GTK_TYPE_BOX,
		G_ADD_PRIVATE(MyDownloadUi));

void my_download_ui_set_property(MyDownloadUi *self, guint property_id,
		const GValue *value, GParamSpec *pspec) {
	guint i;
	GList *list;
	MyDownloadUiPrivate *priv = my_download_ui_get_instance_private(self);
	switch (property_id) {
	case prop_max_count:
		gtk_adjustment_set_value(priv->max_count,
				(gdouble) g_value_get_uint(value));
		break;
	case prop_same_name_operation:
		i = g_value_get_uint(value);
		switch (i) {
		case add_suffix:
			gtk_toggle_button_set_active(priv->add_suffix, TRUE);
			break;
		case over_write:
			gtk_toggle_button_set_active(priv->over_write, TRUE);
			break;
		case skip_download:
			gtk_toggle_button_set_active(priv->skip_download, TRUE);
			break;
		default:
			break;
		}
		break;
	case prop_save_dir:
		gtk_file_chooser_set_filename(priv->save_dir_chooser,
				g_value_get_string(value));
		break;
	case prop_timeout:
		gtk_adjustment_set_value(priv->timeout,
				(gdouble) g_value_get_uint(value));
		break;
	case prop_uri_as_filename:
		gtk_toggle_button_set_active(priv->uri_as_filename,
				g_value_get_boolean(value));
		break;
	case prop_reply:
		gtk_adjustment_set_value(priv->reply,g_value_get_uint(value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(self, property_id, pspec);
		break;
	}
}
;
void my_download_ui_get_property(MyDownloadUi *self, guint property_id,
		GValue *value, GParamSpec *pspec) {
	MyDownloadUiPrivate *priv = my_download_ui_get_instance_private(self);
	switch (property_id) {
	case prop_downloading_count:
		g_value_set_uint(value, g_list_length(priv->download_list));
		break;
	case prop_error_count:
	case prop_finish_count:
		g_value_set_uint(value, 0);
		break;
	case prop_max_count:
		g_value_set_uint(value, gtk_adjustment_get_value(priv->max_count));
		break;
	case prop_same_name_operation:
		if (gtk_toggle_button_get_active(priv->add_suffix))
			g_value_set_uint(value, add_suffix);
		if (gtk_toggle_button_get_active(priv->over_write))
			g_value_set_uint(value, over_write);
		if (gtk_toggle_button_get_active(priv->skip_download))
			g_value_set_uint(value, skip_download);
		break;
	case prop_save_dir:
		g_value_set_string(value,
				gtk_file_chooser_get_filename(priv->save_dir_chooser));
		break;
	case prop_timeout:
		g_value_set_uint(value, gtk_adjustment_get_value(priv->timeout));
		break;
	case prop_uri_as_filename:
		g_value_set_boolean(value,
				gtk_toggle_button_get_active(priv->uri_as_filename));
		break;
	case prop_reply:
		g_value_set_uint(value,gtk_adjustment_get_value(priv->reply));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(self, property_id, pspec);
		break;
	}
}
;
void my_download_ui_dispose(MyDownloadUi *self) {

}
void my_download_ui_finalize(MyDownloadUi *self) {
	MyDownloadUiPrivate *priv = my_download_ui_get_instance_private(self);
	g_list_free(priv->download_list);
	g_mutex_free(priv->mutex);
}
;
void add_dialog_clear(MyDownloadUi *self){
	MyDownloadUiPrivate *priv = my_download_ui_get_instance_private(self);
	GtkTextIter start, end;
	gtk_text_buffer_get_start_iter(priv->add_uri_buffer, &start);
	gtk_text_buffer_get_end_iter(priv->add_uri_buffer, &end);
	gtk_text_buffer_delete(priv->add_uri_buffer, &start, &end);
	gtk_entry_set_text(priv->add_dialog_cookies,"");
	gtk_entry_set_text(priv->add_dialog_name,"");
	gtk_entry_set_text(priv->add_dialog_prefix,"");
	gtk_entry_set_text(priv->add_dialog_suffix,"");
	gtk_file_chooser_set_current_folder(priv->add_dialog_local,".");
	gtk_toggle_button_set_active(priv->add_dialog_special_local,FALSE);
};

void add_dialog_cancle(GtkButton *button, MyDownloadUi *self) {
	add_dialog_clear(self);
}
;

void add_dialog_ok(GtkButton *button, MyDownloadUi *self) {
	MyDownloadUiPrivate *priv = my_download_ui_get_instance_private(self);
	GtkTextIter start, end;
	gchar *uri,*local,*cookies,*name,*prefix,*suffix;
	gtk_text_buffer_get_start_iter(priv->add_uri_buffer, &start);
	gtk_text_buffer_get_end_iter(priv->add_uri_buffer, &end);
	uri = gtk_text_buffer_get_text(priv->add_uri_buffer, &start, &end,
	TRUE);
	cookies=gtk_entry_get_text(priv->add_dialog_cookies);
	name=gtk_entry_get_text(priv->add_dialog_name);
	prefix=gtk_entry_get_text(priv->add_dialog_prefix);
	suffix=gtk_entry_get_text(priv->add_dialog_suffix);
	if(gtk_toggle_button_get_active(priv->add_dialog_special_local)){
		local=gtk_file_chooser_get_filename(priv->add_dialog_local);
	}else{
		local=gtk_file_chooser_get_filename(priv->save_dir_chooser);
	}
	g_signal_emit_by_name(self, "add-download-uri", uri,local,cookies,name,prefix,suffix,NULL);
	g_free(uri);
	g_free(local);
	add_dialog_clear(self);

}
;

gboolean finish_tree_view_press(GtkTreeView *view, GdkEvent *event,
		MyDownloadUi *self) {
	MyDownloadUiPrivate *priv = my_download_ui_get_instance_private(self);
	if (event->button.button != 3)
		return FALSE;
	gtk_menu_popup_at_pointer(priv->finish_menu, NULL);

	return TRUE;
}
;

gboolean down_tree_view_press(GtkTreeView *view, GdkEvent *event,
		MyDownloadUi *self) {
	MyDownloadUiPrivate *priv = my_download_ui_get_instance_private(self);
	if (event->button.button != 3)
		return FALSE;
	gtk_menu_popup_at_pointer(priv->down_menu, NULL);
	return TRUE;
}
;

void view_open(GtkTreeView *tree_view, gboolean open_dir, MyDownloadUi *self) {
	MyDownloadUiPrivate *priv = my_download_ui_get_instance_private(self);
	GtkTreeModel *model=gtk_tree_view_get_model(tree_view);
	GtkTreeSelection *sel = gtk_tree_view_get_selection(tree_view);
	GList *path_list = gtk_tree_selection_get_selected_rows(sel, &model), *list;
	guint col_name, col_local;
	gchar *name, *local, *temp;
	GtkTreeIter iter;
	GFile *file;
	my_download_ui_mutex_lock(self);
	if (tree_view == priv->down_tree_view) {
		col_name = down_col_name;
		col_local = down_col_save_local;
	} else {
		col_name = finish_col_name;
		col_local = finish_col_local;
	};
	list = path_list;
	while (list != NULL) {
		gtk_tree_model_get_iter(model, &iter, list->data);
		gtk_tree_model_get(model, &iter, col_name, &name, col_local, &local,
				-1);
		if (open_dir) {
			file = g_file_new_for_path(local);
		} else {
			temp = g_strdup_printf("%s%s%s", local, G_DIR_SEPARATOR_S, name);
			file = g_file_new_for_path(temp);
			g_free(temp);
		}
		temp = g_file_get_uri(file);
		gtk_show_uri_on_window(self, temp, GDK_CURRENT_TIME, NULL);
		g_object_unref(file);
		g_free(temp);
		g_free(local);
		g_free(name);
		list = list->next;
	}
	g_list_free_full(path_list, gtk_tree_path_free);
	my_download_ui_mutex_unlock(self);
}

void copy_uri(GtkTreeView *tree_view, guint col, MyDownloadUi *self) {
	GtkTreeIter iter;
	GString *str = g_string_new("");
	gchar *temp;
	GtkTreeSelection *sel = gtk_tree_view_get_selection(tree_view);
	GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
	GList *path = gtk_tree_selection_get_selected_rows(sel, &model), *list;
	list = path;
	my_download_ui_mutex_lock(self);
	while (list != NULL) {
		gtk_tree_model_get_iter(model, &iter, list->data);
		gtk_tree_model_get(model, &iter, col, &temp, -1);
		g_string_append(str, temp);
		g_free(temp);
		if(list->next!=NULL)g_string_append(str, "\n");
		list = list->next;
	}
	if(g_strcmp0(str->str,"")!=0)gtk_clipboard_set_text(gtk_clipboard_get_default(gdk_display_get_default()),
			str->str, -1);
	g_string_free(str, TRUE);
	g_list_free_full(path, gtk_tree_path_free);
	my_download_ui_mutex_unlock(self);
}

void del_task_file(GtkTreeView *tree_view, gboolean del_file, MyDownloadUi *self) {
	MyDownloadUiPrivate *priv = my_download_ui_get_instance_private(self);
	GtkTreeIter iter;
	GtkTreeSelection *sel = gtk_tree_view_get_selection(tree_view);
	GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
	GList *path = gtk_tree_selection_get_selected_rows(sel, &model), *ref_list =
	NULL, *list;
	GtkTreeRowReference *ref;
	GtkTreePath *tpath;
	gboolean down_view=(priv->down_tree_view==tree_view);
	gchar *name, *local, *temp;
	list = path;
	my_download_ui_mutex_lock(self);
	while (list != NULL) {
		ref = gtk_tree_row_reference_new(model, list->data);
		ref_list = g_list_append(ref_list, ref);
		list = list->next;
	}
	g_list_free_full(path, gtk_tree_path_free);
	list = ref_list;
	while (list != NULL) {
		tpath = gtk_tree_row_reference_get_path(list->data);
		gtk_tree_model_get_iter(model, &iter, tpath);
		down_view ?
				g_signal_emit_by_name(self, "down-menu-del", list->data, NULL) :
				g_signal_emit_by_name(self, "finish-menu-del", list->data,
						NULL);
		if (del_file) {
			down_view ?
					gtk_tree_model_get(model, &iter, down_col_name, &name,
							down_col_save_local, &local, -1) :
					gtk_tree_model_get(model, &iter, finish_col_name, &name,
							finish_col_local, &local, -1);
			temp = g_strdup_printf("%s%s%s", local, G_DIR_SEPARATOR_S, name);
			g_unlink(temp);
			g_free(local);
			g_free(name);
			g_free(temp);
		}
		gtk_list_store_remove(model, &iter);
		gtk_tree_path_free(tpath);
		list = list->next;
	}
	g_list_free_full(ref_list, gtk_tree_row_reference_free);
	my_download_ui_mutex_unlock(self);
}

void task_foreach_emit(gchar *signal,GtkTreeView *view,MyDownloadUi *self){
	MyDownloadUiPrivate *priv = my_download_ui_get_instance_private(self);
	GtkTreeSelection *sel=gtk_tree_view_get_selection(view);
	GtkTreeModel *model=gtk_tree_view_get_model(view);
	GList *path_list=gtk_tree_selection_get_selected_rows(sel,&model),*ref_list=NULL,*list;
	list=path_list;
	while(list!=NULL){
		ref_list=g_list_append(ref_list,gtk_tree_row_reference_new(model,list->data));
		list=list->next;
	}
	g_list_free_full(path_list,gtk_tree_path_free);
	list=ref_list;
	while(list!=NULL){
		g_signal_emit_by_name(self,signal,list->data,NULL);
		list=list->next;
	}
	g_list_free_full(ref_list,gtk_tree_row_reference_free);
}

void down_menu_open_dir(GtkMenuItem *menuitem, MyDownloadUi *self){
	MyDownloadUiPrivate *priv = my_download_ui_get_instance_private(self);
	view_open(priv->down_tree_view,TRUE,self);
}

void finish_menu_open(GtkMenuItem *menuitem, MyDownloadUi *self){
	MyDownloadUiPrivate *priv = my_download_ui_get_instance_private(self);
	view_open(priv->finish_tree_view,FALSE,self);
}
void finish_menu_open_dir(GtkMenuItem *menuitem, MyDownloadUi *self){
	MyDownloadUiPrivate *priv = my_download_ui_get_instance_private(self);
	view_open(priv->finish_tree_view,TRUE,self);
}

void down_menu_copy_uri(GtkMenuItem *menuitem, MyDownloadUi *self) {
	MyDownloadUiPrivate *priv = my_download_ui_get_instance_private(self);
	copy_uri(priv->down_tree_view, down_col_uri, self);
}

void finish_menu_copy_uri(GtkMenuItem *menuitem, MyDownloadUi *self) {
	MyDownloadUiPrivate *priv = my_download_ui_get_instance_private(self);
	copy_uri(priv->finish_tree_view, finish_col_uri, self);
}

void down_menu_del_task(GtkMenuItem *menuitem, MyDownloadUi *self) {
	MyDownloadUiPrivate *priv = my_download_ui_get_instance_private(self);
	del_task_file(priv->down_tree_view,  FALSE, self);
}

void down_menu_del_task_file(GtkMenuItem *menuitem, MyDownloadUi *self) {
	MyDownloadUiPrivate *priv = my_download_ui_get_instance_private(self);
	del_task_file(priv->down_tree_view, TRUE, self);
}

void finish_menu_del_task(GtkMenuItem *menuitem, MyDownloadUi *self) {
	MyDownloadUiPrivate *priv = my_download_ui_get_instance_private(self);
	del_task_file(priv->finish_tree_view,  FALSE, self);
}
void finish_menu_del_task_file(GtkMenuItem *menuitem, MyDownloadUi *self) {
	MyDownloadUiPrivate *priv = my_download_ui_get_instance_private(self);
	del_task_file(priv->finish_tree_view,  TRUE, self);
}

void down_menu_stop(GtkMenuItem *menuitem, MyDownloadUi *self){
	MyDownloadUiPrivate *priv = my_download_ui_get_instance_private(self);
	task_foreach_emit("down-menu-stop",priv->down_tree_view,self);
}

void down_menu_resume(GtkMenuItem *menuitem, MyDownloadUi *self){
	MyDownloadUiPrivate *priv = my_download_ui_get_instance_private(self);
	task_foreach_emit("down-menu-resume",priv->down_tree_view,self);
}

void finish_menu_restart(GtkMenuItem *menuitem, MyDownloadUi *self){
	MyDownloadUiPrivate *priv = my_download_ui_get_instance_private(self);
	task_foreach_emit("finish-menu-restart",priv->finish_tree_view,self);
}

static void my_download_ui_class_init(MyDownloadUiClass *klass) {
	GObjectClass *obj_class = klass;
	obj_class->get_property = my_download_ui_get_property;
	obj_class->set_property = my_download_ui_set_property;
	obj_class->dispose = my_download_ui_dispose;
	obj_class->finalize = my_download_ui_finalize;
	PROP[prop_downloading_count] = g_param_spec_uint("downloading-count",
			"downloading count", "downloading count", 0, G_MAXUINT, 0,
			G_PARAM_READABLE);
	PROP[prop_error_count] = g_param_spec_uint("error-count", "error count",
			"error count", 0, G_MAXUINT, 0, G_PARAM_READABLE);
	PROP[prop_finish_count] = g_param_spec_uint("finish-count", "finish count",
			"finish count", 0, G_MAXUINT, 0, G_PARAM_READABLE);
	PROP[prop_max_count] = g_param_spec_uint("max-count", "max count",
			"max count", 1, G_MAXUINT, 5,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	PROP[prop_same_name_operation] = g_param_spec_uint("same-name-operation",
			"same name operation", "same name operation", 0, 2, 0,
			G_PARAM_READWRITE);
	PROP[prop_save_dir] = g_param_spec_string("save-dir", "save directory",
			"save directory", ".", G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	PROP[prop_timeout] = g_param_spec_uint("timeout", "timeout", "timeout", 0,
	G_MAXUINT, 30, G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	PROP[prop_uri_as_filename] = g_param_spec_boolean("uri-as-filename",
			"uri as filename", "uri as filename", FALSE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	PROP[prop_reply] = g_param_spec_uint("reply", "reply", "reply", 0,
	G_MAXUINT, 3, G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_properties(obj_class, n_prop, PROP);
	g_signal_new("add-download-uri", MY_TYPE_DOWNLOAD_UI, G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET(MyDownloadUiClass, add_download_uri), NULL, NULL,
			NULL, G_TYPE_NONE, 6, G_TYPE_STRING, G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING,G_TYPE_STRING);
	g_signal_new("down-menu-stop", MY_TYPE_DOWNLOAD_UI, G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET(MyDownloadUiClass, down_menu_stop), NULL, NULL,
			NULL, G_TYPE_NONE,  1, GTK_TYPE_TREE_ROW_REFERENCE, NULL);
	g_signal_new("down-menu-resume", MY_TYPE_DOWNLOAD_UI, G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET(MyDownloadUiClass, down_menu_resume), NULL, NULL,
			NULL, G_TYPE_NONE,  1, GTK_TYPE_TREE_ROW_REFERENCE, NULL);
	g_signal_new("finish-menu-restart", MY_TYPE_DOWNLOAD_UI, G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET(MyDownloadUiClass, finish_menu_restart), NULL, NULL,
			NULL, G_TYPE_NONE,  1, GTK_TYPE_TREE_ROW_REFERENCE, NULL);
	g_signal_new("finish-menu-del", MY_TYPE_DOWNLOAD_UI, G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET(MyDownloadUiClass, finish_menu_del), NULL, NULL,
			NULL, G_TYPE_NONE, 1, GTK_TYPE_TREE_ROW_REFERENCE, NULL);
	g_signal_new("down-menu-del", MY_TYPE_DOWNLOAD_UI, G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET(MyDownloadUiClass, down_menu_del), NULL, NULL, NULL,
			G_TYPE_NONE, 1, GTK_TYPE_TREE_ROW_REFERENCE, NULL);
	//g_file_get_contents("MyDownloadUi.glade", &content, &size, NULL);
	//gtk_widget_class_set_template(klass, g_bytes_new_static(content, size));
	gtk_widget_class_set_template_from_resource(klass,"/my/downloadui/MyDownloadUi.glade");
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi,
			download_liststore);
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi,
			finish_liststore);
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi,
			add_uri_buffer);
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi,
			add_dialog);
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi,
			set_dialog);
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi,
			max_count);
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi, timeout);
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi,
			save_dir_chooser);
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi,
			state_download);
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi,
			state_stop);
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi,
			state_error);
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi,
			state_retry);
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi,
			state_finish);
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi,
			state_wait);
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi,
			add_suffix);
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi,
			over_write);
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi,
			skip_download);
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi,
			uri_as_filename);
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi,
			down_menu);
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi,
			finish_menu);
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi,
			finish_tree_view);
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi,
			down_tree_view);
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi,
			add_dialog_local);
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi,
			add_dialog_cookies);
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi,
			add_dialog_name);
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi,
			add_dialog_prefix);
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi,
			add_dialog_suffix);//add_dialog_special_local
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi,
			add_dialog_special_local);
	gtk_widget_class_bind_template_child_private(klass, MyDownloadUi,
			reply);
	gtk_widget_class_bind_template_callback(klass, add_dialog_cancle);
	gtk_widget_class_bind_template_callback(klass, add_dialog_ok);
	gtk_widget_class_bind_template_callback(klass, finish_tree_view_press);
	gtk_widget_class_bind_template_callback(klass, down_tree_view_press);
	gtk_widget_class_bind_template_callback(klass, down_menu_copy_uri);
	gtk_widget_class_bind_template_callback(klass, down_menu_open_dir);
	gtk_widget_class_bind_template_callback(klass, down_menu_del_task);
	gtk_widget_class_bind_template_callback(klass, down_menu_del_task_file);
	gtk_widget_class_bind_template_callback(klass, down_menu_resume);
	gtk_widget_class_bind_template_callback(klass, down_menu_stop);
	gtk_widget_class_bind_template_callback(klass, finish_menu_open);
	gtk_widget_class_bind_template_callback(klass, finish_menu_open_dir);
	gtk_widget_class_bind_template_callback(klass, finish_menu_copy_uri);
	gtk_widget_class_bind_template_callback(klass, finish_menu_del_task);
	gtk_widget_class_bind_template_callback(klass, finish_menu_del_task_file);
	gtk_widget_class_bind_template_callback(klass, finish_menu_restart);
}

static void my_download_ui_init(MyDownloadUi *self) {
	gtk_widget_init_template(self);
	MyDownloadUiPrivate *priv = my_download_ui_get_instance_private(self);
	priv->download_list = NULL;
	priv->mutex = g_mutex_new();
	g_object_bind_property(priv->add_dialog_special_local,"active",priv->add_dialog_local,"sensitive",G_BINDING_BIDIRECTIONAL);

}

MyDownloadUi *my_download_ui_new() {
	return g_object_new(MY_TYPE_DOWNLOAD_UI, NULL);
}
;
GtkListStore *my_download_ui_get_download_store(MyDownloadUi *down) {
	MyDownloadUiPrivate *priv = my_download_ui_get_instance_private(down);
	return priv->download_liststore;
}
;
GdkPixbuf *my_download_ui_get_download_state_pixbuf(MyDownloadUi *down,
		download_state state) {
	GdkPixbuf *pixbuf;
	MyDownloadUiPrivate *priv = my_download_ui_get_instance_private(down);
	switch (state) {
	case Downloading:
		pixbuf = gtk_image_get_pixbuf(priv->state_download);
		break;
	case Wait:
		pixbuf = gtk_image_get_pixbuf(priv->state_wait);
		break;
	case Stop:
		pixbuf = gtk_image_get_pixbuf(priv->state_stop);
		break;
	case Retry:
		pixbuf = gtk_image_get_pixbuf(priv->state_retry);
		break;
	case Error:
		pixbuf = gtk_image_get_pixbuf(priv->state_error);
		break;
	case Finish:
		pixbuf = gtk_image_get_pixbuf(priv->state_finish);
		break;
	default:
		pixbuf = NULL;
		break;
	}
	return pixbuf;
}
;
GtkListStore *my_download_ui_get_finish_store(MyDownloadUi *down) {
	MyDownloadUiPrivate *priv = my_download_ui_get_instance_private(down);
	return priv->finish_liststore;
}
;
/*
guint my_download_ui_get_timeout(MyDownloadUi *down){
	guint timeout;
	g_object_get(down,"timeout",&timeout,NULL);
	return timeout;
};
guint my_download_ui_set_timeout(MyDownloadUi *down,guint timeout){
	guint res;
	g_object_set(down,"timeout",timeout,NULL);
	g_object_get(down,"timeout",&res,NULL);
	return res;
};
same_name_operation my_download_ui_get_same_name_op(MyDownloadUi *down){
	same_name_operation op;
	g_object_get(down,"uri as filename",&op,NULL);
	return op;
};
same_name_operation my_download_ui_set_same_name_op(MyDownloadUi *down,same_name_operation opt){
	same_name_operation op;
	g_object_set(down,"uri as filename",opt);
	g_object_get(down,"uri as filename",&op,NULL);
	return op;
};
gboolean my_download_ui_force_uri_as_name(MyDownloadUi *down){
	gboolean res;
	g_object_get(down,"uri as filename",&res,NULL);
	return res;

};
gboolean my_download_ui_set_force_uri_as_name(MyDownloadUi *down,gboolean setting){
	gboolean res;
	g_object_set(down,"uri as filename",setting,NULL);
	g_object_get(down,"uri as filename",&res,NULL);
	return res;
};
*/
DEF_GET_SET_PROP(MyDownloadUi,my_download_ui,"max count",max_count,guint);
DEF_GET_SET_PROP(MyDownloadUi,my_download_ui,"uri as filename",force_uri_as_name,gboolean);
DEF_GET_SET_PROP(MyDownloadUi,my_download_ui,"same-name-operation",same_name_op,same_name_operation);
DEF_GET_SET_PROP(MyDownloadUi,my_download_ui,"timeout",timeout,guint);
DEF_GET_SET_PROP(MyDownloadUi,my_download_ui,"save-dir",default_dir,gchar*);
DEF_GET_SET_PROP(MyDownloadUi,my_download_ui,"reply",reply,guint);

void my_download_ui_mutex_lock(MyDownloadUi *down) {
	MyDownloadUiPrivate *priv = my_download_ui_get_instance_private(down);
	g_mutex_lock(priv->mutex);
}
;
void my_download_ui_mutex_unlock(MyDownloadUi *down) {
	MyDownloadUiPrivate *priv = my_download_ui_get_instance_private(down);
	g_mutex_unlock(priv->mutex);
}
;

