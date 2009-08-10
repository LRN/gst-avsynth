/*
	Telecide plugin for Avisynth -- recovers original progressive
	frames from telecined streams. The filter operates by matching
	fields and automatically adapts to phase/pattern changes.

	Copyright (C) 2003-2008 Donald A. Graft

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
*/
#include "version.h"
#include "asmfuncsYV12.h"
#ifdef _MSC_VER
#pragma warning(disable:4799)
#endif
#ifdef DEINTERLACE_MMX_BUILD

/*********************
 * YV12 Conditional deinterlacer.
 * 
 * (c) 2003, Klaus Post
 *    - Algorithm by Donald Graft
 *
 * ISSE, Blend version.
 *
 * Deinterlaces one line in a planar image based on
 * a given deinterlace map.
 *********************/

void isse_blend_plane(BYTE* org, BYTE* src_prev, BYTE* src_next, BYTE* dmaskp, int w) {
#if !defined(GCC__)
  __asm {
    xor eax, eax
    mov esi, org
    mov ebx, dmaskp
    mov ecx, src_prev
    mov edx, src_next
    pcmpeqb mm7,mm7  // All 1's
    jmp loop_test
    align 16
loopback:
    movq mm0,[esi+eax]  // src
    movq mm1,[ecx+eax]  // next
    movq mm2,[edx+eax]  // prev
    movq mm4,[ebx+eax]  // mask
    // Blend pixels
    pavgb mm1,mm2  // Blend prev & next 50/50
     movq mm5,mm4  // copy mask
    pavgb mm1,mm0  // Now correctly blended.
     pxor mm5,mm7  // Inv mask
    pand mm1, mm4
     pand mm0, mm5
//		movq mm1,mm0  // Enable debug viewing of mask!
    por mm1,mm0
    movq [esi+eax],mm1
    add eax,8
loop_test:
    cmp eax,w
    jl loopback
    emms
  }
#else
	__asm__ __volatile__
	(
	"	xor		eax,eax\n"
	"	mov		esi,%[org]\n"
	"	mov		ebx,%[dmaskp]\n"
	"	mov		ecx,%[src_prev]\n"
	"	mov		edx,%[src_next]\n"
	"	pcmpeqb	mm7,mm7\n"
	"	jmp		isse_blend_plane_loop_test\n"
	"	.align	16\n"
	"isse_blend_plane_loopback:\n"
	"	movq	mm0,[esi+eax]\n"
	"	movq	mm1,[ecx+eax]\n"
	"	movq	mm2,[edx+eax]\n"
	"	movq	mm4,[ebx+eax]\n"

	"	pavgb	mm1,mm2\n"
	"	movq	mm5,mm4\n"
	"	pavgb	mm1,mm0\n"
	"	pxor	mm5,mm7\n"
	"	pand	mm1,mm4\n"
	"	pand	mm0,mm5\n"

	"	por		mm1,mm0\n"
	"	movq	[esi+eax],mm1\n"
	"	add		eax,8\n"
	"isse_blend_plane_loop_test:\n"
	"	cmp		eax,%[w]\n"
	"	jl		isse_blend_plane_loopback\n"
	"	emms\n"

	:
	:	[w] "m" (w),
		[src_next] "m" (src_next),
		[src_prev] "m" (src_prev),
		[dmaskp] "m" (dmaskp),
		[org] "m" (org)
	:	"mm5", "mm0", "mm2", "mm1", "memory", "ebx", "ecx", "esi", "edx", "mm4", "mm7", "eax"
	);
#endif
}

/*********************
 * YV12 Conditional deinterlacer.
 * 
 * (c) 2003, Klaus Post
 *    - Algorithm by Donald Graft
 *
 * ISSE, Interpolate version.
 *
 * Deinterlaces one line in a planar image based on
 * a given deinterlace map.
 *********************/

void isse_interpolate_plane(BYTE* org, BYTE* src_prev, BYTE* src_next, BYTE* dmaskp, int w) {
#if !defined(GCC__)
  __asm {
    xor eax, eax
    mov esi, org
    mov ebx, dmaskp
    mov ecx, src_prev
    mov edx, src_next
    pcmpeqb mm7,mm7  // All 1's
    jmp loop_test
    align 16
loopback:
    movq mm4,[ebx+eax]  // mask
    movq mm0,[esi+eax]  // src
    movq mm1,[ecx+eax]  // next
    movq mm2,[edx+eax]  // prev
    movq mm5,mm4  // copy mask
    // Blend pixels
    pavgb mm1,mm2  // Blend prev & next 50/50
     pxor mm5,mm7  // Inv mask
    pand mm1, mm4
     pand mm0, mm5
//		movq mm1,mm0  // Enable debug viewing of mask!
    por mm1,mm0
    movq [esi+eax],mm1
    add eax,8
loop_test:
    cmp eax,w
    jl loopback
    emms
  }
#else
	__asm__ __volatile__
	(
	"	xor		eax,eax\n"
	"	mov		esi,%[org]\n"
	"	mov		ebx,%[dmaskp]\n"
	"	mov		ecx,%[src_prev]\n"
	"	mov		edx,%[src_next]\n"
	"	pcmpeqb	mm7,mm7\n"
	"	jmp		isse_interpolate_plane_loop_test\n"
	"	.align	16\n"
	"isse_interpolate_plane_loopback:\n"
	"	movq	mm4,[ebx+eax]\n"
	"	movq	mm0,[esi+eax]\n"
	"	movq	mm1,[ecx+eax]\n"
	"	movq	mm2,[edx+eax]\n"
	"	movq	mm5,mm4\n"

	"	pavgb	mm1,mm2\n"
	"	pxor	mm5,mm7\n"
	"	pand	mm1,mm4\n"
	"	pand	mm0,mm5\n"

	"	por		mm1,mm0\n"
	"	movq	[esi+eax],mm1\n"
	"	add		eax,8\n"
	"isse_interpolate_plane_loop_test:\n"
	"	cmp		eax,%[w]\n"
	"	jl		isse_interpolate_plane_loopback\n"
	"	emms\n"

	:
	:	[w] "m" (w),
		[src_next] "m" (src_next),
		[src_prev] "m" (src_prev),
		[dmaskp] "m" (dmaskp),
		[org] "m" (org)
	:	"mm5", "mm0", "mm2", "mm1", "memory", "ebx", "ecx", "edx", "esi", "mm4", "mm7", "eax"
	);
#endif
}



/*********************
 * YV12 Deinterlace mask.
 * 
 * (c) 2003, Klaus Post
 *    - Algorithm by Donald Graft
 *
 * MMX version.
 *
 * Creates two masks based on the interlacing in the image.
 * Overwrites both masks.
 *********************/


void mmx_createmask_plane(const BYTE* srcp, const BYTE* prev_p, const BYTE* next_p, BYTE* fmaskp, BYTE* dmaskp, int threshold, int dthreshold, int row_size) {
  threshold=(threshold+1)/2;
  dthreshold=(dthreshold+1)/2;
  __int64 dt = dthreshold | (dthreshold<<16);
  __int64 t = threshold | (threshold<<16);
#if !defined(GCC__)
  __declspec(align(8)) static __int64 mask1 = 0x00ff00ff00ff00ff;

  __asm {
    xor eax, eax
    mov esi, srcp
    mov ebx, prev_p
    mov ecx, next_p
    mov edx, dmaskp
    mov edi, fmaskp
    movd mm6, [t]
    pxor mm7, mm7
    punpckldq mm6,mm6   
    jmp loop_test
    align 16
loopback:
    movq mm0, [esi+eax]  // src
     movq mm1, [ebx+eax]  // prev
    movq mm3,mm0
     movq mm4,mm1
    movq mm2, [ecx+eax]  // next
     punpcklbw mm0,mm7  // src low
    punpckhbw mm3,mm7  // src high
      movq mm5,mm2
    punpcklbw mm1,mm7  // prev low
     punpckhbw mm4,mm7  // prev high
    punpcklbw mm2,mm7  // next low
     punpckhbw mm5,mm7  // next high
    movd mm7, [dt]
    psubsw mm1, mm0  // p-src low
     psubsw mm2, mm0  // n-src low
		psraw mm1,1       // Avoid multiply signed overflow
     psubsw mm4, mm3   // p-src high
    pmullw mm1,mm2  // multiply low
     psubsw mm5, mm3  // n-src high
		 psraw mm4,1       // Avoid multiply signed overflow
     movq mm0,mm1         // copy low result
    pmullw mm4,mm5    // multiply high
     punpckldq mm7,mm7   // 
     movq mm3,[mask1]
    movq mm2,mm4          // copy high result
     pcmpgtw mm0,mm6      // Threshold low  0 if mult result is less than t
    pcmpgtw mm2,mm6     // Threshold high
     pcmpgtw mm1,mm7      // D-Threshold low
    pcmpgtw mm4,mm7     // D-Threshold high
     pand mm0,mm3
    pand mm1,mm3
     pand mm2,mm3
    pand mm4,mm3
     packuswb mm0,mm2   // fmask
    packuswb mm1,mm4    // dmask
     movq [edi+eax],mm0
   movq [edx+eax],mm1
    pxor mm7, mm7
    add eax,8
loop_test:
    cmp eax,row_size
    jl loopback
		emms
  }
#else
  static __int64 mask1 = 0x00ff00ff00ff00ffLL;

	__asm__ __volatile__
	(
	"	xor		eax,eax\n"
	"	mov		esi,%[srcp]\n"
	"	mov		ebx,%[prev_p]\n"
	"	mov		ecx,%[next_p]\n"
	"	mov		edx,%[dmaskp]\n"
	"	mov		edi,%[fmaskp]\n"
	"	movq		mm6,[%[t]]\n"
	"	pxor		mm7,mm7\n"
	"	punpckldq	mm6,mm6\n"
	"	jmp		mmx_createmask_plane_loop_test\n"
	"	.align	16\n"
	"mmx_createmask_plane_loopback:\n"
	"	movq		mm0,[esi+eax]\n"
	"	movq		mm1,[ebx+eax]\n"
	"	movq		mm3,mm0\n"
	"	movq		mm4,mm1\n"
	"	movq		mm2,[ecx+eax]\n"
	"	punpcklbw	mm0,mm7\n"
	"	punpckhbw	mm3,mm7\n"
	"	movq	mm5,mm2\n"
	"	punpcklbw	mm1,mm7\n"
	"	punpckhbw	mm4,mm7\n"
	"	punpcklbw	mm2,mm7\n"
	"	punpckhbw	mm5,mm7\n"
	"	movq	mm7,[%[dt]]\n"
	"	psubsw	mm1,mm0\n"
	"	psubsw	mm2,mm0\n"
	"	psraw	mm1,1\n"
	"	psubsw	mm4,mm3\n"
	"	pmullw	mm1,mm2\n"
	"	psubsw	mm5,mm3\n"
	"	psraw	mm4,1\n"
	"	movq	mm0,mm1\n"
	"	pmullw	mm4,mm5\n"
	"	punpckldq	mm7,mm7\n"
	"	movq	mm3,[%[mask1]]\n"
	"	movq	mm2,mm4\n"
	"	pcmpgtw	mm0,mm6\n"
	"	pcmpgtw	mm2,mm6\n"
	"	pcmpgtw	mm1,mm7\n"
	"	pcmpgtw	mm4,mm7\n"
	"	pand	mm0,mm3\n"
	"	pand	mm1,mm3\n"
	"	pand	mm2,mm3\n"
	"	pand	mm4,mm3\n"
	"	packuswb	mm0,mm2\n"
	"	packuswb	mm1,mm4\n"
	"	movq	[edi+eax],mm0\n"
	"	movq	[edx+eax],mm1\n"
	"	pxor	mm7,mm7\n"
	"	add		eax,8\n"
	"mmx_createmask_plane_loop_test:\n"
	"	cmp		eax,%[row_size]\n"
	"	jl		mmx_createmask_plane_loopback\n"
	"	emms\n"

	:
	:	[dt] "m" (dt),
		[t] "m" (t),
		[fmaskp] "m" (fmaskp),
		[srcp] "m" (srcp),
		[row_size] "m" (row_size),
		[mask1] "m" (mask1),
		[dmaskp] "m" (dmaskp),
		[next_p] "m" (next_p),
		[prev_p] "m" (prev_p)
	:	"mm5", "mm0", "mm2", "mm1", "mm3", "memory", "ebx", "ecx", "edx", "esi", "mm6", "mm4", "edi", "mm7", "eax"
	);
#endif
}



/*********************
 * YV12 Deinterlace mask.
 * 
 * (c) 2003, Klaus Post
 *    - Algorithm by Donald Graft
 *
 * MMX version. One mask only!
 *
 * Creates one masks based on the interlacing in the image.
 * Overwrites existing mask.
 *********************/


void mmx_createmask_plane_single(const BYTE* srcp, const BYTE* prev_p, const BYTE* next_p, BYTE* dmaskp, int dthreshold, int row_size) {
  dthreshold=(dthreshold+1)/2;
  __int64 dt = dthreshold | (dthreshold<<16);
#if !defined(GCC__)
  __declspec(align(8)) static __int64 mask1 = 0x00ff00ff00ff00ff;
  __asm {
    xor eax, eax
    mov esi, srcp
    mov ebx, prev_p
    mov ecx, next_p
    mov edx, dmaskp
    lea edi, mask1
    movd mm6, [dt]
    pxor mm7, mm7
    punpckldq mm6,mm6   
    jmp loop_test
    align 16
loopback:
    movq mm0, [esi+eax]  // src
     movq mm1, [ebx+eax]  // prev
    movq mm3,mm0
     movq mm4,mm1
    movq mm2, [ecx+eax]  // next
     punpcklbw mm0,mm7  // src low
    punpckhbw mm3,mm7  // src high
      movq mm5,mm2
    punpcklbw mm1,mm7  // prev low
     punpckhbw mm4,mm7  // prev high
    punpcklbw mm2,mm7  // next low
     punpckhbw mm5,mm7  // next high
    psubsw mm1, mm0  // p-src low
     psubsw mm2, mm0  // n-src low
		psraw mm1,1				// Avoid multiply signed overflow
     psubsw mm4, mm3   // p-src high
    pmullw mm1,mm2  // multiply low
     psubsw mm5, mm3  // n-src high
		psraw mm4,1				// Avoid multiply signed overflow
     movq mm0,mm1         // copy low result
    pmullw mm4,mm5    // multiply high
     movq mm3,[edi]
    movq mm2,mm4          // copy high result
     pcmpgtw mm1,mm6      // D-Threshold low
    pcmpgtw mm4,mm6     // D-Threshold high
     pand mm0,mm3
    pand mm1,mm3
     pand mm2,mm3
    pand mm4,mm3
    packuswb mm1,mm4    // dmask
    movq [edx+eax],mm1
    add eax,8
loop_test:
    cmp eax,row_size
    jl loopback		
    emms
  }
#else
  static __int64 mask1 = 0x00ff00ff00ff00ffLL;
	__asm__ __volatile__
	(
	"	xor		eax,eax\n"
	"	mov		esi,%[srcp]\n"
	"	mov		ebx,%[prev_p]\n"
	"	mov		ecx,%[next_p]\n"
	"	mov		edx,%[dmaskp]\n"
	"	lea		edi,%[mask1]\n"
	"	movq	mm6,[%[dt]]\n"
	"	pxor	mm7,mm7\n"
	"	punpckldq	mm6,mm6\n"
	"	jmp		mmx_createmask_plane_single_loop_test\n"
	"	.align	16\n"
	"mmx_createmask_plane_single_loopback:\n"
	"	movq	mm0,[esi+eax]\n"
	"	movq	mm1,[ebx+eax]\n"
	"	movq	mm3,mm0\n"
	"	movq	mm4,mm1\n"
	"	movq	mm2,[ecx+eax]\n"
	"	punpcklbw	mm0,mm7\n"
	"	punpckhbw	mm3,mm7\n"
	"	movq	mm5,mm2\n"
	"	punpcklbw	mm1,mm7\n"
	"	punpckhbw	mm4,mm7\n"
	"	punpcklbw	mm2,mm7\n"
	"	punpckhbw	mm5,mm7\n"
	"	psubsw	mm1,mm0\n"
	"	psubsw	mm2,mm0\n"
	"	psraw	mm1,1\n"
	"	psubsw	mm4,mm3\n"
	"	pmullw	mm1,mm2\n"
	"	psubsw	mm5,mm3\n"
	"	psraw	mm4,1\n"
	"	movq	mm0,mm1\n"
	"	pmullw	mm4,mm5\n"
	"	movq	mm3,[edi]\n"
	"	movq	mm2,mm4\n"
	"	pcmpgtw	mm1,mm6\n"
	"	pcmpgtw	mm4,mm6\n"
	"	pand	mm0,mm3\n"
	"	pand	mm1,mm3\n"
	"	pand	mm2,mm3\n"
	"	pand	mm4,mm3\n"
	"	packuswb	mm1,mm4\n"
	"	movq	[edx+eax],mm1\n"
	"	add		eax,8\n"
	"mmx_createmask_plane_single_loop_test:\n"
	"	cmp		eax,%[row_size]\n"
	"	jl		mmx_createmask_plane_single_loopback\n"
	"	emms\n"

	:
	:	[dt] "m" (dt),
		[srcp] "m" (srcp),
		[row_size] "m" (row_size),
		[mask1] "m" (mask1),
		[dmaskp] "m" (dmaskp),
		[next_p] "m" (next_p),
		[prev_p] "m" (prev_p)
	:	"mm5", "mm0", "mm2", "mm1", "mm3", "memory", "ebx", "ecx", "edx", "esi", "mm6", "mm4", "edi", "mm7", "eax"
	);
#endif
}


#endif

/*******************************
 *****  DECIMATE HELPERS  ******
 ******************************/


#ifdef DECIMATE_MMX_BUILD


/*********************
 * YV12 Plane blender.
 * 
 * (c) 2003, Klaus Post
 *    - Algorithm by Donald Graft
 *
 * ISSE, Blend version.
 *
 * Blends two planes 50/50 
 * Rowsize of mod8 is required.
 * src and dst way very well be the same!
 *********************/

void isse_blend_decimate_plane(BYTE* dst, BYTE* src,  BYTE* src_next, int w, int h, int dst_pitch, int src_pitch, int src_next_pitch) {
  if (!h) return;  // Height == 0 - avoid silly crash.
#if !defined(GCC__)
  __asm {
    mov ebx, h
    mov ecx, w
    mov esi, src
    mov edi, dst
    mov edx, src_next
    align 16
yloop:
    xor eax, eax  // Width counter
    jmp loop_test
    align 16
loopback:
    movq mm0,[esi+eax]  // src
    movq mm1,[edx+eax]  // next
    // Blend pixels
    pavgb mm1,mm0  // Blend prev & src 50 / 50
    movq [edi+eax],mm1
    add eax,8
loop_test:
    cmp eax,ecx
    jl loopback
    add edi,dst_pitch
    add esi,src_pitch
    add edx,src_next_pitch
    dec ebx
    jnz yloop
    emms
  }
#else
	__asm__ __volatile__
	(
	"	mov		ebx,%[h]\n"
	"	mov		ecx,%[w]\n"
	"	mov		esi,%[src]\n"
	"	mov		edi,%[dst]\n"
	"	mov		edx,%[src_next]\n"
	"	.align	16\n"
	"isse_blend_decimate_plane_yloop:\n"
	"	xor		eax,eax\n"
	"	jmp		isse_blend_decimate_plane_loop_test\n"
	"	.align	16\n"
	"isse_blend_decimate_plane_loopback:\n"
	"	movq	mm0,[esi+eax]\n"
	"	movq	mm1,[edx+eax]\n"

	"	pavgb	mm1,mm0\n"
	"	movq	[edi+eax],mm1\n"
	"	add		eax,8\n"
	"isse_blend_decimate_plane_loop_test:\n"
	"	cmp		eax,ecx\n"
	"	jl		isse_blend_decimate_plane_loopback\n"
	"	add		edi,%[dst_pitch]\n"
	"	add		esi,%[src_pitch]\n"
	"	add		edx,%[src_next_pitch]\n"
	"	dec		ebx\n"
	"	jnz		isse_blend_decimate_plane_yloop\n"
	"	emms\n"

	:
	:	[w] "m" (w),
		[src] "m" (src),
		[src_next_pitch] "m" (src_next_pitch),
		[dst_pitch] "m" (dst_pitch),
		[src_next] "m" (src_next),
		[h] "m" (h),
		[src_pitch] "m" (src_pitch),
		[dst] "m" (dst)
	:	"mm0", "mm1", "memory", "ebx", "ecx", "edx", "esi", "edi", "eax"
	);
#endif
}


/*********************
 * YV12 Scenechange detection.
 * 
 * (c) 2003, Klaus Post
 *
 * ISSE, MOD 32 version.
 *
 * Returns an int of the accumulated absolute difference
 * between two planes.
 *
 * The absolute difference between two planes are returned as an int.
 * This version is optimized for mod32 widths. Others widths are allowed, 
 *  but the remaining pixels are simply skipped.
 *********************/

int isse_scenechange_32(const BYTE* c_plane, const BYTE* tplane, int height, int width, int c_pitch, int t_pitch) {
  int wp=(width/32)*32;
  int hp=height;
  int returnvalue=0xbadbad00;
#if !defined(GCC__)
  __asm {
    xor ebx,ebx     // Height
    pxor mm5,mm5  // Maximum difference
    mov edx, c_pitch    //copy pitch
    mov ecx, t_pitch    //copy pitch
    pxor mm6,mm6   // We maintain two sums, for better pairablility
    pxor mm7,mm7
    mov esi, c_plane
    mov edi, tplane
    jmp yloopover
    align 16
yloop:
    inc ebx
    add edi,ecx     // add pitch to both planes
    add esi,edx
yloopover:
    cmp ebx,[hp]
    jge endframe
    xor eax,eax     // Width
    align 16
xloop:
    cmp eax,[wp]    
    jge yloop

    movq mm0,[esi+eax]
     movq mm2,[esi+eax+8]
    movq mm1,[edi+eax]
     movq mm3,[edi+eax+8]
    psadbw mm0,mm1    // Sum of absolute difference
     psadbw mm2,mm3
    paddd mm6,mm0     // Add...
     paddd mm7,mm2
    movq mm0,[esi+eax+16]
     movq mm2,[esi+eax+24]
    movq mm1,[edi+eax+16]
     movq mm3,[edi+eax+24]
    psadbw mm0,mm1
     psadbw mm2,mm3
    paddd mm6,mm0
     paddd mm7,mm2

    add eax,32
    jmp xloop
endframe:
    paddd mm7,mm6
    movd returnvalue,mm7
    emms
  }
#else
	__asm__ __volatile__
	(
	"	xor		ebx,ebx\n"
	"	pxor	mm5,mm5\n"
	"	mov		edx,%[c_pitch]\n"
	"	mov		ecx,%[t_pitch]\n"
	"	pxor	mm6,mm6\n"
	"	pxor	mm7,mm7\n"
	"	mov		esi,%[c_plane]\n"
	"	mov		edi,%[tplane]\n"
	"	jmp		isse_scenechange_32_yloopover\n"
	"	.align	16\n"
	"isse_scenechange_32_yloop:\n"
	"	inc		ebx\n"
	"	add		edi,ecx\n"
	"	add		esi,edx\n"
	"isse_scenechange_32_yloopover:\n"
	"	cmp		ebx,[%[hp]]\n"
	"	jge		isse_scenechange_32_endframe\n"
	"	xor		eax,eax\n"
	"	.align	16\n"
	"isse_scenechange_32_xloop:\n"
	"	cmp		eax,[%[wp]]\n"
	"	jge		isse_scenechange_32_yloop\n"

	"	movq	mm0,[esi+eax]\n"
	"	movq	mm2,[esi+eax+8]\n"
	"	movq	mm1,[edi+eax]\n"
	"	movq	mm3,[edi+eax+8]\n"
	"	psadbw	mm0,mm1\n"
	"	psadbw	mm2,mm3\n"
	"	paddd	mm6,mm0\n"
	"	paddd	mm7,mm2\n"
	"	movq	mm0,[esi+eax+16]\n"
	"	movq	mm2,[esi+eax+24]\n"
	"	movq	mm1,[edi+eax+16]\n"
	"	movq	mm3,[edi+eax+24]\n"
	"	psadbw	mm0,mm1\n"
	"	psadbw	mm2,mm3\n"
	"	paddd	mm6,mm0\n"
	"	paddd	mm7,mm2\n"

	"	add		eax,32\n"
	"	jmp		isse_scenechange_32_xloop\n"
	"isse_scenechange_32_endframe:\n"
	"	paddd	mm7,mm6\n"
	"	movd	%[returnvalue],mm7\n"
	"	emms\n"

	:	[returnvalue] "=m" (returnvalue)
	:	[tplane] "m" (tplane),
		[c_plane] "m" (c_plane),
		[hp] "m" (hp),
		[wp] "m" (wp),
		[t_pitch] "m" (t_pitch),
		[c_pitch] "m" (c_pitch)
	:	"mm5", "mm0", "mm1", "mm2", "mm3", "ebx", "ecx", "esi", "edx", "mm6", "edi", "mm7", "eax"
	);
#endif
  return returnvalue;
}

/*********************
 * YV12 Scenechange detection.
 * 
 * (c) 2003, Klaus Post
 *
 * ISSE, MOD 16 version.
 *
 * Returns an int of the accumulated absolute difference
 * between two planes. 
 *
 * The absolute difference between two planes are returned as an int.
 * This version is optimized for mod16 widths. Others widths are allowed, 
 *  but the remaining pixels are simply skipped.
 *********************/


int isse_scenechange_16(const BYTE* c_plane, const BYTE* tplane, int height, int width, int c_pitch, int t_pitch) {
  int wp=(width/16)*16;
  int hp=height;
  int returnvalue=0xbadbad00;
#if !defined(GCC__)
  __asm {
    xor ebx,ebx     // Height
    pxor mm5,mm5  // Maximum difference
    mov edx, c_pitch    //copy pitch
    mov ecx, t_pitch    //copy pitch
    pxor mm6,mm6   // We maintain two sums, for better pairablility
    pxor mm7,mm7
    mov esi, c_plane
    mov edi, tplane
    jmp yloopover
    align 16
yloop:
    inc ebx
    add edi,ecx     // add pitch to both planes
    add esi,edx
yloopover:
    cmp ebx,[hp]
    jge endframe
    xor eax,eax     // Width
    align 16
xloop:
    cmp eax,[wp]    
    jge yloop

    movq mm0,[esi+eax]
     movq mm2,[esi+eax+8]
    movq mm1,[edi+eax]
     movq mm3,[edi+eax+8]
    psadbw mm0,mm1    // Sum of absolute difference
     psadbw mm2,mm3
    paddd mm6,mm0     // Add...
     paddd mm7,mm2

    add eax,16
    jmp xloop
endframe:
    paddd mm7,mm6
    movd returnvalue,mm7
    emms
  }
#else
	__asm__ __volatile__
	(
	"	xor		ebx,ebx\n"
	"	pxor	mm5,mm5\n"
	"	mov		edx,%[c_pitch]\n"
	"	mov		ecx,%[t_pitch]\n"
	"	pxor	mm6,mm6\n"
	"	pxor	mm7,mm7\n"
	"	mov		esi,%[c_plane]\n"
	"	mov		edi,%[tplane]\n"
	"	jmp		isse_scenechange_16_yloopover\n"
	"	.align	16\n"
	"isse_scenechange_16_yloop:\n"
	"	inc		ebx\n"
	"	add		edi,ecx\n"
	"	add		esi,edx\n"
	"isse_scenechange_16_yloopover:\n"
	"	cmp		ebx,[%[hp]]\n"
	"	jge		isse_scenechange_16_endframe\n"
	"	xor		eax,eax\n"
	"	.align	16\n"
	"isse_scenechange_16_xloop:\n"
	"	cmp		eax,[%[wp]]\n"
	"	jge		isse_scenechange_16_yloop\n"

	"	movq	mm0,[esi+eax]\n"
	"	movq	mm2,[esi+eax+8]\n"
	"	movq	mm1,[edi+eax]\n"
	"	movq	mm3,[edi+eax+8]\n"
	"	psadbw	mm0,mm1\n"
	"	psadbw	mm2,mm3\n"
	"	paddd	mm6,mm0\n"
	"	paddd	mm7,mm2\n"

	"	add		eax,16\n"
	"	jmp		isse_scenechange_16_xloop\n"
	"isse_scenechange_16_endframe:\n"
	"	paddd	mm7,mm6\n"
	"	movd	%[returnvalue],mm7\n"
	"	emms\n"

	:	[returnvalue] "=m" (returnvalue)
	:	[tplane] "m" (tplane),
		[c_plane] "m" (c_plane),
		[hp] "m" (hp),
		[wp] "m" (wp),
		[t_pitch] "m" (t_pitch),
		[c_pitch] "m" (c_pitch)
	:	"mm5", "mm0", "mm1", "mm2", "mm3", "ebx", "ecx", "esi", "edx", "mm6", "edi", "mm7", "eax"
	);
#endif
  return returnvalue;
}

/*********************
 * YV12 Scenechange detection.
 * 
 * (c) 2003, Klaus Post
 *
 * ISSE, MOD 8 version.
 *
 * Returns an int of the accumulated absolute difference
 * between two planes. 
 *
 * The absolute difference between two planes are returned as an int.
 * This version is optimized for mod8 widths. Others widths are allowed, 
 *  but the remaining pixels are simply skipped.
 *********************/


int isse_scenechange_8(const BYTE* c_plane, const BYTE* tplane, int height, int width, int c_pitch, int t_pitch) {
  int wp=(width/8)*8;
  int hp=height;
  int returnvalue=0xbadbad00;
#if !defined(GCC__)
  __asm {
    xor ebx,ebx     // Height
    pxor mm5,mm5  // Maximum difference
    mov edx, c_pitch    //copy pitch
    mov ecx, t_pitch    //copy pitch
    pxor mm6,mm6   // We maintain two sums, for better pairablility
    pxor mm7,mm7
    mov esi, c_plane
    mov edi, tplane
    jmp yloopover
    align 16
yloop:
    inc ebx
    add edi,ecx     // add pitch to both planes
    add esi,edx
yloopover:
    cmp ebx,[hp]
    jge endframe
    xor eax,eax     // Width
    align 16
xloop:
    cmp eax,[wp]    
    jge yloop

    movq mm0,[esi+eax]
    movq mm1,[edi+eax]
    psadbw mm0,mm1    // Sum of absolute difference
    paddd mm6,mm0     // Add...

    add eax,8
    jmp xloop
endframe:
    movd returnvalue,mm6
    emms
  }
#else
	__asm__ __volatile__
	(
	"	xor		ebx,ebx\n"
	"	pxor	mm5,mm5\n"
	"	mov		edx,%[c_pitch]\n"
	"	mov		ecx,%[t_pitch]\n"
	"	pxor	mm6,mm6\n"
	"	pxor	mm7,mm7\n"
	"	mov		esi,%[c_plane]\n"
	"	mov		edi,%[tplane]\n"
	"	jmp		isse_scenechange_8_yloopover\n"
	"	.align	16\n"
	"isse_scenechange_8_yloop:\n"
	"	inc		ebx\n"
	"	add		edi,ecx\n"
	"	add		esi,edx\n"
	"isse_scenechange_8_yloopover:\n"
	"	cmp		ebx,[%[hp]]\n"
	"	jge		isse_scenechange_8_endframe\n"
	"	xor		eax,eax\n"
	"	.align	16\n"
	"isse_scenechange_8_xloop:\n"
	"	cmp		eax,[%[wp]]\n"
	"	jge		isse_scenechange_8_yloop\n"

	"	movq	mm0,[esi+eax]\n"
	"	movq	mm1,[edi+eax]\n"
	"	psadbw	mm0,mm1\n"
	"	paddd	mm6,mm0\n"

	"	add		eax,8\n"
	"	jmp		isse_scenechange_8_xloop\n"
	"isse_scenechange_8_endframe:\n"
	"	movd	%[returnvalue],mm6\n"
	"	emms\n"

	:	[returnvalue] "=m" (returnvalue)
	:	[tplane] "m" (tplane),
		[c_plane] "m" (c_plane),
		[hp] "m" (hp),
		[wp] "m" (wp),
		[t_pitch] "m" (t_pitch),
		[c_pitch] "m" (c_pitch)
	:	"mm5", "mm0", "mm1", "ebx", "ecx", "esi", "edx", "mm6", "edi", "mm7", "eax"
	);
#endif
  return returnvalue;
}


#ifdef _MSC_VER
#pragma warning(default:4799)
#endif
#endif

