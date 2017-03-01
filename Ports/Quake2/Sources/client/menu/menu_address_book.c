#include "client/menu/menu.h"

#define NUM_ADDRESSBOOK_ENTRIES 9

static menuframework_s s_addressbook_menu;
static menufield_s s_addressbook_fields[NUM_ADDRESSBOOK_ENTRIES];

static void AddressBook_MenuInit()
{
	int i;
	float scale = SCR_GetMenuScale();

	menuframework_s *menu = &s_addressbook_menu;
	memset(menu, 0, sizeof(menuframework_s));
	s_addressbook_menu.x = viddef.width / 2 - (142 * scale);
	s_addressbook_menu.y = viddef.height / (2 * scale) - 58;
	s_addressbook_menu.nitems = 0;

	for (i = 0; i < NUM_ADDRESSBOOK_ENTRIES; i++)
	{
		cvar_t *adr;
		char buffer[20];

		Com_sprintf(buffer, sizeof(buffer), "adr%d", i);

		adr = Cvar_Get(buffer, "", CVAR_ARCHIVE);

		s_addressbook_fields[i].generic.type = MTYPE_FIELD;
		s_addressbook_fields[i].generic.name = 0;
		s_addressbook_fields[i].generic.callback = 0;
		s_addressbook_fields[i].generic.x = 0;
		s_addressbook_fields[i].generic.y = i * 18 + 0;
		s_addressbook_fields[i].generic.localdata[0] = i;
		s_addressbook_fields[i].cursor = 0;
		s_addressbook_fields[i].length = 60;
		s_addressbook_fields[i].visible_length = 30;

		strcpy(s_addressbook_fields[i].buffer, adr->string);

		Menu_AddItem(&s_addressbook_menu, &s_addressbook_fields[i]);
	}
}

static const char* AddressBook_MenuKey(int key)
{
	if (key == K_GAMEPAD_SELECT || key == K_ESCAPE)
	{
		int index;
		char buffer[20];

		for (index = 0; index < NUM_ADDRESSBOOK_ENTRIES; index++)
		{
			Com_sprintf(buffer, sizeof(buffer), "adr%d", index);
			Cvar_Set(buffer, s_addressbook_fields[index].buffer);
		}
	}

	return Default_MenuKey(&s_addressbook_menu, key);
}

static void AddressBook_MenuDraw()
{
	M_Banner("m_banner_addressbook");
	Menu_Draw(&s_addressbook_menu);
}

void MenuAddressBook_enter()
{
	AddressBook_MenuInit();
	M_PushMenu(AddressBook_MenuDraw, AddressBook_MenuKey);
}
