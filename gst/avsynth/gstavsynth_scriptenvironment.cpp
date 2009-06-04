/* GStreamer
 * Copyright (C) <2009> LRN <lrn1986 _at_ gmail _dot_ com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <string.h>

#include <gst/gst.h>

#include "gstavsynth.h"
#include "gstavsynth_videobuffer.h"
#include "gstavsynth_videofilter.h"
#include "gstavsynth_loader.h"
#include "gstavsynth_scriptenvironment.h"

class ImplVideoFrameBuffer;
class ImplVideoFrame;
class ImplClip;

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

};

class ImplVideoFrameBuffer: private VideoFrameBuffer
{
  friend class ImplVideoFrame;
public:
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

ImplVideoFrameBuffer::ImplVideoFrameBuffer(): VideoFrameBuffer()
{
}

ImplVideoFrameBuffer::ImplVideoFrameBuffer(int size): VideoFrameBuffer(size)
{
  g_atomic_int_inc (&sequence_number);
}

ImplVideoFrameBuffer::~ImplVideoFrameBuffer() {
//  _ASSERTE(refcount == 0);
  g_atomic_int_inc (&sequence_number); // HACK : Notify any children with a pointer, this buffer has changed!!!
  if (data) delete[] data;
  data = 0; // and mark it invalid!!
  data_size = 0;   // and don't forget to set the size to 0 as well!
}

class LinkedVideoFrameBuffer : public VideoFrameBuffer {
public:
  enum {ident = 0x00AA5500};
  LinkedVideoFrameBuffer *prev, *next;
  bool returned;
  const int signature; // Used by ManageCache to ensure that the VideoFrameBuffer 
                       // it casts is really a LinkedVideoFrameBuffer
  LinkedVideoFrameBuffer(int size) : VideoFrameBuffer(size), returned(true), signature(ident) { next=prev=this; }
  LinkedVideoFrameBuffer() : returned(true), signature(ident) { next=prev=this; }
};


ImplVideoFrame::ImplVideoFrame(VideoFrameBuffer* _vfb, int _offset, int _pitch, int _row_size, int _height):
  VideoFrame (_vfb, _offset, _pitch, _row_size, _height)
{
  g_atomic_int_inc (&dynamic_cast<ImplVideoFrameBuffer*>(vfb)->refcount);
}

ImplVideoFrame::ImplVideoFrame(VideoFrameBuffer* _vfb, int _offset, int _pitch, int _row_size, int _height,
                       int _offsetU, int _offsetV, int _pitchUV):
  VideoFrame (_vfb, _offset, _pitch, _row_size, _height, _offsetU, _offsetV, _pitchUV)
{
  g_atomic_int_inc (&dynamic_cast<ImplVideoFrameBuffer*>(vfb)->refcount);
}

void
ImplVideoFrame::AddRef ()
{
  g_atomic_int_inc (&refcount);
}

void
ImplVideoFrame::Release()
{
  if (refcount==1) g_atomic_int_dec_and_test (&dynamic_cast<ImplVideoFrameBuffer*>(vfb)->refcount);
  g_atomic_int_dec_and_test (&refcount);
}

ImplVideoFrame::~ImplVideoFrame ()
{
  g_atomic_int_dec_and_test (&dynamic_cast<ImplVideoFrameBuffer*>(vfb)->refcount);
}


VideoFrame*
ImplVideoFrame::Subframe (int rel_offset, int new_pitch, int new_row_size, int new_height) const {
  return new ImplVideoFrame(vfb, offset+rel_offset, new_pitch, new_row_size, new_height);
}


VideoFrame*
ImplVideoFrame::Subframe (int rel_offset, int new_pitch, int new_row_size, int new_height,
                                 int rel_offsetU, int rel_offsetV, int new_pitchUV) const {
  return new ImplVideoFrame(vfb, offset+rel_offset, new_pitch, new_row_size, new_height,
                        rel_offsetU+offsetU, rel_offsetV+offsetV, new_pitchUV);
}

void
ImplClip::AddRef()
{
  g_atomic_int_inc (&refcnt);
}

void
ImplClip::Release()
{
  g_atomic_int_dec_and_test (&refcnt);
  if (!refcnt) delete this;
}

/* GPL
AlignPlanar must be applied after a filter that forces an alignment (that is less than default alignment), that filter applies it by itself,
That is why AlignPlanar API is public.
*/
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

ScriptEnvironment::ScriptEnvironment (GstAVSynthVideoFilter *parent)
{
  string_dump = g_string_chunk_new (4096);
  parent_object = (GstAVSynthVideoFilter *) g_object_ref (G_OBJECT (parent));
  parent_class = (GstAVSynthVideoFilterClass *) G_OBJECT_GET_CLASS (parent);
}

ScriptEnvironment::~ScriptEnvironment ()
{
  g_string_chunk_free (string_dump);
  g_object_unref (G_OBJECT (parent_object));
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
  /* FIXME: add cpuflags detection */
  return 0;
}

char*
ScriptEnvironment::SaveString (const char* s, int length)
{
  if (G_UNLIKELY (!s))
    return NULL;
  if (G_UNLIKELY (length > 0))
    return g_string_chunk_insert_len (string_dump, s, (gssize) length);
  return g_string_chunk_insert (string_dump, s);
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
ScriptEnvironment::VSprintf (const char* fmt, void* val)
{
  char *result = NULL, *tmp = NULL;
  /* This kinda defeats the purpose of the string chunks... */
  tmp = g_strdup_vprintf (fmt, (char *) val);
  result = ScriptEnvironment::SaveString (tmp);
  g_free (tmp);
  return result;
}

void
ScriptEnvironment::ThrowError (const char* fmt, ...)
{
  gchar *buf = NULL;
  va_list val;

  va_start(val, fmt);
  buf = g_strdup_vprintf (fmt, val);
  va_end(val);

  throw AvisynthError(ScriptEnvironment::SaveString (buf));
}

void
ScriptEnvironment::AddFunction(const char* name, const char* paramstr, const char* srccapstr, const char* sinkcapstr, ApplyFunc apply_func, void* user_data)
{
  /* One plugin may add more than one function, but this
   * class wraps only one of them.
   */
  if (g_utf8_collate (name, parent_class->name))
    return;

  apply = apply_func;
  userdata = user_data;
}

bool
ScriptEnvironment::FunctionExists(const char* name)
{
  if (g_type_from_name ((gchar *) name))
    return true;
  else
    return false;
}

AVSValue
ScriptEnvironment::Invoke(const char* name, const AVSValue args, const char** arg_names)
{
  /* FIXME: thow exception? Since we won't implement this function */
  return NULL;
}

AVSValue
ScriptEnvironment::GetVar(const char* name)
{
  /* TODO: implement simple variable table? */
  return NULL;
}

bool
ScriptEnvironment::SetVar(const char* name, const AVSValue& val)
{
  /* TODO: implement simple variable table? */
  return false;
}

bool
ScriptEnvironment::SetGlobalVar(const char* name, const AVSValue& val)
{
  /* TODO: implement simple variable table? */
  return false;
}

void
ScriptEnvironment::PushContext(int level)
{
  /* FIXME: thow exception? Since we won't implement this function */
}

void
ScriptEnvironment::PopContext()
{
  /* FIXME: thow exception? Since we won't implement this function */
}

void
ScriptEnvironment::PopContextGlobal()
{
  /* FIXME: thow exception? Since we won't implement this function */
  return;
}

/* Both NewVideoFrame methods are GPL */

PVideoFrame __stdcall
ScriptEnvironment::NewVideoFrame(const VideoInfo& vi, int align) {
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
}

PVideoFrame
ScriptEnvironment::NewVideoFrame (int row_size, int height, int align)
{
  VideoFrameBuffer* vfb;
  const int pitch = (row_size+align-1) / align * align;
  const int size = pitch * height;
  const int _align = (align < FRAME_ALIGN) ? FRAME_ALIGN : align;
  vfb = GetFrameBuffer(size+(_align*4));
  if (!vfb)
    ThrowError("NewVideoFrame: Returned 0 image pointer!");
  const int offset = (-int(vfb->GetWritePtr())) & (FRAME_ALIGN-1);  // align first line offset  (alignment is free here!)
  return new ImplVideoFrame(vfb, offset, pitch, row_size, height);
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
  else if (PlanarChromaAlignmentState) {
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
  const int offset = (-int(vfb->GetWritePtr())) & (FRAME_ALIGN-1);  // align first line offset

  if (U_first) {
    Uoffset = offset + pitch * height;
    Voffset = offset + pitch * height + UVpitch * (height>>1);
  } else {
    Voffset = offset + pitch * height;
    Uoffset = offset + pitch * height + UVpitch * (height>>1);
  }
  return new VideoFrame(vfb, offset, pitch, width, height, Uoffset, Voffset, UVpitch);
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


void BitBlt(BYTE* dstp, int dst_pitch, const BYTE* srcp, int src_pitch, int row_size, int height) {
  if ((!height) || (!row_size)) return;
  /* FIXME: Find faster LGPL implementation */
  if (height == 1 || (dst_pitch == src_pitch && src_pitch == row_size)){
    memcpy(dstp, srcp, row_size * height);
  } else {
    int y;
    for (y = height; y > 0; --y)
    {
      memcpy(dstp, srcp, row_size);
      dstp += dst_pitch;
      srcp += src_pitch;
    }
  }
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
  /* FIXME: implement */
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
}

int
ScriptEnvironment::SetWorkingDir(const char * newdir)
{
  /* TODO: implement */
}

void*
ScriptEnvironment::ManageCache(int key, void* data)
{
  /* Our cache implementation (do we have one?) is completely different at the moment */
  return 0;
}

bool
ScriptEnvironment::PlanarChromaAlignment(IScriptEnvironment::PlanarChromaAlignmentMode key)
{
  bool oldPlanarChromaAlignmentState = PlanarChromaAlignmentState;

  switch (key)
  {
    case IScriptEnvironment::PlanarChromaAlignmentOff:
    {
	    PlanarChromaAlignmentState = false;
      break;
    }
    case IScriptEnvironment::PlanarChromaAlignmentOn:
    {
	    PlanarChromaAlignmentState = true;
      break;
    }
    default:
      break;
  }
  return oldPlanarChromaAlignmentState;
}

VideoFrameBuffer* ScriptEnvironment::NewFrameBuffer(int size) {
//  memory_used += size;
//  _RPT1(0, "Frame buffer memory used: %I64d\n", memory_used);
  return new LinkedVideoFrameBuffer(size);
}


VideoFrameBuffer* ScriptEnvironment::GetFrameBuffer2(int size) {
  LinkedVideoFrameBuffer *i, *j;

  // Before we allocate a new framebuffer, check our memory usage, and if we
  // are 12.5% or more above allowed usage discard some unreferenced frames.
  /*
  if (memory_used >=  memory_max + max(size, (memory_max >> 3)) ) {
    ++g_Mem_stats.CleanUps;
    int freed = 0;
    int freed_count = 0;
    // Deallocate enough unused frames.
    for (i = video_frame_buffers.prev; i != &video_frame_buffers; i = i->prev) {
      if (i->GetRefcount() == 0) {
        if (i->next != i->prev) {
          // Adjust unpromoted sublist if required
          if (unpromotedvfbs == i) unpromotedvfbs = i->next;
          // Store size.
          freed += i->data_size;
          freed_count++;
          // Delete data;
          i->~LinkedVideoFrameBuffer();  // Can't delete me because caches have pointers to me
          // Link onto tail of lost_video_frame_buffers chain.
          j = i;
          i = i -> next; // step back one
          Relink(lost_video_frame_buffers.prev, j, &lost_video_frame_buffers);
          if ((memory_used+size - freed) < memory_max)
            break; // Stop, we are below 100% utilization
        }
        else break;
      }
    }
    _RPT2(0,"Freed %d frames, consisting of %d bytes.\n",freed_count, freed);
    memory_used -= freed;
	g_Mem_stats.Losses += freed_count;
  } 

  // Plan A: When we are below our memory usage :-
  if (memory_used + size < memory_max) {
    //   Part 1: look for a returned free buffer of the same size and reuse it
    for (i = video_frame_buffers.prev; i != &video_frame_buffers; i = i->prev) {
      if (i->returned && (i->GetRefcount() == 0) && (i->GetDataSize() == size)) {
        ++g_Mem_stats.PlanA1;
        return i;
      }
    }
    //   Part 2: else just allocate a new buffer
    ++g_Mem_stats.PlanA2;
    return NewFrameBuffer(size);
  }

  // To avoid Plan D we prod the caches to surrender any VFB's
  // they maybe guarding. We start gently and ask for just the
  // most recently locked VFB from previous cycles, then we ask
  // for the most recently locked VFB, then we ask for all the
  // locked VFB's. Next we start on the CACHE_RANGE protected
  // VFB's, as an offset we promote these.
  // Plan C is not invoked until the Cache's have been poked once.
  for (int c=Cache::PC_Nil; c <= Cache::PC_UnProtectAll; c++) {
    CacheHead->PokeCache(c, size, this);

    // Plan B: Steal the oldest existing free buffer of the same size
    j = 0;
    for (i = video_frame_buffers.prev; i != &video_frame_buffers; i = i->prev) {
      if (i->GetRefcount() == 0) {
        if (i->GetDataSize() == size) {
          ++g_Mem_stats.PlanB;
          InterlockedIncrement((long*)&i->sequence_number);  // Signal to the cache that the vfb has been stolen
          return i;
        }
        if (i->GetDataSize() > size) {
          // Remember the smallest VFB that is bigger than our size
          if ((j == 0) || (i->GetDataSize() < j->GetDataSize())) j = i;
        }
      }
    }

    // Plan C: Steal the oldest, smallest free buffer that is greater in size
    if (j && c > Cache::PC_Nil) {
      ++g_Mem_stats.PlanC;
      InterlockedIncrement((long*)&j->sequence_number);  // Signal to the cache that the vfb has been stolen
      return j;
    }
  }
  */
  // Plan D: Allocate a new buffer, regardless of current memory usage
  //++g_Mem_stats.PlanD;
  return NewFrameBuffer(size);
}

VideoFrameBuffer* ScriptEnvironment::GetFrameBuffer(int size) {
  LinkedVideoFrameBuffer* result = (LinkedVideoFrameBuffer *) GetFrameBuffer2(size);

  if (!result || !result->data) {
    // Damn! we got a NULL from malloc
    /*
    _RPT3(0, "GetFrameBuffer failure, size=%d, memory_max=%I64d, memory_used=%I64d", size, memory_max, memory_used);

    // Put that VFB on the lost souls chain
    if (result) Relink(lost_video_frame_buffers.prev, result, &lost_video_frame_buffers);

	__int64 save_max = memory_max;

    // Set memory_max to 12.5% below memory_used 
    memory_max = max(4*1024*1024, memory_used - max(size, (memory_used/9)));

    // Retry the request
    result = GetFrameBuffer2(size);

	memory_max = save_max;

    if (!result || !result->data) {
      // Damn!! Damn!! we are really screwed, winge!
      if (result) Relink(lost_video_frame_buffers.prev, result, &lost_video_frame_buffers);
    */
      ThrowError("GetFrameBuffer: Returned a VFB with a 0 data pointer!\n"
                 "I think we have run out of memory folks!");
    /*
    }
    */
  }

  // Link onto head of unpromoted video_frame_buffers chain.
  //Relink(unpromotedvfbs->prev, result, unpromotedvfbs);
  // Adjust unpromoted sublist
  //unpromotedvfbs = result;

  // Flag it as returned, i.e. currently not managed
  result->returned = true;
  return result;
}
