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
#include "gstavsynth_scriptenvironment.h"
#include "gstavsynth_scriptenvironment_cpp.h"
#include "gstavsynth_videocache_cpp.h"
#include "gstavsynth_videofilter.h"

#if HAVE_ORC
G_BEGIN_DECLS
#include <orc/orcprogram.h>
G_END_DECLS
#endif

extern gchar *plugindir_var;

/* GPL
AlignPlanar must be applied after a filter that forces an alignment (that is less than default alignment), that filter applies it by itself,
that is why AlignPlanar API is public.
However, this is kinda useless, because GstAVSynth will re-align frames by itself,
and there are no other AviSynth filters attached (since scripting is not implemented).
*/
/* FROM_AVISYNTH_BEGIN */
AlignPlanar::AlignPlanar(PClip _clip) : GenericVideoFilter(_clip) {}

PVideoFrame
AlignPlanar::GetFrame (int n, IScriptEnvironment* env) {
  int plane = (env->PlanarChromaAlignment(IScriptEnvironment::PlanarChromaAlignmentTest)) ? PLANAR_U_ALIGNED : PLANAR_Y_ALIGNED;

  PVideoFrame src = child->GetFrame(n, env);

  if (!(src->GetRowSize(plane)&(FRAME_ALIGN-1)))
    return src;

  PVideoFrame dst = env->NewVideoFrame(vi);

  if ((dst->GetRowSize(PLANAR_Y_ALIGNED)&(FRAME_ALIGN-1)))
    env->ThrowError("AlignPlanar: [internal error] Returned frame was not aligned!");

  env->BitBlt(dst->GetWritePtr(), dst->GetPitch(), src->GetReadPtr(), src->GetPitch(), src->GetRowSize(), src->GetHeight());
  env->BitBlt(dst->GetWritePtr(PLANAR_V), dst->GetPitch(PLANAR_V), src->GetReadPtr(PLANAR_V), src->GetPitch(PLANAR_V), src->GetRowSize(PLANAR_V), src->GetHeight(PLANAR_V));
  env->BitBlt(dst->GetWritePtr(PLANAR_U), dst->GetPitch(PLANAR_U), src->GetReadPtr(PLANAR_U), src->GetPitch(PLANAR_U), src->GetRowSize(PLANAR_U), src->GetHeight(PLANAR_U));

  return dst;
}

PClip
AlignPlanar::Create(PClip clip)
{
  if (!clip->GetVideoInfo().IsPlanar()) {  // If not planar, already ok.
    return clip;
  }
  else
    return new AlignPlanar(clip);
}

/* GPL
FillBorder fills right side of the picture with duplicates of the rightmost pixel.
For unaligned pictures only.
*/
FillBorder::FillBorder(PClip _clip) : GenericVideoFilter(_clip) {}

PVideoFrame
FillBorder::GetFrame (int n, IScriptEnvironment* env)
{
  PVideoFrame src = child->GetFrame(n, env);
  if (src->GetRowSize(PLANAR_Y)==src->GetRowSize(PLANAR_Y_ALIGNED)) return src;  // No need to fill extra pixels

  unsigned char* Ydata = src->GetWritePtr(PLANAR_U) - (src->GetOffset(PLANAR_U)-src->GetOffset(PLANAR_Y)); // Nasty hack, to avoid "MakeWritable" - never, EVER do this at home!
  unsigned char* Udata = src->GetWritePtr(PLANAR_U);
  unsigned char* Vdata = src->GetWritePtr(PLANAR_V);

  int fillp=src->GetRowSize(PLANAR_Y_ALIGNED) - src->GetRowSize(PLANAR_Y);
  int h=src->GetHeight(PLANAR_Y);

  Ydata = &Ydata[src->GetRowSize(PLANAR_Y)-1];
  for (int y=0;y<h;y++){
    for (int x=1;x<=fillp;x++) {
      Ydata[x]=Ydata[0];
    }
    Ydata+=src->GetPitch(PLANAR_Y);
  }

  fillp=src->GetRowSize(PLANAR_U_ALIGNED) - src->GetRowSize(PLANAR_U);
  Udata = &Udata[src->GetRowSize(PLANAR_U)-1];
  Vdata = &Vdata[src->GetRowSize(PLANAR_V)-1];
  h=src->GetHeight(PLANAR_U);

  for (int y=0;y<h;y++){
    for (int x=1;x<=fillp;x++) {
      Udata[x]=Udata[0];
      Vdata[x]=Vdata[0];
    }
    Udata+=src->GetPitch(PLANAR_U);
    Vdata+=src->GetPitch(PLANAR_V);
  }
  return src;
}

PClip FillBorder::Create(PClip clip)
{
  if (!clip->GetVideoInfo().IsPlanar()) {  // If not planar, already ok.
    return clip;
  }
  else
    return new FillBorder(clip);
}
/* FROM_AVISYNTH_END */

ScriptEnvironment::ScriptEnvironment (gpointer parent)
{
  parent_object = (gpointer) g_object_ref (G_OBJECT ((GstAVSynthVideoFilter *)parent));
  parent_class = (gpointer) G_OBJECT_GET_CLASS (parent_object);
  env_c = _avs_se_new ((GstAVSynthVideoFilter *)parent_object);
}

ScriptEnvironment::~ScriptEnvironment ()
{
  _avs_se_free (env_c);
  g_object_unref (G_OBJECT ((GstAVSynthVideoFilter *)parent_object));
}

void
ScriptEnvironment::CheckVersion (int version)
{
  if (version > AVISYNTH_INTERFACE_VERSION)
    ThrowError("Plugin was designed for a later version of Avisynth (%d)", version);
}

long
ScriptEnvironment::GetCPUFlags ()
{
  return _avs_se_get_cpu_flags (env_c);
}

char*
ScriptEnvironment::SaveString (const char* s, int length)
{
  return _avs_se_save_string (env_c, s, length);
}

char*
ScriptEnvironment::Sprintf (const char* fmt, ...)
{
  char* result;
  va_list val;
  va_start(val, fmt);
  result = ScriptEnvironment::VSprintf (fmt, val);
  va_end(val);
  return result;
}

char*
ScriptEnvironment::VSprintf (const char* fmt, va_list val)
{
  return _avs_se_vsprintf (env_c, fmt, val);
}

void
ScriptEnvironment::ThrowError (const char* fmt, ...)
{
  gchar *buf = NULL;
  va_list val;

  va_start(val, fmt);
  buf = g_strdup_vprintf (fmt, val);
  va_end(val);

  /*
   * TODO : This should raise a GST_ERROR on the pipeline bus ! 
   * Having filters raise errors and segfault the whole application might be
   * considered acceptable behaviour on win32/avisynth BUT NOT IN GSTREAMER !
   *
   * Now it does. Still, it is not a perfect solution: filters will expect
   * AviSynthError to be caught and handled or to kill AviSynth.
   */
  GST_ERROR_OBJECT (this->parent_object, "AvisynthError: %s", buf);
  //throw AvisynthError(ScriptEnvironment::SaveString (buf));
}

void
ScriptEnvironment::AddFunction(const char* name, const char* paramstr, const char* srccapstr, const char* sinkcapstr, ApplyFunc apply_func, void* user_data)
{
  /* One plugin may add more than one function, but this
   * class wraps only one of them.
   */
  if (g_utf8_collate (name, ((GstAVSynthVideoFilterClass *)parent_class)->name))
    return;

  ((GstAVSynthVideoFilter *)parent_object)->apply_cpp = apply_func;
  ((GstAVSynthVideoFilter *)parent_object)->user_data_cpp = user_data;
}

bool
ScriptEnvironment::FunctionExists(const char* name)
{
  return _avs_se_function_exists (env_c, name);
}

/* Both NewVideoFrame methods are GPL */
PVideoFrame __stdcall
ScriptEnvironment::NewVideoFrame(const VideoInfo& vi, int align) {
/* FROM_AVISYNTH_BEGIN */
  // Check requested pixel_type:
  switch (vi.pixel_type) {
    case VideoInfo::CS_BGR24:
    case VideoInfo::CS_BGR32:
    case VideoInfo::CS_YUY2:
    case VideoInfo::CS_YV12:
    case VideoInfo::CS_I420:
      break;
    default:
      ThrowError("Filter Error: Filter attempted to create VideoFrame with invalid pixel_type.");
  }
  // If align is negative, it will be forced, if not it may be made bigger
  if (vi.IsPlanar()) { // Planar requires different math ;)
    if (align>=0) {
      align = MAX(align,FRAME_ALIGN);
    }
    if ((vi.height&1)||(vi.width&1))
      ThrowError("Filter Error: Attempted to request an YV12 frame that wasn't mod2 in width and height!");
    return ScriptEnvironment::NewPlanarVideoFrame(vi.width, vi.height, align, !vi.IsVPlaneFirst());  // If planar, maybe swap U&V
  } else {
    if ((vi.width&1)&&(vi.IsYUY2()))
      ThrowError("Filter Error: Attempted to request an YUY2 frame that wasn't mod2 in width.");
    if (align<0) {
      align *= -1;
    } else {
      align = MAX(align,FRAME_ALIGN);
    }
    return NewVideoFrame(vi.RowSize(), vi.height, align);
  }
/* FROM_AVISYNTH_END */
}

PVideoFrame
ScriptEnvironment::NewVideoFrame (int row_size, int height, int align)
{
  VideoFrameBuffer* vfb;
/* FROM_AVISYNTH_BEGIN */
  const int pitch = (row_size+align-1) / align * align;
  const int size = pitch * height;
  const int _align = (align < FRAME_ALIGN) ? FRAME_ALIGN : align;
  vfb = GetFrameBuffer(size+(_align*4));
  if (!vfb)
    ThrowError("NewVideoFrame: Returned 0 image pointer!");
  const gsize offset = (-gsize(vfb->GetWritePtr())) & (FRAME_ALIGN-1);  // align first line offset  (alignment is free here!)
  return new ImplVideoFrame(vfb, offset, pitch, row_size, height);
/* FROM_AVISYNTH_END */
}

/* NewPlanarVideoFrame is GPL */
PVideoFrame
ScriptEnvironment::NewPlanarVideoFrame(int width, int height, int align, bool U_first)
{
  int UVpitch, Uoffset, Voffset, pitch, diff;
  VideoFrameBuffer* vfb;

  diff = align - 1;

  if (align < 0) {
    // Forced alignment - pack Y as specified, pack UV half that
    align = -align;
    diff = align - 1;
    pitch = (width + diff) & ~diff; // Y plane, width = 1 byte per pixel
    UVpitch = (pitch + 1) >> 1; // UV plane, width = 1/2 byte per pixel - can't align UV planes separately.
  }
  else if (((GstAVSynthVideoFilter *)parent_object)->PlanarChromaAlignmentState) {
    // Align UV planes, Y will follow
	  UVpitch = (((width + 1) >> 1) + diff) & ~diff; // UV plane, width = 1/2 byte per pixel
	  pitch = UVpitch << 1; // Y plane, width = 1 byte per pixel
  }
  else {
    // Do legacy alignment
	  pitch = (width + diff) & ~diff; // Y plane, width = 1 byte per pixel
    UVpitch = (pitch + 1) >> 1; // UV plane, width = 1/2 byte per pixel - can't align UV planes seperately.
  }

  const int size = pitch * height + UVpitch * height;
  const int _align = (align < FRAME_ALIGN) ? FRAME_ALIGN : align;
  vfb = GetFrameBuffer(size+(_align*4));
  if (!vfb)
    ThrowError("NewPlanarVideoFrame: Returned 0 image pointer!");
#ifdef _DEBUG
  {
    static const BYTE filler[] = { 0x0A, 0x11, 0x0C, 0xA7, 0xED };
    BYTE* p = vfb->GetWritePtr();
    BYTE* q = p + vfb->GetDataSize()/5*5;
    for (; p<q; p+=5) {
      p[0]=filler[0]; p[1]=filler[1]; p[2]=filler[2]; p[3]=filler[3]; p[4]=filler[4];
    }
  }
#endif
  const gsize offset = (-gsize(vfb->GetWritePtr())) & (FRAME_ALIGN-1);  // align first line offset

  if (U_first) {
    Uoffset = offset + pitch * height;
    Voffset = offset + pitch * height + UVpitch * (height>>1);
  } else {
    Voffset = offset + pitch * height;
    Uoffset = offset + pitch * height + UVpitch * (height>>1);
  }
  return new ImplVideoFrame(vfb, offset, pitch, width, height, Uoffset, Voffset, UVpitch);
}

bool
ScriptEnvironment::MakeWritable(PVideoFrame* pvf) {
  const PVideoFrame& vf = *pvf;
  // If the frame is already writable, do nothing.
  if (vf->IsWritable()) {
    return false;
  }

  // Otherwise, allocate a new frame (using NewVideoFrame) and
  // copy the data into it.  Then modify the passed PVideoFrame
  // to point to the new buffer.
  const int row_size = vf->GetRowSize();
  const int height = vf->GetHeight();
  PVideoFrame dst;

  if (vf->GetPitch(PLANAR_U)) {  // we have no videoinfo, so we assume that it is Planar if it has a U plane.
    dst = NewPlanarVideoFrame(row_size, height, FRAME_ALIGN,false);  // Always V first on internal images
  } else {
    dst = NewVideoFrame(row_size, height, FRAME_ALIGN);
  }
  BitBlt(dst->GetWritePtr(), dst->GetPitch(), vf->GetReadPtr(), vf->GetPitch(), row_size, height);
  // Blit More planes (pitch, rowsize and height should be 0, if none is present)
  BitBlt(dst->GetWritePtr(PLANAR_V), dst->GetPitch(PLANAR_V), vf->GetReadPtr(PLANAR_V), vf->GetPitch(PLANAR_V), vf->GetRowSize(PLANAR_V), vf->GetHeight(PLANAR_V));
  BitBlt(dst->GetWritePtr(PLANAR_U), dst->GetPitch(PLANAR_U), vf->GetReadPtr(PLANAR_U), vf->GetPitch(PLANAR_U), vf->GetRowSize(PLANAR_U), vf->GetHeight(PLANAR_U));

  *pvf = dst;
  return true;
}

void
ScriptEnvironment::BitBlt(BYTE* dstp, int dst_pitch, const BYTE* srcp, int src_pitch, int row_size, int height)
{
  if (height<0)
    ThrowError("Filter Error: Attempting to blit an image with negative height.");
  if (row_size<0)
    ThrowError("Filter Error: Attempting to blit an image with negative row size.");
  ::BitBlt(dstp, dst_pitch, srcp, src_pitch, row_size, height);
}

/* This function adds a handler for AtExit event, which fires when
 * ScriptEnvironment cleans up. ShutdownFunc cleans up after a filter.
 */
void
ScriptEnvironment::AtExit(IScriptEnvironment::ShutdownFunc function, void* user_data)
{
  ((GstAVSynthVideoFilter *)parent_object)->shutdown_cpp = function;
  ((GstAVSynthVideoFilter *)parent_object)->shutdown_data_cpp = user_data;
}

PVideoFrame
ScriptEnvironment::Subframe(PVideoFrame src, int rel_offset, int new_pitch, int new_row_size, int new_height)
{
  return src->Subframe(rel_offset, new_pitch, new_row_size, new_height);
}

PVideoFrame
ScriptEnvironment::SubframePlanar(PVideoFrame src, int rel_offset, int new_pitch, int new_row_size, int new_height, int rel_offsetU, int rel_offsetV, int new_pitchUV)
{
  return src->Subframe(rel_offset, new_pitch, new_row_size, new_height, rel_offsetU, rel_offsetV, new_pitchUV);
}

int
ScriptEnvironment::SetMemoryMax(int mem)
{
  /* TODO: implement */
  return G_MAXINT;
}

int
ScriptEnvironment::SetWorkingDir(const char * newdir)
{
  /* TODO: implement */
  return 0;
}

void*
ScriptEnvironment::ManageCache(int key, void* data)
{
  /* Our cache implementation is completely different at the moment */
  return 0;
}

bool
ScriptEnvironment::PlanarChromaAlignment(IScriptEnvironment::PlanarChromaAlignmentMode key)
{
  bool oldPlanarChromaAlignmentState = ((GstAVSynthVideoFilter *)parent_object)->PlanarChromaAlignmentState;

  switch (key)
  {
    case IScriptEnvironment::PlanarChromaAlignmentOff:
    {
      ((GstAVSynthVideoFilter *)parent_object)->PlanarChromaAlignmentState = false;
      break;
    }
    case IScriptEnvironment::PlanarChromaAlignmentOn:
    {
      ((GstAVSynthVideoFilter *)parent_object)->PlanarChromaAlignmentState = true;
      break;
    }
    default:
      break;
  }
  return oldPlanarChromaAlignmentState;
}

VideoFrameBuffer* ScriptEnvironment::GetFrameBuffer(gint size) {
  ImplVideoFrameBuffer* result = (ImplVideoFrameBuffer *) new ImplVideoFrameBuffer(size);

  if (!result || !result->data) {
    // Damn! we got a NULL from malloc
    ThrowError("GetFrameBuffer: Returned a VFB with a 0 data pointer!\n"
               "I think we have run out of memory folks!");
  }

  return result;
}

AVSValue
ScriptEnvironment::Invoke(const char* name, const AVSValue args, const char** arg_names)
{
  /* Not implemented, used only by scripts */
  return NULL;
}

AVSValue
ScriptEnvironment::GetVar(const char* name)
{
  if (g_utf8_collate (name, "$PluginDir$") == 0)
  {
    return AVSValue (plugindir_var);
  }
  return AVSValue ();
}

bool
ScriptEnvironment::SetVar(const char* name, const AVSValue& val)
{
  /* Rarely used by plugins. Will be implemented when a need arises. */
  return false;
}

bool
ScriptEnvironment::SetGlobalVar(const char* name, const AVSValue& val)
{
  /* Rarely used by plugins. Will be implemented when a need arises. */
  return false;
}

void
ScriptEnvironment::PushContext(int level)
{
  /* Not implemented, used only by scripts */
}

void
ScriptEnvironment::PopContext()
{
  /* Not implemented, used only by scripts */
}

void
ScriptEnvironment::PopContextGlobal()
{
  /* Not implemented, used only by scripts */
  return;
}
