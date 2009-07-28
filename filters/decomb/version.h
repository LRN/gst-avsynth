/*
	Header file for Decomb Avisynth plugin.

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

#define VERSION "5.2.3"

#define USE_MMX //asm parts are not ported completely
#ifdef USE_MMX
#define DEINTERLACE_MMX_BUILD
#define INTERPOLATE_MMX_BUILD
#define BLEND_MMX_BUILD
#define DECIMATE_MMX_BUILD
#endif
