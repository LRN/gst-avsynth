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

#include "gstavsynth.h"
#include "gstavsynth_sdk.h"
#include "gstavsynth_sdk_cpp.h"

#ifndef __GST_AVSYNTH_SCRIPTENVIRONMENT_CPP_H__
#define __GST_AVSYNTH_SCRIPTENVIRONMENT_CPP_H__

class ImplVideoFrameBuffer;
class ImplVideoFrame;
class ImplClip;

typedef AVSValue *PAVSValue;

class ImplVideoFrameBuffer: public VideoFrameBuffer
{
  friend class ImplVideoFrame;
public:
  ImplVideoFrameBuffer(): VideoFrameBuffer()
  {
  }

  ImplVideoFrameBuffer(int size): VideoFrameBuffer(size)
  {
    g_atomic_int_inc (&sequence_number);
  }

  ~ImplVideoFrameBuffer() {
/* FROM_AVISYNTH_BEGIN */
    _ASSERTE(refcount == 0);
    g_atomic_int_inc (&sequence_number); // HACK : Notify any children with a pointer, this buffer has changed!
    if (data) delete[] data;
    data = 0; // and mark it invalid !
    data_size = 0;   // and don't forget to set the size to 0 as well!
/* FROM_AVISYNTH_END */
  }

  ImplVideoFrameBuffer(AVS_VideoFrameBuffer *in_vfb_c):VideoFrameBuffer(in_vfb_c->data_size)
  {
    memcpy (data, in_vfb_c->data, data_size);
    timestamp = in_vfb_c->timestamp;
    image_type = in_vfb_c->image_type;
  }
};

class ImplVideoFrame: public VideoFrame
{
  friend class ImplVideoFrameBuffer;

  VideoFrame* Subframe (int rel_offset, int new_pitch, int new_row_size, int new_height) const {
    return new ImplVideoFrame(vfb, offset+rel_offset, new_pitch, new_row_size, new_height);
  }

  VideoFrame* Subframe (int rel_offset, int new_pitch, int new_row_size, int new_height,
                        int rel_offsetU, int rel_offsetV, int new_pitchUV) const {
    return new ImplVideoFrame(vfb, offset+rel_offset, new_pitch, new_row_size, new_height,
                        rel_offsetU+offsetU, rel_offsetV+offsetV, new_pitchUV);
  }
 
  void AddRef ()
  {
    g_atomic_int_inc (&refcount);
  }

  void Release()
  {
    if (g_atomic_int_dec_and_test (&refcount))
      delete this;
  }

public:
  int GetDataSize()
  {
    if (vfb)
      return vfb->GetDataSize();
    else
      return 0;
  };

  ImplVideoFrame(VideoFrameBuffer* _vfb, int _offset, int _pitch, int _row_size, int _height):
      VideoFrame (_vfb, _offset, _pitch, _row_size, _height)
  {
    g_atomic_int_inc (&dynamic_cast<ImplVideoFrameBuffer*>(vfb)->refcount);
  }

  ImplVideoFrame(VideoFrameBuffer* _vfb, int _offset, int _pitch, int _row_size, int _height,
                       int _offsetU, int _offsetV, int _pitchUV):
      VideoFrame (_vfb, _offset, _pitch, _row_size, _height, _offsetU, _offsetV, _pitchUV)
  {
    g_atomic_int_inc (&dynamic_cast<ImplVideoFrameBuffer*>(vfb)->refcount);
  }

  ~ImplVideoFrame ()
  {
    g_atomic_int_dec_and_test (&dynamic_cast<ImplVideoFrameBuffer*>(vfb)->refcount);
    if (dynamic_cast<ImplVideoFrameBuffer*>(vfb)->refcount == 0)
      delete vfb;
  }

};

class ImplClip: protected IClip
{
  __stdcall ~ImplClip();
  void AddRef()
  {
    g_atomic_int_inc (&refcnt);
  }

  void Release()
  {
    g_atomic_int_dec_and_test (&refcnt);
    if (!refcnt) delete this;
  }
};

class ScriptEnvironment : public IScriptEnvironment
{
public:
  ScriptEnvironment(gpointer parent);
  void __stdcall CheckVersion(int version);
  long __stdcall GetCPUFlags();
  char* __stdcall SaveString(const char* s, int length = -1);
  char* __stdcall Sprintf(const char* fmt, ...);
  char* __stdcall VSprintf(const char* fmt, va_list val);
  void __stdcall ThrowError(const char* fmt, ...);
  virtual void __stdcall AddFunction(const char* name, const char* paramstr, const char* srccapstr, const char* sinkcapstr, ApplyFunc apply, void* user_data=0);
  bool __stdcall FunctionExists(const char* name);
  AVSValue __stdcall Invoke(const char* name, const AVSValue args, const char** arg_names=0);
  AVSValue __stdcall GetVar(const char* name);
  bool __stdcall SetVar(const char* name, const AVSValue& val);
  bool __stdcall SetGlobalVar(const char* name, const AVSValue& val);
  void __stdcall PushContext(int level=0);
  void __stdcall PopContext();
  void __stdcall PopContextGlobal();
  PVideoFrame __stdcall NewVideoFrame(const VideoInfo& vi, int align);
  PVideoFrame NewVideoFrame(int row_size, int height, int align);
  PVideoFrame NewPlanarVideoFrame(int width, int height, int align, bool U_first);
  bool __stdcall MakeWritable(PVideoFrame* pvf);
  void __stdcall BitBlt(BYTE* dstp, int dst_pitch, const BYTE* srcp, int src_pitch, int row_size, int height);
  void __stdcall AtExit(IScriptEnvironment::ShutdownFunc function, void* user_data);
  PVideoFrame __stdcall Subframe(PVideoFrame src, int rel_offset, int new_pitch, int new_row_size, int new_height);
  int __stdcall SetMemoryMax(int mem);
  int __stdcall SetWorkingDir(const char * newdir);
  __stdcall ~ScriptEnvironment();
  void* __stdcall ManageCache(int key, void* data);
  bool __stdcall PlanarChromaAlignment(IScriptEnvironment::PlanarChromaAlignmentMode key);
  PVideoFrame __stdcall SubframePlanar(PVideoFrame src, int rel_offset, int new_pitch, int new_row_size, int new_height, int rel_offsetU, int rel_offsetV, int new_pitchUV);

  AVS_ScriptEnvironment *env_c;

private:

  /* Pointer to the parent object */
  gpointer parent_object;
  gpointer parent_class;

  VideoFrameBuffer* GetFrameBuffer(int size);

};

void BitBlt(guint8* dstp, int dst_pitch, const guint8* srcp, int src_pitch, int row_size, int height);

#endif //__GST_AVSYNTH_SCRIPTENVIRONMENT_CPP_H__
