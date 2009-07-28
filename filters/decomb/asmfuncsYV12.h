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

#include "internal.h"


// Telecide / FieldDeinterlace helpers
#ifdef DEINTERLACE_MMX_BUILD

void isse_blend_plane(BYTE* org, BYTE* src_prev, BYTE* src_next, BYTE* dmaskp, int w);
void isse_interpolate_plane(BYTE* org, BYTE* src_prev, BYTE* src_next, BYTE* dmaskp, int w);
void mmx_createmask_plane(const BYTE* srcp, const BYTE* prev_p, const BYTE* next_p, BYTE* fmaskp, BYTE* dmaskp, int threshold, int dthreshold, int row_size);
void mmx_createmask_plane_single(const BYTE* srcp, const BYTE* prev_p, const BYTE* next_p, BYTE* dmaskp, int dthreshold, int row_size);

#endif



#ifdef DECIMATE_MMX_BUILD

// Decimate Helpers
void isse_blend_decimate_plane(BYTE* dst, BYTE* src,  BYTE* src_next, int w, int h, int dst_pitch, int src_pitch, int src_next_pitch);
int isse_scenechange_32(const BYTE* c_plane, const BYTE* tplane, int height, int width, int c_pitch, int t_pitch);
int isse_scenechange_16(const BYTE* c_plane, const BYTE* tplane, int height, int width, int c_pitch, int t_pitch);
int isse_scenechange_8(const BYTE* c_plane, const BYTE* tplane, int height, int width, int c_pitch, int t_pitch);

#endif
