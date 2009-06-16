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
  for (gchar *params = (gchar *) klass->params; params; params++)
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
            for (gchar *nameptr = params++; nameptr[0] != ']'; nameptr++)
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
        paramstr->param_name = paramname;
      }

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
      else if (paramspec && parammult == '+')
      {
        GParamSpec *paramspec_array;
        gchar *paramname_array;
        paramname_array = g_strdup_printf ("param%d", klass->properties->len);
        paramspec_array = g_param_spec_value_array (paramname_array,
            "", "", paramspec, paramflags);
        g_object_class_install_property (gobject_class, klass->properties->len,
          paramspec_array);
      }
      g_ptr_array_add (klass->properties, (gpointer) paramstr);

      parammult = 0;
      paramname = NULL;
      paramnamepos = 0;
      paramtype = 0;
    }
  }

  gstelement_class->change_state = gst_avsynth_video_filter_change_state;
}

void
gst_avsynth_video_filter_framegetter (void *data)
{
  /* FIXME: normal implementation */
  GstAVSynthVideoFilter *avsynth_video_filter;
  gboolean stop = FALSE;
  int framenumber = 0;

  avsynth_video_filter = GST_AVSYNTH_VIDEO_FILTER (data);

  while (!stop)
  {
    avsynth_video_filter->impl->GetFrame(framenumber, avsynth_video_filter->env);

    g_mutex_lock (avsynth_video_filter->stop_mutex);
    stop = avsynth_video_filter->stop;
    g_mutex_unlock (avsynth_video_filter->stop_mutex);
    framenumber++;
  }  
}

void
gst_avsynth_video_filter_init (GstAVSynthVideoFilter *avsynth_video_filter)
{
  AvisynthPluginInitFunc init_func = NULL;
  GstAVSynthVideoFilterClass *oclass;
  gchar *full_filename = NULL;

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

  avsynth_video_filter->sinkpads = g_ptr_array_new ();

  for (guint i = 0; i < oclass->properties->len; i++)
  {
    GstPad *sinkpad = NULL;
    GstAVSynthVideoCache *vcache = NULL;
    AVSynthVideoFilterParam *param = (AVSynthVideoFilterParam*) g_ptr_array_index (oclass->properties, i);
    if (param->param_type != 'c')
      continue;
    if (param->param_mult != 0)
      GST_FIXME ("Can't create an array of sinkpads for %c%c", param->param_type, param->param_mult);

    sinkpad = gst_pad_new_from_template (oclass->sinktempl, "sink");
    g_ptr_array_add (avsynth_video_filter->sinkpads, (gpointer) sinkpad);
    gst_pad_set_setcaps_function (sinkpad,
        GST_DEBUG_FUNCPTR (gst_avsynth_video_filter_setcaps));
    gst_pad_set_event_function (sinkpad,
        GST_DEBUG_FUNCPTR (gst_avsynth_video_filter_sink_event));
    gst_pad_set_chain_function (sinkpad,
        GST_DEBUG_FUNCPTR (gst_avsynth_video_filter_chain));
    gst_element_add_pad (GST_ELEMENT (avsynth_video_filter), sinkpad);
    vcache = NULL;
    g_object_set_data (G_OBJECT (sinkpad), "video-cache", (gpointer) vcache);
  }

  avsynth_video_filter->srcpad = gst_pad_new_from_template (oclass->srctempl, "src");
  gst_pad_use_fixed_caps (avsynth_video_filter->srcpad);
  gst_pad_set_event_function (avsynth_video_filter->srcpad,
      GST_DEBUG_FUNCPTR (gst_avsynth_video_filter_src_event));
  gst_pad_set_query_function (avsynth_video_filter->srcpad,
      GST_DEBUG_FUNCPTR (gst_avsynth_video_filter_query));
  gst_element_add_pad (GST_ELEMENT (avsynth_video_filter), avsynth_video_filter->srcpad);

  gst_segment_init (&avsynth_video_filter->segment, GST_FORMAT_DEFAULT);

  avsynth_video_filter->framegetter_mutex = g_new (GStaticRecMutex, 1);
  g_static_rec_mutex_init (avsynth_video_filter->framegetter_mutex);

  avsynth_video_filter->framegetter = gst_task_create (gst_avsynth_video_filter_framegetter, (gpointer) avsynth_video_filter);
  gst_task_set_lock (avsynth_video_filter->framegetter, avsynth_video_filter->framegetter_mutex);

  avsynth_video_filter->stop = FALSE;
  avsynth_video_filter->stop_mutex = g_mutex_new();

  avsynth_video_filter->args = new PAVSValue[oclass->properties->len];
  for (guint i = 0; i < oclass->properties->len; i++)
    avsynth_video_filter->args[i] = NULL;
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
  GstPad *peer;
  gboolean res;

  avsynth_video_filter = (GstAVSynthVideoFilter *) gst_pad_get_parent (pad);

  res = FALSE;

  for (guint i = 0; i < avsynth_video_filter->sinkpads->len; i++)
  {
    GstPad *sinkpad = (GstPad *) g_ptr_array_index (avsynth_video_filter->sinkpads, i);
    peer = gst_pad_get_peer (sinkpad);
    /* Forward the query to the peer */
    res = gst_pad_query (peer, query);
    gst_object_unref (peer);
  }

  gst_object_unref (avsynth_video_filter);

  return res;
}

gboolean
gst_avsynth_video_filter_src_event (GstPad * pad, GstEvent * event)
{
  GstAVSynthVideoFilter *avsynth_video_filter;
  gboolean res = FALSE;

  avsynth_video_filter = (GstAVSynthVideoFilter *) gst_pad_get_parent (pad);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_QOS:
    {
      /* It seems appropriate to block any QoS requests - we don't want to skip
       * frames, don't we?
       */
      break;
    }
    default:
      /* forward upstream */
      for (guint i = 0; i < avsynth_video_filter->sinkpads->len; i++)
      {
        GstPad *sinkpad = (GstPad *) g_ptr_array_index (avsynth_video_filter->sinkpads, i);
        res = gst_pad_push_event (sinkpad, event);
      }
      break;
  }

  gst_object_unref (avsynth_video_filter);

  return res;
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
  AVSValue *arguments;
  gint sinkcount = 0;
  AVSValue clipval;
  GstAVSynthVideoCache *vcache;

  vcache = (GstAVSynthVideoCache *) g_object_get_data (G_OBJECT (pad), "video-cache");

  avsynth_video_filter = (GstAVSynthVideoFilter *) (gst_pad_get_parent (pad));
  oclass = (GstAVSynthVideoFilterClass *) (G_OBJECT_GET_CLASS (avsynth_video_filter));

  GST_DEBUG_OBJECT (pad, "setcaps called");

  GST_OBJECT_LOCK (avsynth_video_filter);

  /* So...we've got a new media, with different caps. What to do?
   * As far as i know, AviSynth can't feed dynamically changing media to a
   * filter. Which means that each setcaps after the first one should
   * either fail or recreate the filter. I'll go for recreation, i think.
   */

  vi = g_new0 (VideoInfo, 1);
  gst_avsynth_buf_pad_caps_to_vi (NULL, pad, caps, vi);

  if (vcache)
  {
    delete vcache;
    vcache = NULL;
  }
  vcache = new GstAVSynthVideoCache(vi, pad);
  g_object_set_data (G_OBJECT (pad), "video-cache", (gpointer) vcache);

  if (avsynth_video_filter->impl)
  {
    avsynth_video_filter->impl = NULL;
  }

  arguments = new AVSValue[oclass->properties->len];

  /* args are initialized by _set_property() */
  for (guint i = 0; i < oclass->properties->len; i++)
  {
    AVSynthVideoFilterParam *param = (AVSynthVideoFilterParam *) g_ptr_array_index (oclass->properties, i);
    AVSValue *arg = avsynth_video_filter->args[i];
    /* Clip argument */
    if (param->param_type == 'c')
    {
      GstCaps *padcaps;
      GstPad *sink = GST_PAD (g_ptr_array_index (avsynth_video_filter->sinkpads, sinkcount));
      padcaps = gst_pad_get_negotiated_caps (sink);
      /* Pad is negotiated */
      if (padcaps)
      {
        arguments[i] = AVSValue (g_ptr_array_index (avsynth_video_filter->videocaches, sinkcount));
        gst_caps_unref (padcaps);
      }
      sinkcount++;
    }
    else if (arg != NULL)
    {
      arguments[i] = arg;
    }
  }

  clipval = avsynth_video_filter->env->apply (arguments, avsynth_video_filter->env->userdata, avsynth_video_filter->env);
  avsynth_video_filter->impl = clipval.AsClip();

  delete arguments;

  GST_OBJECT_UNLOCK (avsynth_video_filter);

  gst_object_unref (avsynth_video_filter);

  return ret;
}

gboolean
gst_avsynth_video_filter_sink_event (GstPad * pad, GstEvent * event)
{
  GstAVSynthVideoFilter *avsynth_video_filter;
  GstAVSynthVideoFilterClass *oclass;
  gboolean ret = FALSE;

  avsynth_video_filter = (GstAVSynthVideoFilter *) gst_pad_get_parent (pad);
  oclass = (GstAVSynthVideoFilterClass *) (G_OBJECT_GET_CLASS (avsynth_video_filter));

  GST_DEBUG_OBJECT (avsynth_video_filter, "Handling %s event",
      GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:
    {
      /* TODO: Flush */
      break;
    }
    case GST_EVENT_FLUSH_STOP:
    {
      /* TODO: Flush if working, reset things, init new segment */
      break;
    }
    case GST_EVENT_NEWSEGMENT:
    {
      gboolean update;
      GstFormat fmt;
      gint64 start, stop, time;
      gdouble rate, arate;

      gst_event_parse_new_segment_full (event, &update, &rate, &arate, &fmt,
          &start, &stop, &time);

      switch (fmt) {
        case GST_FORMAT_TIME:
          /* TODO: convert to default */
          break;
        case GST_FORMAT_BYTES:
        {
          /* TODO: convert to default */
          break;
        }
        case GST_FORMAT_DEFAULT:
        {
          /* fine, our native format */
          break;
        }
        default:
          /* invalid format */
          goto invalid_format;
      }

      /* drain pending frames before trying to use the new segment, queued
       * buffers belonged to the previous segment. */
      /* TODO: flush */

      GST_DEBUG_OBJECT (avsynth_video_filter,
          "NEWSEGMENT in time start %" GST_TIME_FORMAT " -- stop %"
          GST_TIME_FORMAT, GST_TIME_ARGS (start), GST_TIME_ARGS (stop));

      /* and store the values */
      gst_segment_set_newsegment_full (&avsynth_video_filter->segment, update,
          rate, arate, fmt, start, stop, time);
      break;
    }
    default:
      break;
  }

  /* and push segment downstream */
  ret = gst_pad_push_event (avsynth_video_filter->srcpad, event);

done:
  gst_object_unref (avsynth_video_filter);

  return ret;

  /* ERRORS */
invalid_format:
  {
    GST_WARNING_OBJECT (avsynth_video_filter, "unknown format received in NEWSEGMENT");
    gst_event_unref (event);
    goto done;
  }
}

GstFlowReturn
gst_avsynth_video_filter_chain (GstPad * pad, GstBuffer * inbuf)
{
  GstAVSynthVideoFilter *avsynth_video_filter;
  GstAVSynthVideoFilterClass *oclass;
  GstFlowReturn ret = GST_FLOW_OK;
  GstClockTime in_timestamp, in_duration;
  gint64 in_offset;
  GstAVSynthVideoCache *vcache;

  avsynth_video_filter = (GstAVSynthVideoFilter *) (GST_PAD_PARENT (pad));

  oclass = (GstAVSynthVideoFilterClass *) (G_OBJECT_GET_CLASS (avsynth_video_filter));

  if (G_UNLIKELY (!avsynth_video_filter->impl))
    goto not_negotiated;

  in_timestamp = GST_BUFFER_TIMESTAMP (inbuf);
  in_duration = GST_BUFFER_DURATION (inbuf);
  in_offset = GST_BUFFER_OFFSET (inbuf);

  GST_LOG_OBJECT (avsynth_video_filter,
      "Received new frane of size %d, offset:%" G_GINT64_FORMAT ", ts:%" GST_TIME_FORMAT ", dur:%"
      GST_TIME_FORMAT, GST_BUFFER_OFFSET (inbuf), GST_BUFFER_SIZE (inbuf),
      GST_TIME_ARGS (in_timestamp), GST_TIME_ARGS (in_duration));

  if (G_UNLIKELY (!avsynth_video_filter->getting_frames))
  {
    avsynth_video_filter->getting_frames = TRUE;
    gst_task_start (avsynth_video_filter->framegetter);
  }

  vcache = (GstAVSynthVideoCache *) g_object_get_data (G_OBJECT (pad), "video-cache");
  if (G_UNLIKELY (!vcache))
    goto not_negotiated;

  vcache->AddBuffer (pad, inbuf, avsynth_video_filter->env);

  gst_buffer_unref (inbuf);

  return ret;

  /* ERRORS */
not_negotiated:
  {
    GST_ELEMENT_ERROR (avsynth_video_filter, CORE, NEGOTIATION, (NULL),
        ("%s: filter was not initialized before data start",
            oclass->name));
    gst_buffer_unref (inbuf);
    return GST_FLOW_NOT_NEGOTIATED;
  }
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
