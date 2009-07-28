/*
	Telecide plugin for Avisynth -- recovers original progressive
	frames from  telecined streams. The filter operates by matching
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
#include "internal.h"
#include "version.h"
#include "utilities.h"

#undef DEBUG_PATTERN_GUIDANCE

#undef WINDOWED_MATCH

#define MAX_CYCLE 6
#define BLKSIZE 24
#define BLKSIZE_TIMES2 (2 * BLKSIZE)
#define GUIDE_NONE 0
#define GUIDE_32 1
#define GUIDE_22 2
#define GUIDE_32322 3
#define AHEAD 0
#define BEHIND 1
#define POST_NONE 0
#define POST_METRICS 1
#define POST_FULL 2
#define POST_FULL_MAP 3
#define POST_FULL_NOMATCH 4
#define POST_FULL_NOMATCH_MAP 5
#define CACHE_SIZE 100000
#define P 0
#define C 1
#define N 2
#define PBLOCK 3
#define CBLOCK 4

#define NO_BACK 0
#define BACK_ON_COMBED 1
#define ALWAYS_BACK 2

struct CACHE_ENTRY
{
	unsigned int frame;
	unsigned int metrics[5];
	unsigned int chosen;
};

struct PREDICTION
{
	unsigned int metric;
	unsigned int phase;
	unsigned int predicted;
	unsigned int predicted_metric;
};

#define GETFRAME(g, fp) \
{ \
	int GETFRAMEf; \
	GETFRAMEf = (g); \
	if (GETFRAMEf < 0) GETFRAMEf = 0; \
	else if (GETFRAMEf > vi.num_frames - 1) GETFRAMEf = vi.num_frames - 1; \
	(fp) = child->GetFrame(GETFRAMEf, env); \
}

class Telecide : public GenericVideoFilter
{
	IScriptEnvironment* env;
	bool tff, chroma, blend, hints, show, debug;
	const char *ovr;
	float dthresh, gthresh, vthresh, vthresh_saved, bthresh;
	int y0, y1, guide, post, back, back_saved;
        unsigned int nt;
	int pitch, dpitch, pitchover2, pitchtimes4;
	int w, h, wover2, hover2, hplus1over2, hminus2;
  	int xblocks, yblocks;
#ifdef WINDOWED_MATCH
	unsigned int *matchc, *matchp, highest_matchc, highest_matchp;
#endif
	unsigned int *sumc, *sump, highest_sumc, highest_sump;
	int vmetric;
	unsigned int *overrides, *overrides_p;
	bool film, override, inpattern, found;
	int force;

	// Used by field matching.
	PVideoFrame fp, fc, fn, dst, final;
	unsigned char *fprp, *fcrp, *fcrp_saved, *fnrp;
	unsigned char *fprpU, *fcrpU, *fcrp_savedU, *fnrpU;
	unsigned char *fprpV, *fcrpV, *fcrp_savedV, *fnrpV;
	unsigned char *dstp, *finalp;
	unsigned char *dstpU, *dstpV;
	unsigned int chosen;
	unsigned int p, c, pblock, cblock, lowest, predicted, predicted_metric;
	unsigned int np, nc, npblock, ncblock, nframe;
	float mismatch;
	int pframe, x, y;
	PVideoFrame lc, lp;
	unsigned char *crp, *prp;
	unsigned char *crpU, *prpU;
	unsigned char *crpV, *prpV;
	bool hard;
	char status[80];

	// Metrics cache.
	struct CACHE_ENTRY *cache;

	// Pattern guidance data.
	int cycle;
	struct PREDICTION pred[MAX_CYCLE+1];

	// For output message formatting.
	char buf[255];

public:
	static AVSValue Create_Telecide(AVSValue args, void* user_data, IScriptEnvironment* env);
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
    void CalculateMetrics(int n, unsigned char *crp, unsigned char *crpU, unsigned char *crpV, 
									unsigned char *prp, unsigned char *prpU, unsigned char *prpV,
									IScriptEnvironment* env);
	void Show(PVideoFrame &dst, int frame);
	void Debug(int frame);

	Telecide(PClip _child, int _back, int _guide, float _gthresh, int _post, bool _chroma,
				float _vthresh, float _bthresh, float _dthresh,	bool _blend, int _nt, int _y0, int _y1, bool _hints,
				const char * _ovr, bool _show, bool _debug, IScriptEnvironment* _env) :
				GenericVideoFilter(_child), env(_env),
			    chroma(_chroma), blend(_blend), hints(_hints), show(_show), debug(_debug), ovr(_ovr), dthresh(_dthresh), gthresh(_gthresh), vthresh(_vthresh), bthresh(_bthresh), y0(_y0), y1(_y1), guide(_guide), post(_post), back(_back), nt(_nt)
	{
		int i;
		FILE *f;
		int count;
		char *d, *dsaved;
		unsigned int *p, *x;
		if (!vi.IsYUY2() && !vi.IsYV12())
			env->ThrowError("Telecide: YUY2 or YV12 data only");
		if (y0 < 0 || y1 < 0 || y0 > y1)
			env->ThrowError("Telecide: bad y0/y1 values");
		// Get the field order from Avisynth.
		tff = (_child->GetParity(0) == 0 ? false : true);	
		if (debug)
		{
			sprintf(buf, "Telecide %s by Donald A. Graft, Copyright 2003-2008\n", VERSION);
			OutputDebugString(buf);
		}
		vi.SetFieldBased(false);
		back_saved = back;

		// Set up pattern guidance.
		cache = (struct CACHE_ENTRY *) malloc(CACHE_SIZE * sizeof(struct CACHE_ENTRY));
		if (cache == NULL)
			env->ThrowError("Telecide: cannot allocate memory");
		for (i = 0; i < CACHE_SIZE; i++)
		{
			cache[i].frame = 0xffffffff;
			cache[i].chosen = 0xff;
		}

		if (guide == GUIDE_32)
		{
			// 24fps to 30 fps telecine.
			cycle = 5;
		}
		if (guide == GUIDE_22)
		{
			// PAL guidance (expect the current match to be continued).
			cycle = 2;
		}
		else if (guide == GUIDE_32322)
		{
			// 25fps to 30 fps telecine.
			cycle = 6;
		}

		// Get needed dynamic storage.
		vmetric = 0;
		vthresh_saved = vthresh;
		xblocks = (vi.width+BLKSIZE-1) / BLKSIZE;
		yblocks = (vi.height+BLKSIZE-1) / BLKSIZE;
#ifdef WINDOWED_MATCH
		matchp = (unsigned int *) malloc(xblocks * yblocks * sizeof(unsigned int));
		if (matchp == NULL) env->ThrowError("Telecide: cannot allocate needed memory");
		matchc = (unsigned int *) malloc(xblocks * yblocks * sizeof(unsigned int));
		if (matchc == NULL) env->ThrowError("Telecide: cannot allocate needed memory");
#endif
		sump = (unsigned int *) malloc(xblocks * yblocks * sizeof(unsigned int));
		if (sump == NULL) env->ThrowError("Telecide: cannot allocate needed memory");
		sumc = (unsigned int *) malloc(xblocks * yblocks * sizeof(unsigned int));
		if (sumc == NULL) env->ThrowError("Telecide: cannot allocate needed memory");

		/* Load manual overrides file if there is one. */
		overrides = NULL;
		if (*ovr && (f = fopen(ovr, "r")) != NULL)
		{
			/* There's an overrides file and it can be opened.
			   Get the number of lines in the file and use that to malloc
			   an in-memory data structure. The structure will have 4 ints
			   per line in the file: starting frame, ending frame, pointer to
			   string of match data, single match specification (if pointer
			   is 0). */
			count = 0;
			while(fgets(buf, 80, f) != 0)
			{
				if (buf[0] == 0 || buf[0] == '\n' || buf[0] == '\r') continue;
				count++;
			}
			fclose(f);
			if ((f = fopen(ovr, "r")) != NULL)
			{
				// Allocate in-memory store and initialize it. Leave room for end marker.
				p = overrides = (unsigned int *) malloc(4 * (count + 1) * sizeof(unsigned int));
				memset(overrides, 0xff, 4 * (count + 1) * sizeof(unsigned int));
				// Parse the file.
				while(fgets(buf, 80, f) != 0)
				{
					// Ignore blank lines.
					if (buf[0] == 0 || buf[0] == '\n' || buf[0] == '\r') continue;
					d = buf;
					// First see if a range is specified.
					while (*d != ',' && *d != 0) d++;
					if (*d == 0)
					{
						// Range not specified. Load the frame number into
						// the starting and ending frame fields.
						sscanf(buf, "%d", p);
						*(p+1) = *p;
						// Skip to pointer field.
						p += 2;
					}
					else
					{
						// Range specified. Get the starting and ending frame numbers.
						sscanf(buf, "%d,%d", p, p+1);
						// Skip to pointer field.
						p += 2;
					}
					// Determine whether a single match or a multiple match sequence is specified.
					// Skip to the first specifier and then see if is followed by another one.
					d = buf;
					while (*d != '+' && *d != '-' && *d != 'v' && *d != 'b' &&
						   *d != 'p' && *d != 'c' && *d != 'n' &&
						   *d != 0) d++;
					if (*d != 0 && *(d+1) == 'p' || *(d+1) == 'c' || *(d+1) == 'n')
					{
						// Multiple match sequence found.
						// Count the number of match specifiers.
						dsaved = d;
						count = 0;
						while (*d == 'p' || *d == 'c' || *d == 'n')
						{
							d++;
							count++;
						}
						// Get storage for the match string plus a count field.
						x = (unsigned int *) malloc((count+1) * sizeof(unsigned int));
						// Store the pointer to the match string.
						*p++ = (unsigned int) x;
						// Mark it as a multimatch specifier.
						*p++ = 'm';
						// Save the count first.
						*x++ = count;
						// Now save the match specifiers.
						d = dsaved;
						while (*d == 'p' || *d == 'c' || *d == 'n')
						{
							*x++ = *d++;
						}
					}
					else if (*d == '+' || *d == '-' || *d == 'p' || *d == 'c' || *d == 'n')
					{
						// Single specifier found. Set the first field to 0 and store the
						// specifier.
						*p++ = 0;
						*p++ = *d;
					}
					else if (*d == 'v')
					{
						d += 2;
						sscanf(d, "%d", p++);
						*p++ = 'v';
					}
					else if (*d == 'b')
					{
						d += 2;
						sscanf(d, "%d", p++);
						*p++ = 'b';
					}
#if 0
					sprintf(buf, "%x %x %x %c\n", *(p-4), *(p-3), *(p-2), *(p-1));
					OutputDebugString(buf);
					if (*(p-1) == 'm' && *(p-2) != 0)
					{
						x = (unsigned int *) *(p-2);
						sprintf(buf, "%d: %c %c %c %c %c\n",
							    *x, *(x+1), *(x+2), *(x+3), *(x+4), *(x+5));
						OutputDebugString(buf);
					}
#endif
				}
				fclose(f);
			}
		}

		/* For safety in case someone came in without doing it. */
#if defined(USE_MMX)
#if !defined(GCC__)
		__asm emms;
#else
		__asm("emms");
#endif
#endif
	}

	~Telecide()
	{
		unsigned int *p;

		if (cache != NULL) free(cache);
#ifdef WINDOWED_MATCH
		if (matchp != NULL) free(matchp);
		if (matchc != NULL) free(matchc);
#endif
		if (sump != NULL) free(sump);
		if (sumc != NULL) free(sumc);

		if (overrides != NULL)
		{
			p = overrides + 2;
			while (*(p+1) == 'm' && *p != 0 && *p != 0xffffffff)
			{
				free((void *) *p);
				p += 4;
			}
			free(overrides);
		}
	}

	void PutChosen(int frame, unsigned int chosen)
	{
		int f;

		f = frame % CACHE_SIZE;
		if (frame < 0 || frame > vi.num_frames - 1 || cache[f].frame != (unsigned int) frame)
			return;
		cache[f].chosen = chosen;
	}

	void CacheInsert(int frame, unsigned int p, unsigned int pblock,
									 unsigned int c, unsigned int cblock)
	{
		int f;

		f = frame % CACHE_SIZE;
		if (frame < 0 || frame > vi.num_frames - 1)
			env->ThrowError("Telecide: internal error: invalid frame %d for CacheInsert", frame);
		cache[f].frame = frame;
		cache[f].metrics[P] = p;
		if (f) cache[f-1].metrics[N] = p;
		cache[f].metrics[C] = c;
		cache[f].metrics[PBLOCK] = pblock;
		cache[f].metrics[CBLOCK] = cblock;
		cache[f].chosen = 0xff;
	}

	bool CacheQuery(int frame, unsigned int *p, unsigned int *pblock,
									unsigned int *c, unsigned int *cblock)
	{
		int f;

		f = frame % CACHE_SIZE;
		if (frame < 0 || frame > vi.num_frames - 1)
			env->ThrowError("Telecide: internal error: invalid frame %d for CacheQuery", frame);
		if (cache[f].frame != (unsigned int) frame)
		{
			return false;
		}
		*p = cache[f].metrics[P];
		*c = cache[f].metrics[C];
		*pblock = cache[f].metrics[PBLOCK];
		*cblock = cache[f].metrics[CBLOCK];
		return true;
	}

	bool PredictHardYUY2(int frame, unsigned int *predicted, unsigned int *predicted_metric)
	{
		// Look for pattern in the actual delivered matches of the previous cycle of frames.
		// If a pattern is found, use that to predict the current match.
		if (guide == GUIDE_22)
		{
			if (cache[(frame-cycle)%CACHE_SIZE  ].chosen == 0xff ||
				cache[(frame-cycle+1)%CACHE_SIZE].chosen == 0xff)
				return false;
			switch ((cache[(frame-cycle)%CACHE_SIZE  ].chosen << 4) +
					(cache[(frame-cycle+1)%CACHE_SIZE].chosen))
			{
			case 0x11:
				*predicted = C;
				*predicted_metric = cache[frame%CACHE_SIZE].metrics[C];
				break;
			case 0x22:
				*predicted = N;
				*predicted_metric = cache[frame%CACHE_SIZE].metrics[N];
				break;
			default: return false;
			}
		}
		else if (guide == GUIDE_32)
		{
			if (cache[(frame-cycle)%CACHE_SIZE  ].chosen == 0xff ||
				cache[(frame-cycle+1)%CACHE_SIZE].chosen == 0xff ||
				cache[(frame-cycle+2)%CACHE_SIZE].chosen == 0xff ||
				cache[(frame-cycle+3)%CACHE_SIZE].chosen == 0xff ||
				cache[(frame-cycle+4)%CACHE_SIZE].chosen == 0xff)
				return false;

			switch ((cache[(frame-cycle)%CACHE_SIZE  ].chosen << 16) +
					(cache[(frame-cycle+1)%CACHE_SIZE].chosen << 12) +
					(cache[(frame-cycle+2)%CACHE_SIZE].chosen <<  8) +
					(cache[(frame-cycle+3)%CACHE_SIZE].chosen <<  4) +
					(cache[(frame-cycle+4)%CACHE_SIZE].chosen))
			{
			case 0x11122:
			case 0x11221:
			case 0x12211:
			case 0x12221: 
			case 0x21122: 
			case 0x11222: 
				*predicted = C;
				*predicted_metric = cache[frame%CACHE_SIZE].metrics[C];
				break;
			case 0x22111:
			case 0x21112:
			case 0x22112: 
			case 0x22211: 
				*predicted = N;
				*predicted_metric = cache[frame%CACHE_SIZE].metrics[N];
				break;
			default: return false;
			}
		}
		else if (guide == GUIDE_32322)
		{
			if (cache[(frame-cycle)%CACHE_SIZE  ].chosen == 0xff ||
				cache[(frame-cycle+1)%CACHE_SIZE].chosen == 0xff ||
				cache[(frame-cycle+2)%CACHE_SIZE].chosen == 0xff ||
				cache[(frame-cycle+3)%CACHE_SIZE].chosen == 0xff ||
				cache[(frame-cycle+4)%CACHE_SIZE].chosen == 0xff ||
				cache[(frame-cycle+5)%CACHE_SIZE].chosen == 0xff)
				return false;

			switch ((cache[(frame-cycle)%CACHE_SIZE  ].chosen << 20) +
					(cache[(frame-cycle+1)%CACHE_SIZE].chosen << 16) +
					(cache[(frame-cycle+2)%CACHE_SIZE].chosen << 12) +
					(cache[(frame-cycle+3)%CACHE_SIZE].chosen <<  8) +
					(cache[(frame-cycle+4)%CACHE_SIZE].chosen <<  4) +
					(cache[(frame-cycle+5)%CACHE_SIZE].chosen))
			{
			case 0x111122:
			case 0x111221:
			case 0x112211:
			case 0x122111:
			case 0x111222: 
			case 0x112221:
			case 0x122211:
			case 0x222111: 
				*predicted = C;
				*predicted_metric = cache[frame%CACHE_SIZE].metrics[C];
				break;
			case 0x221111:
			case 0x211112:

			case 0x221112: 
			case 0x211122: 
				*predicted = N;
				*predicted_metric = cache[frame%CACHE_SIZE].metrics[N];
				break;
			default: return false;
			}
		}
#ifdef DEBUG_PATTERN_GUIDANCE
		sprintf(buf, "%d: HARD: predicted = %d\n", frame, *predicted);
		OutputDebugString(buf);
#endif
		return true;
	}

	struct PREDICTION *PredictSoftYUY2(int frame)
	{
		// Use heuristics to look forward for a match.
		int i, j, y, c, n, phase;
		unsigned int metric;

		pred[0].metric = 0xffffffff;
		if (frame < 0 || frame > vi.num_frames - 1 - cycle) return pred;

		// Look at the next cycle of frames.
		for (y = frame + 1; y <= frame + cycle; y++)
		{
			// Look for a frame where the current and next match values are
			// very close. Those are candidates to predict the phase, because
			// that condition should occur only once per cycle. Store the candidate
			// phases and predictions in a list sorted by goodness. The list will
			// be used by the caller to try the phases in order.
			c = cache[y%CACHE_SIZE].metrics[C]; 
			n = cache[y%CACHE_SIZE].metrics[N];
			if (c == 0) c = 1;
			metric = (100 * abs (c - n)) / c;
			phase = y % cycle;
			if (metric < 5)
			{
				// Place the new candidate phase in sorted order in the list.
				// Find the insertion point.
				i = 0;
				while (metric > pred[i].metric) i++;
				// Find the end-of-list marker.
				j = 0;
				while (pred[j].metric != 0xffffffff) j++;
				// Shift all items below the insertion point down by one to make
				// room for the insertion.
				j++;
				for (; j > i; j--)
				{
					pred[j].metric = pred[j-1].metric;
					pred[j].phase = pred[j-1].phase;
					pred[j].predicted = pred[j-1].predicted;
					pred[j].predicted_metric = pred[j-1].predicted_metric;
				}
				// Insert the new candidate data.
				pred[j].metric = metric;
				pred[j].phase = phase;
				if (guide == GUIDE_32)
				{
					switch ((frame % cycle) - phase)
					{
					case -4: pred[j].predicted = N; pred[j].predicted_metric = cache[frame%CACHE_SIZE].metrics[N]; break; 
					case -3: pred[j].predicted = N; pred[j].predicted_metric = cache[frame%CACHE_SIZE].metrics[N]; break; 
					case -2: pred[j].predicted = C; pred[j].predicted_metric = cache[frame%CACHE_SIZE].metrics[C]; break; 
					case -1: pred[j].predicted = C; pred[j].predicted_metric = cache[frame%CACHE_SIZE].metrics[C]; break; 
					case  0: pred[j].predicted = C; pred[j].predicted_metric = cache[frame%CACHE_SIZE].metrics[C]; break; 
					case +1: pred[j].predicted = N; pred[j].predicted_metric = cache[frame%CACHE_SIZE].metrics[N]; break; 
					case +2: pred[j].predicted = N; pred[j].predicted_metric = cache[frame%CACHE_SIZE].metrics[N]; break; 
					case +3: pred[j].predicted = C; pred[j].predicted_metric = cache[frame%CACHE_SIZE].metrics[C]; break; 
					case +4: pred[j].predicted = C; pred[j].predicted_metric = cache[frame%CACHE_SIZE].metrics[C]; break; 
					}
				}
				else if (guide == GUIDE_32322)
				{
					switch ((frame % cycle) - phase)
					{
					case -5: pred[j].predicted = N; pred[j].predicted_metric = cache[frame%CACHE_SIZE].metrics[N]; break; 
					case -4: pred[j].predicted = N; pred[j].predicted_metric = cache[frame%CACHE_SIZE].metrics[N]; break; 
					case -3: pred[j].predicted = C; pred[j].predicted_metric = cache[frame%CACHE_SIZE].metrics[C]; break; 
					case -2: pred[j].predicted = C; pred[j].predicted_metric = cache[frame%CACHE_SIZE].metrics[C]; break; 
					case -1: pred[j].predicted = C; pred[j].predicted_metric = cache[frame%CACHE_SIZE].metrics[C]; break; 
					case  0: pred[j].predicted = C; pred[j].predicted_metric = cache[frame%CACHE_SIZE].metrics[C]; break; 
					case +1: pred[j].predicted = N; pred[j].predicted_metric = cache[frame%CACHE_SIZE].metrics[N]; break; 
					case +2: pred[j].predicted = N; pred[j].predicted_metric = cache[frame%CACHE_SIZE].metrics[N]; break; 
					case +3: pred[j].predicted = C; pred[j].predicted_metric = cache[frame%CACHE_SIZE].metrics[C]; break; 
					case +4: pred[j].predicted = C; pred[j].predicted_metric = cache[frame%CACHE_SIZE].metrics[C]; break; 
					case +5: pred[j].predicted = C; pred[j].predicted_metric = cache[frame%CACHE_SIZE].metrics[C]; break; 
					}
				}
			}
#ifdef DEBUG_PATTERN_GUIDANCE
			sprintf(buf,"%d: metric = %d phase = %d\n", frame, metric, phase);
			OutputDebugString(buf);
#endif
		}
		return pred;
	}

	void WriteHints(unsigned char *dst, bool film, bool inpattern)
	{
		unsigned int hint;

		if (GetHintingData(dst, &hint) == true) hint = 0;
		if (film == true) hint |= PROGRESSIVE;
		else hint &= ~PROGRESSIVE;
		if (inpattern == true) hint |= IN_PATTERN;
		else hint &= ~IN_PATTERN;
		PutHintingData(dst, hint);
	}
};
