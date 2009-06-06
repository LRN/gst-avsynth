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

#ifndef __GST_AVSYNTH_VIDEOFILTER_H__
#define __GST_AVSYNTH_VIDEOFILTER_H__

#include <gst/gst.h>

G_BEGIN_DECLS

typedef struct _GstAVSynthVideoFilter GstAVSynthVideoFilter;
typedef struct _GstAVSynthVideoFilterClass GstAVSynthVideoFilterClass;
typedef struct _GstAVSynthVideoFilterClassParams GstAVSynthVideoFilterClassParams;

#define GST_AVSYNTH_VIDEO_FILTER_PARAMS_QDATA g_quark_from_static_string("avsynth-video-filter-params")

#include "gstavsynth_scriptenvironment.h"
#include "gstavsynth_videofilter.h"

struct _GstAVSynthVideoFilter
{
  GstElement element;

  /* A plugin that registered the function */
  GModule *plugin;
  /* Function implementation */
  IScriptEnvironment::ApplyFunc apply;
  /* Function extra param */
  void* user_data;

  ScriptEnvironment *env;

  /* We need to keep track of our pads, so we do so here. */
  GstPad *srcpad;

  /* Array of GstPad */
  GPtrArray *sinkpads;

  /* Array of GstAVSynthVideoBuffer */
  GPtrArray *videobuffers;

  /* clipping segment */
  GstSegment segment;
};

struct _GstAVSynthVideoFilterClass
{
  GstElementClass parent_class;

  /* Path to the plugin module (UTF-8) that registered the function*/
  gchar *filename;

  /* Function name */
  gchar *name;
  /* Full (alternative) function name */
  gchar *fullname;
  /* Function parameter string */
  const gchar *params;

  GstPadTemplate *srctempl, *sinktempl;
};

struct _GstAVSynthVideoFilterClassParams
{
  /* Path to the plugin module (UTF-8) */
  gchar *filename;

  /* Function name */
  gchar *name;
  /* Full (alternative) function name */
  gchar *fullname;
  /* Function parameter string */
  const gchar *params;

  GstCaps *srccaps, *sinkcaps;
};

void gst_avsynth_video_filter_base_init (GstAVSynthVideoFilterClass * klass);
void gst_avsynth_video_filter_class_init (GstAVSynthVideoFilterClass * klass);
void gst_avsynth_video_filter_init (GstAVSynthVideoFilter * avsynth_video_filter);
void gst_avsynth_video_filter_finalize (GObject * object);

gboolean gst_avsynth_video_filter_query (GstPad * pad, GstQuery * query);
gboolean gst_avsynth_video_filter_src_event (GstPad * pad, GstEvent * event);

gboolean gst_avsynth_video_filter_setcaps (GstPad * pad, GstCaps * caps);
gboolean gst_avsynth_video_filter_sink_event (GstPad * pad, GstEvent * event);
GstFlowReturn gst_avsynth_video_filter_chain (GstPad * pad, GstBuffer * buf);

GstStateChangeReturn gst_avsynth_video_filter_change_state (GstElement * element,
    GstStateChange transition);

void gst_avsynth_video_filter_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
void gst_avsynth_video_filter_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

gboolean gst_avsynth_video_filter_negotiate (GstAVSynthVideoFilter * avsynth_video_filter);

#define GST_TYPE_AVSYNTH_VIDEO_FILTER \
  (gst_avsynth_video_filter_get_type())
#define GST_AVSYNTH_VIDEO_FILTER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVSYNTH_VIDEO_FILTER,GstAVSynthVideoFilter))
#define GST_AVSYNTH_VIDEO_FILTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVSYNTH_VIDEO_FILTER,GstAVSynthVideoFilterClass))
#define GST_IS_AVSYNTH_VIDEO_FILTER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVSYNTH_VIDEO_FILTER))
#define GST_IS_AVSYNTH_VIDEO_FILTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVSYNTH_VIDEO_FILTER))

G_END_DECLS

#endif /* __GST_AVSYNTH_H__ */
