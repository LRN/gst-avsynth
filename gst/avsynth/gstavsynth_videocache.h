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

#include <gst/video/video.h>
#include "gstavsynth_scriptenvironment.h"

#ifndef __GST_AVSYNTH_VIDEOCACHE_H__
#define __GST_AVSYNTH_VIDEOCACHE_H__

/* This is a cache that caches frames coming from upstream
 * to be presented to underlying filter.
 */
class GstAVSynthVideoCache: public PClip
{
private:
  GPtrArray /*of ptrs to PVideoFrame*/ *bufs;
  /* Array starts at a user-specified (or default) size and grows twice
   * each time its size is found insufficient.
   * rng_from - number of the first frame in the array
   * If the cache receives a frame with its number being less than rng_from,
   * it discards it...i think. That should allow it to extend the cache
   * backwards easily enough...or not...we'll see.
   * used_size - number of elements really used for storage
   * size - number of filled elements
   */
  guint64 rng_from;
  guint64 used_size, size;

  /* Number of buffer touched last time */
  guint64 touched_last_time;

  /* Changes used_size to newsize. If used_size becomes larger than size,
   * extends the cache
   */
  void Resize (gint64 newsize);

  /* Caches the information about the video sequence.
   * Some parts of it stays the same after _setcaps(), while other
   * (such as field order for interlaced clips) may change over time.
   */
  VideoInfo vi;

  GMutex *vcache_mutex;
  GCond *vcache_cond;

  /* A pad to which this cache is attached */
  GstPad *pad;

public:
  /* start_size defines both the size of underlying array and the number of
   * elements of that array used for cache.
   */
  GstAVSynthVideoCache(VideoInfo *init_vi, GstPad *in_pad, gint start_size = 10): bufs(g_ptr_array_new ()), rng_from (0), used_size (start_size), size (0), touched_last_time(0)
  {
    g_memmove (&vi, init_vi, sizeof (VideoInfo));
    vcache_mutex = g_mutex_new();
    vcache_cond = g_cond_new();
    pad = in_pad;
    g_object_ref (pad);
    g_ptr_array_set_size (bufs, start_size);
  };

  ~GstAVSynthVideoCache()
  {
    for (guint i = 0; i < bufs->len; i++)
      delete (PVideoFrame *) g_ptr_array_index (bufs, i);
    g_ptr_array_free (bufs, TRUE);
    g_object_unref (pad);
    g_mutex_unlock (vcache_mutex);
    g_mutex_free (vcache_mutex);
    g_cond_free (vcache_cond);
  }

  gboolean AddBuffer (GstPad *pad, GstBuffer *in, ScriptEnvironment *env);

  /* Fetches a buffer from the cache */
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);

  /* Haven't figured out yet what it does */
  bool __stdcall GetParity(int n);  // return field parity if field_based, else parity of first field in frame

  /* Probably will return empty buffers */
  void __stdcall GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env);  // start and count are in samples

  /* At the moment this function does nothing */
  void __stdcall SetCacheHints(int cachehints,int frame_range);

  const VideoInfo& __stdcall GetVideoInfo();

  void ClearUntouched();
};

#endif /* __GST_AVSYNTH_VIDEOCACHE_H__ */