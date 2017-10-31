#include "client/menu/menu.h"

#define MAX_LOCAL_SERVERS 8

static menuframework_s s_joinserver_menu;
static menuseparator_s s_joinserver_server_title;
static menuaction_s s_joinserver_search_action;
static menuaction_s s_joinserver_address_book_action;
static menuaction_s s_joinserver_server_actions[MAX_LOCAL_SERVERS];

static int m_num_servers;
#define NO_SERVER_STRING "<no server>"

/* network address */
static netadr_t local_server_netadr[MAX_LOCAL_SERVERS];

/* user readable information */
static char local_server_names[MAX_LOCAL_SERVERS][80];
static char local_server_netadr_strings[MAX_LOCAL_SERVERS][80];

void M_AddToServerList(netadr_t adr, char *info)
{
	char *s;
	int i;

	if (m_num_servers == MAX_LOCAL_SERVERS)
	{
		return;
	}

	while (*info == ' ')
	{
		info++;
	}

	s = NET_AdrToString(adr);

	/* ignore if duplicated */
	for (i = 0; i < m_num_servers; i++)
	{
		if (!strcmp(local_server_names[i], info) &&
		    !strcmp(local_server_netadr_strings[i], s))
		{
			return;
		}
	}

	local_server_netadr[m_num_servers] = adr;
	Q_strlcpy(local_server_names[m_num_servers], info,
		sizeof(local_server_names[m_num_servers]));
	Q_strlcpy(local_server_netadr_strings[m_num_servers], s,
		sizeof(local_server_netadr_strings[m_num_servers]));
	m_num_servers++;
}

static void JoinServerFunc(void *self)
{
	char buffer[128];
	int index;

	index = (int)((menuaction_s *)self - s_joinserver_server_actions);

	if (Q_stricmp(local_server_names[index], NO_SERVER_STRING) == 0)
	{
		return;
	}

	if (index >= m_num_servers)
	{
		return;
	}

	Com_sprintf(buffer, sizeof(buffer), "connect %s\n",
		NET_AdrToString(local_server_netadr[index]));
	Cbuf_AddText(buffer);
	M_ForceMenuOff();
}

static void AddressBookFunc(void *self)
{
	MenuAddressBook_enter();
}

static void SearchLocalGames(void)
{
	int i;

	m_num_servers = 0;

	for (i = 0; i < MAX_LOCAL_SERVERS; i++)
	{
		strcpy(local_server_names[i], NO_SERVER_STRING);
		local_server_netadr_strings[i][0] = '\0';
	}

	m_popup_string = "Searching for local servers. This\n"
	        "could take up to a minute, so\n"
	        "please be patient.";
	m_popup_endtime = cls.realtime + 2000;
	M_Popup();

	/* the text box won't show up unless we do a buffer swap */
	R_Frame_end();

	/* send out info packets */
	CL_PingServers_f();
}

static void SearchLocalGamesFunc(void *self)
{
	SearchLocalGames();
}

static void JoinServer_MenuInit(void)
{
	int i;
	float scale = SCR_GetMenuScale();

	menuframework_s *menu = &s_joinserver_menu;
	memset(menu, 0, sizeof(menuframework_s));
	s_joinserver_menu.x = (int)(viddef.width * 0.50f) - 120 * scale;
	s_joinserver_menu.nitems = 0;

	s_joinserver_address_book_action.generic.type = MTYPE_ACTION;
	s_joinserver_address_book_action.generic.name = "address book";
	s_joinserver_address_book_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_joinserver_address_book_action.generic.x = 0;
	s_joinserver_address_book_action.generic.y = 0;
	s_joinserver_address_book_action.generic.callback = AddressBookFunc;

	s_joinserver_search_action.generic.type = MTYPE_ACTION;
	s_joinserver_search_action.generic.name = "refresh server list";
	s_joinserver_search_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_joinserver_search_action.generic.x = 0;
	s_joinserver_search_action.generic.y = 10;
	s_joinserver_search_action.generic.callback = SearchLocalGamesFunc;
	s_joinserver_search_action.generic.statusbar = "search for servers";

	s_joinserver_server_title.generic.type = MTYPE_SEPARATOR;
	s_joinserver_server_title.generic.name = "connect to...";
	s_joinserver_server_title.generic.x = 80 * scale;
	s_joinserver_server_title.generic.y = 30;

	for (i = 0; i < MAX_LOCAL_SERVERS; i++)
	{
		s_joinserver_server_actions[i].generic.type = MTYPE_ACTION;
		s_joinserver_server_actions[i].generic.name = local_server_names[i];
		s_joinserver_server_actions[i].generic.flags = QMF_LEFT_JUSTIFY;
		s_joinserver_server_actions[i].generic.x = 0;
		s_joinserver_server_actions[i].generic.y = 40 + i * 10;
		s_joinserver_server_actions[i].generic.callback = JoinServerFunc;
		s_joinserver_server_actions[i].generic.statusbar =
		        local_server_netadr_strings[i];
	}

	Menu_AddItem(&s_joinserver_menu, &s_joinserver_address_book_action);
	Menu_AddItem(&s_joinserver_menu, &s_joinserver_server_title);
	Menu_AddItem(&s_joinserver_menu, &s_joinserver_search_action);

	for (i = 0; i < 8; i++)
	{
		Menu_AddItem(&s_joinserver_menu, &s_joinserver_server_actions[i]);
	}

	Menu_Center(&s_joinserver_menu);

	SearchLocalGames();
}

static void JoinServer_MenuDraw()
{
	M_Banner("m_banner_join_server");
	Menu_Draw(&s_joinserver_menu);
	M_Popup();
}

static const char* JoinServer_MenuKey(int key, int keyUnmodified)
{
	if (m_popup_string)
	{
		m_popup_string = NULL;
		return NULL;
	}
	return Default_MenuKey(&s_joinserver_menu, key, keyUnmodified);
}

void MenuJoinServer_enter()
{
	JoinServer_MenuInit();
	M_PushMenu(JoinServer_MenuDraw, JoinServer_MenuKey);
}
