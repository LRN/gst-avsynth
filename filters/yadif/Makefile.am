# avsynthplugindir is set in configure

avsynthplugin_LTLIBRARIES = libgstavsynth-yadif.la

# sources used to compile this plug-in
libgstavsynth_yadif_la_SOURCES = yadif.c

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgstavsynth_yadif_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_CFLAGS) $(AVSYNTH_CFLAGS) \
	-I../../gst -mmmx
libgstavsynth_yadif_la_CXXFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_CXXFLAGS) $(AVSYNTH_CFLAGS) \
	-I../../gst -mmmx
libgstavsynth_yadif_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) $(GST_LIBS) $(AVSYNTH_LIBS)
# FIXME : move the following line content to a widely-available GST_AVSYNTH_FILTER_LDFLAGS
libgstavsynth_yadif_la_LDFLAGS = -module -avoid-version  -export-symbols-regex '^_*AvisynthPluginInit2$$' -no-undefined
libgstavsynth_yadif_la_LIBTOOLFLAGS = --tag=disable-static
