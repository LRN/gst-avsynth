/*
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

		The author can be contacted at:
		Donald Graft
		neuron2@comcast.net
*/

#include <stdio.h>
#include <windows.h>
#include "internal.h"
#include "version.h"
#include "utilities.h"
#include "fielddeinterlace.h"

void DrawStringYUY2(PVideoFrame &dst, int x, int y, const char *s); 

bool FieldDeinterlace::MotionMask_YUY2(int frame, IScriptEnvironment* env)
{
	int x, y;
	int *counts, box, boxy;
#ifdef DEINTERLACE_MMX_BUILD
	void asm_deinterlaceYUY2(const unsigned char *srcp, const unsigned char *p,
								  const unsigned char *n, unsigned char *fmask,
								  unsigned char *dmask, int thresh, int dthresh, int row_size);
	void asm_deinterlace_chromaYUY2(const unsigned char *srcp, const unsigned char *p,
								  const unsigned char *n, unsigned char *fmask,
								  unsigned char *dmask, int thresh, int dthresh, int row_size);
#else
	int val, val2;
#endif
	const int CT = 15;
	int boxarraysize;
	char buf[80];

	p = srcp - pitch;
	n = srcp + pitch;
	memset(fmaskp, 0, w);
	for (x = 0; x < w; x += 4) *(int *)(dmaskp+x) = 0x00ff00ff;

	for (y = 1; y < h - 1; y++)
	{
		fmaskp += dpitch;
		dmaskp += dpitch;
		srcp += pitch;
		p += pitch;
		n += pitch;
#ifdef DEINTERLACE_MMX_BUILD
		if (chroma)
			asm_deinterlace_chromaYUY2(srcp, p, n, fmaskp, dmaskp, threshold+1, dthreshold+1, w/4);
		else
			asm_deinterlaceYUY2(srcp, p, n, fmaskp, dmaskp, threshold+1, dthreshold+1, w/4);
#else
		for (x = 0; x < w; x += 4)
		{
			val = ((long)p[x] - (long)srcp[x]) * ((long)n[x] - (long)srcp[x]);
			if (val > threshold)
				*(int *)(fmaskp+x) = 0x000000ff;
			else
				*(int *)(fmaskp+x) = 0;
			if (val > dthreshold)
				*(int *)(dmaskp+x) = 0x000000ff;
			else
				*(int *)(dmaskp+x) = 0;

			val = ((long)p[x+2] - (long)srcp[x+2]) * ((long)n[x+2] - (long)srcp[x+2]);
			if (val > threshold)
				*(int *)(fmaskp+x) |= 0x00ff0000;
			if (val > dthreshold)
				*(int *)(dmaskp+x) |= 0x00ff0000;

			val  = ((long)p[x+1] - (long)srcp[x+1]) * ((long)n[x+1] - (long)srcp[x+1]);
			val2 = ((long)p[x+3] - (long)srcp[x+3]) * ((long)n[x+3] - (long)srcp[x+3]);
			if (val > dthreshold || val2 > dthreshold)
				*(int *)(dmaskp+x) |= 0x00ff00ff;
			if (chroma)
			{
				if (val > threshold || val2 > threshold)
					*(int *)(fmaskp+x) |= 0x00ff00ff;
			}
		}
#endif
	}
#ifdef DEINTERLACE_MMX_BUILD
#if !defined(GCC__)
		__asm emms;
#else
		__asm("emms");
#endif
#endif
	fmaskp += dpitch;
	memset(fmaskp, 0, w);
	dmaskp += dpitch;
	for (x = 0; x < w; x += 4) *(int *)(dmaskp+x) = 0x00ff00ff;

	if (full == true) return(true);

	// interlaced frame detection
	boxarraysize = ((h+8)>>3) * ((w+2)>>3);
	if ((counts = (int *) malloc(boxarraysize << 2)) == 0)
	{
		env->ThrowError("FieldDeinterlace: malloc failure");
	}
	memset(counts, 0, boxarraysize << 2);

	fmaskp = fmaskp_saved;
	p = fmaskp - dpitch;
	n = fmaskp + dpitch;
	for (y = 1; y < h - 1; y++)
	{
		boxy = (y >> 3) * (w >> 3);
		fmaskp += dpitch;
		p += dpitch;
		n += dpitch;
		for (x = 0; x < w; x += 4)
		{
			box =  boxy + (x >> 3);
			if (fmaskp[x] == 0xff && p[x] == 0xff && n[x] == 0xff)
			{
				counts[box]++;
			}

			if (fmaskp[x+2] == 0xff && p[x+2] == 0xff && n[x+2] == 0xff)
			{
				counts[box]++;
			}
		}
	}

	for (x = 0; x < boxarraysize; x++)
	{
		if (counts[x] > CT)
		{
			if (debug == true)
			{
				sprintf(buf,"FieldDeinterlace: %d: Combed frame!\n", frame);
				OutputDebugString(buf);
			}
			free(counts);
			return(true);
		}
	}
	free(counts);
	return(false);
}

PVideoFrame __stdcall FieldDeinterlace::GetFrameYUY2(int n, IScriptEnvironment* env)
{
	int x, y;
	int stat;
#ifdef BLEND_MMX_BUILD
int blendYUY2(const unsigned char *dstp, const unsigned char *cprev,
								const unsigned char *cnext, unsigned char *finalp,
								unsigned char *dmaskp, int count);
#else
	unsigned int p0;
#endif
#ifdef INTERPOLATE_MMX_BUILD
int interpolateYUY2(const unsigned char *dstp, const unsigned char *cprev,
								const unsigned char *cnext, unsigned char *finalp,
								unsigned char *dmaskp, int count);
#endif
	unsigned int p1, p2;
	const unsigned char *cprev, *cnext;
	char buf[255];
	unsigned int hint;

	src = child->GetFrame(n, env);
	srcp_saved = srcp = (unsigned char *) src->GetReadPtr();
	full = full_saved;
	// Take orders from Telecide if he's giving them.
	if (GetHintingData((unsigned char *) srcp, &hint) == false)
	{
		if (hint & PROGRESSIVE) return src;
		full = true;
	}
	pitch = src->GetPitch();
	w = src->GetRowSize();
	h = src->GetHeight();

	fmask = env->NewVideoFrame(vi);
	fmaskp_saved = fmaskp = fmask->GetWritePtr();
	dpitch = fmask->GetPitch();

	dmask = env->NewVideoFrame(vi);
	dmaskp_saved = dmaskp = dmask->GetWritePtr();

	/* Check for manual overrides of the deinterlacing. */
	overrides_p = overrides;
	stat = 0;
	if (overrides_p != NULL)
	{
		while (*overrides_p < 0xffffffff)
		{
			if ((unsigned int) n >= *overrides_p && (unsigned int) n <= *(overrides_p+1) &&
				(*(overrides_p+2) == '+' || *(overrides_p+2) == '-'))
			{
				overrides_p += 2;
				stat = *overrides_p;
				break;
			}
			overrides_p += 3;
		}
		if (stat == '-')
		{
			sprintf(buf,"FieldDeinterlace: frame %d: forcing frame as not combed\n", n);
			OutputDebugString(buf);
		}
		else if (stat == '+')
		{
			sprintf(buf,"FieldDeinterlace: frame %d: forcing frame as combed\n", n);
			OutputDebugString(buf);
		}
	}

	if ((stat == '-') || (MotionMask_YUY2(n, env) == false && full == false && stat != '+'))
	{
		if (show == true)
		{
#ifdef MAKEWRITABLEHACK
			MakeWritable2(&src, env, vi);
#else
			env->MakeWritable(&src);
#endif
			sprintf(buf, "FieldDeinterlace %s", VERSION);
			DrawStringYUY2(src, 0, 0, buf);
			sprintf(buf, "Copyright 2003-2008 Donald Graft");
			DrawStringYUY2(src, 0, 1, buf);
			if (stat == '-')
				sprintf(buf, "%d: Clean frame [forced]", n);
			else
				sprintf(buf, "%d: Clean frame", n);
			DrawStringYUY2(src, 0, 3, buf);
		}
		return src;  // clean frame; return original
	}

	if (map == true)
	{
		// User wants to see the motion map.
		env->MakeWritable(&src);
		srcp = (unsigned char *) src->GetWritePtr();
		dmaskp = dmaskp_saved;
		pitch = src->GetPitch();
		dpitch = dmask->GetPitch();
		for (y = 0; y < h; y++)
		{
			for (x = 0; x < w; x+=4)
			{
				if (!dmaskp[x])
				{
					srcp[x] = (3 * 0x80 + srcp[x]) >> 2;
					srcp[x+2] = (3 * 0x80 + srcp[x+2]) >> 2;
				}
				else
				{
					srcp[x] = 235;
					srcp[x+1] = 240;
					srcp[x+2] = 235;
					srcp[x+3] = 16;
				}
			}
			srcp += pitch;
			dmaskp += dpitch;
		}
		return src;
	}

	/* Frame is combed or user wants all frames, so deinterlace. */
	PVideoFrame final = env->NewVideoFrame(vi);

	srcp = srcp_saved;
	dmaskp = dmaskp_saved;
	finalp = final->GetWritePtr();
	cprev = srcp - pitch;
	cnext = srcp + pitch;
	if (blend)
	{
		/* First line. */
		for (x=0; x<w; x+=4)
		{
			p1 = *(int *)(srcp+x) & 0xfefefefe;
			p2 = *(int *)(cnext+x) & 0xfefefefe;
			*(int *)(finalp+x) = (p1>>1) + (p2>>1);
		}
		srcp += pitch;
		dmaskp += dpitch;
		finalp += dpitch;
		cprev += pitch;
		cnext += pitch;
		for (y = 1; y < h - 1; y++)
		{
#ifdef BLEND_MMX_BUILD
			blendYUY2(srcp, cprev, cnext, finalp, dmaskp, w/8);
#else
			for (x=0; x<w; x+=4)
			{
				if (dmaskp[x])
				{
					p0 = *(int *)(srcp+x) & 0xfefefefe;
					p1 = *(int *)(cprev+x) & 0xfcfcfcfc;
					p2 = *(int *)(cnext+x) & 0xfcfcfcfc;
					*(int *)(finalp+x) = (p0>>1) + (p1>>2) + (p2>>2);
				}
				else
				{
					*(int *)(finalp+x)=*(int *)(srcp+x);
				}
			}
#endif
			srcp += pitch;
			dmaskp += dpitch;
			finalp += dpitch;
			cprev += pitch;
			cnext += pitch;
		}
        /* Last line. */
		for (x=0; x<w; x+=4)
		{
			p1 = *(int *)(srcp+x) & 0xfefefefe;
			p2 = *(int *)(cprev+x) & 0xfefefefe;
			*(int *)(finalp+x) = (p1>>1) + (p2>>1);
		}
#ifdef BLEND_MMX_BUILD
#if !defined(GCC__)
		__asm emms;
#else
		__asm("emms");
#endif
#endif
	}
	else
	{
		for (y = 0; y < h - 1; y++)
		{
			if (!(y&1))
			{
				memcpy(finalp,srcp,w);
			}
			else
			{
#ifdef INTERPOLATE_MMX_BUILD
			interpolateYUY2(srcp, cprev, cnext, finalp, dmaskp, w/8);
#else
				for (x = 0; x < w; x += 4)
				{
					if (dmaskp[x])
					{
						p1 = *(int *)(cprev+x) & 0xfefefefe;
						p2 = *(int *)(cnext+x) & 0xfefefefe;
						*(int *)(finalp+x) = (p1>>1) + (p2>>1);
					}
					else
					{
						*(int *)(finalp+x) = *(int *)(srcp+x);
					}
				}
#endif
			}
			srcp += pitch;
			dmaskp += dpitch;
			finalp += dpitch;
			cprev += pitch;
			cnext += pitch;
		}
		/* Last line. */
		memcpy(finalp,srcp,w);
#ifdef INTERPOLATE_MMX_BUILD
#if !defined(GCC__)
		__asm emms;
#else
		__asm("emms");
#endif

#endif
	}

	if (show == true)
	{
		sprintf(buf, "FieldDeinterlace %s", VERSION);
		DrawStringYUY2(final, 0, 0, buf);
		sprintf(buf, "Copyright 2003-2008 Donald Graft");
		DrawStringYUY2(final, 0, 1, buf);
		if (full == true)
		{
			sprintf(buf, "Deinterlacing all frames");
			DrawStringYUY2(final, 0, 3, buf);
		}
		else
		{
			if (stat == '+')
				sprintf(buf,"%d: Combed frame! [forced]", n);
			else
				sprintf(buf,"%d: Combed frame!", n);
			DrawStringYUY2(final, 0, 3, buf);
		}
	}
	
	return final;
}
