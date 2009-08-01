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
#include "Telecide.h"
#include "info.h"
#include "strYUY2.h"

void Telecide::Show(PVideoFrame &dst, int frame)
{
	void DrawString(PVideoFrame &dst, int x, int y, const char *s);
	void DrawStringYUY2(PVideoFrame &dst, int x, int y, const char *s);
	char use;
	typedef void (* Drawtype)(PVideoFrame &dst, int x, int y, const char *s);
	Drawtype draw = vi.IsYV12() ? DrawString : DrawStringYUY2;

	if (chosen == P) use = 'p';
	else if (chosen == C) use = 'c';
	else use = 'n';

	sprintf(buf, "Telecide %s", VERSION);
	draw(dst, 0, 0, buf);

	sprintf(buf, "Copyright 2003-2008 Donald A. Graft");
	draw(dst, 0, 1, buf);

	sprintf(buf,"frame %d:", frame);
	draw(dst, 0, 3, buf);

	sprintf(buf, "matches: %d  %d  %d", p, c, np);
	draw(dst, 0, 4, buf);

	if (post != POST_NONE)
	{
		sprintf(buf,"vmetrics: %d  %d  %d [chosen=%d]", pblock, cblock, npblock, vmetric);
		draw(dst, 0, 5, buf);
	}

	if (guide != GUIDE_NONE)
	{
		sprintf(buf, "pattern mismatch=%0.2f%%", mismatch); 
		draw(dst, 0, 5 + (post != POST_NONE), buf);
	}

	sprintf(buf,"[%s %c]%s %s",
		found == true ? "forcing" : "using", use,
		post != POST_NONE ? (film == true ? " [progressive]" : " [interlaced]") : "",
		guide != GUIDE_NONE ? status : "");
	draw(dst, 0, 5 + (post != POST_NONE) + (guide != GUIDE_NONE), buf);
}

void Telecide::Debug(int frame)
{
	char use;

	if (chosen == P) use = 'p';
	else if (chosen == C) use = 'c';
	else use = 'n';
	sprintf(buf,"Telecide: frame %d: matches: %d %d %d", frame, p, c, np);
	OutputDebugString(buf);
	if (post != POST_NONE)
	{
		sprintf(buf,"Telecide: frame %d: vmetrics: %d %d %d [chosen=%d]", frame, pblock, cblock, npblock, vmetric);
		OutputDebugString(buf);
	}
	sprintf(buf,"Telecide: frame %d: [%s %c]%s %s", frame, found == true ? "forcing" : "using", use,
		post != POST_NONE ? (film == true ? " [progressive]" : " [interlaced]") : "",
		guide != GUIDE_NONE ? status : "");
	OutputDebugString(buf);
}

PVideoFrame Telecide::GetFrame(int frame, IScriptEnvironment* env)
{
	// Get the current frame.
	if (frame < 0) frame = 0;
	if (frame > vi.num_frames - 1) frame = vi.num_frames - 1;
	GETFRAME(frame, fc);
	fcrp = (unsigned char *) fc->GetReadPtr(PLANAR_Y);
	if (vi.IsYV12())
	{
		fcrpU = (unsigned char *) fc->GetReadPtr(PLANAR_U);
		fcrpV = (unsigned char *) fc->GetReadPtr(PLANAR_V);
	}

	// Get the previous frame.
	pframe = frame == 0 ? 0 : frame - 1;
	GETFRAME(pframe, fp);
	fprp = (unsigned char *) fp->GetReadPtr(PLANAR_Y);
	if (vi.IsYV12())
	{
		fprpU = (unsigned char *) fp->GetReadPtr(PLANAR_U);
		fprpV = (unsigned char *) fp->GetReadPtr(PLANAR_V);
	}

	// Get the next frame metrics if we might need them.
	nframe = frame >= vi.num_frames - 1 ? vi.num_frames - 1 : frame + 1;
	GETFRAME(nframe, fn);
	fnrp = (unsigned char *) fn->GetReadPtr(PLANAR_Y);
	if (vi.IsYV12())
	{
		fnrpU = (unsigned char *) fn->GetReadPtr(PLANAR_U);
		fnrpV = (unsigned char *) fn->GetReadPtr(PLANAR_V);
	}

	pitch = fc->GetPitch(PLANAR_Y);
	pitchover2 = pitch >> 1;
	pitchtimes4 = pitch << 2;
	w = fc->GetRowSize(PLANAR_Y);
	h = fc->GetHeight(PLANAR_Y);
	if (vi.IsYUY2() && ((w/2) & 1))
		env->ThrowError("Telecide: width must be a multiple of 2; use Crop");
	if (vi.IsYV12() && (w & 1))
		env->ThrowError("Telecide: width must be a multiple of 2; use Crop");
	if (h & 1)
		env->ThrowError("Telecide: height must be a multiple of 2; use Crop");
	wover2 = w/2;
	hover2 = h/2;
	hplus1over2 = (h+1)/2;
	hminus2= h - 2;
	dst = env->NewVideoFrame(vi);
        /* GstAVSynth: preserve timestamps */
        dst->SetTimestamp (fc->GetTimestamp());
	dpitch = dst->GetPitch(PLANAR_Y);

	// Ensure that the metrics for the frames
	// after the current frame are in the cache. They will be used for
	// pattern guidance.
	if (guide != GUIDE_NONE)
	{
		for (y = frame + 1; y <= frame + cycle + 1; y++)
		{
			if (y > vi.num_frames - 1) break;
			if (CacheQuery(y, &p, &pblock, &c, &cblock) == false)
			{
				GETFRAME(y, lc);
				crp = (unsigned char *) lc->GetReadPtr(PLANAR_Y);
				if (vi.IsYV12())
				{
					crpU = (unsigned char *) lc->GetReadPtr(PLANAR_U);
					crpV = (unsigned char *) lc->GetReadPtr(PLANAR_V);
				}
				GETFRAME(y == 0 ? 1 : y - 1, lp);
				prp = (unsigned char *) lp->GetReadPtr(PLANAR_Y);
				if (vi.IsYV12())
				{
					prpU = (unsigned char *) lp->GetReadPtr(PLANAR_U);
					prpV = (unsigned char *) lp->GetReadPtr(PLANAR_V);
				}
				CalculateMetrics(y, crp, crpU, crpV, prp, prpU, prpV, env);
			}
		}
	}

	/* Check for manual overrides of the field matching. */
	overrides_p = overrides;
	found = false;
	film = true;
	override = false;
	inpattern = false;
	vthresh = vthresh_saved;
	back = back_saved;
	if (overrides_p != NULL)
	{
		while (*overrides_p < 0xffffffff)
		{
			// If the frame is in range...
			if (((unsigned int) frame >= *overrides_p) && ((unsigned int) frame <= *(overrides_p+1)))
			{
				// and it's a single specifier. 
				if (*(overrides_p+3) == 'p' || *(overrides_p+3) == 'c' || *(overrides_p+3) == 'n')
				{
					// Get the match specifier and stop parsing.
					switch(*(overrides_p+3))
					{
					case 'p': chosen = P; lowest = cache[frame%CACHE_SIZE].metrics[P]; found = true; break;
					case 'c': chosen = C; lowest = cache[frame%CACHE_SIZE].metrics[C]; found = true; break;
					case 'n': chosen = N; lowest = cache[(frame+1)%CACHE_SIZE].metrics[P]; found = true; break;
					}
				}
				else if (*(overrides_p+3) == 'b')
				{
					back = *(overrides_p+2);
				}
				else if (*(overrides_p+3) == 'm')
				{
					// It's a multiple match specifier.
					found = true;
					// Get the pointer to the specifier string.
					unsigned int *x = (unsigned int *) *(overrides_p+2);
					// Get the index into the specification string.
					// Remember, the count is first followed by the specifiers.
					int ndx = ((frame - *overrides_p) % *x);
					// Point to the specifier string.
					x++;
					// Load the specifier for this frame and stop parsing.
					switch(x[ndx])
					{
					case 'p': chosen = P; lowest = cache[frame%CACHE_SIZE].metrics[P]; break;
					case 'c': chosen = C; lowest = cache[frame%CACHE_SIZE].metrics[C]; break;
					case 'n': chosen = N; lowest = cache[(frame+1)%CACHE_SIZE].metrics[P]; break;
					}
				}
			}
			// Next override line.
			overrides_p += 4;
		}
	}

	// Get the metrics for the current-previous (p), current-current (c), and current-next (n) match candidates.
	if (CacheQuery(frame, &p, &pblock, &c, &cblock) == false)
	{
		CalculateMetrics(frame, fcrp, fcrpU, fcrpV, fprp, fprpU, fprpV, env);
		CacheQuery(frame, &p, &pblock, &c, &cblock);
	}
	if (CacheQuery(nframe, &np, &npblock, &nc, &ncblock) == false)
	{
		CalculateMetrics(nframe, fnrp, fnrpU, fnrpV, fcrp, fcrpU, fcrpV, env);
		CacheQuery(nframe, &np, &npblock, &nc, &ncblock);
	}

	// Determine the best candidate match.
	if (found != true)
	{
		lowest = c;
		chosen = C;
		if (back == ALWAYS_BACK && p < lowest)
		{
			lowest = p;
			chosen = P;
		}
		if (np < lowest)
		{
			lowest = np;
			chosen = N;
		}
	}
	if ((frame == 0 && chosen == P) || (frame == vi.num_frames - 1 && chosen == N))
	{
		chosen = C;
		lowest = c;
	}

	// See if we can apply pattern guidance.
	mismatch = 100.0;
	if (guide != GUIDE_NONE)
	{
		hard = false;
		if (frame >= cycle && PredictHardYUY2(frame, &predicted, &predicted_metric) == true)
		{
			inpattern = true;
			mismatch = 0.0;
			hard = true;
			if (chosen != predicted)
			{
				// The chosen frame doesn't match the prediction.
				if (predicted_metric == 0) mismatch = 0.0;
				else mismatch = (100.0*abs(predicted_metric - lowest))/predicted_metric;
				if (mismatch < gthresh)
				{
					// It's close enough, so use the predicted one.
					if (found != true)
					{
						chosen = predicted;
						override = true;
					}
				}
				else
				{
					hard = false;
					inpattern = false;
				}
			}
		}

		if (hard == false && guide != GUIDE_22)
		{
			int i;
			struct PREDICTION *pred = PredictSoftYUY2(frame);

			if ((frame <= vi.num_frames - 1 - cycle) &&	(pred[0].metric != 0xffffffff))
			{
				// Apply pattern guidance.
				// If the predicted match metric is within defined percentage of the
				// best calculated one, then override the calculated match with the
				// predicted match.
				i = 0;
				while (pred[i].metric != 0xffffffff)
				{
					predicted = pred[i].predicted;
					predicted_metric = pred[i].predicted_metric;
#ifdef DEBUG_PATTERN_GUIDANCE
					sprintf(buf, "%d: predicted = %d\n", frame, predicted);
					OutputDebugString(buf);
#endif
					if (chosen != predicted)
					{
						// The chosen frame doesn't match the prediction.
						if (predicted_metric == 0) mismatch = 0.0;
						else mismatch = (100.0*abs(predicted_metric - lowest))/predicted_metric;
						if ((int) mismatch <= gthresh)
						{
							// It's close enough, so use the predicted one.
							if (found != true)
							{
								chosen = predicted;
								override = true;
							}
							inpattern = true;
							break;
						}
						else
						{
							// Looks like we're not in a predictable pattern.
							inpattern = false;
						}
					}
					else
					{
						inpattern = true;
						mismatch = 0.0;
						break;
					}
					i++;
				}
			}
		}
	}

	// Check for overrides of vthresh.
	overrides_p = overrides;
	if (overrides_p != NULL)
	{
		while (*overrides_p < 0xffffffff)
		{
			// If the frame is in range...
			if (((unsigned int) frame >= *overrides_p) && ((unsigned int) frame <= *(overrides_p+1)))
			{
				if (*(overrides_p+3) == 'v')
				{
					vthresh = *(overrides_p+2);
				}
			}
			// Next override line.
			overrides_p += 4;
		}
	}

	// Check the match for progressive versus interlaced.
	if (post != POST_NONE)
	{
		if (chosen == P) vmetric = pblock;
		else if (chosen == C) vmetric = cblock;
		else if (chosen == N) vmetric = npblock;

		if (found == false && back == BACK_ON_COMBED && vmetric > bthresh && p < lowest)
		{
			// Backward match.
			vmetric = pblock;
			chosen = P;
			inpattern = false;
			mismatch = 100;
		}
		if (vmetric > vthresh)
		{
			// After field matching and pattern guidance the frame is still combed.
			film = false;
			if (found == false && (post == POST_FULL_NOMATCH || post == POST_FULL_NOMATCH_MAP))
			{
				chosen = C;
				vmetric = cblock;
				inpattern = false;
				mismatch = 100;
			}
		}
	}
	vthresh = vthresh_saved;

	// Setup strings for debug info.
	if (inpattern == true && override == false) strcpy(status, "[in-pattern]");
	else if (inpattern == true && override == true) strcpy(status, "[in-pattern*]");
	else strcpy(status, "[out-of-pattern]");

	// Assemble and output the reconstructed frame according to the final match.
	dstp = dst->GetWritePtr(PLANAR_Y);
    if (vi.IsYV12())
	{
		dstpU = dst->GetWritePtr(PLANAR_U);
		dstpV = dst->GetWritePtr(PLANAR_V);
	}
	if (chosen == N)
	{
		// The best match was with the next frame.
		if (tff == true)
		{
			env->BitBlt(dstp, 2 * dpitch, fnrp, 2 * pitch, w, hover2);
			env->BitBlt(dstp + dpitch, 2 * dpitch, fcrp + pitch, 2 * pitch,	w, hover2);
			if (vi.IsYV12())
			{
				env->BitBlt(dstpU, dpitch, fnrpU, pitch, w/2, h/4);
				env->BitBlt(dstpV, dpitch, fnrpV, pitch, w/2, h/4);
				env->BitBlt(dstpU + dpitch/2, dpitch, fcrpU + pitch/2, pitch, w/2, h/4);
				env->BitBlt(dstpV + dpitch/2, dpitch, fcrpV + pitch/2, pitch, w/2, h/4);
			}
		}
		else
		{
			env->BitBlt(dstp, 2 * dpitch, fcrp, 2 * pitch, w, hplus1over2);
			env->BitBlt(dstp + dpitch, 2 * dpitch, fnrp + pitch, 2 * pitch,	w, hover2);
			if (vi.IsYV12())
			{
				env->BitBlt(dstpU, dpitch, fcrpU, pitch, w/2, h/4);
				env->BitBlt(dstpV, dpitch, fcrpV, pitch, w/2, h/4);
				env->BitBlt(dstpU + dpitch/2, dpitch, fnrpU + pitch/2, pitch, w/2, h/4);
				env->BitBlt(dstpV + dpitch/2, dpitch, fnrpV + pitch/2, pitch, w/2, h/4);
			}
		}
	}
	else if (chosen == C)
	{
		// The best match was with the current frame.
		env->BitBlt(dstp, 2 * dpitch, fcrp, 2 * pitch, w, hplus1over2);
		env->BitBlt(dstp + dpitch, 2 * dpitch, fcrp + pitch, 2 * pitch,	w, hover2);
		if (vi.IsYV12())
		{
			env->BitBlt(dstpU, dpitch, fcrpU, pitch, w/2, h/4);
			env->BitBlt(dstpV, dpitch, fcrpV, pitch, w/2, h/4);
			env->BitBlt(dstpU + dpitch/2, dpitch, fcrpU + pitch/2, pitch, w/2, h/4);
			env->BitBlt(dstpV + dpitch/2, dpitch, fcrpV + pitch/2, pitch, w/2, h/4);
		}
	}
	else if (tff == false)
	{
		// The best match was with the previous frame.
		env->BitBlt(dstp, 2 * dpitch, fprp, 2 * pitch, w, hplus1over2);
		env->BitBlt(dstp + dpitch, 2 * dpitch, fcrp + pitch, 2 * pitch,	w, hover2);
		if (vi.IsYV12())
		{
			env->BitBlt(dstpU, dpitch, fprpU, pitch, w/2, h/4);
			env->BitBlt(dstpV, dpitch, fprpV, pitch, w/2, h/4);
			env->BitBlt(dstpU + dpitch/2, dpitch, fcrpU + pitch/2, pitch, w/2, h/4);
			env->BitBlt(dstpV + dpitch/2, dpitch, fcrpV + pitch/2, pitch, w/2, h/4);
		}
	}
	else
	{
		// The best match was with the previous frame.
		env->BitBlt(dstp, 2 * dpitch, fcrp, 2 * pitch, w, hplus1over2);
		env->BitBlt(dstp + dpitch, 2 * dpitch, fprp + pitch, 2 * pitch,	w, hover2);
		if (vi.IsYV12())
		{
			env->BitBlt(dstpU, dpitch, fcrpU, pitch, w/2, h/4);
			env->BitBlt(dstpV, dpitch, fcrpV, pitch, w/2, h/4);
			env->BitBlt(dstpU + dpitch/2, dpitch, fprpU + pitch/2, pitch, w/2, h/4);
			env->BitBlt(dstpV + dpitch/2, dpitch, fprpV + pitch/2, pitch, w/2, h/4);
		}
	}
	if (guide != GUIDE_NONE) PutChosen(frame, chosen);

	/* Check for manual overrides of the deinterlacing. */
	overrides_p = overrides;
	force = 0;
	if (overrides_p != NULL)
	{
		while (*overrides_p < 0xffffffff)
		{
			// Is the frame in range...
			if (((unsigned int) frame >= *overrides_p) && ((unsigned int) frame <= *(overrides_p+1)) &&
				// and is it a single specifier...
				(*(overrides_p+2) == 0) &&
				// and is it a deinterlacing specifier?
				(*(overrides_p+3) == '+' || *(overrides_p+3) == '-'))
			{
				// Yes, load the specifier and stop parsing.
				overrides_p += 3;
				force = *overrides_p;
				break;
			}
			// Next specification record.
			overrides_p += 4;
		}
	}

	// Do postprocessing if enabled and required for this frame.
	if (post == POST_NONE || post == POST_METRICS)
	{
		if (force == '+') film = false;
		else if (force == '-') film = true;
	}
	else if ((force == '+') ||
		((post == POST_FULL || post == POST_FULL_MAP || post == POST_FULL_NOMATCH || post == POST_FULL_NOMATCH_MAP)
		         && (film == false && force != '-')))
	{
		unsigned char *dstpp, *dstpn;
		int v1, v2, z;

		if (blend == true)
		{
			// Blend mode.
			final = env->NewVideoFrame(vi);

		        /* GstAVSynth: preserve timestamps */
		        final->SetTimestamp (fc->GetTimestamp());

			// Do first and last lines.
			finalp = final->GetWritePtr(PLANAR_Y);
			dstp = dst->GetWritePtr(PLANAR_Y);
			dstpn = dstp + dpitch;
			for (x = 0; x < w; x++)
			{
				finalp[x] = (((int)dstp[x] + (int)dstpn[x]) >> 1);
			}
			finalp = final->GetWritePtr(PLANAR_Y) + (h-1)*dpitch;
			dstp = dst->GetWritePtr(PLANAR_Y) + (h-1)*dpitch;
			dstpp = dstp - dpitch;
			for (x = 0; x < w; x++)
			{
				finalp[x] = (((int)dstp[x] + (int)dstpp[x]) >> 1);
			}
			// Now do the rest.
			dstp = dst->GetWritePtr(PLANAR_Y) + dpitch;
			dstpp = dstp - dpitch;
			dstpn = dstp + dpitch;
			finalp = final->GetWritePtr(PLANAR_Y) + dpitch;
			for (y = 1; y < h - 1; y++)
			{
				for (x = 0; x < w; x++)
				{
					v1 = (int) dstp[x] - dthresh;
					if (v1 < 0) v1 = 0; 
					v2 = (int) dstp[x] + dthresh;
					if (v2 > 235) v2 = 235; 
					if ((v1 > dstpp[x] && v1 > dstpn[x]) || (v2 < dstpp[x] && v2 < dstpn[x]))
					{
						if (post == POST_FULL_MAP || post == POST_FULL_NOMATCH_MAP)
						{
							if (vi.IsYUY2())
							{
								if (x & 1) finalp[x] = 128;
								else finalp[x] = 235;
							}
							else
							{
								finalp[x] = 235;
							}
						}
						else
							finalp[x] = ((int)dstpp[x] + (int)dstpn[x] + (int)dstp[x] + (int)dstp[x]) >> 2;
					}
					else finalp[x] = dstp[x];
				}
				finalp += dpitch;
				dstp += dpitch;
				dstpp += dpitch;
				dstpn += dpitch;
			}

			if (vi.IsYV12())
			{
				// Chroma planes.
				for (z = 0; z < 2; z++)
				{
					if (z == 0)
					{
						// Do first and last lines.
						finalp = final->GetWritePtr(PLANAR_U);
						dstp = dst->GetWritePtr(PLANAR_U);
						dstpn = dstp + dpitch/2;
						for (x = 0; x < wover2; x++)
						{
							finalp[x] = (((int)dstp[x] + (int)dstpn[x]) >> 1);
						}
						finalp = final->GetWritePtr(PLANAR_U) + (hover2-1)*dpitch/2;
						dstp = dst->GetWritePtr(PLANAR_U) + (hover2-1)*dpitch/2;
						dstpp = dstp - dpitch/2;
						for (x = 0; x < wover2; x++)
						{
							finalp[x] = (((int)dstp[x] + (int)dstpp[x]) >> 1);
						}
						// Now do the rest.
						finalp = final->GetWritePtr(PLANAR_U) + dpitch/2;
						dstp = dst->GetWritePtr(PLANAR_U) + dpitch/2;
					}
					else
					{
						// Do first and last lines.
						finalp = final->GetWritePtr(PLANAR_V);
						dstp = dst->GetWritePtr(PLANAR_V);
						dstpn = dstp + dpitch/2;
						for (x = 0; x < wover2; x++)
						{
							finalp[x] = (((int)dstp[x] + (int)dstpn[x]) >> 1);
						}
						finalp = final->GetWritePtr(PLANAR_V) + (hover2-1)*dpitch/2;
						dstp = dst->GetWritePtr(PLANAR_V) + (hover2-1)*dpitch/2;
						dstpp = dstp - dpitch/2;
						for (x = 0; x < wover2; x++)
						{
							finalp[x] = (((int)dstp[x] + (int)dstpp[x]) >> 1);
						}
						// Now do the rest.
						finalp = final->GetWritePtr(PLANAR_V) + dpitch/2;
						dstp = dst->GetWritePtr(PLANAR_V) + dpitch/2;
					}
					dstpp = dstp - dpitch/2;
					dstpn = dstp + dpitch/2;
					for (y = 1; y < hover2 - 1; y++)
					{
						for (x = 0; x < wover2; x++)
						{
							v1 = (int) dstp[x] - dthresh;
							if (v1 < 0) v1 = 0; 
							v2 = (int) dstp[x] + dthresh;
							if (v2 > 235) v2 = 235; 
							if ((v1 > dstpp[x] && v1 > dstpn[x]) || (v2 < dstpp[x] && v2 < dstpn[x]))
							{
								if (post == POST_FULL_MAP || post == POST_FULL_NOMATCH_MAP)
								{
									finalp[x] = 128;
								}
								else
									finalp[x] = ((int)dstpp[x] + (int)dstpn[x] + (int)dstp[x] + (int)dstp[x]) >> 2;
							}
							else finalp[x] = dstp[x];
						}
						finalp += dpitch/2;
						dstp += dpitch/2;
						dstpp += dpitch/2;
						dstpn += dpitch/2;
					}
				}
			}
			if (show == true) Show(final, frame);
			if (debug == true) Debug(frame);
			if (hints == true) WriteHints(final->GetWritePtr(PLANAR_Y), film, inpattern);
			return final;
		}

		// Interpolate mode.
		// Luma plane.
		dstp = dst->GetWritePtr(PLANAR_Y) + dpitch;
		dstpp = dstp - dpitch;
		dstpn = dstp + dpitch;
		for (y = 1; y < h - 1; y+=2)
		{
			for (x = 0; x < w; x++)
			{
				v1 = (int) dstp[x] - dthresh;
				if (v1 < 0) v1 = 0; 
				v2 = (int) dstp[x] + dthresh;
				if (v2 > 235) v2 = 235; 
				if ((v1 > dstpp[x] && v1 > dstpn[x]) || (v2 < dstpp[x] && v2 < dstpn[x]))
				{
					if (post == POST_FULL_MAP || post == POST_FULL_NOMATCH_MAP)
					{
						if (vi.IsYUY2())
						{
							if (x & 1) dstp[x] = 128;
							else dstp[x] = 235;
						}
						else
						{
							dstp[x] = 235;
						}
					}
					else
						dstp[x] = (dstpp[x] + dstpn[x]) >> 1;
				}
			}
			dstp += 2*dpitch;
			dstpp += 2*dpitch;
			dstpn += 2*dpitch;
		}

		if (vi.IsYV12())
		{
			// Chroma planes.
			for (z = 0; z < 2; z++)
			{
				if (z == 0) dstp = dst->GetWritePtr(PLANAR_U) + dpitch/2;
				else dstp = dst->GetWritePtr(PLANAR_V) + dpitch/2;
				dstpp = dstp - dpitch/2;
				dstpn = dstp + dpitch/2;
				for (y = 1; y < hover2 - 1; y+=2)
				{
					for (x = 0; x < wover2; x++)
					{
						v1 = (int) dstp[x] - dthresh;
						if (v1 < 0) v1 = 0; 
						v2 = (int) dstp[x] + dthresh;
						if (v2 > 235) v2 = 235; 
						if ((v1 > dstpp[x] && v1 > dstpn[x]) || (v2 < dstpp[x] && v2 < dstpn[x]))
						{
							if (post == POST_FULL_MAP || post == POST_FULL_NOMATCH_MAP)
							{
								dstp[x] = 128;
							}
							else
								dstp[x] = (dstpp[x] + dstpn[x]) >> 1;
						}
					}
					dstp += dpitch;
					dstpp += dpitch;
					dstpn += dpitch;
				}
			}
		}
	}

	if (show == true) Show(dst, frame);
	if (debug == true) Debug(frame);
	if (hints == true) WriteHints(dst->GetWritePtr(PLANAR_Y), film, inpattern);
	return dst;
}

void Telecide::CalculateMetrics(int frame, unsigned char *fcrp, unsigned char *fcrpU, unsigned char *fcrpV,
									unsigned char *fprp, unsigned char *fprpU, unsigned char *fprpV,
									IScriptEnvironment* env)
{
	int x, y, p, c, tmp1, tmp2, skip;
	bool vc;
    unsigned char *currbot0, *currbot2, *prevbot0, *prevbot2;
	unsigned char *prevtop0, *prevtop2, *prevtop4, *currtop0, *currtop2, *currtop4;
	unsigned char *a0, *a2, *b0, *b2, *b4;
	unsigned int diff, index;
#define T 4

	/* Clear the block sums. */
 	for (y = 0; y < yblocks; y++)
	{
 		for (x = 0; x < xblocks; x++)
		{
#ifdef WINDOWED_MATCH
			matchp[y*xblocks+x] = 0;
			matchc[y*xblocks+x] = 0;
#endif
			sump[y*xblocks+x] = 0;
			sumc[y*xblocks+x] = 0;
		}
	}

	/* Find the best field match. Subsample the frames for speed. */
	currbot0  = fcrp + pitch;
	currbot2  = fcrp + 3 * pitch;
	currtop0 = fcrp;
	currtop2 = fcrp + 2 * pitch;
	currtop4 = fcrp + 4 * pitch;
	prevbot0  = fprp + pitch;
	prevbot2  = fprp + 3 * pitch;
	prevtop0 = fprp;
	prevtop2 = fprp + 2 * pitch;
	prevtop4 = fprp + 4 * pitch;
	if (tff == true)
	{
		a0 = prevbot0;
		a2 = prevbot2;
		b0 = currtop0;
		b2 = currtop2;
		b4 = currtop4;
	}
	else
	{
		a0 = currbot0;
		a2 = currbot2;
		b0 = prevtop0;
		b2 = prevtop2;
		b4 = prevtop4;
	}
	p = c = 0;

	// Calculate the field match and film/video metrics.
	if (vi.IsYV12()) skip = 1;
	else skip = 1 + (chroma == false);
	for (y = 0, index = 0; y < h - 4; y+=4)
	{
		/* Exclusion band. Good for ignoring subtitles. */
		if (y0 == y1 || y < y0 || y > y1)
		{
			for (x = 0; x < w;)
			{
				if (vi.IsYV12())
					index = (y/BLKSIZE)*xblocks + x/BLKSIZE;
				else
					index = (y/BLKSIZE)*xblocks + x/BLKSIZE_TIMES2;

				// Test combination with current frame.
				tmp1 = ((long)currbot0[x] + (long)currbot2[x]);
//				diff = abs((long)currtop0[x] - (tmp1 >> 1));
				diff = abs((((long)currtop0[x] + (long)currtop2[x] + (long)currtop4[x])) - (tmp1 >> 1) - tmp1);
				if (diff > nt)
				{
					c += diff;
#ifdef WINDOWED_MATCH
					matchc[index] += diff;
#endif
				}

				tmp1 = currbot0[x] + T;
				tmp2 = currbot0[x] - T;
				vc = (tmp1 < currtop0[x] && tmp1 < currtop2[x]) ||
					 (tmp2 > currtop0[x] && tmp2 > currtop2[x]);
				if (vc)
				{
					sumc[index]++;
				}

				// Test combination with previous frame.
				tmp1 = ((long)a0[x] + (long)a2[x]);
				diff = abs((((long)b0[x] + (long)b2[x] + (long)b4[x])) - (tmp1 >> 1) - tmp1);
				if (diff > nt)
				{
					p += diff;
#ifdef WINDOWED_MATCH
					matchp[index] += diff;
#endif
				}

				tmp1 = a0[x] + T;
				tmp2 = a0[x] - T;
				vc = (tmp1 < b0[x] && tmp1 < b2[x]) ||
					 (tmp2 > b0[x] && tmp2 > b2[x]);
				if (vc)
				{
					sump[index]++;
				}

				x += skip;
				if (!(x&3)) x += 4;
			}
		}
		currbot0 += pitchtimes4;
		currbot2 += pitchtimes4;
		currtop0 += pitchtimes4;
		currtop2 += pitchtimes4;
		currtop4 += pitchtimes4;
		a0		 += pitchtimes4;
		a2		 += pitchtimes4;
		b0		 += pitchtimes4;
		b2		 += pitchtimes4;
		b4		 += pitchtimes4;
	}

	if (vi.IsYV12() && chroma == true)
	{
		int z;

		for (z = 0; z < 2; z++)
		{
			// Do the same for the U plane.
			if (z == 0)
			{
				currbot0  = fcrpU + pitchover2;
				currbot2  = fcrpU + 3 * pitchover2;
				currtop0 = fcrpU;
				currtop2 = fcrpU + 2 * pitchover2;
				currtop4 = fcrpU + 4 * pitchover2;
				prevbot0  = fprpU + pitchover2;
				prevbot2  = fprpU + 3 * pitchover2;
				prevtop0 = fprpU;
				prevtop2 = fprpU + 2 * pitchover2;
				prevtop4 = fprpU + 4 * pitchover2;
			}
			else
			{
				currbot0  = fcrpV + pitchover2;
				currbot2  = fcrpV + 3 * pitchover2;
				currtop0 = fcrpV;
				currtop2 = fcrpV + 2 * pitchover2;
				currtop4 = fcrpV + 4 * pitchover2;
				prevbot0  = fprpV + pitchover2;
				prevbot2  = fprpV + 3 * pitchover2;
				prevtop0 = fprpV;
				prevtop2 = fprpV + 2 * pitchover2;
				prevtop4 = fprpV + 4 * pitchover2;
			}
			if (tff == true)
			{
				a0 = prevbot0;
				a2 = prevbot2;
				b0 = currtop0;
				b2 = currtop2;
				b4 = currtop4;
			}
			else
			{
				a0 = currbot0;
				a2 = currbot2;
				b0 = prevtop0;
				b2 = prevtop2;
				b4 = prevtop4;
			}

			for (y = 0, index = 0; y < hover2 - 4; y+=4)
			{
				/* Exclusion band. Good for ignoring subtitles. */
				if (y0 == y1 || y < y0/2 || y > y1/2)
				{
					for (x = 0; x < wover2;)
					{
						if (vi.IsYV12())
							index = (y/BLKSIZE)*xblocks + x/BLKSIZE;
						else
							index = (y/BLKSIZE)*xblocks + x/BLKSIZE_TIMES2;

						// Test combination with current frame.
						tmp1 = ((long)currbot0[x] + (long)currbot2[x]);
						diff = abs((((long)currtop0[x] + (long)currtop2[x] + (long)currtop4[x])) - (tmp1 >> 1) - tmp1);
						if (diff > nt)
						{
							c += diff;
#ifdef WINDOWED_MATCH
							matchc[index] += diff;
#endif
						}

						tmp1 = currbot0[x] + T;
						tmp2 = currbot0[x] - T;
						vc = (tmp1 < currtop0[x] && tmp1 < currtop2[x]) ||
							 (tmp2 > currtop0[x] && tmp2 > currtop2[x]);
						if (vc)
						{
							sumc[index]++;
						}

						// Test combination with previous frame.
						tmp1 = ((long)a0[x] + (long)a2[x]);
						diff = abs((((long)b0[x] + (long)b2[x] + (long)b4[x])) - (tmp1 >> 1) - tmp1);
						if (diff > nt)
						{
							p += diff;
#ifdef WINDOWED_MATCH
							matchp[index] += diff;
#endif
						}

						tmp1 = a0[x] + T;
						tmp2 = a0[x] - T;
						vc = (tmp1 < b0[x] && tmp1 < b2[x]) ||
							 (tmp2 > b0[x] && tmp2 > b2[x]);
						if (vc)
						{
							sump[index]++;
						}

						x ++;
						if (!(x&3)) x += 4;
					}
				}
				currbot0 += 4*pitchover2;
				currbot2 += 4*pitchover2;
				currtop0 += 4*pitchover2;
				currtop2 += 4*pitchover2;
				currtop4 += 4*pitchover2;
				a0		 += 4*pitchover2;
				a2		 += 4*pitchover2;
				b0		 += 4*pitchover2;
				b2		 += 4*pitchover2;
				b4		 += 4*pitchover2;
			}
		}
	}

	// Now find the blocks that have the greatest differences.
#ifdef WINDOWED_MATCH
	highest_matchp = 0;
	for (y = 0; y < yblocks; y++)
	{
		for (x = 0; x < xblocks; x++)
		{
if (frame == 45 && matchp[y * xblocks + x] > 2500)
{
	sprintf(buf, "%d/%d = %d\n", x, y, matchp[y * xblocks + x]);
	OutputDebugString(buf);
}
			if (matchp[y * xblocks + x] > highest_matchp)
			{
				highest_matchp = matchp[y * xblocks + x];
			}
		}
	}
	highest_matchc = 0;
	for (y = 0; y < yblocks; y++)
	{
		for (x = 0; x < xblocks; x++)
		{
if (frame == 44 && matchc[y * xblocks + x] > 2500)
{
	sprintf(buf, "%d/%d = %d\n", x, y, matchc[y * xblocks + x]);
	OutputDebugString(buf);
}
			if (matchc[y * xblocks + x] > highest_matchc)
			{
				highest_matchc = matchc[y * xblocks + x];
			}
		}
	}
#endif
	if (post != POST_NONE)
	{
		highest_sump = 0;
		for (y = 0; y < yblocks; y++)
		{
			for (x = 0; x < xblocks; x++)
			{
				if (sump[y * xblocks + x] > highest_sump)
				{
					highest_sump = sump[y * xblocks + x];
				}
			}
		}
		highest_sumc = 0;
		for (y = 0; y < yblocks; y++)
		{
			for (x = 0; x < xblocks; x++)
			{
				if (sumc[y * xblocks + x] > highest_sumc)
				{
					highest_sumc = sumc[y * xblocks + x];
				}
			}
		}
	}
#ifdef WINDOWED_MATCH
	CacheInsert(frame, highest_matchp, highest_sump, highest_matchc, highest_sumc);
#else
	CacheInsert(frame, p, highest_sump, c, highest_sumc);
#endif
}

AVSValue __cdecl Telecide::Create_Telecide(AVSValue args, void* user_data, IScriptEnvironment* env)
{
	char path[1024], buf[255];
	char *p;
	int back = NO_BACK;
	bool chroma = true;
	int guide = GUIDE_NONE;
	int gthresh = 10.0;
	int post = POST_FULL;
	int vthresh = 50.0;
	int bthresh = 50.0;
	int dthresh = 7.0;
	bool blend = false;
	int nt = 10;
	int y0 = 0;
	int y1 = 0;
	bool hints = true;
	bool show = false;
	bool debug = false;
	char ovr[255], *ovrp;
	FILE *f;

	try
	{
		const char* plugin_dir = env->GetVar("$PluginDir$").AsString();
		strcpy(path, plugin_dir);
		strcat(path, "\\Telecide.def");
		if ((f = fopen(path, "r")) != NULL)
		{
			while(fgets(buf, 80, f) != 0)
			{
				if (strncmp(buf, "back=", 5) == 0)
				{
					p = buf;
					while(*p++ != '=');
					back = atoi(p);
				}
				if (strncmp(buf, "chroma=", 7) == 0)
				{
					p = buf;
					while(*p++ != '=');
					chroma = (*p == 't' ? true : false);
				}
				if (strncmp(buf, "guide=", 6) == 0)
				{
					p = buf;
					while(*p++ != '=');
					guide = atoi(p);
				}
				if (strncmp(buf, "gthresh=", 8) == 0)
				{
					p = buf;
					while(*p++ != '=');
					gthresh = atof(p);
				}
				if (strncmp(buf, "post=", 5) == 0)
				{
					p = buf;
					while(*p++ != '=');
					post = atoi(p);
				}
				if (strncmp(buf, "vthresh=", 8) == 0)
				{
					p = buf;
					while(*p++ != '=');
					vthresh = atof(p);
				}
				if (strncmp(buf, "bthresh=", 8) == 0)
				{
					p = buf;
					while(*p++ != '=');
					bthresh = atof(p);
				}
				if (strncmp(buf, "dthresh=", 8) == 0)
				{
					p = buf;
					while(*p++ != '=');
					dthresh = atof(p);
				}
				if (strncmp(buf, "blend=", 6) == 0)
				{
					p = buf;
					while(*p++ != '=');
					blend = (*p == 't' ? true : false);
				}
				if (strncmp(buf, "nt=", 3) == 0)
				{
					p = buf;
					while(*p++ != '=');
					nt = atoi(p);
				}
				if (strncmp(buf, "y0=", 3) == 0)
				{
					p = buf;
					while(*p++ != '=');
					y0 = atoi(p);
				}
				if (strncmp(buf, "y1=", 3) == 0)
				{
					p = buf;
					while(*p++ != '=');
					y1 = atoi(p);
				}
				if (strncmp(buf, "hints=", 6) == 0)
				{
					p = buf;
					while(*p++ != '=');
					hints = (*p == 't' ? true : false);
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
	}

    return new Telecide(args[0].AsClip(),
						args[1].AsInt(back),		// match mode
						args[2].AsInt(guide),		// guidance mode
						args[3].AsFloat(gthresh),	// guidance threshold
						args[4].AsInt(post),		// postprocessing mode
						args[5].AsBool(chroma),		// include chroma in match calculations
						args[6].AsFloat(vthresh),	// film versus video threshold
						args[7].AsFloat(bthresh),	// backward matching threshold
						args[8].AsFloat(dthresh),	// deinterlacing threshold
						args[9].AsBool(blend),		// blend instead of interpolate
						args[10].AsInt(nt),			// noise threshold
						args[11].AsInt(y0),			// y0
						args[12].AsInt(y1),			// y1
						args[13].AsBool(hints),		// pass hints to Decimate()
						args[14].AsString(ovr),		// overrides file
						args[15].AsBool(show),		// overlay debug info on frames
						args[16].AsBool(debug),		// send debug info to console
						env);
}
