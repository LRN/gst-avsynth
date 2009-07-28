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
#include <avsynth/gstavsynth_sdk.h>

#ifdef DEINTERLACE_MMX_BUILD
#pragma warning(disable:4799)
void __declspec(naked) asm_deinterlaceYUY2(const unsigned char *dstp, const unsigned char *p,
								  const unsigned char *n, unsigned char *fmask,
								  unsigned char *dmask, int thresh, int dthresh, int row_size)
{
#if !defined(GCC__)
	static const __int64 Mask0 = 0xFFFF0000FFFF0000i64;
	static const __int64 Mask2 = 0x00000000000000FFi64;
	static const __int64 Mask3 = 0x0000000000FF0000i64;
	static const __int64 Mask4 = 0x0000000000FF00FFi64;

	__asm
	{
		push		ebp
		push		ebx
		push		esi
		push		edi

		mov			edx,[esp+4+16]	// dstp
		mov			ecx,[esp+8+16]	// p
		mov			ebx,[esp+12+16]	// n
		mov			eax,[esp+16+16] // fmaskp
		mov			esi,[esp+20+16] // dmaskp
		movd		mm0,[esp+24+16] // threshold
		movq		mm3,mm0
		psllq		mm3,32
		por			mm3,mm0
		movd		mm4,[esp+28+16] // dthreshold
		movq		mm5,mm4
		psllq		mm5,32
		por			mm5,mm4
		mov			ebp,[esp+32+16]	// row_size
xloop:
		pxor		mm4,mm4
		movd		mm0,[ecx]		// load p
		punpcklbw	mm0,mm4			// unpack
		movd		mm2,[edx]		// load dstp
		punpcklbw	mm2,mm4			// unpack
		movd		mm1,[ebx]		// load n
		punpcklbw	mm1,mm4			// unpack
		psubw		mm0,mm2			// pprev - curr = P
		psubw		mm1,mm2			// pnext - curr = N
		movq		mm6,Mask0
		pandn		mm6,mm0			// mm6 = 0 | P2 | 0 | P0
		pmaddwd		mm6,mm1			// mm6 = P2*N2  | P0*N0
		movq		mm0,mm6

		pcmpgtd		mm6,mm3			// mm6 = P2*N2<T | P0*N0<T
		movq		mm4,mm6			// save result
		pand		mm6,Mask2		// make 0x000000ff if P0*N0<T
		movq		mm7,mm6			// move it into result
		psrlq		mm4,32			// get P2*N2<T
		pand		mm4,Mask3		// make 0x00ff0000 if P2*N2<T
		por			mm7,mm4			// move it into the result
		movd		[eax],mm7		// save final result for fmask

		pcmpgtd		mm0,mm5			// mm0 = P2*N2<DT | P0*N0<DT
		movq		mm4,mm0			// save result
		pand		mm0,Mask2		// make 0x000000ff if P0*N0<DT
		movq		mm7,mm0			// move it into result
		psrlq		mm4,32			// get P2*N2<DT
		pand		mm4,Mask3		// make 0x00ff0000 if P2*N2<DT
		por			mm7,mm4			// move it into the result
		movq		[esi],mm7			// save intermediate result

		pxor		mm4,mm4			// do chroma bytes now
		movd		mm0,[ecx]		// load p
		punpcklbw	mm0,mm4			// unpack
		movd		mm1,[ebx]		// load n
		punpcklbw	mm1,mm4			// unpack
		psubw		mm0,mm2			// pprev - curr = P
		psubw		mm1,mm2			// pnext - curr = N
		movq		mm6,Mask0
		pand		mm6,mm0			// mm6 = P3 | 0 | P1| 0
		pmaddwd		mm6,mm1			// mm6 = P3*N3  | P1*N1
		movq		mm0,mm6

		pcmpgtd		mm0,mm5			// mm0 = P3*N3<DT | P1*N1<DT
		movq		mm4,mm0			// save result
		pand		mm0,Mask4		// make 0x00ff00ff if P1*N1<DT
		movq		mm7,mm0			// move it into result
		psrlq		mm4,32			// get P3*N3<DT
		pand		mm4,Mask4		// make 0x00ff00ff if P3*N3<DT
		por			mm7,mm4			// move it into the result
		movd		mm4,[esi]		// Pick up saved intermediate result
		por			mm4,mm7
		movd		[esi],mm4		// Save final result for dmask

		add			edx,4
		add			ecx,4
		add			ebx,4
		add			eax,4
		add			esi,4
		dec			ebp
		jnz			xloop

		pop			edi
		pop			esi
		pop			ebx
		pop			ebp
    emms
		ret
	};	
#else
	static const __int64 Mask0 = 0xFFFF0000FFFF0000LL;
	static const __int64 Mask2 = 0x00000000000000FFLL;
	static const __int64 Mask3 = 0x0000000000FF0000LL;
	static const __int64 Mask4 = 0x0000000000FF00FFLL;

	__asm__ __volatile__
	(

	"	push	ebp\n"
	"	push	ebx\n"
	"	push	esi\n"
	"	push	edi\n"

	"	mov		edx,[esp+4+16]\n"
	"	mov		ecx,[esp+8+16]\n"
	"	mov		ebx,[esp+12+16]\n"
	"	mov		eax,[esp+16+16]\n"
	"	mov		esi,[esp+20+16]\n"
	"	movd	mm0,[esp+24+16]\n"
	"	movq	mm3,mm0\n"
	"	psllq	mm3,32\n"
	"	por		mm3,mm0\n"
	"	movd	mm4,[esp+28+16]\n"
	"	movq	mm5,mm4\n"
	"	psllq	mm5,32\n"
	"	por		mm5,mm4\n"
	"	mov		ebp,[esp+32+16]\n"
	"asm_deinterlaceYUY2_xloop:\n"
	"	pxor	mm4,mm4\n"
	"	movd	mm0,[ecx]\n"
	"	punpcklbw	mm0,mm4\n"
	"	movd	mm2,[edx]\n"
	"	punpcklbw	mm2,mm4\n"
	"	movd	mm1,[ebx]\n"
	"	punpcklbw	mm1,mm4\n"
	"	psubw	mm0,mm2\n"
	"	psubw	mm1,mm2\n"
	"	movq	mm6,%[Mask0]\n"
	"	pandn	mm6,mm0\n"
	"	pmaddwd	mm6,mm1\n"
	"	movq	mm0,mm6\n"

	"	pcmpgtd	mm6,mm3\n"
	"	movq	mm4,mm6\n"
	"	pand	mm6,%[Mask2]\n"
	"	movq	mm7,mm6\n"
	"	psrlq	mm4,32\n"
	"	pand	mm4,%[Mask3]\n"
	"	por		mm7,mm4\n"
	"	movd	[eax],mm7\n"

	"	pcmpgtd	mm0,mm5\n"
	"	movq	mm4,mm0\n"
	"	pand	mm0,%[Mask2]\n"
	"	movq	mm7,mm0\n"
	"	psrlq	mm4,32\n"
	"	pand	mm4,%[Mask3]\n"
	"	por		mm7,mm4\n"
	"	movq	[esi],mm7\n"

	"	pxor	mm4,mm4\n"
	"	movd	mm0,[ecx]\n"
	"	punpcklbw	mm0,mm4\n"
	"	movd	mm1,[ebx]\n"
	"	punpcklbw	mm1,mm4\n"
	"	psubw	mm0,mm2\n"
	"	psubw	mm1,mm2\n"
	"	movq	mm6,%[Mask0]\n"
	"	pand	mm6,mm0\n"
	"	pmaddwd	mm6,mm1\n"
	"	movq	mm0,mm6\n"

	"	pcmpgtd	mm0,mm5\n"
	"	movq	mm4,mm0\n"
	"	pand	mm0,%[Mask4]\n"
	"	movq	mm7,mm0\n"
	"	psrlq	mm4,32\n"
	"	pand	mm4,%[Mask4]\n"
	"	por		mm7,mm4\n"
	"	movd	mm4,[esi]\n"
	"	por		mm4,mm7\n"
	"	movd	[esi],mm4\n"

	"	add		edx,4\n"
	"	add		ecx,4\n"
	"	add		ebx,4\n"
	"	add		eax,4\n"
	"	add		esi,4\n"
	"	dec		ebp\n"
	"	jnz		asm_deinterlaceYUY2_xloop\n"

	"	pop		edi\n"
	"	pop		esi\n"
	"	pop		ebx\n"
	"	pop		ebp\n"
	"	emms\n"
	"	ret\n"

	:
	:	[Mask4] "rm" (Mask4),
		[Mask0] "rm" (Mask0),
		[Mask2] "rm" (Mask2),
		[Mask3] "rm" (Mask3)
	:	"mm5", "mm0", "mm1", "mm2", "mm3", "memory", "ebx", "ecx", "edx", "esi", "mm6", "edi", "mm4", "mm7", "eax"
	);
#endif
}

void __declspec(naked) asm_deinterlace_chromaYUY2(const unsigned char *dstp, const unsigned char *p,
								  const unsigned char *n, unsigned char *fmask,
								  unsigned char *dmask, int thresh, int dthresh, int row_size)
{
#if !defined(GCC__)
	static const __int64 Mask0 = 0xFFFF0000FFFF0000i64;
	static const __int64 Mask2 = 0x00000000000000FFi64;
	static const __int64 Mask3 = 0x0000000000FF0000i64;
	static const __int64 Mask4 = 0x0000000000FF00FFi64;
	__asm
	{
		push		ebp
		push		ebx
		push		esi
		push		edi

		mov			edx,[esp+4+16]	// dstp
		mov			ecx,[esp+8+16]	// p
		mov			ebx,[esp+12+16]	// n
		mov			eax,[esp+16+16] // fmaskp
		mov			esi,[esp+20+16] // dmaskp
		movd		mm0,[esp+24+16] // threshold
		movq		mm3,mm0
		psllq		mm3,32
		por			mm3,mm0
		movd		mm4,[esp+28+16] // dthreshold
		movq		mm5,mm4
		psllq		mm5,32
		por			mm5,mm4
		mov			ebp,[esp+32+16]	// row_size
xloop:
		pxor		mm4,mm4
		movd		mm0,[ecx]		// load p
		punpcklbw	mm0,mm4			// unpack
		movd		mm2,[edx]		// load dstp
		punpcklbw	mm2,mm4			// unpack
		movd		mm1,[ebx]		// load n
		punpcklbw	mm1,mm4			// unpack
		psubw		mm0,mm2			// pprev - curr = P
		psubw		mm1,mm2			// pnext - curr = N
		movq		mm6,Mask0
		pandn		mm6,mm0			// mm6 = 0 | P2 | 0 | P0
		pmaddwd		mm6,mm1			// mm6 = P2*N2  | P0*N0
		movq		mm0,mm6

		pcmpgtd		mm6,mm3			// mm6 = P2*N2<T | P0*N0<T
		movq		mm4,mm6			// save result
		pand		mm6,Mask2		// make 0x000000ff if P0*N0<T
		movq		mm7,mm6			// move it into result
		psrlq		mm4,32			// get P2*N2<T
		pand		mm4,Mask3		// make 0x00ff0000 if P2*N2<T
		por			mm7,mm4			// move it into the result
		movd		[eax],mm7		// save intermediate result

		pcmpgtd		mm0,mm5			// mm0 = P2*N2<DT | P0*N0<DT
		movq		mm4,mm0			// save result
		pand		mm0,Mask2		// make 0x000000ff if P0*N0<DT
		movq		mm7,mm0			// move it into result
		psrlq		mm4,32			// get P2*N2<DT
		pand		mm4,Mask3		// make 0x00ff0000 if P2*N2<DT
		por			mm7,mm4			// move it into the result
		movd		[esi],mm7		// save intermediate result

		pxor		mm4,mm4			// do chroma bytes now
		movd		mm0,[ecx]		// load p
		punpcklbw	mm0,mm4			// unpack
		movd		mm1,[ebx]		// load n
		punpcklbw	mm1,mm4			// unpack
		psubw		mm0,mm2			// pprev - curr = P
		psubw		mm1,mm2			// pnext - curr = N
		movq		mm6,Mask0
		pand		mm6,mm0			// mm6 = P3 | 0 | P1| 0
		pmaddwd		mm6,mm1			// mm6 = P3*N3  | P1*N1
		movq		mm0,mm6

		pcmpgtd		mm6,mm3			// mm6 = P3*N3<T | P1*N1<T
		movq		mm4,mm6			// save result
		pand		mm6,Mask4		// make 0x00ff00ff if P1*N1<T
		movq		mm7,mm6			// move it into result
		psrlq		mm4,32			// get P3*N3<T
		pand		mm4,Mask4		// make 0x00ff00ff if P3*N3<T
		por			mm7,mm4			// move it into the result
		movd		mm4,[eax]		// Pick up saved intermediate result
		por			mm4,mm7
		movd		[eax],mm4		// Save final result for fmask

		pcmpgtd		mm0,mm5			// mm0 = P3*N3<DT | P1*N1<DT
		movq		mm4,mm0			// save result
		pand		mm0,Mask4		// make 0x00ff00ff if P1*N1<DT
		movq		mm7,mm0			// move it into result
		psrlq		mm4,32			// get P3*N3<DT
		pand		mm4,Mask4		// make 0x00ff00ff if P3*N3<DT
		por			mm7,mm4			// move it into the result
		movd		mm4,[esi]		// Pick up saved intermediate result
		por			mm4,mm7
		movd		[esi],mm4		// Save final result for dmask

		add			edx,4
		add			ecx,4
		add			ebx,4
		add			eax,4
		add			esi,4
		dec			ebp
		jnz			xloop

		pop			edi
		pop			esi
		pop			ebx
		pop			ebp
    emms
		ret
	};	
#else
	static const __int64 Mask0 = 0xFFFF0000FFFF0000LL;
	static const __int64 Mask2 = 0x00000000000000FFLL;
	static const __int64 Mask3 = 0x0000000000FF0000LL;
	static const __int64 Mask4 = 0x0000000000FF00FFLL;
	__asm__ __volatile__
	(

	"	push	ebp\n"
	"	push	ebx\n"
	"	push	esi\n"
	"	push	edi\n"

	"	mov		edx,[esp+4+16]\n"
	"	mov		ecx,[esp+8+16]\n"
	"	mov		ebx,[esp+12+16]\n"
	"	mov		eax,[esp+16+16]\n"
	"	mov		esi,[esp+20+16]\n"
	"	movd	mm0,[esp+24+16]\n"
	"	movq	mm3,mm0\n"
	"	psllq	mm3,32\n"
	"	por		mm3,mm0\n"
	"	movd	mm4,[esp+28+16]\n"
	"	movq	mm5,mm4\n"
	"	psllq	mm5,32\n"
	"	por		mm5,mm4\n"
	"	mov		ebp,[esp+32+16]\n"
	"asm_deinterlace_chromaYUY2_xloop:\n"
	"	pxor	mm4,mm4\n"
	"	movd	mm0,[ecx]\n"
	"	punpcklbw	mm0,mm4\n"
	"	movd	mm2,[edx]\n"
	"	punpcklbw	mm2,mm4\n"
	"	movd	mm1,[ebx]\n"
	"	punpcklbw	mm1,mm4\n"
	"	psubw	mm0,mm2\n"
	"	psubw	mm1,mm2\n"
	"	movq	mm6,%[Mask0]\n"
	"	pandn	mm6,mm0\n"
	"	pmaddwd	mm6,mm1\n"
	"	movq	mm0,mm6\n"

	"	pcmpgtd	mm6,mm3\n"
	"	movq	mm4,mm6\n"
	"	pand	mm6,%[Mask2]\n"
	"	movq	mm7,mm6\n"
	"	psrlq	mm4,32\n"
	"	pand	mm4,%[Mask3]\n"
	"	por		mm7,mm4\n"
	"	movd	[eax],mm7\n"

	"	pcmpgtd	mm0,mm5\n"
	"	movq	mm4,mm0\n"
	"	pand	mm0,%[Mask2]\n"
	"	movq	mm7,mm0\n"
	"	psrlq	mm4,32\n"
	"	pand	mm4,%[Mask3]\n"
	"	por		mm7,mm4\n"
	"	movd	[esi],mm7\n"

	"	pxor	mm4,mm4\n"
	"	movd	mm0,[ecx]\n"
	"	punpcklbw	mm0,mm4\n"
	"	movd	mm1,[ebx]\n"
	"	punpcklbw	mm1,mm4\n"
	"	psubw	mm0,mm2\n"
	"	psubw	mm1,mm2\n"
	"	movq	mm6,%[Mask0]\n"
	"	pand	mm6,mm0\n"
	"	pmaddwd	mm6,mm1\n"
	"	movq	mm0,mm6\n"

	"	pcmpgtd	mm6,mm3\n"
	"	movq	mm4,mm6\n"
	"	pand	mm6,%[Mask4]\n"
	"	movq	mm7,mm6\n"
	"	psrlq	mm4,32\n"
	"	pand	mm4,%[Mask4]\n"
	"	por		mm7,mm4\n"
	"	movd	mm4,[eax]\n"
	"	por		mm4,mm7\n"
	"	movd	[eax],mm4\n"

	"	pcmpgtd	mm0,mm5\n"
	"	movq	mm4,mm0\n"
	"	pand	mm0,%[Mask4]\n"
	"	movq	mm7,mm0\n"
	"	psrlq	mm4,32\n"
	"	pand	mm4,%[Mask4]\n"
	"	por		mm7,mm4\n"
	"	movd	mm4,[esi]\n"
	"	por		mm4,mm7\n"
	"	movd	[esi],mm4\n"

	"	add		edx,4\n"
	"	add		ecx,4\n"
	"	add		ebx,4\n"
	"	add		eax,4\n"
	"	add		esi,4\n"
	"	dec		ebp\n"
	"	jnz		asm_deinterlace_chromaYUY2_xloop\n"

	"	pop		edi\n"
	"	pop		esi\n"
	"	pop		ebx\n"
	"	pop		ebp\n"
	"	emms\n"
	"	ret\n"

	:
	:	[Mask4] "rm" (Mask4),
		[Mask0] "rm" (Mask0),
		[Mask2] "rm" (Mask2),
		[Mask3] "rm" (Mask3)
	:	"mm5", "mm0", "mm1", "mm2", "mm3", "memory", "ebx", "ecx", "esi", "edx", "mm6", "edi", "mm4", "mm7", "eax"
	);
#endif
}
#pragma warning(default:4799)
#endif

#ifdef BLEND_MMX_BUILD
#pragma warning(disable:4799)
int __declspec(naked) blendYUY2(const unsigned char *dstp, const unsigned char *cprev,
								const unsigned char *cnext, unsigned char *finalp,
								unsigned char *dmaskp, int count)
{
#if !defined(GCC__)
	static const __int64 Mask1 = 0xFEFEFEFEFEFEFEFEi64;
	static const __int64 Mask2 = 0xFCFCFCFCFCFCFCFCi64;
	__asm
	{
		push ebp
		push ebx
		push edi
		push esi

		mov			edx,[esp+4+16]	// dstp
		mov			ecx,[esp+8+16]	// cprev
		mov			ebx,[esp+12+16]	// cnext
		mov			eax,[esp+16+16] // finalp
		mov			edi,[esp+20+16]	// dmaskp
		mov			ebp,[esp+24+16]	// count
		movq		mm7,Mask1
		movq		mm6,Mask2
xloop:
		mov			esi,[edi]
		or			esi,esi
		jnz			blend
		mov			esi,[edi+4]
		or			esi,esi
		jnz			blend
		movq		mm0,[edx]
		movq		[eax],mm0
		jmp			skip
blend:
		movq		mm0,[edx]		// load dstp
		pand		mm0,mm7

		movq		mm1,[ecx]		// load cprev
		pand		mm1,mm6

		movq		mm2,[ebx]		// load cnext
		pand		mm2,mm6

		psrlq		mm0,1
		psrlq		mm1,2
		psrlq       mm2,2
		paddusb		mm0,mm1
		paddusb		mm0,mm2
		movq		[eax],mm0
skip:
		add			edx,8
		add			ecx,8
		add			ebx,8
		add			eax,8
		add			edi,8
		dec			ebp
		jnz			xloop

		pop			esi
		pop			edi
		pop			ebx
		pop			ebp
		emms
		ret
	};
#else
	static const __int64 Mask1 = 0xFEFEFEFEFEFEFEFELL;
	static const __int64 Mask2 = 0xFCFCFCFCFCFCFCFCLL;
	__asm__ __volatile__
	(

	"	push	ebp\n"
	"	push	ebx\n"
	"	push	edi\n"
	"	push	esi\n"

	"	mov		edx,[esp+4+16]\n"
	"	mov		ecx,[esp+8+16]\n"
	"	mov		ebx,[esp+12+16]\n"
	"	mov		eax,[esp+16+16]\n"
	"	mov		edi,[esp+20+16]\n"
	"	mov		ebp,[esp+24+16]\n"
	"	movq	mm7,%[Mask1]\n"
	"	movq	mm6,%[Mask2]\n"
	"blendYUY2_xloop:\n"
	"	mov		esi,[edi]\n"
	"	or		esi,esi\n"
	"	jnz		blendYUY2_blend\n"
	"	mov		esi,[edi+4]\n"
	"	or		esi,esi\n"
	"	jnz		blendYUY2_blend\n"
	"	movq	mm0,[edx]\n"
	"	movq	[eax],mm0\n"
	"	jmp		blendYUY2_skip\n"
	"blendYUY2_blend:\n"
	"	movq	mm0,[edx]\n"
	"	pand	mm0,mm7\n"

	"	movq	mm1,[ecx]\n"
	"	pand	mm1,mm6\n"

	"	movq	mm2,[ebx]\n"
	"	pand	mm2,mm6\n"

	"	psrlq	mm0,1\n"
	"	psrlq	mm1,2\n"
	"	psrlq	mm2,2\n"
	"	paddusb	mm0,mm1\n"
	"	paddusb	mm0,mm2\n"
	"	movq	[eax],mm0\n"
	"blendYUY2_skip:\n"
	"	add		edx,8\n"
	"	add		ecx,8\n"
	"	add		ebx,8\n"
	"	add		eax,8\n"
	"	add		edi,8\n"
	"	dec		ebp\n"
	"	jnz		blendYUY2_xloop\n"

	"	pop		esi\n"
	"	pop		edi\n"
	"	pop		ebx\n"
	"	pop		ebp\n"
	"	emms\n"
	"	ret\n"

	:
	:	[Mask1] "rm" (Mask1),
		[Mask2] "rm" (Mask2)
	:	"mm0", "mm2", "mm1", "memory", "ebx", "ecx", "esi", "edx", "mm6", "edi", "mm7", "eax"
	);
#endif
}
#pragma warning(default:4799)
#endif

#ifdef INTERPOLATE_MMX_BUILD
#pragma warning(disable:4799)
int __declspec(naked) interpolateYUY2(const unsigned char *dstp, const unsigned char *cprev,
								const unsigned char *cnext, unsigned char *finalp,
								unsigned char *dmaskp, int count)
{
#if !defined(GCC__)
	static const __int64 Mask = 0xFEFEFEFEFEFEFEFEi64;
	__asm
	{
		push ebp
		push ebx
		push edi
		push esi

		mov			edx,[esp+4+16]	// dstp
		mov			ecx,[esp+8+16]	// cprev
		mov			ebx,[esp+12+16]	// cnext
		mov			eax,[esp+16+16] // finalp
		mov			edi,[esp+20+16]	// dmaskp
		mov			ebp,[esp+24+16]	// count
		movq		mm7,Mask
xloop:
		mov			esi,[edi]
		or			esi,esi
		jnz			blend
		mov			esi,[edi+4]
		or			esi,esi
		jnz			blend
		movq		mm0,[edx]
		movq		[eax],mm0
		jmp			skip
blend:
		movq		mm0,[ecx]		// load cprev
		pand		mm0,mm7

		movq		mm1,[ebx]		// load cnext
		pand		mm1,mm7

		psrlq		mm0,1
		psrlq       mm1,1
		paddusb		mm0,mm1
		movq		[eax],mm0
skip:
		add			edx,8
		add			ecx,8
		add			ebx,8
		add			eax,8
		add			edi,8
		dec			ebp
		jnz			xloop

		pop			esi
		pop			edi
		pop			ebx
		pop			ebp
		emms
		ret
	};
#else
	static const __int64 Mask = 0xFEFEFEFEFEFEFEFELL;
	__asm__ __volatile__
	(

	"	push	ebp\n"
	"	push	ebx\n"
	"	push	edi\n"
	"	push	esi\n"

	"	mov		edx,[esp+4+16]\n"
	"	mov		ecx,[esp+8+16]\n"
	"	mov		ebx,[esp+12+16]\n"
	"	mov		eax,[esp+16+16]\n"
	"	mov		edi,[esp+20+16]\n"
	"	mov		ebp,[esp+24+16]\n"
	"	movq	mm7,%[Mask]\n"
	"interpolateYUY2_xloop:\n"
	"	mov		esi,[edi]\n"
	"	or		esi,esi\n"
	"	jnz		interpolateYUY2_blend\n"
	"	mov		esi,[edi+4]\n"
	"	or		esi,esi\n"
	"	jnz		interpolateYUY2_blend\n"
	"	movq	mm0,[edx]\n"
	"	movq	[eax],mm0\n"
	"	jmp		interpolateYUY2_skip\n"
	"interpolateYUY2_blend:\n"
	"	movq	mm0,[ecx]\n"
	"	pand	mm0,mm7\n"

	"	movq	mm1,[ebx]\n"
	"	pand	mm1,mm7\n"

	"	psrlq	mm0,1\n"
	"	psrlq	mm1,1\n"
	"	paddusb	mm0,mm1\n"
	"	movq	[eax],mm0\n"
	"interpolateYUY2_skip:\n"
	"	add		edx,8\n"
	"	add		ecx,8\n"
	"	add		ebx,8\n"
	"	add		eax,8\n"
	"	add		edi,8\n"
	"	dec		ebp\n"
	"	jnz		interpolateYUY2_xloop\n"

	"	pop		esi\n"
	"	pop		edi\n"
	"	pop		ebx\n"
	"	pop		ebp\n"
	"	emms\n"
	"	ret\n"

	:
	:	[Mask] "rm" (Mask)
	:	"mm0", "mm1", "memory", "ebx", "ecx", "esi", "edx", "edi", "mm7", "eax"
	);
#endif
}
#pragma warning(default:4799)
#endif

