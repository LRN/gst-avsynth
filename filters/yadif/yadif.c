/*
	Yadif C-plugin for Avisynth 2.5 - Yet Another DeInterlacing Filter
	Copyright (C)2007 Alexander G. Balakhnin aka Fizick  http://avisynth.org.ru
    Port of YADIF filter from MPlayer
	Copyright (C) 2006 Michael Niedermayer <michaelni@gmx.at>

    This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    Avisynth_C plugin
	Assembler optimized for GNU C compiler

*/
//following 2 includes needed
//#include <windows.h>
#include <avsynth/gstavsynth_sdk_c.h>
#include <stdlib.h>
#include <memory.h>

#ifndef MIN
#define MIN(a,b) ((a) > (b) ? (b) : (a))
#endif
#ifndef MAX
#define MAX(a,b) ((a) < (b) ? (b) : (a))
#endif
#ifndef ABS
#define ABS(a) ((a) > 0 ? (a) : (-(a)))
#endif

#define MIN3(a,b,c) MIN(MIN(a,b),c)
#define MAX3(a,b,c) MAX(MAX(a,b),c)

#define uint8_t unsigned char

typedef struct yadif_filter  {
	AVS_FilterInfo *fi;
	int mode;
	int order;
	int yheight;
	int ypitch;
	int uvpitch;
	int ywidth;
	int uvwidth;
	unsigned char *ysrc;
	unsigned char *usrc;
	unsigned char *vsrc;
	unsigned char *yprev;
	unsigned char *uprev;
	unsigned char *vprev;
	unsigned char *ynext;
	unsigned char *unext;
	unsigned char *vnext;
	unsigned char *ydest;
	unsigned char *udest;
	unsigned char *vdest;
} yadif_filter;


static void (*filter_line)(int mode, uint8_t *dst, const uint8_t *prev, const uint8_t *cur, const uint8_t *next, int w, int refs, int parity);

#ifdef __GNUC__
#define uint64_t unsigned __int64
#define LOAD4(mem,dst) \
            "movd      "mem", "#dst" \n\t"\
            "punpcklbw %%mm7, "#dst" \n\t"

#define PABS(tmp,dst) \
            "pxor     "#tmp", "#tmp" \n\t"\
            "psubw    "#dst", "#tmp" \n\t"\
            "pmaxsw   "#tmp", "#dst" \n\t"

#define CHECK(pj,mj) \
            "movq "#pj"(%[cur],%[mrefs]), %%mm2 \n\t" /* cur[x-refs-1+j] */\
            "movq "#mj"(%[cur],%[prefs]), %%mm3 \n\t" /* cur[x+refs-1-j] */\
            "movq      %%mm2, %%mm4 \n\t"\
            "movq      %%mm2, %%mm5 \n\t"\
            "pxor      %%mm3, %%mm4 \n\t"\
            "pavgb     %%mm3, %%mm5 \n\t"\
            "pand     %[pb1], %%mm4 \n\t"\
            "psubusb   %%mm4, %%mm5 \n\t"\
            "psrlq     $8,    %%mm5 \n\t"\
            "punpcklbw %%mm7, %%mm5 \n\t" /* (cur[x-refs+j] + cur[x+refs-j])>>1 */\
            "movq      %%mm2, %%mm4 \n\t"\
            "psubusb   %%mm3, %%mm2 \n\t"\
            "psubusb   %%mm4, %%mm3 \n\t"\
            "pmaxub    %%mm3, %%mm2 \n\t"\
            "movq      %%mm2, %%mm3 \n\t"\
            "movq      %%mm2, %%mm4 \n\t" /* ABS(cur[x-refs-1+j] - cur[x+refs-1-j]) */\
            "psrlq      $8,   %%mm3 \n\t" /* ABS(cur[x-refs  +j] - cur[x+refs  -j]) */\
            "psrlq     $16,   %%mm4 \n\t" /* ABS(cur[x-refs+1+j] - cur[x+refs+1-j]) */\
            "punpcklbw %%mm7, %%mm2 \n\t"\
            "punpcklbw %%mm7, %%mm3 \n\t"\
            "punpcklbw %%mm7, %%mm4 \n\t"\
            "paddw     %%mm3, %%mm2 \n\t"\
            "paddw     %%mm4, %%mm2 \n\t" /* score */

#define CHECK1 \
            "movq      %%mm0, %%mm3 \n\t"\
            "pcmpgtw   %%mm2, %%mm3 \n\t" /* if(score < spatial_score) */\
            "pminsw    %%mm2, %%mm0 \n\t" /* spatial_score= score; */\
            "movq      %%mm3, %%mm6 \n\t"\
            "pand      %%mm3, %%mm5 \n\t"\
            "pandn     %%mm1, %%mm3 \n\t"\
            "por       %%mm5, %%mm3 \n\t"\
            "movq      %%mm3, %%mm1 \n\t" /* spatial_pred= (cur[x-refs+j] + cur[x+refs-j])>>1; */

#define CHECK2 /* pretend not to have checked dir=2 if dir=1 was bad.\
                  hurts both quality and speed, but matches the C version. */\
            "paddw    %[pw1], %%mm6 \n\t"\
            "psllw     $14,   %%mm6 \n\t"\
            "paddsw    %%mm6, %%mm2 \n\t"\
            "movq      %%mm0, %%mm3 \n\t"\
            "pcmpgtw   %%mm2, %%mm3 \n\t"\
            "pminsw    %%mm2, %%mm0 \n\t"\
            "pand      %%mm3, %%mm5 \n\t"\
            "pandn     %%mm1, %%mm3 \n\t"\
            "por       %%mm5, %%mm3 \n\t"\
            "movq      %%mm3, %%mm1 \n\t"

static void filter_line_mmx2(int mode, uint8_t *dst, const uint8_t *prev, const uint8_t *cur, const uint8_t *next, int w, int refs, int parity){
    static const uint64_t pw_1 = 0x0001000100010001ULL;
    static const uint64_t pb_1 = 0x0101010101010101ULL;
//    const int mode = p->mode;
    uint64_t tmp0, tmp1, tmp2, tmp3;
    int x;

#define FILTER\
    for(x=0; x<w; x+=4){\
        asm volatile(\
            "pxor      %%mm7, %%mm7 \n\t"\
            LOAD4("(%[cur],%[mrefs])", %%mm0) /* c = cur[x-refs] */\
            LOAD4("(%[cur],%[prefs])", %%mm1) /* e = cur[x+refs] */\
            LOAD4("(%["prev2"])", %%mm2) /* prev2[x] */\
            LOAD4("(%["next2"])", %%mm3) /* next2[x] */\
            "movq      %%mm3, %%mm4 \n\t"\
            "paddw     %%mm2, %%mm3 \n\t"\
            "psraw     $1,    %%mm3 \n\t" /* d = (prev2[x] + next2[x])>>1 */\
            "movq      %%mm0, %[tmp0] \n\t" /* c */\
            "movq      %%mm3, %[tmp1] \n\t" /* d */\
            "movq      %%mm1, %[tmp2] \n\t" /* e */\
            "psubw     %%mm4, %%mm2 \n\t"\
            PABS(      %%mm4, %%mm2) /* temporal_diff0 */\
            LOAD4("(%[prev],%[mrefs])", %%mm3) /* prev[x-refs] */\
            LOAD4("(%[prev],%[prefs])", %%mm4) /* prev[x+refs] */\
            "psubw     %%mm0, %%mm3 \n\t"\
            "psubw     %%mm1, %%mm4 \n\t"\
            PABS(      %%mm5, %%mm3)\
            PABS(      %%mm5, %%mm4)\
            "paddw     %%mm4, %%mm3 \n\t" /* temporal_diff1 */\
            "psrlw     $1,    %%mm2 \n\t"\
            "psrlw     $1,    %%mm3 \n\t"\
            "pmaxsw    %%mm3, %%mm2 \n\t"\
            LOAD4("(%[next],%[mrefs])", %%mm3) /* next[x-refs] */\
            LOAD4("(%[next],%[prefs])", %%mm4) /* next[x+refs] */\
            "psubw     %%mm0, %%mm3 \n\t"\
            "psubw     %%mm1, %%mm4 \n\t"\
            PABS(      %%mm5, %%mm3)\
            PABS(      %%mm5, %%mm4)\
            "paddw     %%mm4, %%mm3 \n\t" /* temporal_diff2 */\
            "psrlw     $1,    %%mm3 \n\t"\
            "pmaxsw    %%mm3, %%mm2 \n\t"\
            "movq      %%mm2, %[tmp3] \n\t" /* diff */\
\
            "paddw     %%mm0, %%mm1 \n\t"\
            "paddw     %%mm0, %%mm0 \n\t"\
            "psubw     %%mm1, %%mm0 \n\t"\
            "psrlw     $1,    %%mm1 \n\t" /* spatial_pred */\
            PABS(      %%mm2, %%mm0)      /* ABS(c-e) */\
\
            "movq -1(%[cur],%[mrefs]), %%mm2 \n\t" /* cur[x-refs-1] */\
            "movq -1(%[cur],%[prefs]), %%mm3 \n\t" /* cur[x+refs-1] */\
            "movq      %%mm2, %%mm4 \n\t"\
            "psubusb   %%mm3, %%mm2 \n\t"\
            "psubusb   %%mm4, %%mm3 \n\t"\
            "pmaxub    %%mm3, %%mm2 \n\t"\
            "pshufw $9,%%mm2, %%mm3 \n\t"\
            "punpcklbw %%mm7, %%mm2 \n\t" /* ABS(cur[x-refs-1] - cur[x+refs-1]) */\
            "punpcklbw %%mm7, %%mm3 \n\t" /* ABS(cur[x-refs+1] - cur[x+refs+1]) */\
            "paddw     %%mm2, %%mm0 \n\t"\
            "paddw     %%mm3, %%mm0 \n\t"\
            "psubw    %[pw1], %%mm0 \n\t" /* spatial_score */\
\
            CHECK(-2,0)\
            CHECK1\
            CHECK(-3,1)\
            CHECK2\
            CHECK(0,-2)\
            CHECK1\
            CHECK(1,-3)\
            CHECK2\
\
            /* if(p->mode<2) ... */\
            "movq    %[tmp3], %%mm6 \n\t" /* diff */\
            "cmp       $2, %[mode] \n\t"\
            "jge       1f \n\t"\
            LOAD4("(%["prev2"],%[mrefs],2)", %%mm2) /* prev2[x-2*refs] */\
            LOAD4("(%["next2"],%[mrefs],2)", %%mm4) /* next2[x-2*refs] */\
            LOAD4("(%["prev2"],%[prefs],2)", %%mm3) /* prev2[x+2*refs] */\
            LOAD4("(%["next2"],%[prefs],2)", %%mm5) /* next2[x+2*refs] */\
            "paddw     %%mm4, %%mm2 \n\t"\
            "paddw     %%mm5, %%mm3 \n\t"\
            "psrlw     $1,    %%mm2 \n\t" /* b */\
            "psrlw     $1,    %%mm3 \n\t" /* f */\
            "movq    %[tmp0], %%mm4 \n\t" /* c */\
            "movq    %[tmp1], %%mm5 \n\t" /* d */\
            "movq    %[tmp2], %%mm7 \n\t" /* e */\
            "psubw     %%mm4, %%mm2 \n\t" /* b-c */\
            "psubw     %%mm7, %%mm3 \n\t" /* f-e */\
            "movq      %%mm5, %%mm0 \n\t"\
            "psubw     %%mm4, %%mm5 \n\t" /* d-c */\
            "psubw     %%mm7, %%mm0 \n\t" /* d-e */\
            "movq      %%mm2, %%mm4 \n\t"\
            "pminsw    %%mm3, %%mm2 \n\t"\
            "pmaxsw    %%mm4, %%mm3 \n\t"\
            "pmaxsw    %%mm5, %%mm2 \n\t"\
            "pminsw    %%mm5, %%mm3 \n\t"\
            "pmaxsw    %%mm0, %%mm2 \n\t" /* max */\
            "pminsw    %%mm0, %%mm3 \n\t" /* min */\
            "pxor      %%mm4, %%mm4 \n\t"\
            "pmaxsw    %%mm3, %%mm6 \n\t"\
            "psubw     %%mm2, %%mm4 \n\t" /* -max */\
            "pmaxsw    %%mm4, %%mm6 \n\t" /* diff= MAX3(diff, min, -max); */\
            "1: \n\t"\
\
            "movq    %[tmp1], %%mm2 \n\t" /* d */\
            "movq      %%mm2, %%mm3 \n\t"\
            "psubw     %%mm6, %%mm2 \n\t" /* d-diff */\
            "paddw     %%mm6, %%mm3 \n\t" /* d+diff */\
            "pmaxsw    %%mm2, %%mm1 \n\t"\
            "pminsw    %%mm3, %%mm1 \n\t" /* d = clip(spatial_pred, d-diff, d+diff); */\
            "packuswb  %%mm1, %%mm1 \n\t"\
\
            :[tmp0]"=m"(tmp0),\
             [tmp1]"=m"(tmp1),\
             [tmp2]"=m"(tmp2),\
             [tmp3]"=m"(tmp3)\
            :[prev] "r"(prev),\
             [cur]  "r"(cur),\
             [next] "r"(next),\
             [prefs]"r"((long)refs),\
             [mrefs]"r"((long)-refs),\
             [pw1]  "m"(pw_1),\
             [pb1]  "m"(pb_1),\
             [mode] "g"(mode)\
        );\
        asm volatile("movd %%mm1, %0" :"=m"(*dst));\
        dst += 4;\
        prev+= 4;\
        cur += 4;\
        next+= 4;\
    }

    if(parity){
#define prev2 "prev"
#define next2 "cur"
        FILTER
#undef prev2
#undef next2
    }else{
#define prev2 "cur"
#define next2 "next"
        FILTER
#undef prev2
#undef next2
    }
}
#undef LOAD4
#undef PABS
#undef CHECK
#undef CHECK1
#undef CHECK2
#undef FILTER
#endif

static void filter_line_c(int mode, uint8_t *dst, const uint8_t *prev, const uint8_t *cur, const uint8_t *next, int w, int refs, int parity){
    int x;
    const uint8_t *prev2= parity ? prev : cur ;
    const uint8_t *next2= parity ? cur  : next;
    for(x=0; x<w; x++){
        int c= cur[-refs];
        int d= (prev2[0] + next2[0])>>1;
        int e= cur[+refs];
        int temporal_diff0= ABS(prev2[0] - next2[0]);
        int temporal_diff1=( ABS(prev[-refs] - c) + ABS(prev[+refs] - e) )>>1;
        int temporal_diff2=( ABS(next[-refs] - c) + ABS(next[+refs] - e) )>>1;
        int diff= MAX3(temporal_diff0>>1, temporal_diff1, temporal_diff2);
        int spatial_pred= (c+e)>>1;
        int spatial_score= ABS(cur[-refs-1] - cur[+refs-1]) + ABS(c-e)
                         + ABS(cur[-refs+1] - cur[+refs+1]) - 1;

#define CHECK(j)\
    {   int score= ABS(cur[-refs-1+ j] - cur[+refs-1- j])\
                 + ABS(cur[-refs  + j] - cur[+refs  - j])\
                 + ABS(cur[-refs+1+ j] - cur[+refs+1- j]);\
        if(score < spatial_score){\
            spatial_score= score;\
            spatial_pred= (cur[-refs  + j] + cur[+refs  - j])>>1;\

        CHECK(-1) CHECK(-2) }} }}
        CHECK( 1) CHECK( 2) }} }}

        if(mode<2){
            int b= (prev2[-2*refs] + next2[-2*refs])>>1;
            int f= (prev2[+2*refs] + next2[+2*refs])>>1;
#if 0
            int a= cur[-3*refs];
            int g= cur[+3*refs];
            int max= MAX3(d-e, d-c, MIN3(MAX(b-c,f-e),MAX(b-c,b-a),MAX(f-g,f-e)) );
            int min= MIN3(d-e, d-c, MAX3(MIN(b-c,f-e),MIN(b-c,b-a),MIN(f-g,f-e)) );
#else
            int max= MAX3(d-e, d-c, MIN(b-c, f-e));
            int min= MIN3(d-e, d-c, MAX(b-c, f-e));
#endif

            diff= MAX3(diff, min, -max);
        }

        if(spatial_pred > d + diff)
           spatial_pred = d + diff;
        else if(spatial_pred < d - diff)
           spatial_pred = d - diff;

        dst[0] = spatial_pred;

        dst++;
        cur++;
        prev++;
        next++;
        prev2++;
        next2++;
    }
}

static void interpolate(uint8_t *dst, const uint8_t *cur0,  const uint8_t *cur2, int w)
{
    int x;
    for (x=0; x<w; x++) {
        dst[x] = (cur0[x] + cur2[x] + 1)>>1; // simple average
    }
}


static void filter_plane(int mode, uint8_t *dst, int dst_stride, const uint8_t *prev0, const uint8_t *cur0, const uint8_t *next0, int refs, int w, int h, int parity, int tff, int mmx){

	int y;
	filter_line = filter_line_c;
#ifdef __GNUC__
	if (mmx)
		filter_line = filter_line_mmx2;
#endif
        y=0;
        if(((y ^ parity) & 1)){
            memcpy(dst, cur0 + refs, w);// duplicate 1
        }else{
            memcpy(dst, cur0, w);
        }
        y=1;
        if(((y ^ parity) & 1)){
            interpolate(dst + dst_stride, cur0, cur0 + refs*2, w);   // interpolate 0 and 2
        }else{
            memcpy(dst + dst_stride, cur0 + refs, w); // copy original
        }
        for(y=2; y<h-2; y++){
            if(((y ^ parity) & 1)){
                const uint8_t *prev= prev0 + y*refs;
                const uint8_t *cur = cur0 + y*refs;
                const uint8_t *next= next0 + y*refs;
                uint8_t *dst2= dst + y*dst_stride;
                filter_line(mode, dst2, prev, cur, next, w, refs, (parity ^ tff));
            }else{
                memcpy(dst + y*dst_stride, cur0 + y*refs, w); // copy original
            }
        }
        y=h-2;
        if(((y ^ parity) & 1)){
            interpolate(dst + (h-2)*dst_stride, cur0 + (h-3)*refs, cur0 + (h-1)*refs, w);   // interpolate h-3 and h-1
        }else{
            memcpy(dst + (h-2)*dst_stride, cur0 + (h-2)*refs, w); // copy original
        }
        y=h-1;
        if(((y ^ parity) & 1)){
            memcpy(dst + (h-1)*dst_stride, cur0 + (h-2)*refs, w); // duplicate h-2
        }else{
            memcpy(dst + (h-1)*dst_stride, cur0 + (h-1)*refs, w); // copy original
        }

#ifdef __GNUC__
	if (mmx)
		asm volatile("emms");
#endif
}


//----------------------------------------------------------------------------------------------

void YUY2ToPlanes(const unsigned char *pSrcYUY2, int nSrcPitchYUY2, int nWidth, int _nHeight,
							  const unsigned char * pSrc0, int _srcPitch,
							  const unsigned char * pSrcU0, const unsigned char * pSrcV0, int _srcPitchUV, int mmx)
{
	unsigned char * pSrc1 = (unsigned char *) pSrc0;
	unsigned char * pSrcU1 = (unsigned char *) pSrcU0;
	unsigned char * pSrcV1 = (unsigned char *) pSrcV0;

    int h,w;
//	const int awidth = MIN(nSrcPitchYUY2>>1, (nWidth+7) & -8);

//	if (!(awidth&7) && mmx) {  // Use MMX

//		convYUV422to422_mmx(pSrcYUY2, pSrc1, pSrcU1, pSrcV1, nSrcPitchYUY2, _srcPitch, _srcPitchUV,  awidth, _nHeight);
//	}
//	else
	{

	for (h=0; h<_nHeight; h++)
	{
		for (w=0; w<nWidth; w+=2)
		{
			int w2 = w+w;
			pSrc1[w] = pSrcYUY2[w2];
			pSrcU1[w>>1] = pSrcYUY2[w2+1];
			pSrc1[w+1] = pSrcYUY2[w2+2];
			pSrcV1[w>>1] = pSrcYUY2[w2+3];
		}
		pSrc1 += _srcPitch;
		pSrcU1 += _srcPitchUV;
		pSrcV1 += _srcPitchUV;
		pSrcYUY2 += nSrcPitchYUY2;
	}
	}
}

void YUY2FromPlanes(unsigned char *pSrcYUY2, int nSrcPitchYUY2, int nWidth, int nHeight,
							  unsigned char * pSrc1, int _srcPitch,
							  unsigned char * pSrcU1, unsigned char * pSrcV1, int _srcPitchUV, int mmx)
{
    int h,w;
//	const int awidth = MIN(_srcPitch, (nWidth+7) & -8);

//  if (!(awidth&7) && mmx) {  // Use MMX
//    conv422toYUV422_mmx(pSrc1, pSrcU1, pSrcV1, pSrcYUY2, _srcPitch, _srcPitchUV, nSrcPitchYUY2, awidth, nHeight);
//  }
//  else
  {

  for (h=0; h<nHeight; h++)
	{
		for (w=0; w<nWidth; w+=2)
		{
			int w2 = w+w;
			pSrcYUY2[w2] = pSrc1[w];
			pSrcYUY2[w2+1] = pSrcU1[w>>1];
			pSrcYUY2[w2+2] = pSrc1[w+1];
			pSrcYUY2[w2+3] = pSrcV1[w>>1];
		}
		pSrc1 += _srcPitch;
		pSrcU1 += _srcPitchUV;
		pSrcV1 += _srcPitchUV;
		pSrcYUY2 += nSrcPitchYUY2;
	}
  }
}

//----------------------------------------------------------------------------------

AVS_VideoFrame * AVSC_CC yadif_get_frame (AVS_FilterInfo * p, int ndst)
{
// This is the implementation of the GetFrame function.
// See the header definition for further info.

    int mode;
	int parity;
	int tff;
	int iplane;
	int cpu;
	int n;
	AVS_VideoFrame *src, *dst, * prev, *next;
	int h;

    yadif_filter * f = (yadif_filter *)p->user_data;
    mode = f->mode;

	if (mode & 1)
		n = (ndst>>1); // bob
	else
		n = ndst;

	src = p->env->avs_get_frame(p->child, n);
   // Request frame 'n' from the child (source) clip.

	if (n>0)
		prev = p->env->avs_get_frame(p->child, n-1); // get previous frame
	else if ((&p->vi)->num_frames > 1)
		prev = p->env->avs_get_frame(p->child, 1); // next frame
	else
		prev = p->env->avs_get_frame(p->child, 0); // cur 0 frame for one-frame clip

	if (n < (&p->vi)->num_frames - 1)
		next = p->env->avs_get_frame(p->child, n+1); // get next frame
	else if ((&p->vi)->num_frames > 1)
		next = p->env->avs_get_frame(p->child, (&p->vi)->num_frames - 2); // prev frame
	else
		next = p->env->avs_get_frame(p->child, 0); // cur 0 frame for one-frame clip

  	dst = p->env->avs_new_video_frame(p->env, &p->vi);
   // Construct a frame based on the information of the current frame
   // contained in the "vi" struct.

	if (f->order == -1)
//		tff = avs_is_tff(&p->vi) == 0 ? 0 : 1; // 0 or 1
		tff = p->env->avs_get_parity(p->child, n) ? 1 : 0; // 0 or 1
	else
		tff = f->order;

	parity = (mode & 1) ? (ndst & 1) ^ (1^tff) : (tff ^ 1);  // 0 or 1

	cpu = p->env->avs_get_cpu_flags(p->env);

    if (avs_is_planar(&p->vi))
    {
	for (iplane = 0; iplane<3; iplane++)
	{
		int plane = (iplane==0) ? AVS_PLANAR_Y : (iplane==1) ? AVS_PLANAR_U : AVS_PLANAR_V;

		const unsigned char* srcp = avs_get_read_ptr_p(src, plane);
	   // Request a Read pointer from the current source frame

		const unsigned char* prevp0 = avs_get_read_ptr_p(prev, plane);
		 unsigned char* prevp = (unsigned char*) prevp0; // with same pitch
	  // Request a Read pointer from the prev source frame.

  		const unsigned char* nextp0 = avs_get_read_ptr_p(next, plane);
		 unsigned char* nextp = (unsigned char*) nextp0; // with same pitch
	  // Request a Read pointer from the next source frame.

		unsigned char* dstp = avs_get_write_ptr_p(dst, plane);
		// Request a Write pointer from the newly created destination image.
	  // You can request a writepointer to images that have just been

		const int dst_pitch = avs_get_pitch_p(dst, plane);
	  // Requests pitch (length of a line) of the destination image.
	  // For more information on pitch see: http://www.avisynth.org/index.php?page=WorkingWithImages
		// (short version - pitch is always equal to or greater than width to allow for seriously fast assembly code)

		const int width = avs_get_row_size_p(dst,plane);
	  // Requests rowsize (number of used bytes in a line.
	  // See the link above for more information.

		const int height = avs_get_height_p(dst,plane);
	  // Requests the height of the destination image.

		const int src_pitch = avs_get_pitch_p(src,plane);
		const int prev_pitch = avs_get_pitch_p(prev,plane);
		const int next_pitch = avs_get_pitch_p(next,plane);

		// in v.0.1-0.3  all source pitches are  assumed equal (for simplicity)
                                // consider other (rare) case
		if (prev_pitch != src_pitch)
		{
		    prevp = (unsigned char *)malloc(height*src_pitch);
//		    int h =0;
		    for (h=0; h<0; h++)
		       memcpy(prevp+h*src_pitch, prevp0+h*prev_pitch, width);
		}

		if (next_pitch != src_pitch)
		{
		    nextp = (unsigned char *)malloc(height*src_pitch);
//		    int h;
		    for (h=0; h<0; h++)
		       memcpy(nextp+h*src_pitch, nextp0+h*next_pitch, width);
		}

		filter_plane(mode, dstp, dst_pitch, prevp, srcp, nextp, src_pitch, width, height, parity, tff, !!(cpu & AVS_CPU_INTEGER_SSE));
		if (prev_pitch != src_pitch)
			free(prevp);
		if (next_pitch != src_pitch)
			free(nextp);
	}

   }
   else if (avs_is_yuy2(&p->vi))
   {
		const unsigned char* srcp = avs_get_read_ptr(src);
	   // Request a Read pointer from the current source frame

		const unsigned char* prevp = avs_get_read_ptr(prev);
	  // Request a Read pointer from the prev source frame.

  		const unsigned char* nextp = avs_get_read_ptr(next);
	  // Request a Read pointer from the next source frame.

		unsigned char* dstp = avs_get_write_ptr(dst);
		// Request a Write pointer from the newly created destination image.
	  // You can request a writepointer to images that have just been

		const int dst_pitch = avs_get_pitch(dst);
	  // Requests pitch (length of a line) of the destination image.
	  // For more information on pitch see: http://www.avisynth.org/index.php?page=WorkingWithImages
		// (short version - pitch is always equal to or greater than width to allow for seriously fast assembly code)

		const int width = avs_get_row_size(dst)/2;
	  // Requests rowsize (number of used bytes in a line.
	  // See the link above for more information.

		const int height = avs_get_height(dst);
	  // Requests the height of the destination image.

		const int src_pitch = avs_get_pitch(src);
		const int prev_pitch = avs_get_pitch(prev);
		const int next_pitch = avs_get_pitch(next);

	int mmx = !!(cpu & AVS_CPU_INTEGER_SSE);
        YUY2ToPlanes(srcp, src_pitch, width, height, f->ysrc, f->ypitch, f->usrc, f->vsrc, f->uvpitch, mmx);
        YUY2ToPlanes(prevp, prev_pitch, width, height, f->yprev, f->ypitch, f->uprev, f->vprev, f->uvpitch, mmx);
        YUY2ToPlanes(nextp, next_pitch, width, height, f->ynext, f->ypitch, f->unext, f->vnext, f->uvpitch, mmx);

	filter_plane(mode, f->ydest, f->ypitch, f->yprev, f->ysrc, f->ynext, f->ypitch, width, height, parity, tff, mmx);
	filter_plane(mode, f->udest, f->uvpitch, f->uprev, f->usrc, f->unext, f->uvpitch, width/2, height, parity, tff, mmx);
	filter_plane(mode, f->vdest, f->uvpitch, f->vprev, f->vsrc, f->vnext, f->uvpitch, width/2, height, parity, tff, mmx);

        YUY2FromPlanes(dstp, dst_pitch, width, height, f->ydest, f->ypitch,  f->udest, f->vdest, f->uvpitch, mmx);

   }


  // As we now are finished processing the image, we return the destination image.
  p->env->avs_release_video_frame(src);
  p->env->avs_release_video_frame(next);
  p->env->avs_release_video_frame(prev);
	return dst;
}

void AVSC_CC yadif_free_filter (AVS_FilterInfo * fi)
{
  yadif_filter * f = (yadif_filter *)fi->user_data;
  if ( avs_is_yuy2(&f->fi->vi) ) {
  free(f->ysrc);
  free(f->usrc);
  free(f->vsrc);
  free(f->yprev);
  free(f->uprev);
  free(f->vprev);
  free(f->ynext);
  free(f->unext);
  free(f->vnext);
  free(f->ydest);
  free(f->udest);
  free(f->vdest);
  }
  free(f);
  fi->user_data = 0;
}

// This is the function that created the filter, when the filter has been called.
// This can be used for simple parameter checking, so it is possible to create different filters,
// based on the arguments recieved.

    // Calls the constructor with the arguments provied.

AVS_Value AVSC_CC yadif (AVS_ScriptEnvironment * env, AVS_Value args,
			  void * use_inplace)
{
  yadif_filter *f = (yadif_filter *)malloc(sizeof(yadif_filter));
  AVS_Value v;
  AVS_Value tmp;
  AVS_Clip * new_clip = env->avs_new_c_filter(env, &f->fi, avs_array_elt(args, 0), 1);
   f->fi->user_data = f;

 if (avs_is_yuy2(&f->fi->vi)) {
  	// create intermediate planar planes
  	f->yheight = f->fi->vi.height;
  	f->ywidth = f->fi->vi.width;
  	f->uvwidth = f->ywidth/2;
  	f->ypitch = ((f->ywidth)+15)/16*16;
  	f->uvpitch = ((f->uvwidth)+15)/16*16;
  	f->ysrc = (unsigned char *) malloc(f->yheight*f->ypitch);
  	f->usrc = (unsigned char *) malloc(f->yheight*f->uvpitch);
  	f->vsrc = (unsigned char *) malloc(f->yheight*f->uvpitch);
  	f->yprev = (unsigned char *) malloc(f->yheight*f->ypitch);
  	f->uprev = (unsigned char *) malloc(f->yheight*f->uvpitch);
  	f->vprev = (unsigned char *) malloc(f->yheight*f->uvpitch);
  	f->ynext = (unsigned char *) malloc(f->yheight*f->ypitch);
  	f->unext = (unsigned char *) malloc(f->yheight*f->uvpitch);
  	f->vnext = (unsigned char *) malloc(f->yheight*f->uvpitch);
  	f->ydest = (unsigned char *) malloc(f->yheight*f->ypitch);
  	f->udest = (unsigned char *) malloc(f->yheight*f->uvpitch);
  	f->vdest = (unsigned char *) malloc(f->yheight*f->uvpitch);
  }

  if (avs_is_planar(&f->fi->vi) || avs_is_yuy2(&f->fi->vi)) {
  	//   get first argument
    tmp = avs_array_elt(args, 1); // argument mode
     if (avs_defined(tmp))
        f->mode = avs_as_int(tmp);
     else
        f->mode = 0 ;  // set default value = 0 if no argument

	 if (f->mode & 1) {// bob mode, double number of frames and fps
		f->fi->vi.num_frames = f->fi->vi.num_frames * 2;
		if (f->fi->vi.fps_denominator & 1)
		   f->fi->vi.fps_numerator = f->fi->vi.fps_numerator * 2;
		else
		   f->fi->vi.fps_denominator >>= 1;
	}

    tmp = avs_array_elt(args, 2); // argument order
     if (avs_defined(tmp))
        f->order = avs_as_int(tmp);
     else
        f->order = -1 ;  // set default value = -1 if no argument

    f->fi->get_frame = yadif_get_frame;
    v = avs_new_value_clip(new_clip, env);
  } else {
    v = avs_new_value_error("Video must be YUV");
  }
  env->avs_release_clip(new_clip);
  f->fi->free_filter = yadif_free_filter;
  return v;
}


// The following function is the function that actually registers the filter in AviSynth
// It is called automatically, when the plugin is loaded to see which functions this filter contains.

const char * AVSC_CC avisynth_c_plugin_init(AVS_ScriptEnvironment * env)
{
  env->avs_add_function(env, "Yadif", "c[mode]i[order]i", "video/x-raw-yuv, format=(fourcc)YV12; video/x-raw-yuv, format=(fourcc)YUY2", "video/x-raw-yuv, format=(fourcc)YV12; video/x-raw-yuv, format=(fourcc)YUY2", yadif, 0);
  return "Yadif deinterlace";
}


