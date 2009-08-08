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

#include "gstavsynth_videocache_cpp.h"
#include "gstavsynth_scriptenvironment.h"

GstAVSynthVideoCache::GstAVSynthVideoCache(VideoInfo *init_vi, GstPad *in_pad, gint start_size)
{
  vcf = _avs_vcf_construct (NULL, *(AVS_VideoInfo *)init_vi, in_pad, start_size);
};

__stdcall GstAVSynthVideoCache::~GstAVSynthVideoCache()
{
  _avs_clip_release ((AVS_Clip*) vcf);
}

gboolean GstAVSynthVideoCache::AddBuffer (GstPad *pad, GstBuffer *in, ScriptEnvironment *env)
{
  return _avs_vcf_add_buffer (vcf, pad, in, env->env_c);
}

/* Fetches a buffer from the cache */
PVideoFrame __stdcall GstAVSynthVideoCache::GetFrame(int n, IScriptEnvironment* env)
{
  ImplVideoFrame *vf;
  ImplVideoFrameBuffer *ivfb;
  AVS_VideoFrame *avf = _avs_vcf_get_frame ((AVS_Clip*) vcf, n);
  ivfb = new ImplVideoFrameBuffer (avf->vfb);
  _avs_vf_release (avf);
  vf = new ImplVideoFrame(ivfb, avf->offset, avf->pitch, avf->row_size, avf->height, avf->offsetU, avf->offsetV, avf->pitchUV);
  return PVideoFrame(vf);
}

bool __stdcall GstAVSynthVideoCache::GetParity(int n)  // return field parity if field_based, else parity of first field in frame
{
  return _avs_vcf_get_parity ((AVS_Clip*) vcf, n);
}

void __stdcall GstAVSynthVideoCache::GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env)  // start and count are in samples
{
  /* Not implemented */
}

void __stdcall GstAVSynthVideoCache::SetCacheHints(int cachehints,int frame_range)
{
  /* Not implemented */
}

const VideoInfo& __stdcall GstAVSynthVideoCache::GetVideoInfo()
{
  *((AVS_VideoInfo *)&private_vi) = _avs_vcf_get_video_info ((AVS_Clip*) vcf);
  return private_vi;
}

guint64 GstAVSynthVideoCache::GetSize ()
{
  return _avs_vcf_get_size (vcf);
};

void GstAVSynthVideoCache::ClearUntouched()
{
  return _avs_vcf_clear_untouched (vcf);
};

void GstAVSynthVideoCache::Clear()
{
  return _avs_vcf_clear (vcf);
};
