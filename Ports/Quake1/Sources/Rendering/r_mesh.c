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
// Triangle model functions

#include "Client/console.h"
#include "Rendering/r_model.h"

#include <string.h>

/*
   =================================================================

   ALIAS MODEL DISPLAY LIST GENERATION

   =================================================================
 */
//#define MeshBuilder_CommandMax 

typedef struct {
    qboolean used[8192];

    // the command list holds counts and s/t values that are valid for
    // every frame
    int commands[8192];
    int numcommands;

    // all frames will have their vertexes rearranged and expanded
    // so they are in the order expected by the command list
    int vertexorder[8192];
    int numorder;

    int allverts, alltris;

    int stripverts[128];
    int striptris[128];
    int stripcount;

	int bestverts[1024];
	int besttris[1024];
} MeshBuilder;

int StripLength(MeshBuilder *meshBuilder, aliashdr_t *pheader, mtriangle_t *triangles, int starttri, int startv)
{
	meshBuilder->used[starttri] = 2;

	mtriangle_t *last = &triangles[starttri];

	meshBuilder->stripverts[0] = last->vertindex[(startv) % 3];
	meshBuilder->stripverts[1] = last->vertindex[(startv + 1) % 3];
	meshBuilder->stripverts[2] = last->vertindex[(startv + 2) % 3];

	meshBuilder->striptris[0] = starttri;
	meshBuilder->stripcount = 1;

	int m1 = last->vertindex[(startv + 2) % 3];
	int m2 = last->vertindex[(startv + 1) % 3];

	// look for a matching triangle
	int j, numtris = pheader->numtris;
	mtriangle_t *check;
nexttri:
	for (j = starttri + 1, check = &triangles[starttri + 1]; j < numtris; j++, check++)
	{
		if (check->facesfront != last->facesfront)
			continue;
		for (int k = 0; k < 3; k++)
		{
			if (check->vertindex[k] != m1)
				continue;
			if (check->vertindex[(k + 1) % 3] != m2)
				continue;

			// this is the next part of the fan

			// if we can't use this triangle, this tristrip is done
			if (meshBuilder->used[j])
				goto done;

            int stripcount = meshBuilder->stripcount;
            int m = check->vertindex[(k + 2) % 3];
            
			// the new edge
			if (stripcount & 1)
				m2 = m;
			else
				m1 = m;

			meshBuilder->stripverts[stripcount + 2] = m;
			meshBuilder->striptris[stripcount] = j;
			meshBuilder->stripcount = stripcount + 1;

			meshBuilder->used[j] = 2;
			goto nexttri;
		}
	}
done:

	// clear the temp used flags
	for (j = starttri + 1; j < numtris; j++)
		if (meshBuilder->used[j] == 2)
			meshBuilder->used[j] = 0;
            
	return meshBuilder->stripcount;
}

int FanLength(MeshBuilder *meshBuilder, aliashdr_t *pheader, mtriangle_t *triangles, int starttri, int startv)
{
	meshBuilder->used[starttri] = 2;

	mtriangle_t *last = &triangles[starttri];

	meshBuilder->stripverts[0] = last->vertindex[(startv) % 3];
	meshBuilder->stripverts[1] = last->vertindex[(startv + 1) % 3];
	meshBuilder->stripverts[2] = last->vertindex[(startv + 2) % 3];

	meshBuilder->striptris[0] = starttri;
	meshBuilder->stripcount = 1;

	int m1 = last->vertindex[(startv + 0) % 3];
	int m2 = last->vertindex[(startv + 2) % 3];

	// look for a matching triangle
	int j, numtris = pheader->numtris;
	mtriangle_t *check;
nexttri:
	for (j = starttri + 1, check = &triangles[starttri + 1]; j < numtris; j++, check++)
	{
		if (check->facesfront != last->facesfront)
			continue;
		for (int k = 0; k < 3; k++)
		{
			if (check->vertindex[k] != m1)
				continue;
			if (check->vertindex[(k + 1) % 3] != m2)
				continue;

			// this is the next part of the fan

			// if we can't use this triangle, this tristrip is done
			if (meshBuilder->used[j])
				goto done;

			// the new edge
			m2 = check->vertindex[(k + 2) % 3];

			meshBuilder->stripverts[meshBuilder->stripcount + 2] = m2;
			meshBuilder->striptris[meshBuilder->stripcount] = j;
			meshBuilder->stripcount++;

			meshBuilder->used[j] = 2;
			goto nexttri;
		}
	}
done:

	// clear the temp used flags
	for (j = starttri + 1; j < numtris; j++)
		if (meshBuilder->used[j] == 2)
			meshBuilder->used[j] = 0;

	return meshBuilder->stripcount;
}

/*
   Generate a list of trifans or strips
   for the model, which holds for all frames
 */
void BuildTris(MeshBuilder *meshBuilder, aliashdr_t *pheader, mtriangle_t *triangles, stvert_t *stverts)
{
	//
	// build tristrips
	//
	meshBuilder->numorder = 0;
	meshBuilder->numcommands = 0;
	memset(meshBuilder->used, 0, sizeof(meshBuilder->used));
	for (int i = 0; i < pheader->numtris; i++)
	{
		// pick an unused triangle and start the trifan
		if (meshBuilder->used[i])
			continue;

        int besttype = 0;
		int bestlen = 0;
		for (int type = 0; type < 2; type++)
		{
			//	type = 1;
			for (int startv = 0; startv < 3; startv++)
			{
                int len;
				if (type == 1)
					len = StripLength(meshBuilder, pheader, triangles, i, startv);
				else
					len = FanLength(meshBuilder, pheader, triangles, i, startv);
				if (len > bestlen)
				{
					besttype = type;
					bestlen = len;
					for (int j = 0; j < bestlen + 2; j++)
						meshBuilder->bestverts[j] = meshBuilder->stripverts[j];
					for (int j = 0; j < bestlen; j++)
						meshBuilder->besttris[j] = meshBuilder->striptris[j];
				}
			}
		}

		// mark the tris on the best strip as used
		for (int j = 0; j < bestlen; j++)
			meshBuilder->used[meshBuilder->besttris[j]] = 1;

		if (besttype == 1)
			meshBuilder->commands[meshBuilder->numcommands++] = (bestlen + 2);
		else
			meshBuilder->commands[meshBuilder->numcommands++] = -(bestlen + 2);

		for (int j = 0; j < bestlen + 2; j++)
		{
			// emit a vertex into the reorder buffer
			int k = meshBuilder->bestverts[j];
			meshBuilder->vertexorder[meshBuilder->numorder++] = k;

			// emit s/t coords into the commands stream
			float s = stverts[k].s;
			float t = stverts[k].t;
			if (!triangles[meshBuilder->besttris[0]].facesfront && stverts[k].onseam)
				s += pheader->skinwidth / 2;                                                       // on back side
			s = (s + 0.5f) / pheader->skinwidth;
			t = (t + 0.5f) / pheader->skinheight;

            float *command = (float *)&meshBuilder->commands[meshBuilder->numcommands];
            command[0] = s;
            command[1] = t;
            meshBuilder->numcommands += 2;
		}
	}

	meshBuilder->commands[meshBuilder->numcommands++] = 0; // end of list marker

	Con_DPrintf("%3i tri %3i vert %3i cmd\n", pheader->numtris, meshBuilder->numorder, meshBuilder->numcommands);

	meshBuilder->allverts += meshBuilder->numorder;
	meshBuilder->alltris += pheader->numtris;
}

MeshBuilder l_meshBuilder;

void GL_MakeAliasModelDisplayLists(model_t *m, aliashdr_t *pheader, mtriangle_t *triangles, stvert_t *stverts)
{
    MeshBuilder *meshBuilder = &l_meshBuilder;
    
    #if 0 // This doesn't seem to work.
	//
	// look for a cached version
	//
	char cache[MAX_QPATH+1], fullpath[MAX_OSPATH+1];
    cache[MAX_QPATH] = 0;
	strncpy(cache, "glquake/", MAX_QPATH);
	COM_StripExtension(m->name + strlen("progs/"), cache + strlen("glquake/"));
	strncat(cache, ".ms2", MAX_QPATH);

	FILE *f;
	COM_FOpenFile(cache, &f);
	if (f)
	{
		fread(&meshBuilder->numcommands, 4, 1, f);
		fread(&meshBuilder->numorder, 4, 1, f);
		fread(&meshBuilder->commands, meshBuilder->numcommands * sizeof(meshBuilder->commands[0]), 1, f);
		fread(&meshBuilder->vertexorder, meshBuilder->numorder * sizeof(meshBuilder->vertexorder[0]), 1, f);
		fclose(f);
	}
	else
    #endif
	{
		//
		// build it from scratch
		//
		Con_Printf("meshing %s...\n", m->name);

		BuildTris(meshBuilder, pheader, triangles, stverts); // trifans or lists

        #if 0
		//
		// save out the cached version
		//
        char fullpath[MAX_OSPATH+1];
		snprintf(fullpath, MAX_OSPATH, "%s/%s", com_writableGamedir, cache);
        fullpath[MAX_OSPATH] = 0;
		f = fopen(fullpath, "wb");
		if (f)
		{
			fwrite(&meshBuilder->numcommands, 4, 1, f);
			fwrite(&meshBuilder->numorder, 4, 1, f);
			fwrite(&meshBuilder->commands, meshBuilder->numcommands * sizeof(meshBuilder->commands[0]), 1, f);
			fwrite(&meshBuilder->vertexorder, meshBuilder->numorder * sizeof(meshBuilder->vertexorder[0]), 1, f);
			fclose(f);
		}
        #endif
	}

	// save the data out

	pheader->poseverts = meshBuilder->numorder;

	int *cmds = Hunk_Alloc(meshBuilder->numcommands * 4);
	pheader->commands = (byte *)cmds - (byte *)pheader;
	memcpy(cmds, meshBuilder->commands, meshBuilder->numcommands * 4);

	trivertx_t *verts = Hunk_Alloc(pheader->numposes * pheader->poseverts * sizeof(trivertx_t));
	pheader->posedata = (byte *)verts - (byte *)pheader;
	for (int i = 0; i < pheader->numposes; i++)
		for (int j = 0; j < meshBuilder->numorder; j++)
			*verts++ = poseverts[i][meshBuilder->vertexorder[j]];
}
