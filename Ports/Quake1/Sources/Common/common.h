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
// General definitions

#ifndef common_h
#define common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#define QUAKE_VERSION_NAME "0.5"
#define QUAKE_TEAM_NAME "Thenesis"
#define QUAKE_COMPLETE_NAME QUAKE_TEAM_NAME " Quake v" QUAKE_VERSION_NAME

#ifndef NULL
#define NULL ((void *)0)
#endif

#undef true
#undef false
typedef enum { false, true } qboolean;

typedef unsigned char byte;

#define Q_MAXCHAR ((char)0x7f)
#define Q_MAXSHORT ((short)0x7fff)
#define Q_MAXINT ((int)0x7fffffff)
#define Q_MAXLONG ((int)0x7fffffff)
#define Q_MAXFLOAT ((int)0x7fffffff)

#define Q_MINCHAR ((char)0x80)
#define Q_MINSHORT ((short)0x8000)
#define Q_MININT ((int)0x80000000)
#define Q_MINLONG ((int)0x80000000)
#define Q_MINFLOAT ((int)0x7fffffff)

extern qboolean bigendien;
extern short (*BigShort)(short l);
extern short (*LittleShort)(short l);
extern int (*BigLong)(int l);
extern int (*LittleLong)(int l);
extern float (*BigFloat)(float l);
extern float (*LittleFloat)(float l);

void Q_memset(void *dest, int fill, int count);
void Q_memcpy(void *dest, void *src, int count);
int Q_memcmp(void *m1, void *m2, int count);
void Q_strcpy(char *dest, char *src);
void Q_strncpy(char *dest, char *src, int count);
int Q_strlen(char *str);
char* Q_strrchr(char *s, char c);
void Q_strcat(char *dest, char *src);
int Q_strcmp(char *s1, char *s2);
int Q_strncmp(char *s1, char *s2, int count);
int Q_strcasecmp(char *s1, char *s2);
int Q_strncasecmp(char *s1, char *s2, int n);
int Q_atoi(char *str);
float Q_atof(char *str);

#define MAX_NUM_ARGVS 50

#define MAX_OSPATH 128 // max length of a filesystem pathname
#define MAX_QPATH 64 // max length of a quake game pathname

//============================================================================

typedef struct sizebuf_s
{
	qboolean allowoverflow; // if false, do a Sys_Error
	qboolean overflowed; // set to true if the buffer size failed
	byte *data;
	int maxsize;
	int cursize;
} sizebuf_t;

void SZ_Alloc(sizebuf_t *buf, int startsize);
void SZ_Free(sizebuf_t *buf);
void SZ_Clear(sizebuf_t *buf);
void* SZ_GetSpace(sizebuf_t *buf, int length);
void SZ_Write(sizebuf_t *buf, void *data, int length);
void SZ_Print(sizebuf_t *buf, char *data); // strcats onto the sizebuf

//============================================================================

typedef struct link_s
{
	struct link_s *prev, *next;
} link_t;

void ClearLink(link_t *l);
void RemoveLink(link_t *l);
void InsertLinkBefore(link_t *l, link_t *before);
void InsertLinkAfter(link_t *l, link_t *after);

// (type *)STRUCT_FROM_LINK(link_t *link, type, member)
// ent = STRUCT_FROM_LINK(link,entity_t,order)
// Old dirty version for reference.
//#define STRUCT_FROM_LINK(l, t, m) ((t *)((byte *)l - (int)&(((t *)0)->m)))
// Standard compliant version.
#define STRUCT_FROM_LINK(l, t, m) ((t *)((uintptr_t)(l) - offsetof(t, m)))

//============================================================================

void MSG_WriteChar(sizebuf_t *sb, int c);
void MSG_WriteByte(sizebuf_t *sb, int c);
void MSG_WriteShort(sizebuf_t *sb, int c);
void MSG_WriteLong(sizebuf_t *sb, int c);
void MSG_WriteFloat(sizebuf_t *sb, float f);
void MSG_WriteString(sizebuf_t *sb, char *s);
void MSG_WriteCoord(sizebuf_t *sb, float f);
void MSG_WriteAngle(sizebuf_t *sb, float f);

extern int msg_readcount;
extern qboolean msg_badread; // set if a read goes beyond end of message

void MSG_BeginReading();
int MSG_ReadChar();
int MSG_ReadByte();
int MSG_ReadShort();
int MSG_ReadLong();
float MSG_ReadFloat();
char* MSG_ReadString();

float MSG_ReadCoord();
float MSG_ReadAngle();

//============================================================================

extern char com_token[1024];
extern qboolean com_eof;

char* COM_Parse(char *data);

extern int com_argc;
extern char **com_argv;

int COM_CheckParm(char *parm);
void COM_Init(char *path);
void COM_InitArgv(int argc, char **argv);

char* COM_SkipPath(char *pathname);
void COM_StripExtension(char *in, char *out);
void COM_FileBase(char *in, char *out);
void COM_DefaultExtension(char *path, char *extension);

char* va(char *format, ...);
// does a varargs printf into a temp buffer

//============================================================================

extern int com_filesize;
struct cache_user_s;

extern char com_gamedir[MAX_OSPATH];
extern char com_writableGamedir[MAX_OSPATH];

char* Sys_GetHomeDir();

bool COM_CreatePath(char *path);

void COM_WriteFile(char *filename, void *data, int len);
int COM_OpenFile(char *filename, int *hndl);
int COM_FOpenFile(char *filename, FILE **file);
void COM_CloseFile(int h);

byte* COM_LoadStackFile(char *path, void *buffer, int bufsize);
byte* COM_LoadTempFile(char *path);
byte* COM_LoadHunkFile(char *path);
void COM_LoadCacheFile(char *path, struct cache_user_s *cu);

typedef struct
{
	char *basedir;
	int argc;
	char **argv;
	void *membase;
	int memsize;
} quakeparms_t;

extern quakeparms_t host_parms;

#endif
