all:gresources.h gresources.c
	
gresources.h:folder-download-symbolic.svg image-loading-symbolic.svg list-add-symbolic.svg object-select-symbolic.svg preferences-system-symbolic.svg process-stop-symbolic.svg view-refresh-symbolic.svg window-close-symbolic.svg MyDownloadUi.glade gresources.xml
		glib-compile-resources --generate-header gresources.xml --target=$(@)

gresources.c:folder-download-symbolic.svg image-loading-symbolic.svg list-add-symbolic.svg object-select-symbolic.svg preferences-system-symbolic.svg process-stop-symbolic.svg view-refresh-symbolic.svg window-close-symbolic.svg MyDownloadUi.glade gresources.xml
		glib-compile-resources --generate-source gresources.xml --target=$(@)
