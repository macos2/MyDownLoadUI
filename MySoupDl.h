/*
 * MySoupDl.h
 *
 *  Created on: 2019年5月1日
 *      Author: tom
 */

#ifndef MYSOUPDL_H_
#define MYSOUPDL_H_
#include <glib-object.h>
#include <libsoup/soup.h>
#include "MyDownloadUi.h"

G_BEGIN_DECLS
#define MY_TYPE_SOUP_DL my_soup_dl_get_type()
G_DECLARE_DERIVABLE_TYPE(MySoupDl,my_soup_dl,MY,SOUP_DL,GObject)
typedef struct _MySoupDlClass{
	GObjectClass Parent_class;
	gchar* (*set_name)(MySoupDl *dl,const gchar *uri,const gchar *suggest_name,gpointer data);
	void (*download_finish)(MySoupDl *dl,const gchar *uri,const gchar *filename,const gchar *local,gpointer *data);
};

MySoupDl *my_soup_dl_new(SoupSession *session,MyDownloadUi *ui);
gboolean my_soup_dl_add_download(MySoupDl *dl,const gchar  *uri,gpointer user_data);
guint my_soup_dl_get_downloading_count(MySoupDl *dl,gboolean wait_included);
G_END_DECLS


#endif /* MYSOUPDL_H_ */
