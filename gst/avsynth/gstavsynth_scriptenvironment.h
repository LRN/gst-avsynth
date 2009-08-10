/*
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

#ifndef __GST_AVSYNTH_SCRIPTENVIRONMENT_H__
#define __GST_AVSYNTH_SCRIPTENVIRONMENT_H__

void BitBlt(guint8* dstp, int dst_pitch, const guint8* srcp, int src_pitch, int row_size, int height);
void AVSC_CC _avs_vfb_destroy (AVS_VideoFrameBuffer *p);
void AVSC_CC _avs_vfb_release (AVS_VideoFrameBuffer *p);
AVS_VideoFrameBuffer * AVSC_CC _avs_vfb_new (gint size);
AVS_VideoFrameBuffer * AVSC_CC _avs_vfb_copy (AVS_VideoFrameBuffer *p);
int AVSC_CC _avs_vf_get_offset (const AVS_VideoFrame *p);
int AVSC_CC _avs_vf_get_offset_p (const AVS_VideoFrame *p, int plane);
int AVSC_CC _avs_vf_get_pitch(const AVS_VideoFrame * p);
int AVSC_CC _avs_vf_get_pitch_p(const AVS_VideoFrame * p, int plane);
int AVSC_CC _avs_vf_get_row_size(const AVS_VideoFrame * p);
int AVSC_CC _avs_vf_get_row_size_p(const AVS_VideoFrame * p, int plane);
int AVSC_CC _avs_vf_get_height(const AVS_VideoFrame * p);
int AVSC_CC _avs_vf_get_height_p(const AVS_VideoFrame * p, int plane);
const guint8* AVSC_CC _avs_vf_get_read_ptr(const AVS_VideoFrame * p);
const guint8* AVSC_CC _avs_vf_get_read_ptr_p(const AVS_VideoFrame * p, int plane);
int AVSC_CC _avs_vf_is_writable(const AVS_VideoFrame * p);
guint8* AVSC_CC _avs_vf_get_write_ptr(const AVS_VideoFrame * p);
guint8* AVSC_CC _avs_vf_get_write_ptr_p(const AVS_VideoFrame * p, gint plane);
int AVSC_CC _avs_vf_get_parity(AVS_VideoFrame *p);
void AVSC_CC _avs_vf_set_parity (AVS_VideoFrame *p, int newparity);
guint64 AVSC_CC _avs_vf_get_timestamp (AVS_VideoFrame *p);
void AVSC_CC _avs_vf_set_timestamp (AVS_VideoFrame *p, guint64 newtimestamp);
void AVSC_CC _avs_vf_ref (AVS_VideoFrame *p);
void AVSC_CC _avs_vf_destroy (AVS_VideoFrame *p);
void AVSC_CC _avs_vf_release(AVS_VideoFrame *p);
void AVSC_CC _avs_gvf_destroy (gpointer p, gboolean freeself);
AVS_GenericVideoFilter * AVSC_CC _avs_gvf_construct (AVS_GenericVideoFilter *p, AVS_Clip *next);
AVS_VideoFrame * AVSC_CC _avs_gvf_get_frame (AVS_Clip *p, gint64 n);
gint AVSC_CC _avs_gvf_get_audio (AVS_Clip *p, gpointer buf, gint64 start, gint64 count);
AVS_VideoInfo AVSC_CC _avs_gvf_get_video_info (AVS_Clip *p);
gboolean AVSC_CC _avs_gvf_get_parity (AVS_Clip *p, gint64 n);
void AVSC_CC _avs_gvf_set_cache_hints (AVS_Clip *p, gint cachehints, gint64 frame_range);
void AVSC_CC _avs_clip_release (AVS_Clip * p);
void AVSC_CC _avs_clip_ref (AVS_Clip *p);
AVS_Clip * AVSC_CC _avs_clip_construct (AVS_Clip *p);
void AVSC_CC _avs_clip_destroy (AVS_Clip *p, gboolean freeself);
const gchar * AVSC_CC _avs_clip_get_error(AVS_Clip * p);
gint AVSC_CC _avs_clip_get_version(AVS_Clip * p);
AVS_VideoInfo AVSC_CC _avs_clip_get_video_info(AVS_Clip  * p);
AVS_VideoFrame * AVSC_CC _avs_clip_get_frame(AVS_Clip *p, gint64 n);
int AVSC_CC _avs_clip_get_parity(AVS_Clip * p, gint64 n);
gint AVSC_CC _avs_clip_get_audio(AVS_Clip * p, void * buf, gint64 start, gint64 count);
void AVSC_CC _avs_clip_set_cache_hints(AVS_Clip * p, gint cachehints, gint64 frame_range);
AVS_Clip * AVSC_CC _avs_val_take_clip(AVS_Value v);
void AVSC_CC _avs_val_set_to_clip(AVS_Value *v, AVS_Clip *c);
void AVSC_CC _avs_val_copy(AVS_Value * dest, AVS_Value src);
void AVSC_CC _avs_val_release(AVS_Value v);
AVS_Value AVSC_CC _avs_val_new_bool (gboolean v0);
AVS_Value AVSC_CC _avs_val_new_int (gint v0);
AVS_Value AVSC_CC _avs_val_new_string(const gchar * v0);
AVS_Value AVSC_CC _avs_val_new_float(gfloat v0);
AVS_Value AVSC_CC _avs_val_new_error(const gchar * v0);
AVS_Value AVSC_CC _avs_val_new_clip(AVS_Clip * v0);
AVS_Value AVSC_CC _avs_val_new_array(gint size);
int AVSC_CC _avs_se_add_function(AVS_ScriptEnvironment * p, const char * name, const char * params, const char* srccapstr, const char* sinkcapstr, AVSApplyFunc applyf, void * user_data);
glong AVSC_CC _avs_se_get_cpu_flags(AVS_ScriptEnvironment * p);
char * AVSC_CC _avs_se_save_string(AVS_ScriptEnvironment * p, const char* s, gssize length);
char * AVSC_CC _avs_se_sprintf(AVS_ScriptEnvironment * p, const char* fmt, ...);
char * AVSC_CC _avs_se_vsprintf(AVS_ScriptEnvironment * p, const char* fmt, va_list val);
int AVSC_CC _avs_se_function_exists(AVS_ScriptEnvironment * p, const char * name);
AVS_Value AVSC_CC _avs_se_invoke(AVS_ScriptEnvironment * p, const char * name, AVS_Value args, const char * * arg_names);
AVS_Value AVSC_CC _avs_se_get_var(AVS_ScriptEnvironment * p, const char* name);
int AVSC_CC _avs_se_set_var(AVS_ScriptEnvironment * p, const char* name, AVS_Value val);
int AVSC_CC _avs_se_set_global_var(AVS_ScriptEnvironment * p, const char* name, AVS_Value val);
AVS_VideoFrame * AVSC_CC _avs_se_vf_new_a(AVS_ScriptEnvironment *p, const AVS_VideoInfo *  vi, gint align);
AVS_VideoFrame * AVSC_CC _avs_se_vf_new (AVS_ScriptEnvironment *p, gint row_size, gint height, gint align);
gboolean AVSC_CC _avs_se_planar_chroma_alignment_state (AVS_ScriptEnvironment *p, gint key);
AVS_VideoFrame * AVSC_CC _avs_se_vf_new_p (AVS_ScriptEnvironment *p, gint width, gint height, gint align, gboolean U_first);
int AVSC_CC _avs_se_make_vf_writable(AVS_ScriptEnvironment *p, AVS_VideoFrame **pvf);
void AVSC_CC _avs_se_bit_blt(AVS_ScriptEnvironment *p, BYTE * dstp, int dst_pitch, const BYTE * srcp, int src_pitch, int row_size, int height);
void AVSC_CC _avs_se_at_exit(AVS_ScriptEnvironment *p, AVSShutdownFunc function, void * user_data);
int AVSC_CC _avs_se_check_version(AVS_ScriptEnvironment * p, gint version);
AVS_VideoFrame * AVSC_CC _avs_se_subframe(AVS_ScriptEnvironment * p, AVS_VideoFrame * src0, int rel_offset, int new_pitch, int new_row_size, int new_height);
AVS_VideoFrame * AVSC_CC _avs_se_subframe_p(AVS_ScriptEnvironment * p, AVS_VideoFrame * src0, int rel_offset, int new_pitch, int new_row_size, int new_height, int rel_offsetU, int rel_offsetV, int new_pitchUV);
int AVSC_CC _avs_se_set_memory_max(AVS_ScriptEnvironment * p, int mem);
int AVSC_CC _avs_se_set_working_dir(AVS_ScriptEnvironment * p, const char * newdir);
#endif //__GST_AVSYNTH_SCRIPTENVIRONMENT_H__
