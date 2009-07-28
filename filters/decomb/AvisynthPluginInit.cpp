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
*/

#include "telecide.h"

AVSValue __cdecl Create_Telecide(AVSValue args, void* user_data, IScriptEnvironment* env);
AVSValue __cdecl Create_FieldDeinterlace(AVSValue args, void* user_data, IScriptEnvironment* env);
AVSValue __cdecl Create_Decimate(AVSValue args, void* user_data, IScriptEnvironment* env);
AVSValue __cdecl Create_IsCombed(AVSValue args, void* user_data, IScriptEnvironment* env);

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit2(IScriptEnvironment* env)
{
    env->AddFunction("Telecide",
		"c[back]i[guide]i[gthresh]f[post]i[chroma]b[vthresh]f[bthresh]f[dthresh]f[blend]b[nt]i[y0]i[y1]i[hints]b[ovr]s[show]b[debug]b",
		"video/x-raw-yuv, format=(fourcc)YV12; video/x-raw-yuv, format=(fourcc)YUY2",
                "video/x-raw-yuv, format=(fourcc)YV12; video/x-raw-yuv, format=(fourcc)YUY2",
                Telecide::Create_Telecide, 0);
    env->AddFunction("FieldDeinterlace",
                "c[full]b[threshold]i[dthreshold]i[map]b[blend]b[chroma]b[ovr]s[show]b[debug]b",
		"video/x-raw-yuv, format=(fourcc)YV12; video/x-raw-yuv, format=(fourcc)YUY2",
                "video/x-raw-yuv, format=(fourcc)YV12; video/x-raw-yuv, format=(fourcc)YUY2",
                Create_FieldDeinterlace, 0);
    env->AddFunction("Decimate", "c[cycle]i[mode]i[threshold]f[threshold2]f[quality]i[ovr]s[show]b[debug]b",
		"video/x-raw-yuv, format=(fourcc)YV12; video/x-raw-yuv, format=(fourcc)YUY2",
                "video/x-raw-yuv, format=(fourcc)YV12; video/x-raw-yuv, format=(fourcc)YUY2",
                Create_Decimate, 0);
    env->AddFunction("IsCombed", "c[threshold]i",
		"video/x-raw-yuv, format=(fourcc)YV12; video/x-raw-yuv, format=(fourcc)YUY2",
                "video/x-raw-yuv, format=(fourcc)YV12; video/x-raw-yuv, format=(fourcc)YUY2",
                Create_IsCombed, 0);
	return 0;
}
