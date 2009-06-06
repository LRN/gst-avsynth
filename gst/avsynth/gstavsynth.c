/*
 * GStreamer:
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 *
 * AviSynth:
 * Copyright (C) 2007 Ben Rudiak-Gould et al.
 *
 * GstAVSynth:
 * Copyright (C) 2009 LRN <lrn1986 _at_ gmail _dot_ com>
 *
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
 * http://www.gnu.org/copyleft/gpl.html .
 */

/**
 * SECTION:element-avsynth
 *
 * FIXME:Describe avsynth here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! avsynth ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include "gstavsynth.h"

GST_DEBUG_CATEGORY (gst_avsynth_debug);

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
avsynth_init (GstPlugin * avsynth)
{
  gboolean result = TRUE;
  GST_DEBUG_CATEGORY_INIT (gst_avsynth_debug, "avsynth",
      0, "AviSynth filter wrapper");

  GST_LOG("calling gst_avsynth_video_filter_register...");
  result = result && gst_avsynth_video_filter_register(avsynth);
  GST_LOG("gst_avsynth_video_filter_register result is %d", result);

  return TRUE;
}

/* gstreamer looks for this structure to register avsynths */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "avsynth",
    "Wrapper for AviSynth filters",
    avsynth_init,
    VERSION,
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)
