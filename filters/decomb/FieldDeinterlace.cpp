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

#include "windows.h"
#include "stdio.h"
#include "version.h"
#include "internal.h"
#include "utilities.h"
#include "fielddeinterlace.h"
#include "asmfuncsYV12.h"

extern void DrawString(PVideoFrame &dst, int x, int y, const char *s); 

bool FieldDeinterlace::IsCombed(IScriptEnvironment* env)
{
	int x, y;
	int *counts, box, boxy;
	const int CT = 15;
	int boxarraysize;

	if (full == true) return(true);

	fmaskp = (unsigned char *) fmask->GetReadPtr(PLANAR_Y);
	h = fmask->GetHeight(PLANAR_Y);
	w = fmask->GetRowSize(PLANAR_Y);
	dpitch = fmask->GetPitch(PLANAR_Y);

	// interlaced frame detection
	// use box size of x=4 by y=8
	boxarraysize = ((h>>3)+1) * ((w>>2)+1);
	if ((counts = (int *) malloc(boxarraysize << 2)) == 0)
	{
		env->ThrowError("FieldDeinterlace: malloc failure");
	}
	memset(counts, 0, boxarraysize << 2);

	p = fmaskp - dpitch;
	n = fmaskp + dpitch;
	for (y = 1; y < h - 1; y++)
	{
		boxy = (y >> 3) * (w >> 2);
		fmaskp += dpitch;
		p += dpitch;
		n += dpitch;
		for (x = 0; x < w; x ++)
		{
			box = boxy + (x >> 2);
			if (fmaskp[x] == 0xff && p[x] == 0xff && n[x] == 0xff)
			{
				counts[box]++;
			}
		}
	}

	for (x = 0; x < boxarraysize; x++)
	{
		if (counts[x] > CT)
		{
			free(counts);
			return(true);
		}
	}
	free(counts);
	return(false);
}

void FieldDeinterlace::MotionMap(int plane, IScriptEnvironment* env)
{
	int x, y;
	int val, dpitchtimes2;
	unsigned char *dminus1, *dplus1, *dplus2;
	unsigned char *fminus1, *fplus1, *fplus2;

	srcp_saved = srcp = (unsigned char *) src->GetReadPtr(plane);
	pitch = src->GetPitch(plane);
	dpitch = dmask->GetPitch(PLANAR_Y);
	dpitchtimes2 = 2 * dpitch;
	w = src->GetRowSize(plane);
	h = src->GetHeight(plane);
	fmaskp = fmaskp_saved;
	dmaskp = dmaskp_saved;
	dminus1 = dmaskp - dpitch;
	dplus1 = dmaskp + dpitch;
	dplus2 = dmaskp + dpitchtimes2;
	fminus1 = fmaskp - dpitch;
	fplus1 = fmaskp + dpitch;
	fplus2 = fmaskp + dpitchtimes2;

	p = srcp - pitch;
	n = srcp + pitch;
	if (plane == PLANAR_Y)
	{
		if (full == false) memset(fmaskp, 0, w);
		memset(dmaskp, 0xff, w);
	}
	for (y = 1; y < h - 1; y++)
	{
		srcp += pitch;
		p += pitch;
		n += pitch;
		if (plane == PLANAR_Y)
		{
			fmaskp += dpitch;
			dmaskp += dpitch;
#ifdef DEINTERLACE_MMX_BUILD
			if ((env->GetCPUFlags() & CPUF_MMX)) {
				if (!full)
					mmx_createmask_plane(srcp, p, n, fmaskp, dmaskp, threshold, dthreshold, w);
				else 
					mmx_createmask_plane_single(srcp, p, n, dmaskp, dthreshold, w);
			} else {
#endif
				for (x = 0; x < w; x++)
				{
					val = ((long)p[x] - (long)srcp[x]) * ((long)n[x] - (long)srcp[x]);
					if (full == false && val > threshold) fmaskp[x] = (unsigned char) 0xff;
					if (val > dthreshold) dmaskp[x] = (unsigned char) 0xff;
				}
#ifdef DEINTERLACE_MMX_BUILD
			}
#endif
		}
		else
		{
			fmaskp += dpitchtimes2;
			dmaskp += dpitchtimes2;
			dminus1 += dpitchtimes2;
			dplus1 += dpitchtimes2;
			dplus2 += dpitchtimes2;
			fminus1 += dpitchtimes2;
			fplus1 += dpitchtimes2;
			fplus2 += dpitchtimes2;
			for (x = 0; x < w; x++)
			{
				val = ((long)p[x] - (long)srcp[x]) * ((long)n[x] - (long)srcp[x]);
				if (full == false && chroma == true && val > threshold)
				{
					((unsigned short*)fmaskp)[x] = (unsigned short) 0xffff;
					((unsigned short*)fplus1)[x] = (unsigned short) 0xffff;
					if (y & 1) ((unsigned short*)fminus1)[x] = (unsigned short) 0xffff;
					else ((unsigned short*)fplus2)[x] = (unsigned short) 0xffff;
				}
				if (val > dthreshold)
				{
					((unsigned short*)dmaskp)[x] = (unsigned short) 0xffff;
					((unsigned short*)dplus1)[x] = (unsigned short) 0xffff;
					if (y & 1) ((unsigned short*)dminus1)[x] = (unsigned short) 0xffff;
					else ((unsigned short*)dplus2)[x] = (unsigned short) 0xffff;
				}
			}
		}
	}
	if (plane == PLANAR_Y)
	{
		fmaskp += dpitch;
		dmaskp += dpitch;
		if (full == false) memset(fmaskp, 0, w);
		memset(dmaskp, 0xff, w);
	}

	return;
}

void FieldDeinterlace::DeinterlacePlane(int plane, IScriptEnvironment* env)
{
	int x, y;
	unsigned char *cprev, *cnext;

#ifdef MAKEWRITABLEHACK
	MakeWritable2(&src, env, vi);
#else
	env->MakeWritable(&src);
#endif
	srcp_saved = srcp = (unsigned char *) src->GetWritePtr(plane);
	pitch = src->GetPitch(plane);
	w = src->GetRowSize(plane);
	h = src->GetHeight(plane);
	dmaskp = dmaskp_saved;

	if (blend)
	{
		// Blend cannot be done in place.
		const unsigned char *freeit;
#if defined(_MSC_VER)
		freeit = cprev = (unsigned char *) _aligned_malloc(pitch * h, 64);
#elif defined (__MINGW32__)
		freeit = cprev = (unsigned char *) __mingw_aligned_malloc(pitch * h, 64);
#else
#error memalign or something?
#endif

		if (freeit == 0)
			env->ThrowError("FieldDeinterlace: malloc failure");

		env->BitBlt(cprev, pitch, srcp, pitch, w, h);
		cnext = cprev + pitch;

		/* First line. */
		for (x = 0; x < w; x++)
		{
			srcp[x] = (unsigned char)
				      (((unsigned int) srcp[x] + (unsigned int) cnext[x]) >> 1);
		}
		srcp += pitch;
		dmaskp += dpitch;
		cnext += pitch;
		if (plane == PLANAR_Y)
		{
			for (y = 1; y < h - 1; y++)
			{
#ifdef DEINTERLACE_MMX_BUILD
				if ((env->GetCPUFlags() & CPUF_INTEGER_SSE)) {
					isse_blend_plane(srcp, cprev, cnext, dmaskp, w);
				} else {
#endif
					for (x = 0; x < w; x++)
					{
						if (dmaskp[x])
						{
							srcp[x] = (unsigned char)
									  ((((unsigned int) srcp[x]) >> 1) +
									   (((unsigned int) cprev[x] + (unsigned int) cnext[x]) >> 2));
						}
					}
#ifdef DEINTERLACE_MMX_BUILD
				}
#endif
				srcp += pitch;
				dmaskp += dpitch;
				cprev += pitch;
				cnext += pitch;
			}
		}
		else
		{
			for (y = 1; y < h - 1; y++)
			{
				for (x = 0; x < w; x++)
				{
					if (dmaskp[2*x])
					{
						srcp[x] = (unsigned char)
								  ((((unsigned int) srcp[x]) >> 1) +
								   (((unsigned int) cprev[x] + (unsigned int) cnext[x]) >> 2));
					}
				}
				srcp += pitch;
				dmaskp += 2*dpitch;
				cprev += pitch;
				cnext += pitch;
			}
		}
		/* Last line. */
		for (x = 0; x < w; x++)
		{
			srcp[x] = (unsigned char)
				      (((unsigned int) srcp[x] + (unsigned int) cprev[x]) >> 1);
		}
#if defined(_MSC_VER)
		_aligned_free((void *) freeit);
#elif defined (__MINGW32__)
		__mingw_aligned_free((void *) freeit);
#else
#error memalign or something?
#endif


	}
	else
	{
		cprev = srcp - pitch;
		cnext = srcp + pitch;
		if (plane == PLANAR_Y)
		{
			for (y = 0; y < h - 1; y++)
			{
				if (y&1)
				{
#ifdef DEINTERLACE_MMX_BUILD
					if ((env->GetCPUFlags() & CPUF_INTEGER_SSE)) {
						isse_interpolate_plane(srcp, cprev, cnext, dmaskp, w);
					} else {					
#endif
						for (x = 0; x < w; x++)
						{
							if (dmaskp[x])
							{
								srcp[x] = (unsigned char)
											(((unsigned int) cprev[x] + (unsigned int) cnext[x] + 1) >> 1);
							}
						}
#ifdef DEINTERLACE_MMX_BUILD
					}
#endif
				}
				srcp += pitch;
				dmaskp += dpitch;
				cprev += pitch;
				cnext += pitch;
			}
		}
		else
		{
			for (y = 0; y < h - 1; y++)
			{
				if (y&1)
				{
					for (x = 0; x < w; x++)
					{
						if (dmaskp[2*x])
						{
							srcp[x] = (unsigned char)
										(((unsigned int) cprev[x] + (unsigned int) cnext[x] +1) >> 1);
						}
					}
				}
				srcp += pitch;
				dmaskp += 2*dpitch;
				cprev += pitch;
				cnext += pitch;
			}
		}
	}
}

PVideoFrame __stdcall FieldDeinterlace::GetFrame(int n, IScriptEnvironment* env)
{
	if (vi.IsYV12()) return(GetFrameYV12(n, env));
	else if (vi.IsYUY2()) return(GetFrameYUY2(n, env));
	env->ThrowError("FieldDeinterlace: YUY2 or YV12 data only");
	return 0;	
}

PVideoFrame __stdcall FieldDeinterlace::GetFrameYV12(int n, IScriptEnvironment* env)
{
#ifndef DEINTERLACE_MMX_BUILD
	int y;
#endif
	unsigned int hint;

	src = child->GetFrame(n, env);
	full = full_saved;
	// Take orders from Telecide if he's giving them.
	if (GetHintingData((unsigned char *) src->GetReadPtr(PLANAR_Y), &hint) == false)
	{
		if (hint & PROGRESSIVE) return src;
		full = true;
	}

	/* Check for manual overrides of the deinterlacing. */
	overrides_p = overrides;
	force = 0;
	if (overrides_p != NULL)
	{
		while (*overrides_p < 0xffffffff)
		{
			if ((unsigned int) n >= *overrides_p && (unsigned int) n <= *(overrides_p+1) &&
				(*(overrides_p+2) == '+' || *(overrides_p+2) == '-'))
			{
				overrides_p += 2;
				force = *overrides_p;
				break;
			}
			overrides_p += 3;
		}
	}

	clean = true;
	if (force != '-')
	{
		/* If the frame is not forced as clean by user override, then
		   process it for deinterlacing. */

		/* Setup the motion maps: fmask for determining whether the frame is
		   combed (uses threshold), and dmask for the actual deinterlacing
		   (uses dthreshold). Note that only the Y plane is used for the
		   motion maps because it needs to hold only a binary indication of
		   combed versus noncombed for each pixel. */
		fmask = env->NewVideoFrame(vi);
		fmaskp_saved = fmaskp = fmask->GetWritePtr(PLANAR_Y);
		dpitch = fmask->GetPitch(PLANAR_Y);
		dmask = env->NewVideoFrame(vi);
		dmaskp_saved = dmaskp = dmask->GetWritePtr(PLANAR_Y);
		w = src->GetRowSize(PLANAR_Y);
		h = src->GetHeight(PLANAR_Y);

#ifndef DEINTERLACE_MMX_BUILD
		for (y = 0; y < h; y++)
		{
			memset(dmaskp + y * dpitch, 0, w);
			memset(fmaskp + y * dpitch, 0, w);
		}
#endif

		/* Calculate the motion maps. */
		MotionMap(PLANAR_Y, env);
		MotionMap(PLANAR_U, env);
		MotionMap(PLANAR_V, env);

		if (full == true || force == '+' || IsCombed(env) == true)
		{
			if (map == true)
			{
				// User wants to see the motion map.
				int x, y, w, h, pitch, dpitch;

				// Y plane
				env->MakeWritable(&src);
				srcp = (unsigned char *) src->GetWritePtr(PLANAR_Y);
				dmaskp = (unsigned char *) dmask->GetWritePtr(PLANAR_Y);
				pitch = src->GetPitch(PLANAR_Y);
				dpitch = dmask->GetPitch(PLANAR_Y);
				w = src->GetRowSize(PLANAR_Y);
				h = src->GetHeight(PLANAR_Y);
				for (y = 0; y < h; y+=2)
				{
					for (x = 0; x < w; x+=2)
					{
						if (dmaskp[x] || dmaskp[x+1] || (dmaskp+dpitch)[x] || (dmaskp+dpitch)[x+1])
						{
							srcp[x] = 235;
							srcp[x+1] = 235;
							(srcp+pitch)[x] = 235;
							(srcp+pitch)[x+1] = 235;
						}
						else
						{
							srcp[x] = (3 * 0x80 + srcp[x]) >> 2;
							srcp[x+1] = (3 * 0x80 + srcp[x+1]) >> 2;
							(srcp+pitch)[x] = (3 * 0x80 + (srcp+pitch)[x]) >> 2;
							(srcp+pitch)[x+1] = (3 * 0x80 + (srcp+pitch)[x+1]) >> 2;
						}
					}
					srcp += 2*pitch;
					dmaskp += 2*dpitch;
				}
				// U plane
				srcp = (unsigned char *) src->GetWritePtr(PLANAR_U);
				dmaskp = (unsigned char *) dmask->GetWritePtr(PLANAR_Y);
				pitch = src->GetPitch(PLANAR_U);
				dpitch = dmask->GetPitch(PLANAR_Y);
				w = src->GetRowSize(PLANAR_U);
				h = src->GetHeight(PLANAR_U);
				for (y = 0; y < h; y++)
				{
					for (x = 0; x < w; x++)
					{
						if (dmaskp[2*x] || dmaskp[2*x+1] || (dmaskp+dpitch)[2*x] || (dmaskp+dpitch)[2*x+1])
						{
							srcp[x] = 240;
						}
					}
					srcp += pitch;
					dmaskp += 2*dpitch;
				}
				// V plane
				srcp = (unsigned char *) src->GetWritePtr(PLANAR_V);
				dmaskp = (unsigned char *) dmask->GetWritePtr(PLANAR_Y);
				pitch = src->GetPitch(PLANAR_V);
				dpitch = dmask->GetPitch(PLANAR_Y);
				w = src->GetRowSize(PLANAR_V);
				h = src->GetHeight(PLANAR_V);
				for (y = 0; y < h; y++)
				{
					for (x = 0; x < w; x++)
					{
						if (dmaskp[2*x] || dmaskp[2*x+1] || (dmaskp+dpitch)[2*x] || (dmaskp+dpitch)[2*x+1])
						{
							srcp[x] = 16;
						}
					}
					srcp += pitch;
					dmaskp += 2*dpitch;
				}

				return src;
			}
		
			/* User wants all frames deinterlaced, or has forced deinterlacing
			   for this frame, or the frame is combed, so deinterlace. */
			DeinterlacePlane(PLANAR_Y, env);
			DeinterlacePlane(PLANAR_U, env);
			DeinterlacePlane(PLANAR_V, env);
			clean = false;
		}
	}

	// Show debugging info if enabled.
	if (show == true)
	{
#ifdef MAKEWRITABLEHACK
		MakeWritable2(&src, env, vi);
#else
		env->MakeWritable(&src);
#endif
		sprintf(buf, "FieldDeinterlace %s", VERSION);
		DrawString(src, 0, 0, buf);
		sprintf(buf, "Copyright 2003-2008 Donald Graft");
		DrawString(src, 0, 1, buf);
	}
	if (clean == true)
	{
		if (show == true)
		{
			if (force == '-')
				sprintf(buf, "%d: Clean frame [forced]", n);
			else
				sprintf(buf, "%d: Clean frame", n);
			DrawString(src, 0, 3, buf);
		}
		if (debug == true)
		{
			if (force == '-')
			{
				sprintf(buf,"FieldDeinterlace: frame %d: Clean frame [forced]\n", n);
				OutputDebugString(buf);
			}
			else
			{
				sprintf(buf,"FieldDeinterlace: frame %d: Clean frame\n", n);
				OutputDebugString(buf);
			}
		}
	}
	else
	{
		if (full == true)
		{
			if (show == true)
			{
				sprintf(buf, "%d: Deinterlacing all frames", n);
				DrawString(src, 0, 3, buf);
			}
			if (debug == true)
			{
				sprintf(buf,"FieldDeinterlace: %d: Deinterlacing all frames\n", n);
				OutputDebugString(buf);
			}
		}
		else
		{
			if (show == true)
			{
				if (force == '+')
					sprintf(buf,"%d: Combed frame! [forced]", n);
				else
					sprintf(buf,"%d: Combed frame!", n);
				DrawString(src, 0, 3, buf);
			}
			if (debug == true)
			{
				if (force == '+')
				{
					sprintf(buf,"FieldDeinterlace: frame %d: Combed frame! [forced]\n", n);
					OutputDebugString(buf);
				}
				else
				{
					sprintf(buf,"FieldDeinterlace: frame %d: Combed frame!\n", n);
					OutputDebugString(buf);
				}
			}
		}
	}

	return src;
}

AVSValue FieldDeinterlace::ConditionalIsCombed(int n, IScriptEnvironment* env) {
#ifndef DEINTERLACE_MMX_BUILD
        int y;
#endif
	full = false;
	if (vi.IsYV12()) {
		src = child->GetFrame(n, env);
		fmask = env->NewVideoFrame(vi);
		fmaskp_saved = fmaskp = fmask->GetWritePtr(PLANAR_Y);
		dpitch = fmask->GetPitch(PLANAR_Y);
		dmask = env->NewVideoFrame(vi);
		dmaskp_saved = dmaskp = dmask->GetWritePtr(PLANAR_Y);
		w = src->GetRowSize(PLANAR_Y);
		h = src->GetHeight(PLANAR_Y);

#ifndef DEINTERLACE_MMX_BUILD
		for (y = 0; y < h; y++)
		{
			memset(dmaskp + y * dpitch, 0, w);
			memset(fmaskp + y * dpitch, 0, w);
		}
#endif

		/* Calculate the motion maps. */
		MotionMap(PLANAR_Y, env);
		MotionMap(PLANAR_U, env);
		MotionMap(PLANAR_V, env);
		return AVSValue(IsCombed(env));
	} else {
		src = child->GetFrame(n, env);
		srcp_saved = srcp = (unsigned char *) src->GetReadPtr();
		full = full_saved;
		pitch = src->GetPitch();
		w = src->GetRowSize();
		h = src->GetHeight();

		fmask = env->NewVideoFrame(vi);
		fmaskp_saved = fmaskp = fmask->GetWritePtr();
		dpitch = fmask->GetPitch();

		dmask = env->NewVideoFrame(vi);
		dmaskp_saved = dmaskp = dmask->GetWritePtr();
		return AVSValue(MotionMask_YUY2(n, env));
	}
	return false;  // Should never be reached.
}

AVSValue __cdecl Create_FieldDeinterlace(AVSValue args, void* user_data, IScriptEnvironment* env)
{
	char path[1024];
	char buf[80], *p;
	bool full = true;
	int threshold = 20;
	int dthreshold = 7;
	bool map = false;
	bool blend = true;
	bool chroma = false;
	char ovr[255], *ovrp;
	bool show = false;
	bool debug = false;

	// Default parameter override processing.
	try
	{
		FILE *f;

		const char* plugin_dir = env->GetVar("$PluginDir$").AsString();
		strcpy(path, plugin_dir);
		strcat(path, "\\FieldDeinterlace.def");
		if ((f = fopen(path, "r")) != NULL)
		{
			while(fgets(buf, 80, f) != 0)
			{
				if (strncmp(buf, "full=", 5) == 0)
				{
					p = buf;
					while(*p++ != '=');
					if (*p == 't') full = true;
					else full = false;
				}
				if (strncmp(buf, "threshold=", 10) == 0)
				{
					p = buf;
					while(*p++ != '=');
					threshold = atoi(p);
				}
				if (strncmp(buf, "dthreshold=", 11) == 0)
				{
					p = buf;
					while(*p++ != '=');
					dthreshold = atoi(p);
				}
				if (strncmp(buf, "map=", 4) == 0)
				{
					p = buf;
					while(*p++ != '=');
					if (*p == 't') map = true;
					else map = false;
				}
				if (strncmp(buf, "blend=", 6) == 0)
				{
					p = buf;
					while(*p++ != '=');
					if (*p == 't') blend = true;
					else blend = false;
				}
				if (strncmp(buf, "chroma=", 7) == 0)
				{
					p = buf;
					while(*p++ != '=');
					if (*p == 't') chroma = true;
					else chroma = false;
				}
				ovrp = ovr;
				*ovrp = 0;
				if (strncmp(buf, "ovr=", 4) == 0)
				{
					p = buf;
					while(*p++ != '"');
					while(*p != '\n' && *p != '\r' && *p != 0 && *p != EOF && *p != '"') *ovrp++ = *p++;
					*ovrp = 0;
				}
				if (strncmp(buf, "show=", 5) == 0)
				{
					p = buf;
					while(*p++ != '=');
					if (*p == 't') show = true;
					else show = false;
				}
				if (strncmp(buf, "debug=", 6) == 0)
				{
					p = buf;
					while(*p++ != '=');
					if (*p == 't') debug = true;
					else debug = false;
				}
			}
			fclose(f);
		}
	}
	catch (...)
	{
		// plugin directory not set
		// probably using an older version avisynth
	}

	return new FieldDeinterlace(
		args[0].AsClip(),		// clip
		args[1].AsBool(full),   // full -- deinterlace all frames
		args[2].AsInt(threshold),		// threshold
		args[3].AsInt(dthreshold),		// dthreshold
		args[4].AsBool(map),			// show motion map
		args[5].AsBool(blend),	// blend rather than interpolate
		args[6].AsBool(chroma),	// deinterlace chroma planes
		args[7].AsString(ovr),	// overrides file
		args[8].AsBool(show),	// show debug info on frames
		args[9].AsBool(debug),  // debug mode
		env);
}

AVSValue __cdecl Create_IsCombed(AVSValue args, void* user_data, IScriptEnvironment* env)
{
	AVSValue cn;

	if (!args[0].AsClip()->GetVideoInfo().IsYUV())
		env->ThrowError("IsCombed: Only YUY2 and YV12 data supported.");

	try
	{
		cn = env->GetVar("current_frame");
	}
    catch(IScriptEnvironment::NotFound)
    {
		env->ThrowError("IsCombed: This form of the filter can only be used within ConditionalFilter");
	}
	int n = cn.AsInt();

	FieldDeinterlace *f = new FieldDeinterlace(args[0].AsClip(),false,args[1].AsInt(20),args[1].AsInt(7),false,false,true,"",false,false,env);

  AVSValue isCombed = f->ConditionalIsCombed(n,env);
	
	delete f;
	return isCombed;

}
