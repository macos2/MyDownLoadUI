project('mycurl','c',version:'1.0')
gnome=import('gnome')

#dependency
curl_dep=dependency('libcurl')
gtk_dep=dependency('gtk+-3.0')
gsrc=gnome.compile_resources('gresources','gresources.xml')

#libraries
ui_lib=static_library('MyDownloadUi','MyDownloadUi.c',gsrc,dependencies:gtk_dep,install:true,c_args:['-w','-O3','-fpic','-static'])
mycurl=static_library('MyCurl','MyCurl.c','MyDownloadUi.c',gsrc,dependencies:[curl_dep,gtk_dep],c_args:['-w','-fpic','-O3','-static'])

#demo
executable('mycurl_demo','main.c',gsrc,dependencies:[curl_dep,gtk_dep],link_with:mycurl,install:true,c_args:['-w','-O3','-static'])

#to be subproject 'mycurl_dep'
inc=include_directories('.')
mycurl_dep=declare_dependency(include_directories:inc,link_with:mycurl,sources:gsrc,dependencies:[curl_dep,gtk_dep])

#to be subproject 'mydownload_ui_dep'
mydownload_ui_dep=declare_dependency(include_directories:inc,link_with:ui_lib,sources:gsrc,dependencies:gtk_dep)
