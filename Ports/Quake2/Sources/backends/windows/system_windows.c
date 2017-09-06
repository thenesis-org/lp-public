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
#include "backends/windows/winquake.h"
#include "common/common.h"

#include "SDL/SDLWrapper.h"

#include <errno.h>
#include <float.h>
#include <stdio.h>

#include <conio.h>
#include <direct.h>
//#include <fcntl.h>
#include <io.h>
#include <shlobj.h>

//--------------------------------------------------------------------------------
// File system.
//--------------------------------------------------------------------------------
#ifndef PATH_MAX
// Windows has a MAX_PATH = 260 #define, but allows longer paths anyway. This might not be the maximum allowed length, but is hopefully good enough for realistic use cases.
#define PATH_MAX 4096
#define _DG__DEFINED_PATH_MAX
#endif

void Sys_GetExecutablePath(char * exePath, int maxLength)
{
	DWORD len = GetModuleFileNameA(NULL, exePath, maxLength);
	if (len == 0 || len == (DWORD)maxLength)
	{
		// an error occured, clear exe path
		exePath[0] = '\0';
	}
}

qboolean Sys_Mkdir(char *path)
{
	int error = _mkdir(path);
	return (error < 0 && errno != EEXIST);
}

static char findbase[MAX_OSPATH];
static char findpath[MAX_OSPATH];
static int findhandle;

static qboolean CompareAttributes(unsigned found, unsigned musthave, unsigned canthave)
{
	if ((found & _A_RDONLY) && (canthave & SFF_RDONLY))
		return false;
	if ((found & _A_HIDDEN) && (canthave & SFF_HIDDEN))
		return false;
	if ((found & _A_SYSTEM) && (canthave & SFF_SYSTEM))
		return false;
	if ((found & _A_SUBDIR) && (canthave & SFF_SUBDIR))
		return false;
	if ((found & _A_ARCH) && (canthave & SFF_ARCH))
		return false;
	if ((musthave & SFF_RDONLY) && !(found & _A_RDONLY))
		return false;
	if ((musthave & SFF_HIDDEN) && !(found & _A_HIDDEN))
		return false;
	if ((musthave & SFF_SYSTEM) && !(found & _A_SYSTEM))
		return false;
	if ((musthave & SFF_SUBDIR) && !(found & _A_SUBDIR))
		return false;
	if ((musthave & SFF_ARCH) && !(found & _A_ARCH))
		return false;
	return true;
}

char* Sys_FindFirst(char *path, unsigned musthave, unsigned canthave)
{
	struct _finddata_t findinfo;

	if (findhandle)
	{
		Sys_Error("Sys_BeginFind without close");
	}

	findhandle = 0;

	COM_FilePath(path, findbase);
	findhandle = _findfirst(path, &findinfo);

	if (findhandle == -1)
		return NULL;
	if (!CompareAttributes(findinfo.attrib, musthave, canthave))
		return NULL;

	Com_sprintf(findpath, sizeof(findpath), "%s/%s", findbase, findinfo.name);
	return findpath;
}

char* Sys_FindNext(unsigned musthave, unsigned canthave)
{
	struct _finddata_t findinfo;
	if (findhandle == -1)
		return NULL;
	if (_findnext(findhandle, &findinfo) == -1)
		return NULL;
	if (!CompareAttributes(findinfo.attrib, musthave, canthave))
		return NULL;
	Com_sprintf(findpath, sizeof(findpath), "%s/%s", findbase, findinfo.name);
	return findpath;
}

void Sys_FindClose()
{
	if (findhandle != -1)
		_findclose(findhandle);
	findhandle = 0;
}

//--------------------------------------------------------------------------------
// Console.
//--------------------------------------------------------------------------------
static HANDLE hinput, houtput;
static char console_text[256];
static int console_textlen;

char* Sys_ConsoleInput()
{
	INPUT_RECORD recs[1024];
	int ch;
	DWORD dummy, numread, numevents;

	if (!dedicated || !dedicated->value)
	{
		return NULL;
	}

	for (;; )
	{
		if (!GetNumberOfConsoleInputEvents(hinput, &numevents))
			Sys_Error("Error getting # of console events");

		if (numevents <= 0)
			break;

		if (!ReadConsoleInput(hinput, recs, 1, &numread))
			Sys_Error("Error reading console input");

		if (numread != 1)
			Sys_Error("Couldn't read console input");

		if (recs[0].EventType == KEY_EVENT)
		{
			if (!recs[0].Event.KeyEvent.bKeyDown)
			{
				ch = recs[0].Event.KeyEvent.uChar.AsciiChar;

				switch (ch)
				{
				case '\r':
					WriteFile(houtput, "\r\n", 2, &dummy, NULL);

					if (console_textlen)
					{
						console_text[console_textlen] = 0;
						console_textlen = 0;
						return console_text;
					}

					break;

				case '\b':

					if (console_textlen)
					{
						console_textlen--;
						WriteFile(houtput, "\b \b", 3, &dummy, NULL);
					}

					break;

				default:

					if (ch >= ' ')
					{
						if (console_textlen < (int)sizeof(console_text) - 2)
						{
							WriteFile(houtput, &ch, 1, &dummy, NULL);
							console_text[console_textlen] = ch;
							console_textlen++;
						}
					}

					break;
				}
			}
		}
	}

	return NULL;
}

void Sys_ConsoleOutput(char *string)
{
	char text[256];
	DWORD dummy;

	if (!dedicated || !dedicated->value)
	{
		fputs(string, stdout);
	}
	else
	{
		if (console_textlen)
		{
			text[0] = '\r';
			memset(&text[1], ' ', console_textlen);
			text[console_textlen + 1] = '\r';
			text[console_textlen + 2] = 0;
			WriteFile(houtput, text, console_textlen + 2, &dummy, NULL);
		}

		WriteFile(houtput, string, Q_strlen(string), &dummy, NULL);

		if (console_textlen)
		{
			WriteFile(houtput, console_text, console_textlen, &dummy, NULL);
		}
	}
}

//--------------------------------------------------------------------------------
// Global.
//--------------------------------------------------------------------------------
void Sys_Init()
{
	timeBeginPeriod(1);
	if (dedicated->value)
	{
		AllocConsole();

		hinput = GetStdHandle(STD_INPUT_HANDLE);
		houtput = GetStdHandle(STD_OUTPUT_HANDLE);
	}
}

void Sys_Quit()
{
	timeEndPeriod(1);

	#ifndef DEDICATED_ONLY
	CL_Shutdown();
	#endif

	if (dedicated && dedicated->value)
	{
		FreeConsole();
	}

	/* Close stdout and stderr */
	#ifndef DEDICATED_ONLY
	fclose(stdout);
	fclose(stderr);
	#endif

	exit(0);
}

void Sys_Error(char *error, ...)
{
	va_list argptr;
	char text[1024];

	#ifndef DEDICATED_ONLY
	CL_Shutdown();
	#endif

	va_start(argptr, error);
	vsprintf(text, error, argptr);
	va_end(argptr);

	#ifndef DEDICATED_ONLY
	fprintf(stderr, "Error: %s\n", text);
	#endif

	MessageBox(NULL, text, "Error", 0 /* MB_OK */);

	/* Close stdout and stderr */
	#ifndef DEDICATED_ONLY
	fclose(stdout);
	fclose(stderr);
	#endif

	exit(1);
}

void Sys_Sleep(int ms)
{
    Sleep(1);
}

//--------------------------------------------------------------------------------
// Main.
//--------------------------------------------------------------------------------
#define MAX_NUM_ARGVS 128
static int argc;
static char *argv[MAX_NUM_ARGVS];

static void ParseCommandLine(LPSTR lpCmdLine)
{
	argc = 1;
	argv[0] = "exe";

	while (*lpCmdLine && (argc < MAX_NUM_ARGVS))
	{
		while (*lpCmdLine && ((*lpCmdLine <= 32) || (*lpCmdLine > 126)))
		{
			lpCmdLine++;
		}

		if (*lpCmdLine)
		{
			argv[argc] = lpCmdLine;
			argc++;

			while (*lpCmdLine && ((*lpCmdLine > 32) && (*lpCmdLine <= 126)))
			{
				lpCmdLine++;
			}

			if (*lpCmdLine)
			{
				*lpCmdLine = 0;
				lpCmdLine++;
			}
		}
	}
}

/*
 * Windows main function. Containts the
 * initialization code and the main loop
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	/* Previous instances do not exist in Win32 */
	if (hPrevInstance)
		return 0;

    ParseCommandLine(lpCmdLine);
    Qcommon_Run(argc, argv);

	/* never gets here */
	return TRUE;
}
