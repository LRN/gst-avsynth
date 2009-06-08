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
#include "gstavsynth_videobuffer.h"
#include "gstavsynth_videofilter.h"
#include "gstavsynth_loader.h"


#define DEFAULT_BUFFER_SIZE             10

enum
{
  PROP_0,
  PROP_BUFFER_SIZE,
  PROP_LAST
};

static GstElementClass *parent_class = NULL;

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
gst_avsynth_video_filter_init (GstAVSynthVideoFilter * avsynth_video_filter)
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
    GST_LOG ("Failed. Getting AvisynthPluginInit2@4...");
    if (!g_module_symbol (avsynth_video_filter->plugin, "AvisynthPluginInit2@4", (gpointer *) &init_func))
    {
      GST_ERROR ("Failed. Something is wrong here...");
    }
  }

  init_func (avsynth_video_filter->env);

  /* setup pads */

  avsynth_video_filter->sinkpads = g_ptr_array_new ();

  for (guint i = 0; i < oclass->properties->len; i++)
  {
    GstPad *sinkpad = NULL;
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
  }

  avsynth_video_filter->srcpad = gst_pad_new_from_template (oclass->srctempl, "src");
  gst_pad_use_fixed_caps (avsynth_video_filter->srcpad);
  gst_pad_set_event_function (avsynth_video_filter->srcpad,
      GST_DEBUG_FUNCPTR (gst_avsynth_video_filter_src_event));
  gst_pad_set_query_function (avsynth_video_filter->srcpad,
      GST_DEBUG_FUNCPTR (gst_avsynth_video_filter_query));
  gst_element_add_pad (GST_ELEMENT (avsynth_video_filter), avsynth_video_filter->srcpad);

  gst_segment_init (&avsynth_video_filter->segment, GST_FORMAT_DEFAULT);
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

  for (guing i = 0; i < avsynth_video_filter->sinkpads->len; i++)
  {
    GstPad *sinkpad = (GstPad *) g_ptr_array_index (avsynth_video_filter->sinkpads, i);
    peer = gst_pad_get_peer (sinkpad));
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
      for (guing i = 0; i < avsynth_video_filter->sinkpads->len; i++)
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

  avsynth_video_filter = (GstAVSynthVideoFilter *) (gst_pad_get_parent (pad));
  oclass = (GstAVSynthVideoFilterClass *) (G_OBJECT_GET_CLASS (avsynth_video_filter));

  GST_DEBUG_OBJECT (pad, "setcaps called");

  GST_OBJECT_LOCK (avsynth_video_filter);

  /* TODO: filter reinitialization (for new caps)
   * So...we've got a new media, with different caps. What to do?
   * As far as i know, AviSynth can't feed dynamically changing media to a
   * filter. Which means that each setcaps after the first one should
   * either fail or recreate the filter. I'll go for recreation, i think.
   */
  /*
  if (impl)
    delete impl;
  /* Here we need to pass
  AVSValue *args;
  which is an array of arguments, composed according to filter's param string
  First argument is usually a PClip.
  Which means it is time to think about Clip object implementation.
  impl = avsynth_video_filter->env->apply(args, avsynth_video_filter->env->userdata, avsynth_video_filter->env);
  */
  /*
  // close old session
  gst_avsynth_video_filter_close (avsynth_video_filter);

  // set defaults
  avcodec_get_context_defaults (avsynth_video_filter->context);

  // set buffer functions
  avsynth_video_filter->context->get_buffer = gst_avsynth_video_filter_get_buffer;
  avsynth_video_filter->context->release_buffer = gst_avsynth_video_filter_release_buffer;
  avsynth_video_filter->context->draw_horiz_band = NULL;

  // assume PTS as input, we will adapt when we detect timestamp reordering in the output frames.
  avsynth_video_filter->ts_is_dts = FALSE;
  avsynth_video_filter->has_b_frames = FALSE;

  GST_LOG_OBJECT (avsynth_video_filter, "size %dx%d", avsynth_video_filter->context->width,
      avsynth_video_filter->context->height);

  // get size and so
  gst_ffmpeg_caps_with_codecid (oclass->in_plugin->id,
      oclass->in_plugin->type, caps, avsynth_video_filter->context);

  GST_LOG_OBJECT (avsynth_video_filter, "size after %dx%d", avsynth_video_filter->context->width,
      avsynth_video_filter->context->height);

  if (!avsynth_video_filter->context->time_base.den || !avsynth_video_filter->context->time_base.num) {
    GST_DEBUG_OBJECT (avsynth_video_filter, "forcing 25/1 framerate");
    avsynth_video_filter->context->time_base.num = 1;
    avsynth_video_filter->context->time_base.den = 25;
  }

  // get pixel aspect ratio if it's set
  structure = gst_caps_get_structure (caps, 0);

  par = gst_structure_get_value (structure, "pixel-aspect-ratio");
  if (par) {
    GST_DEBUG_OBJECT (avsynth_video_filter, "sink caps have pixel-aspect-ratio of %d:%d",
        gst_value_get_fraction_numerator (par),
        gst_value_get_fraction_denominator (par));
    // should be NULL
    if (avsynth_video_filter->par)
      g_free (avsynth_video_filter->par);
    avsynth_video_filter->par = g_new0 (GValue, 1);
    gst_value_init_and_copy (avsynth_video_filter->par, par);
  }

  // get the framerate from incomming caps. fps_n is set to -1 when there is no valid framerate
  fps = gst_structure_get_value (structure, "framerate");
  if (fps != NULL && GST_VALUE_HOLDS_FRACTION (fps)) {
    avsynth_video_filter->format.video.fps_n = gst_value_get_fraction_numerator (fps);
    avsynth_video_filter->format.video.fps_d = gst_value_get_fraction_denominator (fps);
    GST_DEBUG_OBJECT (avsynth_video_filter, "Using framerate %d/%d from incoming caps",
        avsynth_video_filter->format.video.fps_n, avsynth_video_filter->format.video.fps_d);
  } else {
    avsynth_video_filter->format.video.fps_n = -1;
    GST_DEBUG_OBJECT (avsynth_video_filter, "Using framerate from codec");
  }

  // figure out if we can use direct rendering
  avsynth_video_filter->current_dr = FALSE;
  avsynth_video_filter->extra_ref = FALSE;
  if (avsynth_video_filter->direct_rendering) {
    GST_DEBUG_OBJECT (avsynth_video_filter, "trying to enable direct rendering");
    if (oclass->in_plugin->capabilities & CODEC_CAP_DR1) {
      if (oclass->in_plugin->id == CODEC_ID_H264) {
        GST_DEBUG_OBJECT (avsynth_video_filter, "disable direct rendering setup for H264");
        // does not work, many stuff reads outside of the planes
        avsynth_video_filter->current_dr = FALSE;
        avsynth_video_filter->extra_ref = TRUE;
      } else {
        GST_DEBUG_OBJECT (avsynth_video_filter, "enabled direct rendering");
        avsynth_video_filter->current_dr = TRUE;
      }
    } else {
      GST_DEBUG_OBJECT (avsynth_video_filter, "direct rendering not supported");
    }
  }
  if (avsynth_video_filter->current_dr) {
    // do *not* draw edges when in direct rendering, for some reason it draws outside of the memory.
    avsynth_video_filter->context->flags |= CODEC_FLAG_EMU_EDGE;
  }

  // workaround encoder bugs
  avsynth_video_filter->context->workaround_bugs |= FF_BUG_AUTODETECT;
  avsynth_video_filter->context->error_recognition = 1;

  // for slow cpus
  avsynth_video_filter->context->lowres = avsynth_video_filter->lowres;
  avsynth_video_filter->context->hurry_up = avsynth_video_filter->hurry_up;

  // ffmpeg can draw motion vectors on top of the image (not every decoder supports it)
  avsynth_video_filter->context->debug_mv = avsynth_video_filter->debug_mv;

  // open codec - we don't select an output pix_fmt yet, simply because we don't know! We only get it during playback...
  if (!gst_avsynth_video_filter_open (avsynth_video_filter))
    goto open_failed;

  // clipping region
  gst_structure_get_int (structure, "width",
      &avsynth_video_filter->format.video.clip_width);
  gst_structure_get_int (structure, "height",
      &avsynth_video_filter->format.video.clip_height);

  // take into account the lowres property
  if (avsynth_video_filter->format.video.clip_width != -1)
    avsynth_video_filter->format.video.clip_width >>= avsynth_video_filter->lowres;
  if (avsynth_video_filter->format.video.clip_height != -1)
    avsynth_video_filter->format.video.clip_height >>= avsynth_video_filter->lowres;
  */

done:
  GST_OBJECT_UNLOCK (avsynth_video_filter);

  gst_object_unref (avsynth_video_filter);

  return ret;

  /* ERRORS */
open_failed:
  {
    GST_DEBUG_OBJECT (avsynth_video_filter, "Failed to open");
/*    if (avsynth_video_filter->par) {
      g_free (avsynth_video_filter->par);
      avsynth_video_filter->par = NULL;
    }*/
    ret = FALSE;
    goto done;
  }
}


gboolean
gst_avsynth_video_filter_negotiate (GstAVSynthVideoFilter * avsynth_video_filter)
{
  GstAVSynthVideoFilterClass *oclass;
  GstCaps *caps = NULL;

  oclass = (GstAVSynthVideoFilterClass *) (G_OBJECT_GET_CLASS (avsynth_video_filter));

  /* TODO: do something? I'm not sure this function is required at all */
  /*
  if (avsynth_video_filter->format.video.width == avsynth_video_filter->context->width &&
      avsynth_video_filter->format.video.height == avsynth_video_filter->context->height &&
      avsynth_video_filter->format.video.fps_n == avsynth_video_filter->format.video.old_fps_n &&
      avsynth_video_filter->format.video.fps_d == avsynth_video_filter->format.video.old_fps_d &&
      avsynth_video_filter->format.video.pix_fmt == avsynth_video_filter->context->pix_fmt)
    return TRUE;
  GST_DEBUG_OBJECT (avsynth_video_filter,
      "Renegotiating video from %dx%d@ %d/%d fps to %dx%d@ %d/%d fps",
      avsynth_video_filter->format.video.width, avsynth_video_filter->format.video.height,
      avsynth_video_filter->format.video.old_fps_n, avsynth_video_filter->format.video.old_fps_n,
      avsynth_video_filter->context->width, avsynth_video_filter->context->height,
      avsynth_video_filter->format.video.fps_n, avsynth_video_filter->format.video.fps_d);
  avsynth_video_filter->format.video.width = avsynth_video_filter->context->width;
  avsynth_video_filter->format.video.height = avsynth_video_filter->context->height;
  avsynth_video_filter->format.video.old_fps_n = avsynth_video_filter->format.video.fps_n;
  avsynth_video_filter->format.video.old_fps_d = avsynth_video_filter->format.video.fps_d;
  avsynth_video_filter->format.video.pix_fmt = avsynth_video_filter->context->pix_fmt;

  caps = gst_ffmpeg_codectype_to_caps (oclass->in_plugin->type,
      avsynth_video_filter->context, oclass->in_plugin->id, FALSE);

  if (caps == NULL)
    goto no_caps;

  switch (oclass->in_plugin->type) {
    case CODEC_TYPE_VIDEO:
    {
      gint width, height;

      width = avsynth_video_filter->format.video.clip_width;
      height = avsynth_video_filter->format.video.clip_height;

      if (width != -1 && height != -1) {
        // overwrite the output size with the dimension of the clipping region
        gst_caps_set_simple (caps,
            "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, NULL);
      }
      // If a demuxer provided a framerate then use it (#313970)
      if (avsynth_video_filter->format.video.fps_n != -1) {
        gst_caps_set_simple (caps, "framerate",
            GST_TYPE_FRACTION, avsynth_video_filter->format.video.fps_n,
            avsynth_video_filter->format.video.fps_d, NULL);
      }
      gst_avsynth_video_filter_add_pixel_aspect_ratio (avsynth_video_filter,
          gst_caps_get_structure (caps, 0));
      break;
    }
    case CODEC_TYPE_AUDIO:
    {
      break;
    }
    default:
      break;
  }
  */

  if (!gst_pad_set_caps (avsynth_video_filter->srcpad, caps))
    goto caps_failed;

  gst_caps_unref (caps);

  return TRUE;

  /* ERRORS */
no_caps:
  {
    GST_ELEMENT_ERROR (avsynth_video_filter, CORE, NEGOTIATION, (NULL),
        ("could not find caps for filter (%s), unknown type",
            oclass->name));
    return FALSE;
  }
caps_failed:
  {
    GST_ELEMENT_ERROR (avsynth_video_filter, CORE, NEGOTIATION, (NULL),
        ("Could not set caps for filter (%s), not fixed?",
            oclass->name));
    gst_caps_unref (caps);

    return FALSE;
  }
}

/* returns TRUE if buffer is within segment, else FALSE.
 * if Buffer is on segment border, it's timestamp and duration will be clipped */
/* TODO: rewrite in default format? */
/*
gboolean
clip_video_buffer (GstAVSynthVideoFilter * avsynth_video_filter, GstBuffer * buf, GstClockTime in_ts,
    GstClockTime in_dur)
{
  gboolean res = TRUE;
  gint64 cstart, cstop;
  GstClockTime stop;

  GST_LOG_OBJECT (dec,
      "timestamp:%" GST_TIME_FORMAT " , duration:%" GST_TIME_FORMAT,
      GST_TIME_ARGS (in_ts), GST_TIME_ARGS (in_dur));

  // can't clip without TIME segment
  if (G_UNLIKELY (dec->segment.format != GST_FORMAT_TIME))
    goto beach;

  // we need a start time
  if (G_UNLIKELY (!GST_CLOCK_TIME_IS_VALID (in_ts)))
    goto beach;

  // generate valid stop, if duration unknown, we have unknown stop
  stop =
      GST_CLOCK_TIME_IS_VALID (in_dur) ? (in_ts + in_dur) : GST_CLOCK_TIME_NONE;

  // now clip
  res =
      gst_segment_clip (&dec->segment, GST_FORMAT_TIME, in_ts, stop, &cstart,
      &cstop);
  if (G_UNLIKELY (!res))
    goto beach;

  // we're pretty sure the duration of this buffer is not till the end of this segment (which _clip will assume when the stop is -1)
  if (stop == GST_CLOCK_TIME_NONE)
    cstop = GST_CLOCK_TIME_NONE;

  // update timestamp and possibly duration if the clipped stop time is valid
  GST_BUFFER_TIMESTAMP (buf) = cstart;
  if (GST_CLOCK_TIME_IS_VALID (cstop))
    GST_BUFFER_DURATION (buf) = cstop - cstart;

  GST_LOG_OBJECT (dec,
      "clipped timestamp:%" GST_TIME_FORMAT " , duration:%" GST_TIME_FORMAT,
      GST_TIME_ARGS (cstart), GST_TIME_ARGS (GST_BUFFER_DURATION (buf)));

beach:
  GST_LOG_OBJECT (dec, "%sdropping", (res ? "not " : ""));
  return res;
}
*/

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
no_bitrate:
  {
    GST_WARNING_OBJECT (avsynth_video_filter, "no bitrate to convert BYTES to TIME");
    gst_event_unref (event);
    goto done;
  }
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
  guint8 *data, *bdata, *pdata;
  gint size, bsize, len, have_data;
  GstFlowReturn ret = GST_FLOW_OK;
  GstClockTime in_timestamp, in_duration;
  gboolean discont;
  gint64 in_offset;

  avsynth_video_filter = (GstAVSynthVideoFilter *) (GST_PAD_PARENT (pad));

  /* TODO: check that we're ready */
  /*
  if (G_UNLIKELY (!avsynth_video_filter->opened))
    goto not_negotiated;
  */

  discont = GST_BUFFER_IS_DISCONT (inbuf);

  /* The discont flags marks a buffer that is not continuous with the previous
   * buffer. This means we need to clear whatever data we currently have. We
   * currently also wait for a new keyframe, which might be suboptimal in the
   * case of a network error, better show the errors than to drop all data.. */
  /* I don't think this is applicable for AviSynth... We won't get partial buffers */
  if (G_UNLIKELY (discont)) {
    GST_DEBUG_OBJECT (avsynth_video_filter, "received DISCONT");
    /* TODO: drain what we have queued */
    /*
    gst_avsynth_video_filter_drain (avsynth_video_filter);
    gst_avsynth_video_filter_flush_pcache (avsynth_video_filter);
    avcodec_flush_buffers (avsynth_video_filter->context);
    avsynth_video_filter->waiting_for_key = TRUE;
    avsynth_video_filter->discont = TRUE;
    avsynth_video_filter->last_out = GST_CLOCK_TIME_NONE;
    avsynth_video_filter->next_ts = GST_CLOCK_TIME_NONE;
    */
  }
  /* by default we clear the input timestamp after decoding each frame so that
   * interpolation can work. */
  //avsynth_video_filter->clear_ts = TRUE;

  oclass = (GstAVSynthVideoFilterClass *) (G_OBJECT_GET_CLASS (avsynth_video_filter));

  /* do early keyframe check pretty bad to rely on the keyframe flag in the
   * source for this as it might not even be parsed (UDP/file/..).  */
  /* We don't have any keyframes at this point */
  /*
  if (G_UNLIKELY (avsynth_video_filter->waiting_for_key)) {
    if (GST_BUFFER_FLAG_IS_SET (inbuf, GST_BUFFER_FLAG_DELTA_UNIT) &&
        oclass->in_plugin->type != CODEC_TYPE_AUDIO)
      goto skip_keyframe;

    GST_DEBUG_OBJECT (avsynth_video_filter, "got keyframe");
    avsynth_video_filter->waiting_for_key = FALSE;
  }
  */

  in_timestamp = GST_BUFFER_TIMESTAMP (inbuf);
  in_duration = GST_BUFFER_DURATION (inbuf);
  in_offset = GST_BUFFER_OFFSET (inbuf);

  GST_LOG_OBJECT (avsynth_video_filter,
      "Received new data of size %d, offset:%" G_GINT64_FORMAT ", ts:%" GST_TIME_FORMAT ", dur:%"
      GST_TIME_FORMAT, GST_BUFFER_OFFSET (inbuf), GST_BUFFER_SIZE (inbuf),
      GST_TIME_ARGS (in_timestamp), GST_TIME_ARGS (in_duration));

  /* parse cache joining. If there is cached data, its timestamp will be what we
   * send to the parse. */
  /* TODO: put the buffer in the cache. Cache will invoke the filter when it is full */

  bdata = GST_BUFFER_DATA (inbuf);
  bsize = GST_BUFFER_SIZE (inbuf);

  gst_buffer_unref (inbuf);

  return ret;

  /* ERRORS */
not_negotiated:
  {
    oclass = (GstAVSynthVideoFilterClass *) (G_OBJECT_GET_CLASS (avsynth_video_filter));
    GST_ELEMENT_ERROR (avsynth_video_filter, CORE, NEGOTIATION, (NULL),
        ("ffdec_%s: input format was not set before data start",
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
  GstAVSynthVideoFilter *avsynth_video_filter = (GstAVSynthVideoFilter *) object;
  /* FIXME: shouldn't i use some obscure GObject safe casting macro? */
  GstAVSynthVideoFilterClass *filter_class = (GstAVSynthVideoFilterClass *) G_OBJECT_GET_CLASS (avsynth_video_filter);

  switch (prop_id) {
    /* TODO: implement properties */
    /*
    case PROP_LOWRES:
      avsynth_video_filter->lowres = avsynth_video_filter->context->lowres = g_value_get_enum (value);
      break;
    case PROP_SKIPFRAME:
      avsynth_video_filter->hurry_up = avsynth_video_filter->context->hurry_up =
          g_value_get_enum (value);
      break;
    case PROP_DIRECT_RENDERING:
      avsynth_video_filter->direct_rendering = g_value_get_boolean (value);
      break;
    case PROP_DO_PADDING:
      avsynth_video_filter->do_padding = g_value_get_boolean (value);
      break;
    case PROP_DEBUG_MV:
      avsynth_video_filter->debug_mv = avsynth_video_filter->context->debug_mv =
          g_value_get_boolean (value);
      break;
    case PROP_CROP:
      avsynth_video_filter->crop = g_value_get_boolean (value);
      break;
    */
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

void
gst_avsynth_video_filter_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstAVSynthVideoFilter *avsynth_video_filter = (GstAVSynthVideoFilter *) object;
  GstAVSynthVideoFilterClass *filter_class = (GstAVSynthVideoFilterClass *) G_OBJECT_GET_CLASS (avsynth_video_filter);

  switch (prop_id) {
    /* TODO: implement properties */
    /*
    case PROP_LOWRES:
      g_value_set_enum (value, avsynth_video_filter->context->lowres);
      break;
    case PROP_SKIPFRAME:
      g_value_set_enum (value, avsynth_video_filter->context->hurry_up);
      break;
    case PROP_DIRECT_RENDERING:
      g_value_set_boolean (value, avsynth_video_filter->direct_rendering);
      break;
    case PROP_DO_PADDING:
      g_value_set_boolean (value, avsynth_video_filter->do_padding);
      break;
    case PROP_DEBUG_MV:
      g_value_set_boolean (value, avsynth_video_filter->context->debug_mv);
      break;
    case PROP_CROP:
      g_value_set_boolean (value, avsynth_video_filter->crop);
      break;
    */
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


