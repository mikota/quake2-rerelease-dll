#include "g_local.h"

// void SP_item_health (edict_t * self);
// void SP_item_health_small (edict_t * self);
// void SP_item_health_large (edict_t * self);
// void SP_item_health_mega (edict_t * self);

// void SP_info_player_start (edict_t * ent);
// void SP_info_player_deathmatch (edict_t * ent);
// void SP_info_player_intermission (edict_t * ent);

// void SP_func_plat (edict_t * ent);
// void SP_func_rotating (edict_t * ent);
// void SP_func_button (edict_t * ent);
// void SP_func_door (edict_t * ent);
// void SP_func_door_secret (edict_t * ent);
// void SP_func_door_rotating (edict_t * ent);
// void SP_func_water (edict_t * ent);
// void SP_func_train (edict_t * ent);
// void SP_func_conveyor (edict_t * self);
// void SP_func_wall (edict_t * self);
// void SP_func_object (edict_t * self);
// void SP_func_explosive (edict_t * self);
// void SP_func_timer (edict_t * self);
// void SP_func_areaportal (edict_t * ent);
// void SP_func_clock (edict_t * ent);
// void SP_func_killbox (edict_t * ent);

// void SP_trigger_always (edict_t * ent);
// void SP_trigger_once (edict_t * ent);
// void SP_trigger_multiple (edict_t * ent);
// void SP_trigger_relay (edict_t * ent);
// void SP_trigger_push (edict_t * ent);
// void SP_trigger_hurt (edict_t * ent);
// void SP_trigger_key (edict_t * ent);
// void SP_trigger_counter (edict_t * ent);
// void SP_trigger_elevator (edict_t * ent);
// void SP_trigger_gravity (edict_t * ent);
// void SP_trigger_monsterjump (edict_t * ent);

// void SP_target_temp_entity (edict_t * ent);
// void SP_target_speaker (edict_t * ent);
// void SP_target_explosion (edict_t * ent);
// void SP_target_changelevel (edict_t * ent);
// void SP_target_secret (edict_t * ent);
// void SP_target_goal (edict_t * ent);
// void SP_target_splash (edict_t * ent);
// void SP_target_spawner (edict_t * ent);
// void SP_target_blaster (edict_t * ent);
// void SP_target_crosslevel_trigger (edict_t * ent);
// void SP_target_crosslevel_target (edict_t * ent);
// void SP_target_laser (edict_t * self);
// void SP_target_help (edict_t * ent);
// void SP_target_lightramp (edict_t * self);
// void SP_target_earthquake (edict_t * ent);
// void SP_target_character (edict_t * ent);
// void SP_target_string (edict_t * ent);

// void SP_worldspawn (edict_t * ent);
// void SP_viewthing (edict_t * ent);

// void SP_light (edict_t * self);
// void SP_light_mine1 (edict_t * ent);
// void SP_light_mine2 (edict_t * ent);
// void SP_info_null (edict_t * self);
// void SP_info_notnull (edict_t * self);
// void SP_path_corner (edict_t * self);
// void SP_point_combat (edict_t * self);

// void SP_misc_explobox (edict_t * self);
// void SP_misc_banner (edict_t * self);
// void SP_misc_satellite_dish (edict_t * self);
// void SP_misc_deadsoldier (edict_t * self);
// void SP_misc_viper (edict_t * self);
// void SP_misc_viper_bomb (edict_t * self);
// void SP_misc_bigviper (edict_t * self);
// void SP_misc_strogg_ship (edict_t * self);
// void SP_misc_teleporter (edict_t * self);
// void SP_misc_teleporter_dest (edict_t * self);
// void SP_misc_blackhole (edict_t * self);


//zucc - item replacement function
int LoadFlagsFromFile (const char *mapname);
void SVCmd_CheckSB_f(void); //rekkie -- silence ban
extern void UnBan_TeamKillers(void);


//Precaches and enables download options for user sounds. All sounds
//have to be listed within "sndlist.ini". called from g_spawn.c -> SP_worldspawn
static void PrecacheUserSounds(void)
{
	int count = 0;
	size_t length;
	FILE *soundlist;
	char buf[1024], fullpath[MAX_QPATH];

	soundlist = fopen(fmt::format("{}{}/sndlist.ini", GAMEVERSION, "/"), "r");
	if (!soundlist) { // no "sndlist.ini" file...
		gi.Com_PrintFmt("Cannot load %s, sound download is disabled.\n", GAMEVERSION "/sndlist.ini");
		return;
	}

	// read the sndlist.ini file
	while (fgets(buf, sizeof(buf), soundlist) != NULL)
	{
		length = strlen(buf);
		//first remove trailing spaces
		while (length > 0 && buf[length - 1] <= ' ')
			buf[--length] = '\0';

		//Comments are marked with # or // at line start
		if (length < 5 || buf[0] == '#' || !strncmp(buf, "//", 2))
			continue;

		Q_strncpyz(fullpath, PG_SNDPATH, sizeof(fullpath));
		Q_strncatz(fullpath, buf, sizeof(fullpath));
		gi.soundindex(fullpath);
		//gi.Com_PrintFmt("Sound %s: precache %i",fullpath, gi.soundindex(fullpath)); 
		count++;
		if (count == 100)
			break;
	}
	fclose(soundlist);
	if (!count)
		gi.Com_PrintFmt("%s is empty, no sounds to precache.\n", GAMEVERSION "/sndlist.ini");
	else
		gi.Com_PrintFmt("%i user sounds precached.\n", count);
}

void G_LoadLocations( void )
{
	//AQ2:TNG New Location Code
	char	locfile[MAX_QPATH], buffer[256];
	FILE	*f;
	int		i, x, y, z, rx, ry, rz;
	char	*locationstr, *param, *line;
	cvar_t	*game_cvar;
	placedata_t *loc;

	memset( ml_creator, 0, sizeof( ml_creator ) );
	ml_count = 0;

	game_cvar = gi.cvar ("game", "action", 0);

	if (!*game_cvar->string)
		Com_sprintf(locfile, sizeof(locfile), "%s/tng/%s.aqg", GAMEVERSION, level.mapname);
	else
		Com_sprintf(locfile, sizeof(locfile), "%s/tng/%s.aqg", game_cvar->string, level.mapname);

	f = fopen( locfile, "r" );
	if (!f) {
		gi.Com_PrintFmt( "No location file for %s\n", level.mapname );
		return;
	}

	gi.Com_PrintFmt( "Location file: %s\n", level.mapname );

	do
	{
		line = fgets( buffer, sizeof( buffer ), f );
		if (!line) {
			break;
		}

		if (strlen( line ) < 12)
			continue;

		if (line[0] == '#')
		{
			param = line + 1;
			while (*param == ' ') { param++; }
			if (*param && !Q_strnicmp(param, "creator", 7))
			{
				param += 8;
				while (*param == ' ') { param++; }
				for (i = 0; *param >= ' ' && i < sizeof( ml_creator ) - 1; i++) {
					ml_creator[i] = *param++;
				}
				ml_creator[i] = 0;
				while (i > 0 && ml_creator[i - 1] == ' ') //Remove railing spaces
					ml_creator[--i] = 0;
			}
			continue;
		}

		param = strtok( line, " :\r\n\0" );
		// TODO: better support for file comments
		if (!param || param[0] == '#')
			continue;

		x = atoi( param );

		param = strtok( NULL, " :\r\n\0" );
		if (!param)
			continue;
		y = atoi( param );

		param = strtok( NULL, " :\r\n\0" );
		if (!param)
			continue;
		z = atoi( param );

		param = strtok( NULL, " :\r\n\0" );
		if (!param)
			continue;
		rx = atoi( param );

		param = strtok( NULL, " :\r\n\0" );
		if (!param)
			continue;
		ry = atoi( param );

		param = strtok( NULL, " :\r\n\0" );
		if (!param)
			continue;
		rz = atoi( param );

		param = strtok( NULL, "\r\n\0" );
		if (!param)
			continue;
		locationstr = param;

		loc = &locationbase[ml_count++];
		loc->x = x;
		loc->y = y;
		loc->z = z;
		loc->rx = rx;
		loc->ry = ry;
		loc->rz = rz;
		Q_strncpyz( loc->desc, locationstr, sizeof( loc->desc ) );

		if (ml_count >= MAX_LOCATIONS_IN_BASE) {
			gi.Com_PrintFmt( "Cannot read more than %d locations.\n", MAX_LOCATIONS_IN_BASE );
			break;
		}
	} while (1);

	fclose( f );
	gi.Com_PrintFmt( "Found %d locations.\n", ml_count );
}



int Gamemode(void) // These are distinct game modes; you cannot have a teamdm tourney mode, for example
{
	int gamemode = 0;
	if (teamdm->value) {
		gamemode = GM_TEAMDM;
	} else if (ctf->value) {
		gamemode = GM_CTF;
	} else if (use_tourney->value) {
		gamemode = GM_TOURNEY;
	} else if (teamplay->value) {
		gamemode = GM_TEAMPLAY;
	} else if (deathmatch->value) {
		gamemode = GM_DEATHMATCH;
	}
	return gamemode;
}

int Gamemodeflag(void)
// These are gamemode flags that change the rules of gamemodes.
// For example, you can have a darkmatch matchmode 3team teamplay server
{
	int gamemodeflag = 0;
	char gmfstr[16];

	if (use_3teams->value) {
		gamemodeflag += GMF_3TEAMS;
	}
	if (darkmatch->value) {
		gamemodeflag += GMF_DARKMATCH;
	}
	if (matchmode->value) {
		gamemodeflag += GMF_MATCHMODE;
	}
	sprintf(gmfstr, "%d", gamemodeflag);
	gi.cvar_forceset("gmf", gmfstr);
	return gamemodeflag;
}

/*
==============
SpawnEntities

Creates a server's entity / program execution context by
parsing textual entity definitions out of an ent file.
==============
*/
void SpawnEntities (char *mapname, char *entities, char *spawnpoint)
{
	edict_t *ent = NULL;
	gclient_t   *client;
	client_persistant_t pers;
	int i, inhibit = 0;
	char *com_token;
	int saved_team;

	// Reset teamplay stuff
	for(i = TEAM1; i < TEAM_TOP; i++)
	{
		teams[i].score = teams[i].total = 0;
		teams[i].ready = teams[i].locked = 0;
		teams[i].pauses_used = teams[i].wantReset = 0;
		teams[i].captain = NULL;
		gi.cvar_forceset(teams[i].teamscore->name, "0");
	}

	day_cycle_at = 0;
	team_round_going = team_game_going = team_round_countdown = 0;
	lights_camera_action = holding_on_tie_check = 0;
	timewarning = fragwarning = 0;

	teamCount = 2;
	gameSettings = 0;

	if (jump->value)
	{
	gi.cvar_forceset(gm->name, "jump");
	gi.cvar_forceset(stat_logs->name, "0"); // Turn off stat logs for jump mode
		if (teamplay->value)
		{
			gi.Com_PrintFmt ("Jump Enabled - Forcing teamplay ff\n");
			gi.cvar_forceset(teamplay->name, "0");
		}
		if (use_3teams->value)
		{
			gi.Com_PrintFmt ("Jump Enabled - Forcing 3Teams off\n");
			gi.cvar_forceset(use_3teams->name, "0");
		}
		if (teamdm->value)
		{
			gi.Com_PrintFmt ("Jump Enabled - Forcing Team DM off\n");
			gi.cvar_forceset(teamdm->name, "0");
		}
		if (use_tourney->value)
		{
			gi.Com_PrintFmt ("Jump Enabled - Forcing Tourney off\n");
			gi.cvar_forceset(use_tourney->name, "0");
		}
		if (ctf->value)
		{
			gi.Com_PrintFmt ("Jump Enabled - Forcing CTF off\n");
			gi.cvar_forceset(ctf->name, "0");
		}
		if (dom->value)
		{
			gi.Com_PrintFmt ("Jump Enabled - Forcing Domination off\n");
			gi.cvar_forceset(dom->name, "0");
		}
		if (use_randoms->value)
		{
			gi.Com_PrintFmt ("Jump Enabled - Forcing Random weapons and items off\n");
			gi.cvar_forceset(use_randoms->name, "0");
		}
	}
	else if (ctf->value)
	{
	gi.cvar_forceset(gm->name, "ctf");
		if (ctf->value == 2)
			gi.cvar_forceset(ctf->name, "1"); //for now

		gameSettings |= GS_WEAPONCHOOSE;

		// Make sure teamplay is enabled
		if (!teamplay->value)
		{
			gi.Com_PrintFmt ("CTF Enabled - Forcing teamplay on\n");
			gi.cvar_forceset(teamplay->name, "1");
		}
		if (use_3teams->value)
		{
			gi.Com_PrintFmt ("CTF Enabled - Forcing 3Teams off\n");
			gi.cvar_forceset(use_3teams->name, "0");
		}
		if(teamdm->value)
		{
			gi.Com_PrintFmt ("CTF Enabled - Forcing Team DM off\n");
			gi.cvar_forceset(teamdm->name, "0");
		}
		if (use_tourney->value)
		{
			gi.Com_PrintFmt ("CTF Enabled - Forcing Tourney off\n");
			gi.cvar_forceset(use_tourney->name, "0");
		}
		if (dom->value)
		{
			gi.Com_PrintFmt ("CTF Enabled - Forcing Domination off\n");
			gi.cvar_forceset(dom->name, "0");
		}
		if (!DMFLAGS(DF_NO_FRIENDLY_FIRE))
		{
			gi.Com_PrintFmt ("CTF Enabled - Forcing Friendly Fire off\n");
			gi.cvar_forceset(dmflags->name, va("%i", (int)dmflags->value | DF_NO_FRIENDLY_FIRE));
		}
		if (use_randoms->value)
		{
			gi.Com_PrintFmt ("CTF Enabled - Forcing Random weapons and items off\n");
			gi.cvar_forceset(use_randoms->name, "0");
		}
		Q_strncpyz(teams[TEAM1].name, "RED", sizeof(teams[TEAM1].name));
		Q_strncpyz(teams[TEAM2].name, "BLUE", sizeof(teams[TEAM2].name));
		Q_strncpyz(teams[TEAM1].skin, "male/ctf_r", sizeof(teams[TEAM1].skin));
		Q_strncpyz(teams[TEAM2].skin, "male/ctf_b", sizeof(teams[TEAM2].skin));
		Q_strncpyz(teams[TEAM1].skin_index, "i_ctf1", sizeof(teams[TEAM1].skin_index));
		Q_strncpyz(teams[TEAM2].skin_index, "i_ctf2", sizeof(teams[TEAM2].skin_index));
	}
	else if (dom->value)
	{
		gi.cvar_forceset(gm->name, "dom");
		gameSettings |= GS_WEAPONCHOOSE;
		if (!teamplay->value)
		{
			gi.Com_PrintFmt ("Domination Enabled - Forcing teamplay on\n");
			gi.cvar_forceset(teamplay->name, "1");
		}
		if (teamdm->value)
		{
			gi.Com_PrintFmt ("Domination Enabled - Forcing Team DM off\n");
			gi.cvar_forceset(teamdm->name, "0");
		}
		if (use_tourney->value)
		{
			gi.Com_PrintFmt ("Domination Enabled - Forcing Tourney off\n");
			gi.cvar_forceset(use_tourney->name, "0");
		}
		if (use_randoms->value)
		{
			gi.Com_PrintFmt ("Domination Enabled - Forcing Random weapons and items off\n");
			gi.cvar_forceset(use_randoms->name, "0");
		}
		Q_strncpyz(teams[TEAM1].name, "RED", sizeof(teams[TEAM1].name));
		Q_strncpyz(teams[TEAM2].name, "BLUE", sizeof(teams[TEAM2].name));
		Q_strncpyz(teams[TEAM3].name, "GREEN", sizeof(teams[TEAM3].name));
		Q_strncpyz(teams[TEAM1].skin, "male/ctf_r", sizeof(teams[TEAM1].skin));
		Q_strncpyz(teams[TEAM2].skin, "male/ctf_b", sizeof(teams[TEAM2].skin));
		Q_strncpyz(teams[TEAM3].skin, "male/commando", sizeof(teams[TEAM3].skin));
		Q_strncpyz(teams[TEAM1].skin_index, "i_ctf1", sizeof(teams[TEAM1].skin_index));
		Q_strncpyz(teams[TEAM2].skin_index, "i_ctf2", sizeof(teams[TEAM2].skin_index));
		Q_strncpyz(teams[TEAM3].skin_index, "i_pack", sizeof(teams[TEAM3].skin_index));
	}
	else if(teamdm->value)
	{
		gi.cvar_forceset(gm->name, "tdm");
		gameSettings |= GS_DEATHMATCH;

		if (dm_choose->value)
			gameSettings |= GS_WEAPONCHOOSE;

		if (!teamplay->value)
		{
			gi.Com_PrintFmt ("Team Deathmatch Enabled - Forcing teamplay on\n");
			gi.cvar_forceset(teamplay->name, "1");
		}
		if (use_tourney->value)
		{
			gi.Com_PrintFmt ("Team Deathmatch Enabled - Forcing Tourney off\n");
			gi.cvar_forceset(use_tourney->name, "0");
		}
	}
	else if (use_3teams->value)
	{
		gi.cvar_forceset(gm->name, "tp");
		gameSettings |= (GS_ROUNDBASED | GS_WEAPONCHOOSE);

		if (!teamplay->value)
		{
			gi.Com_PrintFmt ("3 Teams Enabled - Forcing teamplay on\n");
			gi.cvar_forceset(teamplay->name, "1");
		}
		if (use_tourney->value)
		{
			gi.Com_PrintFmt ("3 Teams Enabled - Forcing Tourney off\n");
			gi.cvar_forceset(use_tourney->name, "0");
		}
	}
	else if (matchmode->value)
	{
		gameSettings |= (GS_ROUNDBASED | GS_WEAPONCHOOSE);
		if (!teamplay->value)
		{
			gi.Com_PrintFmt ("Matchmode Enabled - Forcing teamplay on\n");
			gi.cvar_forceset(teamplay->name, "1");
		}
		if (use_tourney->value)
		{
			gi.Com_PrintFmt ("Matchmode Enabled - Forcing Tourney off\n");
			gi.cvar_forceset(use_tourney->name, "0");
		}
	}
	else if (use_tourney->value)
	{
		gi.cvar_forceset(gm->name, "tourney");
		gameSettings |= (GS_ROUNDBASED | GS_WEAPONCHOOSE);

		if (!teamplay->value)
		{
			gi.Com_PrintFmt ("Tourney Enabled - Forcing teamplay on\n");
			gi.cvar_forceset(teamplay->name, "1");
		}
	}
	else if (teamplay->value)
	{
		gi.cvar_forceset(gm->name, "tp");
		gameSettings |= (GS_ROUNDBASED | GS_WEAPONCHOOSE);
	}
	else { //Its deathmatch
		gi.cvar_forceset(gm->name, "dm");
		gameSettings |= GS_DEATHMATCH;
		if (dm_choose->value)
			gameSettings |= GS_WEAPONCHOOSE;
	}

	if (teamplay->value)
	{
		gameSettings |= GS_TEAMPLAY;
	}
	if (matchmode->value)
	{
		gameSettings |= GS_MATCHMODE;
		gi.Com_PrintFmt ("Matchmode Enabled - Forcing g_spawn_items off\n");
		gi.cvar_forceset(g_spawn_items->name, "0"); // Turn off spawning of items for matchmode games
	}
	if (use_3teams->value)
	{
		teamCount = 3;
		if (!use_oldspawns->value)
		{
			gi.Com_PrintFmt ("3 Teams Enabled - Forcing use_oldspawns on\n");
			gi.cvar_forceset(use_oldspawns->name, "1");
		}
	}


	gi.cvar_forceset(maptime->name, "0:00");

	gi.FreeTags(TAG_LEVEL);

	// Set serverinfo correctly for gamemodeflags
	Gamemodeflag();

	memset(&level, 0, sizeof (level));
	memset(g_edicts, 0, game.maxentities * sizeof (g_edicts[0]));

	Q_strncpyz(level.mapname, mapname, sizeof(level.mapname));
	Q_strncpyz(game.spawnpoint, spawnpoint, sizeof(game.spawnpoint));

	InitTransparentList();

	// set client fields on player ents
	for (i = 0, ent = &g_edicts[1]; i < game.maxclients; i++, ent++)
	{
		client = &game.clients[i];
		ent->client = client;

		// clear everything but the persistant data
		pers = client->pers;
		saved_team = client->resp.team;
		memset(client, 0, sizeof(*client));
		client->pers = pers;
		if( pers.connected )
		{
			client->clientNum = i;

			if( auto_join->value )
				client->resp.team = saved_team;

			// combine name and skin into a configstring
			AssignSkin( ent, Info_ValueForKey( client->pers.userinfo, "skin" ), false );
		}
	}

	ent = NULL;
	// parse ents
	while (1)
	{
		// parse the opening brace      
		com_token = COM_Parse(&entities);
		if (!entities)
			break;

		if (com_token[0] != '{')
			gi.error ("ED_LoadFromFile: found %s when expecting {", com_token);

		if (!ent)
			ent = g_edicts;
		else
			ent = G_Spawn();

		entities = ED_ParseEdict(entities, ent);

		// yet another map hack
		if (!Q_stricmp (level.mapname, "command")
			&& !Q_stricmp (ent->classname, "trigger_once")
			&& !Q_stricmp (ent->model, "*27"))
			ent->spawnflags &= ~SPAWNFLAG_NOT_HARD;

		// remove things (except the world) from different skill levels or deathmatch
		if (ent != g_edicts)
		{
			if (ent->spawnflags & SPAWNFLAG_NOT_DEATHMATCH)
			{
				G_FreeEdict (ent);
				inhibit++;
				continue;
			}

			ent->spawnflags &=
			~(SPAWNFLAG_NOT_EASY | SPAWNFLAG_NOT_MEDIUM | SPAWNFLAG_NOT_HARD |
			SPAWNFLAG_NOT_COOP | SPAWNFLAG_NOT_DEATHMATCH);
		}

		ED_CallSpawn (ent);
	}

	gi.Com_PrintFmt ("%i entities inhibited\n", inhibit);

	// AQ2:TNG Igor adding .flg files

	// CTF configuration
	if(ctf->value)
	{
		if(!CTFLoadConfig(level.mapname))
		{
			if ((!G_Find(NULL, FOFS (classname), "item_flag_team1") ||
				 !G_Find(NULL, FOFS (classname), "item_flag_team2")))
			{
				gi.Com_PrintFmt ("No native CTF map, loading flag positions from file\n");
				if (LoadFlagsFromFile(level.mapname))
					ChangePlayerSpawns ();
			}
		}
	}
	else if( dom->value )
		DomLoadConfig( level.mapname );

	G_FindTeams();

	// TNG:Freud - Ghosts
	num_ghost_players = 0;

	if (!(gameSettings & GS_WEAPONCHOOSE) && !jump->value)
	{
		//zucc for special items
		SetupSpecSpawn();
	}
	else if (teamplay->value)
	{
		GetSpawnPoints();
		//TNG:Freud - New spawning system
		if(!use_oldspawns->value)
			NS_GetSpawnPoints();
	}

	G_LoadLocations();

	SVCmd_CheckSB_f(); //rekkie -- silence ban

	UnBan_TeamKillers();
}


//===================================================================

#if 0
	// cursor positioning
xl < value > xr < value > yb < value > yt < value > xv < value > yv < value >
  // drawing
  statpic < name > pic < stat > num < fieldwidth > <stat > string < stat >
  // control
  if <stat
  >ifeq < stat > <value > ifbit < stat > <value > endif
#endif

#define STATBAR_COMMON \
/* team icon (draw first to prevent covering health at 320x240) */ \
	"if 4 " \
		"xl 0 " \
		"yb -32 " \
		"pic 4 " \
	"endif " \
	"yb -24 " \
/* health */ \
	"if 0 " \
		"xv 0 " \
		"hnum " \
		"xv 50 " \
		"pic 0 " \
	"endif " \
/* ammo */ \
	"if 2 " \
		"xv 100 " \
		"anum " \
		"xv 150 " \
		"pic 2 " \
	"endif " \
/* selected item */ \
	"if 6 " \
		"xv 296 " \
		"pic 6 " \
	"endif " \
	"yb -50 " \
/* picked up item */ \
	"if 7 " \
		"xv 0 " \
		"pic 7 " \
		"xv 26 " \
		"yb -42 " \
		"stat_string 8 " \
		"yb -50 " \
	"endif " \
/*  help / weapon icon */ \
	"if 11 " \
		"xv 148 " \
		"pic 11 " \
	"endif " \
/* clip(s) */ \
	"if 16 " \
		"yb -24 " \
		"xr -60 " \
		"num 2 17 " \
		"xr -24 " \
		"pic 16 " \
	"endif " \
/* special item ( vest etc ) */ \
	"if 19 " \
		"yb -72 " \
		"xr -24 " \
		"pic 19 " \
	"endif " \
/* special weapon */ \
	"if 20 " \
		"yb -48 " \
		"xr -24 " \
		"pic 20 " \
	"endif " \
/* grenades */ \
	"if 28 " \
		"yb -96 " \
		"xr -60 " \
		"num 2 29 " \
		"xr -24 " \
		"pic 28 " \
	"endif " \
/* spec viewing */ \
	"if 21 " \
		"xv 0 " \
		"yb -58 " \
		"string \"Viewing\" " \
		"xv 64 " \
		"stat_string 21 " \
	"endif " \
/* sniper graphic/icon */ \
	"if 18 " \
		"xv 0 " \
		"yv 0 " \
		"pic 18 " \
	"endif "

void G_SetupStatusbar( void )
{
	level.statusbar[0] = 0;

	if (!nohud->value)
	{
		Q_strncpyz(level.statusbar, STATBAR_COMMON, sizeof(level.statusbar));

		if(!((noscore->value || hud_noscore->value) && teamplay->value)) //  frags
			Q_strncatz(level.statusbar, "xr -50 yt 2 num 3 14 ", sizeof(level.statusbar));

		if (ctf->value)
		{
			Q_strncatz(level.statusbar, 
				// Red Team
				"yb -164 " "if 24 " "xr -24 " "pic 24 " "endif " "xr -60 " "num 2 26 "
				// Blue Team
				"yb -140 " "if 25 " "xr -24 " "pic 25 " "endif " "xr -60 " "num 2 27 "
				// Flag carried
				"if 23 " "yt 26 " "xr -24 " "pic 23 " "endif ",
				sizeof(level.statusbar) );
		}
		else if( dom->value )
			DomSetupStatusbar();
		else if( jump->value )
			strcpy( level.statusbar, jump_statusbar );
	}

	Q_strncpyz(level.spec_statusbar, level.statusbar, sizeof(level.spec_statusbar));
	level.spec_statusbar_lastupdate = 0;
}

void G_UpdateSpectatorStatusbar( void )
{
	char buffer[2048];
	int i, count, isAlive;
	char *rowText;
	gclient_t *cl;
	edict_t *cl_ent;

	Q_strncpyz(buffer, level.statusbar, sizeof(buffer));

	//Team 1
	count = 0;
	//rowText = "Name            Frg Tim Png";
	rowText = "Name            Health   Score";
	sprintf( buffer + strlen( buffer ), "xl 0 yt %d string2 \"%s\" ", 40, rowText );
	for (i = 0, cl = game.clients; i < game.maxclients; i++, cl++) {
		if (!cl->pers.connected || cl->resp.team != TEAM1)
			continue;

		if (cl->resp.subteam)
			continue;

		cl_ent = g_edicts + 1 + i;

		isAlive = IS_ALIVE(cl_ent);

		sprintf( buffer + strlen( buffer ),
			"yt %d string%s \"%-15s %6d   %5d\" ",
			48 + count * 8,
			isAlive ? "2" : "",
			cl->pers.netname,
			isAlive ? cl_ent->health : 0,
			cl->resp.score );

		count++;
		if (count >= 5)
			break;
	}

	//Team 2
	count = 0;
	sprintf( buffer + strlen( buffer ), "xr -%d yt %d string2 \"%s\" ", (int) strlen(rowText) * 8, 40, rowText );
	for (i = 0, cl = game.clients; i < game.maxclients; i++, cl++) {
		if (!cl->pers.connected || cl->resp.team != TEAM2)
			continue;

		if (cl->resp.subteam)
			continue;

		cl_ent = g_edicts + 1 + i;

		isAlive = IS_ALIVE(cl_ent);

		sprintf( buffer + strlen( buffer ),
			"yt %d string%s \"%-15s %6d   %5d\" ",
			48 + count * 8,
			isAlive ? "2" : "",
			cl->pers.netname,
			isAlive ? cl_ent->health : 0,
			cl->resp.score );

		count++;
		if (count >= 5)
			break;
	}

	if (strlen( buffer ) > 1023) {
		buffer[1023] = 0;
	}

	if (strcmp(level.spec_statusbar, buffer)) {
		Q_strncpyz(level.spec_statusbar, buffer, sizeof(level.spec_statusbar));
		level.spec_statusbar_lastupdate = level.realFramenum;
	}
}

/*
==============
G_UpdatePlayerStatusbar
==============
*/
void G_UpdatePlayerStatusbar( edict_t * ent, int force )
{
	char *playerStatusbar;
	if (!teamplay->value || teamCount != 2 || spectator_hud->value <= 0 || !(ent->client->pers.spec_flags & SPECFL_SPECHUD)) {
		return;
	}

	playerStatusbar = level.statusbar;

	if (!ent->client->resp.team)
	{
		if (level.spec_statusbar_lastupdate < level.realFramenum - 3 * HZ) {
			G_UpdateSpectatorStatusbar();
			if (level.spec_statusbar_lastupdate < level.realFramenum - 3 * HZ && !force)
				return;
		}

		playerStatusbar = level.spec_statusbar;
	}
	gi.WriteByte( svc_configstring );
	gi.WriteShort( CS_STATUSBAR );
	gi.WriteString( playerStatusbar );
	gi.unicast( ent, force ? true : false );
}

/*QUAKED worldspawn (0 0 0) ?

Only used for the world.
"sky"   environment map name
"skyaxis"       vector axis for rotating sky
"skyrotate"     speed of rotation in degrees/second
"sounds"        music cd track number
"gravity"       800 is default gravity
"message"       text to print at user logon
*/

void SP_worldspawn (edict_t * ent)
{
	int i, bullets, shells;
	char *picname;

	ent->movetype = MOVETYPE_PUSH;
	ent->solid = SOLID_BSP;
	ent->inuse = true;		// since the world doesn't use G_Spawn()
	ent->s.modelindex = 1;	// world model is always index 1

	// reserve some spots for dead player bodies for coop / deathmatch
	InitBodyQue();

	// set configstrings for items
	SetItemNames();

	level.framenum = 0;
	level.time = 0;
	level.realFramenum = 0;
	level.pauseFrames = 0;
	level.matchTime = 0;
	level.weapon_sound_framenum = 0;

	if (st.nextmap)
		strcpy(level.nextmap, st.nextmap);

	// make some data visible to the server

	if (ent->message && ent->message[0])
	{
		Q_strncpyz(level.level_name, ent->message, sizeof(level.level_name));
		gi.configstring(CS_NAME, level.level_name);
	}
	else {
		strcpy(level.level_name, level.mapname);
	}

	if (st.sky && st.sky[0])
		gi.configstring(CS_SKY, st.sky);
	else
		gi.configstring(CS_SKY, "unit1_");

	gi.configstring(CS_SKYROTATE, va("%f", st.skyrotate));
	gi.configstring(CS_SKYAXIS, va("%f %f %f", st.skyaxis[0], st.skyaxis[1], st.skyaxis[2]));
	gi.configstring(CS_CDTRACK, va("%i", ent->sounds));
	gi.configstring(CS_MAXCLIENTS, va("%i", game.maxclients));

	G_SetupStatusbar();
	gi.configstring(CS_STATUSBAR, level.statusbar);

	level.pic_health = gi.imageindex("i_health");
	gi.imageindex("field_3");

	// zucc - preload sniper stuff
	level.pic_sniper_mode[1] = gi.imageindex("scope2x");
	level.pic_sniper_mode[2] = gi.imageindex("scope4x");
	level.pic_sniper_mode[3] = gi.imageindex("scope6x");

	for (i = 1; i < AMMO_MAX; i++) {
		picname = GET_ITEM(i)->icon;
		if (picname)
			level.pic_items[i] = gi.imageindex( picname );
	}

	bullets = gi.imageindex("a_bullets");
	shells = gi.imageindex("a_shells");
	level.pic_weapon_ammo[MK23_NUM] = bullets;
	level.pic_weapon_ammo[MP5_NUM] = bullets;
	level.pic_weapon_ammo[M4_NUM] = bullets;
	level.pic_weapon_ammo[M3_NUM] = shells;
	level.pic_weapon_ammo[HC_NUM] = shells;
	level.pic_weapon_ammo[SNIPER_NUM] = bullets;
	level.pic_weapon_ammo[DUAL_NUM] = bullets;
	level.pic_weapon_ammo[KNIFE_NUM] = gi.imageindex("w_knife");
	level.pic_weapon_ammo[GRENADE_NUM] = gi.imageindex("a_m61frag");

	gi.imageindex("tag1");
	gi.imageindex("tag2");
	if (teamplay->value)
	{
		level.pic_teamtag = gi.imageindex("tag3");

		if (ctf->value) {
			level.pic_ctf_teamtag[TEAM1] = gi.imageindex("ctfsb1");
			level.pic_ctf_flagbase[TEAM1] = gi.imageindex("i_ctf1");
			level.pic_ctf_flagtaken[TEAM1] = gi.imageindex("i_ctf1t");
			level.pic_ctf_flagdropped[TEAM1] = gi.imageindex("i_ctf1d");

			level.pic_ctf_teamtag[TEAM2] = gi.imageindex("ctfsb2");
			level.pic_ctf_flagbase[TEAM2] = gi.imageindex("i_ctf2");
			level.pic_ctf_flagtaken[TEAM2] = gi.imageindex("i_ctf2t");
			level.pic_ctf_flagdropped[TEAM2] = gi.imageindex("i_ctf2d");
			gi.imageindex("sbfctf1");
			gi.imageindex("sbfctf2");
		}

		for(i = TEAM1; i <= teamCount; i++)
		{
			if (teams[i].skin_index[0] == 0) {
				// If the action.ini file isn't found, set default skins rather than kill the server
				gi.Com_PrintFmt("WARNING: No skin was specified for team %i in config file, server either could not find it or is does not exist.\n", i);
				gi.Com_PrintFmt("Setting default team names, skins and skin indexes.\n", i);
				Q_strncpyz(teams[TEAM1].name, "RED", sizeof(teams[TEAM1].name));
				Q_strncpyz(teams[TEAM2].name, "BLUE", sizeof(teams[TEAM2].name));
				Q_strncpyz(teams[TEAM3].name, "GREEN", sizeof(teams[TEAM3].name));
				Q_strncpyz(teams[TEAM1].skin, "male/ctf_r", sizeof(teams[TEAM1].skin));
				Q_strncpyz(teams[TEAM2].skin, "male/ctf_b", sizeof(teams[TEAM2].skin));
				Q_strncpyz(teams[TEAM3].skin, "male/commando", sizeof(teams[TEAM3].skin));
				Q_strncpyz(teams[TEAM1].skin_index, "i_ctf1", sizeof(teams[TEAM1].skin_index));
				Q_strncpyz(teams[TEAM2].skin_index, "i_ctf2", sizeof(teams[TEAM2].skin_index));
				Q_strncpyz(teams[TEAM3].skin_index, "i_pack", sizeof(teams[TEAM3].skin_index));
				//exit(1);
			}
			level.pic_teamskin[i] = gi.imageindex(teams[i].skin_index);
		}

		level.snd_lights = gi.soundindex("atl/lights.wav");
		level.snd_camera = gi.soundindex("atl/camera.wav");
		level.snd_action = gi.soundindex("atl/action.wav");
		level.snd_teamwins[0] = gi.soundindex("tng/no_team_wins.wav");
		level.snd_teamwins[1] = gi.soundindex("tng/team1_wins.wav");
		level.snd_teamwins[2] = gi.soundindex("tng/team2_wins.wav");
		level.snd_teamwins[3] = gi.soundindex("tng/team3_wins.wav");
	}

	level.snd_silencer = gi.soundindex("misc/silencer.wav");	// all silencer weapons
	level.snd_headshot = gi.soundindex("misc/headshot.wav");	// headshot sound
	level.snd_vesthit = gi.soundindex("misc/vest.wav");		// kevlar hit
	level.snd_knifethrow = gi.soundindex("misc/flyloop.wav");	// throwing knife
	level.snd_kick = gi.soundindex("weapons/kick.wav");	// not loaded by any item, kick sound
	level.snd_noammo = gi.soundindex("weapons/noammo.wav");

	gi.soundindex("tng/1_minute.wav");
	gi.soundindex("tng/3_minutes.wav");
	gi.soundindex("tng/1_frag.wav");
	gi.soundindex("tng/2_frags.wav");
	gi.soundindex("tng/3_frags.wav");
	gi.soundindex("tng/impressive.wav");
	gi.soundindex("tng/excellent.wav");
	gi.soundindex("tng/accuracy.wav");
	gi.soundindex("tng/clanwar.wav");
	gi.soundindex("tng/disabled.wav");
	gi.soundindex("tng/enabled.wav");
	gi.soundindex("misc/flashlight.wav"); // Caching Flashlight

	gi.soundindex("world/10_0.wav");	// countdown
	gi.soundindex("world/xian1.wav");	// intermission music
	gi.soundindex("misc/secret.wav");	// used for ctf swap sound
	gi.soundindex("weapons/grenlf1a.wav");	// respawn sound

	PrecacheItems();
	PrecacheRadioSounds();
	PrecacheUserSounds();

	TourneyInit();
	vInitLevel();

	if (!st.gravity)
		gi.cvar_set("sv_gravity", "800");
	else
		gi.cvar_set("sv_gravity", st.gravity);

	level.snd_fry = gi.soundindex("player/fry.wav");	// standing in lava / slime

	gi.soundindex("player/lava1.wav");
	gi.soundindex("player/lava2.wav");

	gi.soundindex("misc/pc_up.wav");
	gi.soundindex("misc/talk1.wav");

	gi.soundindex("misc/udeath.wav");
	gi.soundindex("misc/glurp.wav");

	// gibs
	gi.soundindex("items/respawn1.wav");

	// sexed sounds
	gi.soundindex("*death1.wav");
	gi.soundindex("*death2.wav");
	gi.soundindex("*death3.wav");
	gi.soundindex("*death4.wav");
	gi.soundindex("*fall1.wav");
	gi.soundindex("*fall2.wav");
	gi.soundindex("*gurp1.wav");	// drowning damage
	gi.soundindex("*gurp2.wav");
	//gi.soundindex("*jump1.wav");	// player jump - AQ2 doesn't use this
	gi.soundindex("*pain25_1.wav");
	gi.soundindex("*pain25_2.wav");
	gi.soundindex("*pain50_1.wav");
	gi.soundindex("*pain50_2.wav");
	gi.soundindex("*pain75_1.wav");
	gi.soundindex("*pain75_2.wav");
	gi.soundindex("*pain100_1.wav");
	gi.soundindex("*pain100_2.wav");

	//-------------------

	// precache vwep models
	// THIS ORDER MUST MATCH THE DEFINES IN g_local.h
	gi.modelindex("#w_mk23.md2");
	gi.modelindex("#w_mp5.md2");
	gi.modelindex("#w_m4.md2");
	gi.modelindex("#w_super90.md2");
	gi.modelindex("#w_cannon.md2");
	gi.modelindex("#w_sniper.md2");
	gi.modelindex("#w_akimbo.md2");
	gi.modelindex("#w_knife.md2");
	gi.modelindex("#a_m61frag.md2");

	level.model_null = gi.modelindex("sprites/null.sp2");      // null sprite
	level.model_lsight = gi.modelindex("sprites/lsight.sp2");  // laser sight dot sprite
	gi.soundindex("player/gasp1.wav");	// gasping for air
	gi.soundindex("player/gasp2.wav");	// head breaking surface, not gasping

	gi.soundindex("player/watr_in.wav");	// feet hitting water
	gi.soundindex("player/watr_out.wav");	// feet leaving water

	gi.soundindex("player/watr_un.wav");	// head going underwater

	gi.soundindex("player/u_breath1.wav");
	gi.soundindex("player/u_breath2.wav");

	gi.soundindex("items/pkup.wav");	// bonus item pickup
	gi.soundindex("world/land.wav");	// landing thud
	gi.soundindex("misc/h2ohit1.wav");	// landing splash

	gi.soundindex("items/damage.wav");
	gi.soundindex("items/protect.wav");
	gi.soundindex("items/protect4.wav");

	gi.soundindex("infantry/inflies1.wav");

	sm_meat_index = gi.modelindex("models/objects/gibs/sm_meat/tris.md2");
	gi.modelindex("models/objects/gibs/skull/tris.md2");


//
// Setup light animation tables. 'a' is total darkness, 'z' is doublebright.
//

	/*
		Darkmatch.
		Darkmatch has three settings:
		0 - normal, no changes to lights
		1 - all lights are off.
		2 - dusk/dawn mode.
		3 - using the day_cycle to change the lights every xx seconds as defined by day_cycle
	*/
	if (darkmatch->value == 1)
		gi.configstring (CS_LIGHTS + 0, "a");	// Pitch Black
	else if (darkmatch->value == 2)
		gi.configstring (CS_LIGHTS + 0, "b");	// Dusk
	else
		gi.configstring (CS_LIGHTS + 0, "m");	// 0 normal

	gi.configstring(CS_LIGHTS + 1, "mmnmmommommnonmmonqnmmo");	// 1 FLICKER (first variety)
	gi.configstring(CS_LIGHTS + 2, "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba");	// 2 SLOW STRONG PULSE
	gi.configstring(CS_LIGHTS + 3, "mmmmmaaaaammmmmaaaaaabcdefgabcdefg");	// 3 CANDLE (first variety)
	gi.configstring(CS_LIGHTS + 4, "mamamamamama");	// 4 FAST STROBE
	gi.configstring(CS_LIGHTS + 5, "jklmnopqrstuvwxyzyxwvutsrqponmlkj");	// 5 GENTLE PULSE 1
	gi.configstring(CS_LIGHTS + 6, "nmonqnmomnmomomno");	// 6 FLICKER (second variety)
	gi.configstring(CS_LIGHTS + 7, "mmmaaaabcdefgmmmmaaaammmaamm");	// 7 CANDLE (second variety)
	gi.configstring(CS_LIGHTS + 8, "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa");	// 8 CANDLE (third variety)
	gi.configstring(CS_LIGHTS + 9, "aaaaaaaazzzzzzzz");	// 9 SLOW STROBE (fourth variety)
	gi.configstring(CS_LIGHTS + 10, "mmamammmmammamamaaamammma");	// 10 FLUORESCENT FLICKER
	gi.configstring(CS_LIGHTS + 11, "abcdefghijklmnopqrrqponmlkjihgfedcba");	// 11 SLOW PULSE NOT FADE TO BLACK
	// styles 32-62 are assigned by the light program for switchable lights
	gi.configstring(CS_LIGHTS + 63, "a");	// 63 testing
}

int LoadFlagsFromFile (const char *mapname)
{
	FILE *fp;
	char buf[1024], *s;
	int flagCount = 0;
	edict_t *ent;
	vec3_t position;
	size_t length;

	Com_sprintf(buf, sizeof(buf), "%s/tng/%s.flg", GAMEVERSION, mapname);
	fp = fopen(buf, "r");
	if (!fp)  {
		gi.Com_PrintFmt("Warning: No flag definition file for map %s.\n", mapname);
		return 0;
	}

	// FIXME: remove this functionality completely in the future
	gi.Com_PrintFmt("Warning: .flg files are deprecated, use .ctf ones for more control!\n");

	while (fgets(buf, 1000, fp) != NULL)
	{
		length = strlen(buf);
		if (length < 7)
			continue;

		//first remove trailing spaces
		s = buf + length - 1;
		for (; *s && (*s == '\r' || *s == '\n' || *s == ' '); s--)
			*s = '\0';

		//check if it's a valid line
		length = strlen(buf);
		if (length < 7 || buf[0] == '#' || !strncmp(buf, "//", 2))
			continue;

		//a little bit dirty... :)
		if (sscanf(buf, "<%f %f %f>", &position[0], &position[1], &position[2]) != 3)
			continue;

		ent = G_Spawn ();

		ent->spawnflags &=
			~(SPAWNFLAG_NOT_EASY | SPAWNFLAG_NOT_MEDIUM | SPAWNFLAG_NOT_HARD |
			SPAWNFLAG_NOT_COOP | SPAWNFLAG_NOT_DEATHMATCH);

		VectorCopy(position, ent->s.origin);

		if (!flagCount)	// Red Flag
			ent->classname = ED_NewString ("item_flag_team1");
		else	// Blue Flag
			ent->classname = ED_NewString ("item_flag_team2");

		ED_CallSpawn (ent);
		flagCount++;
		if (flagCount == 2)
			break;
	}

	fclose(fp);

	if (flagCount < 2)
		return 0;

	return 1;
}

// This function changes the nearest two spawnpoint from each flag
// to info_player_teamX, so the other team won't restart
// beneath the flag of the other team
void ChangePlayerSpawns ()
{
	edict_t *flag1 = NULL, *flag2 = NULL;
	edict_t *spot, *spot1, *spot2, *spot3, *spot4;
	float range, range1, range2, range3, range4;

	range1 = range2 = range3 = range4 = 99999;
	spot = spot1 = spot2 = spot3 = spot4 = NULL;

	flag1 = G_Find (flag1, FOFS(classname), "item_flag_team1");
	flag2 = G_Find (flag2, FOFS(classname), "item_flag_team2");

	if(!flag1 || !flag2) {
		gi.Com_PrintFmt("Warning: ChangePlayerSpawns() requires both flags!\n");
		return;
	}

	while ((spot = G_Find(spot, FOFS(classname), "info_player_deathmatch")) != NULL)
	{
		range = Distance(spot->s.origin, flag1->s.origin);
		if (range < range1)
		{
			range3 = range1;
			spot3 = spot1;
			range1 = range;
			spot1 = spot;
		}
		else if (range < range3)
		{
			range3 = range;
			spot3 = spot;
		}

		range = Distance(spot->s.origin, flag2->s.origin);
		if (range < range2)
		{
			range4 = range2;
			spot4 = spot2;
			range2 = range;
			spot2 = spot;
		}
		else if (range < range4)
		{
			range4 = range;
			spot4 = spot;
		}
	}

	if (spot1)
	{
		// gi.Com_PrintFmt ("Ersetze info_player_deathmatch auf <%f %f %f> durch info_player_team1\n", spot1->s.origin[0], spot1->s.origin[1], spot1->s.origin[2]);
		strcpy (spot1->classname, "info_player_team1");
	}

	if (spot2)
	{
		// gi.Com_PrintFmt ("Ersetze info_player_deathmatch auf <%f %f %f> durch info_player_team2\n", spot2->s.origin[0], spot2->s.origin[1], spot2->s.origin[2]);
		strcpy (spot2->classname, "info_player_team2");
	}

	if (spot3)
	{
		// gi.Com_PrintFmt ("Ersetze info_player_deathmatch auf <%f %f %f> durch info_player_team1\n", spot3->s.origin[0], spot3->s.origin[1], spot3->s.origin[2]);
		strcpy (spot3->classname, "info_player_team1");
	}

	if (spot4)
	{
		// gi.Com_PrintFmt ("Ersetze info_player_deathmatch auf <%f %f %f> durch info_player_team2\n", spot4->s.origin[0], spot4->s.origin[1], spot4->s.origin[2]);
		strcpy (spot4->classname, "info_player_team2");
	}

}
