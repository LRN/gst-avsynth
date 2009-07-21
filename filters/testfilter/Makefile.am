# avsynthplugindir is set in configure

avsynthplugin_LTLIBRARIES = libgstavsynth-test.la

# sources used to compile this plug-in
libgstavsynth_test_la_SOURCES = gstavsynth-test.cpp

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgstavsynth_test_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_CFLAGS) $(AVSYNTH_CFLAGS) \
	-I../../gst #for gstavsynth_sdk.h
libgstavsynth_test_la_CXXFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_CXXFLAGS) $(AVSYNTH_CFLAGS) \
	-I../../gst #for gstavsynth_sdk.h
libgstavsynth_test_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) $(GST_LIBS) $(AVSYNTH_LIBS)
# FIXME : move the following line content to a widely-available GST_AVSYNTH_FILTER_LDFLAGS
libgstavsynth_test_la_LDFLAGS = -module -avoid-version  -export-symbols-regex '^_*AvisynthPluginInit2$$' -no-undefined
libgstavsynth_test_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstavsynth-test.h
