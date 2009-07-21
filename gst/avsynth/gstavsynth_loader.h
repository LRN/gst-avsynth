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

#ifndef __GST_AVSYNTH_LOADER_H__
#define __GST_AVSYNTH_LOADER_H__

#include "gstavsynth.h"
#include "gstavsynth_sdk.h"
#include "gstavsynth_videofilter.h"

typedef const char* (__stdcall *AvisynthPluginInitFunc)(IScriptEnvironment* env);

/* This class implements only a few methods,
 * and is only used to pre-scan plugins
 */
class LoaderScriptEnvironment : public IScriptEnvironment
{
public:
  LoaderScriptEnvironment();
  void __stdcall AddFunction(const char* name, const char* paramstr, const char* srccapstr, const char* sinkcapstr, IScriptEnvironment::ApplyFunc apply, void* user_data=0);
  //bool __stdcall FunctionExists(const char* name);
  __stdcall ~LoaderScriptEnvironment();

  void __stdcall CheckVersion(int version) {};
  long __stdcall GetCPUFlags() { return NULL; };
  char* __stdcall SaveString(const char* s, int length = -1) { return NULL; };
  char* __stdcall Sprintf(const char* fmt, ...) { return NULL; };
  char* __stdcall VSprintf(const char* fmt, va_list val) { return NULL; };
  void __stdcall ThrowError(const char* fmt, ...) {};
  bool __stdcall FunctionExists(const char* name) { return false; };
  AVSValue __stdcall Invoke(const char* name, const AVSValue args, const char** arg_names=0) { return NULL; };
  AVSValue __stdcall GetVar(const char* name) { return NULL; };
  bool __stdcall SetVar(const char* name, const AVSValue& val) { return false; };
  bool __stdcall SetGlobalVar(const char* name, const AVSValue& val) { return false; };
  void __stdcall PushContext(int level=0) {};
  void __stdcall PopContext() {};
  void __stdcall PopContextGlobal() {};
  PVideoFrame __stdcall NewVideoFrame(const VideoInfo& vi, int align) { return NULL; };
  PVideoFrame NewVideoFrame(int row_size, int height, int align) { return NULL; };
  PVideoFrame NewPlanarVideoFrame(int width, int height, int align, bool U_first) { return NULL; };
  bool __stdcall MakeWritable(PVideoFrame* pvf) { return false; };
  void __stdcall BitBlt(BYTE* dstp, int dst_pitch, const BYTE* srcp, int src_pitch, int row_size, int height) {};
  void __stdcall AtExit(IScriptEnvironment::ShutdownFunc function, void* user_data) {};
  PVideoFrame __stdcall Subframe(PVideoFrame src, int rel_offset, int new_pitch, int new_row_size, int new_height) { return NULL; };
  int __stdcall SetMemoryMax(int mem) { return NULL; };
  int __stdcall SetWorkingDir(const char * newdir) { return NULL; };
  void* __stdcall ManageCache(int key, void* data) { return NULL; }; 
  bool __stdcall PlanarChromaAlignment(IScriptEnvironment::PlanarChromaAlignmentMode key) { return false; };
  PVideoFrame __stdcall SubframePlanar(PVideoFrame src, int rel_offset, int new_pitch, int new_row_size, int new_height, int rel_offsetU, int rel_offsetV, int new_pitchUV) { return NULL; };

  void SetFilename (gchar *name);
  void SetPrefix (gchar *prefix);
  void SetPlugin (GstPlugin *plug);

private:
  /* GStreamer plugin that registers the type */
  GstPlugin *plugin;
  /* Path to the plugin module (UTF-8) */
  const gchar *filename;
  /* Prefix for full names*/
  const gchar *fullnameprefix;

};

gboolean gst_avsynth_video_filter_register (GstPlugin * plugin, gchar *plugindirs);

#endif //__GST_AVSYNTH_LOADER_H__
