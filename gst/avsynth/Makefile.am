# plugindir is set in configure

if HAVE_ORC
ADD_ORC_CFLAGS = $(ORC_CFLAGS)
ADD_ORC_LIBS = $(ORC_LIBS) 
endif

##############################################################################
# change libgstplugin.la to something more suitable, e.g. libmysomething.la  #
##############################################################################
plugin_LTLIBRARIES = libgstavsynth.la

##############################################################################
# for the next set of variables, rename the prefix if you renamed the .la,   #
#  e.g. libgstplugin_la_SOURCES => libmysomething_la_SOURCES                 #
#       libgstplugin_la_CFLAGS  => libmysomething_la_CFLAGS                  #
#       libgstplugin_la_LIBADD  => libmysomething_la_LIBADD                  #
#       libgstplugin_la_LDFLAGS => libmysomething_la_LDFLAGS                 #
##############################################################################

# sources used to compile this plug-in
libgstavsynth_la_SOURCES = gstavsynth.c gstavsynth_loader.cpp gstavsynth_videofilter.cpp gstavsynth_videocache.cpp gstavsynth_videocache_cpp.cpp gstavsynth_scriptenvironment.cpp gstavsynth_scriptenvironment_cpp.cpp 

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgstavsynth_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_CFLAGS) $(AVSYNTH_CFLAGS) $(ADD_ORC_CFLAGS)  $(LIBOIL_CFLAGS)
libgstavsynth_la_CXXFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_CXXFLAGS) $(AVSYNTH_CFLAGS) $(ADD_ORC_CFLAGS) $(LIBOIL_CFLAGS)
libgstavsynth_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) $(GST_LIBS) $(AVSYNTH_LIBS) -lgstvideo-$(GST_MAJORMINOR) $(ADD_ORC_LIBS)  $(LIBOIL_LIBS)
libgstavsynth_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstavsynth_la_LIBTOOLFLAGS = --tag=disable-static
libgstavsynthincludedir = $(includedir)/gstreamer-@GST_MAJORMINOR@/gst/avsynth
libgstavsynthinclude_HEADERS = \
	gstavsynth_sdk.h \
        gstavsynth_sdk_cpp.h

# headers we need but don't want installed
noinst_HEADERS = gstavsynth.h gstavsynth_loader.h gstavsynth_videofilter.h gstavsynth_videocache.h gstavsynth_videocache_cpp.h gstavsynth_scriptenvironment.h gstavsynth_scriptenvironment_cpp.h
