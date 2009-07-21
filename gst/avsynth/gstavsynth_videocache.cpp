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

#include "gstavsynth_videocache.h"

gboolean gst_avsynth_buf_pad_caps_to_vi (GstBuffer *buf, GstPad *pad, GstCaps *caps, VideoInfo *vi)
{
  gboolean ret = TRUE;
  GstVideoFormat vf;
  gint fps_num = 0, fps_den = 0;
  gint width = 0, height = 0;
  gboolean interlaced;
  GstFormat qfmt = GST_FORMAT_DEFAULT;
  gint64 duration = 0;
 
  if (pad)
    ret = ret && gst_pad_query_peer_duration (pad, &qfmt, &duration);
 
  ret = ret && gst_video_format_parse_caps (caps, &vf, &width, &height);
  ret = ret && gst_video_format_parse_caps_interlaced (caps, &interlaced);
  ret = ret && gst_video_parse_caps_framerate (caps, &fps_num, &fps_den);

  if (!ret)
  {
    GST_ERROR ("Failed to convert caps to videoinfo");
    goto cleanup;
  }

  /* FIXME: What about true 24-bit BGR? */
  switch (vf)
  {
    case GST_VIDEO_FORMAT_I420:
      vi->pixel_type = VideoInfo::CS_I420;
      break;
    case GST_VIDEO_FORMAT_YUY2:
      vi->pixel_type = VideoInfo::CS_YUY2;
      break;
    case GST_VIDEO_FORMAT_BGRx:
      vi->pixel_type = VideoInfo::CS_BGR32;
      break;
    case GST_VIDEO_FORMAT_YV12:
      vi->pixel_type = VideoInfo::CS_YV12;
      break;
    default:
      ret = FALSE;
  }
  if (interlaced)
  {
    vi->image_type = VideoInfo::IT_FIELDBASED;
    if (buf)
    {
      if (GST_BUFFER_FLAG_IS_SET (buf, GST_VIDEO_BUFFER_TFF))
        vi->image_type |= VideoInfo::IT_TFF;
      else
        /* FIXME: Not sure about that. Absence of TFF doesn't always mean BFF*/
        vi->image_type |= VideoInfo::IT_BFF;
    }
  }

  vi->fps_numerator = fps_num;
  vi->fps_denominator = fps_den;
  vi->width = width;
  vi->height = height;
  vi->num_frames = duration;

cleanup:
  return ret;
}

gboolean
GstAVSynthVideoCache::AddBuffer (GstPad *pad, GstBuffer *inbuf, ScriptEnvironment *env)
{
  PVideoFrame *buf_ptr;
  ImplVideoFrameBuffer *ivf;
  AVSynthSink *sink;

  guint8 *in_data;
  guint in_size;
  GstClockTime in_timestamp, in_duration;
  guint64 in_offset;
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

  GST_DEBUG ("Video cache %p: locking sinkmutex", (gpointer) this);
  g_mutex_lock (sink->sinkmutex);

  if (size >= used_size && !sink->flush)
  {
    GST_DEBUG ("Video cache %p: blocking at frame %" G_GUINT64_FORMAT, (gpointer) this, in_offset);
    while (size >= used_size && !sink->flush && !sink->seeking)
    {
      GST_DEBUG ("Video cache %p: sleeping while waiting at frame %" G_GUINT64_FORMAT
        ", cache range%" G_GUINT64_FORMAT "+ %" G_GUINT64_FORMAT
        ", size=%" G_GUINT64_FORMAT" , stream lock = %p", (gpointer) this, in_offset, rng_from, used_size,
        size, GST_PAD_GET_STREAM_LOCK (pad));
      g_cond_wait (vcache_block_cond, sink->sinkmutex);
      GST_DEBUG ("Video cache %p: woke up while waiting at frame %" G_GUINT64_FORMAT
        ", cache range%" G_GUINT64_FORMAT "+ %" G_GUINT64_FORMAT
        ", size=%" G_GUINT64_FORMAT" , stream lock = %p", (gpointer) this, in_offset, rng_from, used_size,
        size, GST_PAD_GET_STREAM_LOCK (pad));
    }
  }

  /* We've been seeking backwards and the seek wasn't very precise, so
   * we're getting frames previous to the frame we need.
   * Or we're in seek mode and the frame is not the frame we're seeking to.
   * If we've pushed a seek event and it moved the source to a frame after
   * rng_from (i.e. the seek missed), this will turn into infinite loop.
   */
  if (G_UNLIKELY (sink->flush || in_offset < rng_from && !sink->seeking || sink->seeking && in_offset != rng_from))
  {
    if (sink->flush)
      GST_DEBUG ("Video cache %p: skipping frame %" G_GUINT64_FORMAT " - flushing", (gpointer) this, in_offset);
    else if (in_offset < rng_from && !sink->seeking)
      GST_DEBUG ("Video cache %p: skipping frame %" G_GUINT64_FORMAT " < %" G_GUINT64_FORMAT, (gpointer) this, in_offset, rng_from);
    else if (sink->seeking && in_offset != rng_from)
      GST_DEBUG ("Video cache %p: skipping frame %" G_GUINT64_FORMAT " - seeking to %" G_GUINT64_FORMAT, (gpointer) this, in_offset, rng_from);   
    goto end;
  }
  sink->seeking = FALSE;

  gst_avsynth_buf_pad_caps_to_vi (inbuf, pad, GST_BUFFER_CAPS (inbuf), &vi);

  gst_video_format_parse_caps (GST_BUFFER_CAPS (inbuf), &vf, NULL, NULL);

  /* Allocate a new frame, with default alignment */
  buf_ptr = g_new0 (PVideoFrame, 1);
  *buf_ptr = (PVideoFrame) dynamic_cast<IScriptEnvironment*>(env)->NewVideoFrame(vi);

  offset0 = gst_video_format_get_component_offset (vf, 0, vi.width, vi.height);
  offset1 = gst_video_format_get_component_offset (vf, 1, vi.width, vi.height);
  offset2 = gst_video_format_get_component_offset (vf, 2, vi.width, vi.height);

  /* The Spherical Horse in Vacuum: row stride is not guaranteed to match the
   * value returned by this function.
   */
  in_stride0 = gst_video_format_get_row_stride (vf, 0, vi.width);
  in_stride1 = gst_video_format_get_row_stride (vf, 1, vi.width);
  in_stride2 = gst_video_format_get_row_stride (vf, 2, vi.width);

  rowsize0 = gst_video_format_get_component_width (vf, 0, vi.width) * gst_video_format_get_pixel_stride (vf, 0);
  rowsize1 = gst_video_format_get_component_width (vf, 1, vi.width) * gst_video_format_get_pixel_stride (vf, 1);
  rowsize2 = gst_video_format_get_component_width (vf, 2, vi.width) * gst_video_format_get_pixel_stride (vf, 2);

  height0 = gst_video_format_get_component_height (vf, 0, vi.height);
  height1 = gst_video_format_get_component_height (vf, 1, vi.height);
  height2 = gst_video_format_get_component_height (vf, 2, vi.height);

  if (!vi.IsPlanar())
  {
    offset2 = offset1 = offset0;
    in_stride2 = in_stride1 = 0;
    rowsize2 = rowsize1 = 0;
    height2 = height1 = 0;
  }

  env->BitBlt((*buf_ptr)->GetWritePtr(), (*buf_ptr)->GetPitch(), in_data + offset0, in_stride0, rowsize0, height0);
  // Blit More planes (pitch, rowsize and height should be 0, if none is present)
  env->BitBlt((*buf_ptr)->GetWritePtr(PLANAR_V), (*buf_ptr)->GetPitch(PLANAR_V), in_data + offset2, in_stride2, rowsize2, height2);
  env->BitBlt((*buf_ptr)->GetWritePtr(PLANAR_U), (*buf_ptr)->GetPitch(PLANAR_U), in_data + offset1, in_stride1, rowsize1, height1);

  ivf = (ImplVideoFrameBuffer *) ((*buf_ptr)->GetFrameBuffer());
  ivf->touched = FALSE;
  ivf->selfindex = in_offset;


  /* Buffer is full, meaning that a filter is not processing frames
   * fast enough.
   */
  if (G_UNLIKELY (used_size <= size))
  {
    if (size > used_size)
      GST_ERROR ("Video cache %p: buffer overflow - %" G_GUINT64_FORMAT " > %" G_GUINT64_FORMAT, (gpointer) this, used_size, size);
    GST_DEBUG ("Video cache %p: cache is full", (gpointer) this);
    /* Cache is relatively small, we can expand it */
    if (G_UNLIKELY (touched_last_time * 3 > used_size))
    {
      GST_DEBUG ("Video cache %p: cache is relatively small (%" G_GUINT64_FORMAT " > %" G_GUINT64_FORMAT "), expanding...", (gpointer) this, touched_last_time * 3, used_size);
      Resize (used_size + 1);
    }
    else
      GST_ERROR ("Video cache %p: cache is overflowing!", (gpointer) this);
  }

  /* It is guaranteed that at this moment we have at least one free unused
   * array element left. At least it should be guaranteed...
   */
  GST_DEBUG ("Video cache %p: cache size = %" G_GUINT64_FORMAT ", adding a buffer %p (%p), offset = %" G_GUINT64_FORMAT, (gpointer) this, size, ivf, buf_ptr, in_offset);
  g_ptr_array_index (bufs, size++) = (gpointer) buf_ptr;

  /* We don't really know the number of frame the other thread is waiting for
   * (or even if it waits at all), so we'll send a signal each time we add
   * a buffer
   * FIXME: Maybe we SHOULD know that? Just how taxing g_cond_signal() is?
   */
  GST_DEBUG ("Video cache %p: signaling newframe", (gpointer) this);
  g_cond_signal (vcache_cond);

end:
  g_mutex_unlock (sink->sinkmutex);
 
  GST_DEBUG ("Video cache %p: unlocked sinkmutex", (gpointer) this);

  return TRUE;

}

/* Do not call Resize unless you locked the sinkmutex */
void
GstAVSynthVideoCache::Resize(gint64 newsize)
{
  /* FIXME: implement memory limitation */
  while (newsize > bufs->len)
    g_ptr_array_set_size (bufs, bufs->len * 2);
  used_size = newsize;
}

PVideoFrame __stdcall
GstAVSynthVideoCache::GetFrame(int in_n, IScriptEnvironment* env)
{
  PVideoFrame *ret = NULL;
  ImplVideoFrameBuffer *ivf;
  guint64 n;
  gint64 store_rng;
  AVSynthSink *sink;

  sink = (AVSynthSink *) g_object_get_data (G_OBJECT (pad), "sinkstruct");

  GST_DEBUG ("Video cache %p: frame %d is requested", (gpointer) this, in_n);

  GST_DEBUG ("Video cache %p: locking sinkmutex", (gpointer) this);
  g_mutex_lock (sink->sinkmutex);

  store_rng = rng_from;

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
  if (vi.num_frames >= 0)
  {
    n = MIN (vi.num_frames - 1, MAX (0, in_n));
    if (in_n > vi.num_frames - 1)
    {
      GST_WARNING ("Video cache %p: frame index %d exceedes max frame index %d",
          (gpointer) this, in_n, vi.num_frames - 1);
      sink->starving = TRUE;
    }
  }
  else
    n = in_n;

  GST_DEBUG ("Video cache %p: frame %" G_GUINT64_FORMAT
      " of %d is requested. Cache range=%" G_GUINT64_FORMAT "+ %" G_GUINT64_FORMAT
      ", size=%" G_GUINT64_FORMAT, (gpointer) this, n, vi.num_frames, rng_from, used_size,
      size);

  /* Filter asked for a frame we haven't reached yet */
  if ((size == 0 || rng_from + size - 1 < n) && !sink->seek && !sink->seeking)
  {
    GST_DEBUG ("Video cache %p: frame %" G_GUINT64_FORMAT " is not yet in the cache", (gpointer) this, n);

    if (G_UNLIKELY (rng_from + used_size - 1 < n))
    {
      GST_DEBUG ("Video cache %p: frame %" G_GUINT64_FORMAT " is out of reach", (gpointer) this, n);
      Resize (n - rng_from - used_size);
    }

    GST_DEBUG ("Video cache %p: signalling getnewframe", (gpointer) this);
    g_cond_signal (vcache_block_cond);

    GST_DEBUG ("Video cache %p: waiting for frame %" G_GUINT64_FORMAT, (gpointer) this, n);
    /* Wait until GStreamer gives us enough frames to reach the frame n */
    /* Don't wait if the sink got EOS (we won't get any more buffers). */
    while ((size == 0 || size - 1 < n - rng_from) && !sink->eos && !sink->flush)
    {
      GST_DEBUG ("Video cache %p: sleeping while waiting for frame %" G_GUINT64_FORMAT
        ", cache range%" G_GUINT64_FORMAT "+ %" G_GUINT64_FORMAT
        ", size=%" G_GUINT64_FORMAT" , stream lock = %p", (gpointer) this, n, rng_from, used_size,
        size, GST_PAD_GET_STREAM_LOCK (pad));
      g_cond_wait (vcache_cond, sink->sinkmutex);
      GST_DEBUG ("Video cache %p: waked up while waiting for frame %" G_GUINT64_FORMAT
        ", cache range%" G_GUINT64_FORMAT "+ %" G_GUINT64_FORMAT
        ", size=%" G_GUINT64_FORMAT" , stream lock = %p", (gpointer) this, n, rng_from, used_size,
        size, GST_PAD_GET_STREAM_LOCK (pad));
    }
  }
  /* Filter asked for a frame we already discarded or we have to seek somewhere*/
  if (n < rng_from || sink->seek)
  {
    GstEvent *seek_event;

    /* If we are not seeking, it means that the filter has bigger frame
     * range than we thought
     */
    if (!sink->seek)
    {
      GST_DEBUG ("Video cache %p: frame %" G_GUINT64_FORMAT " was left behind", (gpointer) this, n);
      Resize (used_size + (rng_from - n));
    }

    /* Clear the cache */
    for (guint64 i = 0; i < size; i++)
    {
      PVideoFrame *vf = (PVideoFrame *) g_ptr_array_index (bufs, i);

      if (vf == NULL)
        break;

      delete vf;
      g_ptr_array_index (bufs, i) = NULL;
    }
    size = 0;
    
    /* AddBuffer will discard all frames with number less than rng_from */
    sink->seekhint = sink->seekhint < n ? sink->seekhint : n;
    rng_from = sink->seekhint;
    /* Tells AddBuffer that rng_from is set and we are seeking, have to use
     * this since sink->seek could be TRUE before we have a chance to update
     * rng_from.
     */
    sink->seeking = TRUE;

    /* Seek to frame n (or use sinkhint - whichever is smaller)*/
    seek_event = gst_event_new_seek (1, GST_FORMAT_DEFAULT,
        (GstSeekFlags) (GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
        GST_SEEK_TYPE_SET, (gint64) rng_from,
        GST_SEEK_TYPE_NONE, -1);

    /* Wake up the stream thread (sinke sink->seek == TRUE, it will abort pengin AddBuffer and return
     * control to upstream elements)
     */
    GST_DEBUG ("Video cache %p: signalling getnewframe", (gpointer) this);
    g_cond_signal (vcache_block_cond);
    sink->eos = FALSE;

    /* Stream thread won't wake up until we unlock this */
    GST_DEBUG ("Video cache %p: seeking...", (gpointer) this);
    g_mutex_unlock (sink->sinkmutex);

    /* Now we can seek */
    gst_pad_push_event (pad, seek_event);
    GST_DEBUG ("Video cache %p: seeked", (gpointer) this);

    /* Wait until GStreamer reaches the frame n. By this time AddBuffer is finished and unlocked
     * the sinkmutex, we can lock it back - we need it for waiting.
     */
    g_mutex_lock (sink->sinkmutex);
    GST_DEBUG ("Video cache %p: waiting for frame %" G_GUINT64_FORMAT, (gpointer) this, rng_from);
    sink->seek = FALSE;
    while ((size == 0 || rng_from + size < sink->seekhint + 1) && (!sink->eos))
    {
      GST_DEBUG ("Video cache %p: waiting at stream lock %p", (gpointer) this, GST_PAD_GET_STREAM_LOCK (pad));
      g_cond_wait (vcache_cond, sink->sinkmutex);
      GST_DEBUG ("Video cache %p: not waiting at stream lock %p; %" G_GINT64_FORMAT " ? %" G_GINT64_FORMAT ", eos = %d, ", (gpointer) this, GST_PAD_GET_STREAM_LOCK (pad), rng_from + size, sink->seekhint + 1, sink->eos);
    }
    GST_DEBUG ("Video cache %p: not waiting for frame %" G_GUINT64_FORMAT ", size = %" G_GINT64_FORMAT ", seekhint = %" G_GINT64_FORMAT, (gpointer) this, rng_from, size, sink->seekhint);
  }

  if (n >= rng_from && rng_from + size > n)
  {
    ret = (PVideoFrame *) g_ptr_array_index (bufs, n - rng_from);
    ivf = (ImplVideoFrameBuffer *) (*ret)->GetFrameBuffer();
    GST_DEBUG ("Video cache %p: touching and returning frame %" G_GUINT64_FORMAT ", its calculated index is %" G_GUINT64_FORMAT", self index is %" G_GUINT64_FORMAT", %p (%p)", (gpointer) this, n, store_rng + (n - rng_from), ivf->selfindex, ivf, ret);
    if (n != ivf->selfindex)
      GST_ERROR ("Video cache %p: returning wrong frame: %" G_GUINT64_FORMAT" != %" G_GUINT64_FORMAT, (gpointer) this, n, ivf->selfindex);
    ivf->touched = TRUE;
  }
  else
  {
    if (size == 0)
    {
      ret = new PVideoFrame();
      GST_WARNING ("Returning empty frame, rng_from = %" G_GINT64_FORMAT", size = %" G_GINT64_FORMAT", n = %"G_GINT64_FORMAT, rng_from, size, n);
    } else {
      ret = (PVideoFrame *) g_ptr_array_index (bufs, size - 1);
      ivf = (ImplVideoFrameBuffer *) (*ret)->GetFrameBuffer();
      ivf->touched = TRUE;
      GST_WARNING ("Returning frame %" G_GINT64_FORMAT " instead of %" G_GINT64_FORMAT", rng_from = %" G_GINT64_FORMAT", size = %" G_GINT64_FORMAT, rng_from + size - 1, n, rng_from, size);
    }
     
    if (sink->eos)
      sink->starving = TRUE;
  }

  g_mutex_unlock (sink->sinkmutex);

  GST_DEBUG ("Video cache %p: unlocked sinkmutex", (gpointer) this);

  return *ret;
}

bool __stdcall
GstAVSynthVideoCache::GetParity(int n)
{
  /* FIXME: return something meaningful */
  return false;
}

void __stdcall
GstAVSynthVideoCache::GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env)
{
  GST_ERROR ("GetAudio is not implemented");
}

void __stdcall
GstAVSynthVideoCache::SetCacheHints(int cachehints,int frame_range)
{
  /* TODO: Implement? */
}

const VideoInfo& __stdcall
GstAVSynthVideoCache::GetVideoInfo()
{
  return vi;
}

void
GstAVSynthVideoCache::ClearUntouched()
{
  gboolean found_touched = FALSE;
  guint64 removed = 0;
  ImplVideoFrameBuffer *ivf;
  AVSynthSink *sink;

  sink = (AVSynthSink *) g_object_get_data (G_OBJECT (pad), "sinkstruct");

  g_mutex_lock (sink->sinkmutex);

  touched_last_time = 0;

  GST_DEBUG ("Video cache %p: Clearing untouched frames. Cache range=%"
      G_GUINT64_FORMAT "+ %" G_GUINT64_FORMAT ", size=%" G_GUINT64_FORMAT,
      (gpointer) this, rng_from, used_size, size);

  for (guint64 i = 0; i < size; i++)
  {
    PVideoFrame *vf = (PVideoFrame *) g_ptr_array_index (bufs, i);
    /* Empty slot (deleted a buffer recently) */
    if (vf == NULL)
    {
      /* Shift the contents to the left */
      if (i + 1 < size)
      {
        g_ptr_array_index (bufs, i) = g_ptr_array_index (bufs, i + 1);
        g_ptr_array_index (bufs, i + 1) = NULL;
        vf = (PVideoFrame *) g_ptr_array_index (bufs, i);
      }
    }
    /* Next slot is empty too -> there are no more buffers */
    if (vf == NULL)
      break;

    ivf = (ImplVideoFrameBuffer *) (*vf)->GetFrameBuffer();
    if (ivf->touched || found_touched)
    {
      /* This buffer was used in the last cycle */
      if (ivf->touched)
      {
        touched_last_time++;
        if (!found_touched)
        {
          GST_DEBUG ("Video cache %p: first touched frame is %" G_GUINT64_FORMAT" (#%" G_GUINT64_FORMAT")", (gpointer) this, removed, rng_from + removed);
          /* Don't delete buffers anymore (we don't want any gaps) */
          found_touched = TRUE;
        }
      }
      ivf->touched = FALSE;
    }
    /* Buffer was not touched and there are no touched buffers before it */
    else if (!found_touched)
    {
      GST_DEBUG ("Video cache %p: Removing frame %"
          G_GUINT64_FORMAT, (gpointer) this, i);

      /* Delete it and reiterate */
      delete vf;
      g_ptr_array_index (bufs, i) = NULL;
      i--;
      /* Now the first buffer in the cache has a greater frame number */
      removed++;
    }
  }
  GST_DEBUG ("Video cache %p: touched %" G_GUINT64_FORMAT " frames, removed %" G_GUINT64_FORMAT " frames", (gpointer) this, touched_last_time, removed);

  size -= removed;
  rng_from += removed;

  if (size < used_size)
  {
    GST_DEBUG ("Video cache %p: signalling getnewframe", (gpointer) this);
    g_cond_signal (vcache_block_cond);
  }

  g_mutex_unlock (sink->sinkmutex);
}

void
GstAVSynthVideoCache::Clear()
{
  ImplVideoFrameBuffer *ivf;
  AVSynthSink *sink;

  sink = (AVSynthSink *) g_object_get_data (G_OBJECT (pad), "sinkstruct");

  g_mutex_lock (sink->sinkmutex);

  GST_DEBUG ("Video cache %p: Clearing all frames", (gpointer) this);

  for (guint64 i = 0; i < size; i++)
  {
    PVideoFrame *vf = (PVideoFrame *) g_ptr_array_index (bufs, i);

    if (vf == NULL)
      break;

    ivf = (ImplVideoFrameBuffer *) (*vf)->GetFrameBuffer();

    delete vf;
    g_ptr_array_index (bufs, i) = NULL;
  }

  size  = 0;

  GST_DEBUG ("Video cache %p: signalling getnewframe", (gpointer) this);
  g_cond_signal (vcache_block_cond);

  g_mutex_unlock (sink->sinkmutex);
}
