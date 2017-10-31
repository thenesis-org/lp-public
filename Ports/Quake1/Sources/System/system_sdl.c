#include "Client/client.h"
#include "Common/quakedef.h"
#include "Common/sys.h"

#include <SDL2/SDL.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#ifndef __WIN32__
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#endif

bool logFileEnabled = false;

char *basedir = ".";
char *cachedir = "/tmp";

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
	if (!logFileEnabled)
		return;

	char *home = Sys_GetHomeDir();
	if (home == NULL)
		return;

	if (COM_CreatePath(home))
		return;

	char path_stdout[MAX_OSPATH+1];
	char path_stderr[MAX_OSPATH+1];
	snprintf(path_stdout, MAX_OSPATH, "%s/%s", home, "stdout.txt");
	snprintf(path_stderr, MAX_OSPATH, "%s/%s", home, "stderr.txt");
    path_stdout[MAX_OSPATH] = 0;
    path_stderr[MAX_OSPATH] = 0;
	(void)freopen(path_stdout, "w", stdout);
	(void)freopen(path_stderr, "w", stderr);
}

/*
   ===============================================================================

   FILE IO

   ===============================================================================
 */

#define MAX_HANDLES 10
static FILE *sys_handles[MAX_HANDLES];
//static char sys_path[MAX_HANDLES][MAX_OSPATH];

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
    if (i < 0)
    {
		*hndl = -1;
        return -1;
    }
	FILE *f = fopen(path, "rb");
	if (!f)
	{
		*hndl = -1;
		return -1;
	}
	sys_handles[i] = f;
    //Q_strncpy(sys_path[i], path, MAX_OSPATH);
    //printf("Opening %s\n", path);
	*hndl = i;
	return Qfilelength(f);
}

int Sys_FileOpenWrite(char *path)
{
	int i = findhandle();
    if (i < 0)
        return -1;
	FILE *f = fopen(path, "wb");
	if (!f)
    {
		Sys_Error("Error opening %s: %s", path, strerror(errno));
        return -1;
    }
	sys_handles[i] = f;
    //Q_strncpy(sys_path[i], path, MAX_OSPATH);
	return i;
}

void Sys_FileClose(int handle)
{
	if (handle >= 0 && handle < MAX_HANDLES)
	{
        FILE *sysHandle = sys_handles[handle];
        if (sysHandle)
        {
            fclose(sysHandle);
            sys_handles[handle] = NULL;
            // printf("Closing %s\n", sys_path[handle]);
        }
	}
}

void Sys_FileSeek(int handle, int position)
{
	if (handle >= 0 && handle < MAX_HANDLES)
    {
        FILE *sysHandle = sys_handles[handle];
        if (sysHandle)
        {
            fseek(sys_handles[handle], position, SEEK_SET);
        }
    }
}

int Sys_FileRead(int handle, void *dst, int count)
{
	int size = 0;
	if (handle >= 0 && handle < MAX_HANDLES)
	{
        FILE *sysHandle = sys_handles[handle];
        if (sysHandle)
        {
            char *data = dst;
            while (count > 0)
            {
                int done = fread(data, 1, count, sysHandle);
                if (done == 0)
                    break;
                data += done;
                count -= done;
                size += done;
            }
        }
	}
	return size;
}

int Sys_FileWrite(int handle, void *src, int count)
{
	int size = 0;
	if (handle >= 0 && handle < MAX_HANDLES)
	{
        FILE *sysHandle = sys_handles[handle];
        if (sysHandle)
        {
            char *data = src;
            while (count > 0)
            {
                int done = fwrite(data, 1, count, sysHandle);
                if (done == 0)
                    break;
                data += done;
                count -= done;
                size += done;
            }
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
	quakeparms_t parms;
	extern int vcrFile;
	extern int recording;

    #if 0
	parms.memsize = 8 * 1024 * 1024; // Original game memory. It does not work with the mission packs.
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

	double oldtime = Sys_FloatTime() - 0.1f;
	while (1)
	{
		// find time spent rendering last frame
		double newtime = Sys_FloatTime();
		double time = newtime - oldtime;

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
