#include "client/refresh/r_private.h"

//--------------------------------------------------------------------------------
// Scrap.
//--------------------------------------------------------------------------------
// Allocate all the little status bar obejcts into a single texture
// to crutch up inefficient hardware / drivers.

#define SCRAP_MAX_NB 1
#define SCRAP_WIDTH 128
#define SCRAP_HEIGHT 128

static int scrap_allocated[SCRAP_MAX_NB][SCRAP_WIDTH];
static byte scrap_texels[SCRAP_MAX_NB][SCRAP_WIDTH * SCRAP_HEIGHT];
bool scrap_dirty;
static int scrap_uploads;

/* returns a texture number and the position inside it */
static int Scrap_AllocBlock(int w, int h, int *x, int *y)
{
	int i, j;
	int best, best2;
	int texnum;

	for (texnum = 0; texnum < SCRAP_MAX_NB; texnum++)
	{
		best = SCRAP_HEIGHT;

		for (i = 0; i < SCRAP_WIDTH - w; i++)
		{
			best2 = 0;

			for (j = 0; j < w; j++)
			{
				if (scrap_allocated[texnum][i + j] >= best)
					break;
				if (scrap_allocated[texnum][i + j] > best2)
					best2 = scrap_allocated[texnum][i + j];
			}

			if (j == w)
			{ /* this is a valid spot */
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > SCRAP_HEIGHT)
			continue;

		for (i = 0; i < w; i++)
			scrap_allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	return -1;
}

void Scrap_Upload()
{
	scrap_uploads++;
	oglwSetCurrentTextureUnitForced(0);
	oglwBindTextureForced(0, TEXNUM_SCRAPS);
	R_Texture_upload8(scrap_texels[0], SCRAP_WIDTH, SCRAP_HEIGHT, true, false, false, NULL, NULL);
	scrap_dirty = false;
}

//--------------------------------------------------------------------------------
// Textures.
//--------------------------------------------------------------------------------
#define MAX_GLTEXTURES 1024

#define TEXNUM_LIGHTMAPS 1024
#define TEXNUM_SCRAPS (TEXNUM_LIGHTMAPS + LIGHTMAP_MAX_NB)
#define TEXNUM_IMAGES (TEXNUM_SCRAPS + SCRAP_MAX_NB)

static image_t gltextures[MAX_GLTEXTURES];
static int numgltextures;

static byte intensitytable[256];
static byte gammatable[256];
static byte lightScaleTable[256];

cvar_t *r_intensity;

unsigned char d_8to24table[256][3];

static int gl_tex_solid_format = GL_RGB;
static int gl_tex_alpha_format = GL_RGBA;

static int gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
static int gl_filter_max = GL_LINEAR;

int Draw_GetPalette();

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

#define NUM_GL_MODES (sizeof(modes) / sizeof(glmode_t))

typedef struct
{
	char *name;
	int mode;
} gltmode_t;

static const gltmode_t gl_alpha_modes[] =
{
	{ "default", GL_RGBA },
	{ "R8G8B8A8", GL_RGBA },
	{ "R5G5B5A1", GL_RGBA },
	{ "R4G4B4A4", GL_RGBA },
};

#define NUM_GL_ALPHA_MODES (sizeof(gl_alpha_modes) / sizeof(gltmode_t))

static const gltmode_t gl_solid_modes[] =
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

void R_TextureMode(char *string)
{
	int i;
	image_t *glt;

	for (i = 0; i < (int)NUM_GL_MODES; i++)
	{
		if (!Q_stricmp(modes[i].name, string))
			break;
	}

	if (i == NUM_GL_MODES)
	{
		R_printf(PRINT_ALL, "bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	/* clamp selected anisotropy */
	if (gl_config.anisotropic)
	{
		if (r_texture_anisotropy->value > gl_config.max_anisotropy)
		{
			Cvar_SetValue("r_texture_anisotropy", gl_config.max_anisotropy);
		}
		else
		if (r_texture_anisotropy->value < 1.0f)
		{
			Cvar_SetValue("r_texture_anisotropy", 1.0f);
		}
	}
	else
	{
		Cvar_SetValue("r_texture_anisotropy", 0.0);
	}

	/* change all the existing mipmap texture objects */
	oglwSetCurrentTextureUnitForced(0);
    float anisotropy = r_texture_anisotropy->value;
    if (anisotropy < 1.0f)
        anisotropy = 1.0f;
	for (i = 0, glt = gltextures; i < numgltextures; i++, glt++)
	{
		if ((glt->type != it_pic) && (glt->type != it_sky))
		{
			oglwBindTextureForced(0, glt->texnum);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
			if (gl_config.anisotropic)
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
		}
	}
}

void R_TextureAlphaMode(char *string)
{
    int i;
	for (i = 0; i < (int)NUM_GL_ALPHA_MODES; i++)
	{
		if (!Q_stricmp(gl_alpha_modes[i].name, string))
			break;
	}
	if (i == NUM_GL_ALPHA_MODES)
	{
		R_printf(PRINT_ALL, "bad alpha texture mode name\n");
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
			break;
	}
	if (i == NUM_GL_SOLID_MODES)
	{
		R_printf(PRINT_ALL, "bad solid texture mode name\n");
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

	R_printf(PRINT_ALL, "------------------\n");
	texels = 0;

	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (image->texnum <= 0)
			continue;

		texels += image->upload_width * image->upload_height;

		switch (image->type)
		{
		case it_skin:
			R_printf(PRINT_ALL, "M");
			break;
		case it_sprite:
			R_printf(PRINT_ALL, "S");
			break;
		case it_wall:
			R_printf(PRINT_ALL, "W");
			break;
		case it_pic:
			R_printf(PRINT_ALL, "P");
			break;
		default:
			R_printf(PRINT_ALL, " ");
			break;
		}

		R_printf(PRINT_ALL, " %3i %3i %s: %s\n",
			image->upload_width, image->upload_height,
			palstrings[image->paletted], image->name);
	}

	R_printf(PRINT_ALL,
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
	unsigned p1[1024], p2[1024];

	unsigned fracstep = inwidth * 0x10000 / outwidth;
	unsigned frac = fracstep >> 2;

	for (int i = 0; i < outwidth; i++)
	{
		p1[i] = 4 * (frac >> 16);
		frac += fracstep;
	}

	frac = 3 * (fracstep >> 2);
	for (int i = 0; i < outwidth; i++)
	{
		p2[i] = 4 * (frac >> 16);
		frac += fracstep;
	}

    float yScale = (float)inheight / (float)outheight;
	for (int y = 0; y < outheight; y++, out += outwidth)
	{
		unsigned *inrow = in + inwidth * (int)((y + 0.25f) * yScale);
		unsigned *inrow2 = in + inwidth * (int)((y + 0.75f) * yScale);
		for (int j = 0; j < outwidth; j++)
		{
			byte *pix1 = (byte *)inrow + p1[j];
			byte *pix2 = (byte *)inrow + p2[j];
			byte *pix3 = (byte *)inrow2 + p1[j];
			byte *pix4 = (byte *)inrow2 + p2[j];
            int r = (pix1[0] + pix2[0] + pix3[0] + pix4[0]) >> 2;
            int g = (pix1[1] + pix2[1] + pix3[1] + pix4[1]) >> 2;
            int b = (pix1[2] + pix2[2] + pix3[2] + pix4[2]) >> 2;
            int a = (pix1[3] + pix2[3] + pix3[3] + pix4[3]) >> 2;
            byte *po = (byte *)(out + j);
			po[0] = (byte)r;
			po[1] = (byte)g;
			po[2] = (byte)b;
			po[3] = (byte)a;
		}
	}
}

// Scale up the pixel values in a texture to increase the lighting range
static void R_LightScaleTexture(unsigned *in, int inwidth, int inheight, bool only_gamma)
{
    byte *p = (byte *)in;
    int n = inwidth * inheight;
    byte *table = only_gamma ? gammatable : lightScaleTable;
    for (int i = 0; i < n; i++, p += 4)
    {
        int r = table[p[0]];
        int g = table[p[1]];
        int b = table[p[2]];
        p[0] = r;
        p[1] = g;
        p[2] = b;
    }
}

static bool R_Texture_checkAlpha(unsigned *data, int width, int height)
{
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
    return hasAlpha;
}

void R_Texture_upload(void *data, int x, int y, int width, int height, bool fullUploadFlag, bool noFilteringFlag, bool mipmapFlag)
{
	#if defined(EGLW_GLES1)
    if (mipmapFlag)
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, true);
	#endif
    if (fullUploadFlag)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    else
        glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
	#if defined(EGLW_GLES1)
    if (mipmapFlag)
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, false);
    #else
    if (mipmapFlag)
        glGenerateMipmap(GL_TEXTURE_2D);
	#endif

    if (noFilteringFlag)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        if (gl_config.anisotropic)
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f);
    }
    else
    {
        if (mipmapFlag)
        {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
        }
        else
        {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
        }
        if (gl_config.anisotropic)
        {
            float anisotropy = r_texture_anisotropy->value;
            if (anisotropy < 1.0f)
                anisotropy = 1.0f;
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
        }
    }
}

static bool R_Texture_uploadWithoutResampling32(unsigned *data, int width, int height, bool noFilteringFlag, bool mipmapFlag)
{
	bool hasAlpha = R_Texture_checkAlpha(data, width, height);
	R_LightScaleTexture(data, width, height, !mipmapFlag);
    R_Texture_upload(data, 0, 0, width, height, true, noFilteringFlag, mipmapFlag);
	return hasAlpha;
}

static bool R_Texture_uploadWithResampling32(unsigned *data, int width, int height, bool noFilteringFlag, bool mipmapFlag, int *uploadWidth, int *uploadHeight)
{
	int scaled_width, scaled_height;

	for (scaled_width = 1; scaled_width < width; scaled_width <<= 1)
		;
	if (r_texture_rounddown->value && (scaled_width > width) && mipmapFlag)
		scaled_width >>= 1;

	for (scaled_height = 1; scaled_height < height; scaled_height <<= 1)
		;
	if (r_texture_rounddown->value && (scaled_height > height) && mipmapFlag)
		scaled_height >>= 1;

	/* let people sample down the world textures for speed */
	if (mipmapFlag)
	{
		scaled_width >>= (int)r_texture_scaledown->value;
		scaled_height >>= (int)r_texture_scaledown->value;
	}

    // TODO: Clamp to max supported size by OpenGL.
	if (scaled_width < 1)
		scaled_width = 1;
//	if (scaled_width > 256)
//		scaled_width = 256;
	if (scaled_height < 1)
		scaled_height = 1;
//	if (scaled_height > 256)
//		scaled_height = 256;

    if (uploadWidth)
        *uploadWidth = scaled_width;
    if (uploadHeight)
        *uploadHeight = scaled_height;

	bool hasAlpha = R_Texture_checkAlpha(data, width, height);

	unsigned *scaled = NULL;
	if (scaled_width != width || scaled_height != height)
	{
		scaled = malloc(scaled_width * scaled_height * sizeof(unsigned));
		R_ResampleTexture(data, width, height, scaled, scaled_width, scaled_height);
		data = scaled;
	}

	R_LightScaleTexture(data, scaled_width, scaled_height, !mipmapFlag);

    R_Texture_upload(data, 0, 0, scaled_width, scaled_height, true, noFilteringFlag, mipmapFlag);

	free(scaled);

	return hasAlpha;
}

static bool R_Texture_upload32(unsigned *data, int width, int height, bool noFilteringFlag, bool mipmapFlag, int *uploadWidth, int *uploadHeight)
{
	bool hasAlpha;
	if (gl_config.tex_npot)
    {
		hasAlpha = R_Texture_uploadWithoutResampling32(data, width, height, noFilteringFlag, mipmapFlag);
        *uploadWidth = width;
        *uploadHeight = height;
    }
	else
		hasAlpha = R_Texture_uploadWithResampling32(data, width, height, noFilteringFlag, mipmapFlag, uploadWidth, uploadHeight);
	return hasAlpha;
}

bool R_Texture_upload8(byte *data, int width, int height, bool noFilteringFlag, bool mipmapFlag, bool skyFlag, int *uploadWidth, int *uploadHeight)
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

	bool hasAlpha = R_Texture_upload32(buffer, width, height, noFilteringFlag, mipmapFlag, uploadWidth, uploadHeight);

	free(buffer);
	return hasAlpha;
}

/*
 * This is also used as an entry point for the generated r_notexture
 */
image_t* R_LoadPic(char *name, byte *pic, int width, int realwidth, int height, int realheight, imagetype_t type, int bits)
{
	image_t *image;
	int i;

	/* find a free image_t */
	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (!image->texnum)
			break;
	}

	if (i == numgltextures)
	{
		if (numgltextures == MAX_GLTEXTURES)
		{
			R_error(ERR_DROP, "MAX_GLTEXTURES");
            return NULL;
		}
		numgltextures++;
	}

	image = &gltextures[i];

	if (Q_strlen(name) >= (int)sizeof(image->name))
	{
		R_error(ERR_DROP, "Draw_LoadPic: \"%s\" is too long", name);
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

	qboolean noFilteringFlag = (strstr(Cvar_VariableString("gl_nolerp_list"), name) != NULL);

	/* load little pics into the scrap */
	if (!noFilteringFlag && (image->type == it_pic) && (bits == 8) && (image->width < 64) && (image->height < 64))
	{
		int x = 0, y = 0;
		int texnum = Scrap_AllocBlock(image->width, image->height, &x, &y);
		if (texnum == -1)
			goto nonscrap;

		scrap_dirty = true;

		/* copy the texels into the scrap block */
		int k = 0;
		for (int i = 0; i < image->height; i++)
			for (int j = 0; j < image->width; j++, k++)
				scrap_texels[texnum][(y + i) * SCRAP_WIDTH + x + j] = pic[k];

		image->texnum = TEXNUM_SCRAPS + texnum;
		image->scrap = true;
		image->has_alpha = true;
		image->sl = (x + 0.01f) / (float)SCRAP_WIDTH;
		image->sh = (x + image->width - 0.01f) / (float)SCRAP_WIDTH;
		image->tl = (y + 0.01f) / (float)SCRAP_WIDTH;
		image->th = (y + image->height - 0.01f) / (float)SCRAP_WIDTH;
	}
	else
	{
nonscrap:
		image->scrap = false;
		image->texnum = TEXNUM_IMAGES + (image - gltextures);
		oglwSetCurrentTextureUnitForced(0);
		oglwBindTextureForced(0, image->texnum);

        bool skyFlag = image->type == it_sky;
        bool mipmapFlag = (image->type != it_pic && !skyFlag);
		if (bits == 8)
			image->has_alpha = R_Texture_upload8(pic, width, height, noFilteringFlag, mipmapFlag, skyFlag, &image->upload_width, &image->upload_height);
		else
			image->has_alpha = R_Texture_upload32((unsigned *)pic, width, height, noFilteringFlag, mipmapFlag, &image->upload_width, &image->upload_height);
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
				R_printf(PRINT_DEVELOPER,
					"Warning, image '%s' has hi-res replacement smaller than the original! (%d x %d) < (%d x %d)\n",
					name, image->width, image->height, realwidth, realheight);
			}
		}

		image->sl = 0;
		image->sh = 1;
		image->tl = 0;
		image->th = 1;
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
		return NULL;

	ext = COM_FileExtension(name);
	if (!ext[0])
		return NULL; // file has no extension

	len = Q_strlen(name);

	/* Remove the extension */
	memset(namewe, 0, 256);
	memcpy(namewe, name, len - 4);

	if (len < 5)
		return NULL;

	/* fix backslashes */
	while ((ptr = strchr(name, '\\')))
		*ptr = '/';

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
		if (r_texture_retexturing->value)
		{
			GetPCXInfo(name, &realwidth, &realheight);
			if (realwidth == 0)
				return NULL; // No texture found

			/* try to load a tga, png or jpg (in that order/priority) */
			if (LoadSTB(namewe, "tga", &pic, &width, &height) || LoadSTB(namewe, "png", &pic, &width, &height) || LoadSTB(namewe, "jpg", &pic, &width, &height))
			{
				/* upload tga or png or jpg */
				image = R_LoadPic(name, pic, width, realwidth, height, realheight, type, 32);
			}
			else
			{
				/* PCX if no TGA/PNG/JPEG available (exists always) */
				LoadPCX(name, &pic, &palette, &width, &height);
				if (!pic)
					return NULL; // No texture found
				/* Upload the PCX */
				image = R_LoadPic(name, pic, width, 0, height, 0, type, 8);
			}
		}
		else /* gl_retexture is not set */
		{
			LoadPCX(name, &pic, &palette, &width, &height);
			if (!pic)
				return NULL;
			image = R_LoadPic(name, pic, width, 0, height, 0, type, 8);
		}
	}
	else
	if (strcmp(ext, "wal") == 0)
	{
		if (r_texture_retexturing->value)
		{
			/* Get size of the original texture */
			GetWalInfo(name, &realwidth, &realheight);
			if (realwidth == 0)
				return NULL; // No texture found

			/* try to load a tga, png or jpg (in that order/priority) */
			if (LoadSTB(namewe, "tga", &pic, &width, &height) || LoadSTB(namewe, "png", &pic, &width, &height) || LoadSTB(namewe, "jpg", &pic, &width, &height))
			{
				/* upload tga or png or jpg */
				image = R_LoadPic(name, pic, width, realwidth, height, realheight, type, 32);
			}
			else
			{
				/* WAL if no TGA/PNG/JPEG available (exists always) */
				image = LoadWal(namewe);
			}

			if (!image)
				return NULL; // No texture found
		}
		else /* gl_retexture is not set */
		{
			image = LoadWal(name);
			if (!image)
				return NULL; // No texture found
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
		image = R_LoadPic(name, pic, width, realwidth, height, realheight, type, 32);
	}
	else
		return NULL;

	if (pic)
		free(pic);

	if (palette)
		free(palette);

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
void R_FreeUnusedImages()
{
	/* never free r_notexture or particle texture */
	r_notexture->registration_sequence = registration_sequence;
	r_particletexture->registration_sequence = registration_sequence;

	int i;
	image_t *image;
	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (image->registration_sequence == registration_sequence)
			continue; /* used this sequence */
		if (!image->registration_sequence)
			continue; /* free image_t slot */
		if (image->type == it_pic)
			continue; /* don't free pics */

		/* free it */
		glDeleteTextures(1, (GLuint *)&image->texnum);
		memset(image, 0, sizeof(*image));
	}
}

void R_InitImages()
{
	registration_sequence = 1;

	Draw_GetPalette();

    byte *gt = gammatable;
    {
        // use 1/gamma so higher value is brighter, to match HW gamma settings
        float g = 1.0f / r_gamma->value;
        if (g == 1.0f || gl_state.hwgamma)
        {
            for (int i = 0; i < 256; i++)
                gt[i] = (byte)i;
        }
        else
        {
            for (int i = 0; i < 256; i++)
            {
                float inf = 255 * powf(((float)i + 0.5f) / 255.5f, g) + 0.5f;
                if (inf < 0)
                    inf = 0;
                if (inf > 255)
                    inf = 255;
                gt[i] = (byte)inf;
            }
        }
    }

    byte *it = intensitytable;
    {
        r_intensity = Cvar_Get("r_intensity", "2", CVAR_ARCHIVE);
        float intensity = r_intensity->value;
        if (intensity <= 1)
        {
            intensity = 1;
            Cvar_SetValue("r_intensity", intensity);
        }
        gl_state.inverse_intensity = 1 / intensity;

        for (int i = 0; i < 256; i++)
        {
            float j = (float)i * intensity;
            if (j > 255)
                j = 255;
            it[i] = (byte)j;
        }
    }
    
    byte *lst = lightScaleTable;
	for (int i = 0; i < 256; i++)
	{
        lst[i] = gt[it[i]];
    }
}

void R_ShutdownImages()
{
	int i;
	image_t *image;
	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (!image->registration_sequence)
			continue; /* free image_t slot */

		/* free it */
		glDeleteTextures(1, (GLuint *)&image->texnum);
		memset(image, 0, sizeof(*image));
	}
}
