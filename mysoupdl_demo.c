/*
 * main.c
 *
 *  Created on: 2019年4月29日
 *      Author: tom
 */

#include <libsoup/soup.h>
#include "MySoupDl.h"
GMainLoop *loop;
SoupSession *session;
gint run=0;

int main(int argc,char *argv[]){
	gtk_init(&argc,&argv);
	GtkWindow *mwin=gtk_window_new(GTK_WINDOW_TOPLEVEL);
	MyDownloadUi *ui=my_download_ui_new();
	SoupSession *session=soup_session_new();
	MySoupDl *dl=my_soup_dl_new(session,ui);
	gtk_container_add(mwin,ui);
	gtk_widget_show_all(mwin);
	g_signal_connect(mwin,"delete-event",gtk_main_quit,NULL);
	my_download_ui_set_timeout(ui,5);
	gtk_main();

}
