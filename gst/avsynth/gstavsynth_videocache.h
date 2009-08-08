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

#ifndef __GST_AVSYNTH_VIDEOCACHE_H__
#define __GST_AVSYNTH_VIDEOCACHE_H__

#include "gstavsynth.h"
#include "gstavsynth_sdk.h"

/*
 * These structures are stored in the cache
 */
typedef struct _AVS_CacheableVideoFrame AVS_CacheableVideoFrame;
struct _AVS_CacheableVideoFrame
{
  AVS_VideoFrame *vf;
  gboolean touched;
  /* Frame index */
  guint64 selfindex;
  /* For debugging */
  guint64 countindex;
};

typedef struct _AVS_VideoCacheFilter AVS_VideoCacheFilter;
struct _AVS_VideoCacheFilter
{
  AVS_GenericVideoFilter parent;
  AVS_Clip *child;
  AVS_DestroyFunc destroy;

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
  gint64 rng_from;
  guint64 used_size, size;

  /* Number of buffer touched last time */
  guint64 touched_last_time;

  GCond *vcache_block_cond;

  /* A pad to which this cache is attached to */
  GstPad *pad;

  /* AVSynthSink->seek is set once we get a seek event, while this variable
   * is set *only* between calls to GetFrame(), so if it will always be TRUE
   * only for the first call to cache's GetFrame() for a given call to
   * filter's GetFrame().
   */
  gboolean seek;

  guint64 framecounter;

  GCond *vcache_cond;

  AVS_VideoInfo vi;
};


void AVSC_CC
_avs_vcf_destroy(AVS_Clip *p, gboolean freeself);

AVS_VideoCacheFilter * AVSC_CC
_avs_vcf_construct (AVS_VideoCacheFilter *p, AVS_VideoInfo init_vi, GstPad *in_pad, gint start_size);
void AVSC_CC
_avs_vcf_resize (AVS_VideoCacheFilter *p, gint64 newsize);

gboolean AVSC_CC
gst_avsynth_buf_pad_caps_to_vi (GstBuffer *buf, GstPad *pad, GstCaps *caps, AVS_VideoInfo *vi);

gboolean AVSC_CC
_avs_vcf_add_buffer (AVS_VideoCacheFilter *p, GstPad *pad, GstBuffer *in, AVS_ScriptEnvironment *env);

AVS_VideoFrame * AVSC_CC
_avs_vcf_get_frame (AVS_Clip *p, gint64 n);

gboolean AVSC_CC
_avs_vcf_get_parity(AVS_Clip *p, gint64 n);

void AVSC_CC
_avs_vcf_get_audio (AVS_Clip *p, gpointer buf, gint64 start, gint64 count, AVS_ScriptEnvironment* env);

void AVSC_CC
_avs_vcf_set_cache_hints(AVS_Clip *p, gint cachehints, gint64 frame_range);

AVS_VideoInfo AVSC_CC
_avs_vcf_get_video_info (AVS_Clip *p);

void AVSC_CC
_avs_vcf_clear_untouched (AVS_VideoCacheFilter *p);

void AVSC_CC
_avs_vcf_clear (AVS_VideoCacheFilter *p);

guint64 AVSC_CC
_avs_vcf_get_size (AVS_VideoCacheFilter *p);

gint64 gst_avsynth_query_duration (GstPad *pad, AVS_VideoInfo *vi);

#endif /* __GST_AVSYNTH_VIDEOCACHE_H__ */
