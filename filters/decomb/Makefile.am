# avsynthplugindir is set in configure

avsynthplugin_LTLIBRARIES = libgstavsynth-decomb.la

# sources used to compile this plug-in
libgstavsynth_decomb_la_SOURCES = asmfuncsYUY2_intel.cpp asmfuncsYV12_intel.cpp AvisynthPluginInit.cpp Decimate.cpp DecimateYUY2.cpp FieldDeinterlace.cpp FieldDeinterlaceYUY2.cpp Telecide.cpp Utilities.cpp

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgstavsynth_decomb_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_CFLAGS) $(AVSYNTH_CFLAGS) \
	-I../../gst -masm=intel -DGCC__ -mmmx
libgstavsynth_decomb_la_CXXFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_CXXFLAGS) $(AVSYNTH_CFLAGS) \
	-I../../gst -masm=intel -DGCC__ -mmmx
libgstavsynth_decomb_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) $(GST_LIBS) $(AVSYNTH_LIBS)
# FIXME : move the following line content to a widely-available GST_AVSYNTH_FILTER_LDFLAGS
libgstavsynth_decomb_la_LDFLAGS = -module -avoid-version  -export-symbols-regex '^_*AvisynthPluginInit2$$' -no-undefined
libgstavsynth_decomb_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = asmfuncsYV12.h Decimate.h FieldDeinterlace.h info.h internal.h strYUY2.h Telecide.h Utilities.h version.h
