/*
 * main.c
 *
 *  Created on: 2018年12月24日
 *      Author: tom
 */

#include <curl/curl.h>
#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include "MyCurl.h"

int main(int argc, char *argv[]) {
	GtkWindow *mwin;
	MyDownloadUi *down;
	MyCurl *mycurl;
	gtk_init(&argc,&argv);
	mwin=gtk_window_new(GTK_WINDOW_TOPLEVEL);
	mycurl=my_curl_new(NULL);
	down=my_curl_get_download_ui(mycurl);
	gtk_container_add(mwin,down);
	gtk_widget_show_all(mwin);
	g_signal_connect(mwin,"delete-event",gtk_main_quit,NULL);
	my_download_ui_set_default_dir(down,g_get_home_dir());
	gtk_main();
	return 0;
}
