#include "g_local.h"
#include <time.h>

/*----------------------------------------
 * SP_LaserSight
 *
 * Create/remove the laser sight entity
 *---------------------------------------*/

void SP_LaserSight(edict_t *self, gitem_t *item)
{
	edict_t *lasersight = self->client->lasersight;
	action_item_num_t item = LASER_NUM;
	action_weapon_num_t weap;

	if (!INV_AMMO(self, item) || !self->client->weapon) {
		if (lasersight) {  // laser is on
			G_FreeEdict(lasersight);
			self->client->lasersight = NULL;
		}
		return;
	}
	//zucc code to make it be used with the right weapons

	//switch (self->client->weapon->typeNum) {
	switch (weap) {
	case MK23_NUM:
	case MP5_NUM:
	case M4_NUM:
		break;
	default:
		// laser is on but we want it off
		if (lasersight) {
			G_FreeEdict(lasersight);
			self->client->lasersight = NULL;
		}
		return;
	}

	if (lasersight) { //Lasersight is already on
		return;
	}

	lasersight = G_Spawn();
	self->client->lasersight = lasersight;
	lasersight->owner = self;
	lasersight->movetype = MOVETYPE_NOCLIP;
	lasersight->solid = SOLID_NOT;
	lasersight->classname = "lasersight";
	lasersight->s.modelindex = level.model_lsight;
	lasersight->s.renderfx = RF_TRANSLUCENT;
	lasersight->ideal_yaw = self->viewheight;
	lasersight->count = 0;
	lasersight->think = LaserSightThink;
	lasersight->nextthink = level.time + 10_hz;
	LaserSightThink( lasersight );
	VectorCopy( lasersight->s.origin, lasersight->s.old_origin );
	VectorCopy( lasersight->s.origin, lasersight->old_origin );
}

/*---------------------------------------------
 * LaserSightThink
 *
 * Updates the sights position, angle, and shape
 * is the lasersight entity
 *-------------------------------------------*/

void LaserSightThink(edict_t * self)
{
	vec3_t start, end, endp, offset;
	vec3_t forward, right, up, angles;
	trace_t tr;
	float viewheight = self->owner->viewheight;

	// zucc compensate for weapon ride up
	VectorAdd(self->owner->client->v_angle, self->owner->client->kick_angles, angles);
	AngleVectors(angles, forward, right, up);

	if (self->owner->client->lasersight != self) {
		self->think = G_FreeEdict;
	}

	if (self->owner->client->pers.firing_style == ACTION_FIRING_CLASSIC)
		viewheight -= 8;

	VectorSet(offset, 24, 8, viewheight);
	P_ProjectSource(self->owner->client, self->owner->s.origin, offset, forward, right, start);
	VectorMA(start, 8192, forward, end);

	PRETRACE();
	tr = gi.trace(start, NULL, NULL, end, self->owner, CONTENTS_SOLID | CONTENTS_MONSTER | CONTENTS_DEADMONSTER);
	POSTTRACE();

	if (tr.fraction != 1) {
		VectorMA(tr.endpos, -4, forward, endp);
		VectorCopy(endp, tr.endpos);
	}

	vectoangles(tr.plane.normal, self->s.angles);
	VectorCopy(tr.endpos, self->s.origin);

	self->s.modelindex = (tr.surface && (tr.surface->flags & SURF_SKY)) ? level.model_null : level.model_lsight;

	gi.linkentity(self);
	self->nextthink = level.time + 10_hz;
}

void Cmd_New_Reload_f(edict_t * ent)
{
//FB 6/1/99 - refuse to reload during LCA
	if (lights_camera_action)
		return;
	ent->client->reload_attempts++;
}

// Handles weapon reload requests
void Cmd_Reload_f(edict_t * ent)
{
	action_weapon_num_t weapNum;
	//+BD - If the player is dead, don't bother
	if (!IS_ALIVE(ent) || !ent->client->weapon)
		return;

	if (ent->client->weaponstate == WEAPON_BANDAGING ||
	    ent->client->bandaging == 1 ||
	    ent->client->bandage_stopped == 1 ||
	    ent->client->weaponstate == WEAPON_ACTIVATING ||
	    ent->client->weaponstate == WEAPON_DROPPING || ent->client->weaponstate == WEAPON_FIRING) {
		return;
	}

	if (!ent->client->fast_reload)
		ent->client->reload_attempts--;
	if (ent->client->reload_attempts < 0)
		ent->client->reload_attempts = 0;

	//First, grab the current magazine max count...

	//Set the weaponstate...
	switch(weapNum) {
	case M3_NUM:
		if (ent->client->shot_rds >= ent->client->shot_max)
			return;

		if(ent->client->inventory[ent->client->ammo_index] <= 0) {
			gi.Center_Print(ent, "Out of ammo\n");
			return;
		}
		// already in the process of reloading!
		if (ent->client->weaponstate == WEAPON_RELOADING &&
		    (ent->client->shot_rds < (ent->client->shot_max - 1)) &&
		    !(ent->client->fast_reload) &&
		    ((ent->client->inventory[ent->client->ammo_index] - 1) > 0)) {
			// don't let them start fast reloading until far enough into the firing sequence
			// this gives them a chance to break off from reloading to fire the weapon - zucc
			if (ent->client->ps.gunframe >= 48) {
				ent->client->fast_reload = 1;
				(ent->client->inventory[ent->client->ammo_index])--;
			} else {
				ent->client->reload_attempts++;
			}
		}
		break;
	case HC_NUM:
		if (ent->client->cannon_rds >= ent->client->cannon_max)
			return;

		if(ent->client->inventory[ent->client->ammo_index] <= 0) {
			gi.Center_Print(ent, "Out of ammo\n");
			return;
		}
		if(hc_single->value)
		{
			if(ent->client->pers.hc_mode || ent->client->cannon_rds == 1)
			{	if(ent->client->inventory[ent->client->ammo_index] < 1)
					return;
			}
			else if(ent->client->inventory[ent->client->ammo_index] < 2)
				return;
		}
		else if (ent->client->inventory[ent->client->ammo_index] < 2)
			return;
		break;
	case SNIPER_NUM:
		if (ent->client->sniper_rds >= ent->client->sniper_max)
			return;

		if(ent->client->inventory[ent->client->ammo_index] <= 0) {
			gi.Center_Print(ent, "Out of ammo\n");
			return;
		}
		// already in the process of reloading!
		if (ent->client->weaponstate == WEAPON_RELOADING
		    && (ent->client->sniper_rds < (ent->client->sniper_max - 1))
		    && !(ent->client->fast_reload)
		    && ((ent->client->inventory[ent->client->ammo_index] - 1) > 0)) {
			// don't let them start fast reloading until far enough into the firing sequence
			// this gives them a chance to break off from reloading to fire the weapon - zucc
			if (ent->client->ps.gunframe >= 72) {
				ent->client->fast_reload = 1;
				(ent->client->inventory[ent->client->ammo_index])--;
			} else {
				ent->client->reload_attempts++;
			}
		}
		ent->client->ps.fov = 90;
		if (ent->client->weapon)
			ent->client->ps.gunindex = gi.modelindex(ent->client->weapon->view_model);
		break;
	case DUAL_NUM:
		if (ent->client->dual_rds == ent->client->dual_max)
			return;

		if(ent->client->inventory[ent->client->ammo_index] <= 0) {
			gi.Center_Print(ent, "Out of ammo\n");
			return;
		}
		//TempFile change to pistol, then reload
		if (ent->client->inventory[ent->client->ammo_index] == 1) {
			gitem_t *it;

			it = GetItemByIndex(IT_WEAPON_MK23);
			it->use(ent, it);
			ent->client->autoreloading = true;
			return;
		}

		break;
	case MP5_NUM:
		if (ent->client->mp5_rds == ent->client->mp5_max)
			return;
		if(ent->client->inventory[ent->client->ammo_index] <= 0) {
			gi.Center_Print(ent, "Out of ammo\n");
			return;
		}
		break;
	case M4_NUM:
		if (ent->client->m4_rds == ent->client->m4_max)
			return;
		if(ent->client->inventory[ent->client->ammo_index] <= 0) {
			gi.Center_Print(ent, "Out of ammo\n");
			return;
		}
		break;
	case MK23_NUM:
		if (ent->client->mk23_rds == ent->client->mk23_max)
			return;
		if(ent->client->inventory[ent->client->ammo_index] <= 0) {
			gi.Center_Print(ent, "Out of ammo\n");
			return;
		}
		break;
	default:
	//We should never get here, but...
	//BD 5/26 - Actually we get here quite often right now. Just exit for weaps that we
	//          don't want reloaded or that never reload (grenades)
		return;
	}

	ent->client->weaponstate = WEAPON_RELOADING;
}

//+BD END CODE BLOCK

// zoom_comp
int calc_zoom_comp(edict_t * ent)
{
// Determine client ping to indicate how quickly we zoom from 0x to 1x
	int default_idle_frames = 6;
	int pingfloor = 80;
	int clping = ent->client->ping;

	int idle_weapon_frames = 0;
	int calc_idle_weapon_frames = round(clping / pingfloor);

	// No compensation if player ping is less than pingfloor
	// For every tier of pingfloor you reduce your frames by 1
	if (clping < pingfloor){
		idle_weapon_frames = default_idle_frames;
	} else if ((clping > pingfloor) && (clping <= (pingfloor * 2))){
		idle_weapon_frames = 5;
	} else if ((clping > (pingfloor * 2)) && (clping <= (pingfloor * 3))){
		idle_weapon_frames = 4;
	} else if ((clping > (pingfloor * 3))){
		idle_weapon_frames = 3;
	} else {
		// Somehow clping wasn't calculated correctly, default to be safe
		idle_weapon_frames = default_idle_frames;
	}

	return idle_weapon_frames;
}

//tempfile BEGIN
/*
   Function _SetSniper
   (move to g_weapon.c?)
   Arguments:
   ent: client edict for which to set sniper mode
   zoom: zoom level to set. Set to 1, 2, 4 or 6.

   Return value: none
 */
void _SetSniper(edict_t * ent, int zoom)
{
	int desired_fov, sniper_mode, oldmode;

	switch (zoom) {
	default:
	case 1:
		desired_fov = SNIPER_FOV1;
		sniper_mode = SNIPER_1X;
		break;
	case 2:
		desired_fov = SNIPER_FOV2;
		sniper_mode = SNIPER_2X;
		break;
	case 4:
		desired_fov = SNIPER_FOV4;
		sniper_mode = SNIPER_4X;
		break;
	case 6:
		desired_fov = SNIPER_FOV6;
		sniper_mode = SNIPER_6X;
		break;
	}

	oldmode = ent->client->resp.sniper_mode;

	if (sniper_mode == oldmode)
		return;

	//Moved here, no need to make sound if zoom isnt changed -M
	gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/lensflik.wav"), 1, ATTN_NORM, 0);

	ent->client->resp.sniper_mode = sniper_mode;
	ent->client->desired_fov = desired_fov;

	if (sniper_mode == SNIPER_1X && ent->client->weapon)
		ent->client->ps.gunindex = gi.modelindex(ent->client->weapon->view_model);
	//show the model if switching to 1x

	if (oldmode == SNIPER_1X && ent->client->weaponstate != WEAPON_RELOADING) {
		//do idleness stuff when switching from 1x, see function below
		ent->client->weaponstate = WEAPON_BUSY;
		if(zoom_comp->value) {
			ent->client->idle_weapon = calc_zoom_comp(ent);
		} else {
			ent->client->idle_weapon = 6;
		}
		ent->client->ps.gunframe = 22;
	}
}

//tempfile END

void Cmd_New_Weapon_f(edict_t * ent)
{
	ent->client->weapon_attempts++;
	if (ent->client->weapon_attempts == 1)
		Cmd_Weapon_f(ent);
}


int _SniperMode(edict_t *ent)
{
	switch (ent->client->desired_zoom) { //lets update old desired zoom
	case 1:
		return SNIPER_1X;
	case 2:
		return SNIPER_2X;
	case 4:
		return SNIPER_4X;
	case 6:
		return SNIPER_6X;
	}
	return ent->client->resp.sniper_mode;
}
//TempFile BEGIN

void _ZoomIn(edict_t * ent, bool overflow)
{
	switch (_SniperMode(ent)) {
	case SNIPER_1X:
		ent->client->desired_zoom = 2;
		break;
	case SNIPER_2X:
		ent->client->desired_zoom = 4;
		break;
	case SNIPER_4X:
		ent->client->desired_zoom = 6;
		break;
	case SNIPER_6X:
		if (overflow)
			ent->client->desired_zoom = 1;
		break;
	}
}

void _ZoomOut(edict_t * ent, bool overflow)
{
	switch (_SniperMode(ent)) {
	case SNIPER_1X:
		if (overflow)
			ent->client->desired_zoom = 6;
		break;
	case SNIPER_2X:
		ent->client->desired_zoom = 1;
		break;
	case SNIPER_4X:
		ent->client->desired_zoom = 2;
		break;
	case SNIPER_6X:
		ent->client->desired_zoom = 4;
		break;
	}
}

void Cmd_NextMap_f(edict_t * ent)
{
	int map_num = -1;
	const char *next_map = level.nextmap;
	const char *rot_type = "in rotation";
	votelist_t *voted = NULL;

	if( next_map[0] )
		rot_type = "selected";
	else if( vrot->value && ((voted = MapWithMostAllVotes())) )
	{
		next_map = voted->mapname;
		rot_type = "by votes";
	}

	if( next_map[0] )
	{
		int i;
		for( i = 0; i < num_maps; i ++ )
		{
			if( Q_stricmp( map_rotation[i], next_map ) == 0 )
			{
				map_num = i;
				break;
			}
		}
	}
	else if( num_maps )
	{
		map_num = (cur_map + (rrot->value ? rand_map : 1)) % num_maps;
		next_map = map_rotation[ map_num ];
		rot_type = rrot->value ? "randomly" : "in rotation";
	}

	if( g_dm_same_level->integer )
	{
		map_num = cur_map;
		next_map = level.mapname;
		rot_type = "repeated";
	}

	gi.LocClient_Print( ent, PRINT_HIGH, "Next map %s is %s (%i/%i).\n", rot_type, next_map, map_num+1, num_maps );
}

void Cmd_Lens_f(edict_t * ent)
{
	int nArg;
	char args[8];

	if (!ent->client->weapon || ent->client->weapon->typeNum != SNIPER_NUM)
		return;

	nArg = atoi(gi.args());

	if (nArg == 0) {
		Q_strncpyz(args, gi.args(), sizeof(args));
		//perhaps in or out? let's see.
		if (Q_stricmp(args, "in") == 0)
			_ZoomIn(ent, false);
		else if (Q_stricmp(args, "out") == 0)
			_ZoomOut(ent, false);
		else
			_ZoomIn(ent, true);

		if(!ent->client->desired_zoom)
			return;
	}
	else if ((nArg == 1) || (!(nArg % 2) && (nArg <= 6)))
		ent->client->desired_zoom = nArg;
	else
		_ZoomIn(ent, true);

	if(ent->client->weapon_attempts > 0)
		return; //Already waiting to change the zoom, otherwise it
				//first change to desired zoom and then usual zoomin -M

	ent->client->weapon_attempts++;
	if (ent->client->weapon_attempts == 1)
		Cmd_Weapon_f(ent);
}

//TempFile END

// function to change the firing mode of weapons (when appropriate) 

void Cmd_Weapon_f(edict_t * ent)
{
	int dead;
	action_weapon_num_t weap;

	if (!ent->client->weapon)
		return;

	dead = !IS_ALIVE(ent);

	ent->client->weapon_attempts--;
	if (ent->client->weapon_attempts < 0)
		ent->client->weapon_attempts = 0;

	if (ent->client->bandaging || ent->client->bandage_stopped) {
		if (!ent->client->weapon_after_bandage_warned) {
			ent->client->weapon_after_bandage_warned = true;
			gi.Center_Print(ent, "You'll get to your weapon when you're done bandaging!\n");
		}
		ent->client->weapon_attempts++;
		return;
	}

	ent->client->weapon_after_bandage_warned = false;

	if (ent->client->weaponstate == WEAPON_FIRING || ent->client->weaponstate == WEAPON_BUSY)
	{
		//gi.Center_Print(ent, "Try again when you aren't using your weapon.\n");
		ent->client->weapon_attempts++;
		return;
	}

	switch(weap) {
	case MK23_NUM:
		if (!dead)
			gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/click.wav"), 1, ATTN_NORM, 0);
		ent->client->pers.mk23_mode = !(ent->client->pers.mk23_mode);
		if (ent->client->pers.mk23_mode)
			gi.Center_Print(ent, "MK23 Pistol set for semi-automatic action\n");
		else
			gi.Center_Print(ent, "MK23 Pistol set for automatic action\n");
		break;
	case MP5_NUM:
		if (!dead)
			gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/click.wav"), 1, ATTN_NORM, 0);
		ent->client->pers.mp5_mode = !(ent->client->pers.mp5_mode);
		if (ent->client->pers.mp5_mode)
			gi.Center_Print(ent, "MP5 set to 3 Round Burst mode\n");
		else
			gi.Center_Print(ent, "MP5 set to Full Automatic mode\n");
		break;
	case M4_NUM:
		if (!dead)
			gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/click.wav"), 1, ATTN_NORM, 0);
		ent->client->pers.m4_mode = !(ent->client->pers.m4_mode);
		if (ent->client->pers.m4_mode)
			gi.Center_Print(ent, "M4 set to 3 Round Burst mode\n");
		else
			gi.Center_Print(ent, "M4 set to Full Automatic mode\n");
		break;
	case SNIPER_NUM:
		if (dead)
			return;
		
		if (!ent->client->desired_zoom)
			_ZoomIn(ent, true);	// standard behaviour

		_SetSniper(ent, ent->client->desired_zoom);
		ent->client->desired_zoom = 0;
		break;
	case HC_NUM:
		// AQ2:TNG Deathwatch - Single Barreled HC
		if(!hc_single->value)
			return;

		if (!dead)
			gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/click.wav"), 1, ATTN_NORM, 0);

		ent->client->pers.hc_mode = !(ent->client->pers.hc_mode);
		if (ent->client->pers.hc_mode)
			gi.Center_Print(ent, "Single Barreled Handcannon\n");
		else
			gi.Center_Print(ent, "Double Barreled Handcannon\n");
		// AQ2:TNG End
		break;
	case KNIFE_NUM:
		if (dead)
			return;
		if (ent->client->weaponstate == WEAPON_READY) {
			ent->client->pers.knife_mode = !(ent->client->pers.knife_mode);
			ent->client->weaponstate = WEAPON_ACTIVATING;
			if (ent->client->pers.knife_mode) {
				gi.Center_Print(ent, "Switching to throwing\n");
				ent->client->ps.gunframe = 0;
			} else {
				gi.Center_Print(ent, "Switching to slashing\n");
				ent->client->ps.gunframe = 106;
			}
		}
		break;
	case GRENADE_NUM:
		if (ent->client->pers.grenade_mode == 0) {
			gi.Center_Print(ent, "Prepared to make a medium range throw\n");
			ent->client->pers.grenade_mode = 1;
		} else if (ent->client->pers.grenade_mode == 1) {
			gi.Center_Print(ent, "Prepared to make a long range throw\n");
			ent->client->pers.grenade_mode = 2;
		} else {
			gi.Center_Print(ent, "Prepared to make a short range throw\n");
			ent->client->pers.grenade_mode = 0;
		}
		break;
	}
}

// sets variable to toggle nearby door status
void Cmd_OpenDoor_f(edict_t * ent)
{
	ent->client->doortoggle = 1;
	return;
}

void Cmd_Bandage_f(edict_t *ent)
{
	if (!IS_ALIVE(ent))
		return;

	if (ent->client->bandaging) {
		gi.Center_Print(ent, "Already bandaging\n");
		return;
	}

	bool can_use_medkit = (ent->client->medkit > 0) && (ent->health < ent->max_health);

	if (ent->client->bleeding == 0 && ent->client->leg_damage == 0 && ! can_use_medkit) {
		gi.Center_Print(ent, "No need to bandage\n");
		return;
	}

	ent->client->reload_attempts = 0;	// prevent any further reloading

	if (ent->client->weaponstate != WEAPON_READY && ent->client->weaponstate != WEAPON_END_MAG) {
		gi.Center_Print(ent, "Can't bandage now\n");
		return;
	}

	// zucc - check if they have a primed grenade
	if (ent->client->curr_weap == GRENADE_NUM
		&& ((ent->client->ps.gunframe >= GRENADE_IDLE_FIRST
			&& ent->client->ps.gunframe <= GRENADE_IDLE_LAST)
		|| (ent->client->ps.gunframe >= GRENADE_THROW_FIRST
			&& ent->client->ps.gunframe <= GRENADE_THROW_LAST)))
	{
		int damage;

		ent->client->ps.gunframe = 0;

		if (use_classic->value)
			damage = 170;
		else
			damage = GRENADE_DAMRAD;
			
		if(ent->client->quad_time > level.time + 1_sec)
			damage *= 1.5f;

		fire_grenade2(ent, ent->s.origin, vec3_origin, damage, 0, 2_sec, damage * 2, false);

		INV_AMMO(ent, GRENADE_NUM)--;
		if (INV_AMMO(ent, GRENADE_NUM) <= 0) {
			ent->client->newweapon = GetItemByIndex(IT_WEAPON_MK23);
		}
	}

	ent->client->bandaging = 1;
	ent->client->resp.sniper_mode = SNIPER_1X;
	ent->client->ps.fov = 90;
	ent->client->desired_fov = 90;
	if (ent->client->weapon)
		ent->client->ps.gunindex = gi.modelindex(ent->client->weapon->view_model);
	gi.Center_Print(ent, "You've started bandaging\n");
}

// function called in generic_weapon function that does the bandaging

void Bandage(edict_t * ent)
{
	ClientFixLegs(ent);
	ent->client->bleeding = 0;
	ent->client->bleed_remain = 0;
	ent->client->bandaging = 0;
	ent->client->attacker = NULL;
	ent->client->bandage_stopped = 1;
	if (esp->value && esp_leaderenhance->value)
		ent->client->idle_weapon = ENHANCED_BANDAGE_TIME;
	else
		ent->client->idle_weapon = BANDAGE_TIME;
}

void Cmd_ID_f(edict_t * ent)
{
	if (!ent->client->pers.id) {
		gi.Center_Print(ent, "Disabling player identification display.\n");
		ent->client->pers.id = 1;
	} else {
		gi.Center_Print(ent, "Activating player identification display.\n");
		ent->client->pers.id = 0;
	}
	return;
}

// originally from Zoid's CTF
void SetIDView(edict_t * ent)
{
	vec3_t forward, dir;
	trace_t tr;
	edict_t *who, *best;
//FIREBLADE, suggested by hal[9k]  3/11/1999
	float bd = 0.9f;
	float d;
    int i;

	ent->client->ps.stats[STAT_ID_VIEW] = 0;

//FIREBLADE
	//https://github.com/actionquake/quake2-rerelease-dll/issues/4
	// if (ent->solid != SOLID_NOT && !teamplay->value) {
	// 	if (!DMFLAGS( (DF_MODELTEAMS | DF_SKINTEAMS) ))
	// 		return;	// won't ever work in non-teams so don't run the code...

	// }

	if (ent->client->chase_mode) {
		if (ent->client->chase_target && ent->client->chase_target->inuse) {
			ent->client->ps.stats[STAT_ID_VIEW] =
			    CS_PLAYERSKINS + (ent->client->chase_target - g_edicts - 1);
		}
		return;
	}
//FIREBLADE

	if (ent->client->pers.id == 1)
		return;

	AngleVectors(ent->client->v_angle, forward, NULL, NULL);
	VectorScale(forward, 8192, forward);
	VectorAdd(ent->s.origin, forward, forward);
	PRETRACE();
	tr = gi.trace(ent->s.origin, NULL, NULL, forward, ent, MASK_SOLID);
	POSTTRACE();
	if (tr.fraction < 1 && tr.ent && tr.ent->client) {
		ent->client->ps.stats[STAT_ID_VIEW] = CS_PLAYERSKINS + (ent - g_edicts - 1);
		return;
	}

	AngleVectors(ent->client->v_angle, forward, NULL, NULL);
	best = NULL;
	for (i = 1; i <= game.maxclients; i++) {
		who = g_edicts + i;
		if (!who->inuse)
			continue;

		if ((who->solid == SOLID_NOT && who->deadflag) ||
			(ent->solid != SOLID_NOT && !OnSameTeam(ent, who)))
			continue;

		VectorSubtract(who->s.origin, ent->s.origin, dir);
		VectorNormalize(dir);
		d = DotProduct(forward, dir);
		if (d > bd && visible(ent, who, MASK_SOLID)) {
			bd = d;
			best = who;
		}
	}
	if (best != NULL && bd > 0.90) {
		ent->client->ps.stats[STAT_ID_VIEW] = CS_PLAYERSKINS + (best - g_edicts - 1);
	}
}

void Cmd_IR_f(edict_t * ent)
{
	int band = 0;

	if (!ir->value) {
		gi.Center_Print(ent, "IR vision not enabled on this server.\n");
		return;
	}
	if (INV_AMMO(ent, BAND_NUM))
		band = 1;

	ent->client->pers.irvision = !ent->client->pers.irvision;
	if (ent->client->pers.irvision == 0)
	{
		if (band)
			gi.Center_Print(ent, "IR vision disabled.\n");
		else
			gi.Center_Print(ent, "IR vision will be disabled when you get a bandolier.\n");
	}
	else
	{
		if (band)
			gi.Center_Print(ent, "IR vision enabled.\n");
		else
			gi.Center_Print(ent, "IR vision will be enabled when you get a bandolier.\n");
	}
}

// zucc choose command, avoids using the menus in teamplay
int GetWeaponNumFromArg(const char *s)
{
	int i, itemNum;
	gitem_t *item;

	if (!s || !*s)
		return 0;

	for (i = 0; i < WEAPON_MAX - 1; i++) {
		itemNum = MK23_NUM + i;
		item = GetItemByIndex(itemNum);
		if (item->pickup_name && !strcmp(s, item->pickup_name))
			return itemNum;
	}

	itemNum = 0;
	// convert names a player might try (DW added a few)
	if (!strcmp(s, "mark 23 pistol"))
		itemNum = MK23_NUM;
	else if (!strcmp(s, "A 2nd pistol") || !strcmp(s, "akimbo"))
		itemNum = DUAL_NUM;
	else if (!strcmp(s, "shotgun"))
		itemNum = M3_NUM;
	else if (!strcmp(s, "mp5"))
		itemNum = MP5_NUM;
	else if (!strcmp(s, "sniper"))
		itemNum = SNIPER_NUM;
	else if (!strcmp(s, "m4"))
		itemNum = M4_NUM;

	return itemNum;
}

int GetItemNumFromArg(const char *s)
{
	int i, itemNum;
	gitem_t *item;

	if (!s || !*s)
		return 0;

	for (i = 0; i < ITEM_MAX - 1; i++) {
		itemNum = NO_ITEM_NUM + i;
		item = GetItemByIndex(itemNum);
		if (item->pickup_name && !strcmp(s, item->pickup_name))
			return itemNum;
	}

	itemNum = 0;
	if (!strcmp(s, "laser"))
		itemNum = LASER_NUM;
	else if (!strcmp(s, "vest"))
		itemNum = KEV_NUM;
	else if (!strcmp(s, "slippers"))
		itemNum = SLIP_NUM;
	else if (!strcmp(s, "helmet"))
		itemNum = HELM_NUM;

	return itemNum;
}


void Cmd_Choose_f(edict_t * ent)
{
	char *s, *wpnText, *itmText;
	//int itemNum = 0;
	//gitem_t *item;
	action_weapon_num_t weapNum;
	action_item_num_t itemNum;
	action_itemkit_num_t itemKitNum;

	// only works in teamplay
	if (!(gameSettings & GS_WEAPONCHOOSE))
		return;

	s = gi.args();
	if (*s) {
		itemNum = GetItemNumFromArg(s);
		if (!itemNum)
			itemNum = GetWeaponNumFromArg(s);
	}

	switch(weapNum) {
	case DUAL_NUM:
	case M3_NUM:
	case HC_NUM:
	case MP5_NUM:
	case SNIPER_NUM:
	case KNIFE_NUM:
	case M4_NUM:
		// TODO: Fix weapon bans
		// if (!WPF_ALLOWED(itemNum)) {
		// 	gi.Center_Print(ent, "Weapon disabled on this server.\n");
		// 	return;
		// }
		ent->client->pers.chosenWeapon = GetItemByIndex(itemNum);
		break;
	}

	switch(itemNum) {
	case LASER_NUM:
	case KEV_NUM:
	case SLIP_NUM:
	case SIL_NUM:
	case HELM_NUM:
	case BAND_NUM:
		// TODO: Fix item bans
		// if (!ITF_ALLOWED(itemNum)) {
		// 	gi.Center_Print(ent, "Item disabled on this server.\n");
		// 	return;
		// }
		ent->client->pers.chosenItem = GetItemByIndex(itemNum);
		break;
	}

	switch(itemKitNum) {
	case C_KIT_NUM:
	case S_KIT_NUM:
	case A_KIT_NUM:
	default:
		gi.Center_Print(ent, "Invalid weapon or item choice.\n");
		return;
	}

	itemNum = ent->client->pers.chosenWeapon;
	wpnText = (weapNum && weapNum->pickup_name) ? item->pickup_name : "NONE";

	item = ent->client->pers.chosenItem;
	itmText = (item && item->pickup_name) ? item->pickup_name : "NONE";

	if (item_kit_mode->value) {
		if (itemNum == C_KIT_NUM){
			itmText = "Commando Kit (Bandolier + Kevlar Helmet)";
		} else if (itemNum == A_KIT_NUM){
			itmText = "Assassin Kit (Laser Sight + Silencer)";
		} else if (itemNum == S_KIT_NUM){
			if (e_enhancedSlippers->value){
				itmText = "Stealth Kit (Enhanced Stealth Slippers + Silencer)";
			} else {
				itmText = "Stealth Kit (Stealth Slippers + Silencer)";
			}
		} else {
			// How did you pick a kit not on the list?
			itmText = "NONE";
		}
		
		gi.LocClient_Print(ent, PRINT_HIGH, "Weapon selected: %s\nItem kit selected: %s\n", wpnText, itmText );
	} else {
		gi.LocClient_Print(ent, PRINT_HIGH, "Weapon selected: %s\nItem selected: %s\n", wpnText, itmText );
	}
}

// AQ:TNG - JBravo adding tkok
void Cmd_TKOk(edict_t * ent)
{
	if (!ent->enemy || !ent->enemy->inuse || !ent->enemy->client || (ent == ent->enemy)) {
		gi.Center_Print(ent, "Nothing to forgive\n");
	} else if (ent->client->resp.team == ent->enemy->client->resp.team) {
		if (ent->enemy->client->resp.team_kills) {
			gi.LocCenter_Print(ent, "You forgave %s\n", ent->enemy->client->pers.netname);
			gi.LocCenter_Print(ent->enemy, "%s forgave you\n", ent->client->pers.netname);
			ent->enemy->client->resp.team_kills--;
			if (ent->enemy->client->resp.team_wounds)
				ent->enemy->client->resp.team_wounds /= 2;
		}
	} else {
		gi.Client_Print(ent, PRINT_HIGH, "That's very noble of you...\n");
		gi.LocBroadcast_Print(PRINT_HIGH, "%s turned the other cheek\n", ent->client->pers.netname);
	}
	ent->enemy = NULL;
	return;
}

void Cmd_FF_f( edict_t *ent )
{
	if( teamplay->value )
		gi.LocClient_Print( ent, PRINT_MEDIUM, "Friendly Fire %s\n", g_friendly_fire->integer ? "OFF": "ON" );
	else
		gi.LocClient_Print( ent, PRINT_MEDIUM, "FF only applies to teamplay.\n" );
}

void Cmd_Time(edict_t * ent)
{
	int mins = 0, secs = 0, remaining = 0, rmins = 0, rsecs = 0, gametime = 0;

	gametime = level.matchTime;

	mins = gametime / 60;
	secs = gametime % 60;
	remaining = (timelimit->value * 60) - gametime;
	if( remaining >= 0 )
	{
		rmins = remaining / 60;
		rsecs = remaining % 60;
	}

	if( timelimit->value )
		gi.LocClient_Print( ent, PRINT_HIGH, "Elapsed time: %d:%02d. Remaining time: %d:%02d\n", mins, secs, rmins, rsecs );
	else
		gi.LocClient_Print( ent, PRINT_HIGH, "Elapsed time: %d:%02d\n", mins, secs );
}

void Cmd_Roundtimeleft_f(edict_t * ent)
{
	int remaining;

	if(!teamplay->value) {
		gi.Center_Print(ent, "This command need teamplay to be enabled\n");
		return;
	}

	if (!(gameSettings & GS_ROUNDBASED) || !team_round_going)
		return;

	if ((int)roundtimelimit->value <= 0)
		return;

	remaining = (roundtimelimit->value * 60) - (current_round_length/10);
	gi.LocClient_Print(ent, "There is %d:%02i left in this round\n", remaining / 60, remaining % 60);
}

/*
	Freud's AutoRecord
	This will automatically record a new demo for every map if the client turns it on
*/
void RemoveSpaces(char *s)
{
	char *p;

	p = strchr(s, ' ');
	if(!p)
		return;

	for(s = p+1; *s; s++)
	{
		if(*s != ' ')
			*p++ = *s;
	}
	*p = 0;
}

void Cmd_AutoRecord_f(edict_t * ent)
{
	char rec_date[20], recstr[MAX_QPATH];
	time_t clock;
	char *stuffmsg;

	time( &clock );
	strftime( rec_date, sizeof(rec_date)-1, "%Y_%b_%d_%H%M", localtime(&clock));

	if (matchmode->value)
	{
		if(teamCount == 3)
			Com_sprintf(recstr, sizeof(recstr), "%s-%s_vs_%s_vs_%s-%s", rec_date, teams[TEAM1].name, teams[TEAM2].name, teams[TEAM3].name, level.mapname);
		else
			Com_sprintf(recstr, sizeof(recstr), "%s-%s_vs_%s-%s", rec_date, teams[TEAM1].name, teams[TEAM2].name, level.mapname);

		RemoveSpaces(recstr); //Remove spaces -M
	} else {
		Com_sprintf(recstr, sizeof(recstr), "%s-%s", rec_date, level.mapname);
	}

	Com_sprintf(stuffmsg, sizeof(stuffmsg), "record \"%s\"\n", recstr);
	stuffcmd(ent, stuffmsg);
	//stuffcmd(ent, va("record \"%s\"\n", recstr));
}

/*
TNG:Freud
Cmd_Ghost_f

Person gets frags/kills/damage/weapon/item/team/stats back if he disconnected
*/

//TODO: Reenable at some point?
// void Cmd_Ghost_f(edict_t * ent)
// {
// 	int i = 0, frames_since = 0;
// 	gghost_t *ghost = NULL;

// 	if (!use_ghosts->value) {
// 		gi.Center_Print(ent, "Ghosting is not enabled on this server\n");
// 		return;
// 	}

// 	if (num_ghost_players == 0) {
// 		gi.Center_Print(ent, "No ghost match found\n");
// 		return;
// 	}

// 	for (i = 0, ghost = ghost_players; i < num_ghost_players; i++, ghost++) {
// 		if (!strcmp(ghost->ip, ent->client->pers.ip) && !strcmp(ghost->netname, ent->client->pers.netname)) {
// 			break;
// 		}
// 	}

// 	if (i >= num_ghost_players) {
// 		gi.Client_Print( ent, PRINT_HIGH, "No ghost match found\n" );
// 		return;
// 	}

// 	gi.Client_Print( ent, PRINT_HIGH, "Welcome back %s\n", ent->client->pers.netname );
// 	frames_since = level.framenum - ghost->disconnect_frame;
// 	ent->client->resp.enterframe = ghost->enterframe + frames_since;
// 	ent->client->resp.score = ghost->score;
// 	ent->client->resp.kills = ghost->kills;
// 	ent->client->resp.deaths = ghost->deaths;
// 	ent->client->resp.damage_dealt = ghost->damage_dealt;
// 	ent->client->resp.ctf_caps = ghost->ctf_caps;

// 	if (teamplay->value && ghost->team && ghost->team != ent->client->resp.team)
// 			JoinTeam( ent, ghost->team, 1 );

// 	if (gameSettings & GS_WEAPONCHOOSE) {
// 		if (ghost->weapon)
// 			ent->client->pers.chosenWeapon = ghost->weapon;

// 		if (ghost->item)
// 			ent->client->pers.chosenItem = ghost->item;
// 	}

// 	ent->client->resp.shotsTotal = ghost->shotsTotal;
// 	ent->client->resp.hitsTotal = ghost->hitsTotal;

// 	memcpy(ent->client->resp.hitsLocations, ghost->hitsLocations, sizeof(ent->client->resp.hitsLocations));
// 	memcpy(ent->client->resp.gunstats, ghost->gunstats, sizeof(ent->client->resp.gunstats));

// 	//Remove it from the list
// 	for (i += 1; i < num_ghost_players; i++) {
// 		ghost_players[i - 1] = ghost_players[i];
// 	}
// 	num_ghost_players--;
// }

void Cmd_Volunteer_f(edict_t * ent)
{
	int teamNum;
	edict_t *oldLeader;

	// Ignore if not Espionage mode
	if (!esp->value) {
		gi.Center_Print(ent, "This command needs Espionage to be enabled\n");
		return;
	}

	// Ignore entity if not on a team
	teamNum = ent->client->resp.team;
	if (teamNum == NOTEAM) {
		gi.Center_Print(ent, "You need to be on a team for that...\n");
		return;
	}

	// Ignore entity if they are a sub
	if (ent->client->resp.subteam == teamNum) {
		gi.Center_Print(ent, "Subs cannot be leaders...\n");
		return;
	}

	// If the current leader is issuing this command again, remove them as leader
	oldLeader = teams[teamNum].leader;
	if (oldLeader == ent) {
		EspSetLeader( teamNum, NULL );
		return;
	}

	// If the team already has a leader, send this message to the ent volunteering
	if (oldLeader) {
		gi.Client_Print( ent, "Your team already has a leader (%s)\n",
			teams[teamNum].leader->client->pers.netname );
		return;
	}

	EspSetLeader( teamNum, ent );
}