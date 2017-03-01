/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (C) 2011 Yamagi Burmeister
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
 * Game fields to be saved.
 *
 * =======================================================================
 */

{ "classname", FOFS(classname), F_LSTRING, 0 },
{ "model", FOFS(model), F_LSTRING, 0 },
{ "spawnflags", FOFS(spawnflags), F_INT, 0 },
{ "speed", FOFS(speed), F_FLOAT, 0 },
{ "accel", FOFS(accel), F_FLOAT, 0 },
{ "decel", FOFS(decel), F_FLOAT, 0 },
{ "target", FOFS(target), F_LSTRING, 0 },
{ "targetname", FOFS(targetname), F_LSTRING, 0 },
{ "pathtarget", FOFS(pathtarget), F_LSTRING, 0 },
{ "deathtarget", FOFS(deathtarget), F_LSTRING, 0 },
{ "killtarget", FOFS(killtarget), F_LSTRING, 0 },
{ "combattarget", FOFS(combattarget), F_LSTRING, 0 },
{ "message", FOFS(message), F_LSTRING, 0 },
{ "team", FOFS(team), F_LSTRING, 0 },
{ "wait", FOFS(wait), F_FLOAT, 0 },
{ "delay", FOFS(delay), F_FLOAT, 0 },
{ "random", FOFS(random), F_FLOAT, 0 },
{ "move_origin", FOFS(move_origin), F_VECTOR, 0 },
{ "move_angles", FOFS(move_angles), F_VECTOR, 0 },
{ "style", FOFS(style), F_INT, 0 },
{ "count", FOFS(count), F_INT, 0 },
{ "health", FOFS(health), F_INT, 0 },
{ "sounds", FOFS(sounds), F_INT, 0 },
{ "light", 0, F_IGNORE, 0 },
{ "dmg", FOFS(dmg), F_INT, 0 },
{ "mass", FOFS(mass), F_INT, 0 },
{ "volume", FOFS(volume), F_FLOAT, 0 },
{ "attenuation", FOFS(attenuation), F_FLOAT, 0 },
{ "map", FOFS(map), F_LSTRING, 0 },
{ "origin", FOFS(s.origin), F_VECTOR, 0 },
{ "angles", FOFS(s.angles), F_VECTOR, 0 },
{ "angle", FOFS(s.angles), F_ANGLEHACK, 0 },
{ "goalentity", FOFS(goalentity), F_EDICT, FFL_NOSPAWN },
{ "movetarget", FOFS(movetarget), F_EDICT, FFL_NOSPAWN },
{ "enemy", FOFS(enemy), F_EDICT, FFL_NOSPAWN },
{ "oldenemy", FOFS(oldenemy), F_EDICT, FFL_NOSPAWN },
{ "activator", FOFS(activator), F_EDICT, FFL_NOSPAWN },
{ "groundentity", FOFS(groundentity), F_EDICT, FFL_NOSPAWN },
{ "teamchain", FOFS(teamchain), F_EDICT, FFL_NOSPAWN },
{ "teammaster", FOFS(teammaster), F_EDICT, FFL_NOSPAWN },
{ "owner", FOFS(owner), F_EDICT, FFL_NOSPAWN },
{ "mynoise", FOFS(mynoise), F_EDICT, FFL_NOSPAWN },
{ "mynoise2", FOFS(mynoise2), F_EDICT, FFL_NOSPAWN },
{ "target_ent", FOFS(target_ent), F_EDICT, FFL_NOSPAWN },
{ "chain", FOFS(chain), F_EDICT, FFL_NOSPAWN },
{ "prethink", FOFS(prethink), F_FUNCTION, FFL_NOSPAWN },
{ "think", FOFS(think), F_FUNCTION, FFL_NOSPAWN },
{ "blocked", FOFS(blocked), F_FUNCTION, FFL_NOSPAWN },
{ "touch", FOFS(touch), F_FUNCTION, FFL_NOSPAWN },
{ "use", FOFS(use), F_FUNCTION, FFL_NOSPAWN },
{ "pain", FOFS(pain), F_FUNCTION, FFL_NOSPAWN },
{ "die", FOFS(die), F_FUNCTION, FFL_NOSPAWN },
{ "stand", FOFS(monsterinfo.stand), F_FUNCTION, FFL_NOSPAWN },
{ "idle", FOFS(monsterinfo.idle), F_FUNCTION, FFL_NOSPAWN },
{ "search", FOFS(monsterinfo.search), F_FUNCTION, FFL_NOSPAWN },
{ "walk", FOFS(monsterinfo.walk), F_FUNCTION, FFL_NOSPAWN },
{ "run", FOFS(monsterinfo.run), F_FUNCTION, FFL_NOSPAWN },
{ "dodge", FOFS(monsterinfo.dodge), F_FUNCTION, FFL_NOSPAWN },
{ "attack", FOFS(monsterinfo.attack), F_FUNCTION, FFL_NOSPAWN },
{ "melee", FOFS(monsterinfo.melee), F_FUNCTION, FFL_NOSPAWN },
{ "sight", FOFS(monsterinfo.sight), F_FUNCTION, FFL_NOSPAWN },
{ "checkattack", FOFS(monsterinfo.checkattack), F_FUNCTION, FFL_NOSPAWN },
{ "currentmove", FOFS(monsterinfo.currentmove), F_MMOVE, FFL_NOSPAWN },
{ "endfunc", FOFS(moveinfo.endfunc), F_FUNCTION, FFL_NOSPAWN },
{ "lip", STOFS(lip), F_INT, FFL_SPAWNTEMP },
{ "distance", STOFS(distance), F_INT, FFL_SPAWNTEMP },
{ "height", STOFS(height), F_INT, FFL_SPAWNTEMP },
{ "noise", STOFS(noise), F_LSTRING, FFL_SPAWNTEMP },
{ "pausetime", STOFS(pausetime), F_FLOAT, FFL_SPAWNTEMP },
{ "item", STOFS(item), F_LSTRING, FFL_SPAWNTEMP },
{ "item", FOFS(item), F_ITEM, 0 },
{ "gravity", STOFS(gravity), F_LSTRING, FFL_SPAWNTEMP },
{ "sky", STOFS(sky), F_LSTRING, FFL_SPAWNTEMP },
{ "skyrotate", STOFS(skyrotate), F_FLOAT, FFL_SPAWNTEMP },
{ "skyaxis", STOFS(skyaxis), F_VECTOR, FFL_SPAWNTEMP },
{ "minyaw", STOFS(minyaw), F_FLOAT, FFL_SPAWNTEMP },
{ "maxyaw", STOFS(maxyaw), F_FLOAT, FFL_SPAWNTEMP },
{ "minpitch", STOFS(minpitch), F_FLOAT, FFL_SPAWNTEMP },
{ "maxpitch", STOFS(maxpitch), F_FLOAT, FFL_SPAWNTEMP },
{ "nextmap", STOFS(nextmap), F_LSTRING, FFL_SPAWNTEMP },
{ 0, 0, 0, 0 }
