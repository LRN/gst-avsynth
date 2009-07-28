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

#define MAX_CYCLE_SIZE 25
#define MAX_BLOCKS 50

#define GETFRAME(g, fp) \
{ \
	int GETFRAMEf; \
	GETFRAMEf = (g); \
	if (GETFRAMEf < 0) GETFRAMEf = 0; \
	if (GETFRAMEf > num_frames_hi - 1) GETFRAMEf = num_frames_hi - 1; \
	(fp) = child->GetFrame(GETFRAMEf, env); \
}

/* Decimate 1-in-N implementation. */
class Decimate : public GenericVideoFilter
{
	int num_frames_hi, cycle, mode, quality;
	double threshold, threshold2;
	const char *ovr;
	int last_request, last_result;
	bool last_forced;
	double last_metric;
	double metrics[MAX_CYCLE_SIZE];
	double showmetrics[MAX_CYCLE_SIZE];
	int Dprev[MAX_CYCLE_SIZE];
	int Dcurr[MAX_CYCLE_SIZE];
	int Dnext[MAX_CYCLE_SIZE];
	int Dshow[MAX_CYCLE_SIZE];
	unsigned int hints[MAX_CYCLE_SIZE];
	bool hints_invalid;
	bool all_video_cycle;
	bool firsttime;
	int heightY, row_sizeY, pitchY;
	int heightUV, row_sizeUV, pitchUV;
	int pitch, row_size, height;
	int xblocks, yblocks;
	unsigned int *sum, div;
	bool debug, show;
	unsigned int *overrides, *overrides_p;
public:
	Decimate(PClip _child, int _cycle, int _mode, double _threshold, double _threshold2,
				int _quality, const char * _ovr, bool _show, bool _debug, IScriptEnvironment* env) :
		        GenericVideoFilter(_child), cycle(_cycle), mode(_mode), quality(_quality), threshold(_threshold),
				threshold2(_threshold2), ovr(_ovr), debug(_debug), show(_show)
	{
		FILE *f;
		int count = 0;
		char buf[80];
		unsigned int *p;

		/* Adjust frame rate and count. */
		if (!vi.IsYV12() && !vi.IsYUY2())
			env->ThrowError("Decimate: input must be YV12 or YUY2");
		if (cycle < 2 || cycle > 25)
			env->ThrowError("Decimate: cycle out of range (2-25)");
		if (quality < 0 || quality > 3)
			env->ThrowError("Decimate: quality out of range (0-3)");
		if (mode < 0 || mode > 3)
			env->ThrowError("Decimate: mode out of range (0-3)");
		if (mode == 0 || mode == 2 || mode == 3)
		{
			num_frames_hi = vi.num_frames;
			vi.num_frames = vi.num_frames * (cycle - 1) / cycle;
			vi.SetFPS(vi.fps_numerator * (cycle - 1), vi.fps_denominator * cycle);
		}
		last_request = -1;
		firsttime = true;
		sum = (unsigned int *) malloc(MAX_BLOCKS * MAX_BLOCKS * sizeof(unsigned int));
		if (sum == NULL)
			env->ThrowError("Decimate: cannot allocate needed memory");
		overrides = NULL;
		if (*ovr && (f = fopen(ovr, "r")) != NULL)
		{
			while(fgets(buf, 80, f) != 0)
			{
				count++;
			}
			fclose(f);
			if ((f = fopen(ovr, "r")) != NULL)
			{
				p = overrides = (unsigned int *) malloc((count + 1) * sizeof(unsigned int));
				memset(overrides, 0xff, (count + 1) * sizeof(unsigned int));
				while(fgets(buf, 80, f) != 0)
				{
					sscanf(buf, "%d", p);
					p++;
				}
				fclose(f);
			}
		}

		all_video_cycle = true;

		if (debug)
		{
			char b[80];
			sprintf(b, "Decimate %s by Donald Graft, Copyright 2003-2008\n", VERSION);
			OutputDebugString(b);
		}
	}
	~Decimate(void)
	{
		if (sum != NULL) free(sum);
		if (overrides != NULL) free(overrides);
	}

    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
	PVideoFrame __stdcall GetFrameYV12(int inframe, IScriptEnvironment* env);
	PVideoFrame __stdcall GetFrameYUY2(int inframe, IScriptEnvironment* env);
	void __stdcall DrawShow(PVideoFrame &src, int useframe, bool forced, int dropframe,
		                              double metric, int inframe, IScriptEnvironment* env);
	void __stdcall DrawShowYUY2(PVideoFrame &src, int useframe, bool forced, int dropframe,
		                              double metric, int inframe, IScriptEnvironment* env);
    void __stdcall FindDuplicate(int frame, int *chosen, double *metric, bool *forced, IScriptEnvironment* env);
    void __stdcall FindDuplicate2(int frame, int *chosen, bool *forced, IScriptEnvironment* env);
    void __stdcall FindDuplicateYUY2(int frame, int *chosen, double *metric, bool *forced, IScriptEnvironment* env);
    void __stdcall FindDuplicate2YUY2(int frame, int *chosen, bool *forced, IScriptEnvironment* env);
};

