#include "g_local.h"
#include "m_player.h"

//AQ2:TNG - Slicer Old Location support
//loccube_t *setcube = NULL;
//AQ2:TNG End

//
void _Cmd_Rules_f (edict_t * self, char *argument)
{
	char section[32], mbuf[1024], *p, buf[30][INI_STR_LEN];
	int i, j = 0;
	ini_t ini;

	strcpy (mbuf, "\n");
	if (*argument)
		Q_strncpyz(section, argument, sizeof(section));
	else
		strcpy (section, "main");

	if (OpenIniFile (GAMEVERSION "/prules.ini", &ini))
	{
		i = ReadIniSection (&ini, section, buf, 30);
		while (j < i)
		{
			p = buf[j++];
			if (*p == '.')
				p++;
			Q_strncatz(mbuf, p, sizeof(mbuf));
			Q_strncatz(mbuf, "\n", sizeof(mbuf));
		}
		CloseIniFile (&ini);
	}
	if (!j)
		gi.LocClient_Print (self, PRINT_MEDIUM, "No rules on %s available\n", section);
	else
		gi.LocClient_Print (self, PRINT_MEDIUM, "%s", mbuf);
}

void Cmd_Rules_f (edict_t * self)
{
	char *s;

	s = gi.args ();
	_Cmd_Rules_f (self, s);
}

//
void Cmd_Menu_f (edict_t * self)
{
	char *s;

	s = gi.args ();
	vShowMenu (self, s);
}

//
void Cmd_Punch_f (edict_t * self)
{
	float punch_delay = 0.5f;

	if (!use_punch->value || !IS_ALIVE(self) || self->client->resp.sniper_mode != SNIPER_1X)
		return;

	if (self->client->weaponstate != WEAPON_READY && self->client->weaponstate != WEAPON_END_MAG)
		return;

	// animation moved to punch_attack() in a_xgame.c
	// punch_attack is now called in ClientThink after evaluation punch_desired
	// for "no punch when firing" stuff - TempFile
	if (level.time.milliseconds() > (self->client->punch_framenum + punch_delay)) {
		self->client->punch_framenum = level.time.milliseconds();	// you aren't Bruce Lee! :)
		self->client->punch_desired = true;
	}
}

//Plays a sound file
void Cmd_Voice_f (edict_t * self)
{
	char const *s;
	char fullpath[MAX_QPATH];

	if (!use_voice->value)
		return;

	s = gi.args();
	//check if no sound is given
	if (!*s)
	{
		gi.LocClient_Print (self, PRINT_MEDIUM,
			"\nCommand needs argument, use voice <soundfile.wav>.\n");
		return;
	}
	if (strlen (s) > 32)
	{
		gi.LocClient_Print (self, PRINT_MEDIUM,
			"\nArgument is too long. Maximum length is 32 characters.\n");
		return;
	}
	// AQ2:TNG Disabled this message: why? -M
	if (strstr (s, ".."))
	{
		gi.LocClient_Print (self, PRINT_MEDIUM,
			"\nArgument must not contain \"..\".\n");
		return;
	}
	
	//check if player is dead
	if (!IS_ALIVE(self))
		return;

	strcpy(fullpath, PG_SNDPATH);
	strcat(fullpath, s);
	// SLIC2 Taking this out.
	/*if (radio_repeat->value)
	{
	if ((d = CheckForRepeat (self, s)) == false)
	return;
	}*/
	if (radio_max->value)
	{
		if (CheckForFlood (self)== false)
			return;
	}
	// AQ2:TNG Deathwatch - This should be IDLE not NORM
	gi.sound (self, CHAN_VOICE, gi.soundindex (fullpath), 1, ATTN_IDLE, 0);
	// AQ2:TNG END
}

//AQ2:TNG END

// Show your location name, plus position and heading if cheats are enabled.
void Cmd_WhereAmI_f( edict_t * self )
{
	char location[ 128 ] = "";
	bool found = GetPlayerLocation( self, location );

	if( found )
		gi.LocClient_Print( self, PRINT_MEDIUM, "Location: %s\n", location );
	else if( ! sv_cheats->value )
		gi.LocClient_Print( self, PRINT_MEDIUM, "Location unknown.\n" );

	if( sv_cheats->value )
	{
		gi.LocClient_Print( self, PRINT_MEDIUM, "Origin: %5.0f,%5.0f,%5.0f  Facing: %3.0f\n",
			self->s.origin[0], self->s.origin[1], self->s.origin[2], self->s.angles[1] );
	}
}

// Variables for new flags

static char flagpos1[64] = { 0 };
static char flagpos2[64] = { 0 };

//sets red flag position - cheats must be enabled!
void Cmd_SetFlag1_f (edict_t * self)
{
	Com_sprintf (flagpos1, sizeof(flagpos1), "<%.2f %.2f %.2f>", self->s.origin[0], self->s.origin[1],
		self->s.origin[2]);
	gi.LocClient_Print (self, PRINT_MEDIUM, "\nRed Flag added at %s.\n", flagpos1);
}

//sets blue flag position - cheats must be enabled!
void Cmd_SetFlag2_f (edict_t * self)
{
	Com_sprintf (flagpos2, sizeof(flagpos2), "<%.2f %.2f %.2f>", self->s.origin[0], self->s.origin[1],
		self->s.origin[2]);
	gi.LocClient_Print (self, PRINT_MEDIUM, "\nBlue Flag added at %s.\n", flagpos2);
}

//Save flag definition file - cheats must be enabled!
void Cmd_SaveFlags_f (edict_t * self)
{
	FILE *fp;
	char buf[128];

	if (!(flagpos1[0] && flagpos2[0]))
	{
		gi.LocClient_Print (self, PRINT_MEDIUM,
			"You only can save flag positions when you've already set them\n");
		return;
	}

	sprintf (buf, "%s/tng/%s.flg", GAMEVERSION, level.mapname);
	fp = fopen (buf, "w");
	if (fp == NULL)
	{
		gi.LocClient_Print (self, PRINT_MEDIUM, "\nError accessing flg file %s.\n", buf);
		return;
	}
	sprintf (buf, "# %s\n", level.mapname);
	fputs (buf, fp);
	sprintf (buf, "%s\n", flagpos1);
	fputs (buf, fp);
	sprintf (buf, "%s\n", flagpos2);
	fputs (buf, fp);
	fclose (fp);

	flagpos1[0] = 0;
	flagpos2[0] = 0;

	gi.LocClient_Print (self, PRINT_MEDIUM, "\nFlag File saved.\n");
}

//-----------------------------------------------------------------------------

// Old g_cmds.c START
//-----------------------------------------------------------------------------
bool FloodCheck (edict_t *ent)
{
	if (flood_threshold->value)
	{
		ent->client->resp.penalty++;

		if (ent->client->resp.penalty > flood_threshold->value)
		{
			gi.LocClient_Print(ent, PRINT_HIGH, "You can't talk for %d seconds.\n", ent->client->resp.penalty - (int)flood_threshold->value);
			return 1;
		}
	}

	return 0;
}

/*
==============
LookupPlayer
==============
Look up a player by partial subname, full name or client id. If multiple
matches, show a list. Return NULL on failure. Case insensitive.
*/
edict_t *LookupPlayer(edict_t *ent, const char *text, bool checkNUM, bool checkNick)
{
	edict_t		*p = NULL, *entMatch = NULL;
	int			i, matchCount, numericMatch;
	char		match[32];
	const char	*m, *name;

	if (!text[0])
		return NULL;

	Q_strncpyz(match, text, sizeof(match));
	matchCount = numericMatch = 0;

	m = match;

	while (*m) {
		if (!Q_isdigit(*m)) {
			break;
		}
		m++;
	}

	if (!*m && checkNUM)
	{
		numericMatch = atoi(match);
		if (numericMatch < 0 || numericMatch >= game.maxclients)
		{
			if (ent)
				gi.LocClient_Print(ent, PRINT_HIGH, "Invalid client id %d.\n", numericMatch);
			return 0;
		}

		p = g_edicts + 1 + numericMatch;
		if (!p->inuse || !p->client) {
			if (ent)
				gi.LocClient_Print(ent, PRINT_HIGH, "Client %d is not active.\n", numericMatch);
			return NULL;
		}

		return p;
	}

	if (!checkNick) {
		if (ent)
			gi.LocClient_Print(ent, PRINT_HIGH, "Invalid client id '%s'\n", match);

		return NULL;
	}

	for (i = 0, p = g_edicts + 1; i < game.maxclients; i++, p++)
	{
		if (!p->inuse || !p->client)
			continue;

		name = p->client->pers.netname;
		if (!Q_stricmp(name, match)) { //Exact match
			return p;
		}

		if (Q_stristr(name, match)) {
			matchCount++;
			entMatch = p;
			continue;
		}
	}

	if (matchCount == 1) {
		return entMatch;
	}

	if (matchCount > 1)
	{
		if (ent)
		{
			gi.LocClient_Print(ent, PRINT_HIGH, "'%s' matches multiple players:\n", match);
			for (i = 0, p = g_edicts + 1; i < game.maxclients; i++, p++) {
				if (!p->inuse || !p->client)
					continue;

				name = p->client->pers.netname;
				if (Q_stristr(name, match)) {
					gi.LocClient_Print(ent, PRINT_HIGH, "%3d. %s\n", i, name);
				}
			}
		}
		return NULL;
	}

	if (ent)
		gi.LocClient_Print(ent, PRINT_HIGH, "No player match found for '%s'\n", match);

	return NULL;
}

char *ClientTeam (edict_t * ent)
{
	char *p;
	static char value[128];

	value[0] = 0;

	if (!ent->client)
		return value;

	gi.Info_ValueForKey(ent->client->pers.userinfo, "skin", value, sizeof(value));
	//Q_strncpyz(value, Info_ValueForKey (ent->client->pers.userinfo, "skin"), sizeof(value));
	p = strchr (value, '/');
	if (!p)
		return value;

	// No model teams
	// if (DMFLAGS(DF_MODELTEAMS))
	// {
	// 	*p = 0;
	// 	return value;
	// }

	// if (DMFLAGS(DF_SKINTEAMS))
	return ++p;
}

bool OnSameTeam (edict_t * ent1, edict_t * ent2)
{
	char ent1Team[128], ent2Team[128];

	//FIREBLADE
	if (!ent1->client || !ent2->client)
		return false;

	if (teamplay->value)
		return ent1->client->resp.team == ent2->client->resp.team;
	//FIREBLADE

	// Not used
	// if (!DMFLAGS( (DF_MODELTEAMS | DF_SKINTEAMS) ))
	// 	return false;

	Q_strncpyz (ent1Team, ClientTeam(ent1), sizeof(ent1Team));
	Q_strncpyz (ent2Team, ClientTeam(ent2), sizeof(ent2Team));

	if (strcmp (ent1Team, ent2Team) == 0)
		return true;

	return false;
}

//=================================================================================


//SLICER
static void Cmd_Ent_Count_f (edict_t * ent)
{
	int x = 0;
	edict_t *e;

	for (e = g_edicts; e < &g_edicts[globals.num_edicts]; e++)
	{
		if (e->inuse)
			x++;
	}

	gi.LocClient_Print(ent, PRINT_HIGH, "%d entities counted\n", x);
}

extern char *menu_itemnames[ITEM_MAX_NUM];

// TODO: Work with weapon / item bans in new Q2R engine
// static void wpflagsSettings( char *s, size_t size, int flags )
// {
// 	int i, num;

// 	if (!(flags & WPF_MASK)) {
// 		Q_strncatz( s, "No weapons", size );
// 		return;
// 	}
// 	if ((flags & WPF_MASK) == WPF_MASK) {
// 		Q_strncatz( s, "All weapons", size );
// 		return;
// 	}

// 	for (i = 0; i<WEAPON_COUNT; i++) {
// 		num = WEAPON_FIRST + i;
// 		if (flags == items[WEAPON_FIRST + i].flag) {
// 			Q_strncatz( s, va("%s only", menu_itemnames[num]), size );
// 			return;
// 		}
// 	}

// 	for (i = 0; i<WEAPON_COUNT; i++) {
// 		num = WEAPON_FIRST + i;
// 		if (flags & items[num].flag) {
// 			Q_strncatz( s, va("%d = %s ", items[num].flag, menu_itemnames[num]), size );
// 		}
// 	}
// }

// static void itmflagsSettings( char *s, size_t size, int flags )
// {
// 	int i, num;

// 	if (!(flags & ITF_MASK)) {
// 		Q_strncatz( s, "No items", size );
// 		return;
// 	}
// 	if ((flags & ITF_MASK) == ITF_MASK) {
// 		Q_strncatz( s, "All items", size );
// 		return;
// 	}

// 	for (i = 0; i<ITEM_COUNT; i++) {
// 		num = ITEM_FIRST + i;
// 		if (flags == items[num].flag) {
// 			Q_strncatz( s, va("%s only", menu_itemnames[num]), size );
// 			return;
// 		}
// 	}

// 	for (i = 0; i<ITEM_COUNT; i++) {
// 		num = ITEM_FIRST + i;
// 		if (flags & items[num].flag) {
// 			Q_strncatz( s, va("%d = %s ", items[num].flag, menu_itemnames[num]), size );
// 		}
// 	}
// }

//Print current match settings
static void Cmd_PrintSettings_f( edict_t * ent )
{
	char text[1024] = "\0";
	size_t length = 0;

	length = strlen( text );
	Com_sprintf( text + length, sizeof( text ) - length, "\n"
		"timelimit   %2d roundlimit  %2d roundtimelimit %2d\n"
		"limchasecam %2d tgren\n",
		(int)timelimit->value, (int)roundlimit->value, (int)roundtimelimit->value,
		(int)limchasecam->value, (int)tgren->value);

	gi.LocClient_Print( ent, PRINT_HIGH, text );
}

static void Cmd_Follow_f( edict_t *ent )
{
	edict_t *target = NULL;

	if( (!IS_ALIVE(ent)) )
	{
		gi.LocClient_Print( ent, PRINT_HIGH, "Only spectators may follow!\n" );
		return;
	}

	target = LookupPlayer( ent, gi.argv(1), true, true );
	if( target == ent )
	{
		if( ! limchasecam->value )
			SetChase( ent, NULL );
	}
	else if( target )
	{
		if( limchasecam->value && teamplay->value
		&& (ent->client->resp.team != NOTEAM)
		&& (ent->client->resp.team != target->client->resp.team) )
		{
			gi.LocClient_Print( ent, PRINT_HIGH, "You may not follow enemies!\n" );
			return;
		}

		if( ! ent->client->chase_mode )
			NextChaseMode( ent );
		SetChase( ent, target );
	}
}

static void Cmd_SayAll_f (edict_t * ent) {
	Cmd_Say_f (ent, false, false, false);
}
static void Cmd_SayTeam_f (edict_t * ent) {
	if (teamplay->value) // disable mm2 trigger flooding
		Cmd_Say_f (ent, true, false, false);
}

static void Cmd_Streak_f (edict_t * ent) {
	gi.LocClient_Print(ent,PRINT_HIGH, "Your Killing Streak is: %d\n", ent->client->resp.streakKills);
}

static void Cmd_LockTeam_f (edict_t * ent) {
	Cmd_TeamLock_f(ent, 1);
}

static void Cmd_UnlockTeam_f (edict_t * ent) {
	Cmd_TeamLock_f(ent, 0);
}

static void Cmd_PrintStats_f (edict_t *ent) {
	Cmd_Stats_f(ent, gi.argv(1));
}

static void Cmd_PauseGame_f (edict_t *ent) {
	Cmd_TogglePause_f(ent, true);
}

static void Cmd_UnpauseGame_f (edict_t *ent) {
	Cmd_TogglePause_f(ent, false);
}

static void Cmd_InvNext_f (edict_t *ent) {
	SelectNextItem(ent, -1);
}

static void Cmd_InvPrev_f (edict_t *ent) {
	SelectPrevItem(ent, -1);
}

static void Cmd_InvNextw_f (edict_t *ent) {
	SelectNextItem(ent, IF_WEAPON);
}

static void Cmd_InvPrevw_f (edict_t *ent) {
	SelectPrevItem(ent, IF_WEAPON);
}

static void Cmd_InvNextp_f (edict_t *ent) {
	SelectNextItem(ent, IF_POWERUP);
}

static void Cmd_InvPrevp_f (edict_t *ent) {
	SelectPrevItem(ent, IF_POWERUP);
}

#define CMDF_CHEAT	1 //Need cheat to be enabled
#define CMDF_PAUSE	2 //Cant use while pause

typedef struct cmdList_s
{
	const char *name;
	void( *function ) (edict_t *ent);
	int flags;
	struct cmdList_s *next;
	struct cmdList_s *hashNext;
} cmdList_t;

static cmdList_t commandList[] =
{
	{ "players", Cmd_Players_f, 0 },
	{ "say", Cmd_SayAll_f, 0 },
	{ "say_team", Cmd_SayTeam_f, 0 },
	{ "score", Cmd_Score_f, 0 },
	{ "help", Cmd_Help_f, 0 },
	{ "use", Cmd_Use_f, CMDF_PAUSE },
	{ "drop", Cmd_Drop_f, CMDF_PAUSE },
	//cheats
	{ "give", Cmd_Give_f, CMDF_CHEAT },
	{ "god", Cmd_God_f, CMDF_CHEAT },
	{ "notarget", Cmd_Notarget_f, CMDF_CHEAT },
	{ "noclip", Cmd_Noclip_f, CMDF_CHEAT },
	//-
	{ "inven", Cmd_Inven_f, 0 },
	{ "invnext", Cmd_InvNext_f, 0 },
	{ "invprev", Cmd_InvPrev_f, 0 },
	{ "invnextw", Cmd_InvNextw_f, 0 },
	{ "invprevw", Cmd_InvPrevw_f, 0 },
	{ "invnextp", Cmd_InvNextp_f, 0 },
	{ "invprevp", Cmd_InvPrevp_f, 0 },
	{ "invuse", Cmd_InvUse_f, CMDF_PAUSE },
	{ "invdrop", Cmd_InvDrop_f, CMDF_PAUSE },
	{ "weapprev", Cmd_WeapPrev_f, CMDF_PAUSE },
	{ "weapnext", Cmd_WeapNext_f, CMDF_PAUSE },
	{ "weaplast", Cmd_WeapLast_f, CMDF_PAUSE },
	{ "kill", Cmd_Kill_f, 0 },
	{ "putaway", Cmd_PutAway_f, 0 },
	{ "wave", Cmd_Wave_f, CMDF_PAUSE },
	{ "streak", Cmd_Streak_f, 0 },
	{ "reload", Cmd_New_Reload_f, CMDF_PAUSE },
	{ "weapon", Cmd_New_Weapon_f, CMDF_PAUSE },
	{ "opendoor", Cmd_OpenDoor_f, CMDF_PAUSE },
	{ "bandage", Cmd_Bandage_f, CMDF_PAUSE },
	{ "id", Cmd_ID_f, 0 },
	{ "irvision", Cmd_IR_f, CMDF_PAUSE },
	{ "playerlist", Cmd_PlayerList_f, 0 },
	{ "team", Team_f, 0 },
	{ "radio", Cmd_Radio_f, 0 },
	{ "radiogender", Cmd_Radiogender_f, 0 },
	{ "radio_power", Cmd_Radio_power_f, 0 },
	{ "radiopartner", Cmd_Radiopartner_f, 0 },
	{ "radioteam", Cmd_Radioteam_f, 0 },
	{ "channel", Cmd_Channel_f, 0 },
	{ "say_partner", Cmd_Say_partner_f, 0 },
	{ "partner", Cmd_Partner_f, 0 },
	{ "unpartner", Cmd_Unpartner_f, 0 },
	{ "motd", PrintMOTD, 0 },
	{ "deny", Cmd_Deny_f, 0 },
	{ "choose", Cmd_Choose_f, 0 },
	{ "tkok", Cmd_TKOk, 0 },
	{ "forgive", Cmd_TKOk, 0 },
	{ "ff", Cmd_FF_f, 0 },
	{ "time", Cmd_Time, 0 },
	{ "voice", Cmd_Voice_f, CMDF_PAUSE },
	{ "whereami", Cmd_WhereAmI_f, 0 },
	{ "setflag1", Cmd_SetFlag1_f, CMDF_PAUSE|CMDF_CHEAT },
	{ "setflag2", Cmd_SetFlag2_f, CMDF_PAUSE|CMDF_CHEAT },
	{ "saveflags", Cmd_SaveFlags_f, CMDF_PAUSE|CMDF_CHEAT },
	{ "punch", Cmd_Punch_f, CMDF_PAUSE },
	{ "menu", Cmd_Menu_f, 0 },
	{ "rules", Cmd_Rules_f, 0 },
	{ "lens", Cmd_Lens_f, CMDF_PAUSE },
	{ "nextmap", Cmd_NextMap_f, 0 },
	{ "%cpsi", Cmd_CPSI_f, 0 },
	{ "%!fc", Cmd_VidRef_f, 0 },
	{ "sub", Cmd_Sub_f, 0 },
	{ "captain", Cmd_Captain_f, 0 },
	{ "ready", Cmd_Ready_f, 0 },
	{ "teamname", Cmd_Teamname_f, 0 },
	{ "teamskin", Cmd_Teamskin_f, 0 },
	{ "lock", Cmd_LockTeam_f, 0 },
	{ "unlock", Cmd_UnlockTeam_f, 0 },
	{ "entcount", Cmd_Ent_Count_f, 0 },
	{ "stats", Cmd_PrintStats_f, 0 },
	{ "flashlight", FL_make, CMDF_PAUSE },
	{ "matchadmin", Cmd_SetAdmin_f, 0 },
	{ "roundtimeleft", Cmd_Roundtimeleft_f, 0 },
	{ "autorecord", Cmd_AutoRecord_f, 0 },
	{ "stat_mode", Cmd_Statmode_f, 0 },
	{ "cmd_stat_mode", Cmd_Statmode_f, 0 },
	{ "ghost", Cmd_Ghost_f, 0 },
	{ "pausegame", Cmd_PauseGame_f, 0 },
	{ "unpausegame", Cmd_UnpauseGame_f, 0 },
	{ "resetscores", Cmd_ResetScores_f, 0 },
	{ "gamesettings", Cmd_PrintSettings_f, 0 },
	{ "follow", Cmd_Follow_f, 0 },
	//vote stuff
	{ "votemap", Cmd_Votemap_f, 0 },
	{ "maplist", Cmd_Maplist_f, 0 },
	{ "votekick", Cmd_Votekick_f, 0 },
	{ "votekicknum", Cmd_Votekicknum_f, 0 },
	{ "kicklist", Cmd_Kicklist_f, 0 },
	{ "ignore", Cmd_Ignore_f, 0 },
	{ "ignorenum", Cmd_Ignorenum_f, 0 },
	{ "ignorelist", Cmd_Ignorelist_f, 0 },
	{ "ignoreclear", Cmd_Ignoreclear_f, 0 },
	{ "ignorepart", Cmd_IgnorePart_f, 0 },
	{ "voteconfig", Cmd_Voteconfig_f, 0 },
	{ "configlist", Cmd_Configlist_f, 0 },
	{ "votescramble", Cmd_Votescramble_f, 0 },
	// JumpMod
	{ "jmod", Cmd_Jmod_f, 0 }
};

#define MAX_COMMAND_HASH 64

static cmdList_t *commandHash[MAX_COMMAND_HASH];
static const int numCommands = sizeof( commandList ) / sizeof( commandList[0] );

size_t Cmd_HashValue( const char *name )
{
	size_t hash = 0;

	while (*name) {
		hash = hash * 33 + Q_tolower( *name++ );
	}

	return hash + (hash >> 5);
}

void InitCommandList( void )
{

	int i;
	size_t hash;

	for (i = 0; i < numCommands - 1; i++) {
		commandList[i].next = &commandList[i + 1];
	}
	commandList[i].next = NULL;

	memset( commandHash, 0, sizeof( commandHash ) );
	for (i = 0; i < numCommands; i++) {
		hash = Cmd_HashValue( commandList[i].name ) & (MAX_COMMAND_HASH - 1);
		commandList[i].hashNext = commandHash[hash];
		commandHash[hash] = &commandList[i];
	}
}


