// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
#include "g_local.h"

/*
=================
fire_hit

Used for all impact (hit/punch/slash) attacks
=================
*/
bool fire_hit(edict_t *self, vec3_t aim, int damage, int kick)
{
	trace_t tr;
	vec3_t	forward, right, up;
	vec3_t	v;
	vec3_t	point;
	float	range;
	vec3_t	dir;

	// see if enemy is in range
	range = distance_between_boxes(self->enemy->absmin, self->enemy->absmax, self->absmin, self->absmax);
	if (range > aim[0])
		return false;

	if (!(aim[1] > self->mins[0] && aim[1] < self->maxs[0]))
	{
		// this is a side hit so adjust the "right" value out to the edge of their bbox
		if (aim[1] < 0)
			aim[1] = self->enemy->mins[0];
		else
			aim[1] = self->enemy->maxs[0];
	}

	point = closest_point_to_box(self->s.origin, self->enemy->absmin, self->enemy->absmax);

	// check that we can hit the point on the bbox
	tr = gi.traceline(self->s.origin, point, self, MASK_PROJECTILE);

	if (tr.fraction < 1)
	{
		if (!tr.ent->takedamage)
			return false;
		// if it will hit any client/monster then hit the one we wanted to hit
		if ((tr.ent->svflags & SVF_MONSTER) || (tr.ent->client))
			tr.ent = self->enemy;
	}

	// check that we can hit the player from the point
	tr = gi.traceline(point, self->enemy->s.origin, self, MASK_PROJECTILE);

	if (tr.fraction < 1)
	{
		if (!tr.ent->takedamage)
			return false;
		// if it will hit any client/monster then hit the one we wanted to hit
		if ((tr.ent->svflags & SVF_MONSTER) || (tr.ent->client))
			tr.ent = self->enemy;
	}

	AngleVectors(self->s.angles, forward, right, up);
	point = self->s.origin + (forward * range);
	point += (right * aim[1]);
	point += (up * aim[2]);
	dir = point - self->enemy->s.origin;

	// do the damage
	T_Damage(tr.ent, self, self, dir, point, vec3_origin, damage, kick / 2, DAMAGE_NO_KNOCKBACK, MOD_HIT);

	if (!(tr.ent->svflags & SVF_MONSTER) && (!tr.ent->client))
		return false;

	// do our special form of knockback here
	v = (self->enemy->absmin + self->enemy->absmax) * 0.5f;
	v -= point;
	v.normalize();
	self->enemy->velocity += v * kick;
	if (self->enemy->velocity[2] > 0)
		self->enemy->groundentity = nullptr;
	return true;
}

// helper routine for piercing traces;
// mask = the input mask for finding what to hit
// you can adjust the mask for the re-trace (for water, etc).
// note that you must take care in your pierce callback to mark
// the entities that are being pierced.
void pierce_trace(const vec3_t &start, const vec3_t &end, edict_t *ignore, pierce_args_t &pierce, contents_t mask)
{
	int	   loop_count = MAX_EDICTS;
	vec3_t own_start, own_end;
	own_start = start;
	own_end = end;

	while (--loop_count)
	{
		pierce.tr = gi.traceline(start, own_end, ignore, mask);

		// didn't hit anything, so we're done
		if (!pierce.tr.ent || pierce.tr.fraction == 1.0f)
			return;

		// hit callback said we're done
		if (!pierce.hit(mask, own_end))
			return;

		own_start = pierce.tr.endpos;
	}

	gi.Com_Print("runaway pierce_trace\n");
}

struct fire_lead_pierce_t : pierce_args_t
{
	edict_t		*self;
	vec3_t		 start;
	vec3_t		 aimdir;
	int			 damage;
	int			 kick;
	int			 hspread;
	int			 vspread;
	mod_t		 mod;
	int			 te_impact;
	contents_t   mask;
	bool	     water = false;
	vec3_t	     water_start = {};
	edict_t	    *chain = nullptr;

	inline fire_lead_pierce_t(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, mod_t mod, int te_impact, contents_t mask) :
		pierce_args_t(),
		self(self),
		start(start),
		aimdir(aimdir),
		damage(damage),
		kick(kick),
		hspread(hspread),
		vspread(vspread),
		mod(mod),
		te_impact(te_impact),
		mask(mask)
	{
	}

	// we hit an entity; return false to stop the piercing.
	// you can adjust the mask for the re-trace (for water, etc).
	bool hit(contents_t &mask, vec3_t &end) override
	{
		// see if we hit water
		if (tr.contents & MASK_WATER)
		{
			int color;

			water = true;
			water_start = tr.endpos;

			// CHECK: is this compare ever true?
			if (te_impact != -1 && start != tr.endpos)
			{
				if (tr.contents & CONTENTS_WATER)
				{
					// FIXME: this effectively does nothing..
					if (strcmp(tr.surface->name, "brwater") == 0)
						color = SPLASH_BROWN_WATER;
					else
						color = SPLASH_BLUE_WATER;
				}
				else if (tr.contents & CONTENTS_SLIME)
					color = SPLASH_SLIME;
				else if (tr.contents & CONTENTS_LAVA)
					color = SPLASH_LAVA;
				else
					color = SPLASH_UNKNOWN;

				if (color != SPLASH_UNKNOWN)
				{
					gi.WriteByte(svc_temp_entity);
					gi.WriteByte(TE_SPLASH);
					gi.WriteByte(8);
					gi.WritePosition(tr.endpos);
					gi.WriteDir(tr.plane.normal);
					gi.WriteByte(color);
					gi.multicast(tr.endpos, MULTICAST_PVS, false);
				}

				// change bullet's course when it enters water
				vec3_t dir, forward, right, up;
				dir = end - start;
				dir = vectoangles(dir);
				AngleVectors(dir, forward, right, up);
				float r = crandom() * hspread * 2;
				float u = crandom() * vspread * 2;
				end = water_start + (forward * 8192);
				end += (right * r);
				end += (up * u);
			}

			// re-trace ignoring water this time
			mask &= ~MASK_WATER;
			return true;
		}

		// did we hit an hurtable entity?
		if (tr.ent->takedamage)
		{
			T_Damage(tr.ent, self, self, aimdir, tr.endpos, tr.plane.normal, damage, kick, mod.id == MOD_TESLA ? DAMAGE_ENERGY : DAMAGE_BULLET, mod);

			// only deadmonster is pierceable, or actual dead monsters
			// that haven't been made non-solid yet
			if ((tr.ent->svflags & SVF_DEADMONSTER) ||
				(tr.ent->health <= 0 && (tr.ent->svflags & SVF_MONSTER)))
			{
				if (!mark(tr.ent))
					return false;

				return true;
			}
		}
		else
		{
			// send gun puff / flash
			// don't mark the sky
			if (te_impact != -1 && !(tr.surface && ((tr.surface->flags & SURF_SKY) || strncmp(tr.surface->name, "sky", 3) == 0)))
			{
				gi.WriteByte(svc_temp_entity);
				gi.WriteByte(te_impact);
				gi.WritePosition(tr.endpos);
				gi.WriteDir(tr.plane.normal);
				gi.multicast(tr.endpos, MULTICAST_PVS, false);

				if (self->client)
					PlayerNoise(self, tr.endpos, PNOISE_IMPACT);
			}
		}

		// hit a solid, so we're stopping here

		return false;
	}
};

/*
=================
fire_lead

This is an internal support routine used for bullet/pellet based weapons.
=================
*/
static void fire_lead(edict_t *self, const vec3_t &start, const vec3_t &aimdir, int damage, int kick, int te_impact, int hspread, int vspread, mod_t mod)
{
	fire_lead_pierce_t args = {
		self,
		start,
		aimdir,
		damage,
		kick,
		hspread,
		vspread,
		mod,
		te_impact,
		MASK_PROJECTILE | MASK_WATER
	};

	// [Paril-KEX]
	if (self->client && !G_ShouldPlayersCollide(true))
		args.mask &= ~CONTENTS_PLAYER;

	// special case: we started in water.
	if (gi.pointcontents(start) & MASK_WATER)
	{
		args.water = true;
		args.water_start = start;
		args.mask &= ~MASK_WATER;
	}

	// check initial firing position
	pierce_trace(self->s.origin, start, self, args, args.mask);

	// we're clear, so do the second pierce
	if (args.tr.fraction == 1.f)
	{
		args.restore();

		vec3_t end, dir, forward, right, up;
		dir = vectoangles(aimdir);
		AngleVectors(dir, forward, right, up);

		float r = crandom() * hspread;
		float u = crandom() * vspread;
		end = start + (forward * 8192);
		end += (right * r);
		end += (up * u);

		pierce_trace(args.tr.endpos, end, self, args, args.mask);
	}

	// if went through water, determine where the end is and make a bubble trail
	if (args.water && te_impact != -1)
	{
		vec3_t pos, dir;

		dir = args.tr.endpos - args.water_start;
		dir.normalize();
		pos = args.tr.endpos + (dir * -2);
		if (gi.pointcontents(pos) & MASK_WATER)
			args.tr.endpos = pos;
		else
			args.tr = gi.traceline(pos, args.water_start, args.tr.ent != world ? args.tr.ent : nullptr, MASK_WATER);

		pos = args.water_start + args.tr.endpos;
		pos *= 0.5f;

		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_BUBBLETRAIL);
		gi.WritePosition(args.water_start);
		gi.WritePosition(args.tr.endpos);
		gi.multicast(pos, MULTICAST_PVS, false);
	}
}

/*
=================
fire_bullet

Fires a single round.  Used for machinegun and chaingun.  Would be fine for
pistols, rifles, etc....
=================
*/
void fire_bullet(edict_t *self, const vec3_t &start, const vec3_t &aimdir, int damage, int kick, int hspread, int vspread, mod_t mod)
{
	fire_lead(self, start, aimdir, damage, kick, TE_GUNSHOT, hspread, vspread, mod);
}

// zucc fire_load_ap for rounds that pass through soft targets and keep going
static void fire_lead_ap(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int te_impact, int hspread, int vspread, int mod)
{
	trace_t tr;
	vec3_t dir, forward, right, up, end;
	float r, u;
	vec3_t water_start;
	bool water = false;
	int content_mask = MASK_SHOT | MASK_WATER;
	vec3_t from;
	edict_t *ignore;


	InitTookDamage();

	// setup
	stopAP = 0;
	vectoangles(aimdir, dir);
	AngleVectors(dir, forward, right, up);

	r = crandom() * hspread;
	u = crandom() * vspread;
	VectorMA(start, 8192, forward, end);
	VectorMA(end, r, right, end);
	VectorMA(end, u, up, end);
	VectorCopy(start, from);
	if (gi.pointcontents(start) & MASK_WATER)
	{
		water = true;
		VectorCopy (start, water_start);
		content_mask &= ~MASK_WATER;
	}

	ignore = self;

	//      PRETRACE();
	//      tr = gi.trace (self->s.origin, NULL, NULL, start, self, MASK_SHOT);
	//      POSTTRACE();
	while (ignore)
	//if (!(tr.fraction < 1.0))
	{

		PRETRACE();
		//tr = gi.trace (from, NULL, NULL, end, ignore, mask);
		tr = gi.trace(from, NULL, NULL, end, ignore, content_mask);
		POSTTRACE();

		// glass fx
		// catch case of firing thru one or breakable glasses
		while (tr.fraction < 1.0 && (tr.surface->flags & (SURF_TRANS33|SURF_TRANS66))
			&& (tr.ent) && (0 == Q_stricmp (tr.ent->classname, "func_explosive")))
		{
			// break glass  
			CGF_SFX_ShootBreakableGlass(tr.ent, self, &tr, mod);
			// continue trace from current endpos to start
			PRETRACE();
			tr = gi.trace(tr.endpos, NULL, NULL, end, tr.ent, content_mask);
			POSTTRACE();
		}
		// ---

		// see if we hit water
		if (tr.contents & MASK_WATER)
		{
			int color;

			water = true;
			VectorCopy(tr.endpos, water_start);

			if (!VectorCompare(from, tr.endpos))
			{
				if (tr.contents & CONTENTS_WATER)
				{
					if (strcmp(tr.surface->name, "*brwater") == 0)
						color = SPLASH_BROWN_WATER;
					else
						color = SPLASH_BLUE_WATER;
				}
				else if (tr.contents & CONTENTS_SLIME)
					color = SPLASH_SLIME;
				else if (tr.contents & CONTENTS_LAVA)
					color = SPLASH_LAVA;
				else
					color = SPLASH_UNKNOWN;

				if (color != SPLASH_UNKNOWN)
				{
					gi.WriteByte(svc_temp_entity);
					gi.WriteByte(TE_SPLASH);
					gi.WriteByte(8);
					gi.WritePosition(tr.endpos);
					gi.WriteDir(tr.plane.normal);
					gi.WriteByte(color);
					gi.multicast(tr.endpos, MULTICAST_PVS);
				}

				// change bullet's course when it enters water
				VectorSubtract(end, from, dir);
				vectoangles(dir, dir);
				AngleVectors(dir, forward, right, up);
				r = crandom() * hspread * 2;
				u = crandom() * vspread * 2;
				VectorMA(water_start, 8192, forward, end);
				VectorMA(end, r, right, end);
				VectorMA(end, u, up, end);
			}

			// re-trace ignoring water this time
			PRETRACE();
			tr = gi.trace(water_start, NULL, NULL, end, ignore, MASK_SHOT);
			POSTTRACE();
		}

		// send gun puff / flash

		ignore = NULL;

		if (tr.surface && (tr.surface->flags & SURF_SKY))
			continue;

		if (tr.fraction < 1.0)
		{

			if ((tr.ent->svflags & SVF_MONSTER) || tr.ent->client)
			{
				ignore = tr.ent;
				VectorCopy(tr.endpos, from);
				//FIREBLADE
				// Advance the "from" point a few units
				// towards "end" here
				if (tr.ent->client)
				{
					if (tr.ent->client->took_damage)
					{
						vec3_t out;
						VectorSubtract(end, from, out);
						VectorNormalize(out);
						VectorScale(out, 8, out);
						VectorAdd(out, from, from);
						continue;
					}
					tr.ent->client->took_damage++;
				}
				//FIREBLADE
			}

			if (tr.ent != self && tr.ent->takedamage)
			{
				T_Damage(tr.ent, self, self, aimdir, tr.endpos, tr.plane.normal, damage, kick, 0, mod);
				if (stopAP)	// the AP round hit something that would stop it (kevlar)
					ignore = NULL;
			}
			else if (tr.ent != self && !water)
			{
				if (strncmp(tr.surface->name, "sky", 3) != 0)
				{
					AddDecal(self, &tr);
					gi.WriteByte(svc_temp_entity);
					gi.WriteByte(te_impact);
					gi.WritePosition (tr.endpos);
					gi.WriteDir(tr.plane.normal);
					gi.multicast(tr.endpos, MULTICAST_PVS);

					if (self->client)
						PlayerNoise(self, tr.endpos, PNOISE_IMPACT);
				}
			}
		}
	}

	// if went through water, determine where the end and make a bubble trail
	if (water)
	{
		vec3_t pos;

		VectorSubtract(tr.endpos, water_start, dir);
		VectorNormalize(dir);
		VectorMA(tr.endpos, -2, dir, pos);
		if (gi.pointcontents(pos) & MASK_WATER) {
			VectorCopy(pos, tr.endpos);
		} else {
			PRETRACE();
			tr = gi.trace(pos, NULL, NULL, water_start, tr.ent, MASK_WATER);
			POSTTRACE();
		}

		VectorAdd(water_start, tr.endpos, pos);
		VectorScale(pos, 0.5, pos);

		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_BUBBLETRAIL);
		gi.WritePosition(water_start);
		gi.WritePosition(tr.endpos);
		gi.multicast(pos, MULTICAST_PVS);
	}

}

// zucc - for the M4
void fire_bullet_sparks (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int mod)
{
	setFFState(self);
	antilag_rewind_all(self);
	fire_lead_ap(self, start, aimdir, damage, kick, TE_BULLET_SPARKS, hspread, vspread, mod);
	antilag_unmove_all();
}

// zucc - for sniper
void fire_bullet_sniper (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int mod)
{
	setFFState (self);
	antilag_rewind_all(self);
	fire_lead_ap (self, start, aimdir, damage, kick, TE_GUNSHOT, hspread, vspread, mod);
	antilag_unmove_all();
}


/*
 *  ProduceShotgunDamageReport 
 */
void ProduceShotgunDamageReport (edict_t *self)
{
	int i, total = 0, total_to_print, printed = 0;
	char *textbuf;
	gclient_t *cl;

	for (i = 0, cl = game.clients; i < game.maxclients; i++, cl++) {
		if (cl->took_damage)
			total++;
	}

	if (!total)
		return;

	if (total > 10)
		total_to_print = 10;
	else
		total_to_print = total;

	//256 is enough, its limited to 10 nicks
	textbuf = self->client->last_damaged_players;
	*textbuf = '\0';

	for (i = 0, cl = game.clients; i < game.maxclients; i++, cl++)
	{
		if (!cl->took_damage)
			continue;

		if (printed == total_to_print - 1)
		{
			if (total_to_print == 2)
				strcat(textbuf, " and ");
			else if (total_to_print != 1)
				strcat(textbuf, ", and ");
		}
		else if (printed) {
			strcat(textbuf, ", ");
		}
		strcat(textbuf, cl->pers.netname);
		printed++;

		if (printed == total_to_print)
			break;
	}
	gi.cprintf(self, PRINT_HIGH, "You hit %s in the body\n", textbuf);

	// TNG Stats
	if (self->client->curr_weap == M3_NUM)
		Stats_AddHit(self, MOD_M3, LOC_NO);
	else if (self->client->curr_weap == HC_NUM)
		Stats_AddHit(self, MOD_HC, LOC_NO);
}


/*
=================
fire_shotgun

Shoots shotgun pellets.  Used by shotgun and super shotgun.
=================
*/
void fire_shotgun(edict_t *self, const vec3_t &start, const vec3_t &aimdir, int damage, int kick, int hspread, int vspread, int count, mod_t mod)
{
	for (int i = 0; i < count; i++)
		fire_lead(self, start, aimdir, damage, kick, TE_SHOTGUN, hspread, vspread, mod);
}

/*
=================
fire_blaster

Fires a single blaster bolt.  Used by the blaster and hyper blaster.
=================
*/
TOUCH(blaster_touch) (edict_t *self, edict_t *other, const trace_t &tr, bool other_touching_self) -> void
{
	if (other == self->owner)
		return;

	if (tr.surface && (tr.surface->flags & SURF_SKY))
	{
		G_FreeEdict(self);
		return;
	}

	// PMM - crash prevention
	if (self->owner && self->owner->client)
		PlayerNoise(self->owner, self->s.origin, PNOISE_IMPACT);

	if (other->takedamage)
		T_Damage(other, self, self->owner, self->velocity, self->s.origin, tr.plane.normal, self->dmg, 1, DAMAGE_ENERGY, static_cast<mod_id_t>(self->style));
	else
	{
		gi.WriteByte(svc_temp_entity);
		gi.WriteByte( ( self->style != MOD_BLUEBLASTER ) ? TE_BLASTER : TE_BLUEHYPERBLASTER );
		gi.WritePosition(self->s.origin);
		gi.WriteDir(tr.plane.normal);
		gi.multicast(self->s.origin, MULTICAST_PHS, false);
	}

	G_FreeEdict(self);
}

void fire_blaster(edict_t *self, const vec3_t &start, const vec3_t &dir, int damage, int speed, effects_t effect, mod_t mod)
{
	edict_t *bolt;
	trace_t	 tr;

	bolt = G_Spawn();
	bolt->svflags = SVF_PROJECTILE;
	bolt->s.origin = start;
	bolt->s.old_origin = start;
	bolt->s.angles = vectoangles(dir);
	bolt->velocity = dir * speed;
	bolt->movetype = MOVETYPE_FLYMISSILE;
	bolt->clipmask = MASK_PROJECTILE;
	// [Paril-KEX]
	if (self->client && !G_ShouldPlayersCollide(true))
		bolt->clipmask &= ~CONTENTS_PLAYER;
	bolt->flags |= FL_DODGE;
	bolt->solid = SOLID_BBOX;
	bolt->s.effects |= effect;
	bolt->s.modelindex = gi.modelindex("models/objects/laser/tris.md2");
	bolt->s.sound = gi.soundindex("misc/lasfly.wav");
	bolt->owner = self;
	bolt->touch = blaster_touch;
	bolt->nextthink = level.time + 2_sec;
	bolt->think = G_FreeEdict;
	bolt->dmg = damage;
	bolt->classname = "bolt";
	bolt->style = mod.id;
	gi.linkentity(bolt);

	tr = gi.traceline(self->s.origin, bolt->s.origin, bolt, bolt->clipmask);
	if (tr.fraction < 1.0f)
	{
		bolt->s.origin = tr.endpos + (tr.plane.normal * 1.f);
		bolt->touch(bolt, tr.ent, tr, false);
	}
}

constexpr spawnflags_t SPAWNFLAG_GRENADE_HAND = 1_spawnflag;
constexpr spawnflags_t SPAWNFLAG_GRENADE_HELD = 2_spawnflag;

/*
=================
fire_grenade
=================
*/
THINK(Grenade_Explode) (edict_t *ent) -> void
{
	vec3_t origin;
	mod_t  mod;

	if (ent->owner->client)
		PlayerNoise(ent->owner, ent->s.origin, PNOISE_IMPACT);

	// FIXME: if we are onground then raise our Z just a bit since we are a point?
	if (ent->enemy)
	{
		float  points;
		vec3_t v;
		vec3_t dir;

		v = ent->enemy->mins + ent->enemy->maxs;
		v = ent->enemy->s.origin + (v * 0.5f);
		v = ent->s.origin - v;
		points = ent->dmg - 0.5f * v.length();
		dir = ent->enemy->s.origin - ent->s.origin;
		if (ent->spawnflags.has(SPAWNFLAG_GRENADE_HAND))
			mod = MOD_HANDGRENADE;
		else
			mod = MOD_GRENADE;
		T_Damage(ent->enemy, ent, ent->owner, dir, ent->s.origin, vec3_origin, (int) points, (int) points, DAMAGE_RADIUS, mod);
	}

	if (ent->spawnflags.has(SPAWNFLAG_GRENADE_HELD))
		mod = MOD_HELD_GRENADE;
	else if (ent->spawnflags.has(SPAWNFLAG_GRENADE_HAND))
		mod = MOD_HG_SPLASH;
	else
		mod = MOD_G_SPLASH;
	T_RadiusDamage(ent, ent->owner, (float) ent->dmg, ent->enemy, ent->dmg_radius, DAMAGE_NONE, mod);

	origin = ent->s.origin + (ent->velocity * -0.02f);
	gi.WriteByte(svc_temp_entity);
	if (ent->waterlevel)
	{
		if (ent->groundentity)
			gi.WriteByte(TE_GRENADE_EXPLOSION_WATER);
		else
			gi.WriteByte(TE_ROCKET_EXPLOSION_WATER);
	}
	else
	{
		if (ent->groundentity)
			gi.WriteByte(TE_GRENADE_EXPLOSION);
		else
			gi.WriteByte(TE_ROCKET_EXPLOSION);
	}
	gi.WritePosition(origin);
	gi.multicast(ent->s.origin, MULTICAST_PHS, false);

	G_FreeEdict(ent);
}

TOUCH(Grenade_Touch) (edict_t *ent, edict_t *other, const trace_t &tr, bool other_touching_self) -> void
{
	if (other == ent->owner)
		return;

	if (tr.surface && (tr.surface->flags & SURF_SKY))
	{
		G_FreeEdict(ent);
		return;
	}

	if (!other->takedamage)
	{
		if (ent->spawnflags.has(SPAWNFLAG_GRENADE_HAND))
		{
			if (frandom() > 0.5f)
				gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/hgrenb1a.wav"), 1, ATTN_NORM, 0);
			else
				gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/hgrenb2a.wav"), 1, ATTN_NORM, 0);
		}
		else
		{
			gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/grenlb1b.wav"), 1, ATTN_NORM, 0);
		}
		return;
	}

	ent->enemy = other;
	Grenade_Explode(ent);
}

THINK(Grenade4_Think) (edict_t *self) -> void
{
	if (level.time >= self->timestamp)
	{
		Grenade_Explode(self);
		return;
	}
	
	if (self->velocity)
	{
		float p = self->s.angles.x;
		float z = self->s.angles.z;
		float speed_frac = clamp(self->velocity.lengthSquared() / (self->speed * self->speed), 0.f, 1.f);
		self->s.angles = vectoangles(self->velocity);
		self->s.angles.x = LerpAngle(p, self->s.angles.x, speed_frac);
		self->s.angles.z = z + (gi.frame_time_s * 360 * speed_frac);
	}

	self->nextthink = level.time + FRAME_TIME_S;
}

void fire_grenade(edict_t *self, const vec3_t &start, const vec3_t &aimdir, int damage, int speed, gtime_t timer, float damage_radius, float right_adjust, float up_adjust, bool monster)
{
	edict_t *grenade;
	vec3_t	 dir;
	vec3_t	 forward, right, up;

	dir = vectoangles(aimdir);
	AngleVectors(dir, forward, right, up);

	grenade = G_Spawn();
	grenade->s.origin = start;
	grenade->velocity = aimdir * speed;

	if (up_adjust)
	{
		float gravityAdjustment = level.gravity / 800.f;
		grenade->velocity += up * up_adjust * gravityAdjustment;
	}

	if (right_adjust)
		grenade->velocity += right * right_adjust;

	grenade->movetype = MOVETYPE_BOUNCE;
	grenade->clipmask = MASK_PROJECTILE;
	// [Paril-KEX]
	if (self->client && !G_ShouldPlayersCollide(true))
		grenade->clipmask &= ~CONTENTS_PLAYER;
	grenade->solid = SOLID_BBOX;
	grenade->svflags |= SVF_PROJECTILE;
	grenade->flags |= ( FL_DODGE | FL_TRAP );
	grenade->s.effects |= EF_GRENADE;
	grenade->speed = speed;
	if (monster)
	{
		grenade->avelocity = { crandom() * 360, crandom() * 360, crandom() * 360 };
		grenade->s.modelindex = gi.modelindex("models/objects/grenade/tris.md2");
		grenade->nextthink = level.time + timer;
		grenade->think = Grenade_Explode;
		grenade->s.effects |= EF_GRENADE_LIGHT;
	}
	else
	{
		grenade->s.modelindex = gi.modelindex("models/objects/grenade4/tris.md2");
		grenade->s.angles = vectoangles(grenade->velocity);
		grenade->nextthink = level.time + FRAME_TIME_S;
		grenade->timestamp = level.time + timer;
		grenade->think = Grenade4_Think;
		grenade->s.renderfx |= RF_MINLIGHT;
	}
	grenade->owner = self;
	grenade->touch = Grenade_Touch;
	grenade->dmg = damage;
	grenade->dmg_radius = damage_radius;
	grenade->classname = "grenade";

	gi.linkentity(grenade);
}

void fire_grenade2(edict_t *self, const vec3_t &start, const vec3_t &aimdir, int damage, int speed, gtime_t timer, float damage_radius, bool held)
{
	edict_t *grenade;
	vec3_t	 dir;
	vec3_t	 forward, right, up;

	dir = vectoangles(aimdir);
	AngleVectors(dir, forward, right, up);

	grenade = G_Spawn();
	grenade->s.origin = start;
	grenade->velocity = aimdir * speed;

	float gravityAdjustment = level.gravity / 800.f;

	grenade->velocity += up * (200 + crandom() * 10.0f) * gravityAdjustment;
	grenade->velocity += right * (crandom() * 10.0f);

	grenade->avelocity = { crandom() * 360, crandom() * 360, crandom() * 360 };
	grenade->movetype = MOVETYPE_BOUNCE;
	grenade->clipmask = MASK_PROJECTILE;
	// [Paril-KEX]
	if (self->client && !G_ShouldPlayersCollide(true))
		grenade->clipmask &= ~CONTENTS_PLAYER;
	grenade->solid = SOLID_BBOX;
	grenade->svflags |= SVF_PROJECTILE;
	grenade->flags |= ( FL_DODGE | FL_TRAP );
	grenade->s.effects |= EF_GRENADE;

	grenade->s.modelindex = gi.modelindex("models/objects/grenade3/tris.md2");
	grenade->owner = self;
	grenade->touch = Grenade_Touch;
	grenade->nextthink = level.time + timer;
	grenade->think = Grenade_Explode;
	grenade->dmg = damage;
	grenade->dmg_radius = damage_radius;
	grenade->classname = "hand_grenade";
	grenade->spawnflags = SPAWNFLAG_GRENADE_HAND;
	if (held)
		grenade->spawnflags |= SPAWNFLAG_GRENADE_HELD;
	grenade->s.sound = gi.soundindex("weapons/hgrenc1b.wav");

	if (timer <= 0_ms)
		Grenade_Explode(grenade);
	else
	{
		gi.sound(self, CHAN_WEAPON, gi.soundindex("weapons/hgrent1a.wav"), 1, ATTN_NORM, 0);
		gi.linkentity(grenade);
	}
}

/*
=================
fire_rocket
=================
*/
TOUCH(rocket_touch) (edict_t *ent, edict_t *other, const trace_t &tr, bool other_touching_self) -> void
{
	vec3_t origin;

	if (other == ent->owner)
		return;

	if (tr.surface && (tr.surface->flags & SURF_SKY))
	{
		G_FreeEdict(ent);
		return;
	}

	if (ent->owner->client)
		PlayerNoise(ent->owner, ent->s.origin, PNOISE_IMPACT);

	// calculate position for the explosion entity
	origin = ent->s.origin + tr.plane.normal;

	if (other->takedamage)
	{
		T_Damage(other, ent, ent->owner, ent->velocity, ent->s.origin, tr.plane.normal, ent->dmg, 0, DAMAGE_NONE, MOD_ROCKET);
	}
	else
	{
		// don't throw any debris in net games
		if (!deathmatch->integer && !coop->integer)
		{
			if (tr.surface && !(tr.surface->flags & (SURF_WARP | SURF_TRANS33 | SURF_TRANS66 | SURF_FLOWING)))
			{
				ThrowGibs(ent, 2, {
					{ (size_t) irandom(5), "models/objects/debris2/tris.md2", GIB_METALLIC | GIB_DEBRIS }
				});
			}
		}
	}

	T_RadiusDamage(ent, ent->owner, (float) ent->radius_dmg, other, ent->dmg_radius, DAMAGE_NONE, MOD_R_SPLASH);

	gi.WriteByte(svc_temp_entity);
	if (ent->waterlevel)
		gi.WriteByte(TE_ROCKET_EXPLOSION_WATER);
	else
		gi.WriteByte(TE_ROCKET_EXPLOSION);
	gi.WritePosition(origin);
	gi.multicast(ent->s.origin, MULTICAST_PHS, false);

	G_FreeEdict(ent);
}

edict_t *fire_rocket(edict_t *self, const vec3_t &start, const vec3_t &dir, int damage, int speed, float damage_radius, int radius_damage)
{
	edict_t *rocket;

	rocket = G_Spawn();
	rocket->s.origin = start;
	rocket->s.angles = vectoangles(dir);
	rocket->velocity = dir * speed;
	rocket->movetype = MOVETYPE_FLYMISSILE;
	rocket->svflags |= SVF_PROJECTILE;
	rocket->flags |= FL_DODGE;
	rocket->clipmask = MASK_PROJECTILE;
	// [Paril-KEX]
	if (self->client && !G_ShouldPlayersCollide(true))
		rocket->clipmask &= ~CONTENTS_PLAYER;
	rocket->solid = SOLID_BBOX;
	rocket->s.effects |= EF_ROCKET;
	rocket->s.modelindex = gi.modelindex("models/objects/rocket/tris.md2");
	rocket->owner = self;
	rocket->touch = rocket_touch;
	rocket->nextthink = level.time + gtime_t::from_sec(8000.f / speed);
	rocket->think = G_FreeEdict;
	rocket->dmg = damage;
	rocket->radius_dmg = radius_damage;
	rocket->dmg_radius = damage_radius;
	rocket->s.sound = gi.soundindex("weapons/rockfly.wav");
	rocket->classname = "rocket";

	gi.linkentity(rocket);

	return rocket;
}

void kick_attack (edict_t *ent)
{
	vec3_t start;
	vec3_t forward, right;
	vec3_t offset;
	int damage = 20, kick = 400, friendlyFire = 0;
	trace_t tr;
	vec3_t end;
	char *genderstr;


	AngleVectors(ent->client->v_angle, forward, right, NULL);

	VectorScale(forward, 0, ent->client->kick_origin);

	VectorSet(offset, 0, 0, ent->viewheight - 20);
	P_ProjectSource (ent->client, ent->s.origin, offset, forward, right, start);

	VectorMA(start, 25, forward, end);

	PRETRACE();
	tr = gi.trace(ent->s.origin, NULL, NULL, end, ent, MASK_SHOT);
	POSTTRACE();

	// don't need to check for water
	if (tr.surface && (tr.surface->flags & SURF_SKY))
		return;

	if (tr.fraction >= 1.0)
		return;

	if (tr.ent->takedamage || KickDoor(&tr, ent, forward))
	{
		ent->client->jumping = 0;	// only 1 jumpkick per jump

		if (tr.ent->health <= 0)
			return;

		// PaTMaN's jmod kickable toggle
		if (tr.ent->client && jump->value)
			if (!(tr.ent->client->resp.toggles & TG_KICKABLE))
				return;

		if (tr.ent->client)
		{
			if (tr.ent->client->uvTime)
				return;
			
			if (tr.ent != ent && ent->client && OnSameTeam( tr.ent, ent ))
				friendlyFire = 1;

			if (friendlyFire/* && DMFLAGS(DF_NO_FRIENDLY_FIRE)*/){
				if (!teamplay->value || team_round_going || !ff_afterround->value)
					return;
			}
		}
		// zucc stop powerful upwards kicking
		//forward[2] = 0;
		// glass fx
		if (Q_stricmp(tr.ent->classname, "func_explosive") == 0)
			CGF_SFX_ShootBreakableGlass(tr.ent, ent, &tr, MOD_KICK);
		else
			T_Damage(tr.ent, ent, ent, forward, tr.endpos, tr.plane.normal, damage, kick, 0, MOD_KICK);

		// Stat add
		Stats_AddHit(ent, MOD_KICK, LOC_NO);
		// Stat end
		gi.sound(ent, CHAN_WEAPON, level.snd_kick, 1, ATTN_NORM, 0);
		PlayerNoise (ent, ent->s.origin, PNOISE_SELF);
		if (tr.ent->client && (tr.ent->client->curr_weap == M4_NUM
			|| tr.ent->client->curr_weap == MP5_NUM
			|| tr.ent->client->curr_weap == M3_NUM
			|| tr.ent->client->curr_weap ==	SNIPER_NUM
			|| tr.ent->client->curr_weap == HC_NUM))		// crandom() > .8 ) 
		{
			// zucc fix this so reloading won't happen on the new gun!
			tr.ent->client->reload_attempts = 0;
			DropSpecialWeapon(tr.ent);

			genderstr = GENDER_STR( tr.ent, "his", "her", "its" );

			gi.cprintf(ent, PRINT_HIGH, "You kick %s's %s from %s hands!\n",
				tr.ent->client->pers.netname,(tr.ent->client->weapon)->pickup_name, genderstr);

			gi.cprintf(tr.ent, PRINT_HIGH,	"%s kicked your weapon from your hands!\n", ent->client->pers.netname);

		} else if(tr.ent->client && tr.ent->client->ctf_grapple && tr.ent->client->ctf_grapplestate == CTF_GRAPPLE_STATE_FLY) {
			// hifi: if the player is shooting a grapple, lose it's focus
			CTFPlayerResetGrapple(tr.ent);
		}
	}
}


void punch_attack(edict_t * ent)
{
	vec3_t start, forward, right, offset, end;
	int damage = 7, kick = 100, friendlyFire = 0;
	int randmodify;
	trace_t tr;
	char *genderstr;

	AngleVectors(ent->client->v_angle, forward, right, NULL);
	VectorScale(forward, 0, ent->client->kick_origin);
	VectorSet(offset, 0, 0, ent->viewheight - 20);
	P_ProjectSource(ent->client, ent->s.origin, offset, forward, right, start);
	VectorMA(start, 50, forward, end);
	PRETRACE();
	tr = gi.trace(ent->s.origin, NULL, NULL, end, ent, MASK_SHOT);
	POSTTRACE();

	if (!((tr.surface) && (tr.surface->flags & SURF_SKY)))
	{
		if (tr.fraction < 1.0 && tr.ent->takedamage)
		{
			if (tr.ent->health <= 0)
				return;

			if (tr.ent->client)
			{
				if (tr.ent->client->uvTime)
					return;

				if (tr.ent != ent && ent->client && OnSameTeam(tr.ent, ent))
					friendlyFire = 1;

				if (friendlyFire && DMFLAGS(DF_NO_FRIENDLY_FIRE)){
					if (!teamplay->value || team_round_going || !ff_afterround->value)
						return;
				}
			}

			// add some random damage, damage range from 8 to 20.
			randmodify = rand() % 13 + 1;
			damage += randmodify;
			// modify kick by damage
			kick += (randmodify * 10);

			// reduce damage, if he tries to punch within or out of water
			if (ent->waterlevel)
				damage -= rand() % 10 + 1;
			// reduce kick, if victim is in water
			if (tr.ent->waterlevel)
				kick /= 2;

			T_Damage(tr.ent, ent, ent, forward, tr.endpos, tr.plane.normal,
				damage, kick, 0, MOD_PUNCH);
			// Stat add
			Stats_AddHit(ent, MOD_PUNCH, LOC_NO);
			// Stat end
			gi.sound(ent, CHAN_WEAPON, level.snd_kick, 1, ATTN_NORM, 0);
			PlayerNoise(ent, ent->s.origin, PNOISE_SELF);

			//only hit weapon out of hand if damage >= 15
			if (tr.ent->client && (tr.ent->client->curr_weap == M4_NUM
				|| tr.ent->client->curr_weap == MP5_NUM
				|| tr.ent->client->curr_weap == M3_NUM
				|| tr.ent->client->curr_weap == SNIPER_NUM
				|| tr.ent->client->curr_weap == HC_NUM) && damage >= 15)
			{
				DropSpecialWeapon(tr.ent);

				genderstr = GENDER_STR(tr.ent, "his", "her", "its");
				gi.cprintf(ent, PRINT_HIGH, "You hit %s's %s from %s hands!\n",
					tr.ent->client->pers.netname, (tr.ent->client->weapon)->pickup_name, genderstr);

				gi.cprintf(tr.ent, PRINT_HIGH, "%s hit your weapon from your hands!\n",
					ent->client->pers.netname);
			}
			return;
		}
	}
	gi.sound(ent, CHAN_WEAPON, gi.soundindex("weapons/swish.wav"), 1, ATTN_NORM, 0);

	// animate the punch
	// can't animate a punch when ducked
	if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
		return;
	if (ent->client->anim_priority >= ANIM_WAVE)
		return;

	SetAnimation( ent, FRAME_flip01 - 1, FRAME_flip03, ANIM_WAVE );
}

// zucc
// return values
// 0 - missed
// 1 - hit player
// 2 - hit wall

int knife_attack (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick)
{
	trace_t tr;
	vec3_t end;

	VectorMA (start, 45, aimdir, end);

	PRETRACE();
	tr = gi.trace(self->s.origin, NULL, NULL, end, self, MASK_SHOT);
	POSTTRACE();

	// don't need to check for water
	if (tr.surface && (tr.surface->flags & SURF_SKY))
		return 0;	// we hit the sky, call it a miss

	if (tr.fraction < 1.0)
	{
		//glass fx
		if (0 == Q_stricmp(tr.ent->classname, "func_explosive"))
		{
			CGF_SFX_ShootBreakableGlass(tr.ent, self, &tr, MOD_KNIFE);
		}
		else if (tr.ent->takedamage)
		{
			setFFState(self);
			T_Damage(tr.ent, self, self, aimdir, tr.endpos, tr.plane.normal, damage, kick, 0, MOD_KNIFE);
			return -2;
		}
		else
		{
			gi.WriteByte(svc_temp_entity);
			gi.WriteByte(TE_SPARKS);
			gi.WritePosition (tr.endpos);
			gi.WriteDir(tr.plane.normal);
			gi.multicast(tr.endpos, MULTICAST_PVS);
			return -1;
		}
	}
	return 0;
}

static int knives = 0;

void knife_touch(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	vec3_t origin;
	edict_t *dropped, *knife;
	vec3_t move_angles;
	gitem_t *item;


	if (other == ent->owner)
		return;

	if (surf && (surf->flags & SURF_SKY)) {
		G_FreeEdict (ent);
		return;
	}

	if (ent->owner->client)
	{
		gi.positioned_sound(ent->s.origin, ent, CHAN_WEAPON, gi.soundindex("weapons/clank.wav"), 1, ATTN_NORM, 0);
		PlayerNoise(ent->owner, ent->s.origin, PNOISE_IMPACT);
	}

	// calculate position for the explosion entity
	VectorMA(ent->s.origin, -0.02, ent->velocity, origin);

	//glass fx
	if (0 == Q_stricmp (other->classname, "func_explosive"))
		return; // ignore it, so it can bounce


	if (other->takedamage)
	{
		// Players hit by throwing knives add it to their inventory.
		if( other->client && (INV_AMMO(other,KNIFE_NUM) < other->client->knife_max) )
			INV_AMMO(other,KNIFE_NUM) ++;

		T_Damage(other, ent, ent->owner, ent->velocity, ent->s.origin, plane->normal, ent->dmg, 0, 0, MOD_KNIFE_THROWN);
	}
	else
	{
		// code to manage excess knives in the game, guarantees that
		// no more than knifelimit knives will be stuck in walls.  
		// if knifelimit == 0 then it won't be in effect and it can
		// start removing knives even when less than the limit are
		// out there.
		if (knifelimit->value != 0)
		{
			knives++;

			if (knives > knifelimit->value)
				knives = 1;

			knife = FindEdictByClassnum("weapon_Knife", knives);
			if (knife)
				knife->nextthink = level.framenum + FRAMEDIV;
		}

		dropped = G_Spawn();
		item = GET_ITEM(KNIFE_NUM);

		dropped->classname = item->classname;
		dropped->typeNum = item->typeNum;
		dropped->item = item;
		dropped->spawnflags = DROPPED_ITEM;
		dropped->s.effects = item->world_model_flags;
		dropped->s.renderfx = RF_GLOW;
		VectorSet(dropped->mins, -15, -15, -15);
		VectorSet(dropped->maxs, 15, 15, 15);
		gi.setmodel(dropped, dropped->item->world_model);
		dropped->solid = SOLID_TRIGGER;
		dropped->movetype = MOVETYPE_TOSS;
		dropped->touch = Touch_Item;
		dropped->owner = ent;
		dropped->gravity = 0;
		dropped->classnum = knives;

		vectoangles(ent->velocity, move_angles);
		//AngleVectors (ent->s.angles, forward, right, up);
		VectorCopy(ent->s.origin, dropped->s.origin);
		VectorCopy(dropped->s.origin, dropped->old_origin);
		VectorCopy(move_angles, dropped->s.angles);

		dropped->nextthink = level.framenum + 120 * HZ;
		dropped->think = G_FreeEdict;

		// Stick to moving doors, platforms, etc.
		if( CanBeAttachedTo(other) )
			AttachToEntity( dropped, other );

		gi.linkentity(dropped);

		if (!(ent->waterlevel))
		{
			gi.WriteByte(svc_temp_entity);
			gi.WriteByte(TE_SPARKS);
			gi.WritePosition(origin);
			gi.WriteDir(plane->normal);
			gi.multicast(ent->s.origin, MULTICAST_PVS);
		}
	}
	G_FreeEdict(ent);
}


void knife_throw(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed)
{
	edict_t *knife;
	trace_t tr;

	knife = G_Spawn();

	VectorNormalize(dir);
	VectorCopy(start, knife->s.origin);
	vectoangles(dir, knife->s.angles);
	VectorScale(dir, speed, knife->velocity);
	knife->movetype = MOVETYPE_TOSS;

	VectorSet(knife->avelocity, 1200, 0, 0);

	knife->movetype = MOVETYPE_TOSS;
	knife->clipmask = MASK_SHOT;
	knife->solid = SOLID_BBOX;
	knife->s.effects = 0;		//EF_ROTATE?
	VectorClear(knife->mins);
	VectorClear(knife->maxs);
	knife->s.modelindex = gi.modelindex ("models/objects/knife/tris.md2");
	knife->owner = self;
	knife->touch = knife_touch;
	knife->nextthink = level.framenum + 8000 * HZ / speed;
	knife->think = G_FreeEdict;
	knife->dmg = damage;
	knife->s.sound = level.snd_knifethrow;
	knife->classname = "thrown_knife";
	knife->typeNum = KNIFE_NUM;

	PRETRACE();
	tr = gi.trace(self->s.origin, NULL, NULL, knife->s.origin, knife, MASK_SHOT);
	POSTTRACE();
	if (tr.fraction < 1.0)
	{
		VectorMA(knife->s.origin, -10, dir, knife->s.origin);
		knife->touch(knife, tr.ent, NULL, NULL);
	}

	if (knife->inuse) {
		VectorCopy(knife->s.origin, knife->s.old_origin);
		VectorCopy(knife->s.origin, knife->old_origin);
		gi.linkentity(knife);
	}
}



using search_callback_t = decltype(game_import_t::inPVS);

bool binary_positional_search_r(const vec3_t &viewer, const vec3_t &start, const vec3_t &end, search_callback_t cb, int32_t split_num)
{
	// check half-way point
	vec3_t mid = (start + end) * 0.5f;

	if (cb(viewer, mid, true))
		return true;

	// no more splits
	if (!split_num)
		return false;

	// recursively check both sides
	return binary_positional_search_r(viewer, start, mid, cb, split_num - 1) || binary_positional_search_r(viewer, mid, end, cb, split_num - 1);
}

// [Paril-KEX] simple binary search through a line to see if any points along
// the line (in a binary split) pass the callback
bool binary_positional_search(const vec3_t &viewer, const vec3_t &start, const vec3_t &end, search_callback_t cb, int32_t num_splits)
{
	// check start/end first
	if (cb(viewer, start, true) || cb(viewer, end, true))
		return true;

	// recursive split
	return binary_positional_search_r(viewer, start, end, cb, num_splits);
}

struct fire_rail_pierce_t : pierce_args_t
{
	edict_t *self;
	vec3_t	 aimdir;
	int		 damage;
	int		 kick;
	bool	 water = false;

	inline fire_rail_pierce_t(edict_t *self, vec3_t aimdir, int damage, int kick) :
		pierce_args_t(),
		self(self),
		aimdir(aimdir),
		damage(damage),
		kick(kick)
	{
	}

	// we hit an entity; return false to stop the piercing.
	// you can adjust the mask for the re-trace (for water, etc).
	bool hit(contents_t &mask, vec3_t &end) override
	{
		if (tr.contents & (CONTENTS_SLIME | CONTENTS_LAVA))
		{
			mask &= ~(CONTENTS_SLIME | CONTENTS_LAVA);
			water = true;
			return true;
		}
		else
		{
			// try to kill it first
			if ((tr.ent != self) && (tr.ent->takedamage))
				T_Damage(tr.ent, self, self, aimdir, tr.endpos, tr.plane.normal, damage, kick, DAMAGE_NONE, MOD_RAILGUN);

			// dead, so we don't need to care about checking pierce
			if (!tr.ent->inuse || (!tr.ent->solid || tr.ent->solid == SOLID_TRIGGER))
				return true;

			// ZOID--added so rail goes through SOLID_BBOX entities (gibs, etc)
			if ((tr.ent->svflags & SVF_MONSTER) || (tr.ent->client) ||
				// ROGUE
				(tr.ent->flags & FL_DAMAGEABLE) ||
				// ROGUE
				(tr.ent->solid == SOLID_BBOX))
			{
				if (!mark(tr.ent))
					return false;

				return true;
			}
		}

		return false;
	}
};

// [Paril-KEX] get the current unique unicast key
uint32_t GetUnicastKey()
{
	static uint32_t key = 1;

	if (!key)
		return key = 1;

	return key++;
}

/*
=================
fire_rail
=================
*/
void fire_rail(edict_t *self, const vec3_t &start, const vec3_t &aimdir, int damage, int kick)
{
	fire_rail_pierce_t args = {
		self,
		aimdir,
		damage,
		kick
	};

	contents_t mask = MASK_PROJECTILE | CONTENTS_SLIME | CONTENTS_LAVA;
	
	// [Paril-KEX]
	if (self->client && !G_ShouldPlayersCollide(true))
		mask &= ~CONTENTS_PLAYER;

	vec3_t end = start + (aimdir * 8192);

	pierce_trace(start, end, self, args, mask);

	uint32_t unicast_key = GetUnicastKey();

	// send gun puff / flash
	// [Paril-KEX] this often makes double noise, so trying
	// a slightly different approach...
	for (auto player : active_players())
	{
		vec3_t org = player->s.origin + player->client->ps.viewoffset + vec3_t{ 0, 0, (float) player->client->ps.pmove.viewheight };

		if (binary_positional_search(org, start, args.tr.endpos, gi.inPHS, 3))
		{
			gi.WriteByte(svc_temp_entity);
			gi.WriteByte(g_instagib->integer ? TE_RAILTRAIL2 : TE_RAILTRAIL);
			gi.WritePosition(start);
			gi.WritePosition(args.tr.endpos);
			gi.unicast(player, false, unicast_key);
		}
	}

	if (self->client)
		PlayerNoise(self, args.tr.endpos, PNOISE_IMPACT);
}

static vec3_t bfg_laser_pos(vec3_t p, float dist)
{
	float theta = frandom(2 * PIf);
	float phi = acos(crandom());

	vec3_t d {
		sin(phi) * cos(theta),
		sin(phi) * sin(theta),
		cos(phi)
	};

	return p + (d * dist);
}

THINK(bfg_laser_update) (edict_t *self) -> void
{
	if (level.time > self->timestamp || !self->owner->inuse)
	{
		G_FreeEdict(self);
		return;
	}

	self->s.origin = self->owner->s.origin;
	self->nextthink = level.time + 1_ms;
	gi.linkentity(self);
}

static void bfg_spawn_laser(edict_t *self)
{
	vec3_t end = bfg_laser_pos(self->s.origin, 256);
	trace_t tr = gi.traceline(self->s.origin, end, self, MASK_OPAQUE);

	if (tr.fraction == 1.0f)
		return;

	edict_t *laser = G_Spawn();
	laser->s.frame = 3;
	laser->s.renderfx = RF_BEAM_LIGHTNING;
	laser->movetype = MOVETYPE_NONE;
	laser->solid = SOLID_NOT;
	laser->s.modelindex = MODELINDEX_WORLD; // must be non-zero
	laser->s.origin = self->s.origin;
	laser->s.old_origin = tr.endpos;
	laser->s.skinnum = 0xD0D0D0D0;
	laser->think = bfg_laser_update;
	laser->nextthink = level.time + 1_ms;
	laser->timestamp = level.time + 300_ms;
	laser->owner = self;
	gi.linkentity(laser);
}

/*
=================
fire_bfg
=================
*/
THINK(bfg_explode) (edict_t *self) -> void
{
	edict_t *ent;
	float	 points;
	vec3_t	 v;
	float	 dist;

	bfg_spawn_laser(self);

	if (self->s.frame == 0)
	{
		// the BFG effect
		ent = nullptr;
		while ((ent = findradius(ent, self->s.origin, self->dmg_radius)) != nullptr)
		{
			if (!ent->takedamage)
				continue;
			if (ent == self->owner)
				continue;
			if (!CanDamage(ent, self))
				continue;
			if (!CanDamage(ent, self->owner))
				continue;
			// ROGUE - make tesla hurt by bfg
			if (!(ent->svflags & SVF_MONSTER) && !(ent->flags & FL_DAMAGEABLE) && (!ent->client) && (strcmp(ent->classname, "misc_explobox") != 0))
				continue;
			// ZOID
			// don't target players in CTF
			if (CheckTeamDamage(ent, self->owner))
				continue;
			// ZOID

			v = ent->mins + ent->maxs;
			v = ent->s.origin + (v * 0.5f);
			vec3_t centroid = v;
			v = self->s.origin - centroid;
			dist = v.length();
			points = self->radius_dmg * (1.0f - sqrtf(dist / self->dmg_radius));

			T_Damage(ent, self, self->owner, self->velocity, centroid, vec3_origin, (int) points, 0, DAMAGE_ENERGY, MOD_BFG_EFFECT);

			// Paril: draw BFG lightning laser to enemies
			gi.WriteByte(svc_temp_entity);
			gi.WriteByte(TE_BFG_ZAP);
			gi.WritePosition(self->s.origin);
			gi.WritePosition(centroid);
			gi.multicast(self->s.origin, MULTICAST_PHS, false);
		}
	}

	self->nextthink = level.time + 10_hz;
	self->s.frame++;
	if (self->s.frame == 5)
		self->think = G_FreeEdict;
}

TOUCH(bfg_touch) (edict_t *self, edict_t *other, const trace_t &tr, bool other_touching_self) -> void
{
	if (other == self->owner)
		return;

	if (tr.surface && (tr.surface->flags & SURF_SKY))
	{
		G_FreeEdict(self);
		return;
	}

	if (self->owner->client)
		PlayerNoise(self->owner, self->s.origin, PNOISE_IMPACT);

	// core explosion - prevents firing it into the wall/floor
	if (other->takedamage)
		T_Damage(other, self, self->owner, self->velocity, self->s.origin, tr.plane.normal, 200, 0, DAMAGE_ENERGY, MOD_BFG_BLAST);
	T_RadiusDamage(self, self->owner, 200, other, 100, DAMAGE_ENERGY, MOD_BFG_BLAST);

	gi.sound(self, CHAN_VOICE, gi.soundindex("weapons/bfg__x1b.wav"), 1, ATTN_NORM, 0);
	self->solid = SOLID_NOT;
	self->touch = nullptr;
	self->s.origin += self->velocity * (-1 * gi.frame_time_s);
	self->velocity = {};
	self->s.modelindex = gi.modelindex("sprites/s_bfg3.sp2");
	self->s.frame = 0;
	self->s.sound = 0;
	self->s.effects &= ~EF_ANIM_ALLFAST;
	self->think = bfg_explode;
	self->nextthink = level.time + 10_hz;
	self->enemy = other;

	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_BFG_BIGEXPLOSION);
	gi.WritePosition(self->s.origin);
	gi.multicast(self->s.origin, MULTICAST_PHS, false);
}


struct bfg_laser_pierce_t : pierce_args_t
{
	edict_t *self;
	vec3_t	 dir;
	int		 damage;

	inline bfg_laser_pierce_t(edict_t *self, vec3_t dir, int damage) :
		pierce_args_t(),
		self(self),
		dir(dir),
		damage(damage)
	{
	}

	// we hit an entity; return false to stop the piercing.
	// you can adjust the mask for the re-trace (for water, etc).
	bool hit(contents_t &mask, vec3_t &end) override
	{
		// hurt it if we can
		if ((tr.ent->takedamage) && !(tr.ent->flags & FL_IMMUNE_LASER) && (tr.ent != self->owner))
			T_Damage(tr.ent, self, self->owner, dir, tr.endpos, vec3_origin, damage, 1, DAMAGE_ENERGY, MOD_BFG_LASER);

		// if we hit something that's not a monster or player we're done
		if (!(tr.ent->svflags & SVF_MONSTER) && !(tr.ent->flags & FL_DAMAGEABLE) && (!tr.ent->client))
		{
			gi.WriteByte(svc_temp_entity);
			gi.WriteByte(TE_LASER_SPARKS);
			gi.WriteByte(4);
			gi.WritePosition(tr.endpos);
			gi.WriteDir(tr.plane.normal);
			gi.WriteByte(self->s.skinnum);
			gi.multicast(tr.endpos, MULTICAST_PVS, false);
			return false;
		}

		if (!mark(tr.ent))
			return false;
		
		return true;
	}
};

THINK(bfg_think) (edict_t *self) -> void
{
	edict_t *ent;
	vec3_t	 point;
	vec3_t	 dir;
	vec3_t	 start;
	vec3_t	 end;
	int		 dmg;
	trace_t	 tr;

	if (deathmatch->integer)
		dmg = 5;
	else
		dmg = 10;

	bfg_spawn_laser(self);

	ent = nullptr;
	while ((ent = findradius(ent, self->s.origin, 256)) != nullptr)
	{
		if (ent == self)
			continue;

		if (ent == self->owner)
			continue;

		if (!ent->takedamage)
			continue;

		// ROGUE - make tesla hurt by bfg
		if (!(ent->svflags & SVF_MONSTER) && !(ent->flags & FL_DAMAGEABLE) && (!ent->client) && (strcmp(ent->classname, "misc_explobox") != 0))
			continue;
		// ZOID
		// don't target players in CTF
		if (CheckTeamDamage(ent, self->owner))
			continue;
		// ZOID

		point = (ent->absmin + ent->absmax) * 0.5f;

		dir = point - self->s.origin;
		dir.normalize();

		start = self->s.origin;
		end = start + (dir * 2048);

		// [Paril-KEX] don't fire a laser if we're blocked by the world
		tr = gi.traceline(start, point, nullptr, MASK_SOLID);

		if (tr.fraction < 1.0f)
			continue;

		bfg_laser_pierce_t args {
			self,
			dir,
			dmg
		};
		
		pierce_trace(start, end, self, args, CONTENTS_SOLID | CONTENTS_MONSTER | CONTENTS_PLAYER | CONTENTS_DEADMONSTER);

		gi.WriteByte(svc_temp_entity);
		gi.WriteByte(TE_BFG_LASER);
		gi.WritePosition(self->s.origin);
		gi.WritePosition(tr.endpos);
		gi.multicast(self->s.origin, MULTICAST_PHS, false);
	}

	self->nextthink = level.time + 10_hz;
}

void fire_bfg(edict_t *self, const vec3_t &start, const vec3_t &dir, int damage, int speed, float damage_radius)
{
	edict_t *bfg;

	bfg = G_Spawn();
	bfg->s.origin = start;
	bfg->s.angles = vectoangles(dir);
	bfg->velocity = dir * speed;
	bfg->movetype = MOVETYPE_FLYMISSILE;
	bfg->clipmask = MASK_PROJECTILE;
	bfg->svflags = SVF_PROJECTILE;
	// [Paril-KEX]
	if (self->client && !G_ShouldPlayersCollide(true))
		bfg->clipmask &= ~CONTENTS_PLAYER;
	bfg->solid = SOLID_BBOX;
	bfg->s.effects |= EF_BFG | EF_ANIM_ALLFAST;
	bfg->s.modelindex = gi.modelindex("sprites/s_bfg1.sp2");
	bfg->owner = self;
	bfg->touch = bfg_touch;
	bfg->nextthink = level.time + gtime_t::from_sec(8000.f / speed);
	bfg->think = G_FreeEdict;
	bfg->radius_dmg = damage;
	bfg->dmg_radius = damage_radius;
	bfg->classname = "bfg blast";
	bfg->s.sound = gi.soundindex("weapons/bfg__l1a.wav");

	bfg->think = bfg_think;
	bfg->nextthink = level.time + FRAME_TIME_S;
	bfg->teammaster = bfg;
	bfg->teamchain = nullptr;

	gi.linkentity(bfg);
}

TOUCH(disintegrator_touch) (edict_t *self, edict_t *other, const trace_t &tr, bool other_touching_self) -> void
{
	gi.WriteByte(svc_temp_entity);
	gi.WriteByte(TE_WIDOWSPLASH);
	gi.WritePosition(self->s.origin - (self->velocity * 0.01f));
	gi.multicast(self->s.origin, MULTICAST_PHS, false);

	G_FreeEdict(self);

	if (other->svflags & (SVF_MONSTER | SVF_PLAYER))
	{
		other->disintegrator_time += 50_sec;
		other->disintegrator = self->owner;
	}
}

void fire_disintegrator(edict_t *self, const vec3_t &start, const vec3_t &forward, int speed)
{
	edict_t *bfg;

	bfg = G_Spawn();
	bfg->s.origin = start;
	bfg->s.angles = vectoangles(forward);
	bfg->velocity = forward * speed;
	bfg->movetype = MOVETYPE_FLYMISSILE;
	bfg->clipmask = MASK_PROJECTILE;
	// [Paril-KEX]
	if (self->client && !G_ShouldPlayersCollide(true))
		bfg->clipmask &= ~CONTENTS_PLAYER;
	bfg->solid = SOLID_BBOX;
	bfg->s.effects |= EF_TAGTRAIL | EF_ANIM_ALL;
	bfg->s.renderfx |= RF_TRANSLUCENT;
	bfg->svflags |= SVF_PROJECTILE;
	bfg->flags |= FL_DODGE;
	bfg->s.modelindex = gi.modelindex("sprites/s_bfg1.sp2");
	bfg->owner = self;
	bfg->touch = disintegrator_touch;
	bfg->nextthink = level.time + gtime_t::from_sec(8000.f / speed);
	bfg->think = G_FreeEdict;
	bfg->classname = "disint ball";
	bfg->s.sound = gi.soundindex("weapons/bfg__l1a.wav");

	gi.linkentity(bfg);
}