#include "Client/client.h"
#include "Common/quakedef.h"
#include "Common/sys.h"

#include <SDL2/SDL.h>

#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#ifndef __WIN32__
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mman.h>
#endif

bool logFileEnabled = true;

char *basedir = ".";
char *cachedir = "/tmp";

cvar_t sys_linerefresh = { "sys_linerefresh", "0" }; // set for entity display

// =======================================================================
// General routines
// =======================================================================
void Sys_Printf(char *fmt, ...)
{
	va_list argptr;
	va_start(argptr, fmt);
    vfprintf(stdout, fmt, argptr);
	va_end(argptr);
}

void Sys_Quit()
{
	Host_Shutdown();
	exit(0);
}

void Sys_Error(char *error, ...)
{
	va_list argptr;
	char string[1024];
	va_start(argptr, error);
	vsnprintf(string, 1024, error, argptr);
    string[1023] = 0;
	va_end(argptr);
	fprintf(stderr, "Error: %s\n", string);

	Host_Shutdown();
	exit(1);
}

void Sys_Warn(char *warning, ...)
{
	va_list argptr;
	char string[1024];
	va_start(argptr, warning);
	vsnprintf(string, 1024, warning, argptr);
    string[1023] = 0;
	va_end(argptr);
	fprintf(stderr, "Warning: %s", string);
}

void Sys_RedirectStdout()
{
	char path_stdout[MAX_OSPATH];
	char path_stderr[MAX_OSPATH];

	if (!logFileEnabled)
		return;

	char *home = Sys_GetHomeDir();
	if (home == NULL)
		return;

	if (COM_CreatePath(home))
		return;

	snprintf(path_stdout, sizeof(path_stdout), "%s/%s", home, "stdout.txt");
	snprintf(path_stderr, sizeof(path_stderr), "%s/%s", home, "stderr.txt");

	(void)freopen(path_stdout, "w", stdout);
	(void)freopen(path_stderr, "w", stderr);
}

/*
   ===============================================================================

   FILE IO

   ===============================================================================
 */

#define MAX_HANDLES 10
FILE *sys_handles[MAX_HANDLES];

int findhandle()
{
	int i;
	for (i = 1; i < MAX_HANDLES; i++)
		if (!sys_handles[i])
			return i;
	Sys_Error("out of handles");
	return -1;
}

static int Qfilelength(FILE *f)
{
	int pos;
	int end;

	pos = ftell(f);
	fseek(f, 0, SEEK_END);
	end = ftell(f);
	fseek(f, pos, SEEK_SET);

	return end;
}

int Sys_FileOpenRead(char *path, int *hndl)
{
	int i = findhandle();
	FILE *f = fopen(path, "rb");
	if (!f)
	{
		*hndl = -1;
		return -1;
	}
	sys_handles[i] = f;
	*hndl = i;
	return Qfilelength(f);
}

int Sys_FileOpenWrite(char *path)
{
	int i = findhandle();
	FILE *f = fopen(path, "wb");
	if (!f)
		Sys_Error("Error opening %s: %s", path, strerror(errno));
	sys_handles[i] = f;
	return i;
}

void Sys_FileClose(int handle)
{
	if (handle >= 0)
	{
		fclose(sys_handles[handle]);
		sys_handles[handle] = NULL;
	}
}

void Sys_FileSeek(int handle, int position)
{
	if (handle >= 0)
		fseek(sys_handles[handle], position, SEEK_SET);
}

int Sys_FileRead(int handle, void *dst, int count)
{
	char *data;
	int size, done;

	size = 0;
	if (handle >= 0)
	{
		data = dst;
		while (count > 0)
		{
			done = fread(data, 1, count, sys_handles[handle]);
			if (done == 0)
				break;
			data += done;
			count -= done;
			size += done;
		}
	}
	return size;
}

int Sys_FileWrite(int handle, void *src, int count)
{
	char *data;
	int size, done;

	size = 0;
	if (handle >= 0)
	{
		data = src;
		while (count > 0)
		{
			done = fread(data, 1, count, sys_handles[handle]);
			if (done == 0)
				break;
			data += done;
			count -= done;
			size += done;
		}
	}
	return size;
}

int Sys_FileTime(char *path)
{
	FILE *f = fopen(path, "rb");
	if (f)
	{
		fclose(f);
		return 1;
	}
	return -1;
}

bool Sys_mkdir(char *path)
{
	#ifdef __WIN32__
	int error = _mkdir(path);
	#else
	int error = mkdir(path, 0755);
//	int error = mkdir(path, 0777);
	#endif
	return (error < 0 && errno != EEXIST);
}

void Sys_DebugLog(char *file, char *fmt, ...)
{
	va_list argptr;
	static char data[1024];
	FILE *fp;

	va_start(argptr, fmt);
	vsprintf(data, fmt, argptr);
	va_end(argptr);
	fp = fopen(file, "a");
	fwrite(data, strlen(data), 1, fp);
	fclose(fp);
}

double Sys_FloatTime()
{
	#ifdef __WIN32__

	static clock_t starttime = 0;

	if (!starttime)
		starttime = clock();

	return (clock() - starttime) * (1.0f / 1024.0f);

	#else

	struct timeval tp;
	struct timezone tzp;
	static int secbase;

	gettimeofday(&tp, &tzp);

	if (!secbase)
	{
		secbase = tp.tv_sec;
		return tp.tv_usec / 1000000.0f;
	}

	return (tp.tv_sec - secbase) + tp.tv_usec / 1000000.0f;

	#endif
}

// =======================================================================
// Sleeps for microseconds
// =======================================================================

static volatile int oktogo;

void alarm_handler(int x)
{
	oktogo = 1;
}

byte* Sys_ZoneBase(int *size)
{
	char *QUAKEOPT = getenv("QUAKEOPT");

	*size = 0xc00000;
	if (QUAKEOPT)
	{
		while (*QUAKEOPT)
			if (tolower(*QUAKEOPT++) == 'm')
			{
				*size = atof(QUAKEOPT) * 1024 * 1024;
				break;
			}
	}
	return malloc(*size);
}

void Sys_Sleep()
{
	SDL_Delay(1);
}


int main(int c, char **v)
{
	double time, oldtime, newtime;
	quakeparms_t parms;
	extern int vcrFile;
	extern int recording;

    #if 0
	parms.memsize = 8 * 1024 * 1024; 
    #else
	parms.memsize = 16 * 1024 * 1024; // Because of the numerous changes in the game engine, we probably need more memory than the original game.
    #endif
	parms.membase = malloc(parms.memsize);
	parms.basedir = basedir;

	COM_InitArgv(c, v);
	parms.argc = com_argc;
	parms.argv = com_argv;

    Sys_RedirectStdout();

	Host_Init(&parms);

	oldtime = Sys_FloatTime() - 0.1f;
	while (1)
	{
		// find time spent rendering last frame
		newtime = Sys_FloatTime();
		time = newtime - oldtime;

		if (cls.state == ca_dedicated) // play vcrfiles at max speed
		{
			if (time < sys_ticrate.value && (vcrFile == -1 || recording))
			{
				SDL_Delay(1);
				continue; // not time to run a server only tic yet
			}
			time = sys_ticrate.value;
		}

		if (time > sys_ticrate.value * 2)
			oldtime = newtime;
		else
			oldtime += time;

		Host_Frame(time);
	}
}
