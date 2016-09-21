# Note: plugindir is set in configure

plugin_LTLIBRARIES = libsmoothingplugin.la

# Path to installation of the output SDK 
#ILLCORRECTION_CFLAGS = 
#ILLCORRECTION_LIBS = 

# sources used to compile this plug-in
libsmoothingplugin_la_SOURCES = gstsmoothingfilter.c gstsmoothingfilter.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libsmoothingplugin_la_CFLAGS = $(GST_CFLAGS)
libsmoothingplugin_la_LIBADD = $(GST_LIBS) -lgstvideo-1.0 
libsmoothingplugin_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) -rpath /usr/local/lib
libsmoothingplugin_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstsmoothingfilter.h
