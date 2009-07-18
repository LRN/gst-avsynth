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

typedef struct _GstAVSynthVideoFilter GstAVSynthVideoFilter;
typedef struct _GstAVSynthVideoFilterClass GstAVSynthVideoFilterClass;
typedef struct _GstAVSynthVideoFilterClassParams GstAVSynthVideoFilterClassParams;

#include "gstavsynth_scriptenvironment.h"
#include "gstavsynth_videocache.h"

G_BEGIN_DECLS

#define GST_TYPE_AVSYNTH_VIDEO_FILTER			(gst_avsynth_video_filter_get_type())
#define GST_AVSYNTH_VIDEO_FILTER(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVSYNTH_VIDEO_FILTER,GstAVSynthVideoFilter))
#define GST_IS_AVSYNTH_VIDEO_FILTER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVSYNTH_VIDEO_FILTER))
#define GST_AVSYNTH_VIDEO_FILTER_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVSYNTH_VIDEO_FILTER,GstAVSynthVideoFilterClass))
#define GST_IS_AVSYNTH_VIDEO_FILTER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVSYNTH_VIDEO_FILTER))
#define GST_AVSYNTH_VIDEO_FILTER_GET_CLASS(obj)		(G_TYPE_CHECK_CLASS_TYPE((obj),GST_TYPE_AVSYNTH_VIDEO_FILTER, GstAVSynthVideoFilterclass))

/* Groups sinkpad handle, videocache handle and related information */
struct AVSynthSink
{
  /* Sink handle */
  GstPad *sinkpad;

  /* Cache object (starts at NULL, allocated when we get data) */
  GstAVSynthVideoCache *cache;

  /* Segment we've got from upstream on this pad, different for each pad */
  GstSegment segment;

  /* Segment we've made by converting segment to default format */
  GstSegment defsegment;

  /* TRUE if we've got EOS on this pad (set to FALSE on newsegment) */
  gboolean eos;

  /* TRUE if we've got EOS and cache is empied too */
  gboolean starving;

  /* TRUE if we're flushing */
  gboolean flush;

  /* TRUE if we've got a seek event from downstream */
  gboolean seek;
 
  /* TRUE if we're seeking upstream (cache->rng_from is up to date) */
  gboolean seeking;

  /* If seek is TRUE, seek to this frame or earlier */
  gint64 seekhint;

  GMutex *sinkmutex;

  /* Is set to TRUE by framegetter before each call to GetFrame() */
  gboolean firstcall;

  /* After GetFrame() returns, these will contain the lowest and highest
   * frame intices that were requested since firstcall was TRUE.
   */
  gint64 minframe;
  gint64 maxframe;
  
  /* Add this to current frame index to get the index of earliest frame
   * required to produce current frame. Updated by framegetter.
   */
  gint64 minframeshift;

  gint64 maxframeshift;
};

struct _GstAVSynthVideoFilter
{
  GstElement element;

  /* Whenever the _chain() is called, create (and initialize) the filter */
  gboolean uninitialized;

  /* A plugin that registered the function */
  GModule *plugin;

  /* An object that implements the function */
  PClip impl;
  GMutex *impl_mutex;

  ScriptEnvironment *env;

  /* We need to keep track of our pads, so we do so here. */
  GstPad *srcpad;

  /* Array of AVSynthSink pointers */
  GPtrArray *sinks;

  /* Segment of the output video stream */
  GstSegment segment;
  
  /* New segment we've got from a seek event */
  GstSegment seeksegment;

  /* TRUE if we should rebuild our segment */
  gboolean newsegment;

  /* Frame counter, changes on seek */
/*  guint64 framenum;*/

  /* A thread that keeps calling GetFrame() method of the underlying filter */
  GstTask *framegetter;
  GStaticRecMutex *framegetter_mutex;

  /* Tells the framegetter thread to stop or pause*/
  gboolean stop;
  gboolean pause;
  GCond *pause_cond;
  GMutex *stop_mutex;

  /* An array to store GObject properties to be passes to apply() */
  AVSValue **args;  

  /* VideoInfo of the last output buffer */
  VideoInfo vi;

  /* TRUE when framegetter can't produce any more frames
   * (usually - because one of the source frames has reached eos)
   */
  gboolean eos;
};

struct AVSynthVideoFilterParam
{
  guint param_id;
  gchar *param_name;
  gchar param_type;
  gchar param_mult;
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

  GPtrArray /*of ptrs to AVSynthVideoFilterParam*/ *properties;
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
void gst_avsynth_video_filter_init (GstAVSynthVideoFilter *avsynth_video_filter);


GType gst_avsynth_video_filter_get_type (void);

void gst_avsynth_video_filter_finalize (GObject * object);

gboolean gst_avsynth_video_filter_query (GstPad * pad, GstQuery * query);
gboolean gst_avsynth_video_filter_src_event (GstPad * pad, GstEvent * event);

gboolean gst_avsynth_video_filter_setcaps (GstPad * pad, GstCaps * caps);
gboolean gst_avsynth_video_filter_sink_event (GstPad * pad, GstEvent * event);

GstFlowReturn gst_avsynth_video_filter_chain (GstPad * pad, GstBuffer * buf);

GstStateChangeReturn gst_avsynth_video_filter_change_state (GstElement * element, GstStateChange transition);

void gst_avsynth_video_filter_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
void gst_avsynth_video_filter_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);

gboolean gst_avsynth_video_filter_negotiate (GstAVSynthVideoFilter * avsynth_video_filter);

#define GST_AVSYNTH_VIDEO_FILTER_PARAMS_QDATA g_quark_from_static_string("avsynth-video-filter-params")

gboolean gst_avsynth_buf_pad_caps_to_vi (GstBuffer *buf, GstPad *pad, GstCaps *caps, VideoInfo *vi);

static gboolean gst_avsynth_video_filter_src_convert (GstPad * pad, GstFormat src_format, gint64 src_value,
    GstFormat * dest_format, gint64 * dest_value);

G_END_DECLS

#endif /* __GST_AVSYNTH_H__ */
