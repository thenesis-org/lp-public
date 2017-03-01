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
 * This file is the starting point of the program and implements
 * the main loop
 *
 * =======================================================================
 */

#include "common/common.h"

#include "SDL/SDLWrapper.h"

#include <fcntl.h>
#include <locale.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	int time, oldtime, newtime;
	int verLen, i;
	const char * versionString;

	versionString = va("%s based on Yamagi Quake II v5.34", QUAKE2_COMPLETE_NAME);
	verLen = Q_strlen(versionString);

	printf("\n%s\n", versionString);
	for (i = 0; i < verLen; ++i)
	{
		putc('=', stdout);
	}
	puts("\n");

	#ifndef DEDICATED_ONLY
	printf("Client build options:\n");
	#ifdef OGG
	printf(" + OGG/Vorbis\n");
	#else
	printf(" - OGG/Vorbis\n");
	#endif
	#ifdef USE_OPENAL
	printf(" + OpenAL audio\n");
	#else
	printf(" - OpenAL audio\n");
	#endif
	#ifdef ZIP
	printf(" + Zip file support\n");
	#else
	printf(" - Zip file support\n");
	#endif
	#endif

	printf("Platform: %s\n", OSTYPE);
	printf("Architecture: %s\n", ARCH);

	/* Seed PRNG */
	randk_seed();

	/* Initialze the game */
	Qcommon_Init(argc, argv);

	/* Do not delay reads on stdin*/
	//fcntl(fileno(stdin), F_SETFL, fcntl(fileno(stdin), F_GETFL, NULL) | O_NONBLOCK);

	oldtime = Sys_Milliseconds();

	/* The legendary Quake II mainloop */
	while (1)
	{
		if (sdlwIsExitRequested())
		{
			Com_Quit();
		}

		/* find time spent rendering last frame */
		do
		{
			newtime = Sys_Milliseconds();
			time = newtime - oldtime;
		}
		while (time < 1);

		Qcommon_Frame(time);
		oldtime = newtime;
	}

	return 0;
}
