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
 * Texture handling
 *
 * =======================================================================
 */

#include "local.h"

image_t gltextures[MAX_GLTEXTURES];
int numgltextures;
int base_textureid; /* gltextures[i] = base_textureid+i */
extern qboolean scrap_dirty;
extern byte scrap_texels[MAX_SCRAPS][BLOCK_WIDTH * BLOCK_HEIGHT];

static byte intensitytable[256];
static unsigned char gammatable[256];

cvar_t *intensity;

unsigned char d_8to24table[256][3];

qboolean R_Upload8(byte *data, int width, int height, qboolean mipmap, qboolean is_sky);
qboolean R_Upload32(unsigned *data, int width, int height, qboolean mipmap);

int gl_tex_solid_format = GL_RGB;
int gl_tex_alpha_format = GL_RGBA;

int gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int gl_filter_max = GL_LINEAR;

int Draw_GetPalette(void);

typedef struct
{
	char *name;
	int minimize, maximize;
} glmode_t;

glmode_t modes[] =
{
	{ "GL_NEAREST", GL_NEAREST, GL_NEAREST },
	{ "GL_LINEAR", GL_LINEAR, GL_LINEAR },
	{ "GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST },
	{ "GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR },
	{ "GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST },
	{ "GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR }
};

#define NUM_GL_MODES (sizeof(modes) / sizeof(glmode_t))

typedef struct
{
	char *name;
	int mode;
} gltmode_t;

gltmode_t gl_alpha_modes[] =
{
	{ "default", GL_RGBA },
	{ "R8G8B8A8", GL_RGBA },
	{ "R5G5B5A1", GL_RGBA },
	{ "R4G4B4A4", GL_RGBA },
};

#define NUM_GL_ALPHA_MODES (sizeof(gl_alpha_modes) / sizeof(gltmode_t))

gltmode_t gl_solid_modes[] =
{
	{ "default", GL_RGB },
	{ "R8G8B8", GL_RGB },
	{ "R5G6B5", GL_RGB },
};

#define NUM_GL_SOLID_MODES (sizeof(gl_solid_modes) / sizeof(gltmode_t))

typedef struct
{
	short x, y;
} floodfill_t;

/* must be a power of 2 */
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP(off, dx, dy) \
	{ \
		if (pos[off] == fillcolor) \
		{ \
			pos[off] = 255; \
			fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
			inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
		} \
		else \
		if (pos[off] != 255) \
		{ \
			fdc = pos[off]; \
		} \
	}

int upload_width, upload_height;

void R_TextureMode(char *string)
{
	int i;
	image_t *glt;

	for (i = 0; i < (int)NUM_GL_MODES; i++)
	{
		if (!Q_stricmp(modes[i].name, string))
		{
			break;
		}
	}

	if (i == NUM_GL_MODES)
	{
		VID_Printf(PRINT_ALL, "bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	/* clamp selected anisotropy */
	if (gl_config.anisotropic)
	{
		if (gl_anisotropic->value > gl_config.max_anisotropy)
		{
			Cvar_SetValue("gl_anisotropic", gl_config.max_anisotropy);
		}
		else
		if (gl_anisotropic->value < 1.0f)
		{
			Cvar_SetValue("gl_anisotropic", 1.0f);
		}
	}
	else
	{
		Cvar_SetValue("gl_anisotropic", 0.0);
	}

	/* change all the existing mipmap texture objects */
	oglwSetCurrentTextureUnitForced(0);
	for (i = 0, glt = gltextures; i < numgltextures; i++, glt++)
	{
		if ((glt->type != it_pic) && (glt->type != it_sky))
		{
			oglwBindTextureForced(0, glt->texnum);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
				gl_filter_min);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
				gl_filter_max);

			/* Set anisotropic filter if supported and enabled */
			if (gl_config.anisotropic && gl_anisotropic->value)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
					gl_anisotropic->value);
			}
		}
	}
}

void R_TextureAlphaMode(char *string)
{
	int i;

	for (i = 0; i < (int)NUM_GL_ALPHA_MODES; i++)
	{
		if (!Q_stricmp(gl_alpha_modes[i].name, string))
		{
			break;
		}
	}

	if (i == NUM_GL_ALPHA_MODES)
	{
		VID_Printf(PRINT_ALL, "bad alpha texture mode name\n");
		return;
	}

	gl_tex_alpha_format = gl_alpha_modes[i].mode;
}

void R_TextureSolidMode(char *string)
{
	int i;

	for (i = 0; i < (int)NUM_GL_SOLID_MODES; i++)
	{
		if (!Q_stricmp(gl_solid_modes[i].name, string))
		{
			break;
		}
	}

	if (i == NUM_GL_SOLID_MODES)
	{
		VID_Printf(PRINT_ALL, "bad solid texture mode name\n");
		return;
	}

	gl_tex_solid_format = gl_solid_modes[i].mode;
}

void R_ImageList_f()
{
	int i;
	image_t *image;
	int texels;
	const char *palstrings[2] =
	{
		"RGB",
		"PAL"
	};

	VID_Printf(PRINT_ALL, "------------------\n");
	texels = 0;

	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (image->texnum <= 0)
		{
			continue;
		}

		texels += image->upload_width * image->upload_height;

		switch (image->type)
		{
		case it_skin:
			VID_Printf(PRINT_ALL, "M");
			break;
		case it_sprite:
			VID_Printf(PRINT_ALL, "S");
			break;
		case it_wall:
			VID_Printf(PRINT_ALL, "W");
			break;
		case it_pic:
			VID_Printf(PRINT_ALL, "P");
			break;
		default:
			VID_Printf(PRINT_ALL, " ");
			break;
		}

		VID_Printf(PRINT_ALL, " %3i %3i %s: %s\n",
			image->upload_width, image->upload_height,
			palstrings[image->paletted], image->name);
	}

	VID_Printf(PRINT_ALL,
		"Total texel count (not counting mipmaps): %i\n",
		texels);
}

/*
 * Fill background pixels so mipmapping doesn't have haloes
 */
void R_FloodFillSkin(byte *skin, int skinwidth, int skinheight)
{
	byte fillcolor = *skin; /* assume this is the pixel to fill */
	floodfill_t fifo[FLOODFILL_FIFO_SIZE];
	int inpt = 0, outpt = 0;
	int filledcolor = -1;

	if (filledcolor == -1)
	{
		filledcolor = 0;

		/* attempt to find opaque black */
		for (int i = 0; i < 256; ++i)
		{
			unsigned char *pc = d_8to24table[i];
			if (pc[0] == 0 && pc[1] == 0 && pc[2] == 0)
			{
				filledcolor = i;
				break;
			}
		}
	}

	/* can't fill to filled color or to transparent color (used as visited marker) */
	if ((fillcolor == filledcolor) || (fillcolor == 255))
	{
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt)
	{
		int x = fifo[outpt].x, y = fifo[outpt].y;
		int fdc = filledcolor;
		byte *pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)
		{
			FLOODFILL_STEP(-1, -1, 0);
		}

		if (x < skinwidth - 1)
		{
			FLOODFILL_STEP(1, 1, 0);
		}

		if (y > 0)
		{
			FLOODFILL_STEP(-skinwidth, 0, -1);
		}

		if (y < skinheight - 1)
		{
			FLOODFILL_STEP(skinwidth, 0, 1);
		}

		skin[x + skinwidth * y] = fdc;
	}
}

void R_ResampleTexture(unsigned *in, int inwidth, int inheight, unsigned *out, int outwidth, int outheight)
{
	int i, j;
	unsigned *inrow, *inrow2;
	unsigned frac, fracstep;
	unsigned p1[1024], p2[1024];
	byte *pix1, *pix2, *pix3, *pix4;

	fracstep = inwidth * 0x10000 / outwidth;

	frac = fracstep >> 2;

	for (i = 0; i < outwidth; i++)
	{
		p1[i] = 4 * (frac >> 16);
		frac += fracstep;
	}

	frac = 3 * (fracstep >> 2);

	for (i = 0; i < outwidth; i++)
	{
		p2[i] = 4 * (frac >> 16);
		frac += fracstep;
	}

	for (i = 0; i < outheight; i++, out += outwidth)
	{
		inrow = in + inwidth * (int)((i + 0.25) * inheight / outheight);
		inrow2 = in + inwidth * (int)((i + 0.75) * inheight / outheight);

		for (j = 0; j < outwidth; j++)
		{
			pix1 = (byte *)inrow + p1[j];
			pix2 = (byte *)inrow + p2[j];
			pix3 = (byte *)inrow2 + p1[j];
			pix4 = (byte *)inrow2 + p2[j];
			((byte *)(out + j))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0]) >> 2;
			((byte *)(out + j))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1]) >> 2;
			((byte *)(out + j))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2]) >> 2;
			((byte *)(out + j))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3]) >> 2;
		}
	}
}

/*
 * Scale up the pixel values in a
 * texture to increase the
 * lighting range
 */
void R_LightScaleTexture(unsigned *in, int inwidth, int inheight, qboolean only_gamma)
{
	if (only_gamma)
	{
		int i, c;
		byte *p;

		p = (byte *)in;

		c = inwidth * inheight;

		for (i = 0; i < c; i++, p += 4)
		{
			p[0] = gammatable[p[0]];
			p[1] = gammatable[p[1]];
			p[2] = gammatable[p[2]];
		}
	}
	else
	{
		int i, c;
		byte *p;

		p = (byte *)in;

		c = inwidth * inheight;

		for (i = 0; i < c; i++, p += 4)
		{
			p[0] = gammatable[intensitytable[p[0]]];
			p[1] = gammatable[intensitytable[p[1]]];
			p[2] = gammatable[intensitytable[p[2]]];
		}
	}
}

qboolean R_Upload32Native(unsigned *data, int width, int height, qboolean mipmap)
{
	upload_width = width;
	upload_height = height;
	R_LightScaleTexture(data, upload_width, upload_height, !mipmap);

	bool hasAlpha = false;
	byte *scan = ((byte *)data) + 3;
	int c = width * height;
	for (int i = 0; i < c; i++, scan += 4)
	{
		if (*scan != 255)
		{
			hasAlpha = true;
			break;
		}
	}

	#if defined(GLES1)
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, mipmap);
	#endif
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	#if defined(GLES2)
	//glGenerateMipmap(GL_TEXTURE_2D);
	#endif
	#if defined(GLES1)
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, false);
	#endif

	return hasAlpha;
}

qboolean R_Upload32Old(unsigned *data, int width, int height, qboolean mipmap)
{
	int scaled_width, scaled_height;

	for (scaled_width = 1; scaled_width < width; scaled_width <<= 1)
		;
	if (gl_round_down->value && (scaled_width > width) && mipmap)
		scaled_width >>= 1;

	for (scaled_height = 1; scaled_height < height; scaled_height <<= 1)
		;
	if (gl_round_down->value && (scaled_height > height) && mipmap)
		scaled_height >>= 1;

	/* let people sample down the world textures for speed */
	if (mipmap)
	{
		scaled_width >>= (int)gl_picmip->value;
		scaled_height >>= (int)gl_picmip->value;
	}

	/* don't ever bother with >256 textures */
	if (scaled_width < 1)
		scaled_width = 1;
	if (scaled_width > 256)
		scaled_width = 256;

	if (scaled_height < 1)
		scaled_height = 1;
	if (scaled_height > 256)
		scaled_height = 256;

	upload_width = scaled_width;
	upload_height = scaled_height;

	bool hasAlpha = false;
	byte *scan = ((byte *)data) + 3;
	int c = width * height;
	for (int i = 0; i < c; i++, scan += 4)
	{
		if (*scan != 255)
		{
			hasAlpha = true;
			break;
		}
	}

	unsigned *scaled = NULL;
	if (scaled_width != width || scaled_height != height)
	{
		scaled = malloc(scaled_width * scaled_height * sizeof(unsigned));
		R_ResampleTexture(data, width, height, scaled, scaled_width, scaled_height);
		data = scaled;
	}

	R_LightScaleTexture(data, scaled_width, scaled_height, !mipmap);

	#if defined(GLES1)
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, mipmap);
	#endif
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	#if defined(GLES2)
	//glGenerateMipmap(GL_TEXTURE_2D);
	#endif
	#if defined(GLES1)
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, false);
	#endif

	free(scaled);

	return hasAlpha;
}

qboolean R_Upload32(unsigned *data, int width, int height, qboolean mipmap)
{
	qboolean res;

	if (gl_config.tex_npot)
	{
		res = R_Upload32Native(data, width, height, mipmap);
	}
	else
	{
		res = R_Upload32Old(data, width, height, mipmap);
	}

	if (mipmap)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}

	if (mipmap && gl_config.anisotropic && gl_anisotropic->value)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_anisotropic->value);
	}
	return res;
}

/*
 * Returns has_alpha
 */
qboolean R_Upload8(byte *data, int width, int height, qboolean mipmap, qboolean is_sky)
{
	int s = width * height;
	unsigned *buffer = (unsigned *)malloc(s * sizeof(unsigned));

	for (int i = 0; i < s; i++)
	{
		int p = data[i];
		int alpha = 255;

		/* transparent, so scan around for
		   another color to avoid alpha fringes */
		if (p == 255)
		{
			alpha = 0;
			if ((i > width) && (data[i - width] != 255))
			{
				p = data[i - width];
			}
			else
			if ((i < s - width) && (data[i + width] != 255))
			{
				p = data[i + width];
			}
			else
			if ((i > 0) && (data[i - 1] != 255))
			{
				p = data[i - 1];
			}
			else
			if ((i < s - 1) && (data[i + 1] != 255))
			{
				p = data[i + 1];
			}
			else
			{
				p = 0;
			}
		}

		unsigned char *pc = d_8to24table[p];
		unsigned c = (alpha << 24) | (pc[2] << 16) | (pc[1] << 8) | (pc[0]);
		buffer[i] = c;
	}

	int result = R_Upload32(buffer, width, height, mipmap);

	free(buffer);
	return result;
}

/*
 * This is also used as an entry point for the generated r_notexture
 */
image_t* R_LoadPic(char *name, byte *pic, int width, int realwidth, int height, int realheight, imagetype_t type, int bits)
{
	image_t *image;
	int i;
	qboolean nolerp = (strstr(Cvar_VariableString("gl_nolerp_list"), name) != NULL);

	/* find a free image_t */
	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (!image->texnum)
		{
			break;
		}
	}

	if (i == numgltextures)
	{
		if (numgltextures == MAX_GLTEXTURES)
		{
			VID_Error(ERR_DROP, "MAX_GLTEXTURES");
		}

		numgltextures++;
	}

	image = &gltextures[i];

	if (Q_strlen(name) >= (int)sizeof(image->name))
	{
		VID_Error(ERR_DROP, "Draw_LoadPic: \"%s\" is too long", name);
	}

	strcpy(image->name, name);
	image->registration_sequence = registration_sequence;
	image->image_chain_node = 0;
	image->texturechain = 0;
	image->used = 0;

	image->width = width;
	image->height = height;
	image->type = type;

	if ((type == it_skin) && (bits == 8))
	{
		R_FloodFillSkin(pic, width, height);
	}

	/* load little pics into the scrap */
	if (!nolerp && (image->type == it_pic) && (bits == 8) &&
	    (image->width < 64) && (image->height < 64))
	{
		int x, y;
		int i, j, k;
		int texnum;

		texnum = Scrap_AllocBlock(image->width, image->height, &x, &y);

		if (texnum == -1)
		{
			goto nonscrap;
		}

		scrap_dirty = true;

		/* copy the texels into the scrap block */
		k = 0;

		for (i = 0; i < image->height; i++)
		{
			for (j = 0; j < image->width; j++, k++)
			{
				scrap_texels[texnum][(y + i) * BLOCK_WIDTH + x + j] = pic[k];
			}
		}

		image->texnum = TEXNUM_SCRAPS + texnum;
		image->scrap = true;
		image->has_alpha = true;
		image->sl = (x + 0.01f) / (float)BLOCK_WIDTH;
		image->sh = (x + image->width - 0.01f) / (float)BLOCK_WIDTH;
		image->tl = (y + 0.01f) / (float)BLOCK_WIDTH;
		image->th = (y + image->height - 0.01f) / (float)BLOCK_WIDTH;
	}
	else
	{
nonscrap:
		image->scrap = false;
		image->texnum = TEXNUM_IMAGES + (image - gltextures);
		oglwSetCurrentTextureUnitForced(0);
		oglwBindTextureForced(0, image->texnum);

		if (bits == 8)
		{
			image->has_alpha = R_Upload8(pic, width, height,
					(image->type != it_pic && image->type != it_sky),
					image->type == it_sky);
		}
		else
		{
			image->has_alpha = R_Upload32((unsigned *)pic, width, height,
					(image->type != it_pic && image->type != it_sky));
		}

		image->upload_width = upload_width; /* after power of 2 and scales */
		image->upload_height = upload_height;
		image->paletted = false;

		if (realwidth && realheight)
		{
			if ((realwidth <= image->width) && (realheight <= image->height))
			{
				image->width = realwidth;
				image->height = realheight;
			}
			else
			{
				VID_Printf(PRINT_DEVELOPER,
					"Warning, image '%s' has hi-res replacement smaller than the original! (%d x %d) < (%d x %d)\n",
					name, image->width, image->height, realwidth, realheight);
			}
		}

		image->sl = 0;
		image->sh = 1;
		image->tl = 0;
		image->th = 1;

		if (nolerp)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}
	}

	return image;
}

/*
 * Finds or loads the given image
 */
image_t* R_FindImage(char *name, imagetype_t type)
{
	image_t *image;
	int i, len;
	byte *pic, *palette;
	int width, height;
	char *ptr;
	char namewe[256];
	int realwidth = 0, realheight = 0;
	const char * ext;

	if (!name)
	{
		return NULL;
	}

	ext = COM_FileExtension(name);
	if (!ext[0])
	{
		/* file has no extension */
		return NULL;
	}

	len = Q_strlen(name);

	/* Remove the extension */
	memset(namewe, 0, 256);
	memcpy(namewe, name, len - 4);

	if (len < 5)
	{
		return NULL;
	}

	/* fix backslashes */
	while ((ptr = strchr(name, '\\')))
	{
		*ptr = '/';
	}

	/* look for it */
	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (!strcmp(name, image->name))
		{
			image->registration_sequence = registration_sequence;
			return image;
		}
	}

	/* load the pic from disk */
	pic = NULL;
	palette = NULL;

	if (strcmp(ext, "pcx") == 0)
	{
		if (gl_retexturing->value)
		{
			GetPCXInfo(name, &realwidth, &realheight);
			if (realwidth == 0)
			{
				/* No texture found */
				return NULL;
			}

			/* try to load a tga, png or jpg (in that order/priority) */
			if (LoadSTB(namewe, "tga", &pic, &width, &height)
			    || LoadSTB(namewe, "png", &pic, &width, &height)
			    || LoadSTB(namewe, "jpg", &pic, &width, &height))
			{
				/* upload tga or png or jpg */
				image = R_LoadPic(name, pic, width, realwidth, height,
						realheight, type, 32);
			}
			else
			{
				/* PCX if no TGA/PNG/JPEG available (exists always) */
				LoadPCX(name, &pic, &palette, &width, &height);

				if (!pic)
				{
					/* No texture found */
					return NULL;
				}

				/* Upload the PCX */
				image = R_LoadPic(name, pic, width, 0, height, 0, type, 8);
			}
		}
		else /* gl_retexture is not set */
		{
			LoadPCX(name, &pic, &palette, &width, &height);

			if (!pic)
			{
				return NULL;
			}

			image = R_LoadPic(name, pic, width, 0, height, 0, type, 8);
		}
	}
	else
	if (strcmp(ext, "wal") == 0)
	{
		if (gl_retexturing->value)
		{
			/* Get size of the original texture */
			GetWalInfo(name, &realwidth, &realheight);
			if (realwidth == 0)
			{
				/* No texture found */
				return NULL;
			}

			/* try to load a tga, png or jpg (in that order/priority) */
			if (LoadSTB(namewe, "tga", &pic, &width, &height)
			    || LoadSTB(namewe, "png", &pic, &width, &height)
			    || LoadSTB(namewe, "jpg", &pic, &width, &height))
			{
				/* upload tga or png or jpg */
				image = R_LoadPic(name, pic, width, realwidth, height,
						realheight, type, 32);
			}
			else
			{
				/* WAL if no TGA/PNG/JPEG available (exists always) */
				image = LoadWal(namewe);
			}

			if (!image)
			{
				/* No texture found */
				return NULL;
			}
		}
		else /* gl_retexture is not set */
		{
			image = LoadWal(name);

			if (!image)
			{
				/* No texture found */
				return NULL;
			}
		}
	}
	else
	if (strcmp(ext, "tga") == 0 || strcmp(ext, "png") == 0 || strcmp(ext, "jpg") == 0)
	{
		char tmp_name[256];

		realwidth = 0;
		realheight = 0;

		strcpy(tmp_name, namewe);
		strcat(tmp_name, ".wal");
		GetWalInfo(tmp_name, &realwidth, &realheight);

		if (realwidth == 0 || realheight == 0)
		{
			/* It's a sky or model skin. */
			strcpy(tmp_name, namewe);
			strcat(tmp_name, ".pcx");
			GetPCXInfo(tmp_name, &realwidth, &realheight);
		}

		/* TODO: not sure if not having realwidth/heigth is bad - a tga/png/jpg
		 * was requested, after all, so there might be no corresponding wal/pcx?
		 * if (realwidth == 0 || realheight == 0) return NULL;
		 */

		LoadSTB(name, ext, &pic, &width, &height);
		image = R_LoadPic(name, pic, width, realwidth,
				height, realheight, type, 32);
	}
	else
	{
		return NULL;
	}

	if (pic)
	{
		free(pic);
	}

	if (palette)
	{
		free(palette);
	}

	return image;
}

struct image_s* R_RegisterSkin(char *name)
{
	return R_FindImage(name, it_skin);
}

/*
 * Any image that was not touched on
 * this registration sequence
 * will be freed.
 */
void R_FreeUnusedImages(void)
{
	int i;
	image_t *image;

	/* never free r_notexture or particle texture */
	r_notexture->registration_sequence = registration_sequence;
	r_particletexture->registration_sequence = registration_sequence;

	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (image->registration_sequence == registration_sequence)
		{
			continue; /* used this sequence */
		}

		if (!image->registration_sequence)
		{
			continue; /* free image_t slot */
		}

		if (image->type == it_pic)
		{
			continue; /* don't free pics */
		}

		/* free it */
		glDeleteTextures(1, (GLuint *)&image->texnum);
		memset(image, 0, sizeof(*image));
	}
}

void R_InitImages(void)
{
	int i, j;
	// use 1/gamma so higher value is brighter, to match HW gamma settings
	float g = 1.0f / vid_gamma->value;

	registration_sequence = 1;

	/* init intensity conversions */
	intensity = Cvar_Get("intensity", "2", CVAR_ARCHIVE);

	if (intensity->value <= 1)
	{
		Cvar_Set("intensity", "1");
	}

	gl_state.inverse_intensity = 1 / intensity->value;

	Draw_GetPalette();

	for (i = 0; i < 256; i++)
	{
		if ((g == 1) || gl_state.hwgamma)
		{
			gammatable[i] = (unsigned char)i;
		}
		else
		{
			float inf;

			inf = 255 * powf(((float)i + 0.5f) / 255.5f, g) + 0.5f;

			if (inf < 0)
			{
				inf = 0;
			}

			if (inf > 255)
			{
				inf = 255;
			}

			gammatable[i] = (unsigned char)inf;
		}
	}

	for (i = 0; i < 256; i++)
	{
		j = (float)i * intensity->value;

		if (j > 255)
		{
			j = 255;
		}

		intensitytable[i] = (unsigned char)j;
	}
}

void R_ShutdownImages(void)
{
	int i;
	image_t *image;

	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (!image->registration_sequence)
		{
			continue; /* free image_t slot */
		}

		/* free it */
		glDeleteTextures(1, (GLuint *)&image->texnum);
		memset(image, 0, sizeof(*image));
	}
}