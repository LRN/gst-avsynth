/*
 * GStreamer:
 * Copyright (C) 1999 Erik Walthinsen <omega@cse.ogi.edu>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <string.h>

#include <gst/gst.h>

#include "gstavsynth.h"
#include "gstavsynth_videocache.h"
#include "gstavsynth_videofilter.h"
#include "gstavsynth_loader.h"

#define SEEK_TYPE_IS_RELATIVE(t) (((t) != GST_SEEK_TYPE_NONE) && ((t) != GST_SEEK_TYPE_SET))

#define DEFAULT_CACHE_SIZE 10

enum
{
  PROP_0,
  PROP_CACHE_SIZE,
  PROP_LAST
};

static GstElementClass *parent_class = NULL;

GType
gst_avsynth_video_filter_get_type (void)
{
  static GType avsvf_type = 0;

  if (G_UNLIKELY (avsvf_type == 0)) {
    static const GTypeInfo avsvf_info = {
      sizeof (GstAVSynthVideoFilterClass), NULL, NULL,
      (GClassInitFunc) gst_avsynth_video_filter_class_init, NULL, NULL,
      sizeof (GstAVSynthVideoFilter), 0,
      (GInstanceInitFunc) gst_avsynth_video_filter_init,
    };

    avsvf_type = g_type_register_static (GST_TYPE_ELEMENT, "GstAVSynthVideoFilter",
        &avsvf_info, (GTypeFlags) 0);
  }
  return avsvf_type;
}


void
gst_avsynth_video_filter_base_init (GstAVSynthVideoFilterClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstAVSynthVideoFilterClassParams *params;
  GstElementDetails details;
  GstPadTemplate *sinktempl, *srctempl;

  params =
      (GstAVSynthVideoFilterClassParams *) g_type_get_qdata (G_OBJECT_CLASS_TYPE (klass),
      GST_AVSYNTH_VIDEO_FILTER_PARAMS_QDATA);
  g_assert (params != NULL);

  /* construct the element details struct */
  details.longname = g_strdup_printf ("AVSynth video filter %s",
      params->name);
  details.klass = g_strdup_printf ("Effect/Video");
  details.description = g_strdup_printf ("AVSynth video filter %s - %s (%s)",
      params->name, params->filename, params->params);
  details.author = (gchar *) "Author of the plugin, "
      "LRN <lrn1986 _at_ gmail _dot_ com>";
  gst_element_class_set_details (element_class, &details);
  g_free (details.longname);
  g_free (details.klass);
  g_free (details.description);

  /* pad templates */
  sinktempl = gst_pad_template_new ("sink", GST_PAD_SINK,
      GST_PAD_ALWAYS, params->sinkcaps);
  srctempl = gst_pad_template_new ("src", GST_PAD_SRC,
      GST_PAD_ALWAYS, params->srccaps);

  gst_element_class_add_pad_template (element_class, srctempl);
  gst_element_class_add_pad_template (element_class, sinktempl);

  klass->srctempl = srctempl;
  klass->sinktempl = sinktempl;
  klass->filename = params->filename;
  klass->name = params->name;
  klass->fullname = params->fullname;
  klass->params = params->params;
}

void
gst_avsynth_video_filter_class_init (GstAVSynthVideoFilterClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gint paramstate = -1;
  /*
   * -1 - reading '[' (name definition opening)
   * 0 - reading param name
   * 1 - reading type
   * 2 - reading multiplier
   */

  gchar *paramname = NULL;
  gint paramnamepos = 0;
  gchar paramtype = 0;
  gchar parammult = 0;
  gboolean newparam = FALSE;

  parent_class = (GstElementClass *) g_type_class_peek_parent (klass);

  gobject_class->finalize = gst_avsynth_video_filter_finalize;

  gobject_class->set_property = gst_avsynth_video_filter_set_property;
  gobject_class->get_property = gst_avsynth_video_filter_get_property;

  /* Param-string consists of one or more elements, each element has the following format:
   * <['['name']']type[multiplier]>
   * where
   * optional [name] token - parameter name (for named parameters)
   * mandatory type token - parameter type, a single letter that could be:
   *   'b'oolean, 'i'nteger, 'f'loat, 'c'lip, 's'tring or '.' (for untyped parameter)
   * optional multiplier token - either '+' or '*', meaning 'one or more arguments'
   *   or 'zero or more arguments' of the specified type
   * Examples:
   *   ci*s - one clip, zero or more integers, one string
   *   [clip1]c[threshold]i[mask]c - one clip (named "clip1"), one integer (named "threshold"),
   *     one clip (named "mask")
   *
   * GstAVSynth-specific:
   * each parameter is mapped to an object property
   * multiple (with + or *) parameters are mapped to one array property, which takes
   *   a list of comma-separated arguments or an array of arguments
   * GstAVSynth removes clip arguments and maps each one to a sink pad
   * No multiplier support for clips (yet?)
   * No untyped parameter support (yet?)
   */

  klass->properties = g_ptr_array_new ();

  /* This is a kind of Turing machine...i think... 
   * Goes through each character in param string.
   */
  for (gchar *params = (gchar *) klass->params; params[0]; params++)
  {
    /* Depending on the machine state ... */
    switch (paramstate)
    {
      case -1: /* Ready to read a new param group */
      {
        switch (params[0]) /* What character do we have here... */
        {
          case '[': /* It's a beginning of an optional [paramname] part of a group */
          {
            paramstate = 0;
            paramnamepos = 0;
            
            /* Find a closing ']' to calculate the name length */
            for (gchar *nameptr = params + 1; nameptr[0] != ']'; nameptr++)
            {
              /* we've reached the null terminator, param string is broken */            
              if (!nameptr)
                GST_ERROR ("Broken AviSynth param string - no trailing ']'");
              paramnamepos++;
            }
            paramname = g_new0 (gchar, ++paramnamepos);
            paramnamepos = 0;
            break;
          }
          case 'b': /* There is no name, i.e. a type goes right away */
          case 'i':
          case 'f':
          case 'c':
          case 's':
          case '.':
          {
            /* Change the state to 'read type' and make sure we re-enter this position */
            paramstate = 1;
            params--;
            break;
          }
          default:
          {
            GST_ERROR ("Broken AviSynth param string, unexpected character '%c'", params[0]);
          }
        }
        break;
      }
      case 0: /* Reading a name */
      {
        switch (params[0])
        {
          case ']': /* Done reading the name (no length check here, we've done it in state -1) */
          {
            paramstate = 1;
            break;
          }
          default: /* Let's hope that AviSynth is more restrictive in what characters are allowed to be in a name than GObject is */
          {
            paramname[paramnamepos++] = params[0];
          }
        }
        break;
      }
      case 1: /* Reading a type */
      {
        switch (params[0])
        {
          case 'b':
          case 'i':
          case 'f':
          case 'c':
          case 's':
          case '.':
          {
            /* Save the type and change state to 'reading multiplier' */
            paramtype = params[0];
            paramstate = 2;
            if (params[1] == 0)
               newparam = TRUE;
            break;
          }
          default:
          {
            GST_ERROR ("Broken AviSynth param string - '%c' is not a type", params[0]);
          }
        }
        break;
      }
      case 2: /* Reading a multiplier */
      {
        switch (params[0])
        {
          case '+':
          case '*':
          {
            /* Save the multiplier and reset the machine */
            parammult = params[0];
            paramstate = -1;
            newparam = TRUE;
            break;
          }
          case '[':
          {
            /* No multiplier, but a beginning of a new group (param name) */
            paramstate = -1;
            params--;
            newparam = TRUE;
            break;
          }
          case 'b':
          case 'i':
          case 'f':
          case 'c':
          case 's':
          case '.':
          {
            /* No multiplier, but a beginning of a new group (param type) */
            paramstate = 1;
            params--;
            newparam = TRUE;
            break;
          }
          default:
          {
            GST_ERROR ("Broken AviSynth param string");
          }
        }
      }
      break;
    }
    /* We've finished reading a group */
    if (newparam)
    {
      AVSynthVideoFilterParam *paramstr = NULL;
      GParamSpec *paramspec = NULL;
      /* FIXME: make all params construct-time-only? Because they are...right? */
      GParamFlags paramflags = (GParamFlags) (G_PARAM_READABLE |
        G_PARAM_WRITABLE | G_PARAM_STATIC_NICK |
        G_PARAM_STATIC_BLURB);

      paramstr = g_new0 (AVSynthVideoFilterParam, 1);
      newparam = FALSE;

      if (paramname == NULL)
      {
        /* The property is not named - create a name */
        if (paramtype == 'c')
          paramname = g_strdup_printf ("sink%d", klass->properties->len);
        else if (parammult == 0)
          paramname = g_strdup_printf ("param%d", klass->properties->len);
        else
          paramname = g_strdup_printf ("paramelement%d", klass->properties->len);
      }
      paramstr->param_name = paramname;

      switch (paramtype)
      {
        case 'b':
          paramspec = g_param_spec_boolean (paramname, "", "",
              FALSE, paramflags);
          break;
        case 'i':
          paramspec = g_param_spec_int (paramname, "", "",
              G_MININT, G_MAXINT, G_MININT, paramflags);
          break;
        case 'f':
          paramspec = g_param_spec_float (paramname, "", "",
              G_MINFLOAT, G_MAXFLOAT, G_MINFLOAT, paramflags);
          break;
        case 'c':
          break;
        case 's':
          paramspec = g_param_spec_string (paramname, "", "",
              "", paramflags);
          break;
        case '.':
          GST_ERROR ("Sorry, but i have not yet figured a way to implement "
                     "untyped params");
          break;
      }
      paramstr->param_type = paramtype;

      if (paramspec && parammult == 0)
      {
        g_object_class_install_property (gobject_class, klass->properties->len,
          paramspec);
      }
      else if (paramspec && (parammult == '+' || parammult == '*'))
      {
        GParamSpec *paramspec_array;
        gchar *paramname_array;
        paramname_array = g_strdup_printf ("param%d", klass->properties->len);
        paramspec_array = g_param_spec_value_array (paramname_array,
            "", "", paramspec, paramflags);
        g_object_class_install_property (gobject_class, klass->properties->len,
          paramspec_array);
      }
      paramstr->param_id = klass->properties->len;
      g_ptr_array_add (klass->properties, (gpointer) paramstr);

      parammult = 0;
      paramname = NULL;
      paramnamepos = 0;
      paramtype = 0;
    }
  }

  gstelement_class->change_state = gst_avsynth_video_filter_change_state;
}


GstBuffer *
gst_avsynth_ivf_to_buf (PVideoFrame &pvf, VideoInfo vi)
{
  ImplVideoFrameBuffer *ivf;
  GstBuffer *ret = NULL;
  GstVideoFormat vf;
  GstCaps *caps = NULL;
  //VideoInfo vi = NULL;
  gint64 data_size;
  gint offset;

  offset = pvf->GetOffset();

  ivf = (ImplVideoFrameBuffer *) pvf->GetFrameBuffer();

  data_size = ivf->GetDataSize();

  if (!data_size)
  {
    GST_ERROR ("Filter gave us a zero-sized buffer or a buffer with empty pointer");
    return NULL;
  }

  // Check requested pixel_type:
  switch (vi.pixel_type) {
    case VideoInfo::CS_BGR24:
      vf = GST_VIDEO_FORMAT_YV12;
      break;
    case VideoInfo::CS_BGR32:
      vf = GST_VIDEO_FORMAT_BGR;
      break;
    case VideoInfo::CS_YUY2:
      vf = GST_VIDEO_FORMAT_YUY2;
      break;
    case VideoInfo::CS_YV12:
      vf = GST_VIDEO_FORMAT_YV12;
      break;
    case VideoInfo::CS_I420:
      vf = GST_VIDEO_FORMAT_I420;
      break;
    default:
      GST_ERROR ("Filter gave us a buffer with unknown colorspace");
      return NULL;
  }

  if (vi.IsFieldBased())
  {
    caps = gst_video_format_new_caps_interlaced (vf, vi.width, vi.height, vi.fps_numerator, vi.fps_denominator,  1, 1, TRUE);
    if (vi.IsParityKnown())
    {
      if (vi.IsTFF())
        GST_BUFFER_FLAG_SET (ret, GST_VIDEO_BUFFER_TFF);
      else
        GST_BUFFER_FLAG_UNSET (ret, GST_VIDEO_BUFFER_TFF);
    }
  }
  else
    caps = gst_video_format_new_caps_interlaced (vf, vi.width, vi.height, vi.fps_numerator, vi.fps_denominator,  1, 1, FALSE);

  /* I'm assuming that downstream elements are smart enough to process aligned
   * buffer properly, so i am going to keep filter-specific alignment
   */

  ret = gst_buffer_try_new_and_alloc (data_size);

  if (!ret)
  {
    GST_ERROR ("Cannot allocate buffer of size %" G_GINT64_FORMAT, data_size);
    gst_caps_unref (caps);
    return NULL;
  }

  gst_buffer_set_caps (ret, caps);
  gst_caps_unref (caps);

  g_memmove (GST_BUFFER_DATA (ret), ivf->GetReadPtr() + offset, data_size - offset);

  return ret;
}

GstElement *debug_get_the_pipeline (GstElement *self)
{
  GstElement *parent = NULL;
  GstElement *result = NULL;
  parent = GST_ELEMENT (gst_element_get_parent (self));
  if (parent)
  {
    result = debug_get_the_pipeline (parent);
    gst_object_unref (parent);
  }
  else
  {
    result = self;
    gst_object_ref (self);
  }
  return result;
}

GstElement *debug_get_sink (GstElement *pipeline)
{
  GstIterator *it = NULL;
  GstElement *item = NULL;
  gboolean done = FALSE;
  it = gst_bin_iterate_sinks (GST_BIN (pipeline));
  while (!done) {
    switch (gst_iterator_next (it, (gpointer *) &item)) {
      case GST_ITERATOR_OK:
        if (TRUE)
          done = TRUE;
        else
        {
          gst_object_unref (item);
          item = NULL;
        }
        break;
      case GST_ITERATOR_RESYNC:
        gst_iterator_resync (it);
        break;
      case GST_ITERATOR_ERROR:
        done = TRUE;
        break;
      case GST_ITERATOR_DONE:
        done = TRUE;
        item = NULL;
        break;
    }
  }
  gst_iterator_free (it);
  return item;
}

void
gst_avsynth_video_filter_framegetter (void *data)
{
  /* FIXME: normal implementation */
  GstAVSynthVideoFilter *avsynth_video_filter;
  gboolean stop = FALSE;
  gboolean push = TRUE;
  gint64 clip_start, clip_end, framenum;

  /* FIXME: Uh...shouldn't i use typecast macro here? */
  avsynth_video_filter = (GstAVSynthVideoFilter *) data;
  gst_object_ref (avsynth_video_filter);

  while (!stop)
  {
    PVideoFrame vf = NULL;
    GstBuffer *outbuf;

    GST_DEBUG_OBJECT (avsynth_video_filter, "Calling GetFrame()");

    /* Don't let anyone swap the filter from under us while we're working */
    GST_DEBUG_OBJECT (avsynth_video_filter, "Locking impl_mutex");
    g_mutex_lock (avsynth_video_filter->impl_mutex);

    /* Store it, we'll need it later */
    framenum = avsynth_video_filter->segment.time;

    vf = avsynth_video_filter->impl->GetFrame(framenum, avsynth_video_filter->env);

    GST_DEBUG_OBJECT (avsynth_video_filter, "GetFrame() returned");

    /* Update last videoinfo */
    avsynth_video_filter->vi = avsynth_video_filter->impl->GetVideoInfo();

    outbuf = gst_avsynth_ivf_to_buf (vf, avsynth_video_filter->vi);

    GST_BUFFER_OFFSET (outbuf) = avsynth_video_filter->segment.time++;

    g_mutex_unlock (avsynth_video_filter->impl_mutex);
    GST_DEBUG_OBJECT (avsynth_video_filter, "Unlocked impl_mutex");

    GST_DEBUG_OBJECT (avsynth_video_filter, "Cleaning all caches");
    
    for (guint i = 0; i < avsynth_video_filter->sinks->len; i++)
    {
      AVSynthSink *sink = NULL;
      GST_DEBUG_OBJECT (avsynth_video_filter, "Obtaining cache #%d", i);
      sink = (AVSynthSink *) g_ptr_array_index (avsynth_video_filter->sinks, i);
      GST_DEBUG_OBJECT (avsynth_video_filter, "Calling ClearUntouched() for cache #%d", i);
      sink->cache->ClearUntouched();
      GST_DEBUG_OBJECT (avsynth_video_filter, "Returned from ClearUntouched() for cache #%d", i);
      g_mutex_lock (sink->sinkmutex);
      sink->firstcall = TRUE;
      sink->maxframeshift = sink->maxframeshift < sink->minframe - framenum ? sink->minframe - framenum : sink->maxframeshift;
      g_mutex_unlock (sink->sinkmutex);
    }

    GST_DEBUG_OBJECT (avsynth_video_filter, "Pushing a frame downstream");

    /* If the other thread wants us to pause, this is a good place to do so*/
    g_mutex_lock (avsynth_video_filter->stop_mutex);
    while (!(stop = avsynth_video_filter->stop) && avsynth_video_filter->pause)
    {
      g_cond_wait (avsynth_video_filter->pause_cond, avsynth_video_filter->stop_mutex);
    }
    g_mutex_unlock (avsynth_video_filter->stop_mutex);
 
    if (G_UNLIKELY (stop))
      break;
    g_mutex_lock (avsynth_video_filter->stop_mutex);
    if (G_UNLIKELY (avsynth_video_filter->newsegment))
    {
      gint64 stop;
      GstEvent *newsegevent;
      avsynth_video_filter->newsegment = FALSE;
      /* for deriving a stop position for the playback segment from the seek
       * segment, we must take the duration when the stop is not set */
      if ((stop = avsynth_video_filter->segment.stop) == -1)
        stop = avsynth_video_filter->segment.duration;
  
      GST_DEBUG_OBJECT (avsynth_video_filter, "Sending newsegment from %" G_GINT64_FORMAT
          " to %" G_GINT64_FORMAT, avsynth_video_filter->segment.start, stop);
  
      /* now replace the old segment so that we send it in the stream thread the
       * next time it is scheduled. */
      /*if (src->priv->start_segment)
        gst_event_unref (src->priv->start_segment);*/
      if (avsynth_video_filter->segment.rate >= 0.0) {
        /* forward, we send data from last_stop to stop */
        newsegevent =
            gst_event_new_new_segment_full (FALSE,
            avsynth_video_filter->segment.rate, avsynth_video_filter->segment.applied_rate, avsynth_video_filter->segment.format,
            avsynth_video_filter->segment.last_stop, stop, avsynth_video_filter->segment.time);
      } else {
        /* reverse, we send data from last_stop to start */
        newsegevent =
            gst_event_new_new_segment_full (FALSE,
            avsynth_video_filter->segment.rate, avsynth_video_filter->segment.applied_rate, avsynth_video_filter->segment.format,
            avsynth_video_filter->segment.start, avsynth_video_filter->segment.last_stop, avsynth_video_filter->segment.time);
      }
      gst_pad_push_event (avsynth_video_filter->srcpad, newsegevent);
    }
    push = TRUE;
    if (
      G_UNLIKELY (
        !gst_segment_clip (&avsynth_video_filter->segment, GST_FORMAT_DEFAULT, GST_BUFFER_OFFSET (outbuf), -1, &clip_start, &clip_end)
        || (
          (
            (gint64) GST_BUFFER_OFFSET (outbuf)
          )
          <
          clip_start
        )
      )
    )
    {
      GST_DEBUG_OBJECT (avsynth_video_filter,
          "Not pushing a frame %" G_GINT64_FORMAT
          ", segment %" G_GINT64_FORMAT
          " - %" G_GINT64_FORMAT
          ", %" G_GINT64_FORMAT
          " %c %" G_GINT64_FORMAT,
          GST_BUFFER_OFFSET (outbuf),
          avsynth_video_filter->segment.start,
          avsynth_video_filter->segment.stop,
          (gint64) GST_BUFFER_OFFSET (outbuf), 
          (
            (
              (gint64) GST_BUFFER_OFFSET (outbuf)
            )
            < clip_start
          )
          ? '<' :
          (
            (
              (gint64) GST_BUFFER_OFFSET (outbuf)
            )
            > clip_start ? '>' : '='
          ),
          clip_start);
      push = FALSE;
    }
    g_mutex_unlock (avsynth_video_filter->stop_mutex);
    if (G_UNLIKELY (!push))
    {
      gst_buffer_unref (outbuf);
      continue;
    }
    gst_pad_push (avsynth_video_filter->srcpad, outbuf);
    if (clip_start == 50)
    {
      GST_DEBUG_OBJECT (avsynth_video_filter, "Locking impl_mutex");
      g_mutex_lock (avsynth_video_filter->impl_mutex);
      avsynth_video_filter->segment.time = 30;
      g_mutex_unlock (avsynth_video_filter->impl_mutex);
      GST_DEBUG_OBJECT (avsynth_video_filter, "Unlocked impl_mutex");
 
      for (guint i = 0; i < avsynth_video_filter->sinks->len; i++)
      {
        AVSynthSink *sink = NULL;
        sink = (AVSynthSink *) g_ptr_array_index (avsynth_video_filter->sinks, i);
        g_mutex_lock (sink->sinkmutex);
        sink->seek = TRUE;
        g_mutex_unlock (sink->sinkmutex);
      }

    }
    else if (clip_start == 100 || clip_start == 400 || clip_start == 700)
    {
      GstElement *pipeline;
      GstStructure *s;
      GstMessage *m;
      GstBus *b;
    
      pipeline = debug_get_the_pipeline (GST_ELEMENT (avsynth_video_filter));

      if (clip_start == 100)
        s = gst_structure_new ("GstAVSynthSeek",
            "rate", G_TYPE_DOUBLE, (gdouble) 1.0,
            "format", G_TYPE_INT, (gint) GST_FORMAT_DEFAULT,
            "flags", G_TYPE_INT, (gint) GST_SEEK_FLAG_FLUSH,
            "cur_type", G_TYPE_INT, (gint) GST_SEEK_TYPE_SET,
            "cur", G_TYPE_INT64, (gint64) 300,
            "stop_type", G_TYPE_INT, (gint) GST_SEEK_TYPE_NONE,
            "stop", G_TYPE_INT64, (gint64) -1,
            NULL);
      else if (clip_start == 400)
        s = gst_structure_new ("GstAVSynthSeek",
            "rate", G_TYPE_DOUBLE, 1.0,
            "format", G_TYPE_INT, GST_FORMAT_DEFAULT,
            "flags", G_TYPE_INT, 0,
            "cur_type", G_TYPE_INT, GST_SEEK_TYPE_SET,
            "cur", G_TYPE_INT64, 500,
            "stop_type", G_TYPE_INT, GST_SEEK_TYPE_NONE,
            "stop", G_TYPE_INT64, -1,
            NULL);
      else if (clip_start == 700)
        s = gst_structure_new ("GstAVSynthSeek",
            "rate", G_TYPE_DOUBLE, 1.0,
            "format", G_TYPE_INT, GST_FORMAT_DEFAULT,
            "flags", G_TYPE_INT, GST_SEEK_FLAG_FLUSH,
            "cur_type", G_TYPE_INT, GST_SEEK_TYPE_SET,
            "cur", G_TYPE_INT64, 200,
            "stop_type", G_TYPE_INT, GST_SEEK_TYPE_NONE,
            "stop", G_TYPE_INT64, -1,
            NULL);
    
      m = gst_message_new_custom (GST_MESSAGE_ELEMENT, GST_OBJECT (avsynth_video_filter), s);
    
      b = gst_element_get_bus (pipeline);

      gst_bus_post (b, m);

      gst_object_unref (pipeline);
      gst_object_unref (b);
    }
  }
  GST_DEBUG_OBJECT (avsynth_video_filter, "Stopped");
  gst_object_unref (avsynth_video_filter);
}

GstPadLinkReturn gst_avsynth_video_filter_sink_link(GstPad *pad, GstPad *peer)
{
  return GST_PAD_LINK_OK;
}

void
gst_avsynth_video_filter_init (GstAVSynthVideoFilter *avsynth_video_filter)
{
  AvisynthPluginInitFunc init_func = NULL;
  GstAVSynthVideoFilterClass *oclass;
  gchar *full_filename = NULL;
  gint sinkcount = 0;

  oclass = (GstAVSynthVideoFilterClass *) (G_OBJECT_GET_CLASS (avsynth_video_filter));

  avsynth_video_filter->env = new ScriptEnvironment(avsynth_video_filter);

  full_filename = g_filename_from_utf8 (oclass->filename, -1, NULL, NULL, NULL);

  avsynth_video_filter->plugin = g_module_open (full_filename, (GModuleFlags) G_MODULE_BIND_LAZY);

  g_free (full_filename);
  
  GST_LOG ("Getting AvisynthPluginInit2...");
  if (!g_module_symbol (avsynth_video_filter->plugin, "AvisynthPluginInit2", (gpointer *) &init_func))
  {
    GST_LOG ("Failed to get AvisynthPluginInit2. Getting AvisynthPluginInit2@4...");
    if (!g_module_symbol (avsynth_video_filter->plugin, "AvisynthPluginInit2@4", (gpointer *) &init_func))
    {
      GST_ERROR ("Failed to get AvisynthPluginInits. Something is wrong here...");
    }
  }

  init_func (avsynth_video_filter->env);

  /* setup pads */

  avsynth_video_filter->sinks = g_ptr_array_new ();

  for (guint i = 0; i < oclass->properties->len; i++)
  {
    gchar *padname = NULL;
    AVSynthSink *sink = NULL;

    AVSynthVideoFilterParam *param = (AVSynthVideoFilterParam*) g_ptr_array_index (oclass->properties, i);
    if (param->param_type != 'c')
      continue;
    if (param->param_mult != 0)
      GST_FIXME ("Can't create an array of sinkpads for %c%c", param->param_type, param->param_mult);

    sink = g_new0 (AVSynthSink, 1);

    padname = g_strdup_printf ("sink%d", sinkcount);
    sink->sinkpad = gst_pad_new_from_template (oclass->sinktempl, padname);
    g_free (padname);

    g_ptr_array_add (avsynth_video_filter->sinks, (gpointer) sink);

    gst_pad_set_setcaps_function (sink->sinkpad,
        GST_DEBUG_FUNCPTR (gst_avsynth_video_filter_setcaps));
    gst_pad_set_event_function (sink->sinkpad,
        GST_DEBUG_FUNCPTR (gst_avsynth_video_filter_sink_event));
    gst_pad_set_chain_function (sink->sinkpad,
        GST_DEBUG_FUNCPTR (gst_avsynth_video_filter_chain));

    gst_pad_set_link_function (sink->sinkpad,
        GST_DEBUG_FUNCPTR (gst_avsynth_video_filter_sink_link));

    gst_element_add_pad (GST_ELEMENT (avsynth_video_filter), sink->sinkpad);
    sink->cache = NULL;
    sink->sinkmutex = g_mutex_new ();
    sink->firstcall = TRUE;
    g_object_set_data (G_OBJECT (sink->sinkpad), "sinkstruct", (gpointer) sink);
    sinkcount++;
  }

  avsynth_video_filter->srcpad = gst_pad_new_from_template (oclass->srctempl, "src");
  gst_pad_use_fixed_caps (avsynth_video_filter->srcpad);
  gst_pad_set_event_function (avsynth_video_filter->srcpad,
      GST_DEBUG_FUNCPTR (gst_avsynth_video_filter_src_event));
  gst_pad_set_query_function (avsynth_video_filter->srcpad,
      GST_DEBUG_FUNCPTR (gst_avsynth_video_filter_query));
  gst_element_add_pad (GST_ELEMENT (avsynth_video_filter), avsynth_video_filter->srcpad);

  gst_segment_init (&avsynth_video_filter->segment, GST_FORMAT_UNDEFINED);

  avsynth_video_filter->framegetter_mutex = g_new (GStaticRecMutex, 1);
  g_static_rec_mutex_init (avsynth_video_filter->framegetter_mutex);

  avsynth_video_filter->framegetter = gst_task_create (gst_avsynth_video_filter_framegetter, (gpointer) avsynth_video_filter);
  gst_task_set_lock (avsynth_video_filter->framegetter, avsynth_video_filter->framegetter_mutex);

  avsynth_video_filter->stop = FALSE;
  avsynth_video_filter->pause = FALSE;
  avsynth_video_filter->stop_mutex = g_mutex_new ();
  avsynth_video_filter->pause_cond = g_cond_new ();

  avsynth_video_filter->impl_mutex = g_mutex_new ();

  avsynth_video_filter->args = new PAVSValue[oclass->properties->len];
  for (guint i = 0; i < oclass->properties->len; i++)
    avsynth_video_filter->args[i] = NULL;

  avsynth_video_filter->uninitialized = TRUE;

  avsynth_video_filter->segment.time = 0;
}

void
gst_avsynth_video_filter_finalize (GObject * object)
{
  GstAVSynthVideoFilter *avsynth_video_filter = (GstAVSynthVideoFilter *) object;

  g_module_close (avsynth_video_filter->plugin);
  /* TODO: free the stuff */

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

gboolean
gst_avsynth_video_filter_query (GstPad * pad, GstQuery * query)
{
  GstAVSynthVideoFilter *avsynth_video_filter;
  gboolean res;

  GST_DEBUG_OBJECT (pad, "Processing a query");
 
  avsynth_video_filter = (GstAVSynthVideoFilter *) gst_pad_get_parent (pad);

  res = FALSE;

  switch (GST_QUERY_TYPE (query))
  {
    case GST_QUERY_POSITION:
    {
      GstFormat format;

      gst_query_parse_position (query, &format, NULL);
      switch (format) {
        case GST_FORMAT_PERCENT:
        {
          gint64 percent;
          gint64 position;
          gint64 duration;
          
          g_mutex_lock (avsynth_video_filter->stop_mutex);
          position = avsynth_video_filter->segment.last_stop;
          duration = avsynth_video_filter->segment.duration;
          g_mutex_unlock (avsynth_video_filter->stop_mutex);

          if (position != -1 && duration != -1) {
            if (position < duration)
              percent = gst_util_uint64_scale (GST_FORMAT_PERCENT_MAX, position,
                  duration);
            else
              percent = GST_FORMAT_PERCENT_MAX;
          } else
            percent = -1;

          gst_query_set_position (query, GST_FORMAT_PERCENT, percent);
          res = TRUE;
          break;
        }
        default:
        {
          gint64 position;

          g_mutex_lock (avsynth_video_filter->stop_mutex);
          position = avsynth_video_filter->segment.last_stop;
          g_mutex_unlock (avsynth_video_filter->stop_mutex);

          if (position != -1) {
            /* convert to requested format */
            res =
                gst_pad_query_convert (avsynth_video_filter->srcpad, GST_FORMAT_DEFAULT,
                position, &format, &position);
          } else
            res = TRUE;

          gst_query_set_position (query, format, position);
          break;
        }
      }
      break; 
    }
    case GST_QUERY_DURATION:
    {
      GstFormat format;

      gst_query_parse_duration (query, &format, NULL);

      GST_DEBUG_OBJECT (avsynth_video_filter, "duration query in format %s",
          gst_format_get_name (format));

      switch (format) {
        case GST_FORMAT_PERCENT:
          gst_query_set_duration (query, GST_FORMAT_PERCENT,
              GST_FORMAT_PERCENT_MAX);
          res = TRUE;
          break;
        default:
        {
          gint64 duration;

          /* this is the duration as configured by the subclass. */
          g_mutex_lock (avsynth_video_filter->stop_mutex);
          duration = avsynth_video_filter->segment.duration;
          g_mutex_unlock (avsynth_video_filter->stop_mutex);

          if (duration != -1) {
            /* convert to requested format, if this fails, we have a duration
             * but we cannot answer the query, we must return FALSE. */
            res =
                gst_pad_query_convert (avsynth_video_filter->srcpad, GST_FORMAT_DEFAULT,
                duration, &format, &duration);
          } else {
            /* The subclass did not configure a duration, we assume that the
             * media has an unknown duration then and we return TRUE to report
             * this. Note that this is not the same as returning FALSE, which
             * means that we cannot report the duration at all. */
            res = TRUE;
          }
          gst_query_set_duration (query, format, duration);
          break;
        }
      }
      break;
    }
    case GST_QUERY_CONVERT:
    {
      GstFormat src_fmt, dest_fmt;
      gint64 src_val, dest_val;

      gst_query_parse_convert (query, &src_fmt, &src_val, &dest_fmt, &dest_val);
      if (res =
              gst_avsynth_video_filter_src_convert (pad, src_fmt, src_val, &dest_fmt,
                  &dest_val))
        gst_query_set_convert (query, src_fmt, src_val, dest_fmt, dest_val);
    }
    default:
      res = FALSE;
  }

  /* FIXME: query implementation */
  /* res = gst_pad_query_default (pad, query); */
  /*
  for (guint i = 0; i < avsynth_video_filter->sinkpads->len; i++)
  {
    GstPad *sinkpad = (GstPad *) g_ptr_array_index (avsynth_video_filter->sinkpads, i);
    // Forward the query to the peer
    res = gst_pad_peer_query (sinkpad, query);
  }
  */
  gst_object_unref (avsynth_video_filter);

  GST_DEBUG_OBJECT (pad, "Finished processing a query");

  return res;
}

/* From GstAdder */

typedef struct
{
  GstEvent *event;
  gboolean flush;
} EventData;

static gboolean
forward_event_func (GstPad * pad, GValue * ret, EventData * data)
{
  GstEvent *event = data->event;

  gst_event_ref (event);
  GST_LOG_OBJECT (pad, "About to send event %s", GST_EVENT_TYPE_NAME (event));
  if (!gst_pad_push_event (pad, event)) {
    g_value_set_boolean (ret, FALSE);
    GST_WARNING_OBJECT (pad, "Sending event  %p (%s) failed.",
        event, GST_EVENT_TYPE_NAME (event));
    /* quick hack to unflush the pads, ideally we need a way to just unflush
     * this single collect pad */
    if (data->flush)
      gst_pad_send_event (pad, gst_event_new_flush_stop ());
  } else {
    GST_LOG_OBJECT (pad, "Sent event  %p (%s).",
        event, GST_EVENT_TYPE_NAME (event));
  }
  gst_object_unref (pad);

  /* continue on other pads, even if one failed */
  return TRUE;
}

/* forwards the event to all sinkpads, takes ownership of the
 * event
 *
 * Returns: TRUE if the event could be forwarded on all
 * sinkpads.
 */
static gboolean
forward_event (GstAVSynthVideoFilter * filter, GstEvent * event, gboolean flush)
{
  gboolean ret;
  GstIterator *it;
  GstIteratorResult ires;
  GValue vret = { 0 };
  EventData data;

  GST_LOG_OBJECT (filter, "Forwarding event %p (%s)", event,
      GST_EVENT_TYPE_NAME (event));

  ret = TRUE;
  data.event = event;
  data.flush = flush;

  g_value_init (&vret, G_TYPE_BOOLEAN);
  g_value_set_boolean (&vret, TRUE);
  it = gst_element_iterate_sink_pads (GST_ELEMENT_CAST (filter));
  while (TRUE) {
    ires = gst_iterator_fold (it, (GstIteratorFoldFunction) forward_event_func,
        &vret, &data);
    switch (ires) {
      case GST_ITERATOR_RESYNC:
        GST_WARNING ("resync");
        gst_iterator_resync (it);
        g_value_set_boolean (&vret, TRUE);
        break;
      case GST_ITERATOR_OK:
      case GST_ITERATOR_DONE:
        ret = g_value_get_boolean (&vret);
        goto done;
      default:
        ret = FALSE;
        goto done;
    }
  }
done:
  gst_iterator_free (it);
  GST_LOG_OBJECT (filter, "Forwarded event %p (%s), ret=%d", event,
      GST_EVENT_TYPE_NAME (event), ret);
  gst_event_unref (event);

  return ret;
}

static gboolean
gst_avsynth_video_filter_src_convert (GstPad * pad,
    GstFormat src_format, gint64 src_value,
    GstFormat * dest_format, gint64 * dest_value)
{
  gboolean res = TRUE;
  GstAVSynthVideoFilter *avsynth_video_filter;

  if (src_format == *dest_format) {
    *dest_value = src_value;
    return TRUE;
  }

  avsynth_video_filter = (GstAVSynthVideoFilter *) gst_pad_get_parent (pad);

  GST_DEBUG_OBJECT (avsynth_video_filter, "Locking impl_mutex");
  g_mutex_lock (avsynth_video_filter->impl_mutex);

  GST_DEBUG ("src convert");
  switch (src_format) {
    case GST_FORMAT_DEFAULT:
      switch (*dest_format) {
        case GST_FORMAT_TIME:
          *dest_value = gst_util_uint64_scale (src_value,
              avsynth_video_filter->vi.fps_denominator * GST_SECOND, avsynth_video_filter->vi.fps_numerator);
          break;
        default:
          res = FALSE;
      }
      break;
    case GST_FORMAT_TIME:
      switch (*dest_format) {
        case GST_FORMAT_DEFAULT:
        {
          *dest_value = gst_util_uint64_scale (src_value,
              avsynth_video_filter->vi.fps_numerator, avsynth_video_filter->vi.fps_denominator * GST_SECOND);
          break;
        }
        default:
          res = FALSE;
          break;
      }
      break;
    default:
      res = FALSE;
      break;
  }
  g_mutex_unlock (avsynth_video_filter->impl_mutex);
  GST_DEBUG_OBJECT (avsynth_video_filter, "Unlocked impl_mutex");

  gst_object_unref (avsynth_video_filter);

  return res;
}

static gboolean
gst_avsynth_video_filter_prepare_seek_segment (GstAVSynthVideoFilter * avsynth_video_filter,
    GstEvent * event, GstSegment * segment)
{
  /* By default, we try one of 2 things:
   *   - For absolute seek positions, convert the requested position to our 
   *     configured processing format and place it in the output segment \
   *   - For relative seek positions, convert our current (input) values to the
   *     seek format, adjust by the relative seek offset and then convert back to
   *     the processing format
   */
  GstSeekType cur_type, stop_type;
  gint64 cur, stop;
  GstSeekFlags flags;
  GstFormat seek_format, dest_format;
  gdouble rate;
  gboolean update;
  gboolean res = TRUE;

  gst_event_parse_seek (event, &rate, &seek_format, &flags,
      &cur_type, &cur, &stop_type, &stop);
  dest_format = segment->format;

  if (seek_format == dest_format) {
    gst_segment_set_seek (segment, rate, seek_format, flags,
        cur_type, cur, stop_type, stop, &update);
    return TRUE;
  }

  if (cur_type != GST_SEEK_TYPE_NONE) {
    /* FIXME: Handle seek_cur & seek_end by converting the input segment vals */
    res =
        gst_pad_query_convert (avsynth_video_filter->srcpad, seek_format, cur, &dest_format,
        &cur);
    cur_type = GST_SEEK_TYPE_SET;
  }

  if (res && stop_type != GST_SEEK_TYPE_NONE) {
    /* FIXME: Handle seek_cur & seek_end by converting the input segment vals */
    res =
        gst_pad_query_convert (avsynth_video_filter->srcpad, seek_format, stop, &dest_format,
        &stop);
    stop_type = GST_SEEK_TYPE_SET;
  }

  /* And finally, configure our output segment in the desired format */
  gst_segment_set_seek (segment, rate, dest_format, flags, cur_type, cur,
      stop_type, stop, &update);

  if (!res)
    goto no_format;

  return res;

no_format:
  {
    GST_DEBUG_OBJECT (avsynth_video_filter, "undefined format given, seek aborted.");
    return FALSE;
  }
}

/* This is a rip off basesrc class */
static gboolean
avsynth_video_filter_perform_seek (GstAVSynthVideoFilter * avsynth_video_filter, GstEvent * event, gboolean unlock)
{
  gboolean res = TRUE;
  gdouble rate;
  GstFormat seek_format, dest_format;
  GstSeekFlags flags;
  GstSeekType cur_type, stop_type;
  gint64 cur, stop;
  gboolean flush;
  gboolean update;
  gboolean relative_seek = FALSE;
  gboolean seekseg_configured = FALSE;
  GstSegment seeksegment;
  guint32 seqnum;
  GstEvent *tevent;

  GST_DEBUG_OBJECT (avsynth_video_filter, "doing seek");

  dest_format = avsynth_video_filter->segment.format;

  if (event) {
    gst_event_parse_seek (event, &rate, &seek_format, &flags,
        &cur_type, &cur, &stop_type, &stop);

    relative_seek = SEEK_TYPE_IS_RELATIVE (cur_type) ||
        SEEK_TYPE_IS_RELATIVE (stop_type);

    if (dest_format != seek_format && !relative_seek) {
      /* If we have an ABSOLUTE position (SEEK_SET only), we can convert it
       * here before taking the stream lock, otherwise we must convert it later,
       * once we have the stream lock and can read the last configures segment
       * start and stop positions */
      gst_segment_init (&seeksegment, dest_format);

      if (!gst_avsynth_video_filter_prepare_seek_segment (avsynth_video_filter, event, &seeksegment))
        goto prepare_failed;

      seekseg_configured = TRUE;
    }

    flush = flags & GST_SEEK_FLAG_FLUSH;
    seqnum = gst_event_get_seqnum (event);
  } else {
    /* FIXME: Unlikey to happen? */
    flush = FALSE;
    /* get next seqnum */
    seqnum = gst_util_seqnum_next ();
  }

  /* send flush start */
  if (flush) {
    tevent = gst_event_new_flush_start ();
    gst_event_set_seqnum (tevent, seqnum);
    gst_pad_push_event (avsynth_video_filter->srcpad, tevent);
  } else {
    g_mutex_lock (avsynth_video_filter->stop_mutex);
    avsynth_video_filter->stop = TRUE;
    g_mutex_unlock (avsynth_video_filter->stop_mutex);
  }

  /* unblock streaming thread. */
  /* FIXME: replace with something suitable
  gst_base_src_set_flushing (src, TRUE, FALSE, unlock, &playing); */

  /* grab streaming lock, this should eventually be possible, either
   * because the task is paused, our streaming thread stopped 
   * or because our peer is flushing. */
  GST_PAD_STREAM_LOCK (avsynth_video_filter->srcpad);
/*
  if (G_UNLIKELY (avsynth_video_filter->priv->seqnum == seqnum)) {
    *//* we have seen this event before, issue a warning for now *//*
    GST_WARNING_OBJECT (src, "duplicate event found %" G_GUINT32_FORMAT,
        seqnum);
  } else {
    src->priv->seqnum = seqnum;
    GST_DEBUG_OBJECT (src, "seek with seqnum %" G_GUINT32_FORMAT, seqnum);
  }
*/

  /* FIXME: replace with something suitable
  gst_base_src_set_flushing (src, FALSE, playing, unlock, NULL); */
  
  /* If we configured the seeksegment above, don't overwrite it now. Otherwise
   * copy the current segment info into the temp segment that we can actually
   * attempt the seek with. We only update the real segment if the seek suceeds. */
  if (!seekseg_configured) {
    g_mutex_lock (avsynth_video_filter->stop_mutex);
    memcpy (&seeksegment, &avsynth_video_filter->segment, sizeof (GstSegment));

    /* now configure the final seek segment */
    if (event) {
      if (avsynth_video_filter->segment.format != seek_format) {
        /* OK, here's where we give the subclass a chance to convert the relative
         * seek into an absolute one in the processing format. We set up any
         * absolute seek above, before taking the stream lock. */
        if (!gst_avsynth_video_filter_prepare_seek_segment (avsynth_video_filter, event, &seeksegment)) {
          GST_DEBUG_OBJECT (avsynth_video_filter, "Preparing the seek failed after flushing. "
              "Aborting seek");
          res = FALSE;
        }
      } else {
        /* The seek format matches our processing format, no need to ask the
         * the subclass to configure the segment. */
        gst_segment_set_seek (&seeksegment, rate, seek_format, flags,
            cur_type, cur, stop_type, stop, &update);
      }
    }
    g_mutex_unlock (avsynth_video_filter->stop_mutex);
    /* Else, no seek event passed, so we're just (re)starting the 
       current segment. */
  }

  if (res) {
    GST_DEBUG_OBJECT (avsynth_video_filter, "segment configured from %" G_GINT64_FORMAT
        " to %" G_GINT64_FORMAT ", position %" G_GINT64_FORMAT,
        seeksegment.start, seeksegment.stop, seeksegment.last_stop);

    /* do the seek, segment.last_stop contains the new position. */
    /* update our offset if the start/stop position was updated */
    if (seeksegment.format == GST_FORMAT_DEFAULT) {
      seeksegment.time = seeksegment.start;
    } else if (seeksegment.start == 0) {
      seeksegment.time = 0;
    } else {
      res = FALSE;
      GST_INFO_OBJECT (avsynth_video_filter, "Can't do a seek");
    }
  }

  /* and prepare to continue streaming */
  if (flush) {
    tevent = gst_event_new_flush_stop ();
    gst_event_set_seqnum (tevent, seqnum);
    /* send flush stop, peer will accept data and events again. We
     * are not yet providing data as we still have the STREAM_LOCK. */
    gst_pad_push_event (avsynth_video_filter->srcpad, tevent);
  } else if (res /*&& avsynth_video_filter->data.ABI.running*/) {
    /* we are running the current segment and doing a non-flushing seek, 
     * close the segment first based on the last_stop. */
    GST_DEBUG_OBJECT (avsynth_video_filter, "closing running segment %" G_GINT64_FORMAT
        " to %" G_GINT64_FORMAT, avsynth_video_filter->segment.start, avsynth_video_filter->segment.last_stop);

    /* queue the segment for sending in the stream thread */
/*
    if (src->priv->close_segment)
      gst_event_unref (src->priv->close_segment);
    src->priv->close_segment =
        gst_event_new_new_segment_full (TRUE,
        src->segment.rate, src->segment.applied_rate, src->segment.format,
        src->segment.start, src->segment.last_stop, src->segment.time);
    gst_event_set_seqnum (src->priv->close_segment, seqnum);
*/  
  }

  /* The subclass must have converted the segment to the processing format 
   * by now */
  if (res && seeksegment.format != dest_format) {
    GST_DEBUG_OBJECT (avsynth_video_filter, "Failed to prepare a seek segment "
        "in the correct format. Aborting seek.");
    res = FALSE;
  }

  /* if successfull seek, we update our real segment and push
   * out the new segment. */
  if (res) {
    g_mutex_lock (avsynth_video_filter->stop_mutex);
    memcpy (&avsynth_video_filter->segment, &seeksegment, sizeof (GstSegment));

    if (avsynth_video_filter->segment.flags & GST_SEEK_FLAG_SEGMENT) {
      GstMessage *message;

      message = gst_message_new_segment_start (GST_OBJECT (avsynth_video_filter),
          avsynth_video_filter->segment.format, avsynth_video_filter->segment.last_stop);
      gst_message_set_seqnum (message, seqnum);

      gst_element_post_message (GST_ELEMENT (avsynth_video_filter), message);
    }

    /* for deriving a stop position for the playback segment from the seek
     * segment, we must take the duration when the stop is not set */
    if ((stop = avsynth_video_filter->segment.stop) == -1)
      stop = avsynth_video_filter->segment.duration;

    GST_DEBUG_OBJECT (avsynth_video_filter, "Sending newsegment from %" G_GINT64_FORMAT
        " to %" G_GINT64_FORMAT, avsynth_video_filter->segment.start, stop);

    /* now replace the old segment so that we send it in the stream thread the
     * next time it is scheduled. */
    /*
    if (src->priv->start_segment)
      gst_event_unref (src->priv->start_segment);
    if (src->segment.rate >= 0.0) {
      *//* forward, we send data from last_stop to stop *//*
      src->priv->start_segment =
          gst_event_new_new_segment_full (FALSE,
          src->segment.rate, src->segment.applied_rate, src->segment.format,
          src->segment.last_stop, stop, src->segment.time);
    } else {
      *//* reverse, we send data from last_stop to start *//*
      src->priv->start_segment =
          gst_event_new_new_segment_full (FALSE,
          src->segment.rate, src->segment.applied_rate, src->segment.format,
          src->segment.start, src->segment.last_stop, src->segment.time);
    }
    gst_event_set_seqnum (src->priv->start_segment, seqnum);
    */
    avsynth_video_filter->newsegment = TRUE;
    g_mutex_unlock (avsynth_video_filter->stop_mutex);
  }

  /* and restart the task in case it got paused explicitely or by
   * the FLUSH_START event we pushed out. */
  //GST_DEBUG_OBJECT (avsynth_video_filter, "Locking impl_mutex");
  //g_mutex_lock (avsynth_video_filter->impl_mutex);
  if (G_UNLIKELY ((gst_task_get_state (avsynth_video_filter->framegetter) == GST_TASK_STOPPED) && avsynth_video_filter->impl))
  {
    GST_DEBUG_OBJECT (avsynth_video_filter, "Starting framegetter");
    gst_task_start (avsynth_video_filter->framegetter);
  }
  //g_mutex_unlock (avsynth_video_filter->impl_mutex);
  //GST_DEBUG_OBJECT (avsynth_video_filter, "Unlocked impl_mutex");

  /* and release the lock again so we can continue streaming */
  GST_PAD_STREAM_UNLOCK (avsynth_video_filter->srcpad);

  for (guint i = 0; i < avsynth_video_filter->sinks->len; i++)
  {
    AVSynthSink *sink = NULL;
    sink = (AVSynthSink *) g_ptr_array_index (avsynth_video_filter->sinks, i);
    GST_DEBUG_OBJECT (avsynth_video_filter, "Video cache %p: Locking sinkmutex", (gpointer) sink->cache);
    g_mutex_lock (sink->sinkmutex);
    sink->eos = FALSE;
    sink->seek = TRUE;
    g_mutex_unlock (sink->sinkmutex);
    GST_DEBUG_OBJECT (avsynth_video_filter, "Video cache %p: Unlocked sinkmutex", (gpointer) sink->cache);
  }

  return res;

  /* ERROR */
prepare_failed:
  GST_DEBUG_OBJECT (avsynth_video_filter, "Preparing the seek failed before flushing. "
      "Aborting seek");
  return FALSE;
}


gboolean
gst_avsynth_video_filter_src_event (GstPad * pad, GstEvent * event)
{
  GstAVSynthVideoFilter *avsynth_video_filter;
  gboolean res = FALSE;

  GST_DEBUG_OBJECT (pad, "Processing an event");

  avsynth_video_filter = (GstAVSynthVideoFilter *) gst_pad_get_parent (pad);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_QOS:
    {
      /* It seems appropriate to block any QoS requests - we don't want to skip
       * frames, don't we?
       */
      break;
    }
    case GST_EVENT_SEEK:
    {
      gboolean started;

      GST_OBJECT_LOCK (avsynth_video_filter->srcpad);
      if (GST_PAD_ACTIVATE_MODE (avsynth_video_filter->srcpad) == GST_ACTIVATE_PULL)
        goto wrong_mode;
      started = GST_PAD_ACTIVATE_MODE (avsynth_video_filter->srcpad) == GST_ACTIVATE_PUSH;
      GST_OBJECT_UNLOCK (avsynth_video_filter->srcpad);

      if (started) {
        GST_DEBUG_OBJECT (avsynth_video_filter, "performing seek");
        /* when we are running in push mode, we can execute the
         * seek right now, we need to unlock. */
        res = avsynth_video_filter_perform_seek (avsynth_video_filter, event, TRUE);
      } else {
        /* GstEvent **event_p; */

        /* else we store the event and execute the seek when we
         * get activated */
        /* For now let's do it right away 
        GST_OBJECT_LOCK (avsynth_video_filter);
        GST_DEBUG_OBJECT (avsynth_video_filter, "queueing seek");
        event_p = &avsynth_video_filter->data.ABI.pending_seek;
        gst_event_replace ((GstEvent **) event_p, event);
        GST_OBJECT_UNLOCK (avsynth_video_filter);
        */ 
        /* assume the seek will work */
        /*res = TRUE;*/
        res = avsynth_video_filter_perform_seek (avsynth_video_filter, event, TRUE);
      }
      break;
    }
    case GST_EVENT_NAVIGATION:
    {
      break;
    }
    case GST_EVENT_LATENCY:
    {
      break;
    }
    case GST_EVENT_STEP:
    {
      break;
    }
    default:
      /* forward upstream */
      res = forward_event (avsynth_video_filter, event, FALSE);
      break;
  }
done:
  gst_object_unref (avsynth_video_filter);

  if (event)
    gst_event_unref (event);

  GST_DEBUG_OBJECT (pad, "Finished processing an event");

  return res;
  /* ERRORS */
convert_error:
  {
    GST_DEBUG_OBJECT (avsynth_video_filter, "could not convert format");
    goto done;
  }
wrong_mode:
  {
    GST_DEBUG_OBJECT (avsynth_video_filter, "cannot perform seek when operating in pull mode");
    GST_OBJECT_UNLOCK (avsynth_video_filter->srcpad);
    res = FALSE;
    goto done;
  }

}

gboolean
gst_avsynth_video_filter_setcaps (GstPad * pad, GstCaps * caps)
{
  GstAVSynthVideoFilter *avsynth_video_filter;
  GstAVSynthVideoFilterClass *oclass;
//  GstStructure *structure;
//  const GValue *par;
//  const GValue *fps;
  gboolean ret = TRUE;
  VideoInfo *vi;
  AVSynthSink *sink = NULL;

  avsynth_video_filter = (GstAVSynthVideoFilter *) (gst_pad_get_parent (pad));
  oclass = (GstAVSynthVideoFilterClass *) (G_OBJECT_GET_CLASS (avsynth_video_filter));
  sink = (AVSynthSink *) g_object_get_data (G_OBJECT (pad), "sinkstruct");

  GST_DEBUG_OBJECT (pad, "setcaps called");

  GST_OBJECT_LOCK (avsynth_video_filter);

  /* So...we've got a new media, with different caps. What to do?
   * As far as i know, AviSynth can't feed dynamically changing media to a
   * filter. Which means that each setcaps after the first one should
   * either fail or recreate the filter. I'll go for recreation, i think.
   */

  vi = g_new0 (VideoInfo, 1);
  gst_avsynth_buf_pad_caps_to_vi (NULL, pad, caps, vi);

  /* New caps -> new VideoInfo -> new videocache */
  if (sink->cache)
  {
    delete sink->cache;
    sink->cache = NULL;
  }
  sink->cache = new GstAVSynthVideoCache(vi, pad);

  GST_DEBUG_OBJECT (avsynth_video_filter, "Locking impl_mutex");
  g_mutex_lock (avsynth_video_filter->impl_mutex);
  if (avsynth_video_filter->impl)
  {
    /* PClip is a weird one, but i think that NULL assignment should
     * make it call the destructor. FIXME: make sure it does.
     */
    avsynth_video_filter->impl = NULL;
    /* Next time we get any data, recreate the filter */
    avsynth_video_filter->uninitialized = TRUE;
  }
  g_mutex_unlock (avsynth_video_filter->impl_mutex);
  GST_DEBUG_OBJECT (avsynth_video_filter, "Unlocked impl_mutex");

  GST_OBJECT_UNLOCK (avsynth_video_filter);

  GST_DEBUG_OBJECT (pad, "setcaps finished");

  gst_object_unref (avsynth_video_filter);

  return ret;
}

gboolean
gst_avsynth_video_filter_sink_event (GstPad * pad, GstEvent * event)
{
  GstAVSynthVideoFilter *avsynth_video_filter;
  GstAVSynthVideoFilterClass *oclass;
  AVSynthSink *sink;
  gboolean ret = FALSE;

  avsynth_video_filter = (GstAVSynthVideoFilter *) gst_pad_get_parent (pad);
  oclass = (GstAVSynthVideoFilterClass *) (G_OBJECT_GET_CLASS (avsynth_video_filter));
  sink = (AVSynthSink *) g_object_get_data (G_OBJECT (pad), "sinkstruct");

  GST_DEBUG_OBJECT (avsynth_video_filter, "Handling %s event",
      GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:
    {
      /* TODO: Collect EOS event. If we are waiting for data from the pad,
       * stop doing so.
       * Wait until we run out of data and send EOS downstream. We don't
       * need to wait for EOS on all streams (if one stream has ended, while
       * others did not, we can't proceed anyways.
       */
      GST_DEBUG_OBJECT (avsynth_video_filter, "Video cache %p: Locking sinkmutex", (gpointer) sink->cache);
      g_mutex_lock (sink->sinkmutex);
      sink->eos = TRUE;
      /* If that pad is waiting for data, wake it up, since there will be
       * no data to wait for.
       */
      if (sink->cache)
        g_cond_signal (sink->cache->vcache_cond);
      g_mutex_unlock (sink->sinkmutex);
      GST_DEBUG_OBJECT (avsynth_video_filter, "Video cache %p: Unlocked sinkmutex", (gpointer) sink->cache);
      ret = TRUE;
      break;
    }
    case GST_EVENT_FLUSH_START:
    {
      GST_DEBUG_OBJECT (avsynth_video_filter, "Video cache %p: Locking sinkmutex", (gpointer) sink->cache);
      g_mutex_lock (sink->sinkmutex);
      sink->flush = TRUE;
      g_mutex_unlock (sink->sinkmutex);
      GST_DEBUG_OBJECT (avsynth_video_filter, "Video cache %p: Unlocked sinkmutex", (gpointer) sink->cache);

      ret = TRUE;//gst_pad_push_event (avsynth_video_filter->srcpad, event);
    }
    case GST_EVENT_FLUSH_STOP:
    {
      GST_DEBUG_OBJECT (avsynth_video_filter, "Video cache %p: Locking sinkmutex", (gpointer) sink->cache);
      g_mutex_lock (sink->sinkmutex);
      if (sink->cache)
        sink->cache->Clear();
      /* FIXME: init segment */
      sink->eos = FALSE;
      sink->flush = FALSE;
      g_mutex_unlock (sink->sinkmutex);
      GST_DEBUG_OBJECT (avsynth_video_filter, "Video cache %p: Unlocked sinkmutex", (gpointer) sink->cache);
      
      ret = TRUE;//gst_pad_push_event (avsynth_video_filter->srcpad, event);
      break;
    }
    case GST_EVENT_NEWSEGMENT:
    {
      gboolean update;
      GstFormat fmt;
      GstFormat dst_fmt = GST_FORMAT_DEFAULT;
      gint64 start, stop, time, dst_start, dst_stop, dst_time;
      gdouble rate, arate;
      GstQuery *segquery;
      GstPad* peer;

      gst_event_parse_new_segment_full (event, &update, &rate, &arate, &fmt,
          &start, &stop, &time);

      GST_DEBUG_OBJECT (avsynth_video_filter, "Video cache %p: Locking sinkmutex", (gpointer) sink->cache);
      g_mutex_lock (sink->sinkmutex);
      switch (fmt) {
        case GST_FORMAT_DEFAULT:
          gst_segment_set_newsegment_full (&sink->defsegment, update, rate,
              arate, fmt, start, stop, time);
          break;
        default:
          if ((start == -1 || gst_pad_query_peer_convert (pad, fmt, start, &dst_fmt, &dst_start)) &&
              (stop == -1 || gst_pad_query_peer_convert (pad, fmt, stop, &dst_fmt, &dst_stop)) &&
              (time == -1 || gst_pad_query_peer_convert (pad, fmt, time, &dst_fmt, &dst_time)))
          {
            gst_segment_set_newsegment_full (&sink->defsegment, update, rate,
                arate, dst_fmt, dst_start, dst_stop, dst_time);
          }
          break;
      }

      gst_segment_set_newsegment_full (&sink->segment, update,
          rate, arate, fmt, start, stop, time);
      g_mutex_unlock (sink->sinkmutex);
      GST_DEBUG_OBJECT (avsynth_video_filter, "Video cache %p: Unlocked sinkmutex", (gpointer) sink->cache);
      ret = TRUE;
      break;
    }
    default:
      ret = FALSE; //ret = gst_pad_push_event (avsynth_video_filter->srcpad, event);
      break;
  }

done:
  gst_object_unref (avsynth_video_filter);

  return ret;
}

GstFlowReturn
gst_avsynth_video_filter_chain (GstPad * pad, GstBuffer * inbuf)
{
  GstAVSynthVideoFilter *avsynth_video_filter;
  GstAVSynthVideoFilterClass *oclass;
  GstFlowReturn ret = GST_FLOW_OK;
  GstClockTime in_timestamp, in_duration;
  gint64 in_offset;
  AVSynthSink *sink;

  avsynth_video_filter = (GstAVSynthVideoFilter *) (gst_pad_get_parent (pad));

  oclass = (GstAVSynthVideoFilterClass *) (G_OBJECT_GET_CLASS (avsynth_video_filter));

  in_timestamp = GST_BUFFER_TIMESTAMP (inbuf);
  in_duration = GST_BUFFER_DURATION (inbuf);
  in_offset = GST_BUFFER_OFFSET (inbuf);

  GST_LOG_OBJECT (pad,
      "Received new frame of size %d, offset:%" G_GUINT64_FORMAT ", ts:%" GST_TIME_FORMAT ", dur:%"
      GST_TIME_FORMAT, GST_BUFFER_SIZE (inbuf), GST_BUFFER_OFFSET (inbuf),
      GST_TIME_ARGS (in_timestamp), GST_TIME_ARGS (in_duration));

  GST_OBJECT_LOCK (avsynth_video_filter);

  if (G_UNLIKELY (avsynth_video_filter->uninitialized))
  {
    AVSValue *arguments;
    gint sinkcount = 0;
    AVSValue clipval;
    gboolean ready = TRUE;

    GST_DEBUG_OBJECT (pad, "Attempting to create the filter");

    arguments = new AVSValue[oclass->properties->len];
  
    /* args are initialized by _set_property() */
    /* FIXME: simplify the readiness check */
    for (guint i = 0; i < oclass->properties->len && ready; i++)
    {
      AVSynthVideoFilterParam *param = (AVSynthVideoFilterParam *) g_ptr_array_index (oclass->properties, i);
      AVSValue *arg = avsynth_video_filter->args[i];
      /* Clip argument */
      if (param->param_type == 'c')
      {
        GstCaps *padcaps;
        sink = (AVSynthSink *) g_ptr_array_index (avsynth_video_filter->sinks, sinkcount);
        GST_DEBUG_OBJECT (avsynth_video_filter, "Video cache %p: Locking sinkmutex", (gpointer) sink->cache);
        g_mutex_lock (sink->sinkmutex);
        padcaps = gst_pad_get_negotiated_caps (sink->sinkpad);

       /* FIXME: If the argument is mandatory, the filter will not check whether it is NULL or not,
        * it will just call its AsClip().
        * I am yet to find a filter that accepts _optional_ second clip argument, so i don't know
        * anything about optional clip arguments. Until i figure it out, it's an error.
        */
        if (sink->cache == NULL)
        {
          if (!padcaps)
          {
            GST_DEBUG ("Sink %d is linked, but not negotiated. Waiting.", sinkcount);
            ready = FALSE;
          }
          else
            GST_ERROR ("Sink %d is negotiated, but does not have a cache attached.", sinkcount);
        }
        else
        {
          arguments[i] = AVSValue ((IClip *) sink->cache);
        }
        if (padcaps)
          gst_caps_unref (padcaps);

        g_mutex_unlock (sink->sinkmutex);
        GST_DEBUG_OBJECT (avsynth_video_filter, "Video cache %p: Unlocked sinkmutex", (gpointer) sink->cache);
        sinkcount++;
      }
      else if (arg != NULL)
      {
        arguments[i] = *arg;
      }
    }
  
    if (ready)
    {
      GST_DEBUG_OBJECT (avsynth_video_filter, "Wrapper is ready, calling apply()");
      clipval = avsynth_video_filter->env->apply (AVSValue (arguments, oclass->properties->len), avsynth_video_filter->env->userdata, avsynth_video_filter->env);
      GST_DEBUG_OBJECT (avsynth_video_filter, "apply() returned");
      GST_DEBUG_OBJECT (avsynth_video_filter, "Locking impl_mutex");
      g_mutex_lock (avsynth_video_filter->impl_mutex);
      avsynth_video_filter->impl = clipval.AsClip();
      g_mutex_unlock (avsynth_video_filter->impl_mutex);
      GST_DEBUG_OBJECT (avsynth_video_filter, "Unlocked impl_mutex");
      avsynth_video_filter->uninitialized = FALSE;
    }

    delete [] arguments;
  }

  //GST_DEBUG_OBJECT (avsynth_video_filter, "Locking impl_mutex");
  //g_mutex_lock (avsynth_video_filter->impl_mutex);
  if (G_UNLIKELY ((gst_task_get_state (avsynth_video_filter->framegetter) == GST_TASK_STOPPED) && avsynth_video_filter->impl))
  {
    GST_DEBUG_OBJECT (avsynth_video_filter, "Starting framegetter");
    gst_task_start (avsynth_video_filter->framegetter);
  }
  //g_mutex_unlock (avsynth_video_filter->impl_mutex);
  //GST_DEBUG_OBJECT (avsynth_video_filter, "Unlocked impl_mutex");
  
  GST_OBJECT_UNLOCK (avsynth_video_filter);

  GST_DEBUG_OBJECT (pad, "Calling AddFrame()...");

  sink = (AVSynthSink *) g_object_get_data (G_OBJECT (pad), "sinkstruct");
  sink->cache->AddBuffer (pad, inbuf, avsynth_video_filter->env);

  gst_object_unref (avsynth_video_filter);

  gst_buffer_unref (inbuf);

  GST_DEBUG_OBJECT (pad, "Finished AddFrame()");

  return ret;
}

GstStateChangeReturn
gst_avsynth_video_filter_change_state (GstElement * element, GstStateChange transition)
{
  GstAVSynthVideoFilter *avsynth_video_filter = (GstAVSynthVideoFilter *) element;
  GstStateChangeReturn ret;

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      // TODO: close, clean
      break;
    default:
      break;
  }

  return ret;
}

void
gst_avsynth_video_filter_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  gboolean found = FALSE;

  GstAVSynthVideoFilter *avsynth_video_filter = (GstAVSynthVideoFilter *) object;

  GstAVSynthVideoFilterClass *filter_class = (GstAVSynthVideoFilterClass *) G_OBJECT_GET_CLASS (avsynth_video_filter);

  for (guint i = 0; !found && i < filter_class->properties->len; i++)
  {
    AVSynthVideoFilterParam *param = (AVSynthVideoFilterParam *) g_ptr_array_index (filter_class->properties, i);
    if (param->param_id == prop_id)
    {
      AVSValue *arg = avsynth_video_filter->args[i];
      found = TRUE;
      if (arg == NULL)
        arg = avsynth_video_filter->args[i] = new AVSValue();
      switch (param->param_type)
      {
        case 'b':
          *arg = AVSValue (g_value_get_boolean (value));
          break;
        case 'i':
          *arg = AVSValue (g_value_get_int (value));
          break;
        case 'f':
          *arg = AVSValue (g_value_get_float (value));
          break;
        case 's':
          *arg = AVSValue (g_value_get_string (value));
          break;
        default:
          G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          break;
      }
    }
  }
  if (!found)
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

void
gst_avsynth_video_filter_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  gboolean found = FALSE;
  GstAVSynthVideoFilter *avsynth_video_filter = (GstAVSynthVideoFilter *) object;
  GstAVSynthVideoFilterClass *filter_class = (GstAVSynthVideoFilterClass *) G_OBJECT_GET_CLASS (avsynth_video_filter);

  for (guint i = 0; !found && i < filter_class->properties->len; i++)
  {
    AVSynthVideoFilterParam *param = (AVSynthVideoFilterParam *) g_ptr_array_index (filter_class->properties, i);
    if (param->param_id == prop_id)
    {
      AVSValue *arg = avsynth_video_filter->args[i];
      found = TRUE;
      if (arg == NULL)
      {
        g_value_unset (value);;
      }
      switch (param->param_type)
      {
        case 'b':
          g_value_set_boolean (value, arg->AsBool());
          break;
        case 'i':
          g_value_set_int (value, arg->AsInt());
          break;
        case 'f':
          g_value_set_float (value, arg->AsFloat());
          break;
        case 's':
          g_value_set_string (value, arg->AsString());
          break;
        default:
          G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          break;
      }
    }
  }
  if (!found)
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}
