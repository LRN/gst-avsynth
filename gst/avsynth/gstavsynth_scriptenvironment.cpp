/*
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
G_BEGIN_DECLS
#if HAVE_ORC
#include <orc/orcprogram.h>
#endif
#include <liboil/liboil.h>
#include <liboil/liboilcpu.h>
#include <liboil/liboilfunction.h>
G_END_DECLS

gchar *plugindir_var = NULL;

void BitBlt(guint8* dstp, int dst_pitch, const guint8* srcp, int src_pitch, int row_size, int height) {
/* FROM_AVISYNTH_BEGIN */
  if ((!height) || (!row_size)) return;
  if (height == 1 || (dst_pitch == src_pitch && src_pitch == row_size)){
    memcpy(dstp, srcp, row_size * height);
  } else {
/* FROM_AVISYNTH_END */
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
/* FROM_AVISYNTH_BEGIN */
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
/* FROM_AVISYNTH_END */
}


int AVSC_CC
_avs_vf_get_height(const AVS_VideoFrame *p)
{
  return p->height;
}


int AVSC_CC
_avs_vf_get_height_p(const AVS_VideoFrame *p, int plane)
{
/* FROM_AVISYNTH_BEGIN */
  switch (plane)
  {
    case PLANAR_U:
    case PLANAR_V:
      if (p->pitchUV)
        return p->height >> 1;
      return 0;
  }
  return p->height;
/* FROM_AVISYNTH_END */
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

/***********
 *
 * AVS_GenericVideoFilter
 */

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

/***************
 *
 * AVS_Clip
 */

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
  OilImplFlag oilflags;
  glong avsflags = 0;
  p->error = NULL;
  /* This is the most stupid thing i ever did. Using liboil merely to get CPU
   * capabilities.
   */
  oilflags = (OilImplFlag) oil_cpu_get_flags ();

  /* FIXME: is this correct? Some CPUs may not have FPU... */
  avsflags = CPUF_FPU;

  if (oilflags & OIL_IMPL_FLAG_MMX)
  {
    GST_DEBUG ("Is MMX-capable");
    avsflags |= CPUF_MMX;
  }
  if (oilflags & OIL_IMPL_FLAG_MMXEXT)
  {
    GST_DEBUG ("Is MMXEXT-capable");
    avsflags |= CPUF_INTEGER_SSE;
  }
  if (oilflags & OIL_IMPL_FLAG_SSE)
  {
    GST_DEBUG ("Is SSE-capable");
    avsflags |= CPUF_SSE;
  }
  if (oilflags & OIL_IMPL_FLAG_SSE2)
  {
    GST_DEBUG ("Is SSE2-capable");
    avsflags |= CPUF_SSE2;
  }
  if (oilflags & OIL_IMPL_FLAG_3DNOW)
  {
    GST_DEBUG ("Is 3DNOW-capable");
    avsflags |= CPUF_3DNOW;
  }
  if (oilflags & OIL_IMPL_FLAG_3DNOWEXT)
  {
    GST_DEBUG ("Is 3DNOWEXT-capable");
    avsflags |= CPUF_3DNOW_EXT;
  }
  if (oilflags & OIL_IMPL_FLAG_SSE2 & OIL_IMPL_FLAG_3DNOW)
  {
    GST_DEBUG ("Is SSE2&3DNOW-capable");
    avsflags |= CPUF_X86_64;
  }
  if (oilflags & OIL_IMPL_FLAG_SSE3)
  {
    GST_DEBUG ("Is SSE3-capable");
    avsflags |= CPUF_SSE3;
  }

  return avsflags;
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
  gboolean ret = FALSE;
  gchar *avs_string = NULL;
  p->error = NULL;
  avs_string = g_strdup_printf ("%s_%s", "avsynth", name);
  if (g_type_from_name (avs_string))
    ret = TRUE;
  else
    ret = FALSE;
  g_free (avs_string);
  return ret;
}

AVS_Value AVSC_CC
_avs_se_invoke(AVS_ScriptEnvironment * p, const char * name, AVS_Value args, const char **arg_names)
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
/* FROM_AVISYNTH_BEGIN */
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
/* FROM_AVISYNTH_END */
}


AVS_VideoFrame * AVSC_CC
_avs_vf_new_p (AVS_ScriptEnvironment *p, gint width, gint height, gint align, gboolean U_first)
{
/* FROM_AVISYNTH_BEGIN */
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
/* FROM_AVISYNTH_END */
  return _avs_vf_construct (vfb, offset, pitch, width, height, Uoffset, Voffset, UVpitch);
}

AVS_VideoFrame *
_avs_vf_new (AVS_ScriptEnvironment *p, gint row_size, gint height, gint align)
{
  AVS_VideoFrameBuffer *vfb;
/* FROM_AVISYNTH_BEGIN */
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
/* FROM_AVISYNTH_END */
  return _avs_vf_construct (vfb, offset, pitch, row_size, height, 0, 0, 0);
}


AVS_VideoFrame * AVSC_CC
_avs_se_vf_new_a(AVS_ScriptEnvironment *p, const AVS_VideoInfo * vi, gint align)
{
  AVS_VideoFrame *vf = NULL;
  p->error = NULL;
/* FROM_AVISYNTH_BEGIN */
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
/* FROM_AVISYNTH_END */
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
/* FROM_AVISYNTH_BEGIN */
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
/* FROM_AVISYNTH_END */
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
  /* Not implemented. AviSynth automatically sets max memory limit
   * (GstAVSynth should do that too). SetMemoryMax is only used in
   * scripts, but since we have no scripting support...
   */
  return 0;
}


int AVSC_CC
_avs_se_set_working_dir(AVS_ScriptEnvironment * p, const char * newdir)
{
  /* Not implemented. Used only by scripts. */
  return 0;
}


