/*
 * GstAVSynth:
 * Copyright (C) 2009 LRN <lrn1986 _at_ gmail _dot_ com>
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gst/gst.h>
#include <gst/video/video.h>
#include "gstavsynth_videocache.h"
#include "gstavsynth_scriptenvironment.h"
#include "gstavsynth_videofilter.h"

void AVSC_CC
_avs_vcf_destroy(AVS_Clip *p, gboolean freeself)
{
  guint i;
  AVS_VideoCacheFilter *vcf = (AVS_VideoCacheFilter *)p;
  
  for (i = 0; i < vcf->bufs->len; i++)
  {
    _avs_vf_release ((AVS_VideoFrame *) g_ptr_array_index (vcf->bufs, i));
  }
  g_ptr_array_free (vcf->bufs, TRUE);
  g_object_unref (vcf->pad);
  g_cond_free (vcf->vcache_cond);
  g_cond_free (vcf->vcache_block_cond);

  vcf->parent.destroy ((AVS_Clip *) &vcf->parent, FALSE);

  if (freeself)
    g_free (vcf);
}

/* start_size defines both the size of underlying array and the number of
 * elements of that array used for cache.
 */
AVS_VideoCacheFilter * AVSC_CC
_avs_vcf_construct (AVS_VideoCacheFilter *p, AVS_VideoInfo init_vi, GstPad *in_pad, gint start_size)
{
  AVS_VideoCacheFilter *vcf;
  AVS_Clip *clip_self;

  if (!p)
    vcf = g_new0 (AVS_VideoCacheFilter, 1);
  else
    vcf = p;

  vcf->destroy = _avs_vcf_destroy;
  vcf->child = NULL;

  clip_self = (AVS_Clip *)vcf;
  if (!clip_self->get_frame)
    clip_self->get_frame = _avs_vcf_get_frame;
  if (!clip_self->get_video_info)
    clip_self->get_video_info = _avs_vcf_get_video_info;
  if (!clip_self->get_parity)
    clip_self->get_parity = _avs_vcf_get_parity;

  _avs_gvf_construct (&vcf->parent, NULL);

  vcf->parent.child = clip_self;

  vcf->bufs = g_ptr_array_new ();
  vcf->rng_from  = 0;
  vcf->used_size = start_size;
  vcf->size = 0;
  vcf->touched_last_time = 0;
  vcf->vi = init_vi;
  vcf->vcache_cond = g_cond_new ();
  vcf->vcache_block_cond = g_cond_new ();
  vcf->pad = in_pad;

  g_object_ref (vcf->pad);
  g_ptr_array_set_size (vcf->bufs, start_size);
  vcf->framecounter = 0;

  return vcf;
}

/* Changes used_size to newsize. If used_size becomes larger than size,
 * extends the cache
 */
void AVSC_CC
_avs_vcf_resize (AVS_VideoCacheFilter *p, gint64 newsize)
{
  /* FIXME: implement memory limitation */
  while (newsize > p->bufs->len)
    g_ptr_array_set_size (p->bufs, p->bufs->len * 2);
  p->used_size = newsize;
}

gint64
gst_avsynth_query_duration (GstPad *pad, AVS_VideoInfo *vi)
{
  gint64 duration = -1, time_duration = -1;
  gboolean ret;
  GstFormat qfmt = GST_FORMAT_DEFAULT;
  if (pad)
  {
    ret = gst_pad_query_peer_duration (pad, &qfmt, &duration);
    if (!ret)
    {
      GST_INFO ("Failed to get duration in default format");
      qfmt = GST_FORMAT_TIME;
      ret = gst_pad_query_peer_duration (pad, &qfmt, &time_duration);
      if (!ret)
      {
        GST_INFO ("Failed to get duration in time format");
        duration = -1;
      }
      else
      {
        GstPad *peer = gst_pad_get_peer (pad);
        if (peer)
        {
          qfmt = GST_FORMAT_DEFAULT;
          if (!gst_pad_query_convert (peer, GST_FORMAT_TIME, time_duration, &qfmt, &duration))
          {
            GST_WARNING ("Failed to convert duration from time format to default format");
            gst_object_unref (peer);
            peer = NULL;
          }
          else
            gst_object_unref (peer);
          if (peer == NULL)
          {
            duration = gst_util_uint64_scale (time_duration, vi->fps_numerator,
                vi->fps_denominator * GST_SECOND);
            /* Attempt to round to nearest integer: if the difference is more
             * than 0.5 (less than -0.5), it means that gst_util_uint64_scale()
             * just truncated an integer, while it had to be rounded
             */
    
            duration = duration * GST_SECOND - 
                time_duration * vi->fps_numerator / vi->fps_denominator <= 
                -0.5 ? duration + 1: duration;
          }
        }
      }
    }
  
    if (duration <= -1)
    {
      GST_WARNING ("Reporting duration as -1, may break some filters");
    }
  }
  return duration;
}


gboolean AVSC_CC
gst_avsynth_buf_pad_caps_to_vi (GstBuffer *buf, GstPad *pad, GstCaps *caps, AVS_VideoInfo *vi)
{
  gboolean ret = TRUE;
  GstVideoFormat vf;
  gint fps_num = 0, fps_den = 0;
  gint width = 0, height = 0;
  gboolean interlaced;
  gint64 duration = -1;
 

  ret = gst_video_format_parse_caps (caps, &vf, &width, &height);
  if (!ret)
  {
    GST_ERROR ("Failed to convert caps to videoinfo - can't get format/width/height");
    goto cleanup;
  }
  ret = gst_video_format_parse_caps_interlaced (caps, &interlaced);
  if (!ret)
  {
    GST_ERROR ("Failed to convert caps to videoinfo - can't get interlaced state");
    goto cleanup;
  }

  ret = gst_video_parse_caps_framerate (caps, &fps_num, &fps_den);
  if (!ret)
  {
    GST_ERROR ("Failed to convert caps to videoinfo - can't get fps");
    goto cleanup;
  }

  /* FIXME: What about true 24-bit BGR? */
  switch (vf)
  {
    case GST_VIDEO_FORMAT_I420:
      vi->pixel_type = AVS_CS_I420;
      break;
    case GST_VIDEO_FORMAT_YUY2:
      vi->pixel_type = AVS_CS_YUY2;
      break;
    case GST_VIDEO_FORMAT_BGRx:
      vi->pixel_type = AVS_CS_BGR32;
      break;
    case GST_VIDEO_FORMAT_YV12:
      vi->pixel_type = AVS_CS_YV12;
      break;
    default:
      ret = FALSE;
  }
  if (interlaced)
  {
    vi->image_type = AVS_IT_FIELDBASED;
    if (buf)
    {
      if (GST_BUFFER_FLAG_IS_SET (buf, GST_VIDEO_BUFFER_TFF))
        vi->image_type |= AVS_IT_TFF;
      else
        /* FIXME: Not sure about that. Absence of TFF doesn't always mean BFF*/
        vi->image_type |= AVS_IT_BFF;
    }
  }

  vi->fps_numerator = fps_num;
  vi->fps_denominator = fps_den;
  vi->width = width;
  vi->height = height;

  duration = gst_avsynth_query_duration (pad, vi);

  vi->num_frames = duration;

cleanup:
  return ret;
}

gboolean AVSC_CC
_avs_vcf_add_buffer (AVS_VideoCacheFilter *p, GstPad *pad, GstBuffer *inbuf, AVS_ScriptEnvironment *env)
{
  AVS_VideoFrame *buf_ptr;
  AVS_CacheableVideoFrame *cvf;
  AVSynthSink *sink;

  gboolean ret = TRUE;

  guint8 *in_data;
  guint in_size;
  GstClockTime in_timestamp, in_duration, in_running_time;
  gint64 in_offset;
  GstVideoFormat vf;
  gint in_stride0, in_stride1, in_stride2;
  gint offset0, offset1, offset2;
  gint rowsize0, rowsize1, rowsize2;
  gint height0, height1, height2;

  in_data = GST_BUFFER_DATA (inbuf);
  in_size = GST_BUFFER_SIZE (inbuf);
  in_timestamp = GST_BUFFER_TIMESTAMP (inbuf);
  in_duration = GST_BUFFER_DURATION (inbuf);
  in_offset = GST_BUFFER_OFFSET (inbuf);

  sink = (AVSynthSink *) g_object_get_data (G_OBJECT (pad), "sinkstruct");

  GST_DEBUG ("Video cache %p: locking sinkmutex", (gpointer) p);
  g_mutex_lock (sink->sinkmutex);

  in_running_time = gst_segment_to_running_time (&sink->segment, GST_FORMAT_TIME, in_timestamp);

  if (G_UNLIKELY (!GST_CLOCK_TIME_IS_VALID(sink->first_ts) && GST_CLOCK_TIME_IS_VALID(in_timestamp)))
  {
    sink->first_ts = in_timestamp;
  }

  /* Offset voodoo magic */
  /* No offset on incoming frame */
  if (in_offset == -1)
  {
    /* Do we know offset of the previous frame? */
    if (sink->last_offset > -1)
    {
      in_offset = sink->last_offset + 1;
    }
    else
    {
      /* Try to convert timestamp to offset */
      if (in_timestamp >= 0 && in_running_time >= 0)
      {
        in_offset = gst_util_uint64_scale (in_running_time - sink->first_ts, p->parent.vi.fps_numerator,
            p->parent.vi.fps_denominator * GST_SECOND);
        /* Attempt to round to nearest integer: if the difference is more
         * than 0.5 (less than -0.5), it means that gst_util_uint64_scale()
         * just truncated an integer, while it had to be rounded
         */

        in_offset = in_offset * GST_SECOND - 
            in_running_time * p->parent.vi.fps_numerator / p->parent.vi.fps_denominator <= 
            -0.5 ? in_offset + 1: in_offset;
      }
      else
      {
        GST_ERROR ("Video cache %p: frame offset is unknown", (gpointer) p);
        ret = FALSE;
        goto end;
      }
    }
  }
  /* Offset sanity check */
  if (sink->last_offset > -1)
  {
    /* Non-monotonic offsets */
    if (in_offset < sink->last_offset || in_offset > sink->last_offset + 1)
    {
      GST_WARNING ("Video cache %p: last offset was %" G_GUINT64_FORMAT ", current offset is %" G_GUINT64_FORMAT " - shouldn't it be %" G_GUINT64_FORMAT "?", p, sink->last_offset, in_offset, sink->last_offset + 1);
      in_offset = sink->last_offset + 1;
    }
    else if (in_offset == sink->last_offset)
    {
      GST_WARNING ("Video cache %p: duplicate offsets %" G_GUINT64_FORMAT ", dropping", (gpointer) p, in_offset);
      goto end;
    }
  }

  sink->last_offset = in_offset;

  if (p->size >= p->used_size && !sink->flush)
  {
    GST_DEBUG ("Video cache %p: blocking at frame %" G_GUINT64_FORMAT, (gpointer) p, in_offset);
    while (p->size >= p->used_size && !sink->flush && !sink->seeking)
    {
      GST_DEBUG ("Video cache %p: sleeping while waiting at frame %" G_GUINT64_FORMAT
        ", cache range%" G_GUINT64_FORMAT "+ %" G_GUINT64_FORMAT
        ", size=%" G_GUINT64_FORMAT" , stream lock = %p", (gpointer) p, in_offset, p->rng_from, p->used_size,
        p->size, GST_PAD_GET_STREAM_LOCK (pad));
      g_cond_wait (p->vcache_block_cond, sink->sinkmutex);
      GST_DEBUG ("Video cache %p: woke up while waiting at frame %" G_GUINT64_FORMAT
        ", cache range%" G_GUINT64_FORMAT "+ %" G_GUINT64_FORMAT
        ", size=%" G_GUINT64_FORMAT" , stream lock = %p", (gpointer) p, in_offset, p->rng_from, p->used_size,
        p->size, GST_PAD_GET_STREAM_LOCK (pad));
    }
  }

  /* We've been seeking backwards and the seek wasn't very precise, so
   * we're getting frames previous to the frame we need.
   * Or we're in seek mode and the frame is not the frame we're seeking to.
   * If we've pushed a seek event and it moved the source to a frame after
   * rng_from (i.e. the seek missed), this will turn into infinite loop.
   */
  if (G_UNLIKELY (sink->flush || in_offset < p->rng_from && !sink->seeking || sink->seeking && in_offset != p->rng_from))
  {
    if (sink->flush)
      GST_DEBUG ("Video cache %p: skipping frame %" G_GUINT64_FORMAT " - flushing", (gpointer) p, in_offset);
    else if (in_offset < p->rng_from && !sink->seeking)
      GST_DEBUG ("Video cache %p: skipping frame %" G_GUINT64_FORMAT " < %" G_GUINT64_FORMAT, (gpointer) p, in_offset, p->rng_from);
    else if (sink->seeking && in_offset != p->rng_from)
      GST_DEBUG ("Video cache %p: skipping frame %" G_GUINT64_FORMAT " - seeking to %" G_GUINT64_FORMAT, (gpointer) p, in_offset, p->rng_from);   
    goto end;
  }
  sink->seeking = FALSE;

  gst_avsynth_buf_pad_caps_to_vi (inbuf, pad, GST_BUFFER_CAPS (inbuf), &p->parent.vi);

  gst_video_format_parse_caps (GST_BUFFER_CAPS (inbuf), &vf, NULL, NULL);

  /* Allocate a new frame, with default alignment */
  buf_ptr = _avs_se_vf_new_a (env, &p->vi, AVS_FRAME_ALIGN);

  offset0 = gst_video_format_get_component_offset (vf, 0, p->parent.vi.width, p->parent.vi.height);
  offset1 = gst_video_format_get_component_offset (vf, 1, p->parent.vi.width, p->parent.vi.height);
  offset2 = gst_video_format_get_component_offset (vf, 2, p->parent.vi.width, p->parent.vi.height);

  /* The Spherical Horse in Vacuum: row stride is not guaranteed to match the
   * value returned by this function.
   */
  in_stride0 = gst_video_format_get_row_stride (vf, 0, p->parent.vi.width);
  in_stride1 = gst_video_format_get_row_stride (vf, 1, p->parent.vi.width);
  in_stride2 = gst_video_format_get_row_stride (vf, 2, p->parent.vi.width);

  rowsize0 = gst_video_format_get_component_width (vf, 0, p->parent.vi.width) * gst_video_format_get_pixel_stride (vf, 0);
  rowsize1 = gst_video_format_get_component_width (vf, 1, p->parent.vi.width) * gst_video_format_get_pixel_stride (vf, 1);
  rowsize2 = gst_video_format_get_component_width (vf, 2, p->parent.vi.width) * gst_video_format_get_pixel_stride (vf, 2);

  height0 = gst_video_format_get_component_height (vf, 0, p->parent.vi.height);
  height1 = gst_video_format_get_component_height (vf, 1, p->parent.vi.height);
  height2 = gst_video_format_get_component_height (vf, 2, p->parent.vi.height);

  if (!AVS_IS_PLANAR (&p->parent.vi))
  {
    offset2 = offset1 = offset0;
    in_stride2 = in_stride1 = 0;
    rowsize2 = rowsize1 = 0;
    height2 = height1 = 0;
  }

  _avs_se_bit_blt (env, _avs_vf_get_write_ptr (buf_ptr), _avs_vf_get_pitch (buf_ptr), in_data + offset0, in_stride0, rowsize0, height0);
  // Blit More planes (pitch, rowsize and height should be 0, if none is present)
  _avs_se_bit_blt (env, _avs_vf_get_write_ptr_p (buf_ptr, PLANAR_U), _avs_vf_get_pitch_p (buf_ptr, PLANAR_U), in_data + offset1, in_stride1, rowsize1, height1);
  _avs_se_bit_blt (env, _avs_vf_get_write_ptr_p (buf_ptr, PLANAR_V), _avs_vf_get_pitch_p (buf_ptr, PLANAR_V), in_data + offset2, in_stride2, rowsize2, height2);

  cvf = g_new0 (AVS_CacheableVideoFrame, 1);
  cvf->vf = buf_ptr;
  cvf->touched = FALSE;
  cvf->selfindex = in_offset;
  cvf->countindex = p->framecounter++;
  _avs_vf_set_timestamp (buf_ptr, in_timestamp);
  _avs_vf_set_parity (buf_ptr, p->parent.vi.image_type);

  /* Buffer is full, meaning that a filter is not processing frames
   * fast enough.
   */
  if (G_UNLIKELY (p->used_size <= p->size))
  {
    if (p->size > p->used_size)
      g_critical ("Video cache %p: buffer overflow - %" G_GUINT64_FORMAT " > %" G_GUINT64_FORMAT, (gpointer) p, p->used_size, p->size);
    GST_DEBUG ("Video cache %p: cache is full", (gpointer) p);
    /* Cache is relatively small, we can expand it */
    if (G_UNLIKELY (p->touched_last_time * 3 > p->used_size))
    {
      GST_DEBUG ("Video cache %p: cache is relatively small (%" G_GUINT64_FORMAT " > %" G_GUINT64_FORMAT "), expanding...", (gpointer) p, p->touched_last_time * 3, p->used_size);
      _avs_vcf_resize (p, p->used_size + 1);
    }
    else
      g_critical ("Video cache %p: cache is overflowing!", (gpointer) p);
  }

  /* It is guaranteed that at this moment we have at least one free unused
   * array element left. At least it should be guaranteed...
   */
  GST_DEBUG ("Video cache %p: cache size = %" G_GUINT64_FORMAT ", adding a buffer %p (%p), offset = %" G_GUINT64_FORMAT, (gpointer) p, p->size, cvf, buf_ptr, in_offset);
  g_ptr_array_index (p->bufs, p->size++) = (gpointer) cvf;

  /* We don't really know the number of frame the other thread is waiting for
   * (or even if it waits at all), so we'll send a signal each time we add
   * a buffer
   * FIXME: Maybe we SHOULD know that? Just how taxing g_cond_signal() is?
   */
  GST_DEBUG ("Video cache %p: signaling newframe", (gpointer) p);
  g_cond_signal (p->vcache_cond);

end:
  g_mutex_unlock (sink->sinkmutex);
 
  GST_DEBUG ("Video cache %p: unlocked sinkmutex", (gpointer) p);

  return ret;
}

/* Fetches a buffer from the cache */
AVS_VideoFrame * AVSC_CC
_avs_vcf_get_frame (AVS_Clip *_p, gint64 in_n)
{
  AVS_VideoCacheFilter *p = (AVS_VideoCacheFilter *)_p;
  AVS_VideoFrame *ret = NULL;
  AVS_CacheableVideoFrame *cvf;
  gint64 n;
  gint64 store_rng;
  AVSynthSink *sink;

  sink = (AVSynthSink *) g_object_get_data (G_OBJECT (p->pad), "sinkstruct");

  GST_DEBUG ("Video cache %p: frame %d is requested", (gpointer) p, in_n);

  GST_DEBUG ("Video cache %p: locking sinkmutex", (gpointer) p);
  g_mutex_lock (sink->sinkmutex);

  store_rng = p->rng_from;

  if (sink->firstcall)
  {
    sink->firstcall = FALSE;
    sink->minframe = in_n;
    sink->maxframe = in_n;
  }
  else
  {
    if (in_n < sink->minframe)
      sink->minframe = in_n;
    if (in_n > sink->maxframe)
      sink->maxframe = in_n;
  }

  /* Make sure 0 >= n < num_frames, but only if duration is known */
  if (p->parent.vi.num_frames >= 0)
  {
    n = MIN (p->parent.vi.num_frames - 1, MAX (0, in_n));
    if (in_n > p->parent.vi.num_frames - 1)
    {
      GST_WARNING ("Video cache %p: frame index %d exceedes max frame index %d",
          (gpointer) p, in_n, p->parent.vi.num_frames - 1);
      sink->starving = TRUE;
    }
  }
  else
    n = in_n;

  GST_DEBUG ("Video cache %p: frame %" G_GUINT64_FORMAT
      " of %d is requested. Cache range=%" G_GUINT64_FORMAT "+ %" G_GUINT64_FORMAT
      ", size=%" G_GUINT64_FORMAT, (gpointer) p, n, p->parent.vi.num_frames,
      p->rng_from, p->used_size, p->size);

  /* Filter asked for a frame we haven't reached yet */
  if ((p->size == 0 || p->rng_from + ((gint64) p->size) - 1 < n) && !sink->seek && !sink->seeking)
  {
    GST_DEBUG ("Video cache %p: frame %" G_GUINT64_FORMAT " is not yet in the cache", (gpointer) p, n);

    if (G_UNLIKELY (p->rng_from + ((gint64) p->used_size) - 1 < n))
    {
      GST_DEBUG ("Video cache %p: frame %" G_GUINT64_FORMAT " is out of reach", (gpointer) p, n);
      _avs_vcf_resize (p, n - p->rng_from - p->used_size);
    }

    GST_DEBUG ("Video cache %p: signalling getnewframe", (gpointer) p);
    g_cond_signal (p->vcache_block_cond);

    GST_DEBUG ("Video cache %p: waiting for frame %" G_GUINT64_FORMAT, (gpointer) p, n);
    /* Wait until GStreamer gives us enough frames to reach the frame n */
    /* Don't wait if the sink got EOS (we won't get any more buffers). */
    while ((p->size == 0 || (gint64)p->size - 1 < n - p->rng_from) && !sink->eos && !sink->flush)
    {
      GST_DEBUG ("Video cache %p: sleeping while waiting for frame %" G_GUINT64_FORMAT
        ", cache range%" G_GUINT64_FORMAT "+ %" G_GUINT64_FORMAT
        ", size=%" G_GUINT64_FORMAT" , stream lock = %p", (gpointer) p, n,
        p->rng_from, p->used_size, p->size, GST_PAD_GET_STREAM_LOCK (p->pad));
      g_cond_wait (p->vcache_cond, sink->sinkmutex);
      GST_DEBUG ("Video cache %p: waked up while waiting for frame %" G_GUINT64_FORMAT
        ", cache range%" G_GUINT64_FORMAT "+ %" G_GUINT64_FORMAT
        ", size=%" G_GUINT64_FORMAT" , stream lock = %p", (gpointer) p, n,
        p->rng_from, p->used_size, p->size, GST_PAD_GET_STREAM_LOCK (p->pad));
    }
  }
  /* Filter asked for a frame we already discarded or we have to seek somewhere*/
  if (n < p->rng_from || sink->seek)
  {
    GstEvent *seek_event;

    /* If we are not seeking, it means that the filter has bigger frame
     * range than we thought
     */
    if (!sink->seek)
    {
      GST_DEBUG ("Video cache %p: frame %" G_GUINT64_FORMAT " was left behind", (gpointer) p, n);
      _avs_vcf_resize (p, p->used_size + (p->rng_from - n));
    }

    /* Clear the cache */
    for (guint64 i = 0; i < p->size; i++)
    {
      AVS_CacheableVideoFrame *_cvf = (AVS_CacheableVideoFrame *) g_ptr_array_index (p->bufs, i);

      if (_cvf == NULL)
        break;

      _avs_vf_release (_cvf->vf);
      g_free (cvf);
      g_ptr_array_index (p->bufs, i) = NULL;
    }
    p->size = 0;
    
    /* AddBuffer will discard all frames with number less than rng_from */
    sink->seekhint = sink->seekhint < n ? sink->seekhint : n;
    p->rng_from = sink->seekhint;
    /* Tells AddBuffer that rng_from is set and we are seeking, have to use
     * this since sink->seek could be TRUE before we have a chance to update
     * rng_from.
     */
    sink->seeking = TRUE;

    /* Seek to frame n (or use sinkhint - whichever is smaller)*/
    seek_event = gst_event_new_seek (1, GST_FORMAT_DEFAULT,
        (GstSeekFlags) (GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
        GST_SEEK_TYPE_SET, (gint64) p->rng_from,
        GST_SEEK_TYPE_NONE, -1);

    /* Wake up the stream thread (sinke sink->seek == TRUE, it will abort pending AddBuffer and return
     * control to upstream elements)
     */
    GST_DEBUG ("Video cache %p: signalling getnewframe", (gpointer) p);
    g_cond_signal (p->vcache_block_cond);
    sink->eos = FALSE;

    /* Stream thread won't wake up until we unlock this */
    GST_DEBUG ("Video cache %p: seeking...", (gpointer) p);
    g_mutex_unlock (sink->sinkmutex);

    /* Now we can seek */
    gst_pad_push_event (p->pad, seek_event);
    GST_DEBUG ("Video cache %p: seeked", (gpointer) p);

    /* Wait until GStreamer reaches the frame n. By this time AddBuffer is finished and unlocked
     * the sinkmutex, we can lock it back - we need it for waiting.
     */
    g_mutex_lock (sink->sinkmutex);
    GST_DEBUG ("Video cache %p: waiting for frame %" G_GUINT64_FORMAT, (gpointer) p, p->rng_from);
    sink->seek = FALSE;
    while ((p->size == 0 || p->rng_from + (gint64)p->size < sink->seekhint + 1) && (!sink->eos))
    {
      GST_DEBUG ("Video cache %p: waiting at stream lock %p", (gpointer) p, GST_PAD_GET_STREAM_LOCK (p->pad));
      g_cond_wait (p->vcache_cond, sink->sinkmutex);
      GST_DEBUG ("Video cache %p: not waiting at stream lock %p; %" G_GINT64_FORMAT " ? %" G_GINT64_FORMAT ", eos = %d, ", (gpointer) p, GST_PAD_GET_STREAM_LOCK (p->pad), p->rng_from + p->size, sink->seekhint + 1, sink->eos);
    }
    GST_DEBUG ("Video cache %p: not waiting for frame %" G_GUINT64_FORMAT ", size = %" G_GINT64_FORMAT ", seekhint = %" G_GINT64_FORMAT, (gpointer) p, p->rng_from, p->size, sink->seekhint);
  }

  if (n >= p->rng_from && p->rng_from + (gint64)p->size > n)
  {
    cvf = (AVS_CacheableVideoFrame *) g_ptr_array_index (p->bufs, n - p->rng_from);
    ret = cvf->vf;
    _avs_vf_ref (ret);
    GST_DEBUG ("Video cache %p: touching and returning frame %" G_GUINT64_FORMAT ", its calculated index is %" G_GUINT64_FORMAT", self index is %" G_GUINT64_FORMAT", it's %" G_GUINT64_FORMAT "'th frame, %p (%p)", (gpointer) p, n, store_rng + (n - p->rng_from), cvf->selfindex, cvf->countindex, cvf, ret);
/*
    if (n != cvf->selfindex)
      GST_ERROR ("Video cache %p: returning wrong frame: %" G_GUINT64_FORMAT" != %" G_GUINT64_FORMAT, (gpointer) p, n, cvf->selfindex);
*/
    cvf->touched = TRUE;
  }
  else
  {
    if (p->size == 0)
    {
      GST_ERROR ("Returning NULL, rng_from = %" G_GINT64_FORMAT", size = %" G_GINT64_FORMAT", n = %"G_GINT64_FORMAT, p->rng_from, p->size, n);
      ret = NULL;
    } else {
      cvf = (AVS_CacheableVideoFrame *) g_ptr_array_index (p->bufs, p->size - 1);
      ret = cvf->vf;
      _avs_vf_ref (ret);
      cvf->touched = TRUE;
      GST_WARNING ("Returning frame %" G_GINT64_FORMAT " instead of %" G_GINT64_FORMAT", rng_from = %" G_GINT64_FORMAT", size = %" G_GINT64_FORMAT, p->rng_from + p->size - 1, n, p->rng_from, p->size);
    }
     
    if (sink->eos)
      sink->starving = TRUE;
  }

  g_mutex_unlock (sink->sinkmutex);

  GST_DEBUG ("Video cache %p: unlocked sinkmutex", (gpointer) p);

  return ret;
}

gboolean AVSC_CC
_avs_vcf_get_parity(AVS_Clip *_p, gint64 in_n)
{
  AVS_VideoCacheFilter *p = (AVS_VideoCacheFilter *)_p;
  AVS_VideoFrame *vf = NULL;
  AVS_CacheableVideoFrame *cvf;
  gint64 n;
  AVSynthSink *sink;
  gboolean ret = FALSE;

  sink = (AVSynthSink *) g_object_get_data (G_OBJECT (p->pad), "sinkstruct");

  GST_DEBUG ("Video cache %p: locking sinkmutex", (gpointer) p);
  g_mutex_lock (sink->sinkmutex);

  /* Make sure 0 >= n < num_frames, but only if duration is known */
  if (p->parent.vi.num_frames >= 0)
  {
    n = MIN (p->parent.vi.num_frames - 1, MAX (0, in_n));
  }
  else
    n = in_n;

  if (n >= p->rng_from && p->rng_from + (gint64)p->size > n)
  {
    cvf = (AVS_CacheableVideoFrame *) g_ptr_array_index (p->bufs, n - p->rng_from);
    vf = cvf->vf;
    if (vf)
      ret = vf->vfb->image_type & AVS_IT_TFF;
  }

  g_mutex_unlock (sink->sinkmutex);

  GST_DEBUG ("Video cache %p: unlocked sinkmutex", (gpointer) p);

  return ret;
}

/* Probably will return empty buffers */
void AVSC_CC
_avs_vcf_get_audio (AVS_Clip *p, gpointer buf, gint64 start, gint64 count, AVS_ScriptEnvironment* env)
{
}

/* At the moment this function does nothing */
void AVSC_CC
_avs_vcf_set_cache_hints(AVS_Clip *p, gint cachehints, gint64 frame_range)
{
}

AVS_VideoInfo AVSC_CC
_avs_vcf_get_video_info (AVS_Clip *p)
{
  return ((AVS_VideoCacheFilter *)p)->parent.vi;
}

void AVSC_CC
_avs_vcf_clear_untouched (AVS_VideoCacheFilter *p)
{
  gboolean found_touched = FALSE;
  guint64 i;
  guint64 removed = 0;
  AVSynthSink *sink;

  sink = (AVSynthSink *) g_object_get_data (G_OBJECT (p->pad), "sinkstruct");

  g_mutex_lock (sink->sinkmutex);

  p->touched_last_time = 0;

  GST_DEBUG ("Video cache %p: Clearing untouched frames. Cache range=%"
      G_GUINT64_FORMAT "+ %" G_GUINT64_FORMAT ", size=%" G_GUINT64_FORMAT,
      (gpointer) p, p->rng_from, p->used_size, p->size);

  for (i = 0; i < p->size; i++)
  {
    AVS_CacheableVideoFrame *cvf;
    cvf = (AVS_CacheableVideoFrame *) g_ptr_array_index (p->bufs, i);
    /* Empty slot (deleted a buffer recently) */
    if (cvf == NULL)
    {
      /* Shift the contents to the left */
      if (i + 1 < p->size)
      {
        g_ptr_array_index (p->bufs, i) = g_ptr_array_index (p->bufs, i + 1);
        g_ptr_array_index (p->bufs, i + 1) = NULL;
        cvf = (AVS_CacheableVideoFrame *) g_ptr_array_index (p->bufs, i);
      }
    }
    /* Next slot is empty too -> there are no more buffers */
    if (cvf == NULL)
      break;

    if (cvf->touched || found_touched)
    {
      /* This buffer was used in the last cycle */
      if (cvf->touched)
      {
        p->touched_last_time++;
        if (!found_touched)
        {
          GST_DEBUG ("Video cache %p: first touched frame is %" G_GUINT64_FORMAT" (#%" G_GUINT64_FORMAT")", (gpointer) p, removed, p->rng_from + removed);
          /* Don't delete buffers anymore (we don't want any gaps) */
          found_touched = TRUE;
        }
      }
      cvf->touched = FALSE;
    }
    /* Buffer was not touched and there are no touched buffers before it */
    else if (!found_touched)
    {
      GST_DEBUG ("Video cache %p: Removing frame %"
          G_GUINT64_FORMAT, (gpointer) p, i);

      /* Delete it and reiterate */
      _avs_vf_release (cvf->vf);
      g_free (cvf);
      g_ptr_array_index (p->bufs, i) = NULL;
      i--;
      /* Now the first buffer in the cache has a greater frame number */
      removed++;
    }
  }
  GST_DEBUG ("Video cache %p: touched %" G_GUINT64_FORMAT " frames, removed %" G_GUINT64_FORMAT " frames", (gpointer) p, p->touched_last_time, removed);

  p->size -= removed;
  p->rng_from += removed;

  if (p->size < p->used_size)
  {
    GST_DEBUG ("Video cache %p: signalling getnewframe", (gpointer) p);
    g_cond_signal (p->vcache_block_cond);
  }

  g_mutex_unlock (sink->sinkmutex);
}

void AVSC_CC
_avs_vcf_clear (AVS_VideoCacheFilter *p)
{
  AVSynthSink *sink;

  sink = (AVSynthSink *) g_object_get_data (G_OBJECT (p->pad), "sinkstruct");

  g_mutex_lock (sink->sinkmutex);

  GST_DEBUG ("Video cache %p: Clearing all frames", (gpointer) p);

  for (guint64 i = 0; i < p->size; i++)
  {
    AVS_CacheableVideoFrame *cvf = (AVS_CacheableVideoFrame *) g_ptr_array_index (p->bufs, i);

    if (cvf == NULL)
      break;

    _avs_vf_release (cvf->vf);
    g_free (cvf);
    g_ptr_array_index (p->bufs, i) = NULL;
  }

  p->size = 0;

  GST_DEBUG ("Video cache %p: signalling getnewframe", (gpointer) p);
  g_cond_signal (p->vcache_block_cond);

  g_mutex_unlock (sink->sinkmutex);
}

guint64 AVSC_CC
_avs_vcf_get_size (AVS_VideoCacheFilter *p)
{
  return p->size;
};
