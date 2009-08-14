### all of the standard pc files we need to generate
pcverfiles =  \
	gstreamer-avsynth-@GST_MAJORMINOR@.pc
pcverfiles_uninstalled = \
	gstreamer-avsynth-@GST_MAJORMINOR@-uninstalled.pc

### all-local: $(pcverfiles) $(pcverfiles_uninstalled)
all-local: $(pcverfiles_uninstalled)

### how to generate versioned .pc files from .pc files in this dir
%-@GST_MAJORMINOR@.pc: %.pc
	cp $< $@
%-@GST_MAJORMINOR@-uninstalled.pc: %-uninstalled.pc
	cp $< $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pcverfiles)

CLEANFILES = $(pcverfiles) $(pcverfiles_uninstalled)
pcinfiles = \
          gstreamer-avsynth.pc.in gstreamer-avsynth-uninstalled.pc.in

DISTCLEANFILES = $(pcinfiles:.in=)
EXTRA_DIST = $(pcinfiles)
