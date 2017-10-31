#ifndef quakedef_h
#define quakedef_h

#include "Common/common.h"
#include "Common/cvar.h"
#include "Common/mathlib.h"

#define QUAKE_GAME // as opposed to utilities

#define QUAKE_HOST_VERSION 1.09

#if defined(QUAKE_DEBUG)
#define PARANOID // speed sapping error checking
#endif

#define GAMENAME "id1"

#define UNUSED(x) (x = x) // for pesky compiler / lint warnings

#define MINIMUM_MEMORY 0x550000 // FIXME: Meaningless with all engine changes
#define MINIMUM_MEMORY_LEVELPAK (MINIMUM_MEMORY + 0x100000)

// up / down
#define PITCH 0
// left / right
#define YAW 1
// fall over
#define ROLL 2

#define ON_EPSILON 0.1f // point on plane side epsilon

#define MAX_MSGLEN 8000 // max length of a reliable message
#define MAX_DATAGRAM 1024 // max length of unreliable message

#define SAVEGAME_COMMENT_LENGTH 39

// stock defines

#define IT_SHOTGUN 1
#define IT_SUPER_SHOTGUN 2
#define IT_NAILGUN 4
#define IT_SUPER_NAILGUN 8
#define IT_GRENADE_LAUNCHER 16
#define IT_ROCKET_LAUNCHER 32
#define IT_LIGHTNING 64
#define IT_SUPER_LIGHTNING 128
#define IT_SHELLS 256
#define IT_NAILS 512
#define IT_ROCKETS 1024
#define IT_CELLS 2048
#define IT_AXE 4096
#define IT_ARMOR1 8192
#define IT_ARMOR2 16384
#define IT_ARMOR3 32768
#define IT_SUPERHEALTH 65536
#define IT_KEY1 131072
#define IT_KEY2 262144
#define IT_INVISIBILITY 524288
#define IT_INVULNERABILITY 1048576
#define IT_SUIT 2097152
#define IT_QUAD 4194304
#define IT_SIGIL1 (1 << 28)
#define IT_SIGIL2 (1 << 29)
#define IT_SIGIL3 (1 << 30)
#define IT_SIGIL4 (1 << 31)

//===========================================
//rogue changed and added defines

#define RIT_SHELLS 128
#define RIT_NAILS 256
#define RIT_ROCKETS 512
#define RIT_CELLS 1024
#define RIT_AXE 2048
#define RIT_LAVA_NAILGUN 4096
#define RIT_LAVA_SUPER_NAILGUN 8192
#define RIT_MULTI_GRENADE 16384
#define RIT_MULTI_ROCKET 32768
#define RIT_PLASMA_GUN 65536
#define RIT_ARMOR1 8388608
#define RIT_ARMOR2 16777216
#define RIT_ARMOR3 33554432
#define RIT_LAVA_NAILS 67108864
#define RIT_PLASMA_AMMO 134217728
#define RIT_MULTI_ROCKETS 268435456
#define RIT_SHIELD 536870912
#define RIT_ANTIGRAV 1073741824
#define RIT_SUPERHEALTH 2147483648

//MED 01/04/97 added hipnotic defines
//===========================================
//hipnotic added defines
#define HIT_PROXIMITY_GUN_BIT 16
#define HIT_MJOLNIR_BIT 7
#define HIT_LASER_CANNON_BIT 23
#define HIT_PROXIMITY_GUN (1 << HIT_PROXIMITY_GUN_BIT)
#define HIT_MJOLNIR (1 << HIT_MJOLNIR_BIT)
#define HIT_LASER_CANNON (1 << HIT_LASER_CANNON_BIT)
#define HIT_WETSUIT (1 << (23 + 2))
#define HIT_EMPATHY_SHIELDS (1 << (23 + 3))

//===========================================

#define MAX_SCOREBOARD 16

#define SOUND_CHANNELS 8

// This makes anyone on id's net privileged
// Use for multiplayer testing only - VERY dangerous!!!
// #define IDGODS

extern struct cvar_s registered;

extern qboolean standard_quake, rogue, hipnotic;

// the host system specifies the base of the directory tree, the
// command line parms passed to the program, and the amount of memory
// available for the program to use

extern qboolean noclip_anglehack;

//
// host
//

extern cvar_t sys_ticrate;
extern cvar_t developer;

extern double real_frametime; // Not bounded
extern double host_frametime;
extern byte *host_basepal;
extern byte *host_colormap;
extern int host_framecount; // incremented every frame, never reset
extern double realtime; // not bounded in any way, changed at start of every frame, never reset

void Host_ClearMemory();
void Host_ServerFrame();
void Host_InitCommands();
void Host_Init(quakeparms_t *parms);
void Host_Shutdown();
void Host_Error(char *error, ...);
void Host_EndGame(char *message, ...);
void Host_Frame(float time);
void Host_Exit();
void Host_Quit_f();
void Host_ClientCommands(char *fmt, ...);
void Host_ShutdownServer(qboolean crash);

//  an fullscreen DIB focus gain/loss
extern int current_skill; // skill level for currently loaded level (in case
//  the user changes the cvar while the level is
//  running, this reflects the level actually in use)

extern int minimum_memory;

//
// chase
//
extern cvar_t chase_active;

void Chase_Init();
void Chase_Reset();
void Chase_Update();

#endif
