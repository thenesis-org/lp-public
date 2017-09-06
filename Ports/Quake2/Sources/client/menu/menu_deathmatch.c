#include "client/menu/menu.h"

extern int Developer_searchpath();

static char dmoptions_statusbar[128];

static menuframework_s s_dmoptions_menu;

static menulist_s s_friendlyfire_box;
static menulist_s s_falls_box;
static menulist_s s_weapons_stay_box;
static menulist_s s_instant_powerups_box;
static menulist_s s_powerups_box;
static menulist_s s_health_box;
static menulist_s s_spawn_farthest_box;
static menulist_s s_teamplay_box;
static menulist_s s_samelevel_box;
static menulist_s s_force_respawn_box;
static menulist_s s_armor_box;
static menulist_s s_allow_exit_box;
static menulist_s s_infinite_ammo_box;
static menulist_s s_fixed_fov_box;
static menulist_s s_quad_drop_box;
static menulist_s s_no_mines_box;
static menulist_s s_no_nukes_box;
static menulist_s s_stack_double_box;
static menulist_s s_no_spheres_box;

static void MenuDeathmatch_flagCallback(void *self)
{
	menulist_s *f = (menulist_s *)self;
	int flags;
	int bit = 0;

	flags = Cvar_VariableValue("dmflags");

	if (f == &s_friendlyfire_box)
	{
		if (f->curvalue)
			flags &= ~DF_NO_FRIENDLY_FIRE;
		else
			flags |= DF_NO_FRIENDLY_FIRE;
		goto setvalue;
	}
	else if (f == &s_falls_box)
	{
		if (f->curvalue)
			flags &= ~DF_NO_FALLING;
		else
			flags |= DF_NO_FALLING;
		goto setvalue;
	}
	else if (f == &s_weapons_stay_box)
		bit = DF_WEAPONS_STAY;
	else if (f == &s_instant_powerups_box)
		bit = DF_INSTANT_ITEMS;
	else if (f == &s_allow_exit_box)
		bit = DF_ALLOW_EXIT;
	else if (f == &s_powerups_box)
	{
		if (f->curvalue)
			flags &= ~DF_NO_ITEMS;
		else
			flags |= DF_NO_ITEMS;
		goto setvalue;
	}
	else if (f == &s_health_box)
	{
		if (f->curvalue)
			flags &= ~DF_NO_HEALTH;
		else
			flags |= DF_NO_HEALTH;
		goto setvalue;
	}
	else if (f == &s_spawn_farthest_box)
		bit = DF_SPAWN_FARTHEST;
	else if (f == &s_teamplay_box)
	{
		if (f->curvalue == 1)
		{
			flags |= DF_SKINTEAMS;
			flags &= ~DF_MODELTEAMS;
		}
		else if (f->curvalue == 2)
		{
			flags |= DF_MODELTEAMS;
			flags &= ~DF_SKINTEAMS;
		}
		else
			flags &= ~(DF_MODELTEAMS | DF_SKINTEAMS);
		goto setvalue;
	}
	else if (f == &s_samelevel_box)
		bit = DF_SAME_LEVEL;
	else if (f == &s_force_respawn_box)
		bit = DF_FORCE_RESPAWN;
	else if (f == &s_armor_box)
	{
		if (f->curvalue)
			flags &= ~DF_NO_ARMOR;
		else
			flags |= DF_NO_ARMOR;
		goto setvalue;
	}
	else if (f == &s_infinite_ammo_box)
		bit = DF_INFINITE_AMMO;
	else if (f == &s_fixed_fov_box)
		bit = DF_FIXED_FOV;
	else if (f == &s_quad_drop_box)
		bit = DF_QUAD_DROP;
	else if (Developer_searchpath() == 2)
	{
		if (f == &s_no_mines_box)
			bit = DF_NO_MINES;
		else if (f == &s_no_nukes_box)
			bit = DF_NO_NUKES;
		else if (f == &s_stack_double_box)
			bit = DF_NO_STACK_DOUBLE;
		else if (f == &s_no_spheres_box)
			bit = DF_NO_SPHERES;
	}

	if (f)
	{
		if (f->curvalue == 0)
			flags &= ~bit;
		else
			flags |= bit;
	}

setvalue:
	Cvar_SetValue("dmflags", (float)flags);
	Com_sprintf(dmoptions_statusbar, sizeof(dmoptions_statusbar),
		"dmflags = %d", flags);
}

static void MenuDeathmatch_init()
{
	static const char *yes_no_names[] =
	{
		"no", "yes", 0
	};
	static const char *teamplay_names[] =
	{
		"disabled", "by skin", "by model", 0
	};
	int dmflags = Cvar_VariableValue("dmflags");
	int y = 0;

	menuframework_s *menu = &s_dmoptions_menu;
	memset(menu, 0, sizeof(menuframework_s));
	s_dmoptions_menu.x = (int)(viddef.width * 0.50f);
	s_dmoptions_menu.nitems = 0;

	s_falls_box.generic.type = MTYPE_SPINCONTROL;
	s_falls_box.generic.x = 0;
	s_falls_box.generic.y = y;
	s_falls_box.generic.name = "falling damage";
	s_falls_box.generic.callback = MenuDeathmatch_flagCallback;
	s_falls_box.itemnames = yes_no_names;
	s_falls_box.curvalue = (dmflags & DF_NO_FALLING) == 0;

	s_weapons_stay_box.generic.type = MTYPE_SPINCONTROL;
	s_weapons_stay_box.generic.x = 0;
	s_weapons_stay_box.generic.y = y += 10;
	s_weapons_stay_box.generic.name = "weapons stay";
	s_weapons_stay_box.generic.callback = MenuDeathmatch_flagCallback;
	s_weapons_stay_box.itemnames = yes_no_names;
	s_weapons_stay_box.curvalue = (dmflags & DF_WEAPONS_STAY) != 0;

	s_instant_powerups_box.generic.type = MTYPE_SPINCONTROL;
	s_instant_powerups_box.generic.x = 0;
	s_instant_powerups_box.generic.y = y += 10;
	s_instant_powerups_box.generic.name = "instant powerups";
	s_instant_powerups_box.generic.callback = MenuDeathmatch_flagCallback;
	s_instant_powerups_box.itemnames = yes_no_names;
	s_instant_powerups_box.curvalue = (dmflags & DF_INSTANT_ITEMS) != 0;

	s_powerups_box.generic.type = MTYPE_SPINCONTROL;
	s_powerups_box.generic.x = 0;
	s_powerups_box.generic.y = y += 10;
	s_powerups_box.generic.name = "allow powerups";
	s_powerups_box.generic.callback = MenuDeathmatch_flagCallback;
	s_powerups_box.itemnames = yes_no_names;
	s_powerups_box.curvalue = (dmflags & DF_NO_ITEMS) == 0;

	s_health_box.generic.type = MTYPE_SPINCONTROL;
	s_health_box.generic.x = 0;
	s_health_box.generic.y = y += 10;
	s_health_box.generic.callback = MenuDeathmatch_flagCallback;
	s_health_box.generic.name = "allow health";
	s_health_box.itemnames = yes_no_names;
	s_health_box.curvalue = (dmflags & DF_NO_HEALTH) == 0;

	s_armor_box.generic.type = MTYPE_SPINCONTROL;
	s_armor_box.generic.x = 0;
	s_armor_box.generic.y = y += 10;
	s_armor_box.generic.name = "allow armor";
	s_armor_box.generic.callback = MenuDeathmatch_flagCallback;
	s_armor_box.itemnames = yes_no_names;
	s_armor_box.curvalue = (dmflags & DF_NO_ARMOR) == 0;

	s_spawn_farthest_box.generic.type = MTYPE_SPINCONTROL;
	s_spawn_farthest_box.generic.x = 0;
	s_spawn_farthest_box.generic.y = y += 10;
	s_spawn_farthest_box.generic.name = "spawn farthest";
	s_spawn_farthest_box.generic.callback = MenuDeathmatch_flagCallback;
	s_spawn_farthest_box.itemnames = yes_no_names;
	s_spawn_farthest_box.curvalue = (dmflags & DF_SPAWN_FARTHEST) != 0;

	s_samelevel_box.generic.type = MTYPE_SPINCONTROL;
	s_samelevel_box.generic.x = 0;
	s_samelevel_box.generic.y = y += 10;
	s_samelevel_box.generic.name = "same map";
	s_samelevel_box.generic.callback = MenuDeathmatch_flagCallback;
	s_samelevel_box.itemnames = yes_no_names;
	s_samelevel_box.curvalue = (dmflags & DF_SAME_LEVEL) != 0;

	s_force_respawn_box.generic.type = MTYPE_SPINCONTROL;
	s_force_respawn_box.generic.x = 0;
	s_force_respawn_box.generic.y = y += 10;
	s_force_respawn_box.generic.name = "force respawn";
	s_force_respawn_box.generic.callback = MenuDeathmatch_flagCallback;
	s_force_respawn_box.itemnames = yes_no_names;
	s_force_respawn_box.curvalue = (dmflags & DF_FORCE_RESPAWN) != 0;

	s_teamplay_box.generic.type = MTYPE_SPINCONTROL;
	s_teamplay_box.generic.x = 0;
	s_teamplay_box.generic.y = y += 10;
	s_teamplay_box.generic.name = "teamplay";
	s_teamplay_box.generic.callback = MenuDeathmatch_flagCallback;
	s_teamplay_box.itemnames = teamplay_names;

	s_allow_exit_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_exit_box.generic.x = 0;
	s_allow_exit_box.generic.y = y += 10;
	s_allow_exit_box.generic.name = "allow exit";
	s_allow_exit_box.generic.callback = MenuDeathmatch_flagCallback;
	s_allow_exit_box.itemnames = yes_no_names;
	s_allow_exit_box.curvalue = (dmflags & DF_ALLOW_EXIT) != 0;

	s_infinite_ammo_box.generic.type = MTYPE_SPINCONTROL;
	s_infinite_ammo_box.generic.x = 0;
	s_infinite_ammo_box.generic.y = y += 10;
	s_infinite_ammo_box.generic.name = "infinite ammo";
	s_infinite_ammo_box.generic.callback = MenuDeathmatch_flagCallback;
	s_infinite_ammo_box.itemnames = yes_no_names;
	s_infinite_ammo_box.curvalue = (dmflags & DF_INFINITE_AMMO) != 0;

	s_fixed_fov_box.generic.type = MTYPE_SPINCONTROL;
	s_fixed_fov_box.generic.x = 0;
	s_fixed_fov_box.generic.y = y += 10;
	s_fixed_fov_box.generic.name = "fixed FOV";
	s_fixed_fov_box.generic.callback = MenuDeathmatch_flagCallback;
	s_fixed_fov_box.itemnames = yes_no_names;
	s_fixed_fov_box.curvalue = (dmflags & DF_FIXED_FOV) != 0;

	s_quad_drop_box.generic.type = MTYPE_SPINCONTROL;
	s_quad_drop_box.generic.x = 0;
	s_quad_drop_box.generic.y = y += 10;
	s_quad_drop_box.generic.name = "quad drop";
	s_quad_drop_box.generic.callback = MenuDeathmatch_flagCallback;
	s_quad_drop_box.itemnames = yes_no_names;
	s_quad_drop_box.curvalue = (dmflags & DF_QUAD_DROP) != 0;

	s_friendlyfire_box.generic.type = MTYPE_SPINCONTROL;
	s_friendlyfire_box.generic.x = 0;
	s_friendlyfire_box.generic.y = y += 10;
	s_friendlyfire_box.generic.name = "friendly fire";
	s_friendlyfire_box.generic.callback = MenuDeathmatch_flagCallback;
	s_friendlyfire_box.itemnames = yes_no_names;
	s_friendlyfire_box.curvalue = (dmflags & DF_NO_FRIENDLY_FIRE) == 0;

	if (Developer_searchpath() == 2)
	{
		s_no_mines_box.generic.type = MTYPE_SPINCONTROL;
		s_no_mines_box.generic.x = 0;
		s_no_mines_box.generic.y = y += 10;
		s_no_mines_box.generic.name = "remove mines";
		s_no_mines_box.generic.callback = MenuDeathmatch_flagCallback;
		s_no_mines_box.itemnames = yes_no_names;
		s_no_mines_box.curvalue = (dmflags & DF_NO_MINES) != 0;

		s_no_nukes_box.generic.type = MTYPE_SPINCONTROL;
		s_no_nukes_box.generic.x = 0;
		s_no_nukes_box.generic.y = y += 10;
		s_no_nukes_box.generic.name = "remove nukes";
		s_no_nukes_box.generic.callback = MenuDeathmatch_flagCallback;
		s_no_nukes_box.itemnames = yes_no_names;
		s_no_nukes_box.curvalue = (dmflags & DF_NO_NUKES) != 0;

		s_stack_double_box.generic.type = MTYPE_SPINCONTROL;
		s_stack_double_box.generic.x = 0;
		s_stack_double_box.generic.y = y += 10;
		s_stack_double_box.generic.name = "2x/4x stacking off";
		s_stack_double_box.generic.callback = MenuDeathmatch_flagCallback;
		s_stack_double_box.itemnames = yes_no_names;
		s_stack_double_box.curvalue = (dmflags & DF_NO_STACK_DOUBLE) != 0;

		s_no_spheres_box.generic.type = MTYPE_SPINCONTROL;
		s_no_spheres_box.generic.x = 0;
		s_no_spheres_box.generic.y = y += 10;
		s_no_spheres_box.generic.name = "remove spheres";
		s_no_spheres_box.generic.callback = MenuDeathmatch_flagCallback;
		s_no_spheres_box.itemnames = yes_no_names;
		s_no_spheres_box.curvalue = (dmflags & DF_NO_SPHERES) != 0;
	}

	Menu_AddItem(&s_dmoptions_menu, &s_falls_box);
	Menu_AddItem(&s_dmoptions_menu, &s_weapons_stay_box);
	Menu_AddItem(&s_dmoptions_menu, &s_instant_powerups_box);
	Menu_AddItem(&s_dmoptions_menu, &s_powerups_box);
	Menu_AddItem(&s_dmoptions_menu, &s_health_box);
	Menu_AddItem(&s_dmoptions_menu, &s_armor_box);
	Menu_AddItem(&s_dmoptions_menu, &s_spawn_farthest_box);
	Menu_AddItem(&s_dmoptions_menu, &s_samelevel_box);
	Menu_AddItem(&s_dmoptions_menu, &s_force_respawn_box);
	Menu_AddItem(&s_dmoptions_menu, &s_teamplay_box);
	Menu_AddItem(&s_dmoptions_menu, &s_allow_exit_box);
	Menu_AddItem(&s_dmoptions_menu, &s_infinite_ammo_box);
	Menu_AddItem(&s_dmoptions_menu, &s_fixed_fov_box);
	Menu_AddItem(&s_dmoptions_menu, &s_quad_drop_box);
	Menu_AddItem(&s_dmoptions_menu, &s_friendlyfire_box);

	if (Developer_searchpath() == 2)
	{
		Menu_AddItem(&s_dmoptions_menu, &s_no_mines_box);
		Menu_AddItem(&s_dmoptions_menu, &s_no_nukes_box);
		Menu_AddItem(&s_dmoptions_menu, &s_stack_double_box);
		Menu_AddItem(&s_dmoptions_menu, &s_no_spheres_box);
	}

	Menu_Center(&s_dmoptions_menu);

	/* set the original dmflags statusbar */
	MenuDeathmatch_flagCallback(0);
	Menu_SetStatusBar(&s_dmoptions_menu, dmoptions_statusbar);
}

static void MenuDeathmatch_draw()
{
	Menu_Draw(&s_dmoptions_menu);
}

static const char* MenuDeathmatch_key(int key, int keyUnmodified)
{
	return Default_MenuKey(&s_dmoptions_menu, key, keyUnmodified);
}

void MenuDMOptions_enter()
{
	MenuDeathmatch_init();
	M_PushMenu(MenuDeathmatch_draw, MenuDeathmatch_key);
}
