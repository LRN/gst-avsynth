/*
	Decimate plugin for Avisynth -- performs 1-in-N
	decimation on a stream of progressive frames, which are usually
	obtained from the output of my Telecide plugin for Avisynth.
	For each group of N successive frames, this filter deletes the
	frame that is most similar to its predecessor. Thus, duplicate
	frames coming out of Telecide can be removed using Decimate. This
	filter adjusts the frame rate of the clip as
	appropriate. Selection of the cycle size is selected by specifying
	a parameter to Decimate() in the Avisynth scipt.

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

	The author can be contacted at:
	Donald Graft
	neuron2@comcast.net
*/

#include <stdio.h>
#include <windows.h>
#include "internal.h"
#include "version.h"
#include "utilities.h"

#include "decimate.h"

void DrawStringYUY2(PVideoFrame &dst, int x, int y, const char *s);
 
void Decimate::DrawShowYUY2(PVideoFrame &src, int useframe, bool forced, int dropframe,
						double metric, int inframe, IScriptEnvironment* env)
{
	char buf[80];
	int start = (useframe / cycle) * cycle;

	if (show == true)
	{
#ifdef MAKEWRITABLEHACK
		MakeWritable2(&src, env, vi);
#else
		env->MakeWritable(&src);
#endif
		sprintf(buf, "Decimate %s", VERSION);
		DrawStringYUY2(src, 0, 0, buf);
		sprintf(buf, "Copyright 2003-2008 Donald Graft");
		DrawStringYUY2(src, 0, 1, buf);
		sprintf(buf,"%d: %3.2f", start, showmetrics[0]);
		DrawStringYUY2(src, 0, 3, buf);
		sprintf(buf,"%d: %3.2f", start + 1, showmetrics[1]);
		DrawStringYUY2(src, 0, 4, buf);
		sprintf(buf,"%d: %3.2f", start + 2, showmetrics[2]);
		DrawStringYUY2(src, 0, 5, buf);
		sprintf(buf,"%d: %3.2f", start + 3, showmetrics[3]);
		DrawStringYUY2(src, 0, 6, buf);
		sprintf(buf,"%d: %3.2f", start + 4, showmetrics[4]);
		DrawStringYUY2(src, 0, 7, buf);
		if (all_video_cycle == false)
		{
			sprintf(buf,"in frm %d, use frm %d", inframe, useframe);
			DrawStringYUY2(src, 0, 8, buf);
			if (forced == false)
				sprintf(buf,"chose %d, dropping", dropframe);
			else
				sprintf(buf,"chose %d, dropping, forced!", dropframe);
			DrawStringYUY2(src, 0, 9, buf);
		}
		else
		{
			sprintf(buf,"in frm %d", inframe);
			DrawStringYUY2(src, 0, 8, buf);
			sprintf(buf,"chose %d, decimating all-video cycle", dropframe);
			DrawStringYUY2(src, 0, 9, buf);
		}
	}
	if (debug)
	{	
		if (!(inframe%cycle))
		{
			sprintf(buf,"Decimate: %d: %3.2f\n", start, showmetrics[0]);
			OutputDebugString(buf);
			sprintf(buf,"Decimate: %d: %3.2f\n", start + 1, showmetrics[1]);
			OutputDebugString(buf);
			sprintf(buf,"Decimate: %d: %3.2f\n", start + 2, showmetrics[2]);
			OutputDebugString(buf);
			sprintf(buf,"Decimate: %d: %3.2f\n", start + 3, showmetrics[3]);
			OutputDebugString(buf);
			sprintf(buf,"Decimate: %d: %3.2f\n", start + 4, showmetrics[4]);
			OutputDebugString(buf);
		}
		if (all_video_cycle == false)
		{
			sprintf(buf,"Decimate: in frm %d useframe %d\n", inframe, useframe);
			OutputDebugString(buf);
			if (forced == false)
				sprintf(buf,"Decimate: chose %d, dropping\n", dropframe);
			else
				sprintf(buf,"Decimate: chose %d, dropping, forced!\n", dropframe);
			OutputDebugString(buf);
		}
		else
		{
			sprintf(buf,"Decimate: in frm %d\n", inframe);
			OutputDebugString(buf);
			sprintf(buf,"Decimate: chose %d, decimating all-video cycle\n", dropframe);
			OutputDebugString(buf);
		}
	}
}

PVideoFrame __stdcall Decimate::GetFrameYUY2(int inframe, IScriptEnvironment* env)
{
	int dropframe, useframe, nextfrm, w, h, x, y, pitch, dpitch;
	PVideoFrame src, next, dst;
	unsigned char *srcrp, *nextrp, *dstwp;
	double metric;
	char buf[255];

	if (mode == 0)
	{
		bool forced = false;
		int start;

		/* Normal decimation. Remove the frame most similar to its preceding frame. */
		/* Determine the correct frame to use and get it. */
		useframe = inframe + inframe / (cycle - 1);
		start = (useframe / cycle) * cycle;
		FindDuplicateYUY2((useframe / cycle) * cycle, &dropframe, &metric, &forced, env);
		if (useframe >= dropframe) useframe++;
		GETFRAME(useframe, src);
		if (show == true)
		{
#ifdef MAKEWRITABLEHACK
			MakeWritable2(&src, env, vi);
#else
			env->MakeWritable(&src);
#endif
			sprintf(buf, "Decimate %s", VERSION);
			DrawStringYUY2(src, 0, 0, buf);
			sprintf(buf, "Copyright 2003-2008 Donald Graft");
			DrawStringYUY2(src, 0, 1, buf);
			sprintf(buf,"%d: %3.2f", start, showmetrics[0]);
			DrawStringYUY2(src, 0, 3, buf);
			sprintf(buf,"%d: %3.2f", start + 1, showmetrics[1]);
			DrawStringYUY2(src, 0, 4, buf);
			sprintf(buf,"%d: %3.2f", start + 2, showmetrics[2]);
			DrawStringYUY2(src, 0, 5, buf);
			sprintf(buf,"%d: %3.2f", start + 3, showmetrics[3]);
			DrawStringYUY2(src, 0, 6, buf);
			sprintf(buf,"%d: %3.2f", start + 4, showmetrics[4]);
			DrawStringYUY2(src, 0, 7, buf);
			sprintf(buf,"in frm %d, use frm %d", inframe, useframe);
			DrawStringYUY2(src, 0, 8, buf);
			sprintf(buf,"dropping frm %d%s", dropframe, last_forced == true ? ", forced!" : "");
			DrawStringYUY2(src, 0, 9, buf);
		}
		if (debug)
		{	
			if (!(inframe % cycle))
			{
				sprintf(buf,"Decimate: %d: %3.2f\n", start, showmetrics[0]);
				OutputDebugString(buf);
				sprintf(buf,"Decimate: %d: %3.2f\n", start + 1, showmetrics[1]);
				OutputDebugString(buf);
				sprintf(buf,"Decimate: %d: %3.2f\n", start + 2, showmetrics[2]);
				OutputDebugString(buf);
				sprintf(buf,"Decimate: %d: %3.2f\n", start + 3, showmetrics[3]);
				OutputDebugString(buf);
				sprintf(buf,"Decimate: %d: %3.2f\n", start + 4, showmetrics[4]);
				OutputDebugString(buf);
			}
			sprintf(buf,"Decimate: in frm %d, use frm %d\n", inframe, useframe);
			OutputDebugString(buf);
			sprintf(buf,"Decimate: dropping frm %d%s\n", dropframe, last_forced == true ? ", forced!" : "");
			OutputDebugString(buf);
		}
	    return src;
	}
	else if (mode == 1)
	{
		bool forced = false;
		int start = (inframe / cycle) * cycle;
		unsigned int hint, film = 1;

		GETFRAME(inframe, src);
	    srcrp = (unsigned char *) src->GetReadPtr();
		if (GetHintingData(srcrp, &hint) == false)
		{
			film = hint & PROGRESSIVE;
//			if (film) OutputDebugString("film\n");
//			else OutputDebugString("video\n");
		}

		/* Find the most similar frame as above but replace it with a blend of
		   the preceding and following frames. */
		num_frames_hi = vi.num_frames;
		FindDuplicateYUY2((inframe / cycle) * cycle, &dropframe, &metric, &forced, env);
		if (!film || inframe != dropframe || (threshold && metric > threshold))
		{
			if (show == true)
			{
#ifdef MAKEWRITABLEHACK
				MakeWritable2(&src, env, vi);
#else
				env->MakeWritable(&src);
#endif
				sprintf(buf, "Decimate %s", VERSION);
				DrawStringYUY2(src, 0, 0, buf);
				sprintf(buf, "Copyright 2003-2008 Donald Graft");
				DrawStringYUY2(src, 0, 1, buf);
				sprintf(buf,"%d: %3.2f", start, showmetrics[0]);
				DrawStringYUY2(src, 0, 3, buf);
				sprintf(buf,"%d: %3.2f", start + 1, showmetrics[1]);
				DrawStringYUY2(src, 0, 4, buf);
				sprintf(buf,"%d: %3.2f", start + 2, showmetrics[2]);
				DrawStringYUY2(src, 0, 5, buf);
				sprintf(buf,"%d: %3.2f", start + 3, showmetrics[3]);
				DrawStringYUY2(src, 0, 6, buf);
				sprintf(buf,"%d: %3.2f", start + 4, showmetrics[4]);
				DrawStringYUY2(src, 0, 7, buf);
				sprintf(buf,"infrm %d", inframe);
				DrawStringYUY2(src, 0, 8, buf);
				if (last_forced == false)
					sprintf(buf,"chose %d, passing through", dropframe);
				else
					sprintf(buf,"chose %d, passing through, forced!", dropframe);
				DrawStringYUY2(src, 0, 9, buf);
			}
			if (debug)
			{
				if (!(inframe % cycle))
				{
					sprintf(buf,"Decimate: %d: %3.2f\n", start, showmetrics[0]);
					OutputDebugString(buf);
					sprintf(buf,"Decimate: %d: %3.2f\n", start + 1, showmetrics[1]);
					OutputDebugString(buf);
					sprintf(buf,"Decimate: %d: %3.2f\n", start + 2, showmetrics[2]);
					OutputDebugString(buf);
					sprintf(buf,"Decimate: %d: %3.2f\n", start + 3, showmetrics[3]);
					OutputDebugString(buf);
					sprintf(buf,"Decimate: %d: %3.2f\n", start + 4, showmetrics[4]);
					OutputDebugString(buf);
				}
				sprintf(buf,"Decimate: in frm %d\n", inframe);
				OutputDebugString(buf);
				if (last_forced == false)
					sprintf(buf,"Decimate: chose %d, passing through\n", dropframe);
				else
					sprintf(buf,"Decimate: chose %d, passing through, forced!\n", dropframe);
				OutputDebugString(buf);
			}
			return src;
		}
		if (inframe < vi.num_frames - 1)
			nextfrm = inframe + 1;
		else
			nextfrm = vi.num_frames - 1;
		if (debug)
		{
			if (!(inframe % cycle))
			{
				sprintf(buf,"Decimate: %d: %3.2f\n", start, showmetrics[0]);
				OutputDebugString(buf);
				sprintf(buf,"Decimate: %d: %3.2f\n", start + 1, showmetrics[1]);
				OutputDebugString(buf);
				sprintf(buf,"Decimate: %d: %3.2f\n", start + 2, showmetrics[2]);
				OutputDebugString(buf);
				sprintf(buf,"Decimate: %d: %3.2f\n", start + 3, showmetrics[3]);
				OutputDebugString(buf);
				sprintf(buf,"Decimate: %d: %3.2f\n", start + 4, showmetrics[4]);
				OutputDebugString(buf);
			}
			sprintf(buf,"Decimate: in frm %d\n", inframe);
			OutputDebugString(buf);
			if (last_forced == false)
				sprintf(buf,"Decimate: chose %d, blending %d and %d\n", dropframe, inframe, nextfrm);
			else
				sprintf(buf,"Decimate: chose %d, blending %d and %d, forced!\n", dropframe, inframe, nextfrm);
			OutputDebugString(buf);
		}
		GETFRAME(nextfrm, next);
		dst = env->NewVideoFrame(vi);
		pitch = src->GetPitch();
		dpitch = dst->GetPitch();
		w = src->GetRowSize();
		h = src->GetHeight();
		
	    nextrp = (unsigned char *) next->GetReadPtr();
	    dstwp = (unsigned char *) dst->GetWritePtr();
		for (y = 0; y < h; y++)
		{
			for (x = 0; x < w; x++)
			{
				dstwp[x] = ((int)srcrp[x] + (int)nextrp[x]) >> 1;
			}
			srcrp += pitch;
			nextrp += pitch;
			dstwp += dpitch;
		}
		if (show == true)
		{
#ifdef MAKEWRITABLEHACK
			MakeWritable2(&dst, env, vi);
#else
			env->MakeWritable(&dst);
#endif
			sprintf(buf, "Decimate %s", VERSION);
			DrawStringYUY2(dst, 0, 0, buf);
			sprintf(buf, "Copyright 2003-2008 Donald Graft");
			DrawStringYUY2(dst, 0, 1, buf);
			sprintf(buf,"%d: %3.2f", start, showmetrics[0]);
			DrawStringYUY2(dst, 0, 3, buf);
			sprintf(buf,"%d: %3.2f", start + 1, showmetrics[1]);
			DrawStringYUY2(dst, 0, 4, buf);
			sprintf(buf,"%d: %3.2f", start + 2, showmetrics[2]);
			DrawStringYUY2(dst, 0, 5, buf);
			sprintf(buf,"%d: %3.2f", start + 3, showmetrics[3]);
			DrawStringYUY2(dst, 0, 6, buf);
			sprintf(buf,"%d: %3.2f", start + 4, showmetrics[4]);
			DrawStringYUY2(dst, 0, 7, buf);
			sprintf(buf,"infrm %d", inframe);
			DrawStringYUY2(dst, 0, 8, buf);
			if (last_forced == false)
				sprintf(buf,"chose %d, blending %d and %d",dropframe, inframe, nextfrm);
			else
				sprintf(buf,"chose %d, blending %d and %d, forced!", dropframe, inframe, nextfrm);
			DrawStringYUY2(dst, 0, 9, buf);
		}
		return dst;
	}
	else if (mode == 2)
	{
		bool forced = false;

		/* Delete the duplicate in the longest string of duplicates. */
		useframe = inframe + inframe / (cycle - 1);
		FindDuplicate2YUY2((useframe / cycle) * cycle, &dropframe, &forced, env);
		if (useframe >= dropframe) useframe++;
		GETFRAME(useframe, src);
		if (show == true)
		{
			int start = (useframe / cycle) * cycle;

#ifdef MAKEWRITABLEHACK
			MakeWritable2(&src, env, vi);
#else
			env->MakeWritable(&src);
#endif
			sprintf(buf, "Decimate %s", VERSION);
			DrawStringYUY2(src, 0, 0, buf);
			sprintf(buf, "Copyright 2003-2008 Donald Graft");
			DrawStringYUY2(src, 0, 1, buf);
			sprintf(buf,"in frm %d, use frm %d", inframe, useframe);
			DrawStringYUY2(src, 0, 3, buf);
			sprintf(buf,"%d: %3.2f (%s)", start, showmetrics[0],
					Dshow[0] ? "new" : "dup");
			DrawStringYUY2(src, 0, 4, buf);
			sprintf(buf,"%d: %3.2f (%s)", start + 1, showmetrics[1],
					Dshow[1] ? "new" : "dup");
			DrawStringYUY2(src, 0, 5, buf);
			sprintf(buf,"%d: %3.2f (%s)", start + 2, showmetrics[2],
					Dshow[2] ? "new" : "dup");
			DrawStringYUY2(src, 0, 6, buf);
			sprintf(buf,"%d: %3.2f (%s)", start + 3, showmetrics[3],
					Dshow[3] ? "new" : "dup");
			DrawStringYUY2(src, 0, 7, buf);
			sprintf(buf,"%d: %3.2f (%s)", start + 4, showmetrics[4],
					Dshow[4] ? "new" : "dup");
			DrawStringYUY2(src, 0, 8, buf);
			sprintf(buf,"Dropping frm %d%s", dropframe, last_forced == true ? " forced!" : "");
			DrawStringYUY2(src, 0, 9, buf);
		}
		if (debug)
		{	
			sprintf(buf,"Decimate: inframe %d useframe %d\n", inframe, useframe);
			OutputDebugString(buf);
		}
	    return src;
	}
	else if (mode == 3)
	{
		bool forced = false;

		/* Decimate by removing a duplicate from film cycles and doing a
		   blend rate conversion on the video cycles. */
		if (cycle != 5)	env->ThrowError("Decimate: mode=3 requires cycle=5");
		useframe = inframe + inframe / (cycle - 1);
		FindDuplicateYUY2((useframe / cycle) * cycle, &dropframe, &metric, &forced, env);
		/* Use hints from Telecide about film versus video. Also use the difference
		   metric of the most similar frame in the cycle; if it exceeds threshold,
		   assume it's a video cycle. */
		if (!(inframe % 4))
		{
			all_video_cycle = false;
			if (threshold && metric > threshold)
			{
				all_video_cycle = true;
			}
			if ((hints_invalid == false) &&
				(!(hints[0] & PROGRESSIVE) ||
				 !(hints[1] & PROGRESSIVE) ||
				 !(hints[2] & PROGRESSIVE) ||
				 !(hints[3] & PROGRESSIVE) ||
				 !(hints[4] & PROGRESSIVE)))
			{
				all_video_cycle = true;
			}
		}
		if (all_video_cycle == false)
		{
			/* It's film, so decimate in the normal way. */
			if (useframe >= dropframe) useframe++;
			GETFRAME(useframe, src);
			DrawShowYUY2(src, useframe, forced, dropframe, metric, inframe, env);
			return src;
		}
		else if ((inframe % 4) == 0)
		{
			/* It's a video cycle. Output the first frame of the cycle. */
			GETFRAME(useframe, src);
			DrawShowYUY2(src, 0, forced, dropframe, metric, inframe, env);
			return src;
		}
		else if ((inframe % 4) == 3)
		{
			/* It's a video cycle. Output the last frame of the cycle. */
			GETFRAME(useframe+1, src);
			DrawShowYUY2(src, 0, forced, dropframe, metric, inframe, env);
			return src;
		}
		else if ((inframe % 4) == 1 || (inframe % 4) == 2)
		{
			/* It's a video cycle. Make blends for the remaining frames. */
			if ((inframe % 4) == 1)
			{
				GETFRAME(useframe, src);
				if (useframe < num_frames_hi - 1)
					nextfrm = useframe + 1;
				else
					nextfrm = vi.num_frames - 1;
				GETFRAME(nextfrm, next);
			}
			else
			{
				GETFRAME(useframe + 1, src);
				nextfrm = useframe;
				GETFRAME(nextfrm, next);
			}
			dst = env->NewVideoFrame(vi);
			pitch = src->GetPitch();
			dpitch = dst->GetPitch();
			w = src->GetRowSize();
			h = src->GetHeight();
			
			srcrp = (unsigned char *) src->GetReadPtr();
			nextrp = (unsigned char *) next->GetReadPtr();
			dstwp = (unsigned char *) dst->GetWritePtr();
			for (y = 0; y < h; y++)
			{
				for (x = 0; x < w; x++)
				{
					dstwp[x] = ((int)srcrp[x] + (int)nextrp[x]) >> 1;
				}
				srcrp += pitch;
				nextrp += pitch;
				dstwp += dpitch;
			}
			DrawShowYUY2(dst, 0, forced, dropframe, metric, inframe, env);
			return dst;
		}
		return src;
	}
	env->ThrowError("Decimate: invalid mode option (0-3)");
	/* Avoid compiler warning. */
	return src;
}

void __stdcall Decimate::FindDuplicateYUY2(int frame, int *chosen, double *metric, bool *forced, IScriptEnvironment* env)
{
	int f;
	PVideoFrame store[MAX_CYCLE_SIZE+1];
	const unsigned char *storep[MAX_CYCLE_SIZE+1];
	const unsigned char *prev, *curr;
	int pitch;
	int row_size;
	int height;
	int x, y, lowest_index, div;
	unsigned int count[MAX_CYCLE_SIZE], lowest;
	unsigned int highest_sum;
	bool found;
#define BLKSIZE 32

	/* Only recalculate differences when a new set is needed. */
	if (frame == last_request)
	{
		*chosen = last_result;
		*metric = last_metric;
		return;
	}
	last_request = frame;

	/* Get cycle+1 frames starting at the one before the asked-for one. */
	for (f = 0; f <= cycle; f++)
	{
		GETFRAME(frame + f - 1, store[f]);
		storep[f] = store[f]->GetReadPtr();
		hints_invalid = GetHintingData((unsigned char *) storep[f], &hints[f]);
#if 0
		if (hints_invalid == false)
		{
			char b[80];
			sprintf(buf, "hints = %d\n", hints[f]);
			OutputDebugString(buf);
		}
#endif
	}

    pitch = store[0]->GetPitch();
    row_size = store[0]->GetRowSize();
    height = store[0]->GetHeight();
	xblocks = row_size / (2*BLKSIZE);
	if (xblocks % (2*BLKSIZE)) xblocks++;
	yblocks = height/BLKSIZE;
	if (yblocks % BLKSIZE) yblocks++;

	/* Compare each frame to its predecessor. */
	for (f = 1; f <= cycle; f++)
	{
		prev = storep[f-1];
		curr = storep[f];
		for (y = 0; y < yblocks; y++)
		{
			for (x = 0; x < xblocks; x++)
			{
				sum[y*xblocks+x] = 0;
			}
		}
		for (y = 0; y < height; y++)
		{
			for (x = 0; x < row_size;)
			{
				sum[(y/BLKSIZE)*xblocks+x/(2*BLKSIZE)] += abs((int)curr[x] - (int)prev[x]);
				switch (quality)
				{
				case 0:
					x += 2;
					if (!(x%8)) x += 24;
					div = height * row_size / 8;
					div = BLKSIZE * BLKSIZE / 4;
					break;
				case 1:
					x++;
					if (!(x%8)) x += 24;
					div = height * row_size / 8;
					div = BLKSIZE * BLKSIZE / 4;
					break;
				case 2:
					x += 2;
					div = height * row_size / 2;
					div = BLKSIZE * BLKSIZE;
					break;
				case 3:
					x++;
					div = height * row_size / 2;
					div = BLKSIZE * BLKSIZE;
					break;
				}
			}
			prev += pitch;
			curr += pitch;
		}
		highest_sum = 0;
		for (y = 0; y < yblocks; y++)
		{
			for (x = 0; x < xblocks; x++)
			{
				if (sum[y*xblocks+x] > highest_sum)
				{
					highest_sum = sum[y*xblocks+x];
				}
			}
		}
		count[f-1] = highest_sum;
		showmetrics[f-1] = (count[f-1] * 100.0) / (235*div);
	}

	/* Find the frame with the lowest difference count but
	   don't use the artificial duplicate at frame 0. */
	if (frame == 0)
	{
		lowest = count[1];
		lowest_index = 1;
	}
	else
	{
		lowest = count[0];
		lowest_index = 0;
	}
	for (x = 1; x < cycle; x++)
	{
		if (count[x] < lowest)
		{
			lowest = count[x];
			lowest_index = x;
		}
	}
	last_result = frame + lowest_index;
	last_metric = (lowest * 100.0) / (235*div);
	*chosen = last_result;
	*metric = last_metric;

	overrides_p = overrides;
	found = false;
	last_forced = false;
	if (overrides_p != NULL)
	{
		while (*overrides_p < 0xffffffff)
		{
			if (*overrides_p >= (unsigned int) frame &&
				*overrides_p < (unsigned int) frame + cycle)
			{
				found = true;
				break;
			}
			overrides_p++;
		}
	}
	if (found == true)
	{
		*chosen = last_result = *overrides_p;
		*forced = last_forced = true;
	}
}

void __stdcall Decimate::FindDuplicate2YUY2(int frame, int *chosen, bool *forced, IScriptEnvironment* env)
{
	int f, g, fsum, bsum, highest, highest_index;
	PVideoFrame store[MAX_CYCLE_SIZE+1];
	const unsigned char *storep[MAX_CYCLE_SIZE+1];
	const unsigned char *prev, *curr;
	int x, y, tmp;
	double lowest;
	unsigned int lowest_index, div;
	char buf[255];
	unsigned int highest_sum;
	bool found;
#define BLKSIZE 32

	/* Only recalculate differences when a new cycle is started. */
	if (frame == last_request)
	{
		*chosen = last_result;
		*forced = last_forced;
		return;
	}
	last_request = frame;

	if (firsttime == true || frame == 0)
	{
		firsttime = false;
		for (f = 0; f < MAX_CYCLE_SIZE; f++) Dprev[f] = -1;
		GETFRAME(frame, store[0]);
//		store[0] = child->GetFrame(frame, env);
		storep[0] = store[0]->GetReadPtr();

		for (f = 1; f <= cycle; f++)
		{
			GETFRAME(frame + f - 1, store[f]);
//			if (f >= num_frames_hi - 1)
//				store[f] = child->GetFrame(num_frames_hi - 1, env);
//			else
//				store[f] = child->GetFrame(frame + f - 1, env);
			storep[f] = store[f]->GetReadPtr();
		}

		pitch = store[0]->GetPitch();
		row_size = store[0]->GetRowSize();
		height = store[0]->GetHeight();
		xblocks = row_size / (2*BLKSIZE);
		if (xblocks % (2*BLKSIZE)) xblocks++;
		yblocks = height/BLKSIZE;
		if (yblocks % BLKSIZE) yblocks++;

		/* Compare each frame to its predecessor. Subsample the frame by
		   8 in both the x and y dimensions for speed. */
		for (f = 1; f <= cycle; f++)
		{
			prev = storep[f-1];
			curr = storep[f];
			for (y = 0; y < yblocks; y++)
			{
				for (x = 0; x < xblocks; x++)
				{
					sum[y*xblocks+x] = 0;
				}
			}
			for (y = 0; y < height; y++)
			{
				for (x = 0; x < row_size;)
				{
					sum[(y/BLKSIZE)*xblocks+x/(2*BLKSIZE)] += abs((int)curr[x] - (int)prev[x]);
					switch (quality)
					{
					case 0:
						x += 2;
						if (!(x%8)) x += 24;
						div = BLKSIZE * BLKSIZE / 4;
						break;
					case 1:
						x++;
						if (!(x%8)) x += 24;
						div = BLKSIZE * BLKSIZE / 4;
						break;
					case 2:
						x += 2;
						div = BLKSIZE * BLKSIZE;
						break;
					case 3:
						x++;
						div = BLKSIZE * BLKSIZE;
						break;
					}
				}
				prev += pitch;
				curr += pitch;
			}
			highest_sum = 0;
			for (y = 0; y < yblocks; y++)
			{
				for (x = 0; x < xblocks; x++)
				{
					if (sum[y*xblocks+x] > highest_sum)
					{
						highest_sum = sum[y*xblocks+x];
					}
				}
			}
			metrics[f-1] = (highest_sum * 100.0) / (235*div);
		}

		Dcurr[0] = 1;
		for (f = 1; f < cycle; f++)
		{
			if (metrics[f] < threshold2) Dcurr[f] = 0;
			else Dcurr[f] = 1;
		}

		tmp = frame + cycle - 1;
		if (tmp >= num_frames_hi)
			tmp = num_frames_hi - 1;
		GETFRAME(tmp, store[0]);
		storep[0] = store[0]->GetReadPtr();

		if (debug)
		{
			sprintf(buf,"Decimate: %d: %3.2f %3.2f %3.2f %3.2f %3.2f\n",
					0, metrics[0], metrics[1], metrics[2], metrics[3], metrics[4]);
			OutputDebugString(buf);
		}
	}
 	else if (frame >= num_frames_hi - 1)
	{
		GETFRAME(num_frames_hi - 1, store[0]);
//		store[0] = child->GetFrame(num_frames_hi - 1, env);
		storep[0] = store[0]->GetReadPtr();
		for (f = 0; f < MAX_CYCLE_SIZE; f++) Dprev[f] = Dcurr[f];
		for (f = 0; f < MAX_CYCLE_SIZE; f++) Dcurr[f] = Dnext[f];
	}
	else
	{
		GETFRAME(frame + cycle - 1, store[0]);
//		store[0] = child->GetFrame(frame + cycle - 1, env);
		storep[0] = store[0]->GetReadPtr();
		for (f = 0; f < MAX_CYCLE_SIZE; f++) Dprev[f] = Dcurr[f];
		for (f = 0; f < MAX_CYCLE_SIZE; f++) Dcurr[f] = Dnext[f];
	}
	for (f = 0; f < MAX_CYCLE_SIZE; f++) Dshow[f] = Dcurr[f];
	for (f = 0; f < MAX_CYCLE_SIZE; f++) showmetrics[f] = metrics[f];

	for (f = 1; f <= cycle; f++)
	{
		GETFRAME(frame + f + cycle - 1, store[f]);
// 		if (frame + f + cycle >= num_frames_hi - 1)
//			store[f] = child->GetFrame(num_frames_hi - 1, env);
//		else
//			store[f] = child->GetFrame(frame + f + cycle - 1, env);
		storep[f] = store[f]->GetReadPtr();
	}

	/* Compare each frame to its predecessor. Subsample the frame by
	   8 in both the x and y dimensions for speed. */
	for (f = 1; f <= cycle; f++)
	{
		prev = storep[f-1];
		curr = storep[f];
		for (y = 0; y < yblocks; y++)
		{
			for (x = 0; x < xblocks; x++)
			{
				sum[y*xblocks+x] = 0;
			}
		}
		for (y = 0; y < height; y++)
		{
			for (x = 0; x < row_size;)
			{
				sum[(y/BLKSIZE)*xblocks+x/(2*BLKSIZE)] += abs((int)curr[x] - (int)prev[x]);
				switch (quality)
				{
				case 0:
					x += 2;
					if (!(x%8)) x += 24;
					div = BLKSIZE * BLKSIZE / 4;
					break;
				case 1:
					x++;
					if (!(x%8)) x += 24;
					div = BLKSIZE * BLKSIZE / 4;
					break;
				case 2:
					x += 2;
					div = BLKSIZE * BLKSIZE;
					break;
				case 3:
					x++;
					div = BLKSIZE * BLKSIZE;
					break;
				}
			}
			prev += pitch;
			curr += pitch;
		}
		highest_sum = 0;
		for (y = 0; y < yblocks; y++)
		{
			for (x = 0; x < xblocks; x++)
			{
				if (sum[y*xblocks+x] > highest_sum)
				{
					highest_sum = sum[y*xblocks+x];
				}
			}
		}
		metrics[f-1] = (highest_sum * 100.0) / (235*div);
	}

	/* Find the frame with the lowest difference count but
	   don't use the artificial duplicate at frame 0. */
	if (frame == 0)
	{
		lowest = metrics[1];
		lowest_index = 1;
	}
	else
	{
		lowest = metrics[0];
		lowest_index = 0;
	}
	for (f = 1; f < cycle; f++)
	{
		if (metrics[f] < lowest)
		{
			lowest = metrics[f];
			lowest_index = f;
		}
	}

	for (f = 0; f < cycle; f++)
	{
		if (metrics[f] < threshold2) Dnext[f] = 0;
		else Dnext[f] = 1;
	}

	if (debug)
	{
		sprintf(buf,"Decimate: %d: %3.2f %3.2f %3.2f %3.2f %3.2f\n",
		        frame + 5, metrics[0], metrics[1], metrics[2], metrics[3], metrics[4]);
		OutputDebugString(buf);
	}

	if (debug)
	{
		sprintf(buf,"Decimate: %d: %d %d %d %d %d\n",
		        frame, Dcurr[0], Dcurr[1], Dcurr[2], Dcurr[3], Dcurr[4]);
//		sprintf(buf,"Decimate: %d: %d %d %d %d %d - %d %d %d %d %d - %d %d %d %d %d\n",
//		        frame, Dprev[0], Dprev[1], Dprev[2], Dprev[3], Dprev[4],
//					   Dcurr[0], Dcurr[1], Dcurr[2], Dcurr[3], Dcurr[4],
//					   Dnext[0], Dnext[1], Dnext[2], Dnext[3], Dnext[4]);
		OutputDebugString(buf);
	}

	/* Find the longest strings of duplicates and decimate a frame from it. */
	highest = -1;
	for (f = 0; f < cycle; f++)
	{
		if (Dcurr[f] == 1)
		{
			bsum = 0;
			fsum = 0;
		}
		else
		{
			bsum = 1;
			g = f;
			while (--g >= 0)
			{
				if (Dcurr[g] == 0)
				{
					bsum++;
				}
				else break;
			}
			if (g < 0)
			{
				g = cycle;
				while (--g >= 0)
				{
					if (Dprev[g] == 0)
					{
						bsum++;
					}
					else break;
				}
			}
			fsum = 1;
			g = f;
			while (++g < cycle)
			{
				if (Dcurr[g] == 0)
				{
					fsum++;
				}
				else break;
			}
			if (g >= cycle)
			{
				g = -1;
				while (++g < cycle)
				{
					if (Dnext[g] == 0)
					{
						fsum++;
					}
					else break;
				}
			}
		}
		if (bsum + fsum > highest)
		{
			highest = bsum + fsum;
			highest_index = f;
		}
#if 0
		sprintf(buf,"Decimate: bsum %d, fsum %d\n", bsum, fsum);
		OutputDebugString(buf);
#endif
	}

	f = highest_index;
	if (Dcurr[f] == 1)
	{
		/* No duplicates were found! Act as if mode=0. */
		*chosen = last_result = frame + lowest_index;
	}
	else
	{
		// Choose the lowest metric in the longest string of duplicates, remaining
		// within the current cycle.
		lowest = showmetrics[f];
		lowest_index = f;
		f++;
		while (f < cycle && Dcurr[f] == 0)
		{
			if (showmetrics[f] < lowest)
			{
				lowest = showmetrics[f];
				lowest_index = f;
			}
			f++;
		}

		/* Prevent this decimated frame from being considered again. */ 
		Dcurr[lowest_index] = 1;
		*chosen = last_result = frame + lowest_index;
	}
	last_forced = false;
	if (debug)
	{
		sprintf(buf,"Decimate: dropping frame %d\n", last_result);
		OutputDebugString(buf);
	}

	overrides_p = overrides;
	found = false;
	if (overrides_p != NULL)
	{
		while (*overrides_p < 0xffffffff)
		{
			if (*overrides_p >= (unsigned int) frame &&
				*overrides_p < (unsigned int) frame + cycle)
			{
				found = true;
				break;
			}
			overrides_p++;
		}
	}
	if (found == true)
	{
		*chosen = last_result = *overrides_p;
		*forced = last_forced = true;
		if (debug)
		{
			sprintf(buf,"Decimate: overridden drop frame -- drop %d\n", last_result);
			OutputDebugString(buf);
		}
	}
}

AVSValue __cdecl Create_Decimate(AVSValue args, void* user_data, IScriptEnvironment* env)
{
	char path[1024];
	char buf[80], *p;
	int cycle = 5;
	int mode = 0;
	double threshold = 0.0;
	double threshold2 = 3.0;
	int quality = 2;
	char ovr[255], *ovrp;
	bool show = false;
	bool debug = false;

	try
	{
		FILE *f;

		const char* plugin_dir = env->GetVar("$PluginDir$").AsString();
		strcpy(path, plugin_dir);
		strcat(path, "\\Decimate.def");
		if ((f = fopen(path, "r")) != NULL)
		{
			while(fgets(buf, 80, f) != 0)
			{
				if (strncmp(buf, "cycle=", 6) == 0)
				{
					p = buf;
					while(*p++ != '=');
					cycle = atoi(p);
				}
				if (strncmp(buf, "mode=", 5) == 0)
				{
					p = buf;
					while(*p++ != '=');
					mode = atoi(p);
				}
				if (strncmp(buf, "threshold=", 10) == 0)
				{
					p = buf;
					while(*p++ != '=');
					threshold = atof(p);
				}
				if (strncmp(buf, "threshold2=", 11) == 0)
				{
					p = buf;
					while(*p++ != '=');
					threshold2 = atof(p);
				}
				if (strncmp(buf, "quality=", 8) == 0)
				{
					p = buf;
					while(*p++ != '=');
					quality = atoi(p);
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

    return new Decimate(args[0].AsClip(),			// clip
						args[1].AsInt(cycle),		// cycle size
						args[2].AsInt(mode),		// mode
						args[3].AsFloat(threshold),	// threshold
						args[4].AsFloat(threshold2),// threshold2
						args[5].AsInt(quality),		// quality level
						args[6].AsString(ovr),		// overrides file
						args[7].AsBool(show),		// show
						args[8].AsBool(debug),		// debug
						env);
}
