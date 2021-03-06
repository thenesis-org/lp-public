// MD2 file format
#include "client/refresh/r_private.h"

#define MAX_LBM_HEIGHT 480

void LoadMD2(model_t *mod, void *buffer)
{
	int i, j;
	dmdl_t *pinmodel, *pheader;
	dstvert_t *pinst, *poutst;
	dtriangle_t *pintri, *pouttri;
	daliasframe_t *pinframe, *poutframe;
	int *pincmd, *poutcmd;
	int version;

	pinmodel = (dmdl_t *)buffer;

	version = LittleLong(pinmodel->version);

	if (version != ALIAS_VERSION)
	{
		R_error(ERR_DROP, "%s has wrong version number (%i should be %i)",
				mod->name, version, ALIAS_VERSION);
	}

	pheader = Hunk_Alloc(LittleLong(pinmodel->ofs_end));

	/* byte swap the header fields and sanity check */
	for (i = 0; i < (int)sizeof(dmdl_t) / 4; i++)
	{
		((int *)pheader)[i] = LittleLong(((int *)buffer)[i]);
	}

	if (pheader->skinheight > MAX_LBM_HEIGHT)
	{
		R_error(ERR_DROP, "model %s has a skin taller than %d", mod->name, MAX_LBM_HEIGHT);
	}

	if (pheader->num_xyz <= 0)
	{
		R_error(ERR_DROP, "model %s has no vertices", mod->name);
	}

	if (pheader->num_xyz > MAX_VERTS)
	{
		R_error(ERR_DROP, "model %s has too many vertices", mod->name);
	}

	if (pheader->num_st <= 0)
	{
		R_error(ERR_DROP, "model %s has no st vertices", mod->name);
	}

	if (pheader->num_tris <= 0)
	{
		R_error(ERR_DROP, "model %s has no triangles", mod->name);
	}

	if (pheader->num_frames <= 0)
	{
		R_error(ERR_DROP, "model %s has no frames", mod->name);
	}

	/* load base s and t vertices (not used in gl version) */
	pinst = (dstvert_t *)((byte *)pinmodel + pheader->ofs_st);
	poutst = (dstvert_t *)((byte *)pheader + pheader->ofs_st);

	for (i = 0; i < pheader->num_st; i++)
	{
		poutst[i].s = LittleShort(pinst[i].s);
		poutst[i].t = LittleShort(pinst[i].t);
	}

	/* load triangle lists */
	pintri = (dtriangle_t *)((byte *)pinmodel + pheader->ofs_tris);
	pouttri = (dtriangle_t *)((byte *)pheader + pheader->ofs_tris);

	for (i = 0; i < pheader->num_tris; i++)
	{
		for (j = 0; j < 3; j++)
		{
			pouttri[i].index_xyz[j] = LittleShort(pintri[i].index_xyz[j]);
			pouttri[i].index_st[j] = LittleShort(pintri[i].index_st[j]);
		}
	}

	/* load the frames */
	for (i = 0; i < pheader->num_frames; i++)
	{
		pinframe = (daliasframe_t *)((byte *)pinmodel
				+ pheader->ofs_frames + i * pheader->framesize);
		poutframe = (daliasframe_t *)((byte *)pheader
				+ pheader->ofs_frames + i * pheader->framesize);

		memcpy(poutframe->name, pinframe->name, sizeof(poutframe->name));

		for (j = 0; j < 3; j++)
		{
			poutframe->scale[j] = LittleFloat(pinframe->scale[j]);
			poutframe->translate[j] = LittleFloat(pinframe->translate[j]);
		}

		/* verts are all 8 bit, so no swapping needed */
		memcpy(poutframe->verts, pinframe->verts,
				pheader->num_xyz * sizeof(dtrivertx_t));
	}

	mod->type = mod_alias;

	/* load the glcmds */
	pincmd = (int *)((byte *)pinmodel + pheader->ofs_glcmds);
	poutcmd = (int *)((byte *)pheader + pheader->ofs_glcmds);

	for (i = 0; i < pheader->num_glcmds; i++)
	{
		poutcmd[i] = LittleLong(pincmd[i]);
	}

	/* register all skins */
	memcpy((char *)pheader + pheader->ofs_skins,
			(char *)pinmodel + pheader->ofs_skins,
			pheader->num_skins * MAX_SKINNAME);

	for (i = 0; i < pheader->num_skins; i++)
	{
		mod->skins[i] = R_FindImage(
				(char *)pheader + pheader->ofs_skins + i * MAX_SKINNAME,
				it_skin);
	}

	mod->mins[0] = -32;
	mod->mins[1] = -32;
	mod->mins[2] = -32;
	mod->maxs[0] = 32;
	mod->maxs[1] = 32;
	mod->maxs[2] = 32;
}

