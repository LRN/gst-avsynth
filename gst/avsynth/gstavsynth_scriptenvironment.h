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

#include "gstavsynth_sdk.h"
#include "gstavsynth.h"
#include "gstavsynth_videofilter.h"

#ifndef __GST_AVSYNTH_SCRIPTENVIRONMENT_H__
#define __GST_AVSYNTH_SCRIPTENVIRONMENT_H__

class ImplVideoFrameBuffer;
class ImplVideoFrame;
class ImplClip;

typedef AVSValue *PAVSValue;

class ImplVideoFrame: public VideoFrame
{
  friend class ImplVideoFrameBuffer;

  VideoFrame* Subframe(int rel_offset, int new_pitch, int new_row_size, int new_height) const;
  VideoFrame* Subframe(int rel_offset, int new_pitch, int new_row_size, int new_height, int rel_offsetU, int rel_offsetV, int pitchUV) const;
 
  void AddRef();
  void Release();

public:
  ImplVideoFrame(VideoFrameBuffer* _vfb, int _offset, int _pitch, int _row_size, int _height);
  ImplVideoFrame(VideoFrameBuffer* _vfb, int _offset, int _pitch, int _row_size, int _height, int _offsetU, int _offsetV, int _pitchUV);
  ~ImplVideoFrame();

  int GetDataSize()
  {
    if (vfb)
      return vfb->GetDataSize();
    else
      return 0;
  };

};

class ImplVideoFrameBuffer: public VideoFrameBuffer
{
  friend class ImplVideoFrame;
public:
  gboolean touched;
  /* Frame index */
  guint64 selfindex;

  ImplVideoFrameBuffer(int size);
  ImplVideoFrameBuffer();
  ~ImplVideoFrameBuffer();
};

class ImplClip: protected IClip
{
  __stdcall ~ImplClip();
  void AddRef();
  void Release();
};

class ScriptEnvironment : public IScriptEnvironment
{
public:
  ScriptEnvironment(GstAVSynthVideoFilter *parent);
  void __stdcall CheckVersion(int version);
  long __stdcall GetCPUFlags();
  char* __stdcall SaveString(const char* s, int length = -1);
  char* __stdcall Sprintf(const char* fmt, ...);
  char* __stdcall VSprintf(const char* fmt, void* val);
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

  /* Filter implementation */
  ApplyFunc apply;
  void *userdata;

private:

  /* Pointer to the parent object */
  GstAVSynthVideoFilter *parent_object;
  GstAVSynthVideoFilterClass *parent_class;

  /* String storage (for SaveString and *print function implementations) */
  GStringChunk *string_dump;

  /* From AviSynth */
  bool PlanarChromaAlignmentState;

  VideoFrameBuffer* NewFrameBuffer(int size);
  VideoFrameBuffer* GetFrameBuffer2(int size);
  VideoFrameBuffer* GetFrameBuffer(int size);

};

#endif //__GST_AVSYNTH_SCRIPTENVIRONMENT_H__
