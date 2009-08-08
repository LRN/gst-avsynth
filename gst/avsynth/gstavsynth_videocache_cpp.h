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

#ifndef __GST_AVSYNTH_VIDEOCACHE_CPP_H__
#define __GST_AVSYNTH_VIDEOCACHE_CPP_H__

#include <gst/video/video.h>
#include "gstavsynth_sdk.h"
#include "gstavsynth_sdk_cpp.h"

#include "gstavsynth_videocache.h"
#include "gstavsynth_scriptenvironment_cpp.h"


/* This is a cache that caches frames coming from upstream
 * to be presented to underlying filter.
 */
class GstAVSynthVideoCache: public IClip
{
private:
  VideoInfo private_vi;

public:
  AVS_VideoCacheFilter *vcf;

  GstAVSynthVideoCache(VideoInfo *init_vi, GstPad *in_pad, gint start_size = 10);

  __stdcall ~GstAVSynthVideoCache();

  gboolean AddBuffer (GstPad *pad, GstBuffer *in, ScriptEnvironment *env);

  /* Fetches a buffer from the cache */
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);

  bool __stdcall GetParity(int n);  // return field parity if field_based, else parity of first field in frame

  void __stdcall GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env);  // start and count are in samples

  void __stdcall SetCacheHints(int cachehints,int frame_range);

  const VideoInfo& __stdcall GetVideoInfo();

  guint64 GetSize ();

  void ClearUntouched();

  void Clear();
};

#endif /* __GST_AVSYNTH_VIDEOCACHE_CPP_H__ */
