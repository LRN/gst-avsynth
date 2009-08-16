/*
 * AviSynth C Interface:
 * Copyright (C) 2003 Kevin Atkinson
 *
 * GstAVSynth C Interface:
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
/*
 As a special exception, I give you permission to link to the
 Avisynth C interface with independent modules that communicate with
 the Avisynth C interface solely through the interfaces defined in
 avisynth_c.h, regardless of the license terms of these independent
 modules, and to copy and distribute the resulting combined work
 under terms of your choice, provided that every copy of the
 combined work is accompanied by a complete copy of the source code
 of the Avisynth C interface and Avisynth itself (with the version
 used to produce the combined work), being distributed under the
 terms of the GNU General Public License plus this exception.  An
 independent module is a module which is not derived from or based
 on Avisynth C Interface, such as 3rd-party filters, import and
 export plugins, or graphical user interfaces.
*/

#ifndef __GSTAVSYNTH_SDK_C_H__
#define __GSTAVSYNTH_SDK_C_H__

#ifndef AVISYNTH_INTERFACE_VERSION_DEFINED
#define AVISYNTH_INTERFACE_VERSION_DEFINED
enum { AVISYNTH_INTERFACE_VERSION = 3 };
#endif

/* Define all types necessary for interfacing with avisynth
   Moved from internal.h */
#include <stdint.h>
#include <string.h>
#include <glib.h>

#ifndef BYTE
typedef guint8 BYTE;
#endif

#if !defined(G_OS_WIN32)
typedef gint64 __int64;
#endif
typedef guint64 __uint64;


/* Portability macros from wine (winehq.org) LGPL licensed */

#ifndef __stdcall
# ifdef __i386__
#  ifdef __GNUC__
#   ifdef __APPLE__ /* Mac OS X uses a 16-byte aligned stack and not a 4-byte one */
#    define __stdcall __attribute__((__stdcall__)) __attribute__((__force_align_arg_pointer__))
#   else
#    define __stdcall __attribute__((__stdcall__))
#   endif
#  elif defined(_MSC_VER)
    /* Nothing needs to be done. __stdcall already exists */
#  else
#   error You need to define __stdcall for your compiler
#  endif
# elif defined(__x86_64__) && defined (__GNUC__)
#  define __stdcall __attribute__((ms_abi))
# else
#  define __stdcall
# endif
#endif /* __stdcall */

#ifndef __cdecl
# if defined(__i386__) && defined(__GNUC__)
#  ifdef __APPLE__ /* Mac OS X uses 16-byte aligned stack and not a 4-byte one */
#   define __cdecl __attribute__((__cdecl__)) __attribute__((__force_align_arg_pointer__))
#  else
#   define __cdecl __attribute__((__cdecl__))
#  endif
# elif defined(__x86_64__) && defined (__GNUC__)
#  define __cdecl __attribute__((ms_abi))
# elif !defined(_MSC_VER)
#  define __cdecl
# endif
#endif /* __cdecl */

#ifndef DECLSPEC_NORETURN
# if defined(_MSC_VER) && (_MSC_VER >= 1200) && !defined(MIDL_PASS)
#  define DECLSPEC_NORETURN __declspec(noreturn)
# elif defined(__GNUC__)
#  define DECLSPEC_NORETURN __attribute__((noreturn))
# else
#  define DECLSPEC_NORETURN
# endif
#endif

#ifdef _MSC_VER
# ifdef DECLSPEC_EXPORT
#  undef DECLSPEC_EXPORT
# endif
# define DECLSPEC_EXPORT __declspec(dllexport)
#elif defined(__MINGW32__)
# ifdef DECLSPEC_EXPORT
#  undef DECLSPEC_EXPORT
# endif
# define DECLSPEC_EXPORT __attribute__((dllexport))
#elif defined(__GNUC__) && ((__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 3)))
# define DECLSPEC_EXPORT __attribute__((visibility ("default")))
#else
# define DECLSPEC_EXPORT
#endif

/* End of Wine portability macros */

/* Windows portability macros */

#ifndef OutputDebugString
#define OutputDebugString(x) g_debug("%s", (x))
#endif

/* End of Windows portability macros */


/* Raster types used by VirtualDub & Avisynth */
#define in64 (__int64)(guint16)
typedef guint32	Pixel;
typedef guint32	Pixel32;
typedef guchar	Pixel8;
typedef guint32	PixCoord;
typedef guint32	PixDim;
typedef guint32	PixOffset;


/* Compiler-specific crap */

/* Tell MSVC to stop precompiling here */
#ifdef _MSC_VER
  #pragma hdrstop
#endif

/* Set up debugging macros for MS compilers; for others, step down to the
 standard <assert.h> interface
*/
#ifdef _MSC_VER
  #include <crtdbg.h>
#else
  #define _RPT0(a,b) ((void)0)
  #define _RPT1(a,b,c) ((void)0)
  #define _RPT2(a,b,c,d) ((void)0)
  #define _RPT3(a,b,c,d,e) ((void)0)
  #define _RPT4(a,b,c,d,e,f) ((void)0)

  #define _ASSERTE(x) assert(x)
  #include <assert.h>
#endif

/* Default frame alignment is 16 bytes, to help P4, when using SSE2 */
#define AVS_FRAME_ALIGN 16

#ifdef __cplusplus
#  define EXTERN_C extern "C"
#else
#  define EXTERN_C
#endif

#define AVSC_USE_STDCALL 1

#ifndef AVSC_USE_STDCALL
#  define AVSC_CC __cdecl
#else
#  define AVSC_CC __stdcall
#endif

#define AVSC_EXPORT EXTERN_C __declspec(dllexport)
#ifdef AVISYNTH_C_EXPORTS
#  define AVSC_API(ret) EXTERN_C __declspec(dllexport) ret AVSC_CC
#  define AVSC_STRUCT_API(ret) EXTERN_C __declspec(dllexport) ret AVSC_CC
#else
#  define AVSC_API(ret) EXTERN_C ret AVSC_CC
#  define AVSC_STRUCT_API(ret) ret AVSC_CC
/* EXTERN_C __declspec(dllimport) ret AVSC_CC */
#endif

#ifdef __GNUC__
typedef long long int INT64;
#else
typedef __int64 INT64;
#endif


/**
 * AVS_SampleType:
 * @AVS_SAMPLE_INT8: 8-bit integer samples
 * @AVS_SAMPLE_INT8: 16-bit integer samples
 * @AVS_SAMPLE_INT8: 24-bit integer samples
 * @AVS_SAMPLE_INT8: 32-bit integer samples
 * @AVS_SAMPLE_INT8: floating-point samples
 *
 * Enum value describing the audio sample formats.
 */
typedef enum
{
  AVS_SAMPLE_INT8  = 1<<0,
  AVS_SAMPLE_INT16 = 1<<1, 
  AVS_SAMPLE_INT24 = 1<<2,
  AVS_SAMPLE_INT32 = 1<<3,
  AVS_SAMPLE_FLOAT = 1<<4
} AVS_SampleType;

/**
 * AVS_Plane:
 * @AVS_PLANAR_Y: Y-plane in YUV colorspace
 * @AVS_PLANAR_U: U-plane in YUV colorspace
 * @AVS_PLANAR_V: V-plane in YUV colorspace
 * @AVS_PLANAR_ALIGNED: Plane is aligned
 * @AVS_PLANAR_Y_ALIGNED: Aligned Y-plane in YUV colorspace
 * @AVS_PLANAR_U_ALIGNED: Aligned U-plane in YUV colorspace
 * @AVS_PLANAR_V_ALIGNED: Aligned V-plane in YUV colorspace
 *
 * Enum value describing the plane.
 */
typedef enum
{
  AVS_PLANAR_Y=1<<0,
  AVS_PLANAR_U=1<<1,
  AVS_PLANAR_V=1<<2,
  AVS_PLANAR_ALIGNED=1<<3,
  AVS_PLANAR_Y_ALIGNED=AVS_PLANAR_Y|AVS_PLANAR_ALIGNED,
  AVS_PLANAR_U_ALIGNED=AVS_PLANAR_U|AVS_PLANAR_ALIGNED,
  AVS_PLANAR_V_ALIGNED=AVS_PLANAR_V|AVS_PLANAR_ALIGNED
} AVS_Plane;

/**
 * AVS_ColorspaceProperty:
 * @AVS_CS_BGR: RBG colorspace (ordered as BGR)
 * @AVS_CS_YUV: YUV colorspace
 * @AVS_CS_INTERLEAVED: Colorspace is interleaved
 * @AVS_CS_PLANAR: Colorspace is planar
 *
 * Enum value describing the generic colorspace properties.
 */
typedef enum
{
  AVS_CS_BGR = 1<<28,  
  AVS_CS_YUV = 1<<29,
  AVS_CS_INTERLEAVED = 1<<30,
  AVS_CS_PLANAR = 1<<31
} AVS_ColorspaceProperty;

/**
 * AVS_Colorspace:
 * @AVS_CS_UNKNOWN: Unknown colorspace
 * @AVS_CS_BGR24: Interleaved RGB (ordered as BGR), 24 bps
 * @AVS_CS_BGR32: Interleaved RGB (ordered as BGRA), 32 bps
 * @AVS_CS_YUY2: Interleaved YUV with 2x1 subsampled UV (YUV 4:2:2)
 * @AVS_CS_YV12: Planar YUV (ordered as YVU, VU planes are 2x2 subsampled)
 * @AVS_CS_IYUV: Planar YUV (UV planes are 2x2 subsampled)
 * @AVS_CS_I420: Planar YUV (UV planes are 2x2 subsampled, bottom line first)
 *
 * Enum value describing specific colorspaces.
 */
typedef enum
{
  AVS_CS_UNKNOWN = 0,
  AVS_CS_BGR24 = 1<<0 | AVS_CS_BGR | AVS_CS_INTERLEAVED,
  AVS_CS_BGR32 = 1<<1 | AVS_CS_BGR | AVS_CS_INTERLEAVED,
  AVS_CS_YUY2 = 1<<2 | AVS_CS_YUV | AVS_CS_INTERLEAVED,
  AVS_CS_YV12 = 1<<3 | AVS_CS_YUV | AVS_CS_PLANAR,  /* y-v-u, planar */
  AVS_CS_I420 = 1<<4 | AVS_CS_YUV | AVS_CS_PLANAR,  /* y-u-v, planar */
  AVS_CS_IYUV = 1<<4 | AVS_CS_YUV | AVS_CS_PLANAR  /* same as above */
} AVS_Colorspace;

/**
 * AVS_ParityType:
 * @AVS_IT_BFF: Bottom field first
 * @AVS_IT_TFF: Top field first
 * @AVS_IT_FIELDBASED: Field-based frames
 *
 * Enum value describing frame parity flags.
 */
typedef enum
{
  AVS_IT_BFF = 1<<0,
  AVS_IT_TFF = 1<<1,
  AVS_IT_FIELDBASED = 1<<2
} AVS_ParityType;

/**
 * AVS_FilterParams:
 *
 * This enum is not used
 */
typedef enum
{
  AVS_FILTER_TYPE=1,
  AVS_FILTER_INPUT_COLORSPACE=2,
  AVS_FILTER_OUTPUT_TYPE=9,
  AVS_FILTER_NAME=4,
  AVS_FILTER_AUTHOR=5,
  AVS_FILTER_VERSION=6,
  AVS_FILTER_ARGS=7,
  AVS_FILTER_ARGS_INFO=8,
  AVS_FILTER_ARGS_DESCRIPTION=10,
  AVS_FILTER_DESCRIPTION=11
} AVS_FilterParams;

/**
 * AVS_FilterSubtypes:
 *
 * This enum is not used
 */
typedef enum
{
  AVS_FILTER_TYPE_AUDIO=1,
  AVS_FILTER_TYPE_VIDEO=2,
  AVS_FILTER_OUTPUT_TYPE_SAME=3,
  AVS_FILTER_OUTPUT_TYPE_DIFFERENT=4
} AVS_FilterSubtypes;

/**
 * AVS_CacheHints:
 * @AVS_CACHE_NOTHING: do not cache anything
 * @AVS_CACHE_RANGE: cache frames in range
 *
 * Enum value describing cache hints.
 */
typedef enum
{
  AVS_CACHE_NOTHING=0,
  AVS_CACHE_RANGE=1
} AVS_CacheHints;

/**
 * AVS_CPUFlags:
 * @AVS_CPU_FORCE: N/A
 * @AVS_CPU_FPU: 386/486DX
 * @AVS_CPU_MMX: P55C, K6, PII
 * @AVS_CPU_INTEGER_SSE: PIII, Athlon
 * @AVS_CPU_SSE: PIII, Athlon XP/MP
 * @AVS_CPU_SSE2: PIV, Hammer
 * @AVS_CPU_3DNOW: K6-2
 * @AVS_CPU_3DNOW_EXT: Athlon
 * @AVS_CPU_X86_64: Hammer (note: equiv. to 3DNow + SSE2 which only Hammer will
 * have anyway)
 * @AVS_CPU_SSE3: PIV+, Hammer
 *
 * For GetCPUFlags.  These are backwards-compatible with those in VirtualDub.
 * The comment for each value denotes the slowest CPU to support that extension
 */
typedef enum
{                    
                                /* slowest CPU to support extension */
  AVS_CPU_FORCE        = 0x01,   /* N/A */
  AVS_CPU_FPU          = 0x02,   /* 386/486DX */
  AVS_CPU_MMX          = 0x04,   /* P55C, K6, PII */
  AVS_CPU_INTEGER_SSE  = 0x08,   /* PIII, Athlon */
  AVS_CPU_SSE          = 0x10,   /* PIII, Athlon XP/MP */
  AVS_CPU_SSE2         = 0x20,   /* PIV, Hammer */
  AVS_CPU_3DNOW        = 0x40,   /* K6-2 */
  AVS_CPU_3DNOW_EXT    = 0x80,   /* Athlon  */
  AVS_CPU_X86_64       = 0xA0,   /* Hammer (note: equiv. to 3DNow + SSE2,  */
                                 /* which only Hammer will have anyway) */
  AVS_CPU_SSE3        = 0x100,  /* PIV+, Hammer */
} AVS_CPUFlags;

/**
 * AVS_PlanarChroma:
 * @AVS_PLANAR_CHROMA_ALIGNMENT_OFF: Planar chroma alignment is off
 * @AVS_PLANAR_CHROMA_ALIGNMENT_ON: Planar chroma alignment is on
 * @AVS_PLANAR_CHROMA_ALIGNMENT_TEST: Check planar chroma alignment
 *
 * Enum value describing planar chroma alignment.
 */
typedef enum {
  AVS_PLANAR_CHROMA_ALIGNMENT_OFF,
  AVS_PLANAR_CHROMA_ALIGNMENT_ON,
  AVS_PLANAR_CHROMA_ALIGNMENT_TEST
} AVS_PlanarChroma;

typedef struct _AVS_FilterInfo AVS_FilterInfo;
typedef struct _AVS_VideoInfo AVS_VideoInfo;
typedef struct _AVS_Clip AVS_Clip;
typedef struct _AVS_GenericVideoFilter AVS_GenericVideoFilter;
typedef struct _AVS_ScriptEnvironment AVS_ScriptEnvironment;
typedef struct _AVS_VideoFrame AVS_VideoFrame;
typedef struct _AVS_VideoFrameBuffer AVS_VideoFrameBuffer;
typedef struct _AVS_Value AVS_Value;


/* This is the callback type used by avs_add_function */
typedef AVSC_STRUCT_API(AVS_Value) (*AVSApplyFunc) (AVS_ScriptEnvironment *env, AVS_Value args, gpointer user_data);

/* This is the callback type used for avs__at_exit */
typedef AVSC_STRUCT_API(void) (*AVSShutdownFunc) (void *user_data, AVS_ScriptEnvironment *env);

/********************************
 *
 * AVS_Clip
 *
 */

typedef AVSC_STRUCT_API(void) (*__avs_clip_release) (AVS_Clip *p);
typedef AVSC_STRUCT_API(void) (*__avs_clip_ref) (AVS_Clip *p);
typedef AVSC_STRUCT_API(AVS_Clip *) (*__avs_clip_construct) (AVS_Clip *p);
/*typedef AVSC_STRUCT_API(void) (*__avs_clip_destroy) (AVS_Clip *p, gboolean freeself);*/
typedef AVSC_STRUCT_API(const gchar *) (*__avs_clip_get_error) (AVS_Clip *p);
typedef AVSC_STRUCT_API(gint) (*__avs_clip_get_version) (AVS_Clip *p);
typedef AVSC_STRUCT_API(AVS_VideoInfo) (*__avs_clip_get_video_info)(AVS_Clip *p);
typedef AVSC_STRUCT_API(AVS_VideoFrame *) (*__avs_clip_get_frame) (AVS_Clip *p, gint64 n);
typedef AVSC_STRUCT_API(gint) (*__avs_clip_get_parity) (AVS_Clip *p, gint n);
typedef AVSC_STRUCT_API(gint) (*__avs_clip_get_audio) (AVS_Clip *p, void * buf, gint64 start, gint64 count);
typedef AVSC_STRUCT_API(void) (*__avs_clip_set_cache_hints) (AVS_Clip *p, gint cachehints, gint frame_range);
typedef AVSC_STRUCT_API(AVS_Clip *) (*__avs_clip_copy) (AVS_Clip *p);

/********************************
 *
 * AVS_ScriptEnvironment
 *
 */
typedef AVSC_STRUCT_API(glong) (*__avs_se_get_cpu_flags) (AVS_ScriptEnvironment * p);
typedef AVSC_STRUCT_API(gchar *) (*__avs_se_save_string) (AVS_ScriptEnvironment * p, const gchar* s, gssize length);
typedef AVSC_STRUCT_API(gchar *) (*__avs_se_sprintf) (AVS_ScriptEnvironment * p, const gchar* fmt, ...);
typedef AVSC_STRUCT_API(gchar *) (*__avs_se_vsprintf) (AVS_ScriptEnvironment * p, const gchar* fmt, va_list val);
typedef AVSC_STRUCT_API(gint) (*__avs_se_function_exists) (AVS_ScriptEnvironment * p, const gchar * name);
typedef AVSC_STRUCT_API(AVS_Value) (*__avs_se_invoke) (AVS_ScriptEnvironment * p, const gchar * name, AVS_Value args, const gchar **arg_names);
typedef AVSC_STRUCT_API(AVS_Value) (*__avs_se_get_var) (AVS_ScriptEnvironment * p, const gchar* name);
typedef AVSC_STRUCT_API(gint) (*__avs_se_set_var) (AVS_ScriptEnvironment * p, const gchar* name, AVS_Value val);
typedef AVSC_STRUCT_API(gint) (*__avs_se_set_global_var) (AVS_ScriptEnvironment * p, const gchar* name, AVS_Value val);
typedef AVSC_STRUCT_API(void) (*__avs_se_bit_blt) (AVS_ScriptEnvironment *p, guint8 * dstp, gint dst_pitch, const guint8 * srcp, gint src_pitch, gint row_size, gint height);
typedef AVSC_STRUCT_API(void) (*__avs_se_at_exit) (AVS_ScriptEnvironment *p, AVSShutdownFunc function, void *user_data);
typedef AVSC_STRUCT_API(gint) (*__avs_se_check_version) (AVS_ScriptEnvironment * p, gint version);
typedef AVSC_STRUCT_API(AVS_VideoFrame *) (*__avs_se_subframe) (AVS_ScriptEnvironment * p, AVS_VideoFrame * src0, gint rel_offset, gint new_pitch, gint new_row_size, gint new_height);
typedef AVSC_STRUCT_API(AVS_VideoFrame *) (*__avs_se_subframe_p) (AVS_ScriptEnvironment * p, AVS_VideoFrame * src0, gint rel_offset, gint new_pitch, gint new_row_size, gint new_height, gint rel_offsetU, gint rel_offsetV, gint new_pitchUV);
typedef AVSC_STRUCT_API(gint) (*__avs_se_set_memory_max) (AVS_ScriptEnvironment * p, gint mem);
typedef AVSC_STRUCT_API(int) (*__avs_se_set_working_dir) (AVS_ScriptEnvironment * p, const gchar * newdir);
typedef AVSC_STRUCT_API(gint) (*__avs_se_add_function)(AVS_ScriptEnvironment * p, const char * name, const char * params, 
                 const char* srccapstr, const char* sinkcapstr,
                 AVSApplyFunc applyf, void * user_data);

/********************************
 *
 * AVS_VideoFrame
 *
 *
 */
typedef AVSC_STRUCT_API(gint) (*__avs_se_make_vf_writable) (AVS_ScriptEnvironment *p, AVS_VideoFrame **pvf);
typedef AVSC_STRUCT_API(AVS_VideoFrame *) (*__avs_se_vf_new_a) (AVS_ScriptEnvironment *p, const AVS_VideoInfo *vi, gint align);
typedef AVSC_STRUCT_API(int) (*__avs_vf_get_offset) (const AVS_VideoFrame *p);
typedef AVSC_STRUCT_API(int) (*__avs_vf_get_offset_p) (const AVS_VideoFrame *p, gint plane);
typedef AVSC_STRUCT_API(int) (*__avs_vf_get_pitch) (const AVS_VideoFrame * p);
typedef AVSC_STRUCT_API(int) (*__avs_vf_get_pitch_p) (const AVS_VideoFrame * p, gint plane);
typedef AVSC_STRUCT_API(int) (*__avs_vf_get_row_size) (const AVS_VideoFrame * p);
typedef AVSC_STRUCT_API(int) (*__avs_vf_get_row_size_p) (const AVS_VideoFrame * p, gint plane);
typedef AVSC_STRUCT_API(int) (*__avs_vf_get_height) (const AVS_VideoFrame * p);
typedef AVSC_STRUCT_API(int) (*__avs_vf_get_height_p) (const AVS_VideoFrame * p, gint plane);
typedef AVSC_STRUCT_API(const guint8*) (*__avs_vf_get_read_ptr) (const AVS_VideoFrame * p);
typedef AVSC_STRUCT_API(const guint8*) (*__avs_vf_get_read_ptr_p) (const AVS_VideoFrame * p, gint plane);
typedef AVSC_STRUCT_API(int) (*__avs_vf_is_writable) (const AVS_VideoFrame * p);
typedef AVSC_STRUCT_API(guint8*) (*__avs_vf_get_write_ptr) (const AVS_VideoFrame * p);
typedef AVSC_STRUCT_API(guint8*) (*__avs_vf_get_write_ptr_p) (const AVS_VideoFrame * p, gint plane);
typedef AVSC_STRUCT_API(gint) (*__avs_vf_get_parity) (AVS_VideoFrame *p);
typedef AVSC_STRUCT_API(void) (*__avs_vf_set_parity) (AVS_VideoFrame *p, gint newparity);
typedef AVSC_STRUCT_API(guint64) (*__avs_vf_get_timestamp) (AVS_VideoFrame *p);
typedef AVSC_STRUCT_API(void) (*__avs_vf_set_timestamp) (AVS_VideoFrame *p, guint64 newtimestamp);
typedef AVSC_STRUCT_API(void) (*__avs_vf_ref) (AVS_VideoFrame *p);
typedef AVSC_STRUCT_API(void) (*__avs_vf_destroy) (AVS_VideoFrame *p);
typedef AVSC_STRUCT_API(void) (*__avs_vf_release) (AVS_VideoFrame * p);
typedef AVSC_STRUCT_API(AVS_VideoFrame *) (*__avs_vf_copy) (AVS_VideoFrame * p);

/********************************
 *
 * AVS_GenericVideoFilter
 *
 */
typedef AVSC_STRUCT_API(AVS_GenericVideoFilter *) (*__avs_gvf_construct) (AVS_GenericVideoFilter *p, AVS_Clip *next);
typedef AVSC_STRUCT_API(AVS_VideoFrame *) (*__avs_gvf_get_frame) (AVS_Clip *p, gint64 n, AVS_ScriptEnvironment *env);
typedef AVSC_STRUCT_API(void) (*__avs_gvf_get_audio) (AVS_Clip *p, gpointer buf, gint64 start, gint64 count, AVS_ScriptEnvironment *env);
typedef AVSC_STRUCT_API(AVS_VideoInfo) (*__avs_gvf_get_video_info) (AVS_Clip *p);
typedef AVSC_STRUCT_API(gboolean) (*__avs_gvf_get_parity) (AVS_Clip *p, gint64 n);
typedef AVSC_STRUCT_API(void) (*__avs_gvf_set_cache_hints) (AVS_Clip *p, gint64 cachehints, gint64 frame_range);


/***************
 *
 * AVS_Value
 */
typedef AVSC_STRUCT_API(AVS_Clip *) (*__avs_val_take_clip) (AVS_Value v);
typedef AVSC_STRUCT_API(void) (*__avs_val_set_to_clip) (AVS_Value *v, AVS_Clip *c);
typedef AVSC_STRUCT_API(void) (*__avs_val_copy) (AVS_Value * dest, AVS_Value src);
typedef AVSC_STRUCT_API(void) (*__avs_val_release) (AVS_Value v);



/********************************
 *
 * AVS_Value methods
 *
 */
typedef AVSC_STRUCT_API(AVS_Value) (*__avs_val_new_bool) (gboolean v0);
typedef AVSC_STRUCT_API(AVS_Value) (*__avs_val_new_int) (gint v0);
typedef AVSC_STRUCT_API(AVS_Value) (*__avs_val_new_string) (const gchar * v0);
typedef AVSC_STRUCT_API(AVS_Value) (*__avs_val_new_float) (gfloat v0);
typedef AVSC_STRUCT_API(AVS_Value) (*__avs_val_new_error) (const gchar * v0);
typedef AVSC_STRUCT_API(AVS_Value) (*__avs_val_new_clip) (AVS_Clip * v0);
typedef AVSC_STRUCT_API(AVS_Value) (*__avs_val_new_array) (gint size);

typedef AVSC_STRUCT_API(void) (*AVS_DestroyFunc)(AVS_Clip *p, gboolean freeself);
typedef AVSC_STRUCT_API(AVS_VideoFrame *) (*AVS_GetFrameFunc)(AVS_Clip *p, gint64 n);
typedef AVSC_STRUCT_API(gint) (*AVS_GetAudioFunc)(AVS_Clip *p, gpointer buf, gint64 start, gint64 count);
typedef AVSC_STRUCT_API(AVS_VideoInfo) (*AVS_GetVideoInfoFunc)(AVS_Clip *);
typedef AVSC_STRUCT_API(gboolean) (*AVS_GetParityFunc)(AVS_Clip *, gint64 n);
typedef AVSC_STRUCT_API(void) (*AVS_SetCacheHintsFunc)(AVS_Clip *, gint cachehints, gint64 frame_range);
typedef AVSC_STRUCT_API(const gchar *) (*AvisynthCPluginInitFunc)(AVS_ScriptEnvironment* env);

/* This kills GCC for some reason... */
#ifdef _MSC_VER
  #pragma pack(push,8)
#endif

/**
 * AVS_ScriptEnvironment:
 *
 * A structure that contains C API function pointers.
 */
struct _AVS_ScriptEnvironment
{

/********************************
 *
 * AVS_Clip
 *
 */

  __avs_clip_release avs_clip_release;
  __avs_clip_ref avs_clip_ref;

  __avs_clip_construct avs_clip_construct;

  __avs_clip_get_error avs_clip_get_error;
  __avs_clip_get_version avs_clip_get_version;
  AVS_GetVideoInfoFunc avs_clip_get_video_info;
  AVS_GetFrameFunc avs_clip_get_frame;
  AVS_GetParityFunc avs_clip_get_parity;
  AVS_GetAudioFunc avs_clip_get_audio;
  AVS_SetCacheHintsFunc avs_clip_set_cache_hints;
  __avs_clip_copy avs_clip_copy;

/********************************
 *
 * AVS_ScriptEnvironment
 *
 */

  __avs_se_get_cpu_flags avs_se_get_cpu_flags;
  __avs_se_save_string avs_se_save_string;
  __avs_se_sprintf avs_se_sprintf;
  __avs_se_vsprintf avs_se_vsprintf;
  __avs_se_function_exists avs_se_function_exists;
  __avs_se_invoke avs_se_invoke;
  __avs_se_get_var avs_se_get_var;
  __avs_se_set_var avs_se_set_var;
  __avs_se_set_global_var avs_se_set_global_var;
  __avs_se_bit_blt avs_se_bit_blt;
  __avs_se_at_exit avs_se_at_exit;
  __avs_se_check_version avs_se_check_version;
  __avs_se_subframe avs_se_subframe;
  __avs_se_subframe_p avs_se_subframe_p;
  __avs_se_set_memory_max avs_se_set_memory_max;
  __avs_se_set_working_dir avs_se_set_working_dir;
  __avs_se_add_function avs_se_add_function;
  __avs_se_make_vf_writable avs_se_make_vf_writable;

/********************************
 *
 * AVS_VideoFrame methods
 *
 *
 */

  __avs_se_vf_new_a avs_se_vf_new_a;
  __avs_vf_get_offset avs_vf_get_offset;
  __avs_vf_get_offset_p avs_vf_get_offset_p;
  __avs_vf_get_pitch avs_vf_get_pitch;
  __avs_vf_get_pitch_p avs_vf_get_pitch_p;
  __avs_vf_get_row_size avs_vf_get_row_size;
  __avs_vf_get_row_size_p avs_vf_get_row_size_p;
  __avs_vf_get_height avs_vf_get_height;
  __avs_vf_get_height_p avs_vf_get_height_p;
  __avs_vf_get_read_ptr avs_vf_get_read_ptr;
  __avs_vf_get_read_ptr_p avs_vf_get_read_ptr_p;
  __avs_vf_is_writable avs_vf_is_writable;
  __avs_vf_get_write_ptr avs_vf_get_write_ptr;
  __avs_vf_get_write_ptr_p avs_vf_get_write_ptr_p;
  __avs_vf_get_parity avs_vf_get_parity;
  __avs_vf_set_parity avs_vf_set_parity;
  __avs_vf_get_timestamp avs_vf_get_timestamp;
  __avs_vf_set_timestamp avs_vf_set_timestamp;
  __avs_vf_ref avs_vf_ref;
  __avs_vf_destroy avs_vf_destroy;
  __avs_vf_release avs_vf_release;
  __avs_vf_copy avs_vf_copy;

/********************************
 *
 * AVS_GenericVideoFilter
 *
 */

  __avs_gvf_construct avs_gvf_construct;


/**************
 *
 * AVS_Value
 */

  __avs_val_take_clip avs_val_take_clip;
  __avs_val_set_to_clip avs_val_set_to_clip;
  __avs_val_copy avs_val_copy;
  __avs_val_release avs_val_release;



/********************************
 *
 * AVS_Value methods
 *
 */

  __avs_val_new_bool avs_val_new_bool;
  __avs_val_new_int avs_val_new_int;
  __avs_val_new_string avs_val_new_string;
  __avs_val_new_float avs_val_new_float;
  __avs_val_new_error avs_val_new_error;
  __avs_val_new_clip avs_val_new_clip;
  __avs_val_new_array avs_val_new_array;

  /* Never Touch This. Ever. */
  gpointer internal;
  gchar *error;

};

/**
 * AVS_VideoFrameBuffer:
 *
 * Opaque #AVS_VideoFrameBuffer.
 * VideoFrameBuffer holds information about a memory block which is used
 * for video data.
 */
struct _AVS_VideoFrameBuffer
{
  guint8* data;
  gint data_size;

  /* sequence_number is incremented every time the buffer is changed, so
   that stale views can tell they're no longer valid.
  */
  gint64 sequence_number;

  gint refcount;

  /* GstAVSynth-specific: timestamps of the frame. Must be used by
   * a filter to set timestamps of the resulting frames
   */
  guint64 timestamp;

  /* GstAVSynth-specific: parity (as in VideoInfo). GStreamer allows you to
   * get parity of a frame directly from that frame.
   * GstAVSynth never makes a global assumption about video frame parity
   * that is applies to all frames. You can emulate that behaivour by
   * forcing GStreamer to set parity of each frame to the desired value
   * (which is equivalent to AssumeTFF() or AssumeBFF())
   */
  gint image_type;
};

/**
 * AVS_VideoFrame:
 *
 * Opaque #AVS_VideoFrame.
 * VideoFrame holds a "window" into a VideoFrameBuffer.
 */
struct _AVS_VideoFrame
{
  gint offset, pitch, row_size, height, offsetU, offsetV, pitchUV;  /* U&V offsets are from top of picture. */

  AVS_VideoFrameBuffer * vfb;

  gint refcount;
};

/**
 * AVS_VideoInfo:
 *
 * Opaque #AVS_VideoInfo.
 * VideoInfo holds information about video stream.
 */
struct _AVS_VideoInfo
{
  gint width, height;    /* width=0 means no video */
  guint fps_numerator, fps_denominator;
  gint num_frames;

  gint pixel_type;
  
  gint audio_samples_per_second;   /* 0 means no audio */
  gint sample_type;
  gint64 num_audio_samples;
  gint nchannels;

  /* Imagetype properties */
  gint image_type;
};

/**
 * AVS_HAS_VIDEO:
 * @p: a pointer to #AVS_VideoInfo
 *
 * Returns: TRUE if #AVS_VideoInfo pointed by @p describes a video stream.
 */
#define AVS_HAS_VIDEO(p) ((p)->width != 0)

/**
 * AVS_HAS_AUDIO:
 * @p: a pointer to #AVS_VideoInfo
 *
 * Returns: TRUE if #AVS_VideoInfo pointed by @p describes an audio stream.
 */
#define AVS_HAS_AUDIO(p) (AVS_AUDIO_SAMPLES_PER_SECOND (p) != 0)

/**
 * AVS_IS_RGB:
 * @p: a pointer to #AVS_VideoInfo
 *
 * Returns: TRUE if video is in one of the RGB colorspace.
 */
#define AVS_IS_RGB(p) (!!((p)->pixel_type & AVS_CS_BGR))

/**
 * AVS_IS_RGB24:
 * @p: a pointer to #AVS_VideoInfo
 *
 * Returns: TRUE if video is in #AVS_CS_RGB24 colorspace.
 */
#define AVS_IS_RGB24(p) (((p)->pixel_type & AVS_CS_BGR24) == AVS_CS_BGR24)

/**
 * AVS_IS_RGB32:
 * @p: a pointer to #AVS_VideoInfo
 *
 * Returns: TRUE if video is in #AVS_CS_RGB32 colorspace.
 */
#define AVS_IS_RGB32(p) (((p)->pixel_type & AVS_CS_BGR32) == AVS_CS_BGR32)

/**
 * AVS_IS_YUV:
 * @p: a pointer to #AVS_VideoInfo
 *
 * Returns: TRUE if video is in one of the YUV colorspaces.
 */
#define AVS_IS_YUV(p) (!!((p)->pixel_type & AVS_CS_YUV ))

/**
 * AVS_IS_YUY2:
 * @p: a pointer to #AVS_VideoInfo
 *
 * Returns: TRUE if video is in #AVS_CS_YUY2 colorspace.
 */
#define AVS_IS_YUY2(p) (((p)->pixel_type & AVS_CS_YUY2) == AVS_CS_YUY2)

/**
 * AVS_IS_YV12:
 * @p: a pointer to #AVS_VideoInfo
 *
 * Returns: TRUE if video is in #AVS_CS_YV12 or #AVS_CS_I420 colorspace.
 */
#define AVS_IS_YV12(p) ((((p)->pixel_type & AVS_CS_YV12) == AVS_CS_YV12) || (((p)->pixel_type & AVS_CS_I420) == AVS_CS_I420))

/**
 * AVS_IS_YUY2:
 * @p: a pointer to #AVS_VideoInfo
 * @c_space: #AVS_Colorspace to compare to
 *
 * Returns: TRUE if video colorspace is the same as @c_space.
 */
#define AVS_IS_COLOR_SPACE(p, c_space) (((p)->pixel_type & c_space) == c_space)

/**
 * AVS_IS_PROPERTY:
 * @p: a pointer to #AVS_VideoInfo
 * @property: an #AVS_ColorspaceProperty
 *
 * Returns: TRUE if @property is set in the video.
 */
#define AVS_IS_PROPERTY(p, property) (((p)->pixel_type & property) == property)

/**
 * AVS_IS_PLANAR:
 * @p: a pointer to #AVS_VideoInfo
 *
 * Returns: TRUE if video is planar (#AVS_CS_PLANAR is set).
 */
#define AVS_IS_PLANAR(p) (!!((p)->pixel_type & AVS_CS_PLANAR))
        
/**
 * AVS_IS_FIELD_BASED:
 * @p: a pointer to #AVS_VideoInfo
 *
 * Returns: TRUE if video is field-based (#AVS_CS_FIELDBASED is set).
 */
#define AVS_IS_FIELD_BASED(p) (!!((p)->image_type & AVS_IT_FIELDBASED))

/**
 * AVS_IS_PARITY_KNOWN:
 * @p: a pointer to #AVS_VideoInfo
 *
 * Returns: TRUE if video parity is known (#AVS_CS_FIELDBASED is set and
 * either #AVS_IT_BFF or #AVS_IT_TFF is set).
 */
#define AVS_IS_PARITY_KNOWN(p) (((p)->image_type & AVS_IT_FIELDBASED) && ((p)->image_type & (AVS_IT_BFF | AVS_IT_TFF)))

/**
 * AVS_IS_BFF:
 * @p: a pointer to #AVS_VideoInfo
 *
 * Returns: TRUE if video parity bottom-field-first (#AVS_IT_BFF is set).
 */
#define AVS_IS_BFF(p) (!!((p)->image_type & AVS_IT_BFF))

/**
 * AVS_IS_TFF:
 * @p: a pointer to #AVS_VideoInfo
 *
 * Returns: TRUE if video parity top-field-first (#AVS_IT_TFF is set).
 */
#define AVS_IS_TFF(p) (!!((p)->image_type & AVS_IT_TFF))

/**
 * AVS_IS_VPLANE_FIRST:
 * @p: a pointer to #AVS_VideoInfo
 *
 * Returns: TRUE if video is planar YUV and ordered as YVU.
 */
#define AVS_IS_VPLANE_FIRST(p) (((p)->pixel_type & AVS_CS_YV12) == AVS_CS_YV12)

/**
 * AVS_BITS_PER_PIXEL:
 * @p: a pointer to #AVS_VideoInfo
 *
 * Returns: number of bits per pixel of video frame.
 */
#define AVS_BITS_PER_PIXEL(p) ( \
(p)->pixel_type == AVS_CS_BGR24 ? 24 : \
(p)->pixel_type == AVS_CS_BGR32 ? 32 : \
(p)->pixel_type == AVS_CS_YUY2 ? 16 : \
(p)->pixel_type == AVS_CS_YV12 ? 12 : \
(p)->pixel_type == AVS_CS_I420 ? 12 : 0 \
                               )
/**
 * AVS_BYTES_FROM_PIXELS:
 * @p: a pointer to #AVS_VideoInfo
 * @pixels: number of pixels
 *
 * Returns: number of bytes occupied by @pixels number of pixels.
 */
#define AVS_BYTES_FROM_PIXELS(p, pixels) (pixels * (AVS_BITS_PER_PIXEL(p) >> 3))  /* Will work on planar images, but will return only luma planes */

/**
 * AVS_ROW_SIZE:
 * @p: a pointer to #AVS_VideoInfo
 *
 * Returns: length (in bytes) of a row of the video frame.
 */
#define AVS_ROW_SIZE(p) (AVS_BYTES_FROM_PIXELS (p, (p)->width))  /* Also only returns first plane on planar images */

/**
 * AVS_BMP_SIZE:
 * @p: a pointer to #AVS_VideoInfo
 *
 * Returns: size (in bytes) of the video frame in BMP format.
 */
#define AVS_BMP_SIZE(p) ( \
AVS_IS_PLANAR (p) ? (p)->height * ( (AVS_ROW_SIZE (p) + 3) & ~3) + (((p)->height * ( (AVS_ROW_SIZE (p) + 3) & ~3)) >> 1) : \
(p)->height * ( (AVS_ROW_SIZE (p) + 3) & ~3) \
)

/**
 * AVS_SAMPLES_PER_SECOND:
 * @p: a pointer to #AVS_VideoInfo
 *
 * Returns: number of audio samples per second.
 */
#define AVS_SAMPLES_PER_SECOND(p) AVS_AUDIO_SAMPLES_PER_SECOND(p)

/**
 * AVS_BYTES_PER_CHANNEL_SAMPLE:
 * @p: a pointer to #AVS_VideoInfo
 *
 * Returns: number of bytes occupied by one audio sample in one channel.
 */
#define AVS_BYTES_PER_CHANNEL_SAMPLE(p) ( \
(p)->sample_type == AVS_SAMPLE_INT8 ? sizeof (gint8) : \
(p)->sample_type == AVS_SAMPLE_INT16 ? sizeof (gint16) : \
(p)->sample_type == AVS_SAMPLE_INT24 ? 3 : \
(p)->sample_type == AVS_SAMPLE_INT32 ? sizeof (gint32) : \
(p)->sample_type == AVS_SAMPLE_FLOAT ? sizeof (gfloat) : 0 \
)

/**
 * AVS_BYTES_PER_AUDIO_SAMPLE:
 * @p: a pointer to #AVS_VideoInfo
 *
 * Returns: number of bytes occupied by one audio sample in all channels.
 */
#define AVS_BYTES_PER_AUDIO_SAMPLE(p) ((p)->nchannels * AVS_BYTES_PER_CHANNEL_SAMPLE (p))

#define AVS_AUDIO_SAMPLES_FROM_FRAMES(p, frames) ( (frames) * AVS_SAMPLES_PER_SECOND (p) * (p)->fps_denominator / (p)->fps_numerator)

#define AVS_FRAMES_FROM_AUDIO_SAMPLES(p, samples) ( (gint64) (samples * (gint64) (p)->fps_numerator / (int64) (p)->fps_denominator / (int64) AVS_AUDIO_SAMPLES_PER_SECOND(p) ) )

#define AVS_AUDIO_SAMPLES_FROM_BYTES(p, bytes) (bytes / AVS_BYTES_PER_AUDIO_SAMPLE (p))

#define AVS_BYTES_FROM_AUDIO_SAMPLES(p, samples) (samples * AVS_BYTES_PER_AUDIO_SAMPLE (p))

#define AVS_AUDIO_CHANNELS(p) ((p)->nchannels)

#define AVS_SAMPLE_TYPE(p) ( (p)->sample_type )

/**
 * AVS_MIX_PARITY:
 * @p: a pointer to #AVS_VideoInfo
 * @parity: an #AVS_Parity to mix
 *
 * Returns: sets @parity flag(s).
 */
#define AVS_MIX_PARITY(p, parity) ((p)->image_type |= parity)

/**
 * AVS_FORCE_PARITY:
 * @p: a pointer to #AVS_VideoInfo
 * @parity: an #AVS_Parity to force
 *
 * Returns: sets @parity flag(s) and unsets all other parity flags.
 */
#define AVS_FORCE_PARITY(p, parity) ((p)->image_type &= ~parity)

/**
 * AVS_SET_FIELD_BASED:
 * @p: a pointer to #AVS_VideoInfo
 * @isfieldbased: TRUE or FALSE
 *
 * Returns: sets or unsets #AVS_CS_FIELDBASED.
 */
#define AVS_SET_FIELD_BASED(p, isfieldbased) ( \
isfieldbased ? (p)->image_type |= AVS_IT_FIELDBASED : \
(p)->image_type &= ~AVS_IT_FIELDBASED )

/**
 * AVS_SET_FIELD_BASED:
 * @p: a pointer to #AVS_VideoInfo
 * @num: FPS numerator
 * @den: FPS denominator
 *
 * Returns: sets video to @num/@den FPS.
 */
#define AVS_SET_FPS(p, num, den) \
{ \
  unsigned x = numerator, y = denominator; \
  while (y) { \
    unsigned t = x % y; \
    x = y; \
    y = t; \
  } \
  (p)->fps_numerator = numerator / x; \
  (p)->fps_denominator = denominator / x; \
}

/**
 * AVS_MUL_DIV_FPS:
 * @p: a pointer to #AVS_VideoInfo
 * @multiplier: FPS multiplier
 * @divisor: FPS divisor
 *
 * Range protected multiply-divide of FPS.
 * Multiplies FPS numerator by @multiplier and denominator by @divisor.
 */
#define AVS_MUL_DIV_FPS (p, multiplier, divisor) { \
  __uint64 numerator = ((__int64) (p)->fps_numerator) * ((__int64) multiplier); \
  __uint64 denominator = ((__int64) (p)->fps_denominator) * ((__int64) divisor); \
\
  __uint64 x=numerator, y=denominator; \
  while (y) { \
    __uint64 t = x%y; x = y; y = t; \
  } \
  numerator   /= x; \
  denominator /= x; \
\
  __uint64 temp = numerator | denominator; \
  unsigned u = 0; \
  while (temp & 0xffffffff80000000LL) { \
    temp = temp >> 1; \
    u++; \
  } \
  if (u) { \
    const unsigned round = 1 << (u-1); \
    AVS_SET_FPS(p,((unsigned) ((numerator + round) >> u)), \
            ((unsigned) ((denominator + round) >> u))); \
  } \
  else { \
    (p)->fps_numerator   = (unsigned)numerator; \
    (p)->fps_denominator = (unsigned)denominator; \
  } \
}

/**
 * AVS_IS_SAME_COLORSPACE:
 * @x: a pointer to #AVS_VideoInfo
 * @y: a pointer to another #AVS_VideoInfo
 *
 * Returns: TRUE if @x and @y have the same colorspace.
 */
#define AVS_IS_SAME_COLORSPACE(x, y) (((x)->pixel_type == (y)->pixel_type) || (AVS_IS_YV12 (x) && AVS_IS_YV12 (y)))


/**
 * AVS_FilterInfo:
 *
 * Opaque #AVS_FilterInfo.
 * This structure is not used.
 */
struct _AVS_FilterInfo
{
  /* these members should not be modified outside of the AVS_ApplyFunc callback */
  AVS_Clip * child;
  AVS_VideoInfo vi;
  AVS_ScriptEnvironment * env;

  /*
   Should be set whenever there is an error to report.
   It is cleared before any of the above methods are called
  */
  const gchar *error;
  /*  this is to store an arbitrary value and may be modified at will */
  gpointer user_data;
};

/**
 * AVS_Clip:
 * @parent: AVS_Clip does not have a parent, this is always NULL
 * @child: Pointer to derived class/object
 * @destroy: AVS_Clip class/object destructor
 *
 * @get_frame: GetFrame() method pointer
 * @get_audio: GetAudio() method pointer
 * @get_video_info: GetVideoInfo() method pointer
 * @get_parity: GetParity() method pointer
 * @set_cache_hints: SetCacheHints() method pointer
 *
 * @error: Error string
 * @refcount: reference counter
 *
 * This structure describes AVS_Clip class/object.
 */
struct _AVS_Clip
{
  gpointer parent; /* is NULL for AVS_Clip */
  AVS_Clip *child;
  AVS_DestroyFunc destroy;

  AVS_GetFrameFunc get_frame;
  AVS_GetAudioFunc get_audio;
  AVS_GetVideoInfoFunc get_video_info;
  AVS_GetParityFunc get_parity;
  AVS_SetCacheHintsFunc set_cache_hints;

  gchar *error;
  gint refcount;
};

/**
 * AVS_GenericVideoFilter:
 * @parent: Parent #AVS_Clip object/class
 * @child: Pointer to derived object/class
 * @destroy: AVS_GenericVideoFilter class/object destructor
 *
 * @next_clip: Pointer to the next #AVS_Clip in filter chain
 * @vi: #AVS_VideoInfo that contains information about video stream
 *
 * This structure describes AVS_GenericVideoFilter class/object.
 */
struct _AVS_GenericVideoFilter
{
  AVS_Clip parent;
  AVS_Clip *child;
  AVS_DestroyFunc destroy;

  AVS_Clip *next_clip;
  AVS_VideoInfo vi;
};

/* DO NOT USE THIS STRUCTURE DIRECTLY */
/**
 * AVS_Value:
 *
 * Opaque #AVS_Value.
 *
 * Treat AVS_Value as a fat pointer.  That is use #avs_val_copy
 * and #avs_val_release appropiaty as you would if AVS_Value was
 * a pointer.
 *
 * To maintain source code compatibility with future versions of the
 * GstAVSynth C API don't use the AVS_Value directly.  Use the helper
 * functions (the ones that start with "avs_val_") and macros.
 */
struct _AVS_Value
{
  gint16 type;  /* 'a'rray, 'c'lip, 'b'ool, 'i'nt, 'f'loat, 's'tring, 'v'oid, or 'l'ong 
                for some functions it is 'e'rror*/
  gint16 array_size;
  union
  {
    AVS_Clip *clip; /* do not use directly, use avs_take_clip */
    gboolean boolean;
    gint integer;
    gfloat floating_pt;
    const gchar *string;
    AVS_Value *array;
  } d;
};

/**
 * AVS_DEFINED:
 * @v: an #AVS_Value
 *
 * Returns: TRUE if value is defined (was set to something), FALSE otherwise.
 */
#define AVS_DEFINED(v) (v.type != 'v')

/**
 * AVS_IS_CLIP:
 * @v: an #AVS_Value
 *
 * Returns: TRUE if value is of type 'clip'.
 */
#define AVS_IS_CLIP(v) (v.type == 'c')

/**
 * AVS_IS_BOOL:
 * @v: an #AVS_Value
 *
 * Returns: TRUE if value is of type 'boolean'.
 */
#define AVS_IS_BOOL(v) (v.type == 'b')

/**
 * AVS_IS_INT:
 * @v: an #AVS_Value
 *
 * Returns: TRUE if value is of type 'integer'.
 */
#define AVS_IS_INT(v) (v.type == 'i')

/**
 * AVS_IS_FLOAT:
 * @v: an #AVS_Value
 *
 * Returns: TRUE if value is of type 'float' or 'integer'.
 */
#define AVS_IS_FLOAT(v) (v.type == f || v.type == 'i')

/**
 * AVS_IS_STRING:
 * @v: an #AVS_Value
 *
 * Returns: TRUE if value is of type 'string'.
 */
#define AVS_IS_STRING(v) (v.type == 's')

/**
 * AVS_IS_ARRAY:
 * @v: an #AVS_Value
 *
 * Returns: TRUE if value is of type 'array'.
 */
#define AVS_IS_ARRAY(v) (v.type == 'a')

/**
 * AVS_IS_ERROR:
 * @v: an #AVS_Value
 *
 * Returns: TRUE if value is of type 'error'.
 */
#define AVS_IS_ERROR(v) (v.type == 'e')

/**
 * AVS_AS_BOOL:
 * @v: an #AVS_Value
 *
 * Returns: a boolean value of @v.
 */
#define AVS_AS_BOOL(v) (v.d.boolean)

/**
 * AVS_AS_INT:
 * @v: an #AVS_Value
 *
 * Returns: an integer value of @v.
 */
#define AVS_AS_INT(v) (v.d.integer)

/**
 * AVS_AS_STRING:
 * @v: an #AVS_Value
 *
 * Returns: a string value of @v (@v must be of type 'string' or 'error').
 */
#define AVS_AS_STRING(v) (AVS_IS_ERROR(v) || AVS_IS_STRING(v) ? v.d.string : NULL)

/**
 * AVS_AS_FLOAT:
 * @v: an #AVS_Value
 *
 * Returns: a floating-point value of @v (@v must be of type 'int' or 'float').
 */
#define AVS_AS_FLOAT(v) (AVS_IS_INT(v) ? v.d.integer : v.d.floating_pt)

/**
 * AVS_AS_ERROR:
 * @v: an #AVS_Value
 *
 * Returns: an error value of @v (a string).
 */
#define AVS_AS_ERROR(v) (AVS_IS_ERROR (v) ? v.d.string : NULL)

/**
 * AVS_AS_ARRAY:
 * @v: an #AVS_Value
 *
 * Returns: an array value of @v (a pointer).
 */
#define AVS_AS_ARRAY(v) (v.d.array)

/**
 * AVS_ARRAY_SIZE:
 * @v: an #AVS_Value
 *
 * Returns: size of the array of @v (or 1 if @v is not of type 'array').
 */
#define AVS_ARRAY_SIZE(v) (AVS_IS_ARRAY(v) ? v.array_size : 1)

/**
 * AVS_ARRAY_ELT:
 * @v: an #AVS_Value
 * @i: an index of the element within array
 *
 * Returns: the @i'th #AVS_Value in the array @v, or @v itself, if @v is not
 * of type 'array'.
 */
#define AVS_ARRAY_ELT(v, i) (AVS_IS_ARRAY(v) ? (v).d.array[i] : v)

/*
 * AVS_Value should be initilized with avs_void.
 * Should also be avs_void after its value is released
 * by avs_copy_value.  Consider it the equalvent of setting
 * a pointer to NULL
 */
static const AVS_Value avs_void = {'v', 0};

#ifdef _MSC_VER
  #pragma pack(pop)
#endif

#endif /* __GSTAVSYNTH_SDK_C_H__ */

