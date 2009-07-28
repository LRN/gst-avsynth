class FieldDeinterlace: public GenericVideoFilter
{
	bool full, full_saved, debug, blend, chroma, map, show;
	int threshold, dthreshold;
	const char *ovr;
	unsigned int *overrides, *overrides_p;
	PVideoFrame src, fmask, dmask;
	unsigned char *srcp, *srcp_saved, *p, *n;
	unsigned char *fmaskp, *fmaskp_saved, *dmaskp, *dmaskp_saved;
	unsigned char *finalp;
	int pitch, dpitch, w, h, force;
	bool clean;
	char buf[80];
	bool IsCombed(IScriptEnvironment* env);
	void MotionMap(int plane, IScriptEnvironment* env);
	void DeinterlacePlane(int plane, IScriptEnvironment* env);
	bool MotionMask_YUY2(int frame, IScriptEnvironment* env);

public:

	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
	PVideoFrame __stdcall GetFrameYUY2(int n, IScriptEnvironment* env);
	PVideoFrame __stdcall GetFrameYV12(int n, IScriptEnvironment* env);
	AVSValue ConditionalIsCombed(int n, IScriptEnvironment* env);

	FieldDeinterlace(PClip _child,
					bool _full, int _threshold, int _dthreshold, bool _map, bool _blend, bool _chroma,
					const char * _ovr, bool _show, bool _debug,
					IScriptEnvironment* env) : GenericVideoFilter(_child), full(_full), debug(_debug), blend(_blend), chroma(_chroma), map(_map), show(_show),
					threshold(_threshold), dthreshold(_dthreshold), ovr(_ovr)
	{
		FILE *f;
		int count = 0;
		char c, *d;
		unsigned int *p;

		if (!vi.IsYV12() && !vi.IsYUY2())
			env->ThrowError("FieldDeinterlace: YV12 or YUY2 data only");

		full_saved = full;

		/* Load manual overrides file if there is one. */
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
				p = overrides = (unsigned int *) malloc(3 * (count + 1) * sizeof(unsigned int));
				memset(overrides, 0xff, 3 * (count + 1) * sizeof(unsigned int));
				while(fgets(buf, 80, f) != 0)
				{
					d = buf;
					while (*d != ',' && *d != 0) d++;
					if (*d == 0)
					{
						sscanf(buf, "%d %c", p, &c);
						p++;
						*p = *(p-1);
						p++;
						*p = c;
						p++;
					}
					else
					{
						sscanf(buf, "%d,%d %c", p, p+1, &c);
						p += 2;
						*p = c;
						p++;
					}
				}
				fclose(f);
			}
		}
		if (debug)
		{
			sprintf(buf, "FieldDeinterlace %s by Donald Graft, Copyright 2003-2008\n", VERSION);
			OutputDebugString(buf);
		}

		/* Set the frame as progressive since we have deinterlacd it. */
		vi.SetFieldBased(false);

		/* The squares are needed for the comb detection algorithm. */
		threshold *= threshold;
		dthreshold *= dthreshold;
	}

	~FieldDeinterlace(void)
	{
		if (overrides != NULL) free(overrides);
	}
};

