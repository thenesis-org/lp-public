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
 * This file implements all system dependend generic funktions
 *
 * =======================================================================
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dlfcn.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/select.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <termios.h>
#include <unistd.h>

#include "SDL/SDLWrapper.h"

#include "common/common.h"
#include "common/glob.h"
#include "backends/input.h"

//--------------------------------------------------------------------------------
// File system.
//--------------------------------------------------------------------------------
#define PATH_MAX 4096

void Sys_GetExecutablePath(char * exePath, int maxLength)
{
	// all the platforms that have /proc/$pid/exe or similar that symlink the
	// real executable - basiscally Linux and the BSDs except for FreeBSD which
	// doesn't enable proc by default and has a sysctl() for this
	char buf[PATH_MAX] = { 0 };
	snprintf(buf, sizeof(buf), "/proc/%d/exe", getpid());

	// readlink() doesn't null-terminate!
	int len = readlink(buf, exePath, maxLength - 1);
	if (len <= 0)
	{
		// an error occured, clear exe path
		exePath[0] = '\0';
	}
	else
	{
		exePath[len] = '\0';
	}
}

qboolean Sys_Mkdir(char *path)
{
	int error = mkdir(path, 0755);
	if (error < 0 && errno != EEXIST)
		return true;


	return false;
}

static char findbase[MAX_OSPATH];
static char findpath[MAX_OSPATH];
static char findpattern[MAX_OSPATH];
static DIR *fdir;

static qboolean CompareAttributes(char *path, char *name, unsigned musthave, unsigned canthave)
{
	/* . and .. never match */
	if ((strcmp(name, ".") == 0) || (strcmp(name, "..") == 0))
	{
		return false;
	}

	return true;
}

char* Sys_FindFirst(char *path, unsigned musthave, unsigned canhave)
{
	struct dirent *d;
	char *p;

	if (fdir)
	{
		Sys_Error("Sys_BeginFind without close");
	}

	strcpy(findbase, path);

	if ((p = strrchr(findbase, '/')) != NULL)
	{
		*p = 0;
		strcpy(findpattern, p + 1);
	}
	else
	{
		strcpy(findpattern, "*");
	}

	if (strcmp(findpattern, "*.*") == 0)
	{
		strcpy(findpattern, "*");
	}

	if ((fdir = opendir(findbase)) == NULL)
	{
		return NULL;
	}

	while ((d = readdir(fdir)) != NULL)
	{
		if (!*findpattern || glob_match(findpattern, d->d_name))
		{
			if (CompareAttributes(findbase, d->d_name, musthave, canhave))
			{
				sprintf(findpath, "%s/%s", findbase, d->d_name);
				return findpath;
			}
		}
	}

	return NULL;
}

char* Sys_FindNext(unsigned musthave, unsigned canhave)
{
	struct dirent *d;

	if (fdir == NULL)
	{
		return NULL;
	}

	while ((d = readdir(fdir)) != NULL)
	{
		if (!*findpattern || glob_match(findpattern, d->d_name))
		{
			if (CompareAttributes(findbase, d->d_name, musthave, canhave))
			{
				sprintf(findpath, "%s/%s", findbase, d->d_name);
				return findpath;
			}
		}
	}

	return NULL;
}

void Sys_FindClose()
{
	if (fdir != NULL)
	{
		closedir(fdir);
	}

	fdir = NULL;
}

//--------------------------------------------------------------------------------
// Console.
//--------------------------------------------------------------------------------
qboolean stdin_active = true;

void Sys_ConsoleOutput(char *string)
{
	fputs(string, stdout);
}

char* Sys_ConsoleInput()
{
	static char text[256];
	int len;
	fd_set fdset;
	struct timeval timeout;

	if (!dedicated || !dedicated->value)
	{
		return NULL;
	}

	if (!stdin_active)
	{
		return NULL;
	}

	FD_ZERO(&fdset);
	FD_SET(0, &fdset); /* stdin */
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	if ((select(1, &fdset, NULL, NULL, &timeout) == -1) || !FD_ISSET(0, &fdset))
	{
		return NULL;
	}

	len = read(0, text, sizeof(text));

	if (len == 0) /* eof! */
	{
		stdin_active = false;
		return NULL;
	}

	if (len < 1)
	{
		return NULL;
	}

	text[len - 1] = 0; /* rip off the /n and terminate */


	return text;
}

//--------------------------------------------------------------------------------
// Global.
//--------------------------------------------------------------------------------
void Sys_Init()
{
}

extern FILE *logfile;

void Sys_Quit()
{
	#ifndef DEDICATED_ONLY
	CL_Shutdown();
	#endif

	if (logfile)
	{
		fclose(logfile);
		logfile = NULL;
	}

	fcntl(0, F_SETFL, fcntl(0, F_GETFL, 0) & ~O_NONBLOCK);

	printf("------------------------------------\n");

	exit(0);
}

void Sys_Error(char *error, ...)
{
	va_list argptr;
	char string[1024];

	/* change stdin to non blocking */
	fcntl(0, F_SETFL, fcntl(0, F_GETFL, 0) & ~O_NONBLOCK);

	#ifndef DEDICATED_ONLY
	CL_Shutdown();
	#endif

	va_start(argptr, error);
	vsnprintf(string, 1024, error, argptr);
	va_end(argptr);
	fprintf(stderr, "Error: %s\n", string);
	
	exit(1);
}

void Sys_Sleep(int ms)
{
}

int main(int argc, char **argv)
{
	Qcommon_Run(argc, argv);
	return 0;
}

