// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// g_weapon.c

#include "g_local.h"
#include "m_player.h"

bool is_quad;
// RAFAEL
bool is_quadfire;
// RAFAEL
player_muzzle_t is_silenced;

// PGM
byte damage_multiplier;
// PGM

void weapon_grenade_fire(edict_t *ent, bool held);
// RAFAEL
void weapon_trap_fire(edict_t *ent, bool held);
// RAFAEL

//========
// [Kex]
bool G_CheckInfiniteAmmo(gitem_t *item)
{
	if (item->flags & IF_NO_INFINITE_AMMO)
		return false;

	return g_infinite_ammo->integer || g_instagib->integer;
}

//========
// ROGUE
byte P_DamageModifier(edict_t *ent)
{
	is_quad = 0;
	damage_multiplier = 1;

	if (ent->client->quad_time > level.time)
	{
		damage_multiplier *= 4;
		is_quad = 1;

		// if we're quad and DF_NO_STACK_DOUBLE is on, return now.
		if (g_dm_no_stack_double->integer)
			return damage_multiplier;
	}

	if (ent->client->double_time > level.time)
	{
		damage_multiplier *= 2;
		is_quad = 1;
	}

	return damage_multiplier;
}
// ROGUE
//========

// [Paril-KEX] kicks in vanilla take place over 2 10hz server
// frames; this is to mimic that visual behavior on any tickrate.
inline float P_CurrentKickFactor(edict_t *ent)
{
	if (ent->client->kick.time < level.time)
		return 0.f;

	float f = (ent->client->kick.time - level.time).seconds() / ent->client->kick.total.seconds();
	return f;
}

// [Paril-KEX]
vec3_t P_CurrentKickAngles(edict_t *ent)
{
	return ent->client->kick.angles * P_CurrentKickFactor(ent);
}

vec3_t P_CurrentKickOrigin(edict_t *ent)
{
	return ent->client->kick.origin * P_CurrentKickFactor(ent);
}

void P_AddWeaponKick(edict_t *ent, const vec3_t &origin, const vec3_t &angles)
{
	ent->client->kick.origin = origin;
	ent->client->kick.angles = angles;
	ent->client->kick.total = 200_ms;
	ent->client->kick.time = level.time + ent->client->kick.total;
}

void P_ProjectSource(edict_t *ent, const vec3_t &angles, vec3_t distance, vec3_t &result_start, vec3_t &result_dir)
{
	if (ent->client->pers.hand == LEFT_HANDED)
		distance[1] *= -1;
	else if (ent->client->pers.hand == CENTER_HANDED)
		distance[1] = 0;

	vec3_t forward, right, up;
	vec3_t eye_position = (ent->s.origin + vec3_t{ 0, 0, (float) ent->viewheight });

	AngleVectors(angles, forward, right, up);

	result_start = G_ProjectSource2(eye_position, distance, forward, right, up);

	vec3_t	   end = eye_position + forward * 8192;
	contents_t mask = MASK_PROJECTILE & ~CONTENTS_DEADMONSTER;

	// [Paril-KEX]
	if (!G_ShouldPlayersCollide(true))
		mask &= ~CONTENTS_PLAYER;

	trace_t tr = gi.traceline(eye_position, end, ent, mask);

	// if the point was a monster & close to us, use raw forward
	// so railgun pierces properly
	if (tr.startsolid || ((tr.contents & (CONTENTS_MONSTER | CONTENTS_PLAYER)) && (tr.fraction * 8192.f) < 128.f))
		result_dir = forward;
	else
	{
		end = tr.endpos;
		result_dir = (end - result_start).normalized();

#if 0
		// correction for blocked shots
		trace_t eye_tr = gi.traceline(result_start, result_start + (result_dir * tr.fraction * 8192.f), ent, mask);

		if ((eye_tr.endpos - tr.endpos).length() > 32.f)
		{
			result_start = eye_position;
			result_dir = (end - result_start).normalized();
			return;
		}
#endif
	}
}

/*
===============
PlayerNoise

Each player can have two noise objects associated with it:
a personal noise (jumping, pain, weapon firing), and a weapon
target noise (bullet wall impacts)

Monsters that don't directly see the player can move
to a noise in hopes of seeing the player from there.
===============
*/
void PlayerNoise(edict_t *who, const vec3_t &where, player_noise_t type)
{
	edict_t *noise;

	if (type == PNOISE_WEAPON)
	{
		if (who->client->silencer_shots)
			who->client->invisibility_fade_time = level.time + (INVISIBILITY_TIME / 5);
		else
			who->client->invisibility_fade_time = level.time + INVISIBILITY_TIME;

		if (who->client->silencer_shots)
		{
			who->client->silencer_shots--;
			return;
		}
	}

	if (deathmatch->integer)
		return;

	if (who->flags & FL_NOTARGET)
		return;

	if (type == PNOISE_SELF &&
		(who->client->landmark_free_fall || who->client->landmark_noise_time >= level.time))
		return;

	// ROGUE
	if (who->flags & FL_DISGUISED)
	{
		if (type == PNOISE_WEAPON)
		{
			level.disguise_violator = who;
			level.disguise_violation_time = level.time + 500_ms;
		}
		else
			return;
	}
	// ROGUE

	if (!who->mynoise)
	{
		noise = G_Spawn();
		noise->classname = "player_noise";
		noise->mins = { -8, -8, -8 };
		noise->maxs = { 8, 8, 8 };
		noise->owner = who;
		noise->svflags = SVF_NOCLIENT;
		who->mynoise = noise;

		noise = G_Spawn();
		noise->classname = "player_noise";
		noise->mins = { -8, -8, -8 };
		noise->maxs = { 8, 8, 8 };
		noise->owner = who;
		noise->svflags = SVF_NOCLIENT;
		who->mynoise2 = noise;
	}

	if (type == PNOISE_SELF || type == PNOISE_WEAPON)
	{
		noise = who->mynoise;
		who->client->sound_entity = noise;
		who->client->sound_entity_time = level.time;
	}
	else // type == PNOISE_IMPACT
	{
		noise = who->mynoise2;
		who->client->sound2_entity = noise;
		who->client->sound2_entity_time = level.time;
	}

	noise->s.origin = where;
	noise->absmin = where - noise->maxs;
	noise->absmax = where + noise->maxs;
	noise->teleport_time = level.time;
	gi.linkentity(noise);
}

inline bool G_WeaponShouldStay()
{
	if (deathmatch->integer)
		return g_dm_weapons_stay->integer;
	else if (coop->integer)
		return !P_UseCoopInstancedItems();

	return false;
}

void G_CheckAutoSwitch(edict_t *ent, gitem_t *item, bool is_new);


//zucc pickup function for special items
bool Pickup_Special (edict_t * ent, edict_t * other)
{
	if (other->client->unique_item_total >= unique_items->value)
		return false;

	// Don't allow picking up multiple of the same special item.
	if( (! allow_hoarding->value) && other->client->inventory[ITEM_INDEX(ent->item)] )
		return false;

	AddItem(other, ent->item);

	if(!(ent->spawnflags & (SPAWNFLAG_ITEM_DROPPED | SPAWNFLAG_ITEM_DROPPED_PLAYER)) && item_respawnmode->value)
		SetRespawn (ent, item_respawn->value);

	return true;
}



void Drop_Special (edict_t * ent, gitem_t * item)
{
	int count;

	ent->client->unique_item_total--;
	if (item->typeNum == BAND_NUM && INV_AMMO(ent, BAND_NUM) <= 1)
	{
		if (gameSettings & GS_DEATHMATCH)
			count = 2;
		else
			count = 1;

		ent->client->max_pistolmags = count;
		if (INV_AMMO(ent, MK23_ANUM) > count)
			INV_AMMO(ent, MK23_ANUM) = count;

		if (!(gameSettings & GS_DEATHMATCH)) {
			if(ent->client->pers.chosenWeapon = GetItemByIndex(IT_WEAPON_HANDCANNON))
				count = 12;
			else
				count = 7;
		} else {
			count = 14;
		}
		ent->client->max_shells = count;
		if (INV_AMMO(ent, SHELL_ANUM) > count)
			INV_AMMO(ent, SHELL_ANUM) = count;

		ent->client->max_m4mags = 1;
		if (INV_AMMO(ent, M4_ANUM) > 1)
			INV_AMMO(ent, M4_ANUM) = 1;

		ent->client->grenade_max = 2;
		if (use_buggy_bandolier->value == 0) {
			if ((gameSettings & GS_DEATHMATCH) && INV_AMMO(ent, GRENADE_NUM) > 2)
				INV_AMMO(ent, GRENADE_NUM) = 2;
			else if (teamplay->value) {
				if (ent->client->curr_weap == GRENADE_NUM)
					INV_AMMO(ent, GRENADE_NUM) = 1;
				else
					INV_AMMO(ent, GRENADE_NUM) = 0;
			}
		} else {
			if (INV_AMMO(ent, GRENADE_NUM) > 2)
				INV_AMMO(ent, GRENADE_NUM) = 2;
		}
		if (gameSettings & GS_DEATHMATCH)
			count = 2;
		else
			count = 1;
		ent->client->max_mp5mags = count;
		if (INV_AMMO(ent, MP5_ANUM) > count)
			INV_AMMO(ent, MP5_ANUM) = count;

		ent->client->knife_max = 10;
		if (INV_AMMO(ent, KNIFE_NUM) > 10)
			INV_AMMO(ent, KNIFE_NUM) = 10;

		if (gameSettings & GS_DEATHMATCH)
			count = 20;
		else
			count = 10;
		ent->client->max_sniper_rnds = count;
		if (INV_AMMO(ent, SNIPER_ANUM) > count)
			INV_AMMO(ent, SNIPER_ANUM) = count;

		if (ent->client->unique_weapon_total > unique_weapons->value && !allweapon->value)
		{
			DropExtraSpecial (ent);
			gi.LocCenter_Print(ent, PRINT_HIGH, "One of your guns is dropped with the bandolier.\n");
		}
	}
	Drop_Spec(ent, item);
	ValidateSelectedItem(ent);
	SP_LaserSight(ent, item);
}

// called by the "drop item" command
void DropSpecialItem (edict_t * ent)
{
	// this is the order I'd probably want to drop them in...       
	if (INV_AMMO(ent, LASER_NUM))
		Drop_Special (ent, GetItemByIndex(IT_ITEM_LASERSIGHT));
	else if (INV_AMMO(ent, SLIP_NUM))
		Drop_Special (ent, GetItemByIndex(IT_ITEM_SLIPPERS));
	else if (INV_AMMO(ent, SIL_NUM))
		Drop_Special (ent, GetItemByIndex(IT_ITEM_QUIET));
	else if (INV_AMMO(ent, BAND_NUM))
		Drop_Special (ent, GetItemByIndex(IT_ITEM_BANDOLIER));
	else if (INV_AMMO(ent, HELM_NUM))
		Drop_Special (ent, GetItemByIndex(IT_ITEM_HELM));
	else if (INV_AMMO(ent, KEV_NUM))
		Drop_Special (ent, GetItemByIndex(IT_ITEM_VEST));
}

bool Pickup_Weapon(edict_t *ent, edict_t *other)
{
	item_id_t index;
	gitem_t	*ammo;
	bool addAmmo;
	int special = 0;
	int band = 0;

	index = ent->item->id;

	if (G_WeaponShouldStay() && other->client->pers.inventory[index])
	{
		if (!(ent->spawnflags & (SPAWNFLAG_ITEM_DROPPED | SPAWNFLAG_ITEM_DROPPED_PLAYER)))
			return false; // leave the weapon for others to pickup
	}

	// find out if they have a bandolier
	if (INV_AMMO(other, BAND_NUM))
		band = 1;
	else
		band = 0;

	bool is_new = !other->client->pers.inventory[index];

	switch (ent->item->typeNum) {
	case MK23_NUM:
		if (!WPF_ALLOWED(MK23_NUM))
			return false;

		if (other->client->inventory[index])	// already has one
		{
			if (!(ent->spawnflags & DROPPED_ITEM))
			{
				ammo = FindItem(ent->item->ammo);
				addAmmo = Add_Ammo(other, ammo, ammo->quantity);
				if (addAmmo && !(ent->spawnflags & DROPPED_PLAYER_ITEM))
					SetRespawn(ent, weapon_respawn->value);

				return addAmmo;
			}
		}
		other->client->inventory[index]++;
		if (!(ent->spawnflags & DROPPED_ITEM)) {
			other->client->mk23_rds = other->client->mk23_max;
			if (!(ent->spawnflags & DROPPED_PLAYER_ITEM))
				SetRespawn(ent, weapon_respawn->value);
		}
		return true;

	case MP5_NUM:
		if (other->client->unique_weapon_total >= unique_weapons->value + band)
			return false;		// we can't get it
		if ((!allow_hoarding->value) && other->client->inventory[index])
			return false;		// we already have one

		other->client->inventory[index]++;
		other->client->unique_weapon_total++;
		if (!(ent->spawnflags & DROPPED_ITEM)) {
			other->client->mp5_rds = other->client->mp5_max;
		}
		special = 1;
		gi.cprintf(other, PRINT_HIGH, "%s - Unique Weapon\n", ent->item->pickup_name);
		break;

	case M4_NUM:
		if (other->client->unique_weapon_total >= unique_weapons->value + band)
			return false;		// we can't get it
		if ((!allow_hoarding->value) && other->client->inventory[index])
			return false;		// we already have one

		other->client->inventory[index]++;
		other->client->unique_weapon_total++;
		if (!(ent->spawnflags & DROPPED_ITEM)) {
			other->client->m4_rds = other->client->m4_max;
		}
		special = 1;
		gi.cprintf(other, PRINT_HIGH, "%s - Unique Weapon\n", ent->item->pickup_name);
		break;

	case M3_NUM:
		if (other->client->unique_weapon_total >= unique_weapons->value + band)
			return false;		// we can't get it
		if ((!allow_hoarding->value) && other->client->inventory[index])
			return false;		// we already have one

		other->client->inventory[index]++;
		other->client->unique_weapon_total++;
		if (!(ent->spawnflags & DROPPED_ITEM))
		{
			// any weapon that doesn't completely fill up each reload can 
			//end up in a state where it has a full weapon and pending reload(s)
			if (other->client->weaponstate == WEAPON_RELOADING &&
				other->client->curr_weap == M3_NUM)
			{
				if (other->client->fast_reload)
					other->client->shot_rds = other->client->shot_max - 2;
				else
					other->client->shot_rds = other->client->shot_max - 1;
			}
			else
			{
				other->client->shot_rds = other->client->shot_max;
			}
		}
		special = 1;
		gi.cprintf(other, PRINT_HIGH, "%s - Unique Weapon\n", ent->item->pickup_name);
		break;

	case HC_NUM:
		if (other->client->unique_weapon_total >= unique_weapons->value + band)
			return false;		// we can't get it
		if ((!allow_hoarding->value) && other->client->inventory[index])
			return false;		// we already have one

		other->client->inventory[index]++;
		other->client->unique_weapon_total++;
		if (!(ent->spawnflags & DROPPED_ITEM))
		{
			other->client->cannon_rds = other->client->cannon_max;
			index2 = ITEM_INDEX(FindItem(ent->item->ammo));
			if (other->client->inventory[index2] + 5 > other->client->max_shells)
				other->client->inventory[index2] = other->client->max_shells;
			else
				other->client->inventory[index2] += 5;
		}
		gi.cprintf(other, PRINT_HIGH, "%s - Unique Weapon\n", ent->item->pickup_name);
		special = 1;
		break;

	case SNIPER_NUM:
		if (other->client->unique_weapon_total >= unique_weapons->value + band)
			return false;		// we can't get it
		if ((!allow_hoarding->value) && other->client->inventory[index])
			return false;		// we already have one

		other->client->inventory[index]++;
		other->client->unique_weapon_total++;
		if (!(ent->spawnflags & DROPPED_ITEM))
		{
			if (other->client->weaponstate == WEAPON_RELOADING &&
				other->client->curr_weap == SNIPER_NUM)
			{
				if (other->client->fast_reload)
					other->client->sniper_rds = other->client->sniper_max - 2;
				else
					other->client->sniper_rds = other->client->sniper_max - 1;
			}
			else
			{
				other->client->sniper_rds = other->client->sniper_max;
			}
		}
		special = 1;
		gi.cprintf(other, PRINT_HIGH, "%s - Unique Weapon\n", ent->item->pickup_name);
		break;

	case DUAL_NUM:
		if (!WPF_ALLOWED(MK23_NUM))
			return false;

		if (other->client->inventory[index])	// already has one
		{
			if (!(ent->spawnflags & DROPPED_ITEM))
			{
				ammo = FindItem(ent->item->ammo);
				addAmmo = Add_Ammo(other, ammo, ammo->quantity);
				if (addAmmo && !(ent->spawnflags & DROPPED_PLAYER_ITEM))
					SetRespawn(ent, weapon_respawn->value);

				return addAmmo;
			}
		}
		other->client->inventory[index]++;
		if (!(ent->spawnflags & DROPPED_ITEM))
		{
			other->client->dual_rds += other->client->mk23_max;
			// assume the player uses the new (full) pistol
			other->client->mk23_rds = other->client->mk23_max;
			if (!(ent->spawnflags & DROPPED_PLAYER_ITEM))
				SetRespawn(ent, weapon_respawn->value);
		}
		return true;

	case KNIFE_NUM:
		if (other->client->inventory[index] < other->client->knife_max)
		{
			other->client->inventory[index]++;
			return true;
		}

		return false;

	case GRENADE_NUM:
		if (!(gameSettings & GS_DEATHMATCH) && ctf->value != 2 && !band)
			return false;

		if (other->client->inventory[index] >= other->client->grenade_max)
			return false;

		other->client->inventory[index]++;
		if (!(ent->spawnflags & (DROPPED_ITEM | DROPPED_PLAYER_ITEM)))
		{
			if (DMFLAGS(DF_WEAPONS_STAY))
				ent->flags |= FL_RESPAWN;
			else
				SetRespawn(ent, ammo_respawn->value);
		}
		return true;

	default:
		other->client->pers.inventory[index]++;

		if (!(ent->spawnflags & SPAWNFLAG_ITEM_DROPPED))
		{
			// give them some ammo with it
			// PGM -- IF APPROPRIATE!
			if (ent->item->ammo) // PGM
			{
				ammo = GetItemByIndex(ent->item->ammo);
				// RAFAEL: Don't get infinite ammo with trap
				if (G_CheckInfiniteAmmo(ammo))
					Add_Ammo(other, ammo, 1000);
				else
					Add_Ammo(other, ammo, ammo->quantity);
			}

			if (!(ent->spawnflags & SPAWNFLAG_ITEM_DROPPED_PLAYER))
			{
				if (deathmatch->integer)
				{
					if (g_dm_weapons_stay->integer)
						ent->flags |= FL_RESPAWN;

					SetRespawn( ent, gtime_t::from_sec(g_weapon_respawn_time->integer), !g_dm_weapons_stay->integer);
				}
				if (coop->integer)
					ent->flags |= FL_RESPAWN;
			}
		}

	if (!(ent->spawnflags & (DROPPED_ITEM | DROPPED_PLAYER_ITEM))
		&& (SPEC_WEAPON_RESPAWN) && special)
	{
		if (DMFLAGS(DF_WEAPON_RESPAWN) && ((gameSettings & GS_DEATHMATCH) || ctf->value == 2))
			SetRespawn(ent, weapon_respawn->value);
		else
			SetSpecWeapHolder(ent);
	}

	G_CheckAutoSwitch(other, ent->item, is_new);

	return true;
}

static void Weapon_RunThink(edict_t *ent)
{
	// call active weapon think routine
	if (!ent->client->pers.weapon->weaponthink)
		return;

	P_DamageModifier(ent);
	// RAFAEL
	is_quadfire = (ent->client->quadfire_time > level.time);
	// RAFAEL
	if (ent->client->silencer_shots)
		is_silenced = MZ_SILENCED;
	else
		is_silenced = MZ_NONE;
	ent->client->pers.weapon->weaponthink(ent);
}

/*
===============
ChangeWeapon

The old weapon has been dropped all the way, so make the new one
current
===============
*/
void ChangeWeapon(edict_t *ent)
{
	// [Paril-KEX]
	if (ent->health > 0 && !g_instant_weapon_switch->integer && ((ent->client->latched_buttons | ent->client->buttons) & BUTTON_HOLSTER))
		return;

	if (ent->client->grenade_time)
	{
		// force a weapon think to drop the held grenade
		ent->client->weapon_sound = 0;
		Weapon_RunThink(ent);
		ent->client->grenade_time = 0_ms;
	}

	if (ent->client->pers.weapon)
	{
		ent->client->pers.lastweapon = ent->client->pers.weapon;

		if (ent->client->newweapon && ent->client->newweapon != ent->client->pers.weapon)
			gi.sound(ent, CHAN_WEAPON, gi.soundindex("weapons/change.wav"), 1, ATTN_NORM, 0);
	}

	ent->client->pers.weapon = ent->client->newweapon;
	ent->client->newweapon = nullptr;
	ent->client->machinegun_shots = 0;

	// set visible model
	if (ent->s.modelindex == MODELINDEX_PLAYER)
		P_AssignClientSkinnum(ent);

	if (!ent->client->pers.weapon)
	{ // dead
		ent->client->ps.gunindex = 0;
		ent->client->ps.gunskin = 0;
		return;
	}

	ent->client->weaponstate = WEAPON_ACTIVATING;
	ent->client->ps.gunframe = 0;
	ent->client->ps.gunindex = gi.modelindex(ent->client->pers.weapon->view_model);
	ent->client->ps.gunskin = 0;
	ent->client->weapon_sound = 0;

	ent->client->anim_priority = ANIM_PAIN;
	if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
	{
		ent->s.frame = FRAME_crpain1;
		ent->client->anim_end = FRAME_crpain4;
	}
	else
	{
		ent->s.frame = FRAME_pain301;
		ent->client->anim_end = FRAME_pain304;
	}
	ent->client->anim_time = 0_ms;

	ent->client->curr_weap = ent->client->weapon->typeNum;
	if (ent->client->curr_weap == GRENADE_NUM) {
		// Fix the "use grenades;drop bandolier" bug, caused infinite grenades.
		if (teamplay->value && INV_AMMO(ent, GRENADE_NUM) == 0)
			INV_AMMO(ent, GRENADE_NUM) = 1;
	}

	if (INV_AMMO(ent, LASER_NUM))
		SP_LaserSight(ent, GET_ITEM(LASER_NUM));	//item->use(ent, item);

	// for instantweap, run think immediately
	// to set up correct start frame
	if (g_instant_weapon_switch->integer)
		Weapon_RunThink(ent);
}

/*
=================
NoAmmoWeaponChange
=================
*/
void NoAmmoWeaponChange(edict_t *ent, bool sound)
{
	if (sound)
	{
		if (level.time >= ent->client->empty_click_sound)
		{
			gi.sound(ent, CHAN_WEAPON, gi.soundindex("weapons/noammo.wav"), 1, ATTN_NORM, 0);
			ent->client->empty_click_sound = level.time + 1_sec;
		}
	}

	constexpr item_id_t no_ammo_order[] = {
		IT_WEAPON_MK23,
		IT_WEAPON_M3,
		IT_WEAPON_SNIPER,
		IT_WEAPON_MP5,
		IT_WEAPON_HANDCANNON,
		IT_WEAPON_M4,
		IT_WEAPON_SNIPER,
		IT_WEAPON_DUALMK23,
		IT_WEAPON_KNIFE,
		IT_WEAPON_GRENADES,

		IT_AMMO_SHELLS,
		IT_AMMO_BULLETS,
		IT_AMMO_M4,
		IT_AMMO_MAG,
		IT_AMMO_SNIPER,

		IT_ITEM_QUIET,
		IT_ITEM_SLIPPERS,
		IT_ITEM_BANDOLIER,
		IT_ITEM_LASERSIGHT,
		IT_ITEM_HELM,
		
		IT_ITEM_IR_GOGGLES,
		IT_ITEM_TAG_TOKEN,

		IT_HEALTH_MEDIUM,

		IT_FLAG1,
		IT_FLAG2,

		IT_ITEM_FLASHLIGHT
	};

	for (size_t i = 0; i < q_countof(no_ammo_order); i++)
	{
		gitem_t *item = GetItemByIndex(no_ammo_order[i]);

		if (!item)
			gi.Com_ErrorFmt("Invalid no ammo weapon switch weapon {}\n", (int32_t) no_ammo_order[i]);

		if (!ent->client->pers.inventory[item->id])
			continue;

		if (item->ammo && ent->client->pers.inventory[item->ammo] < item->quantity)
			continue;

		ent->client->newweapon = item;
		return;
	}
}

void G_RemoveAmmo(edict_t *ent, int32_t quantity)
{
	if (G_CheckInfiniteAmmo(ent->client->pers.weapon))
		return;

	bool pre_warning = ent->client->pers.inventory[ent->client->pers.weapon->ammo] <=
		ent->client->pers.weapon->quantity_warn;

	ent->client->pers.inventory[ent->client->pers.weapon->ammo] -= quantity;

	bool post_warning = ent->client->pers.inventory[ent->client->pers.weapon->ammo] <=
		ent->client->pers.weapon->quantity_warn;

	if (!pre_warning && post_warning)
		gi.local_sound(ent, CHAN_AUTO, gi.soundindex("weapons/lowammo.wav"), 1, ATTN_NORM, 0);

	if (ent->client->pers.weapon->ammo == IT_AMMO_M4)
		G_CheckPowerArmor(ent);
}

void G_RemoveAmmo(edict_t *ent)
{
	G_RemoveAmmo(ent, ent->client->pers.weapon->quantity);
}

// [Paril-KEX] get time per animation frame
inline gtime_t Weapon_AnimationTime(edict_t *ent)
{
	if (g_quick_weapon_switch->integer && (gi.tick_rate == 20 || gi.tick_rate == 40) &&
		(ent->client->weaponstate == WEAPON_ACTIVATING || ent->client->weaponstate == WEAPON_DROPPING))
		ent->client->ps.gunrate = 20;
	else
		ent->client->ps.gunrate = 10;

	if (ent->client->ps.gunframe != 0 && (!(ent->client->pers.weapon->flags & IF_NO_HASTE) || ent->client->weaponstate != WEAPON_FIRING))
	{
		if (is_quadfire)
			ent->client->ps.gunrate *= 2;
		if (CTFApplyHaste(ent))
			ent->client->ps.gunrate *= 2;
	}

	// network optimization...
	if (ent->client->ps.gunrate == 10)
	{
		ent->client->ps.gunrate = 0;
		return 100_ms;
	}

	return gtime_t::from_ms((1.f / ent->client->ps.gunrate) * 1000);
}

/*
=================
Think_Weapon

Called by ClientBeginServerFrame and ClientThink
=================
*/
void Think_Weapon(edict_t *ent)
{
	if (ent->client->resp.spectator)
		return;

	// if just died, put the weapon away
	if (ent->health < 1)
	{
		ent->client->newweapon = nullptr;
		ChangeWeapon(ent);
	}

	if (!ent->client->pers.weapon)
	{
		if (ent->client->newweapon)
			ChangeWeapon(ent);
		return;
	}

	// call active weapon think routine
	Weapon_RunThink(ent);

	// check remainder from haste; on 100ms/50ms server frames we may have
	// 'run next frame in' times that we can't possibly catch up to,
	// so we have to run them now.
	if (33_ms < FRAME_TIME_MS)
	{
		gtime_t relative_time = Weapon_AnimationTime(ent);

		if (relative_time < FRAME_TIME_MS)
		{
			// check how many we can't run before the next server tick
			gtime_t next_frame = level.time + FRAME_TIME_S;
			int64_t remaining_ms = (next_frame - ent->client->weapon_think_time).milliseconds();

			while (remaining_ms > 0)
			{
				ent->client->weapon_think_time -= relative_time;
				ent->client->weapon_fire_finished -= relative_time;
				Weapon_RunThink(ent);
				remaining_ms -= relative_time.milliseconds();
			}
		}
	}
}

enum weap_switch_t
{
	WEAP_SWITCH_ALREADY_USING,
	WEAP_SWITCH_NO_WEAPON,
	WEAP_SWITCH_NO_AMMO,
	WEAP_SWITCH_NOT_ENOUGH_AMMO,
	WEAP_SWITCH_VALID
};

weap_switch_t Weapon_AttemptSwitch(edict_t *ent, gitem_t *item, bool silent)
{
	if (ent->client->pers.weapon == item)
		return WEAP_SWITCH_ALREADY_USING;
	else if (!ent->client->pers.inventory[item->id])
		return WEAP_SWITCH_NO_WEAPON;

	if (item->ammo && !g_select_empty->integer && !(item->flags & IF_AMMO))
	{
		gitem_t *ammo_item = GetItemByIndex(item->ammo);

		if (!ent->client->pers.inventory[item->ammo])
		{
			if (!silent)
				gi.LocClient_Print(ent, PRINT_HIGH, "$g_no_ammo", ammo_item->pickup_name, item->pickup_name_definite);
			return WEAP_SWITCH_NO_AMMO;
		}
		else if (ent->client->pers.inventory[item->ammo] < item->quantity)
		{
			if (!silent)
				gi.LocClient_Print(ent, PRINT_HIGH, "$g_not_enough_ammo", ammo_item->pickup_name, item->pickup_name_definite);
			return WEAP_SWITCH_NOT_ENOUGH_AMMO;
		}
	}

	return WEAP_SWITCH_VALID;
}

inline bool Weapon_IsPartOfChain(gitem_t *item, gitem_t *other)
{
	return other && other->chain && item->chain && other->chain == item->chain;
}

/*
================
Use_Weapon

Make the weapon ready if there is ammo
================
*/
void Use_Weapon(edict_t *ent, gitem_t *item)
{
	gitem_t		*wanted, *root;
	weap_switch_t result = WEAP_SWITCH_NO_WEAPON;

	// if we're switching to a weapon in this chain already,
	// start from the weapon after this one in the chain
	if (!ent->client->no_weapon_chains && Weapon_IsPartOfChain(item, ent->client->newweapon))
	{
		root = ent->client->newweapon;
		wanted = root->chain_next;
	}
	// if we're already holding a weapon in this chain,
	// start from the weapon after that one
	else if (!ent->client->no_weapon_chains && Weapon_IsPartOfChain(item, ent->client->pers.weapon))
	{
		root = ent->client->pers.weapon;
		wanted = root->chain_next;
	}
	// start from beginning of chain (if any)
	else
		wanted = root = item;

	while (true)
	{
		// try the weapon currently in the chain
		if ((result = Weapon_AttemptSwitch(ent, wanted, false)) == WEAP_SWITCH_VALID)
			break;

		// no chains
		if (!wanted->chain_next || ent->client->no_weapon_chains)
			break;

		wanted = wanted->chain_next;

		// we wrapped back to the root item
		if (wanted == root)
			break;
	}

	if (result == WEAP_SWITCH_VALID)
		ent->client->newweapon = wanted; // change to this weapon when down
	else if ((result = Weapon_AttemptSwitch(ent, wanted, true)) == WEAP_SWITCH_NO_WEAPON && wanted != ent->client->pers.weapon && wanted != ent->client->newweapon)
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_out_of_item", wanted->pickup_name);
}

edict_t* FindSpecWeapSpawn(edict_t* ent)
{
	edict_t* spot = NULL;

	//gi.bprintf (PRINT_HIGH, "Calling the FindSpecWeapSpawn\n");
	spot = G_Find(spot, FOFS(classname), ent->classname);
	//gi.bprintf (PRINT_HIGH, "spot = %p and spot->think = %p and playerholder = %p, spot, (spot ? spot->think : 0), PlaceHolder\n");
	while (spot && spot->think != PlaceHolder)	//(spot->spawnflags & DROPPED_ITEM ) && spot->think != PlaceHolder )//spot->solid == SOLID_NOT )        
	{
		//              gi.bprintf (PRINT_HIGH, "Calling inside the loop FindSpecWeapSpawn\n");
		spot = G_Find(spot, FOFS(classname), ent->classname);
	}
	return spot;
}

static void SpawnSpecWeap(gitem_t* item, edict_t* spot)
{
	SetRespawn(spot, 1);
	gi.linkentity(spot);
}

void temp_think_specweap(edict_t* ent)
{
	ent->touch = Touch_Item;

	if (allweapon->value) { // allweapon set
		ent->nextthink = level.framenum + 1 * HZ;
		ent->think = G_FreeEdict;
		return;
	}

	if (gameSettings & GS_ROUNDBASED) {
		ent->nextthink = level.framenum + 1000 * HZ;
		ent->think = PlaceHolder;
		return;
	}

	if (gameSettings & GS_WEAPONCHOOSE) {
		ent->nextthink = level.framenum + 6 * HZ;
		ent->think = ThinkSpecWeap;
	}
	else if (DMFLAGS(DF_WEAPON_RESPAWN)) {
		ent->nextthink = level.framenum + (weapon_respawn->value * 0.6f) * HZ;
		ent->think = G_FreeEdict;
	}
	else {
		ent->nextthink = level.framenum + weapon_respawn->value * HZ;
		ent->think = ThinkSpecWeap;
	}
}

// zucc make dropped weapons respawn elsewhere
void ThinkSpecWeap(edict_t* ent)
{
	edict_t* spot;

	if ((spot = FindSpecWeapSpawn(ent)) != NULL)
	{
		SpawnSpecWeap(ent->item, spot);
		G_FreeEdict(ent);
	}
	else
	{
		ent->nextthink = level.framenum + 1 * HZ;
		ent->think = G_FreeEdict;
	}
}


/*
================
Drop_Weapon
================
*/
void Drop_Weapon(edict_t *ent, gitem_t *item)
{
	int index;
	gitem_t* replacement;
	edict_t* temp = NULL;

	item_id_t index = item->id;
	// see if we're already using it
	if (((item == ent->client->pers.weapon) || (item == ent->client->newweapon)) && (ent->client->pers.inventory[index] == 1))
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_cant_drop_weapon");
		return;
	}

	// AQ:TNG - JBravo fixing weapon farming
	if (ent->client->weaponstate == WEAPON_DROPPING ||
		ent->client->weaponstate == WEAPON_BUSY)
		return;
	// Weapon farming fix end.

	if (ent->client->weaponstate == WEAPON_BANDAGING ||
		ent->client->bandaging == 1 || ent->client->bandage_stopped == 1)
	{
		gi.cprintf(ent, PRINT_HIGH,
			"You are too busy bandaging right now...\n");
		return;
	}
	// don't let them drop this, causes duplication
	if (ent->client->newweapon == item)
		return;

	index = ITEM_INDEX(item);
	// see if we're already using it
	//zucc special cases for dropping       
	if (item->typeNum == MK23_NUM)
	{
		gi.cprintf(ent, PRINT_HIGH, "Can't drop the %s.\n", MK23_NAME);
		return;
	}
	else if (item->typeNum == MP5_NUM)
	{

		if (ent->client->weapon == item && ent->client->inventory[index] == 1)
		{
			replacement = GET_ITEM(MK23_NUM);	// back to the pistol then
			ent->client->newweapon = replacement;
			ent->client->weaponstate = WEAPON_DROPPING;
			ent->client->ps.gunframe = 51;

			//ChangeWeapon( ent );
		}
		ent->client->unique_weapon_total--;	// dropping 1 unique weapon
		temp = Drop_Item(ent, item);
		temp->think = temp_think_specweap;
		ent->client->inventory[index]--;
	}
	else if (item->typeNum == M4_NUM)
	{

		if (ent->client->weapon == item && ent->client->inventory[index] == 1)
		{
			replacement = GET_ITEM(MK23_NUM);	// back to the pistol then
			ent->client->newweapon = replacement;
			ent->client->weaponstate = WEAPON_DROPPING;
			ent->client->ps.gunframe = 44;

			//ChangeWeapon( ent );
		}
		ent->client->unique_weapon_total--;	// dropping 1 unique weapon
		temp = Drop_Item(ent, item);
		temp->think = temp_think_specweap;
		ent->client->inventory[index]--;
	}
	else if (item->typeNum == M3_NUM)
	{

		if (ent->client->weapon == item && ent->client->inventory[index] == 1)
		{
			replacement = GET_ITEM(MK23_NUM);	// back to the pistol then
			ent->client->newweapon = replacement;
			ent->client->weaponstate = WEAPON_DROPPING;
			ent->client->ps.gunframe = 41;
			//ChangeWeapon( ent );
		}
		ent->client->unique_weapon_total--;	// dropping 1 unique weapon
		temp = Drop_Item(ent, item);
		temp->think = temp_think_specweap;
		ent->client->inventory[index]--;
	}
	else if (item->typeNum == HC_NUM)
	{
		if (ent->client->weapon == item && ent->client->inventory[index] == 1)
		{
			replacement = GET_ITEM(MK23_NUM);	// back to the pistol then
			ent->client->newweapon = replacement;
			ent->client->weaponstate = WEAPON_DROPPING;
			ent->client->ps.gunframe = 61;
			//ChangeWeapon( ent );
		}
		ent->client->unique_weapon_total--;	// dropping 1 unique weapon
		temp = Drop_Item(ent, item);
		temp->think = temp_think_specweap;
		ent->client->inventory[index]--;
	}
	else if (item->typeNum == SNIPER_NUM)
	{
		if (ent->client->weapon == item && ent->client->inventory[index] == 1)
		{
			// in case they are zoomed in
			ent->client->ps.fov = 90;
			ent->client->desired_fov = 90;
			replacement = GET_ITEM(MK23_NUM);	// back to the pistol then
			ent->client->newweapon = replacement;
			ent->client->weaponstate = WEAPON_DROPPING;
			ent->client->ps.gunframe = 50;
			//ChangeWeapon( ent );
		}
		ent->client->unique_weapon_total--;	// dropping 1 unique weapon
		temp = Drop_Item(ent, item);
		temp->think = temp_think_specweap;
		ent->client->inventory[index]--;
	}
	else if (item->typeNum == DUAL_NUM)
	{
		if (ent->client->weapon == item)
		{
			replacement = GET_ITEM(MK23_NUM);	// back to the pistol then
			ent->client->newweapon = replacement;
			ent->client->weaponstate = WEAPON_DROPPING;
			ent->client->ps.gunframe = 40;
			//ChangeWeapon( ent );
		}
		ent->client->dual_rds = ent->client->mk23_rds;
	}
	else if (item->typeNum == KNIFE_NUM)
	{
		//gi.cprintf(ent, PRINT_HIGH, "Before checking knife inven frames = %d\n", ent->client->ps.gunframe);

		if (ent->client->weapon == item && ent->client->inventory[index] == 1)
		{
			replacement = GET_ITEM(MK23_NUM);	// back to the pistol then
			ent->client->newweapon = replacement;
			if (ent->client->pers.knife_mode)	// hack to avoid an error
			{
				ent->client->weaponstate = WEAPON_DROPPING;
				ent->client->ps.gunframe = 111;
			}
			else
				ChangeWeapon(ent);
			//      gi.cprintf(ent, PRINT_HIGH, "After change weap from knife drop frames = %d\n", ent->client->ps.gunframe);
		}
	}
	else if (item->typeNum == GRENADE_NUM)
	{
		if (ent->client->weapon == item && ent->client->inventory[index] == 1)
		{
			if (((ent->client->ps.gunframe >= GRENADE_IDLE_FIRST)
				&& (ent->client->ps.gunframe <= GRENADE_IDLE_LAST))
				|| ((ent->client->ps.gunframe >= GRENADE_THROW_FIRST
					&& ent->client->ps.gunframe <= GRENADE_THROW_LAST)))
			{
				int damage;

				ent->client->ps.gunframe = 0;

				// Reset Grenade Damage to 1.52 when requested:
				if (use_classic->value)
					damage = GRENADE_DAMRAD_CLASSIC;
				else
					damage = GRENADE_DAMRAD;

				if (ent->client->quad_framenum > level.framenum)
					damage *= 1.5f;

				fire_grenade2(ent, ent->s.origin, vec3_origin, damage, 0, 2 * HZ, damage * 2, false);

				INV_AMMO(ent, GRENADE_NUM)--;
				ent->client->newweapon = GET_ITEM(MK23_NUM);
				ent->client->weaponstate = WEAPON_DROPPING;
				ent->client->ps.gunframe = 0;
				return;
			}

			replacement = GET_ITEM(MK23_NUM);	// back to the pistol then
			ent->client->newweapon = replacement;
			ent->client->weaponstate = WEAPON_DROPPING;
			ent->client->ps.gunframe = 0;
			//ChangeWeapon( ent );
		}
	}
	else if ((item == ent->client->weapon || item == ent->client->newweapon)
		&& ent->client->inventory[index] == 1)
	{
		gi.cprintf(ent, PRINT_HIGH, "Can't drop current weapon\n");
		return;
	}

	// AQ:TNG - JBravo fixing weapon farming
	if (ent->client->unique_weapon_total < 0)
		ent->client->unique_weapon_total = 0;
	if (ent->client->inventory[index] < 0)
		ent->client->inventory[index] = 0;
	// Weapon farming fix end

	edict_t *drop = Drop_Item(ent, item);
	drop->spawnflags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
	drop->svflags &= ~SVF_INSTANCED;
	ent->client->pers.inventory[index]--;
}

//zucc drop special weapon (only 1 of them)
void DropSpecialWeapon(edict_t* ent)
{
	int itemNum = ent->client->weapon ? ent->client->weapon->typeNum : 0;

	// first check if their current weapon is a special weapon, if so, drop it.
	if (itemNum >= MP5_NUM && itemNum <= SNIPER_NUM)
		Drop_Weapon(ent, ent->client->weapon);
	else if (INV_AMMO(ent, SNIPER_NUM) > 0)
		Drop_Weapon(ent, GET_ITEM(SNIPER_NUM));
	else if (INV_AMMO(ent, HC_NUM) > 0)
		Drop_Weapon(ent, GET_ITEM(HC_NUM));
	else if (INV_AMMO(ent, M3_NUM) > 0)
		Drop_Weapon(ent, GET_ITEM(M3_NUM));
	else if (INV_AMMO(ent, MP5_NUM) > 0)
		Drop_Weapon(ent, GET_ITEM(MP5_NUM));
	else if (INV_AMMO(ent, M4_NUM) > 0)
		Drop_Weapon(ent, GET_ITEM(M4_NUM));
	// special case, aq does this, guess I can support it
	else if (itemNum == DUAL_NUM)
		ent->client->newweapon = GET_ITEM(MK23_NUM);

}

// used for when we want to force a player to drop an extra special weapon
// for when they drop the bandolier and they are over the weapon limit
void DropExtraSpecial(edict_t* ent)
{
	int itemNum;

	itemNum = ent->client->weapon ? ent->client->weapon->typeNum : 0;
	if (itemNum >= MP5_NUM && itemNum <= SNIPER_NUM)
	{
		// if they have more than 1 then they are willing to drop one           
		if (INV_AMMO(ent, itemNum) > 1) {
			Drop_Weapon(ent, ent->client->weapon);
			return;
		}
	}
	// otherwise drop some weapon they aren't using
	if (INV_AMMO(ent, SNIPER_NUM) > 0 && SNIPER_NUM != itemNum)
		Drop_Weapon(ent, GET_ITEM(SNIPER_NUM));
	else if (INV_AMMO(ent, HC_NUM) > 0 && HC_NUM != itemNum)
		Drop_Weapon(ent, GET_ITEM(HC_NUM));
	else if (INV_AMMO(ent, M3_NUM) > 0 && M3_NUM != itemNum)
		Drop_Weapon(ent, GET_ITEM(M3_NUM));
	else if (INV_AMMO(ent, MP5_NUM) > 0 && MP5_NUM != itemNum)
		Drop_Weapon(ent, GET_ITEM(MP5_NUM));
	else if (INV_AMMO(ent, M4_NUM) > 0 && M4_NUM != itemNum)
		Drop_Weapon(ent, GET_ITEM(M4_NUM));
	else
		gi.dprintf("Couldn't find the appropriate weapon to drop.\n");
}

//zucc ready special weapon
void ReadySpecialWeapon(edict_t* ent)
{
	int weapons[5] = { MP5_NUM, M4_NUM, M3_NUM, HC_NUM, SNIPER_NUM };
	int curr, i;
	int last;


	if (ent->client->weaponstate == WEAPON_BANDAGING || ent->client->bandaging == 1)
		return;


	for (curr = 0; curr < 5; curr++)
	{
		if (ent->client->curr_weap == weapons[curr])
			break;
	}
	if (curr >= 5)
	{
		curr = -1;
		last = 5;
	}
	else
	{
		last = curr + 5;
	}

	for (i = (curr + 1); i != last; i = (i + 1))
	{
		if (INV_AMMO(ent, weapons[i % 5]))
		{
			ent->client->newweapon = GET_ITEM(weapons[i % 5]);
			return;
		}
	}
}

void Weapon_PowerupSound(edict_t *ent)
{
	if (!CTFApplyStrengthSound(ent))
	{
		if (ent->client->quad_time > level.time && ent->client->double_time > level.time)
			gi.sound(ent, CHAN_ITEM, gi.soundindex("ctf/tech2x.wav"), 1, ATTN_NORM, 0);
		else if (ent->client->quad_time > level.time)
			gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage3.wav"), 1, ATTN_NORM, 0);
		else if (ent->client->double_time > level.time)
			gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/ddamage3.wav"), 1, ATTN_NORM, 0);
		else if (ent->client->quadfire_time > level.time
			&& ent->client->ctf_techsndtime < level.time)
		{
			ent->client->ctf_techsndtime = level.time + 1_sec;
			gi.sound(ent, CHAN_ITEM, gi.soundindex("ctf/tech3.wav"), 1, ATTN_NORM, 0);
		}
	}

	CTFApplyHasteSound(ent);
}

inline bool Weapon_CanAnimate(edict_t *ent)
{
	// VWep animations screw up corpses
	return !ent->deadflag && ent->s.modelindex == MODELINDEX_PLAYER;
}

// [Paril-KEX] called when finished to set time until
// we're allowed to switch to fire again
inline void Weapon_SetFinished(edict_t *ent)
{
	ent->client->weapon_fire_finished = level.time + Weapon_AnimationTime(ent);
}

inline bool Weapon_HandleDropping(edict_t *ent, int FRAME_DEACTIVATE_LAST)
{
	if (ent->client->weaponstate == WEAPON_DROPPING)
	{
		if (ent->client->weapon_think_time <= level.time)
		{
			if (ent->client->ps.gunframe == FRAME_DEACTIVATE_LAST)
			{
				ChangeWeapon(ent);
				return true;
			}
			else if ((FRAME_DEACTIVATE_LAST - ent->client->ps.gunframe) == 4)
			{
				ent->client->anim_priority = ANIM_ATTACK | ANIM_REVERSED;
				if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
				{
					ent->s.frame = FRAME_crpain4 + 1;
					ent->client->anim_end = FRAME_crpain1;
				}
				else
				{
					ent->s.frame = FRAME_pain304 + 1;
					ent->client->anim_end = FRAME_pain301;
				}
				ent->client->anim_time = 0_ms;
			}

			ent->client->ps.gunframe++;
			ent->client->weapon_think_time = level.time + Weapon_AnimationTime(ent);
		}
		return true;
	}

	return false;
}

inline bool Weapon_HandleActivating(edict_t *ent, int FRAME_ACTIVATE_LAST, int FRAME_IDLE_FIRST)
{
	if (ent->client->weaponstate == WEAPON_ACTIVATING)
	{
		if (ent->client->weapon_think_time <= level.time || g_instant_weapon_switch->integer)
		{
			ent->client->weapon_think_time = level.time + Weapon_AnimationTime(ent);

			if (ent->client->ps.gunframe == FRAME_ACTIVATE_LAST || g_instant_weapon_switch->integer)
			{
				ent->client->weaponstate = WEAPON_READY;
				ent->client->ps.gunframe = FRAME_IDLE_FIRST;
				ent->client->weapon_fire_buffered = false;
				if (!g_instant_weapon_switch->integer)
					Weapon_SetFinished(ent);
				else
					ent->client->weapon_fire_finished = 0_ms;
				return true;
			}
			// sounds for activation?
		switch (ent->client->curr_weap)
		{
		case MK23_NUM:
		{
			if (ent->client->dual_rds >= ent->client->mk23_max)
				ent->client->mk23_rds = ent->client->mk23_max;
			else
				ent->client->mk23_rds = ent->client->dual_rds;
			if (ent->client->ps.gunframe == 3)	// 3
			{
				if (ent->client->mk23_rds > 0)
				{
					gi.sound(ent, CHAN_WEAPON,
						gi.soundindex("weapons/mk23slide.wav"), 1,
						ATTN_NORM, 0);
				}
				else
				{
					gi.sound(ent, CHAN_WEAPON, level.snd_noammo, 1, ATTN_NORM, 0);
					//mk23slap
					ent->client->ps.gunframe = 62;
					ent->client->weaponstate = WEAPON_END_MAG;
				}
			}
			ent->client->fired = 0;	//reset any firing delays
			break;
		}
		case MP5_NUM:
		{
			if (ent->client->ps.gunframe == 3)	// 3
				gi.sound(ent, CHAN_WEAPON,
					gi.soundindex("weapons/mp5slide.wav"), 1, ATTN_NORM,
					0);
			ent->client->fired = 0;	//reset any firing delays
			ent->client->burst = 0;
			break;
		}
		case M4_NUM:
		{
			if (ent->client->ps.gunframe == 3)	// 3
				gi.sound(ent, CHAN_WEAPON,
					gi.soundindex("weapons/m4a1slide.wav"), 1, ATTN_NORM,
					0);
			ent->client->fired = 0;	//reset any firing delays
			ent->client->burst = 0;
			ent->client->machinegun_shots = 0;
			break;
		}
		case M3_NUM:
		{
			ent->client->fired = 0;	//reset any firing delays
			ent->client->burst = 0;
			ent->client->fast_reload = 0;
			break;
		}
		case HC_NUM:
		{
			ent->client->fired = 0;	//reset any firing delays
			ent->client->burst = 0;
			break;
		}
		case SNIPER_NUM:
		{
			ent->client->fired = 0;	//reset any firing delays
			ent->client->burst = 0;
			ent->client->fast_reload = 0;
			break;
		}
		case DUAL_NUM:
		{
			if (ent->client->dual_rds <= 0 && ent->client->ps.gunframe == 3)
				gi.sound(ent, CHAN_WEAPON, level.snd_noammo, 1, ATTN_NORM, 0);
			if (ent->client->dual_rds <= 0 && ent->client->ps.gunframe == 4)
			{
				gi.sound(ent, CHAN_WEAPON, level.snd_noammo, 1, ATTN_NORM, 0);
				ent->client->ps.gunframe = 68;
				ent->client->weaponstate = WEAPON_END_MAG;
				ent->client->resp.sniper_mode = 0;

				ent->client->desired_fov = 90;
				ent->client->ps.fov = 90;
				ent->client->fired = 0;	//reset any firing delays
				ent->client->burst = 0;

				return;
			}
			ent->client->fired = 0;	//reset any firing delays
			ent->client->burst = 0;
			break;
		}
		case GRAPPLE_NUM:
			break;

		default:
			gi.dprintf("Activated unknown weapon.\n");
			break;
		}

		ent->client->resp.sniper_mode = 0;
		// has to be here for dropping the sniper rifle, in the drop command didn't work...
		ent->client->desired_fov = 90;
		ent->client->ps.fov = 90;
		ent->client->ps.gunframe++;
		return true;
		}

		if (ent->client->weaponstate == WEAPON_BUSY)
	{
		if (ent->client->bandaging == 1)
		{
			if (!(ent->client->idle_weapon))
			{
				Bandage(ent);
				return;
			}
			else
			{
				(ent->client->idle_weapon)--;
				return;
			}
		}

		// for after bandaging delay
		if (!(ent->client->idle_weapon) && ent->client->bandage_stopped)
		{
			ent->client->weaponstate = WEAPON_ACTIVATING;
			ent->client->ps.gunframe = 0;
			ent->client->bandage_stopped = 0;
			return;
		}
		else if (ent->client->bandage_stopped)
		{
			(ent->client->idle_weapon)--;
			return;
		}

		if (ent->client->curr_weap == SNIPER_NUM)
		{
			if (ent->client->desired_fov == 90)
			{
				ent->client->ps.fov = 90;
				ent->client->weaponstate = WEAPON_READY;
				ent->client->idle_weapon = 0;
			}
			if (!(ent->client->idle_weapon) && ent->client->desired_fov != 90)
			{
				ent->client->ps.fov = ent->client->desired_fov;
				ent->client->weaponstate = WEAPON_READY;
				ent->client->ps.gunindex = 0;
				return;
			}
			else
				(ent->client->idle_weapon)--;

		}
	}


	if ((ent->client->newweapon) && (ent->client->weaponstate != WEAPON_FIRING)
		&& (ent->client->weaponstate != WEAPON_BURSTING)
		&& (ent->client->bandage_stopped == 0))
	{
		ent->client->weaponstate = WEAPON_DROPPING;
		ent->client->ps.gunframe = FRAME_DEACTIVATE_FIRST;
		ent->client->desired_fov = 90;
		ent->client->ps.fov = 90;
		ent->client->resp.sniper_mode = 0;
		if (ent->client->weapon)
			ent->client->ps.gunindex = gi.modelindex(ent->client->weapon->view_model);

		// zucc more vwep stuff
		if ((FRAME_DEACTIVATE_LAST - FRAME_DEACTIVATE_FIRST) < 4)
		{
			if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
				SetAnimation( ent, FRAME_crpain4 + 1, FRAME_crpain1, ANIM_REVERSE );
			else
				SetAnimation( ent, FRAME_pain304 + 1, FRAME_pain301, ANIM_REVERSE );
		}
		return;
	}

	// bandaging case
	if ((ent->client->bandaging)
		&& (ent->client->weaponstate != WEAPON_FIRING)
		&& (ent->client->weaponstate != WEAPON_BURSTING)
		&& (ent->client->weaponstate != WEAPON_BUSY)
		&& (ent->client->weaponstate != WEAPON_BANDAGING))
	{
		ent->client->weaponstate = WEAPON_BANDAGING;
		ent->client->ps.gunframe = FRAME_DEACTIVATE_FIRST;
		return;
	}

	if (ent->client->weaponstate == WEAPON_READY)
	{
		if (((ent->client->latched_buttons | ent->client->buttons) & BUTTON_ATTACK)
			&& (ent->solid != SOLID_NOT || ent->deadflag == DEAD_DEAD)
			&& !lights_camera_action && (!ent->client->uvTime || dm_shield->value > 1))
		{
			if (ent->client->uvTime) {
				ent->client->uvTime = 0;
				gi.centerprintf(ent, "Shields are DOWN!");
			}
			ent->client->latched_buttons &= ~BUTTON_ATTACK;
			switch (ent->client->curr_weap)
			{
			case MK23_NUM:
			{
				//      gi.cprintf (ent, PRINT_HIGH, "Calling ammo check %d\n", ent->client->mk23_rds);
				if (ent->client->mk23_rds > 0)
				{
					//              gi.cprintf(ent, PRINT_HIGH, "Entered fire selection\n");
					if (ent->client->pers.mk23_mode != 0
						&& ent->client->fired == 0)
					{
						ent->client->fired = 1;
						bFire = 1;
					}
					else if (ent->client->pers.mk23_mode == 0)
					{
						bFire = 1;
					}
				}
				else
					bOut = 1;
				break;

			}
			case MP5_NUM:
			{
				if (ent->client->mp5_rds > 0)
				{
					if (ent->client->pers.mp5_mode != 0
						&& ent->client->fired == 0 && ent->client->burst == 0)
					{
						ent->client->fired = 1;
						ent->client->ps.gunframe = 71;
						ent->client->burst = 1;
						ent->client->weaponstate = WEAPON_BURSTING;

						// start the animation
						if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
							SetAnimation( ent, FRAME_crattak1 - 1, FRAME_crattak9, ANIM_ATTACK );
						else
							SetAnimation( ent, FRAME_attack1 - 1, FRAME_attack8, ANIM_ATTACK );
					}
					else if (ent->client->pers.mp5_mode == 0
						&& ent->client->fired == 0)
					{
						bFire = 1;
					}
				}
				else
					bOut = 1;
				break;
			}
			case M4_NUM:
			{
				if (ent->client->m4_rds > 0)
				{
					if (ent->client->pers.m4_mode != 0
						&& ent->client->fired == 0 && ent->client->burst == 0)
					{
						ent->client->fired = 1;
						ent->client->ps.gunframe = 65;
						ent->client->burst = 1;
						ent->client->weaponstate = WEAPON_BURSTING;

						// start the animation
						if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
							SetAnimation( ent, FRAME_crattak1 - 1, FRAME_crattak9, ANIM_ATTACK );
						else
							SetAnimation( ent, FRAME_attack1 - 1, FRAME_attack8, ANIM_ATTACK );
					}
					else if (ent->client->pers.m4_mode == 0
						&& ent->client->fired == 0)
					{
						bFire = 1;
					}
				}
				else
					bOut = 1;
				break;
			}
			case M3_NUM:
			{
				if (ent->client->shot_rds > 0)
				{
					bFire = 1;
				}
				else
					bOut = 1;
				break;
			}
			// AQ2:TNG Deathwatch - Single Barreled HC
			// DW: SingleBarrel HC
			case HC_NUM:

				//if client is set to single shot mode, then allow
				//fire if only have 1 round left
			{
				if (ent->client->pers.hc_mode)
				{
					if (ent->client->cannon_rds > 0)
					{
						bFire = 1;
					}
					else
						bOut = 1;
				}
				else
				{
					if (ent->client->cannon_rds == 2)
					{
						bFire = 1;
					}
					else
						bOut = 1;
				}
				break;
			}
			/*
									case HC_NUM:
											{
													if ( ent->client->cannon_rds == 2 )
													{
															bFire = 1;
													}
													else
															bOut = 1;
													break;
											}
			*/
			// AQ2:TNG END
			case SNIPER_NUM:
			{
				if (ent->client->ps.fov != ent->client->desired_fov)
					ent->client->ps.fov = ent->client->desired_fov;
				// if they aren't at 90 then they must be zoomed, so remove their weapon from view
				if (ent->client->ps.fov != 90)
				{
					ent->client->ps.gunindex = 0;
					ent->client->no_sniper_display = 0;
				}

				if (ent->client->sniper_rds > 0)
				{
					bFire = 1;
				}
				else
					bOut = 1;
				break;
			}
			case DUAL_NUM:
			{
				if (ent->client->dual_rds > 0)
				{
					bFire = 1;
				}
				else
					bOut = 1;
				break;
			}
			case GRAPPLE_NUM:
				bFire = 1;
				bOut = 0;
				break;

			default:
			{
				gi.cprintf(ent, PRINT_HIGH,
					"Calling non specific ammo code\n");
				if ((!ent->client->ammo_index)
					|| (ent->client->inventory[ent->client->ammo_index] >=
						ent->client->weapon->quantity))
				{
					bFire = 1;
				}
				else
				{
					bFire = 0;
					bOut = 1;
				}
			}

			}
			if (bFire)
			{
				ent->client->ps.gunframe = FRAME_FIRE_FIRST;
				ent->client->weaponstate = WEAPON_FIRING;

				// start the animation
				if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
					SetAnimation( ent, FRAME_crattak1 - 1, FRAME_crattak9, ANIM_ATTACK );
				else
					SetAnimation( ent, FRAME_attack1 - 1, FRAME_attack8, ANIM_ATTACK );
			}
			else if (bOut)	// out of ammo
			{
				if (level.framenum >= ent->pain_debounce_framenum)
				{
					gi.sound(ent, CHAN_VOICE, level.snd_noammo, 1, ATTN_NORM, 0);
					ent->pain_debounce_framenum = level.framenum + 1 * HZ;
				}
			}
		}
		else
		{

			if (ent->client->ps.fov != ent->client->desired_fov)
				ent->client->ps.fov = ent->client->desired_fov;
			// if they aren't at 90 then they must be zoomed, so remove their weapon from view
			if (ent->client->ps.fov != 90)
			{
				ent->client->ps.gunindex = 0;
				ent->client->no_sniper_display = 0;
			}

			if (ent->client->ps.gunframe == FRAME_IDLE_LAST)
			{
				ent->client->ps.gunframe = FRAME_IDLE_FIRST;
				return;
			}

			/*       if (pause_frames)
			   {
			   for (n = 0; pause_frames[n]; n++)
			   {
			   if (ent->client->ps.gunframe == pause_frames[n])
			   {
			   if (rand()&15)
			   return;
			   }
			   }
			   }
			 */
			ent->client->ps.gunframe++;
			ent->client->fired = 0;	// weapon ready and button not down, now they can fire again
			ent->client->burst = 0;
			ent->client->machinegun_shots = 0;
			return;
		}
	}

	if (ent->client->weaponstate == WEAPON_FIRING)
	{
		for (n = 0; fire_frames[n]; n++)
		{
			if (ent->client->ps.gunframe == fire_frames[n])
			{
				if (ent->client->quad_framenum > level.framenum)
					gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage3.wav"),
						1, ATTN_NORM, 0);

				fire(ent);
				break;
			}
		}

		if (!fire_frames[n])
			ent->client->ps.gunframe++;

		if (ent->client->ps.gunframe == FRAME_IDLE_FIRST + 1)
			ent->client->weaponstate = WEAPON_READY;
	}
	// player switched into 
		if (ent->client->weaponstate == WEAPON_BURSTING)
		{
			for (n = 0; fire_frames[n]; n++)
			{
				if (ent->client->ps.gunframe == fire_frames[n])
				{
					if (ent->client->quad_framenum > level.framenum)
						gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage3.wav"),
							1, ATTN_NORM, 0);
					{
						//                      gi.cprintf (ent, PRINT_HIGH, "Calling fire code, frame = %d.\n", ent->client->ps.gunframe);
						fire(ent);
					}
					break;
				}
			}

			if (!fire_frames[n])
				ent->client->ps.gunframe++;

			//gi.cprintf (ent, PRINT_HIGH, "Calling Q_stricmp, frame = %d.\n", ent->client->ps.gunframe);


			if (ent->client->ps.gunframe == FRAME_IDLE_FIRST + 1)
				ent->client->weaponstate = WEAPON_READY;

			if (ent->client->curr_weap == MP5_NUM)
			{
				if (ent->client->ps.gunframe >= 76)
				{
					ent->client->weaponstate = WEAPON_READY;
					ent->client->ps.gunframe = FRAME_IDLE_FIRST + 1;
				}
				//      gi.cprintf (ent, PRINT_HIGH, "Succes Q_stricmp now: frame = %d.\n", ent->client->ps.gunframe);

			}
			if (ent->client->curr_weap == M4_NUM)
			{
				if (ent->client->ps.gunframe >= 69)
				{
					ent->client->weaponstate = WEAPON_READY;
					ent->client->ps.gunframe = FRAME_IDLE_FIRST + 1;
				}
				//      gi.cprintf (ent, PRINT_HIGH, "Succes Q_stricmp now: frame = %d.\n", ent->client->ps.gunframe);

			}

		}
	}

	return false;
}

inline bool Weapon_HandleNewWeapon(edict_t *ent, int FRAME_DEACTIVATE_FIRST, int FRAME_DEACTIVATE_LAST)
{
	bool is_holstering = false;

	if (!g_instant_weapon_switch->integer)
		is_holstering = ((ent->client->latched_buttons | ent->client->buttons) & BUTTON_HOLSTER);

	if ((ent->client->newweapon || is_holstering) && (ent->client->weaponstate != WEAPON_FIRING))
	{
		if (g_instant_weapon_switch->integer || ent->client->weapon_think_time <= level.time)
		{
			if (!ent->client->newweapon)
				ent->client->newweapon = ent->client->pers.weapon;

			ent->client->weaponstate = WEAPON_DROPPING;

			if (g_instant_weapon_switch->integer)
			{
				ChangeWeapon(ent);
				return true;
			}

			ent->client->ps.gunframe = FRAME_DEACTIVATE_FIRST;

			if ((FRAME_DEACTIVATE_LAST - FRAME_DEACTIVATE_FIRST) < 4)
			{
				ent->client->anim_priority = ANIM_ATTACK | ANIM_REVERSED;
				if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
				{
					ent->s.frame = FRAME_crpain4 + 1;
					ent->client->anim_end = FRAME_crpain1;
				}
				else
				{
					ent->s.frame = FRAME_pain304 + 1;
					ent->client->anim_end = FRAME_pain301;
				}
				ent->client->anim_time = 0_ms;
			}

			ent->client->weapon_think_time = level.time + Weapon_AnimationTime(ent);
		}
		return true;
	}

	return false;
}

enum weapon_ready_state_t
{
	READY_NONE,
	READY_CHANGING,
	READY_FIRING
};

inline weapon_ready_state_t Weapon_HandleReady(edict_t *ent, int FRAME_FIRE_FIRST, int FRAME_IDLE_FIRST, int FRAME_IDLE_LAST, const int *pause_frames)
{
	if (ent->client->weaponstate == WEAPON_READY)
	{
		bool request_firing = ent->client->weapon_fire_buffered || ((ent->client->latched_buttons | ent->client->buttons) & BUTTON_ATTACK);

		if (request_firing && ent->client->weapon_fire_finished <= level.time)
		{
			ent->client->latched_buttons &= ~BUTTON_ATTACK;
			ent->client->weapon_think_time = level.time;

			if ((!ent->client->pers.weapon->ammo) ||
				(ent->client->pers.inventory[ent->client->pers.weapon->ammo] >= ent->client->pers.weapon->quantity))
			{
				ent->client->weaponstate = WEAPON_FIRING;
				return READY_FIRING;
			}
			else
			{
				NoAmmoWeaponChange(ent, true);
				return READY_CHANGING;
			}
		}
		else if (ent->client->weapon_think_time <= level.time)
		{
			ent->client->weapon_think_time = level.time + Weapon_AnimationTime(ent);

			if (ent->client->ps.gunframe == FRAME_IDLE_LAST)
			{
				ent->client->ps.gunframe = FRAME_IDLE_FIRST;
				return READY_CHANGING;
			}

			if (pause_frames)
				for (int n = 0; pause_frames[n]; n++)
					if (ent->client->ps.gunframe == pause_frames[n])
						if (irandom(16))
							return READY_CHANGING;

			ent->client->ps.gunframe++;
			return READY_CHANGING;
		}
	}

	return READY_NONE;
}

inline void Weapon_HandleFiring(edict_t *ent, int32_t FRAME_IDLE_FIRST, std::function<void()> fire_handler)
{
	Weapon_SetFinished(ent);

	if (ent->client->weapon_fire_buffered)
	{
		ent->client->buttons |= BUTTON_ATTACK;
		ent->client->weapon_fire_buffered = false;
	}

	fire_handler();

	if (ent->client->ps.gunframe == FRAME_IDLE_FIRST)
	{
		ent->client->weaponstate = WEAPON_READY;
		ent->client->weapon_fire_buffered = false;
	}

	ent->client->weapon_think_time = level.time + Weapon_AnimationTime(ent);
}

#define FRAME_FIRE_FIRST                (FRAME_ACTIVATE_LAST + 1)
#define FRAME_IDLE_FIRST                (FRAME_FIRE_LAST + 1)
#define FRAME_DEACTIVATE_FIRST  (FRAME_IDLE_LAST + 1)

#define FRAME_RELOAD_FIRST              (FRAME_DEACTIVATE_LAST +1)
#define FRAME_LASTRD_FIRST   (FRAME_RELOAD_LAST +1)

#define MK23MAG 12
#define MP5MAG  30
#define M4MAG   24
#define DUALMAG 24

void Weapon_Generic(edict_t *ent, int FRAME_ACTIVATE_LAST, int FRAME_FIRE_LAST, int FRAME_IDLE_LAST, int FRAME_DEACTIVATE_LAST, const int *pause_frames, const int *fire_frames, void (*fire)(edict_t *ent))
{
	int FRAME_FIRE_FIRST = (FRAME_ACTIVATE_LAST + 1);
	int FRAME_IDLE_FIRST = (FRAME_FIRE_LAST + 1);
	int FRAME_DEACTIVATE_FIRST = (FRAME_IDLE_LAST + 1);

	int n;
	int bFire = 0;
	int bOut = 0;

	if (ent->client->ctf_grapple && ent->client->weapon) {
		if (Q_stricmp(ent->client->weapon->pickup_name, "Grapple") == 0 &&
			ent->client->weaponstate == WEAPON_FIRING)
			return;
	}

	//FIREBLADE
	if (ent->client->weaponstate == WEAPON_FIRING &&
		((ent->solid == SOLID_NOT && ent->deadflag != DEAD_DEAD) ||
			lights_camera_action))
	{
		ent->client->weaponstate = WEAPON_READY;
	}
	//FIREBLADE

	  //+BD - Added Reloading weapon, done manually via a cmd
	if (ent->client->weaponstate == WEAPON_RELOADING)
	{
		if (ent->client->ps.gunframe < FRAME_RELOAD_FIRST
			|| ent->client->ps.gunframe > FRAME_RELOAD_LAST)
			ent->client->ps.gunframe = FRAME_RELOAD_FIRST;
		else if (ent->client->ps.gunframe < FRAME_RELOAD_LAST)
		{
			ent->client->ps.gunframe++;
			switch (ent->client->curr_weap)
			{
				//+BD - Check weapon to find out when to play reload sounds
			case MK23_NUM:
			{
				if (ent->client->ps.gunframe == 46)
					gi.sound(ent, CHAN_WEAPON,
						gi.soundindex("weapons/mk23out.wav"), 1,
						ATTN_NORM, 0);
				else if (ent->client->ps.gunframe == 53)
					gi.sound(ent, CHAN_WEAPON,
						gi.soundindex("weapons/mk23in.wav"), 1,
						ATTN_NORM, 0);
				else if (ent->client->ps.gunframe == 59)	// 3
					gi.sound(ent, CHAN_WEAPON,
						gi.soundindex("weapons/mk23slap.wav"), 1,
						ATTN_NORM, 0);
				break;
			}
			case MP5_NUM:
			{
				if (ent->client->ps.gunframe == 55)
					gi.sound(ent, CHAN_WEAPON,
						gi.soundindex("weapons/mp5out.wav"), 1,
						ATTN_NORM, 0);
				else if (ent->client->ps.gunframe == 59)
					gi.sound(ent, CHAN_WEAPON,
						gi.soundindex("weapons/mp5in.wav"), 1, ATTN_NORM,
						0);
				else if (ent->client->ps.gunframe == 63)	//61
					gi.sound(ent, CHAN_WEAPON,
						gi.soundindex("weapons/mp5slap.wav"), 1,
						ATTN_NORM, 0);
				break;
			}
			case M4_NUM:
			{
				if (ent->client->ps.gunframe == 52)
					gi.sound(ent, CHAN_WEAPON,
						gi.soundindex("weapons/m4a1out.wav"), 1,
						ATTN_NORM, 0);
				else if (ent->client->ps.gunframe == 58)
					gi.sound(ent, CHAN_WEAPON,
						gi.soundindex("weapons/m4a1in.wav"), 1,
						ATTN_NORM, 0);
				break;
			}
			case M3_NUM:
			{
				if (ent->client->shot_rds >= ent->client->shot_max)
				{
					ent->client->ps.gunframe = FRAME_IDLE_FIRST;
					ent->client->weaponstate = WEAPON_READY;
					return;
				}

				if (ent->client->ps.gunframe == 48)
				{
					gi.sound(ent, CHAN_WEAPON,
						gi.soundindex("weapons/m3in.wav"), 1,
						ATTN_NORM, 0);
				}
				if (ent->client->ps.gunframe == 49)
				{
					if (ent->client->fast_reload == 1)
					{
						ent->client->fast_reload = 0;
						ent->client->shot_rds++;
						ent->client->ps.gunframe = 44;
					}
				}
				break;
			}
			case HC_NUM:
			{
				if (ent->client->ps.gunframe == 64)
					gi.sound(ent, CHAN_WEAPON,
						gi.soundindex("weapons/copen.wav"), 1, ATTN_NORM,
						0);
				else if (ent->client->ps.gunframe == 67)
					gi.sound(ent, CHAN_WEAPON,
						gi.soundindex("weapons/cout.wav"), 1, ATTN_NORM,
						0);
				else if (ent->client->ps.gunframe == 76)	//61
					gi.sound(ent, CHAN_WEAPON,
						gi.soundindex("weapons/cin.wav"), 1, ATTN_NORM,
						0);
				else if (ent->client->ps.gunframe == 80)	// 3
					gi.sound(ent, CHAN_WEAPON,
						gi.soundindex("weapons/cclose.wav"), 1,
						ATTN_NORM, 0);
				break;
			}
			case SNIPER_NUM:
			{

				if (ent->client->sniper_rds >= ent->client->sniper_max)
				{
					ent->client->ps.gunframe = FRAME_IDLE_FIRST;
					ent->client->weaponstate = WEAPON_READY;
					return;
				}

				if (ent->client->ps.gunframe == 59)
				{
					gi.sound(ent, CHAN_WEAPON,
						gi.soundindex("weapons/ssgbolt.wav"), 1,
						ATTN_NORM, 0);
				}
				else if (ent->client->ps.gunframe == 71)
				{
					gi.sound(ent, CHAN_WEAPON,
						gi.soundindex("weapons/ssgin.wav"), 1,
						ATTN_NORM, 0);
				}
				else if (ent->client->ps.gunframe == 73)
				{
					if (ent->client->fast_reload == 1)
					{
						ent->client->fast_reload = 0;
						ent->client->sniper_rds++;
						ent->client->ps.gunframe = 67;
					}
				}
				else if (ent->client->ps.gunframe == 76)
				{
					gi.sound(ent, CHAN_WEAPON,
						gi.soundindex("weapons/ssgbolt.wav"), 1,
						ATTN_NORM, 0);
				}
				break;
			}
			case DUAL_NUM:
			{
				if (ent->client->ps.gunframe == 45)
					gi.sound(ent, CHAN_WEAPON,
						gi.soundindex("weapons/mk23out.wav"), 1,
						ATTN_NORM, 0);
				else if (ent->client->ps.gunframe == 53)
					gi.sound(ent, CHAN_WEAPON,
						gi.soundindex("weapons/mk23slap.wav"), 1,
						ATTN_NORM, 0);
				else if (ent->client->ps.gunframe == 60)	// 3
					gi.sound(ent, CHAN_WEAPON,
						gi.soundindex("weapons/mk23slap.wav"), 1,
						ATTN_NORM, 0);
				break;
			}
			default:
				gi.dprintf("No weapon choice for reloading (sounds).\n");
				break;

			}
		}
		else
		{
			ent->client->ps.gunframe = FRAME_IDLE_FIRST;
			ent->client->weaponstate = WEAPON_READY;
			switch (ent->client->curr_weap)
			{
			case MK23_NUM:
			{
				ent->client->dual_rds -= ent->client->mk23_rds;
				ent->client->mk23_rds = ent->client->mk23_max;
				ent->client->dual_rds += ent->client->mk23_max;
				(ent->client->inventory[ent->client->ammo_index])--;
				if (ent->client->inventory[ent->client->ammo_index] < 0)
				{
					ent->client->inventory[ent->client->ammo_index] = 0;
				}
				ent->client->fired = 0;	// reset any firing delays
				break;
				//else
				//      ent->client->mk23_rds = ent->client->inventory[ent->client->ammo_index];
			}
			case MP5_NUM:
			{

				ent->client->mp5_rds = ent->client->mp5_max;
				(ent->client->inventory[ent->client->ammo_index])--;
				if (ent->client->inventory[ent->client->ammo_index] < 0)
				{
					ent->client->inventory[ent->client->ammo_index] = 0;
				}
				ent->client->fired = 0;	// reset any firing delays
				ent->client->burst = 0;	// reset any bursting
				break;
			}
			case M4_NUM:
			{

				ent->client->m4_rds = ent->client->m4_max;
				(ent->client->inventory[ent->client->ammo_index])--;
				if (ent->client->inventory[ent->client->ammo_index] < 0)
				{
					ent->client->inventory[ent->client->ammo_index] = 0;
				}
				ent->client->fired = 0;	// reset any firing delays
				ent->client->burst = 0;	// reset any bursting                           
				ent->client->machinegun_shots = 0;
				break;
			}
			case M3_NUM:
			{
				ent->client->shot_rds++;
				(ent->client->inventory[ent->client->ammo_index])--;
				if (ent->client->inventory[ent->client->ammo_index] < 0)
				{
					ent->client->inventory[ent->client->ammo_index] = 0;
				}
				break;
			}
			case HC_NUM:
			{
				if (hc_single->value)
				{
					int count = 0;

					if (ent->client->cannon_rds == 1)
						count = 1;
					else if ((ent->client->inventory[ent->client->ammo_index]) == 1)
						count = 1;
					else
						count = ent->client->cannon_max;
					ent->client->cannon_rds += count;
					if (ent->client->cannon_rds > ent->client->cannon_max)
						ent->client->cannon_rds = ent->client->cannon_max;

					(ent->client->inventory[ent->client->ammo_index]) -= count;
				}
				else
				{
					ent->client->cannon_rds = ent->client->cannon_max;
					(ent->client->inventory[ent->client->ammo_index]) -= 2;
				}

				if (ent->client->inventory[ent->client->ammo_index] < 0)
				{
					ent->client->inventory[ent->client->ammo_index] = 0;
				}
				break;
			}
			case SNIPER_NUM:
			{
				ent->client->sniper_rds++;
				(ent->client->inventory[ent->client->ammo_index])--;
				if (ent->client->inventory[ent->client->ammo_index] < 0)
				{
					ent->client->inventory[ent->client->ammo_index] = 0;
				}
				return;
			}
			case DUAL_NUM:
			{
				ent->client->dual_rds = ent->client->dual_max;
				ent->client->mk23_rds = ent->client->mk23_max;
				(ent->client->inventory[ent->client->ammo_index]) -= 2;
				if (ent->client->inventory[ent->client->ammo_index] < 0)
				{
					ent->client->inventory[ent->client->ammo_index] = 0;
				}
				break;
			}
			default:
				gi.dprintf("No weapon choice for reloading.\n");
				break;
			}


		}
	}

	if (ent->client->weaponstate == WEAPON_END_MAG)
	{
		if (ent->client->ps.gunframe < FRAME_LASTRD_LAST)
			ent->client->ps.gunframe++;
		else
			ent->client->ps.gunframe = FRAME_LASTRD_LAST;
		// see if our weapon has ammo (from something other than reloading)
		if (
			((ent->client->curr_weap == MK23_NUM)
				&& (ent->client->mk23_rds > 0))
			|| ((ent->client->curr_weap == MP5_NUM)
				&& (ent->client->mp5_rds > 0))
			|| ((ent->client->curr_weap == M4_NUM) && (ent->client->m4_rds > 0))
			|| ((ent->client->curr_weap == M3_NUM)
				&& (ent->client->shot_rds > 0))
			|| ((ent->client->curr_weap == HC_NUM)
				&& (ent->client->cannon_rds > 0))
			|| ((ent->client->curr_weap == SNIPER_NUM)
				&& (ent->client->sniper_rds > 0))
			|| ((ent->client->curr_weap == DUAL_NUM)
				&& (ent->client->dual_rds > 0)))
		{
			ent->client->weaponstate = WEAPON_READY;
			ent->client->ps.gunframe = FRAME_IDLE_FIRST;
		}
		if (((ent->client->latched_buttons | ent->client->buttons) & BUTTON_ATTACK)
			&& (ent->solid != SOLID_NOT || ent->deadflag == DEAD_DEAD)
			&& !lights_camera_action && !ent->client->uvTime)
		{
			ent->client->latched_buttons &= ~BUTTON_ATTACK;
			{
				if (level.framenum >= ent->pain_debounce_framenum)
				{
					gi.sound(ent, CHAN_VOICE, level.snd_noammo, 1, ATTN_NORM, 0);
					ent->pain_debounce_framenum = level.framenum + 1 * HZ;
				}
			}
		}
	}

	if (!Weapon_CanAnimate(ent))
		return;

	if (Weapon_HandleDropping(ent, FRAME_DEACTIVATE_LAST))
		return;
	else if (Weapon_HandleActivating(ent, FRAME_ACTIVATE_LAST, FRAME_IDLE_FIRST))
		return;
	else if (Weapon_HandleNewWeapon(ent, FRAME_DEACTIVATE_FIRST, FRAME_DEACTIVATE_LAST))
		return;
	else if (auto state = Weapon_HandleReady(ent, FRAME_FIRE_FIRST, FRAME_IDLE_FIRST, FRAME_IDLE_LAST, pause_frames))
	{
		if (state == READY_FIRING)
		{
			ent->client->ps.gunframe = FRAME_FIRE_FIRST;
			ent->client->weapon_fire_buffered = false;

			if (ent->client->weapon_thunk)
				ent->client->weapon_think_time += FRAME_TIME_S;

			ent->client->weapon_think_time += Weapon_AnimationTime(ent);
			Weapon_SetFinished(ent);

			for (int n = 0; fire_frames[n]; n++)
			{
				if (ent->client->ps.gunframe == fire_frames[n])
				{
 					Weapon_PowerupSound(ent);
					fire(ent);
					break;
				}
			}

			// start the animation
			ent->client->anim_priority = ANIM_ATTACK;
			if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
			{
				ent->s.frame = FRAME_crattak1 - 1;
				ent->client->anim_end = FRAME_crattak9;
			}
			else
			{
				ent->s.frame = FRAME_attack1 - 1;
				ent->client->anim_end = FRAME_attack8;
			}
			ent->client->anim_time = 0_ms;
		}

		return;
	}

	if (ent->client->weaponstate == WEAPON_BANDAGING)
	{
		if (ent->client->ps.gunframe == FRAME_DEACTIVATE_LAST)
		{
			ent->client->weaponstate = WEAPON_BUSY;
			if (esp->value && esp_leaderenhance->value)
				ent->client->idle_weapon = ENHANCED_BANDAGE_TIME;
			else
				ent->client->idle_weapon = BANDAGE_TIME;
			return;
		}
		ent->client->ps.gunframe++;
		return;
	}

	if (ent->client->weaponstate == WEAPON_FIRING && ent->client->weapon_think_time <= level.time)
	{
		ent->client->ps.gunframe++;
		Weapon_HandleFiring(ent, FRAME_IDLE_FIRST, [&]() {
			for (int n = 0; fire_frames[n]; n++)
			{
				if (ent->client->ps.gunframe == fire_frames[n])
				{
					Weapon_PowerupSound(ent);
					fire(ent);
					break;
				}
			}
		});
	}
}

void Weapon_Repeating(edict_t *ent, int FRAME_ACTIVATE_LAST, int FRAME_FIRE_LAST, int FRAME_IDLE_LAST, int FRAME_DEACTIVATE_LAST, const int *pause_frames, void (*fire)(edict_t *ent))
{
	int FRAME_FIRE_FIRST = (FRAME_ACTIVATE_LAST + 1);
	int FRAME_IDLE_FIRST = (FRAME_FIRE_LAST + 1);
	int FRAME_DEACTIVATE_FIRST = (FRAME_IDLE_LAST + 1);

	if (!Weapon_CanAnimate(ent))
		return;

	if (Weapon_HandleDropping(ent, FRAME_DEACTIVATE_LAST))
		return;
	else if (Weapon_HandleActivating(ent, FRAME_ACTIVATE_LAST, FRAME_IDLE_FIRST))
		return;
	else if (Weapon_HandleNewWeapon(ent, FRAME_DEACTIVATE_FIRST, FRAME_DEACTIVATE_LAST))
		return;
	else if (Weapon_HandleReady(ent, FRAME_FIRE_FIRST, FRAME_IDLE_FIRST, FRAME_IDLE_LAST, pause_frames) == READY_CHANGING)
		return;

	if (ent->client->weaponstate == WEAPON_FIRING && ent->client->weapon_think_time <= level.time)
	{
		Weapon_HandleFiring(ent, FRAME_IDLE_FIRST, [&]() { fire(ent); });

		if (ent->client->weapon_thunk)
			ent->client->weapon_think_time += FRAME_TIME_S;
	}
}

/*
======================================================================

GRENADE

======================================================================
*/

void weapon_grenade_fire(edict_t *ent, bool held)
{
	int	  damage = 125;
	int	  speed;
	float radius;

	radius = (float) (damage + 40);
	if (is_quad)
		damage *= damage_multiplier;

	vec3_t start, dir;
	// Paril: kill sideways angle on grenades
	// limit upwards angle so you don't throw behind you
	P_ProjectSource(ent, { max(-62.5f, ent->client->v_angle[0]), ent->client->v_angle[1], ent->client->v_angle[2] }, { 2, 0, -14 }, start, dir);

	gtime_t timer = ent->client->grenade_time - level.time;
	speed = (int) (ent->health <= 0 ? GRENADE_MINSPEED : min(GRENADE_MINSPEED + (GRENADE_TIMER - timer).seconds() * ((GRENADE_MAXSPEED - GRENADE_MINSPEED) / GRENADE_TIMER.seconds()), GRENADE_MAXSPEED));

	ent->client->grenade_time = 0_ms;

	fire_grenade2(ent, start, dir, damage, speed, timer, radius, held);

	G_RemoveAmmo(ent, 1);
}

void Throw_Generic(edict_t *ent, int FRAME_FIRE_LAST, int FRAME_IDLE_LAST, int FRAME_PRIME_SOUND,
					const char *prime_sound,
					int FRAME_THROW_HOLD, int FRAME_THROW_FIRE, const int *pause_frames, int EXPLODE,
					const char *primed_sound,
					void (*fire)(edict_t *ent, bool held), bool extra_idle_frame)
{
	// when we die, just toss what we had in our hands.
	if (ent->health <= 0)
	{
		fire(ent, true);
		return;
	}

	int n;
	int FRAME_IDLE_FIRST = (FRAME_FIRE_LAST + 1);

	if (ent->client->newweapon && (ent->client->weaponstate == WEAPON_READY))
	{
		if (ent->client->weapon_think_time <= level.time)
		{
			ChangeWeapon(ent);
			ent->client->weapon_think_time = level.time + Weapon_AnimationTime(ent);
		}
		return;
	}

	if (ent->client->weaponstate == WEAPON_ACTIVATING)
	{
		if (ent->client->weapon_think_time <= level.time)
		{
			ent->client->weaponstate = WEAPON_READY;
			if (!extra_idle_frame)
				ent->client->ps.gunframe = FRAME_IDLE_FIRST;
			else
				ent->client->ps.gunframe = FRAME_IDLE_LAST + 1;
			ent->client->weapon_think_time = level.time + Weapon_AnimationTime(ent);
			Weapon_SetFinished(ent);
		}
		return;
	}

	if (ent->client->weaponstate == WEAPON_READY)
	{
		bool request_firing = ent->client->weapon_fire_buffered || ((ent->client->latched_buttons | ent->client->buttons) & BUTTON_ATTACK);

		if (request_firing && ent->client->weapon_fire_finished <= level.time)
		{
			ent->client->latched_buttons &= ~BUTTON_ATTACK;

			if (ent->client->pers.inventory[ent->client->pers.weapon->ammo])
			{
				ent->client->ps.gunframe = 1;
				ent->client->weaponstate = WEAPON_FIRING;
				ent->client->grenade_time = 0_ms;
				ent->client->weapon_think_time = level.time + Weapon_AnimationTime(ent);
			}
			else
				NoAmmoWeaponChange(ent, true);
			return;
		}
		else if (ent->client->weapon_think_time <= level.time)
		{
			ent->client->weapon_think_time = level.time + Weapon_AnimationTime(ent);

			if (ent->client->ps.gunframe >= FRAME_IDLE_LAST)
			{
				ent->client->ps.gunframe = FRAME_IDLE_FIRST;
				return;
			}

			if (pause_frames)
			{
				for (n = 0; pause_frames[n]; n++)
				{
					if (ent->client->ps.gunframe == pause_frames[n])
					{
						if (irandom(16))
							return;
					}
				}
			}

			ent->client->ps.gunframe++;
		}
		return;
	}

	if (ent->client->weaponstate == WEAPON_FIRING)
	{
		if (ent->client->weapon_think_time <= level.time)
		{
			if (prime_sound && ent->client->ps.gunframe == FRAME_PRIME_SOUND)
				gi.sound(ent, CHAN_WEAPON, gi.soundindex(prime_sound), 1, ATTN_NORM, 0);

			// [Paril-KEX] dualfire/time accel
			gtime_t grenade_wait_time = 1_sec;

			if (CTFApplyHaste(ent))
				grenade_wait_time *= 0.5f;
			if (is_quadfire)
				grenade_wait_time *= 0.5f;

			if (ent->client->ps.gunframe == FRAME_THROW_HOLD)
			{
				if (!ent->client->grenade_time && !ent->client->grenade_finished_time)
				{
					ent->client->grenade_time = level.time + GRENADE_TIMER + 200_ms;

					if (primed_sound)
						ent->client->weapon_sound = gi.soundindex(primed_sound);
				}

				// they waited too long, detonate it in their hand
				if (EXPLODE && !ent->client->grenade_blew_up && level.time >= ent->client->grenade_time)
				{
					Weapon_PowerupSound(ent);
					ent->client->weapon_sound = 0;
					fire(ent, true);
					ent->client->grenade_blew_up = true;

					ent->client->grenade_finished_time = level.time + grenade_wait_time;
				}

				if (ent->client->buttons & BUTTON_ATTACK)
				{
					ent->client->weapon_think_time = level.time + 1_ms;
					return;
				}

				if (ent->client->grenade_blew_up)
				{
					if (level.time >= ent->client->grenade_finished_time)
					{
						ent->client->ps.gunframe = FRAME_FIRE_LAST;
						ent->client->grenade_blew_up = false;
						ent->client->weapon_think_time = level.time + Weapon_AnimationTime(ent);
					}
					else
					{
						return;
					}
				}
				else
				{
					ent->client->ps.gunframe++;

					Weapon_PowerupSound(ent);
					ent->client->weapon_sound = 0;
					fire(ent, false);

					if (!EXPLODE || !ent->client->grenade_blew_up)
						ent->client->grenade_finished_time = level.time + grenade_wait_time;

					if (!ent->deadflag && ent->s.modelindex == MODELINDEX_PLAYER && ent->health > 0) // VWep animations screw up corpses
					{
						if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
						{
							ent->client->anim_priority = ANIM_ATTACK;
							ent->s.frame = FRAME_crattak1 - 1;
							ent->client->anim_end = FRAME_crattak3;
						}
						else
						{
							ent->client->anim_priority = ANIM_ATTACK | ANIM_REVERSED;
							ent->s.frame = FRAME_wave08;
							ent->client->anim_end = FRAME_wave01;
						}
						ent->client->anim_time = 0_ms;
					}
				}
			}

			ent->client->weapon_think_time = level.time + Weapon_AnimationTime(ent);

			if ((ent->client->ps.gunframe == FRAME_FIRE_LAST) && (level.time < ent->client->grenade_finished_time))
				return;

			ent->client->ps.gunframe++;

			if (ent->client->ps.gunframe == FRAME_IDLE_FIRST)
			{
				ent->client->grenade_finished_time = 0_ms;
				ent->client->weaponstate = WEAPON_READY;
				ent->client->weapon_fire_buffered = false;
				Weapon_SetFinished(ent);
				
				if (extra_idle_frame)
					ent->client->ps.gunframe = FRAME_IDLE_LAST + 1;

				// Paril: if we ran out of the throwable, switch
				// so we don't appear to be holding one that we
				// can't throw
				if (!ent->client->pers.inventory[ent->client->pers.weapon->ammo])
				{
					NoAmmoWeaponChange(ent, false);
					ChangeWeapon(ent);
				}
			}
		}
	}
}

void Weapon_Grenade(edict_t *ent)
{
	constexpr int pause_frames[] = { 29, 34, 39, 48, 0 };

	Throw_Generic(ent, 15, 48, 5, "weapons/hgrena1b.wav", 11, 12, pause_frames, true, "weapons/hgrenc1b.wav", weapon_grenade_fire, true);

	// [Paril-KEX] skip the duped frame
	if (ent->client->ps.gunframe == 1)
		ent->client->ps.gunframe = 2;
}

/*
======================================================================

GRENADE LAUNCHER

======================================================================
*/

void weapon_grenadelauncher_fire(edict_t *ent)
{
	int	  damage = 120;
	float radius;

	radius = (float) (damage + 40);
	if (is_quad)
		damage *= damage_multiplier;

	vec3_t start, dir;
	// Paril: kill sideways angle on grenades
	// limit upwards angle so you don't fire it behind you
	P_ProjectSource(ent, { max(-62.5f, ent->client->v_angle[0]), ent->client->v_angle[1], ent->client->v_angle[2] }, { 8, 0, -8 }, start, dir);

	P_AddWeaponKick(ent, ent->client->v_forward * -2, { -1.f, 0.f, 0.f });

	fire_grenade(ent, start, dir, damage, 600, 2.5_sec, radius, (crandom_open() * 10.0f), (200 + crandom_open() * 10.0f), false);

	gi.WriteByte(svc_muzzleflash);
	gi.WriteEntity(ent);
	gi.WriteByte(MZ_GRENADE | is_silenced);
	gi.multicast(ent->s.origin, MULTICAST_PVS, false);

	PlayerNoise(ent, start, PNOISE_WEAPON);
	
	G_RemoveAmmo(ent);
}

void Weapon_GrenadeLauncher(edict_t *ent)
{
	constexpr int pause_frames[] = { 34, 51, 59, 0 };
	constexpr int fire_frames[] = { 6, 0 };

	Weapon_Generic(ent, 5, 16, 59, 64, pause_frames, fire_frames, weapon_grenadelauncher_fire);
}

/*
======================================================================

ROCKET

======================================================================
*/

void Weapon_RocketLauncher_Fire(edict_t *ent)
{
	int	  damage;
	float damage_radius;
	int	  radius_damage;

	damage = irandom(100, 120);
	radius_damage = 120;
	damage_radius = 120;
	if (is_quad)
	{
		damage *= damage_multiplier;
		radius_damage *= damage_multiplier;
	}

	vec3_t start, dir;
	P_ProjectSource(ent, ent->client->v_angle, { 8, 8, -8 }, start, dir);
	fire_rocket(ent, start, dir, damage, 650, damage_radius, radius_damage);

	P_AddWeaponKick(ent, ent->client->v_forward * -2, { -1.f, 0.f, 0.f });

	// send muzzle flash
	gi.WriteByte(svc_muzzleflash);
	gi.WriteEntity(ent);
	gi.WriteByte(MZ_ROCKET | is_silenced);
	gi.multicast(ent->s.origin, MULTICAST_PVS, false);

	PlayerNoise(ent, start, PNOISE_WEAPON);
	
	G_RemoveAmmo(ent);
}

void Weapon_RocketLauncher(edict_t *ent)
{
	constexpr int pause_frames[] = { 25, 33, 42, 50, 0 };
	constexpr int fire_frames[] = { 5, 0 };

	Weapon_Generic(ent, 4, 12, 50, 54, pause_frames, fire_frames, Weapon_RocketLauncher_Fire);
}

/*
======================================================================

BLASTER / HYPERBLASTER

======================================================================
*/

void Blaster_Fire(edict_t *ent, const vec3_t &g_offset, int damage, bool hyper, effects_t effect)
{
	if (is_quad)
		damage *= damage_multiplier;

	vec3_t start, dir;
	P_ProjectSource(ent, ent->client->v_angle, vec3_t{ 24, 8, -8 } + g_offset, start, dir);

	if (hyper)
		P_AddWeaponKick(ent, ent->client->v_forward * -2, { crandom() * 0.7f, crandom() * 0.7f, crandom() * 0.7f });
	else
		P_AddWeaponKick(ent, ent->client->v_forward * -2, { -1.f, 0.f, 0.f });

	// let the regular blaster projectiles travel a bit faster because it is a completely useless gun
	int speed = hyper ? 1000 : 1500;

	fire_blaster(ent, start, dir, damage, speed, effect, hyper ? MOD_HYPERBLASTER : MOD_BLASTER);

	// send muzzle flash
	gi.WriteByte(svc_muzzleflash);
	gi.WriteEntity(ent);
	if (hyper)
		gi.WriteByte(MZ_HYPERBLASTER | is_silenced);
	else
		gi.WriteByte(MZ_BLASTER | is_silenced);
	gi.multicast(ent->s.origin, MULTICAST_PVS, false);

	PlayerNoise(ent, start, PNOISE_WEAPON);
}

void Weapon_Blaster_Fire(edict_t *ent)
{
	// give the blaster 15 across the board instead of just in dm
	int damage = 15;
	Blaster_Fire(ent, vec3_origin, damage, false, EF_BLASTER);
}

void Weapon_Blaster(edict_t *ent)
{
	constexpr int pause_frames[] = { 19, 32, 0 };
	constexpr int fire_frames[] = { 5, 0 };

	Weapon_Generic(ent, 4, 8, 52, 55, pause_frames, fire_frames, Weapon_Blaster_Fire);
}

void Weapon_HyperBlaster_Fire(edict_t *ent)
{
	float	  rotation;
	vec3_t	  offset;
	int		  damage;

	// start on frame 6
	if (ent->client->ps.gunframe > 20)
		ent->client->ps.gunframe = 6;
	else
		ent->client->ps.gunframe++;

	// if we reached end of loop, have ammo & holding attack, reset loop
	// otherwise play wind down
	if (ent->client->ps.gunframe == 12)
	{
		if (ent->client->pers.inventory[ent->client->pers.weapon->ammo] && (ent->client->buttons & BUTTON_ATTACK))
			ent->client->ps.gunframe = 6;
		else
			gi.sound(ent, CHAN_AUTO, gi.soundindex("weapons/hyprbd1a.wav"), 1, ATTN_NORM, 0);
	}

	// play weapon sound for firing loop
	if (ent->client->ps.gunframe >= 6 && ent->client->ps.gunframe <= 11)
		ent->client->weapon_sound = gi.soundindex("weapons/hyprbl1a.wav");
	else
		ent->client->weapon_sound = 0;

	// fire frames
	bool request_firing = ent->client->weapon_fire_buffered || (ent->client->buttons & BUTTON_ATTACK);

	if (request_firing)
	{
		if (ent->client->ps.gunframe >= 6 && ent->client->ps.gunframe <= 11)
		{
			ent->client->weapon_fire_buffered = false;

			if (!ent->client->pers.inventory[ent->client->pers.weapon->ammo])
			{
				NoAmmoWeaponChange(ent, true);
				return;
			}

			rotation = (ent->client->ps.gunframe - 5) * 2 * PIf / 6;
			offset[0] = -4 * sinf(rotation);
			offset[2] = 0;
			offset[1] = 4 * cosf(rotation);

			if (deathmatch->integer)
				damage = 15;
			else
				damage = 20;
			Blaster_Fire(ent, offset, damage, true, (ent->client->ps.gunframe % 4) ? EF_NONE : EF_HYPERBLASTER);
			Weapon_PowerupSound(ent);

			G_RemoveAmmo(ent);

			ent->client->anim_priority = ANIM_ATTACK;
			if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
			{
				ent->s.frame = FRAME_crattak1 - (int) (frandom() + 0.25f);
				ent->client->anim_end = FRAME_crattak9;
			}
			else
			{
				ent->s.frame = FRAME_attack1 - (int) (frandom() + 0.25f);
				ent->client->anim_end = FRAME_attack8;
			}
			ent->client->anim_time = 0_ms;
		}
	}
}

void Weapon_HyperBlaster(edict_t *ent)
{
	constexpr int pause_frames[] = { 0 };

	Weapon_Repeating(ent, 5, 20, 49, 53, pause_frames, Weapon_HyperBlaster_Fire);
}

/*
======================================================================

MACHINEGUN / CHAINGUN

======================================================================
*/

void Machinegun_Fire(edict_t *ent)
{
	int i;
	int damage = 8;
	int kick = 2;

	if (!(ent->client->buttons & BUTTON_ATTACK))
	{
		ent->client->machinegun_shots = 0;
		ent->client->ps.gunframe = 6;
		return;
	}

	if (ent->client->ps.gunframe == 4)
		ent->client->ps.gunframe = 5;
	else
		ent->client->ps.gunframe = 4;

	if (ent->client->pers.inventory[ent->client->pers.weapon->ammo] < 1)
	{
		ent->client->ps.gunframe = 6;
		NoAmmoWeaponChange(ent, true);
		return;
	}

	if (is_quad)
	{
		damage *= damage_multiplier;
		kick *= damage_multiplier;
	}

	vec3_t kick_origin {}, kick_angles {};
	for (i = 0; i < 3; i++)
	{
		kick_origin[i] = crandom() * 0.35f;
		kick_angles[i] = crandom() * 0.7f;
	}
	//kick_angles[0] = ent->client->machinegun_shots * -1.5f;
	P_AddWeaponKick(ent, kick_origin, kick_angles);

	// raise the gun as it is firing
	// [Paril-KEX] disabled as this is a bit hard to do with high
	// tickrate, but it also just sucks in general.
	/*if (!deathmatch->integer)
	{
		ent->client->machinegun_shots++;
		if (ent->client->machinegun_shots > 9)
			ent->client->machinegun_shots = 9;
	}*/

	// get start / end positions
	vec3_t start, dir;
	// Paril: kill sideways angle on hitscan
	P_ProjectSource(ent, ent->client->v_angle, { 0, 0, -8 }, start, dir);
	G_LagCompensate(ent, start, dir);
	fire_bullet(ent, start, dir, damage, kick, DEFAULT_BULLET_HSPREAD, DEFAULT_BULLET_VSPREAD, MOD_MACHINEGUN);
	G_UnLagCompensate();
	Weapon_PowerupSound(ent);

	gi.WriteByte(svc_muzzleflash);
	gi.WriteEntity(ent);
	gi.WriteByte(MZ_MACHINEGUN | is_silenced);
	gi.multicast(ent->s.origin, MULTICAST_PVS, false);

	PlayerNoise(ent, start, PNOISE_WEAPON);
	
	G_RemoveAmmo(ent);

	ent->client->anim_priority = ANIM_ATTACK;
	if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
	{
		ent->s.frame = FRAME_crattak1 - (int) (frandom() + 0.25f);
		ent->client->anim_end = FRAME_crattak9;
	}
	else
	{
		ent->s.frame = FRAME_attack1 - (int) (frandom() + 0.25f);
		ent->client->anim_end = FRAME_attack8;
	}
	ent->client->anim_time = 0_ms;
}

void Weapon_Machinegun(edict_t *ent)
{
	constexpr int pause_frames[] = { 23, 45, 0 };

	Weapon_Repeating(ent, 3, 5, 45, 49, pause_frames, Machinegun_Fire);
}

void Chaingun_Fire(edict_t *ent)
{
	int	  i;
	int	  shots;
	float r, u;
	int	  damage;
	int	  kick = 2;

	if (deathmatch->integer)
		damage = 6;
	else
		damage = 8;

	if (ent->client->ps.gunframe > 31)
	{
		ent->client->ps.gunframe = 5;
		gi.sound(ent, CHAN_AUTO, gi.soundindex("weapons/chngnu1a.wav"), 1, ATTN_IDLE, 0);
	}
	else if ((ent->client->ps.gunframe == 14) && !(ent->client->buttons & BUTTON_ATTACK))
	{
		ent->client->ps.gunframe = 32;
		ent->client->weapon_sound = 0;
		return;
	}
	else if ((ent->client->ps.gunframe == 21) && (ent->client->buttons & BUTTON_ATTACK) && ent->client->pers.inventory[ent->client->pers.weapon->ammo])
	{
		ent->client->ps.gunframe = 15;
	}
	else
	{
		ent->client->ps.gunframe++;
	}

	if (ent->client->ps.gunframe == 22)
	{
		ent->client->weapon_sound = 0;
		gi.sound(ent, CHAN_AUTO, gi.soundindex("weapons/chngnd1a.wav"), 1, ATTN_IDLE, 0);
	}

	if (ent->client->ps.gunframe < 5 || ent->client->ps.gunframe > 21)
		return;

	ent->client->weapon_sound = gi.soundindex("weapons/chngnl1a.wav");

	ent->client->anim_priority = ANIM_ATTACK;
	if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
	{
		ent->s.frame = FRAME_crattak1 - (ent->client->ps.gunframe & 1);
		ent->client->anim_end = FRAME_crattak9;
	}
	else
	{
		ent->s.frame = FRAME_attack1 - (ent->client->ps.gunframe & 1);
		ent->client->anim_end = FRAME_attack8;
	}
	ent->client->anim_time = 0_ms;

	if (ent->client->ps.gunframe <= 9)
		shots = 1;
	else if (ent->client->ps.gunframe <= 14)
	{
		if (ent->client->buttons & BUTTON_ATTACK)
			shots = 2;
		else
			shots = 1;
	}
	else
		shots = 3;

	if (ent->client->pers.inventory[ent->client->pers.weapon->ammo] < shots)
		shots = ent->client->pers.inventory[ent->client->pers.weapon->ammo];

	if (!shots)
	{
		NoAmmoWeaponChange(ent, true);
		return;
	}

	if (is_quad)
	{
		damage *= damage_multiplier;
		kick *= damage_multiplier;
	}

	vec3_t kick_origin {}, kick_angles {};
	for (i = 0; i < 3; i++)
	{
		kick_origin[i] = crandom() * 0.35f;
		kick_angles[i] = crandom() * (0.5f + (shots * 0.15f));
	}
	P_AddWeaponKick(ent, kick_origin, kick_angles);

	vec3_t start, dir;
	P_ProjectSource(ent, ent->client->v_angle, { 0, 0, -8 }, start, dir);

	G_LagCompensate(ent, start, dir);
	for (i = 0; i < shots; i++)
	{
		// get start / end positions
		// Paril: kill sideways angle on hitscan
		r = crandom() * 4;
		u = crandom() * 4;
		P_ProjectSource(ent, ent->client->v_angle, { 0, r, u + -8 }, start, dir);

		fire_bullet(ent, start, dir, damage, kick, DEFAULT_BULLET_HSPREAD, DEFAULT_BULLET_VSPREAD, MOD_CHAINGUN);
	}
	G_UnLagCompensate();

	Weapon_PowerupSound(ent);

	// send muzzle flash
	gi.WriteByte(svc_muzzleflash);
	gi.WriteEntity(ent);
	gi.WriteByte((MZ_CHAINGUN1 + shots - 1) | is_silenced);
	gi.multicast(ent->s.origin, MULTICAST_PVS, false);

	PlayerNoise(ent, start, PNOISE_WEAPON);
	
	G_RemoveAmmo(ent, shots);
}

void Weapon_Chaingun(edict_t *ent)
{
	constexpr int pause_frames[] = { 38, 43, 51, 61, 0 };

	Weapon_Repeating(ent, 4, 31, 61, 64, pause_frames, Chaingun_Fire);
}

/*
======================================================================

SHOTGUN / SUPERSHOTGUN

======================================================================
*/

void weapon_shotgun_fire(edict_t *ent)
{
	int damage = 4;
	int kick = 8;

	vec3_t start, dir;
	// Paril: kill sideways angle on hitscan
	P_ProjectSource(ent, ent->client->v_angle, { 0, 0, -8 }, start, dir);

	P_AddWeaponKick(ent, ent->client->v_forward * -2, { -2.f, 0.f, 0.f });

	if (is_quad)
	{
		damage *= damage_multiplier;
		kick *= damage_multiplier;
	}

	G_LagCompensate(ent, start, dir);
	if (deathmatch->integer)
		fire_shotgun(ent, start, dir, damage, kick, 500, 500, DEFAULT_DEATHMATCH_SHOTGUN_COUNT, MOD_SHOTGUN);
	else
		fire_shotgun(ent, start, dir, damage, kick, 500, 500, DEFAULT_SHOTGUN_COUNT, MOD_SHOTGUN);
	G_UnLagCompensate();

	// send muzzle flash
	gi.WriteByte(svc_muzzleflash);
	gi.WriteEntity(ent);
	gi.WriteByte(MZ_SHOTGUN | is_silenced);
	gi.multicast(ent->s.origin, MULTICAST_PVS, false);

	PlayerNoise(ent, start, PNOISE_WEAPON);
	
	G_RemoveAmmo(ent);
}

void Weapon_Shotgun(edict_t *ent)
{
	constexpr int pause_frames[] = { 22, 28, 34, 0 };
	constexpr int fire_frames[] = { 8, 0 };

	Weapon_Generic(ent, 7, 18, 36, 39, pause_frames, fire_frames, weapon_shotgun_fire);
}

void weapon_supershotgun_fire(edict_t *ent)
{
	int damage = 6;
	int kick = 12;

	if (is_quad)
	{
		damage *= damage_multiplier;
		kick *= damage_multiplier;
	}
	
	vec3_t start, dir;
	// Paril: kill sideways angle on hitscan
	P_ProjectSource(ent, ent->client->v_angle, { 0, 0, -8 }, start, dir);
	G_LagCompensate(ent, start, dir);
	vec3_t v;
	v[PITCH] = ent->client->v_angle[PITCH];
	v[YAW] = ent->client->v_angle[YAW] - 5;
	v[ROLL] = ent->client->v_angle[ROLL];
	// Paril: kill sideways angle on hitscan
	P_ProjectSource(ent, v, { 0, 0, -8 }, start, dir);
	fire_shotgun(ent, start, dir, damage, kick, DEFAULT_SHOTGUN_HSPREAD, DEFAULT_SHOTGUN_VSPREAD, DEFAULT_SSHOTGUN_COUNT / 2, MOD_SSHOTGUN);
	v[YAW] = ent->client->v_angle[YAW] + 5;
	P_ProjectSource(ent, v, { 0, 0, -8 }, start, dir);
	fire_shotgun(ent, start, dir, damage, kick, DEFAULT_SHOTGUN_HSPREAD, DEFAULT_SHOTGUN_VSPREAD, DEFAULT_SSHOTGUN_COUNT / 2, MOD_SSHOTGUN);
	G_UnLagCompensate();

	P_AddWeaponKick(ent, ent->client->v_forward * -2, { -2.f, 0.f, 0.f });

	// send muzzle flash
	gi.WriteByte(svc_muzzleflash);
	gi.WriteEntity(ent);
	gi.WriteByte(MZ_SSHOTGUN | is_silenced);
	gi.multicast(ent->s.origin, MULTICAST_PVS, false);

	PlayerNoise(ent, start, PNOISE_WEAPON);
	
	G_RemoveAmmo(ent);
}

void Weapon_SuperShotgun(edict_t *ent)
{
	constexpr int pause_frames[] = { 29, 42, 57, 0 };
	constexpr int fire_frames[] = { 7, 0 };

	Weapon_Generic(ent, 6, 17, 57, 61, pause_frames, fire_frames, weapon_supershotgun_fire);
}

/*
======================================================================

RAILGUN

======================================================================
*/

void weapon_railgun_fire(edict_t *ent)
{
	int damage = 100;
	int kick = 200;

	if (is_quad)
	{
		damage *= damage_multiplier;
		kick *= damage_multiplier;
	}

	vec3_t start, dir;
	P_ProjectSource(ent, ent->client->v_angle, { 0, 7, -8 }, start, dir);
	G_LagCompensate(ent, start, dir);
	fire_rail(ent, start, dir, damage, kick);
	G_UnLagCompensate();

	P_AddWeaponKick(ent, ent->client->v_forward * -3, { -3.f, 0.f, 0.f });

	// send muzzle flash
	gi.WriteByte(svc_muzzleflash);
	gi.WriteEntity(ent);
	gi.WriteByte(MZ_RAILGUN | is_silenced);
	gi.multicast(ent->s.origin, MULTICAST_PVS, false);

	PlayerNoise(ent, start, PNOISE_WEAPON);
	
	G_RemoveAmmo(ent);
}

void Weapon_Railgun(edict_t *ent)
{
	constexpr int pause_frames[] = { 56, 0 };
	constexpr int fire_frames[] = { 4, 0 };

	Weapon_Generic(ent, 3, 18, 56, 61, pause_frames, fire_frames, weapon_railgun_fire);
}

/*
======================================================================

BFG10K

======================================================================
*/

void weapon_bfg_fire(edict_t *ent)
{
	int	  damage;
	float damage_radius = 1000;

	if (deathmatch->integer)
		damage = 200;
	else
		damage = 500;

	if (ent->client->ps.gunframe == 9)
	{
		// send muzzle flash
		gi.WriteByte(svc_muzzleflash);
		gi.WriteEntity(ent);
		gi.WriteByte(MZ_BFG | is_silenced);
		gi.multicast(ent->s.origin, MULTICAST_PVS, false);

		PlayerNoise(ent, ent->s.origin, PNOISE_WEAPON);
		return;
	}

	// cells can go down during windup (from power armor hits), so
	// check again and abort firing if we don't have enough now
	if (ent->client->pers.inventory[ent->client->pers.weapon->ammo] < 50)
		return;

	if (is_quad)
		damage *= damage_multiplier;

	vec3_t start, dir;
	P_ProjectSource(ent, ent->client->v_angle, { 8, 8, -8 }, start, dir);
	fire_bfg(ent, start, dir, damage, 400, damage_radius);

	P_AddWeaponKick(ent, ent->client->v_forward * -2, { -20.f, 0, crandom() * 8 });
	ent->client->kick.total = DAMAGE_TIME();
	ent->client->kick.time = level.time + ent->client->kick.total;

	// send muzzle flash
	gi.WriteByte(svc_muzzleflash);
	gi.WriteEntity(ent);
	gi.WriteByte(MZ_BFG2 | is_silenced);
	gi.multicast(ent->s.origin, MULTICAST_PVS, false);

	PlayerNoise(ent, start, PNOISE_WEAPON);
	
	G_RemoveAmmo(ent);
}

void Weapon_BFG(edict_t *ent)
{
	constexpr int pause_frames[] = { 39, 45, 50, 55, 0 };
	constexpr int fire_frames[] = { 9, 17, 0 };

	Weapon_Generic(ent, 8, 32, 54, 58, pause_frames, fire_frames, weapon_bfg_fire);
}

//======================================================================

void weapon_disint_fire(edict_t *self)
{
	vec3_t start, dir;
	P_ProjectSource(self, self->client->v_angle, { 24, 8, -8 }, start, dir);

	P_AddWeaponKick(self, self->client->v_forward * -2, { -1.f, 0.f, 0.f });

	fire_disintegrator(self, start, dir, 800);

	// send muzzle flash
	gi.WriteByte(svc_muzzleflash);
	gi.WriteEntity(self);
	gi.WriteByte(MZ_BLASTER2);
	gi.multicast(self->s.origin, MULTICAST_PVS, false);

	PlayerNoise(self, start, PNOISE_WEAPON);

	G_RemoveAmmo(self);
}

void Weapon_Beta_Disintegrator(edict_t *ent)
{
	constexpr int pause_frames[] = { 30, 37, 45, 0 };
	constexpr int fire_frames[] = { 17, 0 };

	Weapon_Generic(ent, 16, 23, 46, 50, pause_frames, fire_frames, weapon_disint_fire);
}
