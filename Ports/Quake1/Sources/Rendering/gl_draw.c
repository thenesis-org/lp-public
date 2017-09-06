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

// This is the only file outside the refresh that touches the
// vid buffer

#include "Client/console.h"
#include "Client/sbar.h"
#include "Common/cmd.h"
#include "Common/cvar.h"
#include "Common/quakedef.h"
#include "Common/wad.h"
#include "Common/sys.h"
#include "Common/zone.h"
#include "Rendering/glquake.h"
#include "Rendering/vid.h"

#include "OpenGLES/OpenGLWrapper.h"

#include <stdlib.h>
#include <string.h>

cvar_t gl_nobind = { "gl_nobind", "0" };
cvar_t gl_max_size = { "gl_max_size", "1024" };
cvar_t gl_picmip = { "gl_picmip", "0" };

byte *draw_chars; // 8*8 graphic characters
qpic_t *draw_disc;
qpic_t *draw_backtile;

static int translate_texture;
static int char_texture;

typedef struct
{
	int texnum;
	float sl, tl, sh, th;
} glpic_t;

static byte conback_buffer[sizeof(qpic_t) + sizeof(glpic_t)];
static qpic_t *conback = (qpic_t *)&conback_buffer;

int gl_filter_min = GL_LINEAR_MIPMAP_LINEAR;
int gl_filter_max = GL_LINEAR;

static int texels = 0;

typedef struct
{
	int texnum;
	char identifier[64];
	int width, height;
	qboolean mipmap;
} gltexture_t;

#define MAX_GLTEXTURES 1024
gltexture_t gltextures[MAX_GLTEXTURES];
int numgltextures;

static int GL_LoadPicTexture(qpic_t *pic);
static void GL_Upload8(byte *data, int width, int height, qboolean mipmap, qboolean alpha);

void GL_Bind(int texnum)
{
	if (gl_nobind.value)
		texnum = char_texture;
	if (currenttexture == texnum)
		return;

	currenttexture = texnum;
	glBindTexture(GL_TEXTURE_2D, texnum);
}

/*
   =============================================================================

   scrap allocation

   Allocate all the little status bar obejcts into a single texture
   to crutch up stupid hardware / drivers

   =============================================================================
 */

#define MAX_SCRAPS 2
#define BLOCK_WIDTH 256
#define BLOCK_HEIGHT 256

static int scrap_allocated[MAX_SCRAPS][BLOCK_WIDTH];
static byte scrap_texels[MAX_SCRAPS][BLOCK_WIDTH * BLOCK_HEIGHT * 4];
static qboolean scrap_dirty;
static int scrap_texnum;

// returns a texture number and the position inside it
int Scrap_AllocBlock(int w, int h, int *x, int *y)
{
	int i, j;
	int best, best2;
	int texnum;

	for (texnum = 0; texnum < MAX_SCRAPS; texnum++)
	{
		best = BLOCK_HEIGHT;

		for (i = 0; i < BLOCK_WIDTH - w; i++)
		{
			best2 = 0;

			for (j = 0; j < w; j++)
			{
				if (scrap_allocated[texnum][i + j] >= best)
					break;
				if (scrap_allocated[texnum][i + j] > best2)
					best2 = scrap_allocated[texnum][i + j];
			}
			if (j == w) // this is a valid spot
			{
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		for (i = 0; i < w; i++)
			scrap_allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Sys_Error("Scrap_AllocBlock: full");
	return -1;
}

static int scrap_uploads;

void Scrap_Upload()
{
	scrap_uploads++;
	for (int texnum = 0; texnum < MAX_SCRAPS; texnum++)
	{
		GL_Bind(scrap_texnum + texnum);
		GL_Upload8(scrap_texels[texnum], BLOCK_WIDTH, BLOCK_HEIGHT, false, true);
	}
	scrap_dirty = false;
}

//=============================================================================
/* Support Routines */

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

qpic_t* Draw_PicFromWad(char *name)
{
	qpic_t *p;
	glpic_t *gl;

	p = W_GetLumpName(name);
	gl = (glpic_t *)p->data;

	// load little ones into the scrap
	if (p->width < 64 && p->height < 64)
	{
		int x, y;
		int i, j, k;
		int texnum;

		texnum = Scrap_AllocBlock(p->width, p->height, &x, &y);
		scrap_dirty = true;
		k = 0;
		for (i = 0; i < p->height; i++)
			for (j = 0; j < p->width; j++, k++)
				scrap_texels[texnum][(y + i) * BLOCK_WIDTH + x + j] = p->data[k];

		texnum += scrap_texnum;
		gl->texnum = texnum;
		gl->sl = (x + 0.01f) / (float)BLOCK_WIDTH;
		gl->sh = (x + p->width - 0.01f) / (float)BLOCK_WIDTH;
		gl->tl = (y + 0.01f) / (float)BLOCK_WIDTH;
		gl->th = (y + p->height - 0.01f) / (float)BLOCK_WIDTH;

		pic_count++;
		pic_texels += p->width * p->height;
	}
	else
	{
		gl->texnum = GL_LoadPicTexture(p);
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
	qpic_t *dat;
	glpic_t *gl;

	for (pic = menu_cachepics, i = 0; i < menu_numcachepics; pic++, i++)
		if (!strcmp(path, pic->name))
			return &pic->pic;



	if (menu_numcachepics == MAX_CACHED_PICS)
		Sys_Error("menu_numcachepics == MAX_CACHED_PICS");
	menu_numcachepics++;
	strcpy(pic->name, path);

	//
	// load the pic from disk
	//
	dat = (qpic_t *)COM_LoadTempFile(path);
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

	gl = (glpic_t *)pic->pic.data;
	gl->texnum = GL_LoadPicTexture(dat);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return &pic->pic;
}

void Draw_CharToConback(int num, byte *dest)
{
	int row, col;
	byte *source;
	int drawline;
	int x;

	row = num >> 4;
	col = num & 15;
	source = draw_chars + (row << 10) + (col << 3);

	drawline = 8;

	while (drawline--)
	{
		for (x = 0; x < 8; x++)
			if (source[x] != 255)
				dest[x] = 0x60 + source[x];

		source += 128;
		dest += 320;
	}
}

typedef struct
{
	char *name;
	int minimize, maximize;
} glmode_t;

static const glmode_t modes[] =
{
	{ "GL_NEAREST", GL_NEAREST, GL_NEAREST },
	{ "GL_LINEAR", GL_LINEAR, GL_LINEAR },
	{ "GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST },
	{ "GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR },
	{ "GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST },
	{ "GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR }
};

void Draw_TextureMode_f()
{
	int i;
	gltexture_t *glt;

	if (Cmd_Argc() == 1)
	{
		for (i = 0; i < 6; i++)
			if (gl_filter_min == modes[i].minimize)
			{
				Con_Printf("%s\n", modes[i].name);
				return;
			}
		Con_Printf("current filter is unknown\n");
		return;
	}

	for (i = 0; i < 6; i++)
	{
		if (!Q_strcasecmp(modes[i].name, Cmd_Argv(1)))
			break;
	}
	if (i == 6)
	{
		Con_Printf("bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for (i = 0, glt = gltextures; i < numgltextures; i++, glt++)
	{
		if (glt->mipmap)
		{
			GL_Bind(glt->texnum);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		}
	}
}

void Draw_Init()
{
	int i;
	qpic_t *cb;
	byte *dest;
	int x, y;
	char ver[40];
	glpic_t *gl;
	int start;
	byte *ncdata;

	Cvar_RegisterVariable(&gl_nobind);
	Cvar_RegisterVariable(&gl_max_size);
	Cvar_RegisterVariable(&gl_picmip);

	Cmd_AddCommand("gl_texturemode", &Draw_TextureMode_f);

	// load the console background and the charset
	// by hand, because we need to write the version
	// string into the background before turning
	// it into a texture
	draw_chars = W_GetLumpName("conchars");
	for (i = 0; i < 256 * 64; i++)
		if (draw_chars[i] == 0)
			draw_chars[i] = 255;
	// proper transparent color

	// now turn them into textures
	char_texture = GL_LoadTexture("charset", 128, 128, draw_chars, false, true);

	start = Hunk_LowMark();

	cb = (qpic_t *)COM_LoadTempFile("gfx/conback.lmp");
	if (!cb)
		Sys_Error("Couldn't load gfx/conback.lmp");
	SwapPic(cb);

	// hack the version number directly into the pic
	sprintf(ver, "(gl %4.2f) %4.2f", (float)GLQUAKE_VERSION, (float)VERSION);
	dest = cb->data + 320 * 186 + 320 - 11 - 8 * strlen(ver);
	y = strlen(ver);
	for (x = 0; x < y; x++)
		Draw_CharToConback(ver[x], dest + (x << 3));

	#if 0
	conback->width = vid.conwidth;
	conback->height = vid.conheight;

	// scale console to vid size
	dest = ncdata = Hunk_AllocName(vid.conwidth * vid.conheight, "conback");

	for (y = 0; y < vid.conheight; y++, dest += vid.conwidth)
	{
		byte *src = cb->data + cb->width * (y * cb->height / vid.conheight);
		if (vid.conwidth == cb->width)
			memcpy(dest, src, vid.conwidth);
		else
		{
			int f = 0;
			int fstep = cb->width * 0x10000 / vid.conwidth;
			for (x = 0; x < vid.conwidth; x += 4)
			{
				dest[x] = src[f >> 16];
				f += fstep;
				dest[x + 1] = src[f >> 16];
				f += fstep;
				dest[x + 2] = src[f >> 16];
				f += fstep;
				dest[x + 3] = src[f >> 16];
				f += fstep;
			}
		}
	}
	#else
	conback->width = cb->width;
	conback->height = cb->height;
	ncdata = cb->data;
	#endif

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	gl = (glpic_t *)conback->data;
	gl->texnum = GL_LoadTexture("conback", conback->width, conback->height, ncdata, false, false);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;
	conback->width = vid.width;
	conback->height = vid.height;

	// free loaded console
	Hunk_FreeToLowMark(start);

	// save a texture slot for translated picture
	translate_texture = texture_extension_number++;

	// save slots for scraps
	scrap_texnum = texture_extension_number;
	texture_extension_number += MAX_SCRAPS;

	//
	// get the other pics we need
	//
	draw_disc = Draw_PicFromWad("disc");
	draw_backtile = Draw_PicFromWad("backtile");
}

//--------------------------------------------------------------------------------
// Draw character.
//--------------------------------------------------------------------------------
static int Draw_CharCount = 0;

void Draw_CharBegin()
{
	if (Draw_CharCount == 0)
	{
        GL_Bind(char_texture);
		//oglwBindTexture(0, char_texture);
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
	if (scrap_dirty)
		Scrap_Upload();

    oglwEnableAlphaTest(false);

	glpic_t *gl = (glpic_t *)pic->data;

	GL_Bind(gl->texnum);

	oglwBegin(GL_QUADS);
	OglwVertex *v = oglwAllocateVertex(4);
    if (v != NULL)
        AddQuad2D_CT1(v, x, y, x + pic->width, y + pic->height, 1.0f, 1.0f, 1.0f, alpha, gl->sl, gl->tl, gl->sh, gl->th);
	oglwEnd();

    oglwEnableAlphaTest(true);
}

void Draw_Pic(int x, int y, qpic_t *pic)
{
	if (scrap_dirty)
		Scrap_Upload();

	glpic_t *gl = (glpic_t *)pic->data;

	GL_Bind(gl->texnum);

	oglwBegin(GL_QUADS);
	OglwVertex *v = oglwAllocateVertex(4);
    if (v != NULL)
        AddQuad2D_T1(v, x, y, x + pic->width, y + pic->height, gl->sl, gl->tl, gl->sh, gl->th);
	oglwEnd();
}

void Draw_TransPic(int x, int y, qpic_t *pic)
{
	if (x < 0 || (x + pic->width) > vid.width || y < 0 || (y + pic->height) > vid.height)
	{
		Sys_Error("Draw_TransPic: bad coordinates");
	}

	Draw_Pic(x, y, pic);
}

/*
   Only used for the player color selection menu
 */
void Draw_TransPicTranslate(int x, int y, qpic_t *pic, byte *translation)
{
	GL_Bind(translate_texture);

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
	#if defined(EGLW_GLES1)
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	#endif
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	#if defined(EGLW_GLES1)
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
	#else
	glGenerateMipmap(GL_TEXTURE_2D);
	#endif
	free(trans);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

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
	GL_Bind(*tileTextureId);
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

/*
   Setup as if the screen was 320*200
 */
void GL_Set2D()
{
	oglwSetViewport(glx, gly, glwidth, glheight);

	oglwMatrixMode(GL_PROJECTION);
	oglwLoadIdentity();
	oglwOrtho(0, vid.width, vid.height, 0, -99999, 99999);

	oglwMatrixMode(GL_MODELVIEW);
	oglwLoadIdentity();

	oglwEnableDepthTest(false);
	glDisable(GL_CULL_FACE);
    oglwEnableBlending(true);
    oglwEnableAlphaTest(true);
}

int GL_FindTexture(char *identifier)
{
	int i;
	gltexture_t *glt;
	for (i = 0, glt = gltextures; i < numgltextures; i++, glt++)
	{
		if (!strcmp(identifier, glt->identifier))
			return gltextures[i].texnum;
	}
	return -1;
}

void GL_ResampleTexture(unsigned *in, int inwidth, int inheight, unsigned *out, int outwidth, int outheight)
{
	unsigned fracstep = inwidth * 0x10000 / outwidth;
	for (int i = 0; i < outheight; i++, out += outwidth)
	{
		unsigned *inrow = in + inwidth * (i * inheight / outheight);
		unsigned frac = fracstep >> 1;
		for (int j = 0; j < outwidth; j ++)
		{
			out[j] = inrow[frac >> 16];
			frac += fracstep;
		}
	}
}

static void GL_Upload32(unsigned *data, int width, int height, qboolean mipmap, qboolean alpha)
{
	int scaled_width, scaled_height;
	for (scaled_width = 1; scaled_width < width; scaled_width <<= 1)
		;
	for (scaled_height = 1; scaled_height < height; scaled_height <<= 1)
		;
	scaled_width >>= (int)gl_picmip.value;
	scaled_height >>= (int)gl_picmip.value;
	if (scaled_width > gl_max_size.value)
		scaled_width = gl_max_size.value;
	if (scaled_height > gl_max_size.value)
		scaled_height = gl_max_size.value;

	texels += scaled_width * scaled_height;

	unsigned *scaled = NULL;
	if (scaled_width != width || scaled_height != height)
	{
		scaled = malloc(scaled_width * scaled_height * sizeof(unsigned));
		GL_ResampleTexture(data, width, height, scaled, scaled_width, scaled_height);
		data = scaled;
	}

	#if defined(EGLW_GLES1)
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, mipmap);
	#endif
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	#if defined(EGLW_GLES1)
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
	#else
	if (mipmap)
        glGenerateMipmap(GL_TEXTURE_2D);
	#endif


	if (mipmap)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}

	free(scaled);
}

static void GL_Upload8(byte *data, int width, int height, qboolean mipmap, qboolean alpha)
{
	int s = width * height;
	unsigned *trans = malloc(s * sizeof(unsigned));

	for (int i = 0; i < s; i++)
	{
		trans[i] = d_8to24table[data[i]];
	}

	GL_Upload32(trans, width, height, mipmap, alpha);

	free(trans);
}

int GL_LoadTexture(char *identifier, int width, int height, byte *data, qboolean mipmap, qboolean alpha)
{
	gltexture_t *glt;

	// see if the texture is already present
	if (identifier[0])
	{
		int i;
		for (i = 0, glt = gltextures; i < numgltextures; i++, glt++)
		{
			if (!strcmp(identifier, glt->identifier))
			{
				if (width != glt->width || height != glt->height)
					Sys_Error("GL_LoadTexture: cache mismatch");
				return gltextures[i].texnum;
			}
		}
	}
	else
	{
		glt = &gltextures[numgltextures];
		numgltextures++;
	}

	strcpy(glt->identifier, identifier);
	glt->texnum = texture_extension_number;
	glt->width = width;
	glt->height = height;
	glt->mipmap = mipmap;

	GL_Bind(texture_extension_number);

	GL_Upload8(data, width, height, mipmap, alpha);

	texture_extension_number++;

	return texture_extension_number - 1;
}

static int GL_LoadPicTexture(qpic_t *pic)
{
	return GL_LoadTexture("", pic->width, pic->height, pic->data, false, true);
}

static GLenum oldtarget = GL_TEXTURE0;

// TODO: Remove this ?
void GL_SelectTexture(GLenum target)
{
	if (!gl_mtexable)
		return;

	glActiveTexture(target);
	if (target == oldtarget)
		return;

	cnttextures[oldtarget - GL_TEXTURE0] = currenttexture;
	currenttexture = cnttextures[target - GL_TEXTURE0];
	oldtarget = target;
}
