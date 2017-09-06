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
 */

#include "backends/input.h"
#include "common/common.h"

#include <SDL2/SDL.h>

#include <stdio.h>
#include <string.h>

unsigned int sys_frame_time;
static void *game_library = NULL;
int curtime;

int Sys_Milliseconds()
{
	static int base;
	static qboolean initialized = false;

	if (!initialized)
	{ /* let base retain 16 bits of effectively random data */
		base = SDL_GetTicks() & 0xffff0000;
		initialized = true;
	}

	curtime = SDL_GetTicks() - base;

	return curtime;
}

void Sys_RedirectStdout()
{
	char path_stdout[MAX_OSPATH];
	char path_stderr[MAX_OSPATH];

	if (dedicated && dedicated->value)
		return;

	char *home = Sys_GetHomeDir();
	if (home == NULL)
		return;

	if (FS_CreatePath(home))
		return;

	snprintf(path_stdout, sizeof(path_stdout), "%s/%s", home, "stdout.txt");
	snprintf(path_stderr, sizeof(path_stderr), "%s/%s", home, "stderr.txt");

	freopen(path_stdout, "w", stdout);
	freopen(path_stderr, "w", stderr);
}

void Sys_SendKeyEvents()
{
	#ifndef DEDICATED_ONLY
	IN_Update();
	#endif

	/* grab frame time */
	sys_frame_time = Sys_Milliseconds();
}

#if 1

const char* Sys_GetBinaryDir()
{
	static char exeDir[4096] = { 0 };

	if (exeDir[0] != '\0')
		return exeDir;

	Sys_GetExecutablePath(exeDir, 4096);

	#ifdef _WIN32
	while (1)
	{
		char *backSlash = strchr(exeDir, '\\');
		if (backSlash == NULL)
			break;
		*backSlash = '/';
	}
	#endif // _WIN32

	// cut off executable name
	char * lastSlash = strrchr(exeDir, '/');
	if (lastSlash != NULL)
		lastSlash[1] = '\0'; // cut off after last (back)slash

	return exeDir;
}

#else

const char* Sys_GetBinaryDir()
{
	static char *exeDir = NULL;
	if (exeDir == NULL)
	{
		exeDir = SDL_GetBasePath();

		#ifdef _WIN32
		while (1)
		{
			char *backSlash = strchr(exeDir, '\\');
			if (backSlash == NULL)
				break;
			*backSlash = '/';
		}
		#endif // _WIN32
	}
	return exeDir;
}

#endif

char* Sys_GetCurrentDirectory()
{
	static char *workingDir = NULL;
	if (workingDir == NULL)
	{
		workingDir = SDL_GetBasePath();

		#ifdef _WIN32
		while (1)
		{
			char *backSlash = strchr(workingDir, '\\');
			if (backSlash == NULL)
				break;
			*backSlash = '/';
		}
		#endif // _WIN32
	}
	return workingDir;
}

char* Sys_GetHomeDir()
{
	static char *homeDir = NULL;
	if (homeDir == NULL)
	{
		homeDir = SDL_GetPrefPath(QUAKE2_TEAM_NAME, "Quake2");

		#ifdef _WIN32
		while (1)
		{
			char *backSlash = strchr(homeDir, '\\');
			if (backSlash == NULL)
				break;
			*backSlash = '/';
		}
		#endif // _WIN32
	}
	return homeDir;
}

void Sys_FreeLibrary(void *handle)
{
	if (!handle)
		return;

	SDL_UnloadObject(handle);
}

void* Sys_LoadLibrary(const char *path, const char *sym, void **handle)
{
	*handle = NULL;

	void *module = SDL_LoadObject(path);
	if (!module)
	{
		//Com_Printf("%s failed: SDL_LoadObject returned NULL on %s\n", __func__, path);
		return NULL;
	}

	void *entry = NULL;
	if (sym)
	{
		entry = SDL_LoadFunction(module, sym);
		if (!entry)
		{
			Com_Printf("%s failed: GetProcAddress returned NULL on %s\n", __func__, path);
			SDL_UnloadObject(module);
			return NULL;
		}
	}

	*handle = module;

	Com_DPrintf("%s succeeded: %s\n", __func__, path);

	return entry;
}

void* Sys_GetProcAddress(void *handle, const char *sym)
{
	return SDL_LoadFunction(handle, sym);
}

void Sys_UnloadGame()
{
	Sys_FreeLibrary(game_library);
	game_library = NULL;
}

void* Sys_GetGameAPI(void *parms)
{
	#if defined(__unix__)
	const char *moduleName = "game.so";
	#elif defined(_WIN32)
	const char *moduleName = "game.dll";
	#else
	const char *moduleName = "game";
	#endif
	char name[MAX_OSPATH];

	if (game_library)
	{
		Com_Error(ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");
		Sys_UnloadGame();
	}

	/* now run through the search paths */
	char *path = NULL;
	while (1)
	{
		path = FS_NextPath(path);
		if (!path)
		{
			return NULL; /* couldn't find one anywhere */
		}

		Com_sprintf(name, sizeof(name), "%s/%s", path, moduleName);
		Sys_LoadLibrary(name, NULL, &game_library);
		if (game_library)
		{
			Com_DPrintf("LoadLibrary (%s)\n", name);
			break;
		}
	}

	void * (*GetGameAPI)(void *);
	GetGameAPI = (void *)Sys_GetProcAddress(game_library, "GetGameAPI");
	if (!GetGameAPI)
	{
		Sys_UnloadGame();
		return NULL;
	}

	return (void *)GetGameAPI(parms);
}
