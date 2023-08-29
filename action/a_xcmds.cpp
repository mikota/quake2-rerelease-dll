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
	if (!use_punch->value || !IS_ALIVE(self) || self->client->resp.sniper_mode != SNIPER_1X)
		return;

	if (self->client->weaponstate != WEAPON_READY && self->client->weaponstate != WEAPON_END_MAG)
		return;

	// animation moved to punch_attack() in a_xgame.c
	// punch_attack is now called in ClientThink after evaluation punch_desired
	// for "no punch when firing" stuff - TempFile
	if (level.framenum > self->client->punch_framenum + PUNCH_DELAY) {
		self->client->punch_framenum = level.framenum;	// you aren't Bruce Lee! :)
		self->client->punch_desired = true;
	}
}

//Plays a sound file
void Cmd_Voice_f (edict_t * self)
{
	char *s;
	char fullpath[MAX_QPATH];

	if (!use_voice->value)
		return;

	s = gi.args ();
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

	Q_strncpyz(value, Info_ValueForKey (ent->client->pers.userinfo, "skin"), sizeof(value));
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



/*
==================
Cmd_Say_f
==================
*/
void Cmd_Say_f (edict_t * ent, bool team, bool arg0, bool partner_msg)
{
	int j, offset_of_text;
	edict_t *other;
	char *args, text[256], *s;
	int meing = 0, isadmin = 0;
	bool show_info = false;

	if (gi.argc() < 2 && !arg0)
		return;
	
	args = gi.args();
	if (!args)
		return;

	if (!sv_crlf->value)
	{
		if (strchr(args, '\r') || strchr(args, '\n'))
		{
			gi.LocClient_Print (ent, PRINT_HIGH, "No control characters in chat messages!\n");
			return;
		}
	}

	//TempFile - BEGIN
	if (arg0)
	{
		if (!Q_stricmp("%me", gi.argv(0))) {
			meing = 4;
			if (!*args)
				return;
		}
	}
	else
	{
		if (!*args)
			return;

		if (!Q_strnicmp("%me", args, 3))
			meing = 4;
		else if (!Q_strnicmp("%me", args + 1, 3))
			meing = 5;

		if(meing)
		{
			if (!*(args+meing-1))
				return;
			if (*(args+meing-1) != ' ')
				meing = 0;
			else if(!*(args+meing))
				return;
		}
	}
	//TempFile - END

	if (!teamplay->value)
	{
		//FIREBLADE
		if (!DMFLAGS( (DF_MODELTEAMS | DF_SKINTEAMS) ))
			team = false;
	}
	else if (matchmode->value)
	{	
		if (ent->client->pers.admin)
			isadmin = 1;

		if (mm_forceteamtalk->value == 1)
		{
			if (!IS_CAPTAIN(ent) && !partner_msg && !isadmin)
				team = true;
		}
		else if (mm_forceteamtalk->value == 2)
		{
			if (!IS_CAPTAIN(ent) && !partner_msg && !isadmin &&
				(TeamsReady() || team_round_going))
				team = true;
		}
	}

	if (team)
	{
		if (ent->client->resp.team == NOTEAM)
		{
			gi.LocClient_Print (ent, PRINT_HIGH, "You're not on a team.\n");
			return;
		}
		if (!meing)		// TempFile
			Com_sprintf (text, sizeof (text), "%s(%s): ",
			(teamplay->value && !IS_ALIVE(ent)) ? "[DEAD] " : "",
			ent->client->pers.netname);
		//TempFile - BEGIN
		else
			Com_sprintf (text, sizeof (text), "(%s%s ",
			(teamplay->value && !IS_ALIVE(ent)) ? "[DEAD] " : "",
			ent->client->pers.netname);
		//TempFile - END
	}
	else if (partner_msg)
	{
		if (ent->client->resp.radio.partner == NULL)
		{
			gi.LocClient_Print (ent, PRINT_HIGH, "You don't have a partner.\n");
			return;
		}
		if (!meing)		//TempFile
			Com_sprintf (text, sizeof (text), "[%sPARTNER] %s: ",
			(teamplay->value && !IS_ALIVE(ent)) ? "DEAD " : "",
			ent->client->pers.netname);
		//TempFile - BEGIN
		else
			Com_sprintf (text, sizeof (text), "%s partner %s ",
			(teamplay->value && !IS_ALIVE(ent)) ? "[DEAD] " : "",
			ent->client->pers.netname);
		//TempFile - END
	}
	else
	{
		if (!meing)		//TempFile
		{
			if (isadmin)
				Com_sprintf (text, sizeof (text), "[ADMIN] %s: ",
				ent->client->pers.netname);
			else
				Com_sprintf (text, sizeof (text), "%s%s: ",
				(teamplay->value && !IS_ALIVE(ent)) ? "[DEAD] " : "",
				ent->client->pers.netname);
		}
		else
			Com_sprintf (text, sizeof (text), "%s%s ",
			(teamplay->value && !IS_ALIVE(ent)) ? "[DEAD] " : "",
			ent->client->pers.netname);
	}
	//TempFile - END
	
	offset_of_text = strlen (text);	//FB 5/31/99
	if (!meing)		//TempFile
	{
		if (arg0)
		{
			Q_strncatz(text, gi.argv (0), sizeof(text));
			if(*args) {
				Q_strncatz(text, " ", sizeof(text));
				Q_strncatz(text, args, sizeof(text));
			}
		}
		else
		{	
			if (*args == '"') {
				args++;
				args[strlen(args) - 1] = 0;
			}
			Q_strncatz(text, args, sizeof(text));
		}
	}
	else			// if meing
	{
		if (arg0)
		{
			//this one is easy: gi.args() cuts /me off for us!
			Q_strncatz(text, args, sizeof(text));
		}
		else
		{
			// we have to cut off "%me ".
			args += meing;
			if (args[strlen(args) - 1] == '"')
				args[strlen(args) - 1] = 0;
			Q_strncatz(text, args, sizeof(text));
		}
		
		if (team)
			Q_strncatz(text, ")", sizeof(text));
	}
	//TempFile - END
	// don't let text be too long for malicious reasons
	if (strlen(text) >= 254)
		text[254] = 0;

	show_info = ! strncmp( text + offset_of_text, "!actionversion", 14 );

	//if( IS_ALIVE(ent) )  // Disabled so we parse dead chat too.
	{
		s = strchr(text + offset_of_text, '%');
		if(s) {
			// this will parse the % variables,
			ParseSayText (ent, s, sizeof(text) - (s - text+1) - 2);
		}
	}

	Q_strncatz(text, "\n", sizeof(text));

	if (FloodCheck(ent))
		return;
	
	if (dedicated->value) {
		gi.LocClient_Print (NULL, PRINT_CHAT, "%s", text);
		if ((!team) && (!partner_msg)) {
		}
	}
	
	for (j = 1; j <= game.maxclients; j++)
	{
		other = &g_edicts[j];
		if (!other->inuse || !other->client)
			continue;

		if (other != ent)
		{
			if (team)
			{
				// if we are the adminent... we might want to hear (if hearall is set)
				if (!matchmode->value || !hearall->value || !other->client->pers.admin)	// hearall isn't set and we aren't adminent
					if (!OnSameTeam(ent, other))
						continue;
			}

			if (partner_msg && other != ent->client->resp.radio.partner)
				continue;

			if (team_round_going && (gameSettings & GS_ROUNDBASED))
			{
				if (!deadtalk->value && !IS_ALIVE(ent) && IS_ALIVE(other))
					continue;
			}

			if (IsInIgnoreList(other, ent))
				continue;
		}

		gi.LocClient_Print(other, PRINT_CHAT, "%s", text);

		if( show_info )
			gi.LocClient_Print( other, PRINT_CHAT, "console: AQ2:TNG %s (%i fps)\n", VERSION, game.framerate );
	}

}

static void Cmd_PlayerList_f (edict_t * ent)
{
	int i;
	char st[64];
	char text[1024] = { 0 };
	edict_t *e2;

	// connect time, ping, score, name

	// Set the lines:
	for (i = 0, e2 = g_edicts + 1; i < game.maxclients; i++, e2++)
	{
		int seconds = ((level.framenum - e2->client->resp.enterframe) / HZ) % 60;
		int minutes = ((level.framenum - e2->client->resp.enterframe) / HZ) / 60;

		if (!e2->inuse || !e2->client || e2->client->pers.mvdspec)
			continue;

		if(limchasecam->value)
			Com_sprintf (st, sizeof (st), "%02d:%02d %4d %3d %s\n", minutes, seconds, e2->client->ping, e2->client->resp.team, e2->client->pers.netname); // This shouldn't show player's being 'spectators' during games with limchasecam set and/or during matchmode
		else if (!teamplay->value || !noscore->value)
			Com_sprintf (st, sizeof (st), "%02d:%02d %4d %3d %s%s\n", minutes, seconds, e2->client->ping, e2->client->resp.score, e2->client->pers.netname, (e2->solid == SOLID_NOT && e2->deadflag != DEAD_DEAD) ? " (dead)" : ""); // replaced 'spectator' with 'dead'
		else
			Com_sprintf (st, sizeof (st), "%02d:%02d %4d %s%s\n", minutes, seconds, e2->client->ping, e2->client->pers.netname, (e2->solid == SOLID_NOT && e2->deadflag != DEAD_DEAD) ? " (dead)" : ""); // replaced 'spectator' with 'dead'

		if (strlen(text) + strlen(st) > sizeof(text) - 6)
		{
			strcat(text, "...\n");
			break;
		}
		strcat (text, st);
	}
	gi.LocClient_Print(ent, PRINT_HIGH, "%s", text);
}

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

//SLICER END
static void dmflagsSettings( char *s, size_t size, int flags )
{
	if (!flags) {
		Q_strncatz( s, "NONE", size );
		return;
	}
	if (flags & DF_NO_HEALTH)
		Q_strncatz( s, "1 = no health ", size );
	if (flags & DF_NO_ITEMS)
		Q_strncatz( s, "2 = no items ", size );
	if (flags & DF_WEAPONS_STAY)
		Q_strncatz( s, "4 = weapons stay ", size );
	if (flags & DF_NO_FALLING)
		Q_strncatz( s, "8 = no fall damage ", size );
	if (flags & DF_INSTANT_ITEMS)
		Q_strncatz( s, "16 = instant items ", size );
	if (flags & DF_SAME_LEVEL)
		Q_strncatz( s, "32 = same level ", size );
	if (flags & DF_SKINTEAMS)
		Q_strncatz( s, "64 = skinteams ", size );
	if (flags & DF_MODELTEAMS)
		Q_strncatz( s, "128 = modelteams ", size );
	if (flags & DF_NO_FRIENDLY_FIRE)
		Q_strncatz( s, "256 = no ff ", size );
	if (flags & DF_SPAWN_FARTHEST)
		Q_strncatz( s, "512 = spawn farthest ", size );
	if (flags & DF_FORCE_RESPAWN)
		Q_strncatz( s, "1024 = force respawn ", size );
	//if(flags & DF_NO_ARMOR)
	//	Q_strncatz(s, "2048 = no armor ", size);
	if (flags & DF_ALLOW_EXIT)
		Q_strncatz( s, "4096 = allow exit ", size );
	if (flags & DF_INFINITE_AMMO)
		Q_strncatz( s, "8192 = infinite ammo ", size );
	if (flags & DF_QUAD_DROP)
		Q_strncatz( s, "16384 = quad drop ", size );
	if (flags & DF_FIXED_FOV)
		Q_strncatz( s, "32768 = fixed fov ", size );

	if (flags & DF_WEAPON_RESPAWN)
		Q_strncatz( s, "65536 = weapon respawn ", size );
}

extern char *menu_itemnames[ITEM_MAX_NUM];

static void wpflagsSettings( char *s, size_t size, int flags )
{
	int i, num;

	if (!(flags & WPF_MASK)) {
		Q_strncatz( s, "No weapons", size );
		return;
	}
	if ((flags & WPF_MASK) == WPF_MASK) {
		Q_strncatz( s, "All weapons", size );
		return;
	}

	for (i = 0; i<WEAPON_COUNT; i++) {
		num = WEAPON_FIRST + i;
		if (flags == items[WEAPON_FIRST + i].flag) {
			Q_strncatz( s, va("%s only", menu_itemnames[num]), size );
			return;
		}
	}

	for (i = 0; i<WEAPON_COUNT; i++) {
		num = WEAPON_FIRST + i;
		if (flags & items[num].flag) {
			Q_strncatz( s, va("%d = %s ", items[num].flag, menu_itemnames[num]), size );
		}
	}
}

static void itmflagsSettings( char *s, size_t size, int flags )
{
	int i, num;

	if (!(flags & ITF_MASK)) {
		Q_strncatz( s, "No items", size );
		return;
	}
	if ((flags & ITF_MASK) == ITF_MASK) {
		Q_strncatz( s, "All items", size );
		return;
	}

	for (i = 0; i<ITEM_COUNT; i++) {
		num = ITEM_FIRST + i;
		if (flags == items[num].flag) {
			Q_strncatz( s, va("%s only", menu_itemnames[num]), size );
			return;
		}
	}

	for (i = 0; i<ITEM_COUNT; i++) {
		num = ITEM_FIRST + i;
		if (flags & items[num].flag) {
			Q_strncatz( s, va("%d = %s ", items[num].flag, menu_itemnames[num]), size );
		}
	}
}

//Print current match settings
static void Cmd_PrintSettings_f( edict_t * ent )
{
	char text[1024] = "\0";
	size_t length = 0;

	if (game.serverfeatures & GMF_VARIABLE_FPS) {
		Com_sprintf( text, sizeof( text ), "Server fps = %d\n", game.framerate );
		length = strlen( text );
	}

	Com_sprintf( text + length, sizeof( text ) - length, "sv_antilag = %d\n", (int)sv_antilag->value );
	length = strlen( text );
	
	Com_sprintf( text + length, sizeof( text ) - length, "dmflags %i: ", (int)dmflags->value );
	dmflagsSettings( text, sizeof( text ), (int)dmflags->value );

	length = strlen( text );
	Com_sprintf( text + length, sizeof( text ) - length, "\nwp_flags %i: ", (int)wp_flags->value );
	wpflagsSettings( text, sizeof( text ), (int)wp_flags->value );

	length = strlen( text );
	Com_sprintf( text + length, sizeof( text ) - length, "\nitm_flags %i: ", (int)itm_flags->value );
	itmflagsSettings( text, sizeof( text ), (int)itm_flags->value );

	length = strlen( text );
	Com_sprintf( text + length, sizeof( text ) - length, "\n"
		"timelimit   %2d roundlimit  %2d roundtimelimit %2d\n"
		"limchasecam %2d tgren       %2d antilag_interp %2d\n"
		"llsound     %2d\n",
		(int)timelimit->value, (int)roundlimit->value, (int)roundtimelimit->value,
		(int)limchasecam->value, (int)tgren->value, (int)sv_antilag_interp->value,
		(int)llsound->value );

	gi.LocClient_Print( ent, PRINT_HIGH, text );
}

static void Cmd_Follow_f( edict_t *ent )
{
	edict_t *target = NULL;

	if( (ent->solid != SOLID_NOT) || (ent->deadflag == DEAD_DEAD) )
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
	SelectNextItem(ent, IT_WEAPON);
}

static void Cmd_InvPrevw_f (edict_t *ent) {
	SelectPrevItem(ent, IT_WEAPON);
}

static void Cmd_InvNextp_f (edict_t *ent) {
	SelectNextItem(ent, IT_POWERUP);
}

static void Cmd_InvPrevp_f (edict_t *ent) {
	SelectPrevItem(ent, IT_POWERUP);
}

 // AQ2:TNG - Slicer : Video Check
static void Cmd_VidRef_f (edict_t * ent)
{
	if (video_check->value || video_check_lockpvs->value)
	{
		Q_strncpyz(ent->client->resp.vidref, gi.argv(1), sizeof(ent->client->resp.vidref));
	}

}

static void Cmd_CPSI_f (edict_t * ent)
{
	if (video_check->value || video_check_lockpvs->value || video_check_glclear->value || darkmatch->value)
	{
		ent->client->resp.glmodulate = atoi(gi.argv(1));
		ent->client->resp.gllockpvs = atoi(gi.argv(2));
		ent->client->resp.glclear = atoi(gi.argv(3));
		ent->client->resp.gldynamic = atoi(gi.argv(4));
		ent->client->resp.glbrightness = atoi(gi.argv(5));
		Q_strncpyz(ent->client->resp.gldriver, gi.argv (6), sizeof(ent->client->resp.gldriver));
		//      strncpy(ent->client->resp.vidref,gi.argv(4),sizeof(ent->client->resp.vidref-1));
		//      ent->client->resp.vidref[15] = 0;
	}
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

/*
=================
ClientCommand
=================
*/
void ClientCommand (edict_t * ent)
{
	char		*text;
	cmdList_t	*cmd;
	size_t		hash;

	if (!ent->client)
		return;			// not fully in game yet

	// if (level.intermission_framenum)
	// return;

	text = gi.argv(0);

	hash = Cmd_HashValue( text ) & (MAX_COMMAND_HASH - 1);
	for (cmd = commandHash[hash]; cmd; cmd = cmd->hashNext) {
		if (!Q_stricmp( text, cmd->name )) {
			if ((cmd->flags & CMDF_CHEAT) && !sv_cheats->value) {
				gi.LocClient_Print(ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
				return;
			}

			if ((cmd->flags & CMDF_PAUSE) && level.pauseFrames)
				return;

			cmd->function( ent );
			return;
		}
	}

	// anything that doesn't match a command will be a chat
	Cmd_Say_f(ent, false, true, false);
}

