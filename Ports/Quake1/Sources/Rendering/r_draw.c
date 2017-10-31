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

#include "Client/console.h"
#include "Client/sbar.h"
#include "Common/cmd.h"
#include "Common/cvar.h"
#include "Common/quakedef.h"
#include "Common/wad.h"
#include "Common/sys.h"
#include "Common/zone.h"
#include "Rendering/r_private.h"
#include "Rendering/r_video.h"

#include "OpenGLES/OpenGLWrapper.h"

#include <stdlib.h>
#include <string.h>

//--------------------------------------------------------------------------------
// Scrap textures.
//--------------------------------------------------------------------------------
/*
   scrap allocation
   Allocate all the little status bar objects into a single texture
   to crutch up stupid hardware / drivers
 */

#define ScrapNb 2
#define ScrapBlockWidth 256
#define ScrapBlockHeight 256

static int r_Scrap_allocated[ScrapNb][ScrapBlockWidth];
static byte r_Scrap_texels[ScrapNb][ScrapBlockWidth * ScrapBlockHeight * 4];
static bool r_Scrap_dirty;
static int r_Scrap_textures[ScrapNb];
static int r_Scrap_uploadNb;

// returns a texture number and the position inside it
static int Scrap_allocBlock(int w, int h, int *x, int *y)
{
	for (int scrapIndex = 0; scrapIndex < ScrapNb; scrapIndex++)
	{
		int best = ScrapBlockHeight;
		for (int i = 0; i < ScrapBlockWidth - w; i++)
		{
			int best2 = 0;
            int j;
			for (j = 0; j < w; j++)
			{
				if (r_Scrap_allocated[scrapIndex][i + j] >= best)
					break;
				if (r_Scrap_allocated[scrapIndex][i + j] > best2)
					best2 = r_Scrap_allocated[scrapIndex][i + j];
			}
			if (j == w) // this is a valid spot
			{
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > ScrapBlockHeight)
			continue;

		for (int i = 0; i < w; i++)
			r_Scrap_allocated[scrapIndex][*x + i] = best + h;

		return scrapIndex;
	}

	Sys_Error("Scrap_allocBlock: full");
	return -1;
}

static void Scrap_upload()
{
	r_Scrap_uploadNb++;
    oglwSetCurrentTextureUnitForced(0);
	for (int scrapIndex = 0; scrapIndex < ScrapNb; scrapIndex++)
	{
		oglwBindTextureForced(0, r_Scrap_textures[scrapIndex]);
		R_Texture_load(r_Scrap_texels[scrapIndex], ScrapBlockWidth, ScrapBlockHeight, false, true, 0, d_8to24table);
	}
	r_Scrap_dirty = false;
}

//--------------------------------------------------------------------------------
// Draw.
//--------------------------------------------------------------------------------
static byte *draw_chars; // 8*8 graphic characters
qpic_t *draw_disc;
static qpic_t *draw_backtile;

static int translate_texture;
static int char_texture;

typedef struct
{
	int texnum;
	float sl, tl, sh, th;
} glpic_t;

static byte conback_buffer[sizeof(qpic_t) + sizeof(glpic_t)];
static qpic_t *conback = (qpic_t *)&conback_buffer;

typedef struct cachepic_s
{
	char name[MAX_QPATH];
	qpic_t pic;
	byte padding[32]; // for appended glpic
} cachepic_t;

#define MAX_CACHED_PICS 128
static cachepic_t menu_cachepics[MAX_CACHED_PICS];
static int menu_numcachepics;

static byte menuplyr_pixels[4096];

static int pic_texels;
static int pic_count;

static int Draw_loadPic(qpic_t *pic)
{
	return R_Texture_create("", pic->width, pic->height, pic->data, false, true, true, true);
}

qpic_t* Draw_loadPicFromWad(char *name)
{
	qpic_t *p = W_GetLumpName(name);
	glpic_t *gl = (glpic_t *)p->data;

	// load little ones into the scrap
	if (p->width < 64 && p->height < 64)
	{
		int x = 0, y = 0;
		int scrapIndex = Scrap_allocBlock(p->width, p->height, &x, &y);
        if (scrapIndex < 0)
            return NULL;
        byte *scrapData = r_Scrap_texels[scrapIndex];
		r_Scrap_dirty = true;
		int k = 0;
		for (int i = 0; i < p->height; i++)
			for (int j = 0; j < p->width; j++, k++)
				scrapData[(y + i) * ScrapBlockWidth + x + j] = p->data[k];

		gl->texnum = r_Scrap_textures[scrapIndex];
		gl->sl = (x + 0.01f) / (float)ScrapBlockWidth;
		gl->sh = (x + p->width - 0.01f) / (float)ScrapBlockWidth;
		gl->tl = (y + 0.01f) / (float)ScrapBlockWidth;
		gl->th = (y + p->height - 0.01f) / (float)ScrapBlockWidth;

		pic_count++;
		pic_texels += p->width * p->height;
	}
	else
	{
		gl->texnum = Draw_loadPic(p);
		gl->sl = 0;
		gl->sh = 1;
		gl->tl = 0;
		gl->th = 1;
	}
	return p;
}

qpic_t* Draw_CachePic(char *path)
{
	cachepic_t *pic;
	int i;
	for (pic = menu_cachepics, i = 0; i < menu_numcachepics; pic++, i++)
		if (!strcmp(path, pic->name))
			return &pic->pic;

	if (menu_numcachepics == MAX_CACHED_PICS)
    {
		Sys_Error("menu_numcachepics == MAX_CACHED_PICS");
        return NULL;
    }
	menu_numcachepics++;
	Q_strncpy(pic->name, path, MAX_QPATH);

	//
	// load the pic from disk
	//
	qpic_t *dat = (qpic_t *)COM_LoadTempFile(path);
	if (!dat)
		Sys_Error("Draw_CachePic: failed to load %s", path);
	SwapPic(dat);

	// HACK HACK HACK --- we need to keep the bytes for
	// the translatable player picture just for the menu
	// configuration dialog
	if (!strcmp(path, "gfx/menuplyr.lmp"))
		memcpy(menuplyr_pixels, dat->data, dat->width * dat->height);

	pic->pic.width = dat->width;
	pic->pic.height = dat->height;

	glpic_t *gl = (glpic_t *)pic->pic.data;
	gl->texnum = Draw_loadPic(dat);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return &pic->pic;
}

void Draw_CharToConback(int num, byte *dest)
{
	int row = num >> 4;
	int col = num & 15;
	byte *source = draw_chars + (row << 10) + (col << 3);
	int drawline = 8;
	while (drawline--)
	{
		for (int x = 0; x < 8; x++)
			if (source[x] != 255)
				dest[x] = 0x60 + source[x];
		source += 128;
		dest += 320;
	}
}

void Draw_Init()
{
	// load the console background and the charset
	// by hand, because we need to write the version
	// string into the background before turning
	// it into a texture
	draw_chars = W_GetLumpName("conchars");
	for (int i = 0; i < 256 * 64; i++)
		if (draw_chars[i] == 0)
			draw_chars[i] = 255;
	// proper transparent color

	// now turn them into textures
	char_texture = R_Texture_create("charset", 128, 128, draw_chars, false, true, false, true);

	int start = Hunk_LowMark();

	qpic_t *cb = (qpic_t *)COM_LoadTempFile("gfx/conback.lmp");
	if (!cb)
		Sys_Error("Couldn't load gfx/conback.lmp");
	SwapPic(cb);

	// hack the version number directly into the pic
	char ver[40];
	sprintf(ver, "(Thenesis %s) %4.2f", QUAKE_VERSION_NAME, (float)QUAKE_HOST_VERSION);
	byte *dest = cb->data + 320 * 186 + 320 - 11 - 8 * strlen(ver);
	int y = strlen(ver);
	for (int x = 0; x < y; x++)
		Draw_CharToConback(ver[x], dest + (x << 3));

	conback->width = cb->width;
	conback->height = cb->height;
    byte *ncdata = cb->data;

	glpic_t *gl = (glpic_t *)conback->data;
	gl->texnum = R_Texture_create("conback", conback->width, conback->height, ncdata, true, false, true, true);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;
	conback->width = vid.width;
	conback->height = vid.height;

	// free loaded console
	Hunk_FreeToLowMark(start);

	// save a texture slot for translated picture
	translate_texture = R_Texture_create("", 64, 64, NULL, true, false, false, false);

	// save slots for scraps
    for (int scrapIndex = 0; scrapIndex < ScrapNb; scrapIndex++)
        r_Scrap_textures[scrapIndex] = R_Texture_create("", ScrapBlockWidth, ScrapBlockHeight, NULL, true, false, false, false);

	// get the other pics we need
	draw_disc = Draw_loadPicFromWad("disc");
	draw_backtile = Draw_loadPicFromWad("backtile");
}

//--------------------------------------------------------------------------------
// Draw character.
//--------------------------------------------------------------------------------
static int Draw_CharCount = 0;

void Draw_CharBegin()
{
	if (Draw_CharCount == 0)
	{
		oglwBindTexture(0, char_texture);
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
   Draws one 8*8 graphics character with 0 being transparent.
   It can be clipped to the top of the screen to allow the console to be
   smoothly scrolled off.
 */
void Draw_Character(int x, int y, int num)
{
	if (num == 32)
		return; // space
	if (y <= -8)
		return; // totally off screen

    Draw_CharBegin();
	OglwVertex *v = oglwAllocateVertex(4);
    if (v != NULL)
    {
        num &= 255;
        int row = num >> 4;
        int col = num & 15;
        float frow = row * 0.0625f;
        float fcol = col * 0.0625f;
        float size = 0.0625f;
        AddQuad2D_T1(v, x, y, x + 8, y + 8, fcol, frow, fcol + size, frow + size);
    }
    Draw_CharEnd();
}

void Draw_String(int x, int y, char *str)
{
    Draw_CharBegin();
	while (*str)
	{
		Draw_Character(x, y, *str);
		str++;
		x += 8;
	}
    Draw_CharEnd();
}

//--------------------------------------------------------------------------------
// Draw pics.
//--------------------------------------------------------------------------------
void Draw_AlphaPic(int x, int y, qpic_t *pic, float alpha)
{
	if (r_Scrap_dirty)
		Scrap_upload();

    oglwEnableAlphaTest(false);

	glpic_t *gl = (glpic_t *)pic->data;

	oglwBindTexture(0, gl->texnum);

	oglwBegin(GL_QUADS);
	OglwVertex *v = oglwAllocateVertex(4);
    if (v != NULL)
        AddQuad2D_CT1(v, x, y, x + pic->width, y + pic->height, 1.0f, 1.0f, 1.0f, alpha, gl->sl, gl->tl, gl->sh, gl->th);
	oglwEnd();

    oglwEnableAlphaTest(true);
}

void Draw_Pic(int x, int y, qpic_t *pic)
{
    if (pic == NULL)
        return;
	if (r_Scrap_dirty)
		Scrap_upload();

	glpic_t *gl = (glpic_t *)pic->data;

	oglwBindTexture(0, gl->texnum);

	oglwBegin(GL_QUADS);
	OglwVertex *v = oglwAllocateVertex(4);
    if (v != NULL)
        AddQuad2D_T1(v, x, y, x + pic->width, y + pic->height, gl->sl, gl->tl, gl->sh, gl->th);
	oglwEnd();
}

void Draw_TransPic(int x, int y, qpic_t *pic)
{
    if (pic == NULL)
        return;
	if (x < 0 || (x + pic->width) > vid.width || y < 0 || (y + pic->height) > vid.height)
		Sys_Error("Draw_TransPic: bad coordinates");
	Draw_Pic(x, y, pic);
}

/*
   Only used for the player color selection menu
 */
void Draw_TransPicTranslate(int x, int y, qpic_t *pic, byte *translation)
{
    if (pic == NULL)
        return;
	unsigned *trans = malloc(64 * 64 * sizeof(unsigned));
	unsigned *dest = trans;
	for (int v = 0; v < 64; v++, dest += 64)
	{
		byte *src = &menuplyr_pixels[((v * pic->height) >> 6) * pic->width];
		for (int u = 0; u < 64; u++)
		{
			int p = src[(u * pic->width) >> 6];
			if (p == 255)
				dest[u] = 0x000000ff;
			else
				dest[u] = d_8to24table[translation[p]];
		}
	}

    oglwSetCurrentTextureUnitForced(0);
	oglwBindTextureForced(0, translate_texture);
    R_Texture_load(trans, 64, 64, true, false, 0, NULL);
    
	free(trans);

	oglwBegin(GL_QUADS);
	OglwVertex *v = oglwAllocateVertex(4);
    if (v != NULL)
        AddQuad2D_T1(v, x, y, x + pic->width, y + pic->height, 0.0f, 0.0f, 1.0f, 1.0f);
	oglwEnd();
}

void Draw_ConsoleBackground(int lines)
{
	int y = (vid.height * 3) >> 2;
	if (lines > y)
		Draw_Pic(0, lines - vid.height, conback);
	else
		Draw_AlphaPic(0, lines - vid.height, conback, (float)(1.2f * lines) / y);
}

/*
   This repeats a 64*64 tile graphic to fill the screen around a sized down
   refresh window.
 */
void Draw_TileClear(int x, int y, int w, int h)
{
    int *tileTextureId = (int *)draw_backtile->data;
    oglwSetCurrentTextureUnitForced(0);
	oglwBindTextureForced(0, *tileTextureId);
	oglwBegin(GL_QUADS);
	OglwVertex *v = oglwAllocateVertex(4);
	float k = 1.0f / 64.0f;
    if (v != NULL)
        AddQuad2D_T1(v, x, y, x + w, y + h, x * k, y * k, (x + w) * k, (y + h) * k);
	oglwEnd();
}

/*
   Fills a box of pixels with a single color
 */
void Draw_Fill(int x, int y, int w, int h, int c)
{
	oglwEnableTexturing(0, false);

	oglwBegin(GL_QUADS);
	OglwVertex *v = oglwAllocateVertex(4);
	float k = 1.0f / 255.0f;
    if (v != NULL)
        AddQuad2D_C(v, x, y, x + w, y + h, host_basepal[c * 3] * k, host_basepal[c * 3 + 1] * k, host_basepal[c * 3 + 2] * k, 1.0f);
	oglwEnd();

	oglwEnableTexturing(0, true);
}

void Draw_FadeScreen()
{
	oglwEnableTexturing(0, false);

	oglwBegin(GL_QUADS);
	OglwVertex *v = oglwAllocateVertex(4);
    if (v != NULL)
        AddQuad2D_C(v, 0, 0, vid.width, vid.height, 0.0f, 0.0f, 0.0f, 0.8f);
	oglwEnd();

	oglwEnableTexturing(0, true);

	Sbar_Changed();
}

/*
   Draws the little blue disc in the corner of the screen.
   Call before beginning any disc IO.
 */
void Draw_BeginDisc()
{
	if (!draw_disc)
		return;

	Draw_Pic(vid.width - 24, 0, draw_disc);
}

/*
   Erases the disc icon.
   Call after completing any disc IO
 */
void Draw_EndDisc()
{
}


