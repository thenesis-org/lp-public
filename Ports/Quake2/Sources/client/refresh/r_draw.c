/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * =======================================================================
 *
 * Drawing of all images that are not textures
 *
 * =======================================================================
 */

#include "local.h"

#include <stdbool.h>

image_t *draw_chars;

extern qboolean scrap_dirty;
void Scrap_Upload();

extern unsigned r_rawpalette[256];

static cvar_t *gl_nolerp_list;

void Draw_InitLocal()
{
	/* don't bilerp characters and crosshairs */
	gl_nolerp_list = Cvar_Get("gl_nolerp_list", "pics/conchars.pcx pics/ch1.pcx pics/ch2.pcx pics/ch3.pcx", 0);

	/* load console characters */
	draw_chars = R_FindImage("pics/conchars.pcx", it_pic);
}

static int Draw_CharCount = 0;

void Draw_CharBegin()
{
	if (Draw_CharCount == 0)
	{
		oglwBindTexture(0, draw_chars->texnum);
		oglwBegin(GL_QUADS);
	}
	Draw_CharCount++;
}

void Draw_CharEnd()
{
	Draw_CharCount--;
	if (Draw_CharCount == 0)
	{
		oglwEnd();
	}
}

/*
 * Draws one 8*8 graphics character with 0 being transparent.
 * It can be clipped to the top of the screen to allow the console to be
 * smoothly scrolled off.
 */
void Draw_CharScaled(int x, int y, int num, float scale)
{
	int row, col;
	float frow, fcol, size, scaledSize;

	num &= 255;

	if ((num & 127) == 32)
		return; /* space */

	if (y <= -8)
		return; /* totally off screen */

	row = num >> 4;
	col = num & 15;

	frow = row * 0.0625f;
	fcol = col * 0.0625f;
	size = 0.0625f;

	scaledSize = 8 * scale;

	bool begin = false;
	if (Draw_CharCount == 0)
	{
		begin = true;
		Draw_CharBegin();
	}

	OglwVertex *v = oglwAllocateVertex(4);
	AddQuad2D_T1(v, x, y, x + scaledSize, y + scaledSize, fcol, frow, fcol + size, frow + size);

	if (begin)
		Draw_CharEnd();
}

image_t* Draw_FindPic(char *name)
{
	image_t *gl;
	char fullname[MAX_QPATH];

	if ((name[0] != '/') && (name[0] != '\\'))
	{
		Com_sprintf(fullname, sizeof(fullname), "pics/%s.pcx", name);
		gl = R_FindImage(fullname, it_pic);
	}
	else
	{
		gl = R_FindImage(name + 1, it_pic);
	}

	return gl;
}

void Draw_GetPicSize(int *w, int *h, char *pic)
{
	image_t *gl = Draw_FindPic(pic);
	if (!gl)
	{
		*w = *h = -1;
		return;
	}

	*w = gl->width;
	*h = gl->height;
}

void Draw_StretchPic(int x, int y, int w, int h, char *pic)
{
	image_t *gl = Draw_FindPic(pic);
	if (!gl)
	{
		VID_Printf(PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	if (scrap_dirty)
		Scrap_Upload();

	oglwBindTexture(0, gl->texnum);
	oglwBegin(GL_QUADS);
	OglwVertex *v = oglwAllocateVertex(4);
	AddQuad2D_T1(v, x, y, x + w, y + h, gl->sl, gl->tl, gl->sh, gl->th);
	oglwEnd();
}

void Draw_Pic(int x, int y, char *pic)
{
	Draw_PicScaled(x, y, pic, 1.0f);
}

void Draw_PicScaled(int x, int y, char *pic, float factor)
{
	image_t *gl = Draw_FindPic(pic);
	if (!gl)
	{
		VID_Printf(PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	if (scrap_dirty)
		Scrap_Upload();

	GLfloat w = gl->width * factor;
	GLfloat h = gl->height * factor;

	oglwBindTexture(0, gl->texnum);
	oglwBegin(GL_QUADS);
	OglwVertex *v = oglwAllocateVertex(4);
	AddQuad2D_T1(v, x, y, x + w, y + h, gl->sl, gl->tl, gl->sh, gl->th);
	oglwEnd();
}

/*
 * This repeats a 64*64 tile graphic to fill
 * the screen around a sized down
 * refresh window.
 */
void Draw_TileClear(int x, int y, int w, int h, char *pic)
{
	image_t *image = Draw_FindPic(pic);
	if (!image)
	{
		VID_Printf(PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	oglwBindTexture(0, image->texnum);
	oglwBegin(GL_QUADS);
	OglwVertex *v = oglwAllocateVertex(4);
	float s = 1.0f / 64.0f;
	AddQuad2D_T1(v, x, y, x + w, y + h, x * s, y * s, (x + w) * s, (y + h) * s);
	oglwEnd();
}

/*
 * Fills a box of pixels with a single color
 */
void Draw_Fill(int x, int y, int w, int h, int c)
{
	if ((unsigned)c > 255)
		VID_Error(ERR_FATAL, "Draw_Fill: bad color");

	oglwEnableTexturing(0, GL_FALSE);
	oglwBegin(GL_QUADS);
	OglwVertex *v = oglwAllocateVertex(4);
	unsigned char *pc = d_8to24table[c];
	AddQuad2D_C(v, x, y, x + w, y + h, pc[0] * (1.0f / 255.0f), pc[1] * (1.0f / 255.0f), pc[2] * (1.0f / 255.0f), 1.0f);
	oglwEnd();
	oglwEnableTexturing(0, GL_TRUE);
}

void Draw_FadeScreen()
{
	oglwEnableTexturing(0, GL_FALSE);

	oglwBegin(GL_QUADS);
	OglwVertex *v = oglwAllocateVertex(4);
	AddQuad2D_C(v, 0, 0, vid.width, vid.height, 0.0f, 0.0f, 0.0f, 0.8f);
	oglwEnd();

	oglwEnableTexturing(0, GL_TRUE);
}

void Draw_StretchRaw(int x, int y, int w, int h, int cols, int rows, byte *data)
{
	unsigned image32[256 * 256];
	//unsigned char image8[256 * 256];
	int i, j, trows;
	byte *source;
	int frac, fracstep;
	float hscale;
	int row;

	if (rows <= 256)
	{
		hscale = 1;
		trows = rows;
	}
	else
	{
		hscale = rows / 256.0f;
		trows = 256;
	}

	float t = rows * hscale / 256 - 1.0f / 512.0f;

	{
		unsigned *dest;

		for (i = 0; i < trows; i++)
		{
			row = (int)(i * hscale);
			if (row > rows)
				break;

			source = data + cols * row;
			dest = &image32[i * 256];
			fracstep = cols * 0x10000 / 256;
			frac = fracstep >> 1;

			for (j = 0; j < 256; j++)
			{
				dest[j] = r_rawpalette[source[frac >> 16]];
				frac += fracstep;
			}
		}

		oglwSetCurrentTextureUnitForced(0);
		oglwBindTextureForced(0, 0);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
			256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE,
			image32);
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	oglwBegin(GL_QUADS);
	OglwVertex *v = oglwAllocateVertex(4);
	AddQuad2D_T1(v, x, y, x + w, y + h, 1.0f / 512.0f, 1.0f / 512.0f, 511.0f / 512.0f, t);
	oglwEnd();
}

int Draw_GetPalette()
{
	byte *pic, *pal;
	int width, height;

	/* get the palette */
	LoadPCX("pics/colormap.pcx", &pic, &pal, &width, &height);

	if (!pal)
		VID_Error(ERR_FATAL, "Couldn't load pics/colormap.pcx");

	for (int i = 0; i < 255; i++)
	{
		unsigned char *sc = &pal[i * 3];
		unsigned char *pc = d_8to24table[i];
		pc[0] = sc[0];
		pc[1] = sc[1];
		pc[2] = sc[2];
	}

	// 255 is transparent.
	{
		unsigned char *pc = d_8to24table[255];
		pc[0] = 0;
		pc[1] = 0;
		pc[2] = 0;
	}

	free(pic);
	free(pal);

	return 0;
}
