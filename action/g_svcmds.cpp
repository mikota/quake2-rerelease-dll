// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"

extern int dosoft;
extern int softquit;

void Svcmd_Test_f()
{
	gi.LocClient_Print(nullptr, PRINT_HIGH, "Svcmd_Test_f()\n");
}

void SVCmd_ReloadMOTD_f ()
{
	ReadMOTDFile ();
	gi.LocClient_Print(nullptr, PRINT_HIGH, "MOTD reloaded.\n");
}

/*
==============================================================================

PACKET FILTERING


You can add or remove addresses from the filter list with:

addip <ip>
removeip <ip>

The ip address is specified in dot format, and any unspecified digits will match any value, so you can specify an entire
class C network with "addip 192.246.40".

Removeip will only remove an address specified exactly the same way.  You cannot addip a subnet, then removeip a single
host.

listip
Prints the current list of filters.

writeip
Dumps "addip <ip>" commands to listip.cfg so it can be execed at a later date.  The filter lists are not saved and
restored by default, because I beleive it would cause too much confusion.

filterban <0 or 1>

If 1 (the default), then ip addresses matching the current list will be prohibited from entering the game.  This is the
default setting.

If 0, then only addresses matching the list will be allowed.  This lets you easily set up a private game, or a game that
only allows players from your local network.


==============================================================================
*/

struct ipfilter_t
{
	unsigned mask;
	unsigned compare;
};

constexpr size_t MAX_IPFILTERS = 1024;

ipfilter_t ipfilters[MAX_IPFILTERS];
int		   numipfilters;

/*
=================
StringToFilter
=================
*/
static bool StringToFilter(const char *s, ipfilter_t *f)
{
	char num[128];
	int	 i, j;
	byte b[4];
	byte m[4];

	for (i = 0; i < 4; i++)
	{
		b[i] = 0;
		m[i] = 0;
	}

	for (i = 0; i < 4; i++)
	{
		if (*s < '0' || *s > '9')
		{
			gi.LocClient_Print(nullptr, PRINT_HIGH, "Bad filter address: {}\n", s);
			return false;
		}

		j = 0;
		while (*s >= '0' && *s <= '9')
		{
			num[j++] = *s++;
		}
		num[j] = 0;
		b[i] = atoi(num);
		if (b[i] != 0)
			m[i] = 255;

		if (!*s)
			break;
		s++;
	}

	f->mask = *(unsigned *) m;
	f->compare = *(unsigned *) b;

	return true;
}

/*
=================
SV_FilterPacket
=================
*/
bool SV_FilterPacket(const char *from)
{
	int		 i;
	unsigned in;
	byte	 m[4];
	const char	 *p;

	i = 0;
	p = from;
	while (*p && i < 4)
	{
		m[i] = 0;
		while (*p >= '0' && *p <= '9')
		{
			m[i] = m[i] * 10 + (*p - '0');
			p++;
		}
		if (!*p || *p == ':')
			break;
		i++;
		p++;
	}

	in = *(unsigned *) m;

	for (i = 0; i < numipfilters; i++)
		if ((in & ipfilters[i].mask) == ipfilters[i].compare)
			return filterban->integer;

	return !filterban->integer;
}

/*
=================
SV_AddIP_f
=================
*/
void SVCmd_AddIP_f()
{
	int i;

	if (gi.argc() < 3)
	{
		gi.LocClient_Print(nullptr, PRINT_HIGH, "Usage:  addip <ip-mask>\n");
		return;
	}

	for (i = 0; i < numipfilters; i++)
		if (ipfilters[i].compare == 0xffffffff)
			break; // free spot
	if (i == numipfilters)
	{
		if (numipfilters == MAX_IPFILTERS)
		{
			gi.LocClient_Print(nullptr, PRINT_HIGH, "IP filter list is full\n");
			return;
		}
		numipfilters++;
	}

	if (!StringToFilter(gi.argv(2), &ipfilters[i]))
		ipfilters[i].compare = 0xffffffff;
}

/*
=================
SV_RemoveIP_f
=================
*/
void SVCmd_RemoveIP_f()
{
	ipfilter_t f;
	int		   i, j;

	if (gi.argc() < 3)
	{
		gi.LocClient_Print(nullptr, PRINT_HIGH, "Usage:  sv removeip <ip-mask>\n");
		return;
	}

	if (!StringToFilter(gi.argv(2), &f))
		return;

	for (i = 0; i < numipfilters; i++)
		if (ipfilters[i].mask == f.mask && ipfilters[i].compare == f.compare)
		{
			for (j = i + 1; j < numipfilters; j++)
				ipfilters[j - 1] = ipfilters[j];
			numipfilters--;
			gi.LocClient_Print(nullptr, PRINT_HIGH, "Removed.\n");
			return;
		}
	gi.LocClient_Print(nullptr, PRINT_HIGH, "Didn't find {}.\n", gi.argv(2));
}

/*
=================
SV_ListIP_f
=================
*/
void SVCmd_ListIP_f()
{
	int	 i;
	byte b[4];

	gi.LocClient_Print(nullptr, PRINT_HIGH, "Filter list:\n");
	for (i = 0; i < numipfilters; i++)
	{
		*(unsigned *) b = ipfilters[i].compare;
		gi.LocClient_Print(nullptr, PRINT_HIGH, "{}.{}.{}.{}\n", b[0], b[1], b[2], b[3]);
	}
}

// [Paril-KEX]
void SVCmd_NextMap_f()
{
	gi.LocBroadcast_Print(PRINT_HIGH, "$g_map_ended_by_server");
	EndDMLevel();
}

/*
=================
SV_WriteIP_f
=================
*/
void SVCmd_WriteIP_f(void)
{
	// KEX_FIXME: Sys_FOpen isn't available atm, just commenting this out since i don't think we even need this functionality - sponge
	/*
	FILE* f;

	byte	b[4];
	int		i;
	cvar_t* game;

	game = gi.cvar("game", "", 0);

	std::string name;
	if (!*game->string)
		name = std::string(GAMEVERSION) + "/listip.cfg";
	else
		name = std::string(game->string) + "/listip.cfg";

	gi.LocClient_Print(nullptr, PRINT_HIGH, "Writing {}.\n", name.c_str());

	f = Sys_FOpen(name.c_str(), "wb");
	if (!f)
	{
		gi.LocClient_Print(nullptr, PRINT_HIGH, "Couldn't open {}\n", name.c_str());
		return;
	}

	fprintf(f, "set filterban %d\n", filterban->integer);

	for (i = 0; i < numipfilters; i++)
	{
		*(unsigned*)b = ipfilters[i].compare;
		fprintf(f, "sv addip %i.%i.%i.%i\n", b[0], b[1], b[2], b[3]);
	}

	fclose(f);
	*/
}

// Action add

//rekkie -- silence ban -- s
/*================================================================================================================================================================

SILENCE BAN FILTERING - Indefinitely or temporarly silence a player; they can see their own messages, but other players will not see them.

The ip address is specified in dot format, and any unspecified digits will match any value, so you can specify an entire class C network with "addip 192.246.40"


sv addsb <ip> <OPTIONAL: number of games>
-----------------------------------------
Add address, including subnet, to the silence ban list. Adding 192.168 to the list would block out everyone in the 192.168.*.* net block.
OPTIONAL:	If number of games is specified, the address will be silenced for the specified number of games.
			If not specified the address will be silenced indefinitely.


sv removesb <ip>
----------------
Remove address from the silence ban list. Removing <IP> address must be done in the way it was added, you cannot addip a subnet then removesb a single host ip.


sv listsb
---------
Prints the current list of filters.


sv writesb
----------
Writes bans to listsb.cfg, which can then be run by adding +exec listsb.cfg to the server's command line.
WARNING:	Bans are not saved or loaded by default. Admins must run sv writesb to update listsb.cfg with the changes.


silenceban <0 or 1> Default: 1
------------------------------
silenceban 1		This acts as a BLACKLIST, meaning IPs listed on (sv listsb) will be DENIED from talking to other players in game.
silenceban 0		This acts as a WHITELIST, meaning IPs listed on (sv listsb) will be ALLOWED to talk to other players in game.

==================================================================================================================================================================*/

#define MAX_SB_FILTERS   1024
typedef struct
{
	unsigned mask;
	unsigned compare;
	int temp_sban_games;
}
sb_filter_t;
sb_filter_t sb_filters[MAX_SB_FILTERS];
int num_sb_filters;

//===========================
//== StringToSilenceFilter ==
//===========================
static bool StringToSilenceFilter(char* s, sb_filter_t* f, int temp_sban_games)
{
	char num[128];
	int i, j;
	byte b[4] = { 0,0,0,0 };
	byte m[4] = { 0,0,0,0 };

	for (i = 0; i < 4; i++)
	{
		if (*s < '0' || *s > '9')
		{
			gi.LocClient_Print(NULL, PRINT_HIGH, "Bad silence filter address: %s\n", s);
			return false;
		}

		j = 0;

		while (*s >= '0' && *s <= '9')
			num[j++] = *s++;

		num[j] = 0;
		b[i] = atoi(num);

		if (b[i] != 0)
			m[i] = 255;

		if (!*s)
			break;
		s++;
	}

	f->mask = *(unsigned*)m;
	f->compare = *(unsigned*)b;
	f->temp_sban_games = temp_sban_games;

	return true;
}

//=======================
//== SV_FilterSBPacket ==
//=======================
bool SV_FilterSBPacket(char* from, int* temp) // temp is optional
{
	int i = 0;
	unsigned in;
	byte m[4] = { 0,0,0,0 };
	char* p;

	p = from;
	while (*p && i < 4)
	{
		while (*p >= '0' && *p <= '9')
		{
			m[i] = m[i] * 10 + (*p - '0');
			p++;
		}
		if (!*p || *p == ':')
			break;
		i++, p++;
	}

	in = *(unsigned*)m;

	for (i = 0; i < num_sb_filters; i++)
	{
		if ((in & sb_filters[i].mask) == sb_filters[i].compare)
		{
			if (temp != NULL)
				*temp = sb_filters[i].temp_sban_games;
			return (int)silenceban->value;
		}
	}

	return (int)!silenceban->value;
}

//================
//== SB_Have_IP ==
//================
bool SB_Have_IP(char* from) // Check if IP is in the silence ban list
{
	int i = 0;
	unsigned in;
	byte m[4] = { 0,0,0,0 };
	char* p;

	p = from;
	while (*p && i < 4)
	{
		while (*p >= '0' && *p <= '9')
		{
			m[i] = m[i] * 10 + (*p - '0');
			p++;
		}
		if (!*p || *p == ':')
			break;
		i++, p++;
	}

	in = *(unsigned*)m;

	for (i = 0; i < num_sb_filters; i++)
	{
		if ((in & sb_filters[i].mask) == sb_filters[i].compare)
		{
			return 1;
		}
	}

	return 0;
}

//===================
//== SVCmd_AddSB_f ==
//===================
void SVCmd_AddSB_f(void)
{
	int i;

	if (gi.argc() < 3)
	{
		gi.LocClient_Print(NULL, PRINT_HIGH, "Usage:  sv addsb <ip-mask> <OPTIONAL: num games>\n");
		return;
	}

	if (SB_Have_IP(gi.argv(2))) // Check if IP was already added
	{
		gi.LocClient_Print(NULL, PRINT_HIGH, "This IP %s is already on the list\n", gi.argv(2));
		return;
	}

	for (i = 0; i < num_sb_filters; i++)
	{
		if (sb_filters[i].compare == 0xffffffff)
			break;			// free spot
	}

	if (i == num_sb_filters)
	{
		if (num_sb_filters == MAX_SB_FILTERS)
		{
			gi.LocClient_Print(NULL, PRINT_HIGH, "Silence filter list is full\n");
			return;
		}
		num_sb_filters++;
	}

	if (gi.argc() == 4) // Admin specified number of games for a temporary ban
	{
		if (!StringToSilenceFilter(gi.argv(2), &sb_filters[i], atoi(gi.argv(3))))
			sb_filters[i].compare = 0xffffffff;

		gi.LocClient_Print(NULL, PRINT_HIGH, "Added silence to IP/MASK: %s for %s games\n", gi.argv(2), gi.argv(3));
	}
	else // Admin did not specify number of games, therefore the ban is indefinite
	{
		if (!StringToSilenceFilter(gi.argv(2), &sb_filters[i], 0))
			sb_filters[i].compare = 0xffffffff;

		gi.LocClient_Print(NULL, PRINT_HIGH, "Added silence to IP/MASK: %s\n", gi.argv(2));
	}

	// Check which client(s) need a silence applied
	for (i = 0; i < game.maxclients; i++)
	{
		if (game.clients[i].pers.connected == false)
			continue;
		if (game.clients[i].pers.silence_banned)
			continue;
		if (SV_FilterSBPacket(game.clients[i].pers.ip, NULL))
		{
			game.clients[i].pers.silence_banned = true;
			gi.LocClient_Print(NULL, PRINT_HIGH, "Adding silence to player: %s\n", game.clients[i].pers.netname);
		}
	}
}

//====================
// SVCmd_RemoveSB_f ==
//====================
void SVCmd_RemoveSB_f(void)
{
	sb_filter_t f;
	int i, j;

	if (gi.argc() < 3)
	{
		gi.LocClient_Print(NULL, PRINT_HIGH, "Usage:  sv removesb <ip-mask>\n");
		return;
	}

	if (!StringToSilenceFilter(gi.argv(2), &f, 0))
		return;

	for (i = 0; i < num_sb_filters; i++)
	{
		if (sb_filters[i].mask == f.mask && sb_filters[i].compare == f.compare)
		{
			for (j = i + 1; j < num_sb_filters; j++)
				sb_filters[j - 1] = sb_filters[j];

			num_sb_filters--;
			gi.LocClient_Print(NULL, PRINT_HIGH, "Removed silence from IP/MASK: %s\n", gi.argv(2));

			// Check which client(s) need to their silenced removed
			for (i = 0; i < game.maxclients; i++)
			{
				if (game.clients[i].pers.connected == false)
					continue;
				if (game.clients[i].pers.silence_banned == false)
					continue;
				if (SV_FilterSBPacket(game.clients[i].pers.ip, NULL) == false)
				{
					game.clients[i].pers.silence_banned = false;
					gi.LocClient_Print(NULL, PRINT_HIGH, "Removing silence from player: %s\n", game.clients[i].pers.netname);
				}
			}

			return;
		}
	}
	gi.LocClient_Print(NULL, PRINT_HIGH, "Cannot find IP/MASK: %s\n", gi.argv(2));
}

//===================
// SVCmd_CheckSB_f ==
//===================
void SVCmd_CheckSB_f(void) // Check for temporary silences that need removing
{
	// We don't directly unban them all - we subtract 1 from temp_ban_games, and unban them if it's 0.
	int i, j;
	for (i = 0; i < num_sb_filters; i++)
	{
		if (sb_filters[i].temp_sban_games > 0)
		{
			if (!--sb_filters[i].temp_sban_games)
			{
				// Re-pack the filters
				for (j = i + 1; j < num_sb_filters; j++)
					sb_filters[j - 1] = sb_filters[j];
				num_sb_filters--;
				gi.LocClient_Print(NULL, PRINT_HIGH, "Removed silence\n");

				// Since we removed the current we have to re-process the new current
				i--;
			}
		}
	}

	// Check which client(s) need to their silenced removed
	int temp_sban_games = 0;
	for (i = 0; i < game.maxclients; i++)
	{
		if (game.clients[i].pers.connected == false)
			continue;
		if (game.clients[i].pers.silence_banned == false)
			continue;
		if (SV_FilterSBPacket(game.clients[i].pers.ip, &temp_sban_games) == false && temp_sban_games == 0)
		{
			game.clients[i].pers.silence_banned = false;
			gi.LocClient_Print(NULL, PRINT_HIGH, "Removing silence from player: %s\n", game.clients[i].pers.netname);
		}
	}
}

//====================
//== SVCmd_ListSB_f ==
//====================
void SVCmd_ListSB_f(void)
{
	int i;
	byte b[4];

	gi.LocClient_Print(NULL, PRINT_HIGH, "Silence filter list:\n");
	for (i = 0; i < num_sb_filters; i++)
	{
		*(unsigned*)b = sb_filters[i].compare;
		if (!sb_filters[i].temp_sban_games)
			gi.LocClient_Print(NULL, PRINT_HIGH, "%3i.%3i.%3i.%3i\n", b[0], b[1], b[2], b[3]);
		else
			gi.LocClient_Print(NULL, PRINT_HIGH, "%3i.%3i.%3i.%3i (%d more game(s))\n", b[0], b[1], b[2], b[3], sb_filters[i].temp_sban_games);
	}
}

//=====================
//== SVCmd_WriteSB_f ==
//=====================
void SVCmd_WriteSB_f(void)
{
	FILE* f;
	char name[MAX_OSPATH];
	byte b[4];
	int i;
	cvar_t* game;

	game = gi.cvar("game", "", CVAR_NOFLAGS);

	if (!*game->string)
		sprintf(name, "%s/listsb.cfg", GAMEVERSION);
	else
		sprintf(name, "%s/listsb.cfg", game->string);

	gi.LocClient_Print(NULL, PRINT_HIGH, "Writing %s\n", name);

	f = fopen(name, "wb");
	if (!f)
	{
		gi.LocClient_Print(NULL, PRINT_HIGH, "Couldn't open %s\n", name);
		return;
	}

	fprintf(f, "set silenceban %d\n", (int)silenceban->value);

	for (i = 0; i < num_sb_filters; i++)
	{
		if (!sb_filters[i].temp_sban_games)
		{
			*(unsigned*)b = sb_filters[i].compare;
			fprintf(f, "sv addsb %i.%i.%i.%i\n", b[0], b[1], b[2], b[3]);
		}
	}

	fclose(f);
}
//rekkie -- silence ban -- e

/*
=================
SV_Nextmap_f
=================
*/
void SVCmd_Nextmap_f (char *arg)
{
	// end level and go to next map in map rotation
	gi.LocBroadcast_Print (PRINT_HIGH, "Changing to next map in rotation.\n");
	EndDMLevel ();
	if (arg != NULL && Q_stricmp (arg, "force") == 0)
		ExitLevel ();
	return;
}

//Black Cross - End

/*
=================
STUFF COMMAND

This will stuff a certain command to the client.
=================
*/
void SVCmd_stuffcmd_f ()
{
	int i, u, team = -1, stuffAll = 0;
	char text[256];
	char user[64], tmp[64];
	edict_t *ent;

	if (gi.argc() < 4) {
		gi.LocClient_Print (NULL, PRINT_HIGH, "Usage: stuffcmd <user id> <text>\n");
		return;
	}

	i = gi.argc ();
	Q_strncpyz (user, gi.argv (2), sizeof (user));
	text[0] = 0;

	for (u = 3; u <= i; u++)
	{
		Q_strncpyz (tmp, gi.argv (u), sizeof (tmp));
		if (tmp[0] == '!')	// Checks for "!" and replaces for "$" to see the user info
			tmp[0] = '$';

		if(text[0])
			Q_strncatz (text, " ", sizeof(text)-1);

		Q_strncatz (text, tmp, sizeof(text)-1);
	}
	Q_strncatz (text, "\n", sizeof(text));

	if (!Q_stricmp(user, "all")) {
		stuffAll = 1;
	} else if (!Q_strnicmp(user, "team", 4)) {
		team = TP_GetTeamFromArg(user+4);
	}

	if(team > -1 && !teamplay->value) {
		gi.LocClient_Print(NULL, PRINT_HIGH, "Not in Teamplay mode\n");
		return;
	}

	if (stuffAll || team > -1)
	{
		for (i = 1; i <= game.maxclients; i++)
		{
			ent = getEnt (i);
			if(!ent->inuse)
				continue;

			if (stuffAll || ent->client->resp.team == team) {
				gi.LocClient_Print(ent, PRINT_HIGH, "Console stuffed: %s", text);
				stuffcmd(ent, text);
			}
		}
		return;
	}

	u = strlen(user);
	for (i = 0; i < u; i++)
	{
		if (!isdigit(user[i]))
		{
			gi.LocClient_Print (NULL, PRINT_HIGH, "Usage: stuffcmd <user id> <text>\n");
			return;
		}
	}

	i = atoi(user) + 1;
	if (i > game.maxclients)
	{				/* if is inserted number > server capacity */
		gi.LocClient_Print (NULL, PRINT_HIGH, "User id is not valid\n");
		return;
	}

	ent = getEnt(i);
	if (ent->inuse) { /* if is inserted a user that exists in the server */
		gi.LocClient_Print(ent, PRINT_HIGH, "Console stuffed: %s", text);
		stuffcmd (ent, text);
	}
	else
		gi.LocClient_Print (NULL, PRINT_HIGH, "User id is not valid\n");
}

/*
=================
SV_Softmap_f
=================
*/
void SVCmd_Softmap_f (void)
{
	if (gi.argc() < 3) {
		gi.LocClient_Print(NULL, PRINT_HIGH, "Usage:  sv softmap <map>\n");
		return;
	}
	if (lights_camera_action) {
		gi.LocClient_Print(NULL, PRINT_HIGH, "Please dont use sv softmap during LCA\n");
		return;
	}

	Q_strncpyz(level.nextmap, gi.argv(2), sizeof(level.nextmap));
	gi.LocBroadcast_Print(PRINT_HIGH, "Console is setting map: %s\n", level.nextmap);
	dosoft = 1;
	EndDMLevel();
	return;
}

/*
=================
SV_Map_restart_f
=================
*/
void SVCmd_Map_restart_f (void)
{
	gi.LocBroadcast_Print(PRINT_HIGH, "Console is restarting map\n");
	dosoft = 1;
	strcpy(level.nextmap, level.mapname);
	EndDMLevel();
	return;
}

void SVCmd_ResetScores_f (void)
{
	ResetScores(true);
	gi.LocBroadcast_Print(PRINT_HIGH, "Scores and time were reset by console.\n");
}

void SVCmd_SetTeamScore_f( int team )
{
	if( ! teamplay->value )
	{
		gi.LocClient_Print( NULL, PRINT_HIGH, "Scores can only be set for teamplay.\n" );
		return;
	}

	if( gi.argc() < 3 )
	{
		gi.LocClient_Print( NULL, PRINT_HIGH, "Usage: sv %s <score>\n", gi.argv(1) );
		return;
	}

	// No need to convert to int
	//teams[team].score = atoi(gi.argv(2));
	
	gi.cvar_forceset( teams[team].teamscore->name, gi.argv(2) );

	gi.LocBroadcast_Print( PRINT_HIGH, "Team %i score set to %i by console.\n", team, teams[team].score );
}

void SVCmd_SetTeamName_f( int team )
{
	if( ! teamplay->value )
	{
		gi.LocClient_Print( NULL, PRINT_HIGH, "Teamnames can only be set for teamplay.\n" );
		return;
	}

	if( gi.argc() < 3 )
	{
		gi.LocClient_Print( NULL, PRINT_HIGH, "Usage: sv %s <name>\n", gi.argv(1) );
		return;
	}

	strcpy(teams[team].name, gi.argv(2));

	gi.LocBroadcast_Print( PRINT_HIGH, "Team %i name set to %s by console.\n", team, teams[team].name );
}

void SVCmd_SetTeamSkin_f( int team )
{
	if( ! teamplay->value )
	{
		gi.LocClient_Print( NULL, PRINT_HIGH, "Team skins can only be set for teamplay.\n" );
		return;
	}

	if( gi.argc() < 3 )
	{
		gi.LocClient_Print( NULL, PRINT_HIGH, "Usage: sv %s <name>\n", gi.argv(1) );
		return;
	}

	strcpy(teams[team].skin, gi.argv(2));

	gi.LocBroadcast_Print( PRINT_HIGH, "Team %i skin set to %s by console and will be reflected next round.\n", team, teams[team].skin );
}

void SVCmd_SetTeamSkin_Index_f( int team )
{
	if( ! teamplay->value )
	{
		gi.LocClient_Print( NULL, PRINT_HIGH, "Team skin indexes can only be set for teamplay.\n" );
		return;
	}

	if( gi.argc() < 3 )
	{
		gi.LocClient_Print( NULL, PRINT_HIGH, "Usage: sv %s <name>\n", gi.argv(1) );
		return;
	}

	strcpy(teams[team].skin_index, gi.argv(2));

	gi.LocBroadcast_Print( PRINT_HIGH, "Team %i skin index set to %s by console, requires a new map or server restart.\n", team, teams[team].skin_index );
}

void SVCmd_SoftQuit_f (void)
{
	gi.LocBroadcast_Print(PRINT_HIGH, "The server will exit after this map\n");
	softquit = 1;
}

void SVCmd_Slap_f (void)
{
	const char *name = gi.argv(2);
	size_t name_len = strlen(name);
	int damage = atoi(gi.argv(3));
	float power = (gi.argc() >= 5) ? atof(gi.argv(4)) : 100.f;
	vec3_t slap_dir = {0.f,0.f,1.f}, slap_normal = {0.f,0.f,-1.f};
	bool found_victim = false;
	size_t i = 0;
	int user_id = name_len ? (atoi(name) + 1) : 0;

	if( gi.argc() < 3 )
	{
		gi.LocClient_Print( NULL, PRINT_HIGH, "Usage: sv slap <name/id> [<damage>] [<power>]\n" );
		return;
	}
	if( lights_camera_action )
	{
		gi.LocClient_Print( NULL, PRINT_HIGH, "Can't slap yet!\n" );
		return;
	}

	// See if we're slapping by user ID.
	for( i = 0; i < name_len; i ++ )
	{
		if( ! isdigit(name[i]) )
			user_id = 0;
	}

	for( i = 1; i <= game.maxclients; i ++ )
	{
		edict_t *ent = g_edicts + i;
		if( ent->inuse && ( (user_id == i) || ((! user_id) && (Q_strnicmp( ent->client->pers.netname, name, name_len ) == 0)) ) )
		{
			found_victim = true;
			if( IS_ALIVE(ent) )
			{
				slap_dir[ 0 ] = crandom() * 0.5f;
				slap_dir[ 1 ] = crandom() * 0.5f;
				T_Damage( ent, world, world, slap_dir, ent->s.origin, slap_normal, damage, power, 0, MOD_KICK );
				gi.sound( ent, CHAN_WEAPON, gi.soundindex("weapons/kick.wav"), 1, ATTN_NORM, 0 );
				gi.LocBroadcast_Print( PRINT_HIGH, "Admin slapped %s for %i damage.\n", ent->client->pers.netname, damage );
			}
			else
				gi.LocClient_Print( NULL, PRINT_HIGH, "%s is already dead.\n", ent->client->pers.netname );
		}
	}

	if( ! found_victim )
		gi.LocClient_Print( NULL, PRINT_HIGH, "Couldn't find %s to slap.\n", name );
}

bool ScrambleTeams(void);

void SVCmd_Scramble_f(void)
{
	if (!teamplay->value) {
		gi.LocClient_Print(NULL, PRINT_HIGH, "Scramble is for teamplay only\n");
		return;
	}

	if (!ScrambleTeams()) {
		gi.LocClient_Print(NULL, PRINT_HIGH, "Need more players to scramble\n");
	}
}

// End Action add

/*
=================
ServerCommand

ServerCommand will be called when an "sv" command is issued.
The game can issue gi.argc() / gi.argv() commands to get the rest
of the parameters
=================
*/
void ServerCommand()
{
	const char *cmd;

	cmd = gi.argv(1);
	if (Q_strcasecmp(cmd, "test") == 0)
		Svcmd_Test_f();
	else if (Q_strcasecmp(cmd, "addip") == 0)
		SVCmd_AddIP_f();
	else if (Q_strcasecmp(cmd, "removeip") == 0)
		SVCmd_RemoveIP_f();
	else if (Q_strcasecmp(cmd, "listip") == 0)
		SVCmd_ListIP_f();
	else if (Q_strcasecmp(cmd, "writeip") == 0)
		SVCmd_WriteIP_f();
	else if (Q_strcasecmp(cmd, "nextmap") == 0)
		SVCmd_NextMap_f();
	else
		gi.LocClient_Print(nullptr, PRINT_HIGH, "Unknown server command \"{}\"\n", cmd);
}
