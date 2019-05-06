project('mycurl','c',version:'1.0')
gnome=import('gnome')

#dependency
gtk_dep=dependency('gtk+-3.0')
curl_dep=dependency('libcurl',required : false)
soup_dep=dependency('libsoup-2.4',required : false)
gsrc=gnome.compile_resources('gresources','gresources.xml')
inc=include_directories('.')

#libraries
ui_lib=static_library('MyDownloadUi','MyDownloadUi.c',gsrc,dependencies:gtk_dep,install:true,c_args:['-w','-O3','-fpic','-static'])
if curl_dep.found() 
mycurl=static_library('MyCurl','MyCurl.c','MyDownloadUi.c',gsrc,dependencies:[curl_dep,gtk_dep],c_args:['-w','-fpic','-O3','-static'])
#to be subproject 'mycurl_dep'
mycurl_dep=declare_dependency(include_directories:inc,link_with:mycurl,sources:gsrc,dependencies:[curl_dep,gtk_dep])
#demo
executable('mycurl_demo','mycurl_demo.c',gsrc,dependencies:[curl_dep,gtk_dep],link_with:mycurl,install:true,c_args:['-w','-O3','-static'])
endif

if soup_dep.found()
mysoup=static_library('MySoupDl','MySoupDl.c','MyDownloadUi.c',gsrc,dependencies:[soup_dep,gtk_dep],c_args:['-w','-fpic','-O3','-static'])
#to be subproject 'mysoupdl_dep'
mysoupdl_dep=declare_dependency(include_directories:inc,link_with:mysoup,sources:gsrc,dependencies:[soup_dep,gtk_dep])
executable('mysoupdl_demo','mysoupdl_demo.c',gsrc,dependencies:[soup_dep,gtk_dep],link_with:mysoup,install:true,c_args:['-w','-O3','-static'])
endif

#to be subproject 'mydownload_ui_dep'
mydownload_ui_dep=declare_dependency(include_directories:inc,link_with:ui_lib,sources:gsrc,dependencies:gtk_dep)
