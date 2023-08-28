#include "g_local.h"

// time too wait between failures to respawn?
#define SPEC_RESPAWN_TIME       60
// time before they will get respawned
#define SPEC_TECH_TIMEOUT       60

void SpecThink(edict_t * spec);

static edict_t *FindSpecSpawn(void)
{
	edict_t *spot = NULL;
	int i = rand() % 16;

	while (i--)
		spot = G_FindByString<&edict_t::classname>(spot, "info_player_deathmatch");
	if (!spot)
		spot = G_FindByString<&edict_t::classname>(spot, "info_player_deathmatch");

	return spot;
}

static void SpawnSpec(gitem_t * item, edict_t * spot)
{
	edict_t *ent;
	vec3_t forward, right, angles;

	ent = G_Spawn();
	ent->classname = item->classname;
	ent->typeNum = item->typeNum;
	ent->item = item;
	ent->spawnflags = SPAWNFLAG_ITEM_DROPPED;
	ent->s.effects = item->world_model_flags;
	ent->s.renderfx = RF_GLOW;
	VectorSet(ent->mins, -15, -15, -15);
	VectorSet(ent->maxs, 15, 15, 15);
	// zucc dumb hack to make laser look like it is on the ground
	if (item->typeNum == LASER_NUM) {
		VectorSet(ent->mins, -15, -15, -1);
		VectorSet(ent->maxs, 15, 15, 1);
	}
	gi.setmodel(ent, ent->item->world_model);
	ent->solid = SOLID_TRIGGER;
	ent->movetype = MOVETYPE_TOSS;
	ent->touch = Touch_Item;
	ent->owner = ent;

	angles[0] = 0;
	angles[1] = rand() % 360;
	angles[2] = 0;

	AngleVectors(angles, forward, right, NULL);
	VectorCopy(spot->s.origin, ent->s.origin);
	ent->s.origin[2] += 16;
	VectorCopy(ent->s.origin, ent->old_origin);
	VectorScale(forward, 100, ent->velocity);
	ent->velocity[2] = 300;

	ent->nextthink = level.time + 60_sec;
	ent->think = SpecThink;

	gi.linkentity(ent);
}

void SpawnSpecs(edict_t * ent)
{
	gitem_t *spec;
	edict_t *spot;
	int i, itemNum;

	G_FreeEdict(ent);

	if(item_respawnmode->value)
		return;

	for(i = 0; i < ITEM_MAX; i++)
	{
		itemNum = NO_ITEM_NUM + i;
		// TODO: Item and weapon bans
		// if (!ITF_ALLOWED(itemNum))
		// 	continue;

		if ((spec = GetItemByIndex(itemNum)) != NULL && (spot = FindSpecSpawn()) != NULL) {
			SpawnSpec(spec, spot);
		}
	}
}

void SpecThink(edict_t * spec)
{
	edict_t *spot;

	spot = FindSpecSpawn();
	if (spot) {
		SpawnSpec(spec->item, spot);
	}

	G_FreeEdict(spec);
}

static void MakeTouchSpecThink(edict_t * ent)
{

	ent->touch = Touch_Item;

	if (allitem->value) {
		ent->nextthink = level.time + 40_hz;
		ent->think = G_FreeEdict;
		return;
	}

	if (gameSettings & GS_ROUNDBASED) {
		ent->nextthink = level.time + 60_hz; //FIXME: should this be roundtime left
		ent->think = G_FreeEdict;
		return;
	}

	if (gameSettings & GS_WEAPONCHOOSE) {
		ent->nextthink = level.time + 6_hz;
		ent->think = G_FreeEdict;
		return;
	}

	if(item_respawnmode->value) {
		float respawn_time = (item_respawn->value*0.5f);
		ent->nextthink = level.time + gtime_t::from_sec(respawn_time);
		ent->think = G_FreeEdict;
	}
	else {
		ent->nextthink = level.time + gtime_t::from_sec(item_respawn->value);
		ent->think = SpecThink;
	}
}

void Drop_Spec(edict_t * ent, gitem_t * item)
{
	edict_t *spec;

	spec = Drop_Item(ent, item);
	//gi.cprintf(ent, PRINT_HIGH, "Dropping special item.\n");
	spec->nextthink = level.framenum + 1 * HZ;
	spec->think = MakeTouchSpecThink;
	//zucc this and the one below should probably be -- not = 0, if
	// a server turns on multiple item pickup.
	ent->client->inventory[ITEM_INDEX(item)]--;
}

void DeadDropSpec(edict_t * ent)
{
	gitem_t *spec;
	edict_t *dropped;
	int i, itemNum;

	for(i = 0; i < ITEM_MAX; i++)
	{
		itemNum = NO_ITEM_NUM + i;
		if (INV_AMMO(ent, itemNum) > 0) {
			spec = GetItemByIndex(itemNum);
			dropped = Drop_Item(ent, spec);
			// hack the velocity to make it bounce random
			dropped->velocity[0] = (rand() % 600) - 300;
			dropped->velocity[1] = (rand() % 600) - 300;
			dropped->nextthink = level.time + 40_hz;
			dropped->think = MakeTouchSpecThink;
			dropped->owner = NULL;
			dropped->spawnflags = SPAWNFLAG_ITEM_DROPPED_PLAYER;
			ent->client->inventory[ITEM_INDEX(spec)] = 0;
		}
	}
}

// frees the passed edict!
void RespawnSpec(edict_t * ent)
{
	edict_t *spot;

	if ((spot = FindSpecSpawn()) != NULL)
		SpawnSpec(ent->item, spot);
	G_FreeEdict(ent);
}

void SetupSpecSpawn(void)
{
	edict_t *ent;

	if (level.specspawn)
		return;

	ent = G_Spawn();
	ent->nextthink = level.time + 160_hz;
	ent->think = SpawnSpecs;
	level.specspawn = true;
}
