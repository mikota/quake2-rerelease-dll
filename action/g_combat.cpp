// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// g_combat.c

#include "g_local.h"

/*
============
CanDamage

Returns true if the inflictor can directly damage the target.  Used for
explosions and melee attacks.
============
*/
bool CanDamage(edict_t *targ, edict_t *inflictor)
{
	vec3_t	dest;
	trace_t trace;
	
	// bmodels need special checking because their origin is 0,0,0
	vec3_t inflictor_center;
	
	if (inflictor->linked)
		inflictor_center = (inflictor->absmin + inflictor->absmax) * 0.5f;
	else
		inflictor_center = inflictor->s.origin;
	
	if (targ->solid == SOLID_BSP)
	{
		dest = closest_point_to_box(inflictor_center, targ->absmin, targ->absmax);

		trace = gi.traceline(inflictor_center, dest, inflictor, MASK_SOLID);
		if (trace.fraction == 1.0f)
			return true;
	}

	vec3_t targ_center;
	
	if (targ->linked)
		targ_center = (targ->absmin + targ->absmax) * 0.5f;
	else
		targ_center = targ->s.origin;

	trace = gi.traceline(inflictor_center, targ_center, inflictor, MASK_SOLID);
	if (trace.fraction == 1.0f)
		return true;

	dest = targ_center;
	dest[0] += 15.0f;
	dest[1] += 15.0f;
	trace = gi.traceline(inflictor_center, dest, inflictor, MASK_SOLID);
	if (trace.fraction == 1.0f)
		return true;

	dest = targ_center;
	dest[0] += 15.0f;
	dest[1] -= 15.0f;
	trace = gi.traceline(inflictor_center, dest, inflictor, MASK_SOLID);
	if (trace.fraction == 1.0f)
		return true;

	dest = targ_center;
	dest[0] -= 15.0f;
	dest[1] += 15.0f;
	trace = gi.traceline(inflictor_center, dest, inflictor, MASK_SOLID);
	if (trace.fraction == 1.0f)
		return true;

	dest = targ_center;
	dest[0] -= 15.0f;
	dest[1] -= 15.0f;
	trace = gi.traceline(inflictor_center, dest, inflictor, MASK_SOLID);
	if (trace.fraction == 1.0f)
		return true;

	return false;
}

/*
============
Killed
============
*/
void Killed(edict_t *targ, edict_t *inflictor, edict_t *attacker, int damage, const vec3_t &point, mod_t mod)
{
	if (targ->health < -999)
		targ->health = -999;

	// [Paril-KEX]
	if ((targ->svflags & SVF_MONSTER) && targ->monsterinfo.aiflags & AI_MEDIC)
	{
		if (targ->enemy && targ->enemy->inuse && (targ->enemy->svflags & SVF_MONSTER)) // god, I hope so
		{
			cleanupHealTarget(targ->enemy);
		}

		// clean up self
		targ->monsterinfo.aiflags &= ~AI_MEDIC;
	}

	targ->enemy = attacker;
	targ->lastMOD = mod;

	// [Paril-KEX] monsters call die in their damage handler
	if (targ->svflags & SVF_MONSTER)
		return;

	targ->die(targ, inflictor, attacker, damage, point, mod);

	if (targ->monsterinfo.setskin)
		targ->monsterinfo.setskin(targ);
}

/*
================
SpawnDamage
================
*/
void SpawnDamage(int type, const vec3_t &origin, const vec3_t &normal, int damage)
{
	if (damage > 255)
		damage = 255;
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(type);
	//	gi.WriteByte (damage);
	gi.WritePosition(origin);
	gi.WriteDir(normal);
	gi.multicast(origin, MULTICAST_PVS, false);
}

void BloodSprayThink(edict_t *self)
{
  G_FreeEdict (self);
}

void blood_spray_touch(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	if( (other == ent->owner) || other->client )  // Don't stop on players.
		return;
	ent->think = G_FreeEdict;
	ent->nextthink = level.time + 1_sec;
}

void spray_blood(edict_t *self, vec3_t start, vec3_t dir, int damage, int mod)
{
	edict_t *blood;
	vec3_t	temp;
	int speed;

	switch (mod)
	{
	case MOD_MK23:
		speed = 1800;
		break;
	case MOD_MP5:
		speed = 1500;
		break;
	case MOD_M4:
		speed = 2400;
		break;
	case MOD_KNIFE:
		speed = 0;
		break;
	case MOD_KNIFE_THROWN:
		speed = 0;
		break;
	case MOD_DUAL:
		speed = 1800;
		break;
	case MOD_SNIPER:
		speed = 4000;
		break;
	default:
		speed = 1800;
		break;
	}

	blood = G_Spawn();
	VectorNormalize2(dir, temp);
	VectorCopy(start, blood->s.origin);
	VectorCopy(start, blood->old_origin);
	VectorCopy(temp, blood->movedir);
	vectoangles(temp, blood->s.angles);
	VectorScale(temp, speed, blood->velocity);
	blood->movetype = MOVETYPE_BLOOD;
	blood->clipmask = MASK_SHOT;
	blood->solid = SOLID_BBOX;
	blood->s.effects |= EF_GIB;
	VectorClear(blood->mins);
	VectorClear(blood->maxs);
	blood->s.modelindex = level.model_null;
	blood->owner = self;
	blood->nextthink = (level.time.milliseconds() + speed) * 1_ms;  //3.2;
	blood->touch = blood_spray_touch;
	blood->think = BloodSprayThink;
	blood->dmg = damage;
	blood->classname = "blood_spray";

	gi.linkentity(blood);
}

// zucc based on some code in Action Quake
void spray_sniper_blood(edict_t *self, vec3_t start, vec3_t dir)
{
	vec3_t forward;
	int mod = MOD_SNIPER;

	forward[0] = dir[0];
	forward[1] = dir[1];
	forward[2] = dir[2] + 0.03f;

	spray_blood( self, start, forward, 0, mod );

	forward[2] = dir[2] - 0.03f;
	spray_blood( self, start, forward, 0, mod );
	forward[2] = dir[2];

	if (dir[0] && dir[1]) {
		vec3_t diff = { 0.0f, 0.0f, 0.0f };
		if (dir[0] > 0.0f)
		{
			if (dir[1] > 0.0f) {
				diff[0] = -0.03f;
				diff[1] = 0.03f;
			}
			else {
				diff[0] = 0.03f;
				diff[1] = 0.03f;
			}
		}
		else
		{
			if (dir[1] > 0.0f) {
				diff[0] = -0.03f;
				diff[1] = -0.03f;
			}
			else {
				diff[0] = 0.03f;
				diff[1] = -0.03f;
			}
		}

		forward[0] = dir[0] + diff[0];
		forward[1] = dir[1] + diff[1];

		spray_blood( self, start, forward, 0, mod );

		forward[0] = dir[0] - diff[0];
		forward[1] = dir[1] - diff[1];

		spray_blood( self, start, forward, 0, mod );
	}

	spray_blood( self, start, dir, 0, mod );
}

void VerifyHeadShot(vec3_t point, vec3_t dir, float height, vec3_t newpoint)
{
	vec3_t normdir;

	VectorNormalize2(dir, normdir);
	VectorMA(point, height, normdir, newpoint);
}

// zucc adding location hit code
// location hit code based off ideas by pyromage and shockman
#define LEG_DAMAGE (height/2.2) - fabsf(targ->mins[2]) - 3
#define STOMACH_DAMAGE (height/1.8) - fabsf(targ->mins[2])
#define CHEST_DAMAGE (height/1.4) - fabsf(targ->mins[2])

#define HEAD_HEIGHT 12.0f
/*
============
T_Damage

targ		entity that is being damaged
inflictor	entity that is causing the damage
attacker	entity that caused the inflictor to damage targ
	example: targ=monster, inflictor=rocket, attacker=player

dir			direction of the attack
point		point at which the damage is being inflicted
normal		normal vector from that point
damage		amount of damage being inflicted
knockback	force to be applied against targ as a result of the damage

dflags		these flags are used to control how T_Damage works
	DAMAGE_RADIUS			damage was indirect (from a nearby explosion)
	DAMAGE_NO_ARMOR			armor does not protect from this damage
	DAMAGE_ENERGY			damage is from an energy based weapon
	DAMAGE_NO_KNOCKBACK		do not affect velocity, just view angles
	DAMAGE_BULLET			damage is from a bullet (used for ricochets)
	DAMAGE_NO_PROTECTION	kills godmode, armor, everything
============
*/

static int CheckArmor(edict_t *ent, const vec3_t &point, const vec3_t &normal, int damage, int te_sparks,
					  damageflags_t dflags)
{
	gclient_t *client;
	int		   save;
	item_id_t  index;
	gitem_t	*armor;
	int *power;

	if (!damage)
		return 0;

	// ROGUE
	if (dflags & (DAMAGE_NO_ARMOR | DAMAGE_NO_REG_ARMOR))
		// ROGUE
		return 0;

	client = ent->client;
	index = ArmorIndex(ent);

	if (!index)
		return 0;

	armor = GetItemByIndex(index);

	if (dflags & DAMAGE_ENERGY)
		save = (int) ceilf(armor->armor_info->energy_protection * damage);
	else
		save = (int) ceilf(armor->armor_info->normal_protection * damage);

	if (client)
		power = &client->pers.inventory[index];
	else
		power = &ent->monsterinfo.armor_power;

	if (save >= *power)
		save = *power;

	if (!save)
		return 0;

	*power -= save;

	if (!client && !ent->monsterinfo.armor_power)
		ent->monsterinfo.armor_type = IT_NULL;

	SpawnDamage(te_sparks, point, normal, save);

	return save;
}

void M_ReactToDamage(edict_t *targ, edict_t *attacker, edict_t *inflictor)
{
	// pmm
	bool new_tesla;

	if (!(attacker->client) && !(attacker->svflags & SVF_MONSTER))
		return;

	//=======
	// ROGUE
	// logic for tesla - if you are hit by a tesla, and can't see who you should be mad at (attacker)
	// attack the tesla
	// also, target the tesla if it's a "new" tesla
	if ((inflictor) && (!strcmp(inflictor->classname, "tesla_mine")))
	{
		new_tesla = MarkTeslaArea(targ, inflictor);
		if ((new_tesla || brandom()) && (!targ->enemy || !targ->enemy->classname || strcmp(targ->enemy->classname, "tesla_mine")))
			TargetTesla(targ, inflictor);
		return;
	}
	// ROGUE
	//=======

	if (attacker == targ || attacker == targ->enemy)
		return;

	// if we are a good guy monster and our attacker is a player
	// or another good guy, do not get mad at them
	if (targ->monsterinfo.aiflags & AI_GOOD_GUY)
	{
		if (attacker->client || (attacker->monsterinfo.aiflags & AI_GOOD_GUY))
			return;
	}

	// PGM
	//  if we're currently mad at something a target_anger made us mad at, ignore
	//  damage
	if (targ->enemy && targ->monsterinfo.aiflags & AI_TARGET_ANGER)
	{
		float percentHealth;

		// make sure whatever we were pissed at is still around.
		if (targ->enemy->inuse)
		{
			percentHealth = (float) (targ->health) / (float) (targ->max_health);
			if (targ->enemy->inuse && percentHealth > 0.33f)
				return;
		}

		// remove the target anger flag
		targ->monsterinfo.aiflags &= ~AI_TARGET_ANGER;
	}
	// PGM

	// we recently switched from reacting to damage, don't do it
	if (targ->monsterinfo.react_to_damage_time > level.time)
		return;

	// PMM
	// if we're healing someone, do like above and try to stay with them
	if ((targ->enemy) && (targ->monsterinfo.aiflags & AI_MEDIC))
	{
		float percentHealth;

		percentHealth = (float) (targ->health) / (float) (targ->max_health);
		// ignore it some of the time
		if (targ->enemy->inuse && percentHealth > 0.25f)
			return;

		// remove the medic flag
		cleanupHealTarget(targ->enemy);
		targ->monsterinfo.aiflags &= ~AI_MEDIC;
	}
	// PMM

	// we now know that we are not both good guys
	targ->monsterinfo.react_to_damage_time = level.time + random_time(3_sec, 5_sec);

	// if attacker is a client, get mad at them because he's good and we're not
	if (attacker->client)
	{
		targ->monsterinfo.aiflags &= ~AI_SOUND_TARGET;

		// this can only happen in coop (both new and old enemies are clients)
		// only switch if can't see the current enemy
		if (targ->enemy != attacker)
		{
			if (targ->enemy && targ->enemy->client)
			{
				if (visible(targ, targ->enemy))
				{
					targ->oldenemy = attacker;
					return;
				}
				targ->oldenemy = targ->enemy;
			}

			// [Paril-KEX]
			if ((targ->svflags & SVF_MONSTER) && targ->monsterinfo.aiflags & AI_MEDIC)
			{
				if (targ->enemy && targ->enemy->inuse && (targ->enemy->svflags & SVF_MONSTER)) // god, I hope so
				{
					cleanupHealTarget(targ->enemy);
				}

				// clean up self
				targ->monsterinfo.aiflags &= ~AI_MEDIC;
			}

			targ->enemy = attacker;
			if (!(targ->monsterinfo.aiflags & AI_DUCKED))
				FoundTarget(targ);
		}
		return;
	}

	if (attacker->enemy == targ // if they *meant* to shoot us, then shoot back
		// it's the same base (walk/swim/fly) type and both don't ignore shots,
		// get mad at them
		|| (((targ->flags & (FL_FLY | FL_SWIM)) == (attacker->flags & (FL_FLY | FL_SWIM))) &&
		(strcmp(targ->classname, attacker->classname) != 0) && !(attacker->monsterinfo.aiflags & AI_IGNORE_SHOTS) &&
		!(targ->monsterinfo.aiflags & AI_IGNORE_SHOTS)))
	{
		if (targ->enemy != attacker)
		{
			// [Paril-KEX]
			if ((targ->svflags & SVF_MONSTER) && targ->monsterinfo.aiflags & AI_MEDIC)
			{
				if (targ->enemy && targ->enemy->inuse && (targ->enemy->svflags & SVF_MONSTER)) // god, I hope so
				{
					cleanupHealTarget(targ->enemy);
				}

				// clean up self
				targ->monsterinfo.aiflags &= ~AI_MEDIC;
			}

			if (targ->enemy && targ->enemy->client)
				targ->oldenemy = targ->enemy;
			targ->enemy = attacker;
			if (!(targ->monsterinfo.aiflags & AI_DUCKED))
				FoundTarget(targ);
		}
	}
	// otherwise get mad at whoever they are mad at (help our buddy) unless it is us!
	else if (attacker->enemy && attacker->enemy != targ && targ->enemy != attacker->enemy)
	{
		if (targ->enemy != attacker->enemy)
		{
			// [Paril-KEX]
			if ((targ->svflags & SVF_MONSTER) && targ->monsterinfo.aiflags & AI_MEDIC)
			{
				if (targ->enemy && targ->enemy->inuse && (targ->enemy->svflags & SVF_MONSTER)) // god, I hope so
				{
					cleanupHealTarget(targ->enemy);
				}

				// clean up self
				targ->monsterinfo.aiflags &= ~AI_MEDIC;
			}

			if (targ->enemy && targ->enemy->client)
				targ->oldenemy = targ->enemy;
			targ->enemy = attacker->enemy;
			if (!(targ->monsterinfo.aiflags & AI_DUCKED))
				FoundTarget(targ);
		}
	}
}

// check if the two given entities are on the same team
bool OnSameTeam(edict_t *ent1, edict_t *ent2)
{
	// monsters are never on our team atm
	if (!ent1->client || !ent2->client)
		return false;
	// we're never on our own team
	else if (ent1 == ent2)
		return false;

	// [Paril-KEX] coop 'team' support
	if (coop->integer)
		return ent1->client && ent2->client;
	// ZOID
	else if (G_TeamplayEnabled() && ent1->client && ent2->client)
	{
		if (ent1->client->resp.ctf_team == ent2->client->resp.ctf_team)
			return true;
	}
	// ZOID

	return false;
}

// check if the two entities are on a team and that
// they wouldn't damage each other
bool CheckTeamDamage(edict_t *targ, edict_t *attacker)
{
	// always damage teammates if friendly fire is enabled
	if (g_friendly_fire->integer)
		return false;

	return OnSameTeam(targ, attacker);
}

void T_Damage(edict_t *targ, edict_t *inflictor, edict_t *attacker, const vec3_t &dir, const vec3_t &point,
			  const vec3_t &normal, int damage, int knockback, damageflags_t dflags, mod_t mod)
{
	gclient_t *client;
	int		   take;
	int		   save;
	int		   asave;
	int		   psave;
	int		   te_sparks;
	bool	   sphere_notified; // PGM
	int	       damage_type = 0;		// used for MOD later
	int        bleeding = 0;		// damage causes bleeding
	int        head_success = 0;
	int        instant_dam = 1;
	float      z_rel;
	int        height, friendlyFire = 0, gotArmor = 0;
	float      from_top;
	vec3_t     dist;
	float      targ_maxs2;		//FB 6/1/99

	if (!targ->takedamage)
		return;

	if (g_instagib->integer && attacker->client && targ->client)
	{
		// [Kex] always kill no matter what on instagib
		damage = 9999;
	}

	if (mod.id != MOD_TELEFRAG)
	{
		if (lights_camera_action)
			return;

		if (client)
		{
			if (client->uvTime > 0)
				return;
			if (attacker->client && attacker->client->uvTime > 0)
				return;
		}

		// AQ2:TNG - JBravo adding FF after rounds
		if (!g_friendly_fire->integer) {
			if (!teamplay->value || team_round_going || !ff_afterround->value)
				return;
		}
	}

	sphere_notified = false; // PGM

	// damage reduction for shotgun
	// if far away, reduce it to original action levels
	if (mod.id == MOD_M3)
	{
		dist = Distance(targ->s.origin, inflictor->s.origin);
		if (dist > 450.0)
			damage = damage - 2;
	}

	// friendly fire avoidance
	// if enabled you can't hurt teammates (but you can hurt yourself)
	// knockback still occurs
	if ((targ != attacker) && !(dflags & DAMAGE_NO_PROTECTION))
	{
		// mark as friendly fire
		if (OnSameTeam(targ, attacker))
		{
			mod.friendly_fire = true;

			// if we're not a nuke & friendly fire is disabled, just kill the damage
			if (!g_friendly_fire->integer && (mod.id != MOD_NUKE))
				damage = 0;
		}
	}

	// ROGUE
	//  allow the deathmatch game to change values
	if (deathmatch->integer && gamerules->integer)
	{
		if (DMGame.ChangeDamage)
			damage = DMGame.ChangeDamage(targ, attacker, damage, mod);
		if (DMGame.ChangeKnockback)
			knockback = DMGame.ChangeKnockback(targ, attacker, knockback, mod);

		if (!damage)
			return;
	}

	client = targ->client;

	if (dflags & DAMAGE_BULLET)
		te_sparks = TE_BULLET_SPARKS;
	else
		te_sparks = TE_SPARKS;

	targ_maxs2 = targ->maxs[2];
	if (targ_maxs2 == 4)
		targ_maxs2 = CROUCHING_MAXS2;	//FB 6/1/99

	height = abs (targ->mins[2]) + targ_maxs2;

	// ZOID
	// strength tech
	damage = CTFApplyStrength(attacker, damage);
	// ZOID

	if (client)
	{

		switch (mod.id) {
		case MOD_MK23:
		case MOD_DUAL:
			// damage reduction for longer range pistol shots
			dist = Distance( targ->s.origin, inflictor->s.origin );
			if (dist > 1400.0)
				damage = (int)(damage * 1 / 2);
			else if (dist > 600.0)
				damage = (int)(damage * 2 / 3);
			//Fallthrough
		case MOD_MP5:
		case MOD_M4:
		case MOD_SNIPER:
		case MOD_KNIFE:
		case MOD_KNIFE_THROWN:

			z_rel = point[2] - targ->s.origin[2];
			from_top = targ_maxs2 - z_rel;
			if (from_top < 0.0)	//FB 6/1/99
				from_top = 0.0;	//Slightly negative values were being handled wrong
			bleeding = 1;
			instant_dam = 0;

			if (from_top < 2 * HEAD_HEIGHT)
			{
				vec3_t new_point;
				VerifyHeadShot(point, dir, HEAD_HEIGHT, new_point);
				VectorSubtract(new_point, targ->s.origin, new_point);
				//gi.LocClient_Print(attacker, PRINT_HIGH, "z: %d y: %d x: %d\n", (int)(targ_maxs2 - new_point[2]),(int)(new_point[1]) , (int)(new_point[0]) );

				if ((targ_maxs2 - new_point[2]) < HEAD_HEIGHT
					&& (abs (new_point[1])) < HEAD_HEIGHT * .8
					&& (abs (new_point[0])) < HEAD_HEIGHT * .8)
				{
					head_success = 1;
				}
			}

			if (head_success)
			{
				damage_type = LOC_HDAM;
				if (mod.id != MOD_KNIFE && mod.id != MOD_KNIFE_THROWN) //Knife doesnt care about helmet
					gotArmor = INV_AMMO( targ, HELM_NUM );

				if (attacker->client)
				{
					strcpy( attacker->client->last_damaged_players, client->pers.netname );
					Stats_AddHit( attacker, mod.id, (gotArmor) ? LOC_KVLR_HELMET : LOC_HDAM );

					//AQ2:TNG END
					if (!friendlyFire && !in_warmup)
					{
						attacker->client->resp.streakHS++;
						if (attacker->client->resp.streakHS > attacker->client->resp.streakHSHighest)
							attacker->client->resp.streakHSHighest = attacker->client->resp.streakHS;

						if(attacker->client->resp.streakHS % 3 == 0)
						{
							if (use_rewards->value) {
								Announce_Reward(attacker, ACCURACY);
							}
						}
					}
				}

				if (!gotArmor)
				{
					damage = damage * 1.8 + 1;
					gi.LocClient_Print(targ, PRINT_HIGH, "Head damage\n");
					if (attacker->client)
						gi.LocClient_Print(attacker, PRINT_HIGH, "You hit %s in the head\n", client->pers.netname);

					if (mod.id != MOD_KNIFE && mod.id != MOD_KNIFE_THROWN)
						gi.sound(targ, CHAN_VOICE, level.snd_headshot, 1, ATTN_NORM, 0);
				}
				else if (mod.id == MOD_SNIPER)
				{
					if (attacker->client)
					{
						gi.LocClient_Print(attacker, PRINT_HIGH,
							"%s has a Kevlar Helmet, too bad you have AP rounds...\n",
							client->pers.netname);
						gi.LocClient_Print(targ, PRINT_HIGH,
							"Kevlar Helmet absorbed some of %s's AP sniper round\n",
							attacker->client->pers.netname);
					}
					damage = (int) (damage * 0.325);
					gi.sound(targ, CHAN_VOICE, level.snd_headshot, 1, ATTN_NORM, 0);
				}
				else
				{
					if (attacker->client)
					{
						gi.LocClient_Print( attacker, PRINT_HIGH, "%s has a Kevlar Helmet - AIM FOR THE BODY!\n",
							client->pers.netname );
						gi.LocClient_Print( targ, PRINT_HIGH, "Kevlar Helmet absorbed a part of %s's shot\n",
							attacker->client->pers.netname );
					}
					gi.sound(targ, CHAN_ITEM, level.snd_vesthit, 1, ATTN_NORM, 0);
					damage = (int)(damage / 2);
					bleeding = 0;
					instant_dam = 1;
					stopAP = 1;
					do_sparks = 1;
				}
			}
			else if (z_rel < LEG_DAMAGE)
			{
				damage_type = LOC_LDAM;
				damage = damage * .25;
				if (attacker->client)
				{
					strcpy( attacker->client->last_damaged_players, client->pers.netname );
					Stats_AddHit( attacker, mod.id, LOC_LDAM );
					gi.LocClient_Print(attacker, PRINT_HIGH, "You hit %s in the legs\n",
						client->pers.netname);
				}

				gi.LocClient_Print(targ, PRINT_HIGH, "Leg damage\n");
				ClientLegDamage(targ);
			}
			else if (z_rel < STOMACH_DAMAGE)
			{
				damage_type = LOC_SDAM;
				damage = damage * .4;
				gi.LocClient_Print(targ, PRINT_HIGH, "Stomach damage\n");
				if (attacker->client)
				{
					strcpy( attacker->client->last_damaged_players, client->pers.netname );
					Stats_AddHit(attacker, mod.id, LOC_SDAM);
					gi.LocClient_Print(attacker, PRINT_HIGH, "You hit %s in the stomach\n",
						client->pers.netname);
				}
					
				//TempFile bloody gibbing
				if (mod.id == MOD_SNIPER && sv_gib->value)
					ThrowGib(targ, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_NONE);
			}
			else		//(z_rel < CHEST_DAMAGE)
			{
				damage_type = LOC_CDAM;
				if (mod.id != MOD_KNIFE && mod.id != MOD_KNIFE_THROWN) //Knife doesnt care about kevlar
					gotArmor = INV_AMMO( targ, KEV_NUM );

				if (attacker->client) {
					strcpy( attacker->client->last_damaged_players, client->pers.netname );
					Stats_AddHit(attacker, mod.id, (gotArmor) ? LOC_KVLR_VEST : LOC_CDAM);
				}

				if (!gotArmor)
				{
					damage = damage * .65;
					gi.LocClient_Print(targ, PRINT_HIGH, "Chest damage\n");
					if (attacker->client)
						gi.LocClient_Print(attacker, PRINT_HIGH, "You hit %s in the chest\n",
							client->pers.netname);

					//TempFile bloody gibbing
					if (mod.id == MOD_SNIPER && sv_gib->value)
						ThrowGib(targ, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_NONE);
				}
				else if (mod.id == MOD_SNIPER)
				{
					if (attacker->client)
					{
						gi.LocClient_Print (attacker, PRINT_HIGH, "%s has a Kevlar Vest, too bad you have AP rounds...\n",
							client->pers.netname);
						gi.LocClient_Print (targ, PRINT_HIGH, "Kevlar Vest absorbed some of %s's AP sniper round\n",
							attacker->client->pers.netname);
					}
					damage = damage * .325;
				}
				else
				{
					if (attacker->client)
					{
						gi.LocClient_Print(attacker, PRINT_HIGH, "%s has a Kevlar Vest - AIM FOR THE HEAD!\n",
							client->pers.netname);
						gi.LocClient_Print(targ, PRINT_HIGH, "Kevlar Vest absorbed most of %s's shot\n",
							attacker->client->pers.netname);
					}
					gi.sound(targ, CHAN_ITEM, level.snd_vesthit, 1, ATTN_NORM, 0);
					damage = (int)(damage / 10);
					bleeding = 0;
					instant_dam = 1;
					stopAP = 1;
					do_sparks = 1;
				}
			}
			break;
		case MOD_M3:
		case MOD_HC:
		case MOD_HELD_GRENADE:
		case MOD_HG_SPLASH:
		case MOD_G_SPLASH:
		case MOD_BREAKINGGLASS:
			//shotgun damage report stuff
			if (client)
				client->took_damage++;

			bleeding = 1;
			instant_dam = 0;
			break;
		default:
			break;
		}
		if(friendlyFire && team_round_going)
		{
			Add_TeamWound(attacker, targ, mod);
		}
    }

	if ((targ->flags & FL_NO_KNOCKBACK) ||
		((targ->flags & FL_ALIVE_KNOCKBACK_ONLY) && (!targ->deadflag || targ->dead_time != level.time)))
		knockback = 0;

	// figure momentum add
	if (!(dflags & DAMAGE_NO_KNOCKBACK))
	{
		if ((knockback) && (targ->movetype != MOVETYPE_NONE) && (targ->movetype != MOVETYPE_BOUNCE) &&
			(targ->movetype != MOVETYPE_PUSH) && (targ->movetype != MOVETYPE_STOP))
		{
			vec3_t normalized = dir.normalized();
			vec3_t kvel;
			float  mass;

			if (targ->mass < 50)
				mass = 50;
			else
				mass = (float) targ->mass;

			if (targ->client && attacker == targ)
				kvel = normalized * (1600.0f * knockback / mass); // the rocket jump hack...
			else
				kvel = normalized * (500.0f * knockback / mass);

			targ->velocity += kvel;
		}
	}

	take = damage;
	save = 0;

	// check for godmode
	if ((targ->flags & FL_GODMODE) && !(dflags & DAMAGE_NO_PROTECTION))
	{
		take = 0;
		save = damage;
		SpawnDamage(te_sparks, point, normal, save);
	}

	// check for invincibility
	// ROGUE
	if (!(dflags & DAMAGE_NO_PROTECTION) &&
		(((client && client->invincible_time > level.time)) ||
		 ((targ->svflags & SVF_MONSTER) && targ->monsterinfo.invincible_time > level.time)))
	// ROGUE
	{
		if (targ->pain_debounce_time < level.time)
		{
			gi.sound(targ, CHAN_ITEM, gi.soundindex("items/protect4.wav"), 1, ATTN_NORM, 0);
			targ->pain_debounce_time = level.time + 2_sec;
		}
		take = 0;
		save = damage;
	}

	// ZOID
	// team armor protect
	if (G_TeamplayEnabled() && targ->client && attacker->client &&
		targ->client->resp.ctf_team == attacker->client->resp.ctf_team && targ != attacker &&
		g_teamplay_armor_protect->integer)
	{
		psave = asave = 0;
	}

	// treat cheat/powerup savings the same as armor
	asave += save;

	// ZOID
	// resistance tech
	take = CTFApplyResistance(targ, take);
	// ZOID

	// ZOID
	CTFCheckHurtCarrier(targ, attacker);
	// ZOID

	// ROGUE - this option will do damage both to the armor and person. originally for DPU rounds
	if (dflags & DAMAGE_DESTROY_ARMOR)
	{
		if (!(targ->flags & FL_GODMODE) && !(dflags & DAMAGE_NO_PROTECTION) &&
			!(client && client->invincible_time > level.time))
		{
			take = damage;
		}
	}
	// ROGUE

	// [Paril-KEX] player hit markers
	if (targ != attacker && attacker->client && targ->health > 0 && !((targ->svflags & SVF_DEADMONSTER) || (targ->flags & FL_NO_DAMAGE_EFFECTS)) && mod.id != MOD_TARGET_LASER)
		attacker->client->ps.stats[STAT_HIT_MARKER] += take + psave + asave;

	// do the damage
	if (take)
	{
		if (!(targ->flags & FL_NO_DAMAGE_EFFECTS))
		{
			// ROGUE
			if (targ->flags & FL_MECHANICAL)
				SpawnDamage(TE_ELECTRIC_SPARKS, point, normal, take);
			// ROGUE
			else if ((targ->svflags & SVF_MONSTER) || (client))
			{
				// XATRIX
				if (strcmp(targ->classname, "monster_gekk") == 0)
					SpawnDamage(TE_GREENBLOOD, point, normal, take);
				// XATRIX
				else
					SpawnDamage(TE_BLOOD, point, normal, take);
			}
			else
				SpawnDamage(te_sparks, point, normal, take);
		}

		if (!CTFMatchSetup())
			targ->health = targ->health - take;

		if ((targ->flags & FL_IMMORTAL) && targ->health <= 0)
			targ->health = 1;

		// PGM - spheres need to know who to shoot at
		if (client && client->owned_sphere)
		{
			sphere_notified = true;
			if (client->owned_sphere->pain)
				client->owned_sphere->pain(client->owned_sphere, attacker, 0, 0, mod);
		}
		// PGM

		if (targ->health <= 0)
		{
			if ((targ->svflags & SVF_MONSTER) || (client))
			{
				targ->flags |= FL_ALIVE_KNOCKBACK_ONLY;
				targ->dead_time = level.time;
			}
			targ->monsterinfo.damage_blood += take;
			targ->monsterinfo.damage_attacker = attacker;
			targ->monsterinfo.damage_inflictor = inflictor;
			targ->monsterinfo.damage_from = point;
			targ->monsterinfo.damage_mod = mod;
			targ->monsterinfo.damage_knockback += knockback;
			Killed(targ, inflictor, attacker, take, point, mod);
			return;
		}
	}

	// PGM - spheres need to know who to shoot at
	if (!sphere_notified)
	{
		if (client && client->owned_sphere)
		{
			sphere_notified = true;
			if (client->owned_sphere->pain)
				client->owned_sphere->pain(client->owned_sphere, attacker, 0, 0, mod);
		}
	}
	// PGM

	if ( targ->client ) {
		targ->client->last_attacker_time = level.time;
	}

	if (targ->svflags & SVF_MONSTER)
	{
		if (damage > 0)
		{
			M_ReactToDamage(targ, attacker, inflictor);

			targ->monsterinfo.damage_attacker = attacker;
			targ->monsterinfo.damage_inflictor = inflictor;
			targ->monsterinfo.damage_blood += take;
			targ->monsterinfo.damage_from = point;
			targ->monsterinfo.damage_mod = mod;
			targ->monsterinfo.damage_knockback += knockback;
		}

		if (targ->monsterinfo.setskin)
			targ->monsterinfo.setskin(targ);
	}
	else if (take && targ->pain)
		targ->pain(targ, attacker, (float) knockback, take, mod);

	// add to the damage inflicted on a player this frame
	// the total will be turned into screen blends and view angle kicks
	// at the end of the frame
	if (client)
	{
		client->damage_parmor += psave;
		client->damage_armor += asave;
		client->damage_blood += take;
		client->damage_knockback += knockback;
		client->damage_from = point;
		client->last_damage_time = level.time + COOP_DAMAGE_RESPAWN_TIME;

		if (!(dflags & DAMAGE_NO_INDICATOR) && inflictor != world && attacker != world && (take || psave || asave))
		{
			damage_indicator_t *indicator = nullptr;
			size_t i;

			for (i = 0; i < client->num_damage_indicators; i++)
			{
				if ((point - client->damage_indicators[i].from).length() < 32.f)
				{
					indicator = &client->damage_indicators[i];
					break;
				}
			}

			if (!indicator && i != MAX_DAMAGE_INDICATORS)
			{
				indicator = &client->damage_indicators[i];
				// for projectile direct hits, use the attacker; otherwise
				// use the inflictor (rocket splash should point to the rocket)
				indicator->from = (dflags & DAMAGE_RADIUS) ? inflictor->s.origin : attacker->s.origin;
				indicator->health = indicator->armor = indicator->power = 0;
				client->num_damage_indicators++;
			}

			if (indicator)
			{
				indicator->health += take;
				indicator->power += psave;
				indicator->armor += asave;
			}
		}
	}
}

/*
============
T_RadiusDamage
============
*/
void T_RadiusDamage(edict_t *inflictor, edict_t *attacker, float damage, edict_t *ignore, float radius, damageflags_t dflags, mod_t mod)
{
	float	 points;
	edict_t *ent = nullptr;
	vec3_t	 v;
	vec3_t	 dir;
	vec3_t   inflictor_center;
	
	if (inflictor->linked)
		inflictor_center = (inflictor->absmax + inflictor->absmin) * 0.5f;
	else
		inflictor_center = inflictor->s.origin;

	while ((ent = findradius(ent, inflictor_center, radius)) != nullptr)
	{
		if (ent == ignore)
			continue;
		if (!ent->takedamage)
			continue;

		if (ent->solid == SOLID_BSP && ent->linked)
			v = closest_point_to_box(inflictor_center, ent->absmin, ent->absmax);
		else
		{
			v = ent->mins + ent->maxs;
			v = ent->s.origin + (v * 0.5f);
		}
		v = inflictor_center - v;
		points = damage - 0.5f * v.length();
		if (ent == attacker)
			points = points * 0.5f;
		if (points > 0)
		{
			if (CanDamage(ent, inflictor))
			{
				dir = (ent->s.origin - inflictor_center).normalized();
				// [Paril-KEX] use closest point on bbox to explosion position
				// to spawn damage effect

				T_Damage(ent, inflictor, attacker, dir, closest_point_to_box(inflictor_center, ent->absmin, ent->absmax), dir, (int) points, (int) points,
						 dflags | DAMAGE_RADIUS, mod);
			}
		}
	}
}
