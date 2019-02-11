/*
 * MyCurl.h
 *
 *  Created on: 2018年12月26日
 *      Author: tom
 */

#ifndef MYCURL_H_
#define MYCURL_H_

#include <glib-object.h>
#include <gtk/gtk.h>
#include <curl/curl.h>
#include "MyDownloadUi.h"

G_BEGIN_DECLS
#define MY_TYPE_CURL my_curl_get_type()
G_DECLARE_DERIVABLE_TYPE(MyCurl,my_curl,MY,CURL,GObject)
;
typedef struct _MyCurlClass {
	GObjectClass parent_class;
};
typedef gchar * (*my_curl_set_filename_callback)(gchar *suggest_name,
		void *data);
typedef void (*my_curl_set_cookies_callback)(gchar *cookies,void *data);
typedef gchar * (*my_curl_get_cookies_callback)(void *data);

MyCurl *my_curl_new(MyDownloadUi *ui);
void my_curl_add_download(MyCurl *mycurl, gchar *uri, gchar *cookie,
		gchar *prefix, gchar *suffix, gchar *save_dir, gchar *f_name,void *cookies_cb_data,void *filename_cb_data,
		gboolean force_uri_as_filename);
void my_curl_set_set_cookies_callback(MyCurl *mycurl,my_curl_set_cookies_callback *cb,GFreeFunc *free_data_func);
void my_curl_set_get_cookies_callback(MyCurl *mycurl,my_curl_get_cookies_callback *cb);
void my_curl_set_set_filename_callback(MyCurl *mycurl,my_curl_set_filename_callback *cb,GFreeFunc *free_data_func);
MyDownloadUi *my_curl_get_download_ui(MyCurl *mycurl);

G_END_DECLS

#endif /* MYCURL_H_ */
