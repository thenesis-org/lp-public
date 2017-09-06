/*
   Copyright (C) 1996-1997 Id Software, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "Client/client.h"
#include "Client/console.h"
#include "Common/cmd.h"
#include "Common/cvar.h"
#include "Common/sys.h"
#include "Rendering/glquake.h"

#include <stdlib.h>
#include <string.h>

/*
   Grab six views for environment mapping tests
 */
void R_Envmap_f()
{
	int bufferSize = 256 * 256 * 4;
	byte * buffer = malloc(bufferSize);

	envmap = true;

	r_refdef.vrect.x = 0;
	r_refdef.vrect.y = 0;
	r_refdef.vrect.width = 256;
	r_refdef.vrect.height = 256;

	r_refdef.viewangles[0] = 0;
	r_refdef.viewangles[1] = 0;
	r_refdef.viewangles[2] = 0;
	GL_BeginRendering(&glx, &gly, &glwidth, &glheight);
	R_RenderView();
	glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile("env0.rgb", buffer, bufferSize);

	r_refdef.viewangles[1] = 90;
	GL_BeginRendering(&glx, &gly, &glwidth, &glheight);
	R_RenderView();
	glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile("env1.rgb", buffer, bufferSize);

	r_refdef.viewangles[1] = 180;
	GL_BeginRendering(&glx, &gly, &glwidth, &glheight);
	R_RenderView();
	glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile("env2.rgb", buffer, bufferSize);

	r_refdef.viewangles[1] = 270;
	GL_BeginRendering(&glx, &gly, &glwidth, &glheight);
	R_RenderView();
	glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile("env3.rgb", buffer, bufferSize);

	r_refdef.viewangles[0] = -90;
	r_refdef.viewangles[1] = 0;
	GL_BeginRendering(&glx, &gly, &glwidth, &glheight);
	R_RenderView();
	glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile("env4.rgb", buffer, bufferSize);

	r_refdef.viewangles[0] = 90;
	r_refdef.viewangles[1] = 0;
	GL_BeginRendering(&glx, &gly, &glwidth, &glheight);
	R_RenderView();
	glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile("env5.rgb", buffer, bufferSize);

	envmap = false;
	GL_EndRendering();
}

/*
   Translates a skin texture by the per-player color lookup
 */
void R_TranslatePlayerSkin(int playernum)
{
	model_t *model;
	aliashdr_t *paliashdr;
	byte *original;
	byte *inrow;
	unsigned frac, fracstep;

	GL_DisableMultitexture();

	int top = cl.scores[playernum].colors & 0xf0;
	int bottom = (cl.scores[playernum].colors & 15) << 4;

	byte translate[256];
	for (int i = 0; i < 256; i++)
		translate[i] = i;

	for (int i = 0; i < 16; i++)
	{
		if (top < 128) // the artists made some backwards ranges.  sigh.
			translate[TOP_RANGE + i] = top + i;
		else
			translate[TOP_RANGE + i] = top + 15 - i;

		if (bottom < 128)
			translate[BOTTOM_RANGE + i] = bottom + i;
		else
			translate[BOTTOM_RANGE + i] = bottom + 15 - i;
	}

	//
	// locate the original skin pixels
	//
    entity_t *e = &cl_entities[1 + playernum];
	currententity = e;
	model = e->model;
	if (!model)
		return;      // player doesn't have a model yet

	if (model->type != mod_alias)
		return;                        // only translate skins on alias models

	paliashdr = (aliashdr_t *)Mod_Extradata(model);
	int s = paliashdr->skinwidth * paliashdr->skinheight;
	if (e->skinnum < 0 || e->skinnum >= paliashdr->numskins)
	{
		Con_Printf("(%d): Invalid player skin #%d\n", playernum, e->skinnum);
		original = (byte *)paliashdr + paliashdr->texels[0];
	}
	else
		original = (byte *)paliashdr + paliashdr->texels[e->skinnum];
	if (s & 3)
		Sys_Error("R_TranslateSkin: s&3");

	// because this happens during gameplay, do it fast
	// instead of sending it through gl_upload 8
	GL_Bind(playertextures + playernum);

	int inwidth = paliashdr->skinwidth;
	int inheight = paliashdr->skinheight;

	int scaled_width = gl_max_size.value < 512 ? gl_max_size.value : 512;
	int scaled_height = gl_max_size.value < 256 ? gl_max_size.value : 256;

	// allow users to crunch sizes down even more if they want
	scaled_width >>= (int)gl_playermip.value;
	scaled_height >>= (int)gl_playermip.value;

	unsigned translate32[256];
	for (int i = 0; i < 256; i++)
		translate32[i] = d_8to24table[translate[i]];

	unsigned *pixels = malloc(scaled_width * scaled_height * sizeof(unsigned));
	unsigned *out = pixels;
	fracstep = inwidth * 0x10000 / scaled_width;
	for (int i = 0; i < scaled_height; i++, out += scaled_width)
	{
		inrow = original + inwidth * (i * inheight / scaled_height);
		frac = fracstep >> 1;
		for (int j = 0; j < scaled_width; j += 4)
		{
			out[j] = translate32[inrow[frac >> 16]];
			frac += fracstep;
			out[j + 1] = translate32[inrow[frac >> 16]];
			frac += fracstep;
			out[j + 2] = translate32[inrow[frac >> 16]];
			frac += fracstep;
			out[j + 3] = translate32[inrow[frac >> 16]];
			frac += fracstep;
		}
	}
	#if defined(EGLW_GLES1)
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	#endif
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	#if defined(EGLW_GLES1)
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
	#else
	glGenerateMipmap(GL_TEXTURE_2D);
	#endif
	free(pixels);

	oglwSetTextureBlending(0, GL_MODULATE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

/*
   For program optimization
 */
void R_TimeRefresh_f()
{
	int i;
	float start, stop, time;

	glFinish();

	start = Sys_FloatTime();
	for (i = 0; i < 128; i++)
	{
		r_refdef.viewangles[1] = i / 128.0f * 360.0f;
		R_RenderView();
	}

	glFinish();
	stop = Sys_FloatTime();
	time = stop - start;
	Con_Printf("%f seconds (%f fps)\n", time, 128 / time);

	GL_EndRendering();
}
