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
#include "gstavsynth_loader.h"
#include "gstavsynth_scriptenvironment_c.h"
#if HAVE_ORC
G_BEGIN_DECLS
#include <orc/orcprogram.h>
G_END_DECLS
#endif

extern gchar *plugindir_var;

void _avs_script_environment_init (AVS_ScriptEnvironment *e, IScriptEnvironment *e0 = NULL);

struct AVS_Clip 
{
	PClip clip;
	IScriptEnvironment * env;
	const char * error;
	AVS_Clip() : env(0), error(0) {}
};

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


/////////////////////////////////////////////////////////////////////
//
// AVS_VideoFrame
//
/*
extern "C"
int AVSC_CC
_avs_get_pitch(const AVS_VideoFrame * p)
{
  return ((VideoFrame *)p->frame)->GetPitch();
}

extern "C"
int AVSC_CC
_avs_get_pitch_p(const AVS_VideoFrame * p, int plane)
{ 
  return ((VideoFrame *)p->frame)->GetPitch(plane);
}

extern "C"
int AVSC_CC
_avs_get_row_size(const AVS_VideoFrame * p)
{
  return ((VideoFrame *)p->frame)->GetRowSize();
}

extern "C"
int AVSC_CC
_avs_get_row_size_p(const AVS_VideoFrame * p, int plane)
{ 
  return ((VideoFrame *)p->frame)->GetRowSize(plane);
}

extern "C"
int AVSC_CC
_avs_get_height(const AVS_VideoFrame * p)
{
  return ((VideoFrame *)p->frame)->GetHeight();
}

extern "C"
int AVSC_CC
_avs_get_height_p(const AVS_VideoFrame * p, int plane)
{
  return ((VideoFrame *)p->frame)->GetHeight(plane);
}

extern "C"
const BYTE* AVSC_CC
_avs_get_read_ptr(const AVS_VideoFrame * p)
{
  return ((VideoFrame *)p->frame)->GetReadPtr();
}

extern "C"
const BYTE* AVSC_CC
_avs_get_read_ptr_p(const AVS_VideoFrame * p, int plane) 
{
  return ((VideoFrame *)p->frame)->GetReadPtr(plane);
}

extern "C"
int AVSC_CC
_avs_is_writable(const AVS_VideoFrame * p)
{
  return ((VideoFrame *)p->frame)->IsWritable();
}

extern "C"
BYTE* AVSC_CC
_avs_get_write_ptr(const AVS_VideoFrame * p)
{
  return ((VideoFrame *)p->frame)->GetWritePtr();
}

extern "C"
BYTE* AVSC_CC
_avs_get_write_ptr_p(const AVS_VideoFrame * p, int plane) 
{
  return ((VideoFrame *)p->frame)->GetWritePtr(plane);
}

extern "C"
void AVSC_CC
_avs_release_frame(AVS_VideoFrame * f)
  {f->avs_release_video_frame(f);}
extern "C"
AVS_VideoFrame * AVSC_CC
_avs_copy_frame(AVS_VideoFrame * f)
  {return f->avs_copy_video_frame(f);}

extern "C"
int AVSC_CC
_avs_get_frame_parity (AVS_VideoFrame *f)
{
  return ((VideoFrame *)f->frame)->GetParity();
}

extern "C"
void AVSC_CC
_avs_set_frame_parity (AVS_VideoFrame *f, int newparity)
{
  ((VideoFrame *)f->frame)->SetParity(newparity);
}

extern "C"
guint64 AVSC_CC
_avs_get_timestamp (AVS_VideoFrame *f)
{
  return ((VideoFrame *)f->frame)->GetTimestamp();
}

extern "C"
void AVSC_CC
_avs_set_timestamp (AVS_VideoFrame *f, guint64 newtimestamp)
{
  ((VideoFrame *)f->frame)->SetTimestamp(newtimestamp);
}

*/

extern "C" 
void AVSC_CC
_avs_release_video_frame(AVS_VideoFrame * f)
{
  ((PVideoFrame *)&f)->~PVideoFrame();
}

extern "C"
AVS_VideoFrame *AVSC_CC
_avs_copy_video_frame(AVS_VideoFrame * f)
{
  AVS_VideoFrame * fnew;
  memcpy (&fnew, new PVideoFrame(*(PVideoFrame *)&f), sizeof (fnew));
  //new ((PVideoFrame *)&fnew) PVideoFrame(*(PVideoFrame *)&f);
  return fnew;
}

/////////////////////////////////////////////////////////////////////
//
// C_VideoFilter
//

PVideoFrame
C_VideoFilter::GetFrame(int n, IScriptEnvironment* env) 
{
  if (d.get_frame) {
    d.error = 0;
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
    d.error = 0;
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
    d.error = 0;
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
    d.error = 0;
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


/////////////////////////////////////////////////////////////////////
//
// AVS_Clip
//

extern "C"
void AVSC_CC
_avs_release_clip(AVS_Clip * p)
{
  delete p;
}

AVS_Clip * AVSC_CC
_avs_copy_clip(AVS_Clip * p)
{
  return new AVS_Clip(*p);
}

extern "C"
const char * AVSC_CC
_avs_clip_get_error(AVS_Clip * p) // return 0 if no error
{
  return p->error;
}

extern "C"
int AVSC_CC
_avs_get_version(AVS_Clip * p)
{
  return p->clip->GetVersion();
}

extern "C"
const AVS_VideoInfo * AVSC_CC
_avs_get_video_info(AVS_Clip  * p)
{
  return  (const AVS_VideoInfo  *)&p->clip->GetVideoInfo();
}


extern "C"
AVS_VideoFrame * AVSC_CC
_avs_get_frame(AVS_Clip * p, int n)
{
  p->error = 0;
  try {
    PVideoFrame f0 = p->clip->GetFrame(n,p->env);
    AVS_VideoFrame * f;
    memcpy (&f, new PVideoFrame(f0), sizeof(f));
    return f;
  } catch (AvisynthError err) {
    p->error = err.msg;
    return 0;
  } 
}

extern "C"
int AVSC_CC
_avs_get_parity(AVS_Clip * p, int n) // return field parity if field_based, else parity of first field in frame
{
  try {
    p->error = 0;
    return p->clip->GetParity(n);
  } catch (AvisynthError err) {
    p->error = err.msg;
    return -1;
  } 
}

extern "C"
int AVSC_CC
_avs_get_audio(AVS_Clip * p, void * buf, INT64 start, INT64 count) // start and count are in samples
{
  try {
    p->error = 0;
    p->clip->GetAudio(buf, start, count, (IScriptEnvironment *)p->env);
    return 0;
  } catch (AvisynthError err) {
    p->error = err.msg;
    return -1;
  } 
}

extern "C"
int AVSC_CC
_avs_set_cache_hints(AVS_Clip * p, int cachehints, int frame_range)  // We do not pass cache requests upwards, only to the next filter.
{
  try {
    p->error = 0;
    p->clip->SetCacheHints(cachehints, frame_range);
    return 0;
  } catch (AvisynthError err) {
    p->error = err.msg;
    return -1;
  }
}

//////////////////////////////////////////////////////////////////
//
//
//
extern "C"
AVS_Clip * AVSC_CC
_avs_take_clip(AVS_Value v, AVS_ScriptEnvironment * env)
{
  AVS_Clip * c = new AVS_Clip;
  c->env  = (IScriptEnvironment *) env->env;
  c->clip = (IClip *)v.d.clip;
  return c;
}

extern "C"
void AVSC_CC
_avs_set_to_clip(AVS_Value * v, AVS_Clip * c)
{
  memcpy (v, new AVSValue(c->clip), sizeof (AVS_Value));
}

extern "C"
void AVSC_CC
_avs_copy_value(AVS_Value * dest, AVS_Value src)
{
  memcpy (dest, new AVSValue(*(const AVSValue *)&src), sizeof(AVS_Value));
}

extern "C"
void AVSC_CC
_avs_release_value(AVS_Value v)
{
  ((AVSValue *)&v)->~AVSValue();
}

//////////////////////////////////////////////////////////////////
//
//
//

extern "C"
AVS_Clip * AVSC_CC
_avs_new_c_filter(AVS_ScriptEnvironment * e,
                 AVS_FilterInfo * * fi,
                 AVS_Value child, int store_child)
{
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

/////////////////////////////////////////////////////////////////////
//
// AVS_ScriptEnvironment::add_function
//

struct C_VideoFilter_UserData {
  void * user_data;
  AVS_ApplyFunc func;
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

extern "C"
int AVSC_CC 
_avs_add_function(AVS_ScriptEnvironment * p, const char * name, const char * params, 
                 const char* srccapstr, const char* sinkcapstr,
                 AVS_ApplyFunc applyf, void * user_data)
{
  C_VideoFilter_UserData *dd, *d = new C_VideoFilter_UserData;
  p->error = 0;
  d->func = applyf;
  d->user_data = user_data;
  dd = (C_VideoFilter_UserData *)((IScriptEnvironment *)p->env)->SaveString((const char *)d, sizeof(C_VideoFilter_UserData));
  delete d;
  try {
    ((IScriptEnvironment *)p->env)->AddFunction(name, params, srccapstr, sinkcapstr, create_c_video_filter, dd);
  } catch (AvisynthError & err) {
    p->error = err.msg;
    return -1;
  } 
  return 0;
}

/////////////////////////////////////////////////////////////////////
//
// AVS_ScriptEnvironment
//

extern "C"
long AVSC_CC
_avs_get_cpu_flags(AVS_ScriptEnvironment * p)
{
  p->error = 0;
  return ((IScriptEnvironment *)p->env)->GetCPUFlags();
}

extern "C"
char * AVSC_CC
_avs_save_string(AVS_ScriptEnvironment * p, const char* s, int length)
{
  p->error = 0;
  return ((IScriptEnvironment *)p->env)->SaveString(s, length);
}

extern "C"
char * AVSC_CC
_avs_sprintf(AVS_ScriptEnvironment * p, const char* fmt, ...)
{
  p->error = 0;
  va_list vl;
  va_start(vl, fmt);
  char * v = ((IScriptEnvironment *)p->env)->VSprintf(fmt, vl);
  va_end(vl);
  return v;
}

 // note: val is really a va_list; I hope everyone typedefs va_list to a pointer
 /* NO, NOT EVERYONE TYPEDEF val_list TO A (void *) */
extern "C"
char * AVSC_CC
_avs_vsprintf(AVS_ScriptEnvironment * p, const char* fmt, va_list val)
{
  p->error = 0;
  return ((IScriptEnvironment *)p->env)->VSprintf(fmt, val);
}

extern "C"
int AVSC_CC
_avs_function_exists(AVS_ScriptEnvironment * p, const char * name)
{
  p->error = 0;
  return ((IScriptEnvironment *)p->env)->FunctionExists(name);
}

extern "C"
AVS_Value AVSC_CC
_avs_invoke(AVS_ScriptEnvironment * p, const char * name, AVS_Value args, const char * * arg_names)
{
  AVS_Value v = {0,0};
  p->error = 0;
  try {
    AVSValue v0 = ((IScriptEnvironment *)p->env)->Invoke(name, *(AVSValue *)&args, arg_names);
    memcpy (&v, new AVSValue(v0), sizeof (AVS_Value));
  } catch (IScriptEnvironment::NotFound) {
    p->error = "Function Not Found";
  } catch (AvisynthError err) {
    p->error = err.msg;
  }
  if (p->error)
    v = avs_new_value_error(p->error);
  return v;
}

extern "C"
AVS_Value AVSC_CC
_avs_get_var(AVS_ScriptEnvironment * p, const char* name)
{
  AVS_Value v = {0,0};
  p->error = 0;
  try {
    AVSValue v0 = ((IScriptEnvironment *)p->env)->GetVar(name);
    memcpy (&v, new AVSValue(v0), sizeof (AVS_Value));
  } catch (IScriptEnvironment::NotFound) {}
  return v;
}

extern "C"
int AVSC_CC
_avs_set_var(AVS_ScriptEnvironment * p, const char* name, AVS_Value val)
{
  p->error = 0;
  try {
    return ((IScriptEnvironment *)p->env)->SetVar(((IScriptEnvironment *)p->env)->SaveString(name), *(const AVSValue *)(&val));
  } catch (AvisynthError err) {
    p->error = err.msg;
    return -1;
  }
}

extern "C"
int AVSC_CC
_avs_set_global_var(AVS_ScriptEnvironment * p, const char* name, AVS_Value val)
{
  p->error = 0;
  try {
    return ((IScriptEnvironment *)p->env)->SetGlobalVar(((IScriptEnvironment *)p->env)->SaveString(name), *(const AVSValue *)(&val));
  } catch (AvisynthError err) {
    p->error = err.msg;
    return -1;
  }
}

extern "C"
AVS_VideoFrame * AVSC_CC
_avs_new_video_frame_a(AVS_ScriptEnvironment * p, const AVS_VideoInfo *  vi, int align)
{
  p->error = 0;
  PVideoFrame f0 = ((IScriptEnvironment *) p->env)->NewVideoFrame(*(const VideoInfo *)vi, align);
  AVS_VideoFrame * f;
  //This is to prevent "PVideoFrame::operator =" from doing anything stupid
  memcpy (&f, new PVideoFrame(f0), sizeof (f));
  //new((PVideoFrame *)&f) PVideoFrame(f0);
  return f;
}

extern "C"
AVS_VideoFrame * AVSC_CC
_avs_new_video_frame(AVS_ScriptEnvironment * env, 
                                     const AVS_VideoInfo * vi)
  {return env->avs_new_video_frame_a(env,vi,AVS_FRAME_ALIGN);}

extern "C"
AVS_VideoFrame * AVSC_CC
_avs_new_frame(AVS_ScriptEnvironment * env, 
                               const AVS_VideoInfo * vi)
  {return env->avs_new_video_frame_a(env,vi,AVS_FRAME_ALIGN);}

extern "C"
int AVSC_CC
_avs_make_writable(AVS_ScriptEnvironment * p, AVS_VideoFrame * * pvf)
{
  p->error = 0;
  int res = ((IScriptEnvironment *)p->env)->MakeWritable((PVideoFrame *)(pvf));
  return res;
}

extern "C"
void AVSC_CC
_avs_bit_blt(AVS_ScriptEnvironment * p, BYTE * dstp, int dst_pitch, const BYTE * srcp, int src_pitch, int row_size, int height)
{
  p->error = 0;
  ((IScriptEnvironment *)p->env)->BitBlt(dstp, dst_pitch, srcp, src_pitch, row_size, height);
}

struct ShutdownFuncData
{
  AVS_ShutdownFunc func;
  void * user_data;
};

void __cdecl
shutdown_func_bridge(void* user_data, IScriptEnvironment* env)
{
  ShutdownFuncData * d = (ShutdownFuncData *)user_data;
  AVS_ScriptEnvironment e;
  _avs_script_environment_init (&e);
  e.env = env;
  d->func(d->user_data, &e);
}

extern "C"
void AVSC_CC
_avs_at_exit(AVS_ScriptEnvironment * p, 
                           AVS_ShutdownFunc function, void * user_data)
{
  p->error = 0;
  ShutdownFuncData *dd, *d = new ShutdownFuncData;
  d->func = function;
  d->user_data = user_data;
  dd = (ShutdownFuncData *)((IScriptEnvironment *)p->env)->SaveString((const char *)d, sizeof(ShutdownFuncData));
  delete d;
  ((IScriptEnvironment *)p->env)->AtExit(shutdown_func_bridge, dd);
}

extern "C"
int AVSC_CC
_avs_check_version(AVS_ScriptEnvironment * p, int version)
{
  p->error = 0;
  try {
    ((IScriptEnvironment *)p->env)->CheckVersion(version);
    return 0;
  } catch (AvisynthError err) {
    p->error = err.msg;
    return -1;
  }
}

extern "C"
AVS_VideoFrame * AVSC_CC
_avs_subframe(AVS_ScriptEnvironment * p, AVS_VideoFrame * src0, 
             int rel_offset, int new_pitch, int new_row_size, int new_height)
{
  p->error = 0;
  try {
    PVideoFrame f0 = ((IScriptEnvironment *) p->env)->Subframe((VideoFrame *)src0, rel_offset, new_pitch, new_row_size, new_height);
    AVS_VideoFrame * f;
    memcpy (&f, new PVideoFrame(f0), sizeof(f));
    //new((PVideoFrame *)&f) PVideoFrame(f0);
    return f;
  } catch (AvisynthError err) {
    p->error = err.msg;
    return 0;
  }
}

extern "C"
AVS_VideoFrame * AVSC_CC
_avs_subframe_planar(AVS_ScriptEnvironment * p, AVS_VideoFrame * src0, 
                    int rel_offset, int new_pitch, int new_row_size, int new_height,
                    int rel_offsetU, int rel_offsetV, int new_pitchUV)
{
  p->error = 0;
  try {
    PVideoFrame f0 = ((IScriptEnvironment *) p->env)->SubframePlanar((VideoFrame *)src0, rel_offset, new_pitch, new_row_size,
                        new_height, rel_offsetU, rel_offsetV, new_pitchUV);
    AVS_VideoFrame * f;
    memcpy (&f, new PVideoFrame(f0), sizeof(f));
    //new((PVideoFrame *)&f) PVideoFrame(f0);
    return f;
  } catch (AvisynthError err) {
    p->error = err.msg;
    return 0;
  }
}

extern "C"
int AVSC_CC
_avs_set_memory_max(AVS_ScriptEnvironment * p, int mem)
{
  p->error = 0;
  try {
    return ((IScriptEnvironment *)p->env)->SetMemoryMax(mem);
  } catch (AvisynthError err) {
    p->error = err.msg;
    return -1;
  }
}

extern "C"
int AVSC_CC
_avs_set_working_dir(AVS_ScriptEnvironment * p, const char * newdir)
{
  p->error = 0;
  try {
    return ((IScriptEnvironment *)p->env)->SetWorkingDir(newdir);
  } catch (AvisynthError err) {
    p->error = err.msg;
    return -1;
  }
}

/////////////////////////////////////////////////////////////////////
//
// 
//
/*
extern "C" 
AVS_ScriptEnvironment * AVSC_CC
_avs_create_script_environment(int version)
{
  AVS_ScriptEnvironment * e = new AVS_ScriptEnvironment;
  _avs_script_environment_init (e);
  e->env = CreateScriptEnvironment(version);
  return e;
}
*/
/////////////////////////////////////////////////////////////////////
//
// 
//

extern "C" 
void AVSC_CC
_avs_delete_script_environment(AVS_ScriptEnvironment * e)
{
  if (e) {
    if (e->env) delete ((IScriptEnvironment *)e->env);
      delete e;
  }
}

/////////////////////////////////////////////////////////////////////
//
// 
//

typedef const char * (AVSC_CC *AvisynthCPluginInitFunc)(AVS_ScriptEnvironment* env);

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


AVSFunction CPlugin_filters[] = {
 {"LoadCPlugin", "s", load_c_plugin },
 {"Load_Stdcall_Plugin", "s", load_c_plugin },
 { 0 }
};
*/

/* TODO: move everything to AVS_ScriptEnvironment? It's a bit bulky, but
 * AVS_ScriptEnvironment is created *once*, while other structures may be
 * created multiple times
 *//*
void
_avs_clip_init (AVS_Clip *c)
{
  c->env = NULL;
  c->clip = NULL;
  c->error = NULL;
}
*/
void
_avs_script_environment_init (AVS_ScriptEnvironment *e, IScriptEnvironment *e0)
{
  e->env = NULL;
  e->error = NULL;
  e->avs_new_c_filter = _avs_new_c_filter;
  e->avs_get_cpu_flags = _avs_get_cpu_flags;
  e->avs_check_version = _avs_check_version;
  e->avs_save_string = _avs_save_string;
  e->avs_sprintf = _avs_sprintf;
  e->avs_vsprintf = _avs_vsprintf;
  e->avs_add_function = _avs_add_function;
  e->avs_function_exists = _avs_function_exists;
  e->avs_invoke = _avs_invoke;
  e->avs_get_var = _avs_get_var;
  e->avs_set_var = _avs_set_var;
  e->avs_set_global_var = _avs_set_global_var;
  e->avs_new_video_frame_a = _avs_new_video_frame_a;
  e->avs_new_video_frame = _avs_new_video_frame;
  e->avs_new_frame = _avs_new_frame;
  e->avs_make_writable = _avs_make_writable;
  e->avs_bit_blt = _avs_bit_blt;
  e->avs_at_exit = _avs_at_exit;
  e->avs_subframe = _avs_subframe;
  e->avs_set_memory_max = _avs_set_memory_max;
  e->avs_set_working_dir = _avs_set_working_dir;
  e->avs_delete_script_environment = _avs_delete_script_environment;
  e->avs_subframe_planar = _avs_subframe_planar;

  e->avs_take_clip = _avs_take_clip;
  e->avs_set_to_clip = _avs_set_to_clip;
  e->avs_release_clip = _avs_release_clip;
  e->avs_copy_clip = _avs_copy_clip;
  e->avs_clip_get_error = _avs_clip_get_error;
  e->avs_get_video_info = _avs_get_video_info;
  e->avs_get_version = _avs_get_version;
  e->avs_get_frame = _avs_get_frame;
  e->avs_get_parity = _avs_get_parity;
  e->avs_get_audio = _avs_get_audio;
  e->avs_set_cache_hints = _avs_set_cache_hints;

  e->avs_release_video_frame = _avs_release_video_frame;
  e->avs_copy_video_frame = _avs_copy_video_frame;

  e->avs_copy_value = _avs_copy_value;
  e->avs_release_value = _avs_release_value;

}
/*
void
_avs_video_frame_init (AVS_VideoFrame *f, PVideoFrame *p, IScriptEnvironment *env)
{
  f->env = env;
  f->frame = p;
  f->error = NULL;
  f->avs_get_pitch = _avs_get_pitch;
  f->avs_get_pitch_p = _avs_get_pitch_p;
  f->avs_get_row_size = _avs_get_row_size;
  f->avs_get_row_size_p = _avs_get_row_size_p;
  f->avs_get_height = _avs_get_height;
  f->avs_get_height_p = _avs_get_height_p;
  f->avs_get_read_ptr = _avs_get_read_ptr;
  f->avs_get_read_ptr_p = _avs_get_read_ptr_p;
  f->avs_is_writable = _avs_is_writable;
  f->avs_get_write_ptr = _avs_get_write_ptr;
  f->avs_get_write_ptr_p = _avs_get_write_ptr_p;
  f->avs_release_frame = _avs_release_frame;
  f->avs_copy_frame = _avs_copy_frame;
  f->avs_get_frame_parity = _avs_get_frame_parity;
  f->avs_set_frame_parity = _avs_set_frame_parity;
  f->avs_get_timestamp = _avs_get_timestamp;
  f->avs_set_timestamp = _avs_set_timestamp;
}
*/
