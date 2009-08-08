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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <string.h>

#include <gst/gst.h>

#include "gstavsynth.h"
#include "gstavsynth_videocache.h"
#include "gstavsynth_videofilter.h"
#include "gstavsynth_scriptenvironment.h"
#if HAVE_ORC
G_BEGIN_DECLS
#include <orc/orcprogram.h>
G_END_DECLS
#endif

gchar *plugindir_var = NULL;

/*
class C_VideoFilter : public IClip {
public: // but don't use
  AVS_Clip child;
  AVS_ScriptEnvironment env;
  AVS_FilterInfo d;
public:
  C_VideoFilter() {memset(&d,0,sizeof(d)); _avs_script_environment_init (&env); }
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
  void __stdcall GetAudio(void * buf, __int64 start, __int64 count, IScriptEnvironment* env);
  const VideoInfo & __stdcall GetVideoInfo();
  bool __stdcall GetParity(int n);
  void __stdcall SetCacheHints(int cachehints,int frame_range);
  __stdcall ~C_VideoFilter();
};
*/

void BitBlt(guint8* dstp, int dst_pitch, const guint8* srcp, int src_pitch, int row_size, int height) {
  if ((!height) || (!row_size)) return;
  if (height == 1 || (dst_pitch == src_pitch && src_pitch == row_size)){
    memcpy(dstp, srcp, row_size * height);
  } else {
#if HAVE_ORC
    static OrcProgram *p = NULL;
    OrcExecutor _ex;
    OrcExecutor *ex = &_ex;

    gint i = 0;
    static gboolean dbg = FALSE;

    if (G_UNLIKELY (p == NULL))
    {
      int ret;
  
      p = orc_program_new_ds (1, 1);
      orc_program_append_ds_str (p, "copyb", "d1", "s1");
  
      ret = orc_program_compile (p);
      if (!ORC_COMPILE_RESULT_IS_SUCCESSFUL(ret)) {
        GST_ERROR ("Orc compiler failure");
        AvisynthError ("Failed to compile Orc version of BitBlt");
      }
    }
    ex = orc_executor_new (p);
    orc_executor_set_n (ex, row_size);
    for (i = 0; i < height; i++)
    {
      orc_executor_set_array_str (ex, "s1", src_ptr);
      orc_executor_set_array_str (ex, "d1", dst_ptr);
      orc_executor_run (ex);
      src_ptr += src_pitch;
      dst_ptr += dst_pitch;
    }
    orc_executor_free (ex);

    if (G_UNLIKELY (dbg))
    {
      gboolean ok = TRUE;
      for (i = 0; i < height && ok; i++)
      {
        for (int j = 0; j < row_size && ok; j++)
          ok = (dst_ptr[j] == src_ptr[j]);
        src_ptr += src_pitch;
        dst_ptr += dst_pitch;
      }
      if (!ok)
      {
        GST_ERROR ("BitBlt did something wrong");
      }
    }
#else /* !HAVE_ORC */
    int y;
    for (y = height; y > 0; --y)
    {
      g_memmove(dstp, srcp, row_size);
      dstp += dst_pitch;
      srcp += src_pitch;
    }
#endif /* HAVE_ORC */
  }
}

void AVSC_CC
_avs_vfb_destroy (AVS_VideoFrameBuffer *p)
{
  g_assert (p->refcount == 0);

  if (p->data)
    g_free (p->data);

  g_free (p);
}

void AVSC_CC
_avs_vfb_release (AVS_VideoFrameBuffer *p)
{
  if (g_atomic_int_dec_and_test (&p->refcount))
    _avs_vfb_destroy (p);
}

AVS_VideoFrameBuffer * AVSC_CC
_avs_vfb_new (gint size)
{
  AVS_VideoFrameBuffer *vfb = g_new0 (AVS_VideoFrameBuffer, 1);

  if (size)
  {
    vfb->data = g_new (guint8, size);
  }
  else
  {
    vfb->data = NULL;
  }

  vfb->data_size = size;
  vfb->refcount = 0;
  vfb->timestamp = 0;
  vfb->image_type = 0;
  
  return vfb;
}

AVS_VideoFrameBuffer * AVSC_CC
_avs_vfb_copy (AVS_VideoFrameBuffer *p)
{
  AVS_VideoFrameBuffer *newvfb;
  newvfb = _avs_vfb_new (p->data_size);

  newvfb->timestamp = p->timestamp;
  newvfb->image_type = p->image_type;
  g_memmove (newvfb->data, p->data, p->data_size);

  return newvfb;
}

/*****************
 *
 * AVS_VideoFrame
 *
 */

int AVSC_CC
_avs_vf_get_offset (const AVS_VideoFrame *p)
{
  return p->offset;
}

int AVSC_CC
_avs_vf_get_offset_p (const AVS_VideoFrame *p, int plane)
{
  switch (plane)
  {
    case PLANAR_U:
      return p->offsetU;
    case PLANAR_V:
      return p->offsetV;
    default:
      return p->offset;
  };
}

int AVSC_CC
_avs_vf_get_pitch(const AVS_VideoFrame *p)
{
  return p->pitch;
}

int AVSC_CC
_avs_vf_get_pitch_p(const AVS_VideoFrame *p, int plane)
{ 
  switch (plane)
  {
    case PLANAR_U:
    case PLANAR_V:
      return p->pitchUV;
  }
  return p->pitch;
}

int AVSC_CC
_avs_vf_get_row_size(const AVS_VideoFrame *p)
{
  return p->row_size;
}


int AVSC_CC
_avs_vf_get_row_size_p(const AVS_VideoFrame *p, int plane)
{ 
  int r;
  switch (plane)
  {
    case PLANAR_U:
    case PLANAR_V:
      if (p->pitchUV)
        return p->row_size >> 1;
      else
        return 0;
    case PLANAR_U_ALIGNED:
    case PLANAR_V_ALIGNED:
      if (p->pitchUV) {
        r = ((p->row_size + AVS_FRAME_ALIGN - 1) & (~(AVS_FRAME_ALIGN - 1)) ) >> 1; // Aligned rowsize
        if (r <= p->pitchUV)
          return r;
        return p->row_size >> 1;
      }
      else
        return 0;
    case PLANAR_Y_ALIGNED:
      r = (p->row_size + AVS_FRAME_ALIGN - 1) & (~(AVS_FRAME_ALIGN - 1)); // Aligned rowsize
      if (r <= p->pitch)
        return r;
      return p->row_size;
  }
  return p->row_size;
}


int AVSC_CC
_avs_vf_get_height(const AVS_VideoFrame *p)
{
  return p->height;
}


int AVSC_CC
_avs_vf_get_height_p(const AVS_VideoFrame *p, int plane)
{
  switch (plane)
  {
    case PLANAR_U:
    case PLANAR_V:
      if (p->pitchUV)
        return p->height >> 1;
      return 0;
  }
  return p->height;
}


const guint8* AVSC_CC
_avs_vf_get_read_ptr(const AVS_VideoFrame *p)
{
  return p->vfb->data + _avs_vf_get_offset (p);
}


const guint8* AVSC_CC
_avs_vf_get_read_ptr_p(const AVS_VideoFrame *p, int plane) 
{
  return p->vfb->data + _avs_vf_get_offset_p (p, plane);
}


int AVSC_CC
_avs_vf_is_writable(const AVS_VideoFrame *p)
{
  /* We can only change ViodeFrameBuffer's content when
   * no one but us holds a ref to it and we are reffed only once.
   */
  return p->refcount == 1 && p->vfb->refcount == 1;
}


guint8* AVSC_CC
_avs_vf_get_write_ptr(const AVS_VideoFrame *p)
{
  if (p->vfb->refcount > 1)
  {
    /* Use _avs_vf_make_writable() before writing! */
    g_critical ("Internal Error - refcount was more than one!");
  }
  return p->vfb->data + _avs_vf_get_offset (p);
}

guint8* AVSC_CC
_avs_vf_get_write_ptr_p(const AVS_VideoFrame *p, gint plane) 
{
  if (p->vfb->refcount > 1)
  {
    /* Use _avs_vf_make_writable() before writing! */
    g_critical ("Internal Error - refcount was more than one!");
  }
  return p->vfb->data + _avs_vf_get_offset_p (p, plane);
}

int AVSC_CC
_avs_vf_get_parity(AVS_VideoFrame *p)
{
  return p->vfb->image_type;
}


void AVSC_CC
_avs_vf_set_parity (AVS_VideoFrame *p, int newparity)
{
  if (p->vfb->refcount > 1)
  {
    /* Use _avs_vf_make_writable() before writing! */
    g_critical ("Internal Error - refcount was more than one!");
  }
  p->vfb->image_type = newparity;
}


guint64 AVSC_CC
_avs_vf_get_timestamp (AVS_VideoFrame *p)
{
  return p->vfb->timestamp;
}


void AVSC_CC
_avs_vf_set_timestamp (AVS_VideoFrame *p, guint64 newtimestamp)
{
  if (p->vfb->refcount > 1)
  {
    /* Use _avs_vf_make_writable() before writing! */
    g_critical ("Internal Error - refcount was more than one!");
  }
  p->vfb->timestamp = newtimestamp;
}


void AVSC_CC
_avs_vf_ref (AVS_VideoFrame *p)
{
  g_atomic_int_inc (&p->refcount);
}

void AVSC_CC
_avs_vf_destroy (AVS_VideoFrame *p)
{
  g_assert (p->refcount == 0);
  if (p->vfb)
    _avs_vfb_release (p->vfb);
  g_free (p);
}

void AVSC_CC
_avs_vf_release(AVS_VideoFrame *p)
{
  if (g_atomic_int_dec_and_test (&p->refcount))
    _avs_vf_destroy (p);
}
/*
AVS_VideoFrame * AVSC_CC
_avs_vf_copy(AVS_VideoFrame * p)
{
  AVS_VideoFrame *fnew = g_new0 (AVS_VideoFrame, 1);
  g_memmove (&fnew, p, sizeof (AVS_VideoFrame));
  _avs_vf_release (p)
  return fnew;
}
*/
/////////////////////////////////////////////////////////////////////
//
// C_VideoFilter
//
/*
PVideoFrame
C_VideoFilter::GetFrame(int n, IScriptEnvironment* env) 
{
  if (d.get_frame) {
    d.error = NULL;
    AVS_VideoFrame * f = d.get_frame(&d, n);
    if (d.error)
      throw AvisynthError(d.error);
    PVideoFrame fr((VideoFrame *)f);
    ((PVideoFrame *)&f)->~PVideoFrame();  
    return fr;
  } else {
    return d.child->clip->GetFrame(n, env); 
  }
}

void __stdcall
C_VideoFilter::GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env) 
{
  if (d.get_audio) {
    d.error = NULL;
    d.get_audio(&d, buf, start, count);
    if (d.error)
      throw AvisynthError(d.error);
  } else {
    d.child->clip->GetAudio(buf, start, count, env);
  }
}

const VideoInfo& __stdcall
C_VideoFilter::GetVideoInfo() 
{
  return *(VideoInfo *)&d.vi; 
}

bool __stdcall
C_VideoFilter::GetParity(int n) 
{
  if (d.get_parity) {
    d.error = NULL;
    int res = d.get_parity(&d, n);
    if (d.error)
      throw AvisynthError(d.error);
    return !!res;
  } else {
    return d.child->clip->GetParity(n);
  }
}

void __stdcall
C_VideoFilter::SetCacheHints(int cachehints, int frame_range) 
{
  if (d.set_cache_hints) {
    d.error = NULL;
    d.set_cache_hints(&d, cachehints, frame_range);
    if (d.error)
      throw AvisynthError(d.error);
  }
  // We do not pass cache requests upwards, only to the next filter.
}

C_VideoFilter::~C_VideoFilter()
{
  if (d.free_filter)
    d.free_filter(&d);
}
*/

/////////////////////////////////////////////////////////////////////
//
// AVS_GenericVideoFilter
//

void AVSC_CC
_avs_gvf_destroy (AVS_Clip *p, gboolean freeself)
{
  AVS_GenericVideoFilter *gvf = (AVS_GenericVideoFilter *)p;

  gvf->parent.destroy (&gvf->parent, FALSE);

  if (freeself)
    g_free (gvf);
}

AVS_GenericVideoFilter * AVSC_CC
_avs_gvf_construct (AVS_GenericVideoFilter *p, AVS_Clip *next)
{
  AVS_GenericVideoFilter *gvf;
  AVS_Clip *clip_self;

  if (!p)
    gvf = g_new0 (AVS_GenericVideoFilter, 1);
  else
    gvf = p;

  gvf->destroy = _avs_gvf_destroy;
  gvf->child = NULL;

  clip_self = (AVS_Clip *)gvf;
  if (!clip_self->get_frame)
    clip_self->get_frame = _avs_gvf_get_frame;
  if (!clip_self->get_audio)
    clip_self->get_audio = _avs_gvf_get_audio;
  if (!clip_self->get_video_info)
    clip_self->get_video_info = _avs_gvf_get_video_info;
  if (!clip_self->get_parity)
    clip_self->get_parity = _avs_gvf_get_parity;
  if (!clip_self->set_cache_hints)
    clip_self->set_cache_hints = _avs_gvf_set_cache_hints;

  _avs_clip_construct (&gvf->parent);

  gvf->parent.child = clip_self;

  gvf->next_clip = next;
  if (next)
  {
    _avs_clip_ref (next);
    gvf->vi = next->get_video_info(next);
  }

  return gvf;
}

AVS_VideoFrame * AVSC_CC
_avs_gvf_get_frame (AVS_Clip *p, gint64 n)
{
  AVS_GenericVideoFilter *gvf = (AVS_GenericVideoFilter *)p;
  return _avs_clip_get_frame (gvf->next_clip, n);
}

gint AVSC_CC
_avs_gvf_get_audio (AVS_Clip *p, gpointer buf, gint64 start, gint64 count)
{
  AVS_GenericVideoFilter *gvf = (AVS_GenericVideoFilter *)p;
  return _avs_clip_get_audio (gvf->next_clip, buf, start, count);
}

AVS_VideoInfo AVSC_CC
_avs_gvf_get_video_info (AVS_Clip *p)
{
  return ((AVS_GenericVideoFilter *)p)->vi;
}

gboolean AVSC_CC
_avs_gvf_get_parity (AVS_Clip *p, gint64 n)
{
  return _avs_clip_get_parity (((AVS_GenericVideoFilter *)p)->next_clip, n);
}

void AVSC_CC
_avs_gvf_set_cache_hints (AVS_Clip *p, gint cachehints, gint64 frame_range)
{
  /* Not implemented */
}

/////////////////////////////////////////////////////////////////////
//
// AVS_Clip
//

void AVSC_CC
_avs_clip_release (AVS_Clip * p)
{
  /* Call destructor of the derived class. It will call
   * all parent destructors explicitly
   */
  if (g_atomic_int_dec_and_test (&p->refcount))
    p->destroy(p, TRUE);
}

void AVSC_CC
_avs_clip_ref (AVS_Clip *p)
{
  g_atomic_int_inc (&p->refcount);
}

AVS_Clip * AVSC_CC
_avs_clip_construct (AVS_Clip *p)
{
  AVS_Clip *np;

  if (!p)
    np = g_new0 (AVS_Clip, 1);
  else
    np = p;

  np->destroy = _avs_clip_destroy;
  np->child = NULL;

  if (!np->get_frame)
    np->get_frame = _avs_clip_get_frame;
  if (!np->get_audio)
    np->get_audio = _avs_clip_get_audio;
  if (!np->get_video_info)
    np->get_video_info = _avs_clip_get_video_info;
  if (!np->get_parity)
    np->get_parity = _avs_clip_get_parity;
  if (!np->set_cache_hints)
    np->set_cache_hints = _avs_clip_set_cache_hints;

  np->refcount = 1;

  return np;
}

void AVSC_CC
_avs_clip_destroy (AVS_Clip *p, gboolean freeself)
{
  if (!freeself)
  {
    AVS_Clip *child_clip = p->child;
    if (child_clip)
    {
      while (child_clip)
        child_clip = child_clip->child;
      if (!child_clip->destroy)
        g_critical ("child clip does not have a destructor");
      child_clip->destroy (child_clip, TRUE);
    }
  }

  if (freeself || p->child == NULL)
    g_free(p);
}

const gchar * AVSC_CC
_avs_clip_get_error(AVS_Clip * p)
{
  return p->error;
}

gint AVSC_CC
_avs_clip_get_version(AVS_Clip * p)
{
  return AVISYNTH_INTERFACE_VERSION;
}

AVS_VideoInfo AVSC_CC
_avs_clip_get_video_info(AVS_Clip  * p)
{
  AVS_VideoInfo v;
  g_critical ("_avs_clip_get_video_info() is an abstract method and should not be called");
  return v;
}

AVS_VideoFrame * AVSC_CC
_avs_clip_get_frame(AVS_Clip *p, gint64 n)
{
  g_critical ("_avs_clip_get_frame() is an abstract method and should not be called");
  return NULL;
}

gint AVSC_CC
_avs_clip_get_parity(AVS_Clip * p, gint64 n)
{
  g_critical ("_avs_clip_get_parity() is an abstract method and should not be called");
  return NULL;
}

gint AVSC_CC
_avs_clip_get_audio(AVS_Clip * p, void * buf, gint64 start, gint64 count)
{
  g_critical ("_avs_clip_get_audio() is an abstract method and should not be called");
  return NULL;
}

void AVSC_CC
_avs_clip_set_cache_hints(AVS_Clip * p, int cachehints, gint64 frame_range)
{
  /* Not implemented */
}

/*********************
 *
 * AVS_Value
 *
 */

AVS_Clip * AVSC_CC
_avs_val_take_clip(AVS_Value v)
{
  _avs_clip_ref (v.d.clip);
  return v.d.clip;
}

void AVSC_CC
_avs_val_set_to_clip(AVS_Value *v, AVS_Clip *c)
{
  if (v->type == 'c' && v->d.clip)
    _avs_clip_release (v->d.clip);
  _avs_clip_ref (c);
  v->d.clip = c;
  v->type = 'c';
}

void AVSC_CC
_avs_val_copy(AVS_Value * dest, AVS_Value src)
{
  if (src.type == 'c' && src.d.clip)
    _avs_clip_ref (src.d.clip);
  if (dest->type == 'c' && dest->d.clip)
    _avs_clip_release (dest->d.clip);
  memcpy (dest, &src, sizeof(AVS_Value));
}

void AVSC_CC
_avs_val_release(AVS_Value v)
{
  if (v.type == 'c' && v.d.clip)
    _avs_clip_release (v.d.clip);
  if (v.type == 'a' && v.d.array)
  {
    gint i;
    for (i = 0; i < v.array_size; i++)
      _avs_val_release(v.d.array[i]);
    g_free((gpointer) v.d.array);
  }
}


AVS_Value AVSC_CC
_avs_val_new_bool (gboolean v0)
{
  AVS_Value v;
  v.type = 'b';
  v.d.boolean = v0;
  return v;
}

AVS_Value AVSC_CC
_avs_val_new_int (gint v0)
{
  AVS_Value v;
  v.type = 'i';
  v.d.integer = v0;
  return v;
}

AVS_Value AVSC_CC
_avs_val_new_string(const gchar * v0) 
{
  AVS_Value v;
  v.type = 's';
  v.d.string = v0;
  return v;
}

AVS_Value AVSC_CC
_avs_val_new_float(gfloat v0) 
{
  AVS_Value v;
  v.type = 'f';
  v.d.floating_pt = v0;
  return v;
}

AVS_Value AVSC_CC
_avs_val_new_error(const gchar * v0) 
{
  AVS_Value v;
  v.type = 'e';
  v.d.string = v0;
  return v;
}

AVS_Value AVSC_CC
_avs_val_new_clip(AVS_Clip * v0)
{
  AVS_Value v;
  _avs_val_set_to_clip(&v, v0);
  return v;
}

AVS_Value AVSC_CC
_avs_val_new_array(gint size)
{
  gint i;
  AVS_Value v;
  v.type = 'a';
  v.array_size = size;
  v.d.array = g_new(AVS_Value, size);
  for (i = 0; i < size; i++)
  {
    v.d.array[i].type = 'v';
    v.d.array[i].array_size = 0;
  }

  return v;
}

/*
AVS_Clip * AVSC_CC
_avs_new_c_filter(AVS_ScriptEnvironment * e,
                 AVS_FilterInfo * * fi,
                 AVS_Value child, int store_child)
{
  AVS_Clip *nc = _avs_clip_construct (
  C_VideoFilter * f = new C_VideoFilter();
  AVS_Clip * ff = new AVS_Clip();
  ff->clip = f;
  ff->env  = (IScriptEnvironment *) e->env;
  f->env.env = e->env;
  f->d.env = &f->env;
  if (store_child) {
    _ASSERTE(child.type == 'c');
    f->child.clip = (IClip *)child.d.clip;
    f->child.env  = (IScriptEnvironment *) e->env;
    f->d.child = &f->child;
  }
  *fi = &f->d;
  if (child.type == 'c')
    f->d.vi = *(const AVS_VideoInfo *)(&((IClip *)child.d.clip)->GetVideoInfo());
  return ff;
}
*/

/////////////////////////////////////////////////////////////////////
//
// AVS_ScriptEnvironment::add_function
//
/*
struct C_VideoFilter_UserData {
  void * user_data;
  AVSApplyFunc func;
};

AVSValue __cdecl
create_c_video_filter(AVSValue args, void * user_data,
                      IScriptEnvironment * e0)
{
  C_VideoFilter_UserData * d = (C_VideoFilter_UserData *)user_data;
  AVS_ScriptEnvironment env;
  _avs_script_environment_init (&env, e0);
//OutputDebugString("OK");
  AVS_Value res = (d->func)(&env, *(AVS_Value *)&args, d->user_data);
  if (res.type == 'e') {
    throw AvisynthError(res.d.string);
  } else {
    AVSValue val;
    val = (*(const AVSValue *)&res);
    ((AVSValue *)&res)->~AVSValue();
    return val;
  }
}
*/



/**********
 *
 * AVS_ScriptEnvironment
 *
 */

int AVSC_CC 
_avs_se_add_function(AVS_ScriptEnvironment * p, const char * name, const char * params, 
                 const char* srccapstr, const char* sinkcapstr,
                 AVSApplyFunc applyf, void * user_data)
{
  GstAVSynthVideoFilter *parent = (GstAVSynthVideoFilter *)p->internal;
  GstAVSynthVideoFilterClass *parent_class =  (GstAVSynthVideoFilterClass *) G_OBJECT_GET_CLASS (parent);
  if (g_utf8_collate (name, parent_class->name))
    return 0;

  parent->apply_c = applyf;
  parent->user_data_c = user_data;

  return 0;
}

glong AVSC_CC
_avs_se_get_cpu_flags(AVS_ScriptEnvironment * p)
{
  p->error = NULL;
  /* FIXME: add cpuflags detection */
  return 0;
}


char * AVSC_CC
_avs_se_save_string(AVS_ScriptEnvironment * p, const char* s, gssize length)
{
  GstAVSynthVideoFilter *parent = (GstAVSynthVideoFilter *)p->internal;
  p->error = NULL;
  if (G_UNLIKELY (!s))
    return NULL;
  if (G_UNLIKELY (length > 0))
    return g_string_chunk_insert_len (parent->string_dump, s, (gssize) length);
  return g_string_chunk_insert (parent->string_dump, s);
}

gchar * AVSC_CC
_avs_se_sprintf(AVS_ScriptEnvironment * p, const char* fmt, ...)
{
  gchar* v;
  va_list vl;
  p->error = NULL;
  va_start(vl, fmt);
  v = _avs_se_vsprintf(p, fmt, vl);
  va_end(vl);
  return v;
}

gchar * AVSC_CC
_avs_se_vsprintf(AVS_ScriptEnvironment * p, const char* fmt, va_list val)
{
  char *v = NULL, *tmp = NULL;
  /* This kinda defeats the purpose of the string chunks... */
  tmp = g_strdup_vprintf (fmt, val);
  v = _avs_se_save_string (p, tmp, -1);
  g_free (tmp);
  return v;
}

int AVSC_CC
_avs_se_function_exists(AVS_ScriptEnvironment * p, const char * name)
{
  p->error = NULL;
  /* FIXME: add "avsynth" */
  if (g_type_from_name ((gchar *) name))
    return true;
  else
    return false;
}

AVS_Value AVSC_CC
_avs_se_invoke(AVS_ScriptEnvironment * p, const char * name, AVS_Value args, const char * * arg_names)
{
  AVS_Value v = {0,0};
  p->error = _avs_se_save_string(p, "_avs_invoke: Not implemented", -1);
  return v;
}

AVS_Value AVSC_CC
_avs_se_get_var(AVS_ScriptEnvironment * p, const char* name)
{
  AVS_Value v = {0,0};
  p->error = NULL;
  if (g_utf8_collate (name, "$PluginDir$") == 0)
  {
    v.type = 's';
    v.d.string = plugindir_var;
  }
  return v;
}

int AVSC_CC
_avs_se_set_var(AVS_ScriptEnvironment * p, const char* name, AVS_Value val)
{
  p->error = NULL;
  return 0;
}


int AVSC_CC
_avs_se_set_global_var(AVS_ScriptEnvironment * p, const char* name, AVS_Value val)
{
  p->error = NULL;
  return 0;
}


AVS_VideoFrame * AVSC_CC
_avs_vf_construct (AVS_VideoFrameBuffer *_vfb, gint _offset, gint _pitch, gint _row_size, gint _height, gint _offsetU, gint _offsetV, gint _pitchUV)
{
  AVS_VideoFrame *vf = g_new0 (AVS_VideoFrame, 1);
  g_atomic_int_inc (&_vfb->refcount);
  vf->vfb = _vfb;
  vf->offset = _offset;
  vf->pitch = _pitch;
  vf->row_size = _row_size;
  vf->height = _height;
  vf->offsetU = _offsetU;
  vf->offsetV = _offsetV;
  vf->pitchUV = _pitchUV;
  return vf;
}

gboolean AVSC_CC
_avs_se_planar_chroma_alignment_state (AVS_ScriptEnvironment *p, gint key)
{
  gboolean oldPlanarChromaAlignmentState = ((GstAVSynthVideoFilter *)p->internal)->PlanarChromaAlignmentState;

  switch (key)
  {
    case AVS_PLANAR_CHROMA_ALIGNMENT_OFF:
    {
      ((GstAVSynthVideoFilter *)p->internal)->PlanarChromaAlignmentState = false;
      break;
    }
    case AVS_PLANAR_CHROMA_ALIGNMENT_ON:
    {
      ((GstAVSynthVideoFilter *)p->internal)->PlanarChromaAlignmentState = true;
      break;
    }
    default:
      break;
  }
  return oldPlanarChromaAlignmentState;
}


AVS_VideoFrame * AVSC_CC
_avs_vf_new_p (AVS_ScriptEnvironment *p, gint width, gint height, gint align, gboolean U_first)
{
  int UVpitch, Uoffset, Voffset, pitch, diff;
  AVS_VideoFrameBuffer* vfb;
  int size, _align;
  gsize offset;

  diff = align - 1;

  if (align < 0) {
    // Forced alignment - pack Y as specified, pack UV half that
    align = -align;
    diff = align - 1;
    pitch = (width + diff) & ~diff; // Y plane, width = 1 byte per pixel
    UVpitch = (pitch + 1) >> 1; // UV plane, width = 1/2 byte per pixel - can't align UV planes separately.
  }
  else if (((GstAVSynthVideoFilter *)p->internal)->PlanarChromaAlignmentState) {
    // Align UV planes, Y will follow
    UVpitch = (((width + 1) >> 1) + diff) & ~diff; // UV plane, width = 1/2 byte per pixel
    pitch = UVpitch << 1; // Y plane, width = 1 byte per pixel
  }
  else {
    // Do legacy alignment
    pitch = (width + diff) & ~diff; // Y plane, width = 1 byte per pixel
    UVpitch = (pitch + 1) >> 1; // UV plane, width = 1/2 byte per pixel - can't align UV planes seperately.
  }

  size = pitch * height + UVpitch * height;
  _align = (align < AVS_FRAME_ALIGN) ? AVS_FRAME_ALIGN : align;

  vfb = _avs_vfb_new (size + (_align * 4));
  if (!vfb)
  {
    p->error = _avs_se_save_string (p, "NewPlanarVideoFrame: Returned 0 image pointer!", -1);
    return NULL;
  }
  offset = (-gsize (vfb->data)) & (AVS_FRAME_ALIGN - 1);  // align first line offset

  if (U_first) {
    Uoffset = offset + pitch * height;
    Voffset = offset + pitch * height + UVpitch * (height >> 1);
  } else {
    Voffset = offset + pitch * height;
    Uoffset = offset + pitch * height + UVpitch * (height >> 1);
  }
  return _avs_vf_construct (vfb, offset, pitch, width, height, Uoffset, Voffset, UVpitch);
}

AVS_VideoFrame *
_avs_vf_new (AVS_ScriptEnvironment *p, gint row_size, gint height, gint align)
{
  AVS_VideoFrameBuffer *vfb;
  gint pitch, size, _align;
  gsize offset;
  pitch = (row_size + align - 1) / align * align;
  size = pitch * height;
  _align = (align < AVS_FRAME_ALIGN) ? AVS_FRAME_ALIGN : align;
  p->error = NULL;
  vfb = _avs_vfb_new (size + (_align * 4));
  if (!vfb)
  {
    p->error = _avs_se_save_string (p, "NewVideoFrame: Returned 0 image pointer!", -1);
    return NULL;
  }
  offset = (-gsize (vfb->data)) & (AVS_FRAME_ALIGN - 1);  // align first line offset  (alignment is free here!)
  return _avs_vf_construct (vfb, offset, pitch, row_size, height, 0, 0, 0);
}


AVS_VideoFrame * AVSC_CC
_avs_se_vf_new_a(AVS_ScriptEnvironment *p, const AVS_VideoInfo * vi, gint align)
{
  AVS_VideoFrame *vf = NULL;
  p->error = NULL;

  switch (vi->pixel_type) {
    case AVS_CS_BGR24:
    case AVS_CS_BGR32:
    case AVS_CS_YUY2:
    case AVS_CS_YV12:
    case AVS_CS_I420:
      break;
    default:
      p->error = _avs_se_save_string (p, "Filter Error: Filter attempted to create VideoFrame with invalid pixel_type.", -1);
      return NULL;
  }
  // If align is negative, it will be forced, if not it may be made bigger
  if (AVS_IS_PLANAR(vi)) { // Planar requires different math ;)
    if (align>=0) {
      align = MAX(align, AVS_FRAME_ALIGN);
    }
    if ((vi->height & 1) || (vi->width & 1))
    {
      p->error = _avs_se_save_string (p, "Filter Error: Attempted to request an YV12 frame that wasn't mod2 in width and height!", -1);
      return NULL;
    }
    vf = _avs_vf_new_p (p, vi->width, vi->height, align, !AVS_IS_VPLANE_FIRST(vi));  // If planar, maybe swap U&V
  } else {
    if ((vi->width & 1) && (AVS_IS_YUY2(vi)))
    {
      p->error = _avs_se_save_string (p, "Filter Error: Attempted to request an YUY2 frame that wasn't mod2 in width.", -1);
      return NULL;
    }
    if (align < 0) {
      align *= -1;
    } else {
      align = MAX(align, AVS_FRAME_ALIGN);
    }
    vf = _avs_vf_new (p, AVS_ROW_SIZE(vi), vi->height, align);
  }
  if (vf)
    _avs_vf_ref (vf);
  return vf;
}

int AVSC_CC
_avs_se_make_vf_writable(AVS_ScriptEnvironment *p, AVS_VideoFrame **pvf)
{
  gint row_size, height;
  AVS_VideoFrame *dst;
  AVS_VideoFrame *vf = *pvf;

  p->error = NULL;

  // If the frame is already writable, do nothing.
  if (_avs_vf_is_writable (vf)) {
    return 0;
  }

  // Otherwise, allocate a new frame (using NewVideoFrame) and
  // copy the data into it.  Then modify the passed PVideoFrame
  // to point to the new buffer.
  row_size = _avs_vf_get_row_size (vf);
  height = _avs_vf_get_row_size (vf);

  if (_avs_vf_get_pitch_p (vf, PLANAR_U)) {  // we have no videoinfo, so we assume that it is Planar if it has a U plane.
    dst = _avs_vf_new_p (p, row_size, height, AVS_FRAME_ALIGN, FALSE);  // Always V first on internal images
  } else {
    dst = _avs_vf_new (p, row_size, height, AVS_FRAME_ALIGN);
  }
  _avs_se_bit_blt (p, _avs_vf_get_write_ptr (dst), _avs_vf_get_pitch (dst), _avs_vf_get_read_ptr (vf), _avs_vf_get_pitch (vf), row_size, height);
  // Blit More planes (pitch, rowsize and height should be 0, if none is present)
  _avs_se_bit_blt (p, _avs_vf_get_write_ptr_p (dst, PLANAR_U), _avs_vf_get_pitch_p (dst, PLANAR_U), _avs_vf_get_read_ptr_p (vf, PLANAR_U), _avs_vf_get_pitch_p (vf, PLANAR_U), _avs_vf_get_row_size_p (vf, PLANAR_U), _avs_vf_get_height_p (vf, PLANAR_U));
  _avs_se_bit_blt (p, _avs_vf_get_write_ptr_p (dst, PLANAR_V), _avs_vf_get_pitch_p (dst, PLANAR_V), _avs_vf_get_read_ptr_p (vf, PLANAR_V), _avs_vf_get_pitch_p (vf, PLANAR_V), _avs_vf_get_row_size_p (vf, PLANAR_V), _avs_vf_get_height_p (vf, PLANAR_V));

  *pvf = dst;
  return 1;
}


void AVSC_CC
_avs_se_bit_blt(AVS_ScriptEnvironment *p, BYTE * dstp, int dst_pitch, const BYTE * srcp, int src_pitch, int row_size, int height)
{
  p->error = NULL;
  if (height<0)
  {
    p->error = _avs_se_save_string (p, "Filter Error: Attempting to blit an image with negative height.", -1);
    return;
  }
  if (row_size<0)
  {
    p->error = _avs_se_save_string (p, "Filter Error: Attempting to blit an image with negative row size.", -1);
    return;
  }
  BitBlt(dstp, dst_pitch, srcp, src_pitch, row_size, height);
}

void AVSC_CC
_avs_se_at_exit(AVS_ScriptEnvironment *p, AVSShutdownFunc function,
    void * user_data)
{
  GstAVSynthVideoFilter *parent = (GstAVSynthVideoFilter *)p->internal;
  p->error = NULL;
  parent->shutdown_c = function;
  parent->shutdown_data_c = user_data;
}


int AVSC_CC
_avs_se_check_version(AVS_ScriptEnvironment * p, gint version)
{
  p->error = NULL;
  if (version > AVISYNTH_INTERFACE_VERSION)
  {
    p->error = _avs_se_sprintf(p, "Plugin was designed for a later version of Avisynth (%d)", version);
    return -1;
  }
  return 0;
}

AVS_VideoFrame * AVSC_CC
_avs_se_subframe(AVS_ScriptEnvironment * p, AVS_VideoFrame * src0, 
             int rel_offset, int new_pitch, int new_row_size, int new_height)
{
  p->error = NULL;
  return _avs_vf_construct (src0->vfb, src0->offset + rel_offset, new_pitch, new_row_size, new_height, 0, 0, 0);
}


AVS_VideoFrame * AVSC_CC
_avs_se_subframe_p(AVS_ScriptEnvironment * p, AVS_VideoFrame * src0, 
                    int rel_offset, int new_pitch, int new_row_size, int new_height,
                    int rel_offsetU, int rel_offsetV, int new_pitchUV)
{
  p->error = NULL;
  return _avs_vf_construct (src0->vfb, src0->offset + rel_offset, new_pitch, new_row_size, new_height,
             src0->offsetU + rel_offsetU, src0->offsetV + rel_offsetV, new_pitchUV);
}


int AVSC_CC
_avs_se_set_memory_max(AVS_ScriptEnvironment * p, int mem)
{
  /* FIXME: implement */
  return 0;
}


int AVSC_CC
_avs_se_set_working_dir(AVS_ScriptEnvironment * p, const char * newdir)
{
  /* FIXME: implement */
  return 0;
}


/* This is not implemented (required only for scripting, which is NOT implemented too)

AVSValue __cdecl
load_c_plugin(AVSValue args, void * user_data, 
              IScriptEnvironment * env)
{
  const char * filename = args[0].AsString();
  HMODULE plugin = LoadLibrary(filename);
  if (!plugin)
    env->ThrowError("Unable to load C Plugin: %s", filename);
  AvisynthCPluginInitFunc func = 0;
#ifndef AVSC_USE_STDCALL
  func = (AvisynthCPluginInitFunc)GetProcAddress(plugin, "avisynth_c_plugin_init");
#else // AVSC_USE_STDCALL
  func = (AvisynthCPluginInitFunc)GetProcAddress(plugin, "avisynth_c_plugin_init@4");
  if (!func)
    func = (AvisynthCPluginInitFunc)GetProcAddress(plugin, "avisynth_c_plugin_init");
#endif // AVSC_USE_STDCALL
  if (!func)
    env->ThrowError("Not An Avisynth 2 C Plugin: %s", filename);
  AVS_ScriptEnvironment e;
  _avs_script_environment (&e);
  e.env = env;
  AVS_ScriptEnvironment *pe;
  pe = &e;
  const char *s = NULL;
  int callok = 1; // (stdcall)
  __asm // Tritical - Jan 2006
  {
    push eax
    push edx

    push 0x12345678    // Stash a known value

    mov eax, pe      // Env pointer
    push eax      // Arg1
    call func      // avisynth_c_plugin_init

    lea edx, s      // return value is in eax
    mov DWORD PTR[edx], eax

    pop eax        // Get top of stack
    cmp eax, 0x12345678  // Was it our known value?
    je end        // Yes! Stack was cleaned up, was a stdcall

    lea edx, callok
    mov BYTE PTR[edx], 0 // Set callok to 0 (_cdecl)

    pop eax        // Get 2nd top of stack
    cmp eax, 0x12345678  // Was this our known value?
    je end        // Yes! Stack is now correctly cleaned up, was a _cdecl

    mov BYTE PTR[edx], 2 // Set callok to 2 (bad stack)
end:
    pop edx
    pop eax
  }
  if (callok == 2)
    env->ThrowError("Avisynth 2 C Plugin '%s' has corrupted the stack.", filename);
#ifndef AVSC_USE_STDCALL
  if (callok != 0)
    env->ThrowError("Avisynth 2 C Plugin '%s' has wrong calling convention! Must be _cdecl.", filename);
#else // AVSC_USE_STDCALL
  if (callok != 1)
    env->ThrowError("Avisynth 2 C Plugin '%s' has wrong calling convention! Must be stdcall.", filename);
#endif // AVSC_USE_STDCALL
  if (s == 0)
    env->ThrowError("Avisynth 2 C Plugin '%s' returned a NULL pointer.", filename);
  return AVSValue(s);
}
*/

