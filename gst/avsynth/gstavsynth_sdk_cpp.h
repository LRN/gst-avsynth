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
//
// Linking GstAVSynth statically or dynamically with other modules is making a
// combined work based on GstAVSynth.  Thus, the terms and conditions of the GNU
// General Public License cover the whole combination.
//
// As a special exception, the copyright holders of GstAVSynth give you
// permission to link GstAVSynth with independent modules that communicate with
// Avisynth solely through the interfaces defined in avisynth.h, regardless of the license
// terms of these independent modules, and to copy and distribute the
// resulting combined work under terms of your choice, provided that
// every copy of the combined work is accompanied by a complete copy of
// the source code of Avisynth (the version of GstAVSynth used to produce the
// combined work), being distributed under the terms of the GNU General
// Public License plus this exception.  An independent module is a module
// which is not derived from or based on GstAVSynth, such as 3rd-party filters,
// import and export plugins, or graphical user interfaces.

#ifndef __GSTAVSYNTH_SDK_CPP_H__
#define __GSTAVSYNTH_SDK_CPP_H__

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

// COM interface macros
//#include <objbase.h>

// Raster types used by VirtualDub & Avisynth
#define in64 (__int64)(guint16)
typedef guint32	Pixel;
typedef guint32	Pixel32;
typedef guchar	Pixel8;
typedef guint32	PixCoord;
typedef guint32	PixDim;
typedef guint32	PixOffset;


/* Compiler-specific crap */

// Tell MSVC to stop precompiling here
#ifdef _MSC_VER
  #pragma hdrstop
#endif

// Set up debugging macros for MS compilers; for others, step down to the
// standard <assert.h> interface
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



// I had problems with Premiere wanting 1-byte alignment for its structures,
// so I now set the Avisynth struct alignment explicitly here.
#pragma pack(push,8)

#define FRAME_ALIGN 16
// Default frame alignment is 16 bytes, to help P4, when using SSE2

// The VideoInfo struct holds global information about a clip (i.e.
// information that does not depend on the frame number).  The GetVideoInfo
// method in IClip returns this struct.

// Audio Sample information
typedef float SFLOAT;

enum {SAMPLE_INT8  = 1<<0,
      SAMPLE_INT16 = 1<<1,
      SAMPLE_INT24 = 1<<2,    // Int24 is a very stupid thing to code, but it's supported by some hardware.
      SAMPLE_INT32 = 1<<3,
      SAMPLE_FLOAT = 1<<4};

enum {
   PLANAR_Y=1<<0,
   PLANAR_U=1<<1,
   PLANAR_V=1<<2,
   PLANAR_ALIGNED=1<<3,
   PLANAR_Y_ALIGNED=PLANAR_Y|PLANAR_ALIGNED,
   PLANAR_U_ALIGNED=PLANAR_U|PLANAR_ALIGNED,
   PLANAR_V_ALIGNED=PLANAR_V|PLANAR_ALIGNED,
  };

class AvisynthError /* exception */ {
public:
  const char* const msg;
  AvisynthError(const char* _msg) : msg(_msg) {}
};

struct VideoInfo {
  int width, height;    // width=0 means no video
  unsigned fps_numerator, fps_denominator;
  int num_frames;
  // This is more extensible than previous versions. More properties can be added seeminglesly.

  // Colorspace properties.
  enum {
    CS_BGR = 1<<28,
    CS_YUV = 1<<29,
    CS_INTERLEAVED = 1<<30,
    CS_PLANAR = 1<<31
  };

  // Specific colorformats
  enum { CS_UNKNOWN = 0,
         CS_BGR24 = 1<<0 | CS_BGR | CS_INTERLEAVED,
         CS_BGR32 = 1<<1 | CS_BGR | CS_INTERLEAVED,
         CS_YUY2  = 1<<2 | CS_YUV | CS_INTERLEAVED,
         CS_YV12  = 1<<3 | CS_YUV | CS_PLANAR,  // y-v-u, 4:2:0 planar
         CS_I420  = 1<<4 | CS_YUV | CS_PLANAR,  // y-u-v, 4:2:0 planar
         CS_IYUV  = 1<<4 | CS_YUV | CS_PLANAR,  // same as above
  };
  int pixel_type;                // changed to int as of 2.5


  int audio_samples_per_second;   // 0 means no audio
  int sample_type;                // as of 2.5
  __int64 num_audio_samples;      // changed as of 2.5
  int nchannels;                  // as of 2.5

  // Imagetype properties

  int image_type;

  enum {
    IT_BFF = 1<<0,
    IT_TFF = 1<<1,
    IT_FIELDBASED = 1<<2
  };

  // useful functions of the above
  bool HasVideo() const { return (width!=0); }
  bool HasAudio() const { return (audio_samples_per_second!=0); }
  bool IsRGB() const { return !!(pixel_type&CS_BGR); }
  bool IsRGB24() const { return (pixel_type&CS_BGR24)==CS_BGR24; } // Clear out additional properties
  bool IsRGB32() const { return (pixel_type & CS_BGR32) == CS_BGR32 ; }
  bool IsYUV() const { return !!(pixel_type&CS_YUV ); }
  bool IsYUY2() const { return (pixel_type & CS_YUY2) == CS_YUY2; }
  bool IsYV12() const { return ((pixel_type & CS_YV12) == CS_YV12)||((pixel_type & CS_I420) == CS_I420); }
  bool IsColorSpace(int c_space) const { return ((pixel_type & c_space) == c_space); }
  bool Is(int property) const { return ((pixel_type & property)==property ); }
  bool IsPlanar() const { return !!(pixel_type & CS_PLANAR); }
  bool IsFieldBased() const { return !!(image_type & IT_FIELDBASED); }
  bool IsParityKnown() const { return ((image_type & IT_FIELDBASED)&&(image_type & (IT_BFF|IT_TFF))); }
  bool IsBFF() const { return !!(image_type & IT_BFF); }
  bool IsTFF() const { return !!(image_type & IT_TFF); }

  bool IsVPlaneFirst() const {return ((pixel_type & CS_YV12) == CS_YV12); }  // Don't use this
  int BytesFromPixels(int pixels) const { return pixels * (BitsPerPixel()>>3); }   // Will not work on planar images, but will return only luma planes
  int RowSize() const { return BytesFromPixels(width); }  // Also only returns first plane on planar images
  int BMPSize() const { if (IsPlanar()) {int p = height * ((RowSize()+3) & ~3); p+=p>>1; return p;  } return height * ((RowSize()+3) & ~3); }
  __int64 AudioSamplesFromFrames(__int64 frames) const { return (fps_numerator && HasVideo()) ? ((__int64)(frames) * audio_samples_per_second * fps_denominator / fps_numerator) : 0; }
  int FramesFromAudioSamples(__int64 samples) const { return (fps_denominator && HasAudio()) ? (int)((samples * (__int64)fps_numerator)/((__int64)fps_denominator * (__int64)audio_samples_per_second)) : 0; }
  __int64 AudioSamplesFromBytes(__int64 bytes) const { return HasAudio() ? bytes / BytesPerAudioSample() : 0; }
  __int64 BytesFromAudioSamples(__int64 samples) const { return samples * BytesPerAudioSample(); }
  int AudioChannels() const { return HasAudio() ? nchannels : 0; }
  int SampleType() const{ return sample_type;}
  bool IsSampleType(int testtype) const{ return !!(sample_type&testtype);}
  int SamplesPerSecond() const { return audio_samples_per_second; }
  int BytesPerAudioSample() const { return nchannels*BytesPerChannelSample();}
  void SetFieldBased(bool isfieldbased)  { if (isfieldbased) image_type|=IT_FIELDBASED; else  image_type&=~IT_FIELDBASED; }
  void Set(int property)  { image_type|=property; }
  void Clear(int property)  { image_type&=~property; }

  int BitsPerPixel() const {
    switch (pixel_type) {
      case CS_BGR24:
        return 24;
      case CS_BGR32:
        return 32;
      case CS_YUY2:
        return 16;
      case CS_YV12:
      case CS_I420:
        return 12;
      default:
        return 0;
    }
  }

  int BytesPerChannelSample() const {
    switch (sample_type) {
    case SAMPLE_INT8:
      return sizeof(signed char);
    case SAMPLE_INT16:
      return sizeof(signed short);
    case SAMPLE_INT24:
      return 3;
    case SAMPLE_INT32:
      return sizeof(signed int);
    case SAMPLE_FLOAT:
      return sizeof(SFLOAT);
    default:
      _ASSERTE("Sample type not recognized!");
      return 0;
    }
  }

  // useful mutator
  void SetFPS(unsigned numerator, unsigned denominator) {
	if ((numerator == 0) || (denominator == 0)) {
	  fps_numerator = 0;
	  fps_denominator = 1;
	}
	else {
	  unsigned x=numerator, y=denominator;
	  while (y) {   // find gcd
		unsigned t = x%y; x = y; y = t;
	  }
	  fps_numerator = numerator/x;
	  fps_denominator = denominator/x;
	}
  }

  // Range protected multiply-divide of FPS
  void MulDivFPS(unsigned multiplier, unsigned divisor) {
    __uint64 numerator = ((__int64) fps_numerator) * ((__int64) multiplier);
    __uint64 denominator = ((__int64) fps_denominator) * ((__int64) divisor);
    //__uint64 numerator   = UInt32x32To64(fps_numerator,   multiplier);
    //__uint64 denominator = UInt32x32To64(fps_denominator, divisor);

	__uint64 x=numerator, y=denominator;
	while (y) {   // find gcd
	  __uint64 t = x%y; x = y; y = t;
	}
	numerator   /= x; // normalize
	denominator /= x;

	__uint64 temp = numerator | denominator; // Just looking top bit
	unsigned u = 0;
	while (temp & 0xffffffff80000000LL) { // or perhaps > 16777216*2
	  temp = temp >> 1;
	  u++;
	}
	if (u) { // Scale to fit
	  const unsigned round = 1 << (u-1);
	  SetFPS((unsigned) ((numerator + round) >> u),
	          (unsigned) ((denominator + round) >> u));
	}
	else {
	  fps_numerator   = (unsigned)numerator;
	  fps_denominator = (unsigned)denominator;
	}
  }

  // Test for same colorspace
  bool IsSameColorspace(const VideoInfo& vi) const {
    if (vi.pixel_type == pixel_type) return TRUE;
    if (IsYV12() && vi.IsYV12()) return TRUE;
    return FALSE;
  }

};




// VideoFrameBuffer holds information about a memory block which is used
// for video data.  For efficiency, instances of this class are not deleted
// when the refcount reaches zero; instead they're stored in a linked list
// to be reused.  The instances are deleted when the corresponding AVS
// file is closed.

class VideoFrameBuffer {

protected:
  BYTE* data;
  int data_size;
  // sequence_number is incremented every time the buffer is changed, so
  // that stale views can tell they're no longer valid.
  int sequence_number;

  int refcount;

  /* GstAVSynth-specific: timestamps of the frame. May be optionally used by
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
  int image_type;

  friend class VideoFrame;
  friend class Cache;
  friend class ScriptEnvironment;

public:
  VideoFrameBuffer(int size): data(new BYTE[size]), data_size(data ? size : 0), sequence_number(0), refcount(0), timestamp(0), image_type(0) {};
  VideoFrameBuffer(): data(0), data_size(0), sequence_number(0), refcount (0), timestamp(0), image_type(0) {};

  virtual ~VideoFrameBuffer() {};

  const BYTE* GetReadPtr() const { return data; }
  BYTE* GetWritePtr() { ++sequence_number; return data; }
  int GetDataSize() { return data_size; }
  int GetSequenceNumber() { return sequence_number; }
  int GetRefcount() { return refcount; }

  guint64 GetTimestamp() { return timestamp; }
  void SetTimestamp(guint64 newts) { timestamp = newts; }

  int GetParity() { return image_type; }
  void SetParity(int newparity) { image_type = newparity; }
};


class IClip;
class PClip;
class PVideoFrame;
class IScriptEnvironment;
class AVSValue;


// VideoFrame holds a "window" into a VideoFrameBuffer.  Operator new
// is overloaded to recycle class instances.

class VideoFrame {
  virtual void AddRef() {};
  virtual void Release() {};

  friend class PVideoFrame;
  friend class ScriptEnvironment;
  friend class Cache;
protected:
  const int offset, pitch, row_size, height, offsetU, offsetV, pitchUV;  // U&V offsets are from top of picture.

  VideoFrameBuffer* const vfb;
  int refcount;

  VideoFrame(VideoFrameBuffer* _vfb, int _offset, int _pitch, int _row_size, int _height):
    offset(_offset), pitch(_pitch), row_size(_row_size), height(_height), offsetU(_offset), offsetV(_offset), pitchUV(0), vfb(_vfb), refcount(0)
  {}
  VideoFrame(VideoFrameBuffer* _vfb, int _offset, int _pitch, int _row_size, int _height, int _offsetU, int _offsetV, int _pitchUV):
    offset(_offset), pitch(_pitch), row_size(_row_size), height(_height), offsetU(_offsetU), offsetV(_offsetV), pitchUV(_pitchUV), vfb(_vfb), refcount(0)
  {}

// TESTME: OFFSET U/V may be switched to what could be expected from AVI standard!
public:
  int GetPitch() const { return pitch; }
  int GetPitch(int plane) const { switch (plane) {case PLANAR_U: case PLANAR_V: return pitchUV;} return pitch; }
  int GetRowSize() const { return row_size; }
  int GetRowSize(int plane) const {
    switch (plane) {
    case PLANAR_U: case PLANAR_V: if (pitchUV) return row_size>>1; else return 0;
    case PLANAR_U_ALIGNED: case PLANAR_V_ALIGNED:
      if (pitchUV) {
        int r = ((row_size+FRAME_ALIGN-1)&(~(FRAME_ALIGN-1)) )>>1; // Aligned rowsize
        if (r<=pitchUV)
          return r;
        return row_size>>1;
      } else return 0;
    case PLANAR_Y_ALIGNED:
      int r = (row_size+FRAME_ALIGN-1)&(~(FRAME_ALIGN-1)); // Aligned rowsize
      if (r<=pitch)
        return r;
      return row_size;
    }
    return row_size; }
  int GetHeight() const { return height; }
  int GetHeight(int plane) const {  switch (plane) {case PLANAR_U: case PLANAR_V: if (pitchUV) return height>>1; return 0;} return height; }

  // generally you shouldn't use these three
  VideoFrameBuffer* GetFrameBuffer() const { return vfb; }
  int GetOffset() const { return offset; }
  int GetOffset(int plane) const { switch (plane) {case PLANAR_U: return offsetU;case PLANAR_V: return offsetV;default: return offset;}; }

  // in plugins use env->SubFrame()
  virtual VideoFrame* Subframe(int rel_offset, int new_pitch, int new_row_size, int new_height) const { return 0; };
  virtual VideoFrame* Subframe(int rel_offset, int new_pitch, int new_row_size, int new_height, int rel_offsetU, int rel_offsetV, int pitchUV) const { return 0; };


  const BYTE* GetReadPtr() const { return vfb->GetReadPtr() + offset; }
  const BYTE* GetReadPtr(int plane) const { return vfb->GetReadPtr() + GetOffset(plane); }

  bool IsWritable()
  {
    return (refcount == 1 && vfb->refcount == 1);
  }


  BYTE* GetWritePtr()
  {
    if (vfb->GetRefcount()>1) {
      //_ASSERT(FALSE);
      //throw AvisynthError("Internal Error - refcount was more than one!");
    }
    return IsWritable() ? (vfb->GetWritePtr() + offset) : 0;
  }

  BYTE* GetWritePtr(int plane)
  {
    if (plane == PLANAR_Y) {
      if (vfb->GetRefcount()>1) {
        //_ASSERT(FALSE);
        //throw AvisynthError("Internal Error - refcount was more than one!");
      }
      return IsWritable() ? vfb->GetWritePtr() + GetOffset(plane) : 0;
    }
    return vfb->data + GetOffset(plane);
  }

  guint64 GetTimestamp() { return vfb->GetTimestamp(); }
  void SetTimestamp(guint64 newts) { vfb->SetTimestamp(newts); }

  int GetParity() { return vfb->GetParity(); }
  void SetParity(int newparity) { vfb->SetParity(newparity); }

  virtual ~VideoFrame() {};
};

enum {
  CACHE_NOTHING=0,
  CACHE_RANGE=1,
  CACHE_ALL=2,
  CACHE_AUDIO=3,
  CACHE_AUDIO_NONE=4,
  CACHE_AUDIO_AUTO=5
 };

// Base class for all filters.
class IClip {
  friend class PClip;
  friend class AVSValue;

protected:
  int refcnt;

  virtual void AddRef() {};
  virtual void Release() {};
public:
  IClip() : refcnt(0) {};

  virtual int __stdcall GetVersion() { return AVISYNTH_INTERFACE_VERSION; }

  /* Overloaded by the filter */
  virtual PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) = 0;

  /* Overloaded by the filter, or gets the value from upstream */
  virtual bool __stdcall GetParity(int n) = 0;  // return field parity if field_based, else parity of first field in frame

  /* Overloaded by the filter */
  virtual void __stdcall GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env) = 0;  // start and count are in samples

  /* At the moment this function does nothing */
  virtual void __stdcall SetCacheHints(int cachehints,int frame_range) = 0 ;

  virtual const VideoInfo& __stdcall GetVideoInfo() = 0;

  virtual __stdcall ~IClip() {};
};


// smart pointer to IClip
class PClip {

  IClip* p;

  IClip* GetPointerWithAddRef() const { if (p) p->AddRef(); return p; }
  friend class AVSValue;
  friend class VideoFrame;

  void Init(IClip* x) {
    if (x) x->AddRef();
    p=x;
  }
  void Set(IClip* x) {
    if (x) x->AddRef();
    if (p) p->Release();
    p=x;
  }

public:
  PClip() { p = 0; }
  PClip(const PClip& x) { Init(x.p); }
  PClip(IClip* x) { Init(x); }
  void operator=(IClip* x) { Set(x); }
  void operator=(const PClip& x) { Set(x.p); }

  IClip* operator->() const { return p; }

  // useful in conditional expressions
  operator void*() const { return p; }
  bool operator!() const { return !p; }

  ~PClip() { if (p) p->Release(); }
};


// smart pointer to VideoFrame
class PVideoFrame {

  VideoFrame* p;

  void Init(VideoFrame* x) {
    if (x) x->AddRef();
    p=x;
  }
  void Set(VideoFrame* x) {
    if (x) x->AddRef();
    if (p) p->Release();
    p=x;
  }

public:
  PVideoFrame() { p = 0; }
  PVideoFrame(const PVideoFrame& x) { Init(x.p); }
  PVideoFrame(VideoFrame* x) { Init(x); }
  void operator=(VideoFrame* x) { Set(x); }
  void operator=(const PVideoFrame& x) { Set(x.p); }

  VideoFrame* operator->() const { return p; }

  // for conditional expressions
  operator void*() const { return p; }
  bool operator!() const { return !p; }

  ~PVideoFrame() { if (p) p->Release();}
};

class AVSValue {
public:

  AVSValue() { type = 'v'; }
  AVSValue(IClip* c) { type = 'c'; clip = c; if (c) c->AddRef(); }
  AVSValue(const PClip& c) { type = 'c'; clip = c.GetPointerWithAddRef(); }
  AVSValue(bool b) { type = 'b'; boolean = b; }
  AVSValue(int i) { type = 'i'; integer = i; }
  AVSValue(__int64 l) { type = 'l'; longlong = l; }
  AVSValue(float f) { type = 'f'; floating_pt = f; }
  AVSValue(double f) { type = 'f'; floating_pt = float(f); }
  AVSValue(const char* s) { type = 's'; string = s; }
  AVSValue(const AVSValue* a, int size) { type = 'a'; array = a; array_size = size; }
  AVSValue(const AVSValue& v) { Assign(&v, true); }

  ~AVSValue() { if (IsClip() && clip) clip->Release(); }
  AVSValue& operator=(const AVSValue& v) { Assign(&v, false); return *this; }

  // Note that we transparently allow 'int' to be treated as 'float'.
  // There are no int<->bool conversions, though.

  bool Defined() const { return type != 'v'; }
  bool IsClip() const { return type == 'c'; }
  bool IsBool() const { return type == 'b'; }
  bool IsInt() const { return type == 'i'; }
  bool IsLong() const { return (type == 'l'|| type == 'i'); }
  bool IsFloat() const { return type == 'f' || type == 'i'; }
  bool IsString() const { return type == 's'; }
  bool IsArray() const { return type == 'a'; }

  PClip AsClip() const { _ASSERTE(IsClip()); return IsClip()?clip:0; }
  bool AsBool() const { _ASSERTE(IsBool()); return boolean; }
  int AsInt() const { _ASSERTE(IsInt()); return integer; }
  int AsLong() const { _ASSERTE(IsLong()); return longlong; }
  const char* AsString() const { _ASSERTE(IsString()); return IsString()?string:0; }
  double AsFloat() const { _ASSERTE(IsFloat()); return IsInt()?integer:floating_pt; }

  bool AsBool(bool def) const { _ASSERTE(IsBool()||!Defined()); return IsBool() ? boolean : def; }
  int AsInt(int def) const { _ASSERTE(IsInt()||!Defined()); return IsInt() ? integer : def; }
  double AsFloat(double def) const { _ASSERTE(IsFloat()||!Defined()); return IsInt() ? integer : type=='f' ? floating_pt : def; }
  const char* AsString(const char* def) const { _ASSERTE(IsString()||!Defined()); return IsString() ? string : def; }

  int ArraySize() const { _ASSERTE(IsArray()); return IsArray()?array_size:1; }

  const AVSValue& operator[](int index) const {
    _ASSERTE(IsArray() && index>=0 && index<array_size);
    return (IsArray() && index>=0 && index<array_size) ? array[index] : *this;
  }

private:

  short type;  // 'a'rray, 'c'lip, 'b'ool, 'i'nt, 'f'loat, 's'tring, 'v'oid, or 'l'ong
  short array_size;
  union {
    IClip* clip;
    gboolean boolean;
    int integer;
    float floating_pt;
    const char* string;
    const AVSValue* array;
    __int64 longlong;
  };

  void Assign(const AVSValue* src, bool init) {
    if (src->IsClip() && src->clip)
      src->clip->AddRef();
    if (!init && IsClip() && clip)
      clip->Release();
    // make sure this copies the whole struct!
    /* AviSynth stuff is WEIRD
    ((__int32*)this)[0] = ((__int32*)src)[0];
    ((__int32*)this)[1] = ((__int32*)src)[1];
    */
    memcpy (this, src, sizeof (AVSValue));
  }
};


// instantiable null filter
class GenericVideoFilter : public IClip {
protected:
  PClip child;
  VideoInfo vi;
public:
  __stdcall ~GenericVideoFilter() {};
  GenericVideoFilter(PClip _child) : child(_child) { vi = child->GetVideoInfo(); }
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) { return child->GetFrame(n, env); }
  void __stdcall GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env) { child->GetAudio(buf, start, count, env); }
  const VideoInfo& __stdcall GetVideoInfo() { return vi; }
  bool __stdcall GetParity(int n) { return child->GetParity(n); }
  void __stdcall SetCacheHints(int cachehints,int frame_range) { } ;  // We do not pass cache requests upwards, only to the next filter.
};



/* Helper classes useful to plugin authors */

class AlignPlanar : public GenericVideoFilter
{
public:
  virtual __stdcall ~AlignPlanar() {};
  AlignPlanar(PClip _clip);
  static PClip Create(PClip clip);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
};



class FillBorder : public GenericVideoFilter
{
public:
  __stdcall ~FillBorder() {};
  FillBorder(PClip _clip);
  static PClip Create(PClip clip);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
};

// For GetCPUFlags.  These are backwards-compatible with those in VirtualDub.
enum {
                    /* slowest CPU to support extension */
  CPUF_FORCE        =  0x01,   //  N/A
  CPUF_FPU          =  0x02,   //  386/486DX
  CPUF_MMX          =  0x04,   //  P55C, K6, PII
  CPUF_INTEGER_SSE  =  0x08,   //  PIII, Athlon
  CPUF_SSE          =  0x10,   //  PIII, Athlon XP/MP
  CPUF_SSE2         =  0x20,   //  PIV, Hammer
  CPUF_3DNOW        =  0x40,   //  K6-2
  CPUF_3DNOW_EXT    =  0x80,   //  Athlon
  CPUF_X86_64       =  0xA0,   //  Hammer (note: equiv. to 3DNow + SSE2, which
                               //          only Hammer will have anyway)
  CPUF_SSE3         = 0x100,   //  PIV+, Hammer
};

class IScriptEnvironment {
public:
  virtual __stdcall ~IScriptEnvironment() {}

  virtual /*static*/ long __stdcall GetCPUFlags() = 0;

  virtual char* __stdcall SaveString(const char* s, int length = -1) = 0;
  virtual char* __stdcall Sprintf(const char* fmt, ...) = 0;
  virtual char* __stdcall VSprintf(const char* fmt, va_list val) = 0;

  DECLSPEC_NORETURN virtual void __stdcall ThrowError(const char* fmt, ...) = 0;

  class NotFound /*exception*/ {};  // thrown by Invoke and GetVar

  typedef AVSValue (__cdecl *ApplyFunc)(AVSValue args, void* user_data, IScriptEnvironment* env);


  virtual void __stdcall AddFunction(const char* name, const char* paramstr,
/* GstAVSynth-specific:
 * srccapstr - describes what kind of input the filter supports
 * sinkcapstr - describes what kind of output the filter may output
 * default is "video/x-raw-yuv; video/x-raw-rgb" - accept practially any video.
 * See GStreamer documentation for further explanation
 */
      const char* srccapstr, const char* sinkcapstr,
      ApplyFunc apply, void* user_data=0) = 0;
  virtual bool __stdcall FunctionExists(const char* name) = 0;
  virtual AVSValue __stdcall Invoke(const char* name, const AVSValue args, const char** arg_names=0) = 0;

  virtual AVSValue __stdcall GetVar(const char* name) = 0;
  virtual bool __stdcall SetVar(const char* name, const AVSValue& val) = 0;
  virtual bool __stdcall SetGlobalVar(const char* name, const AVSValue& val) = 0;

  virtual void __stdcall PushContext(int level=0) = 0;
  virtual void __stdcall PopContext() = 0;

  // align should be 4 or 8
  virtual PVideoFrame __stdcall NewVideoFrame(const VideoInfo& vi, int align=FRAME_ALIGN) = 0;

  virtual bool __stdcall MakeWritable(PVideoFrame* pvf) = 0;

  virtual /*static*/ void __stdcall BitBlt(BYTE* dstp, int dst_pitch, const BYTE* srcp, int src_pitch, int row_size, int height) = 0;

  typedef void (__cdecl *ShutdownFunc)(void* user_data, IScriptEnvironment* env);
  virtual void __stdcall AtExit(ShutdownFunc function, void* user_data) = 0;

  virtual void __stdcall CheckVersion(int version = AVISYNTH_INTERFACE_VERSION) = 0;

  virtual PVideoFrame __stdcall Subframe(PVideoFrame src, int rel_offset, int new_pitch, int new_row_size, int new_height) = 0;

  virtual int __stdcall SetMemoryMax(int mem) = 0;

  virtual int __stdcall SetWorkingDir(const char * newdir) = 0;

  virtual void* __stdcall ManageCache(int key, void* data) = 0;

  enum PlanarChromaAlignmentMode {
			PlanarChromaAlignmentOff,
			PlanarChromaAlignmentOn,
			PlanarChromaAlignmentTest };

  virtual bool __stdcall PlanarChromaAlignment(PlanarChromaAlignmentMode key) = 0;

  virtual PVideoFrame __stdcall SubframePlanar(PVideoFrame src, int rel_offset, int new_pitch, int new_row_size, int new_height, int rel_offsetU, int rel_offsetV, int new_pitchUV) = 0;
};

#pragma pack(pop)

#endif //__GSTAVSYNTH_SDK_CPP_H__
