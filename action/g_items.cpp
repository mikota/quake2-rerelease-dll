// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
#include "g_local.h"
#include "bots/bot_includes.h"

bool Pickup_Weapon(edict_t *ent, edict_t *other);
void Use_Weapon(edict_t *ent, gitem_t *inv);
void Drop_Weapon(edict_t *ent, gitem_t *inv);

void Weapon_Blaster(edict_t *ent);
void Weapon_Shotgun(edict_t *ent);
void Weapon_SuperShotgun(edict_t *ent);
void Weapon_Machinegun(edict_t *ent);
void Weapon_Chaingun(edict_t *ent);
void Weapon_HyperBlaster(edict_t *ent);
void Weapon_RocketLauncher(edict_t *ent);
void Weapon_Grenade(edict_t *ent);
void Weapon_GrenadeLauncher(edict_t *ent);
void Weapon_Railgun(edict_t *ent);
void Weapon_BFG(edict_t *ent);
// RAFAEL
void Weapon_Ionripper(edict_t *ent);
void Weapon_Phalanx(edict_t *ent);
void Weapon_Trap(edict_t *ent);
// RAFAEL
// ROGUE
void Weapon_ChainFist(edict_t *ent);
void Weapon_Disintegrator(edict_t *ent);
void Weapon_ETF_Rifle(edict_t *ent);
void Weapon_Heatbeam(edict_t *ent);
void Weapon_Prox(edict_t *ent);
void Weapon_Tesla(edict_t *ent);
void Weapon_ProxLauncher(edict_t *ent);
// ROGUE
void Weapon_Beta_Disintegrator(edict_t *ent);

void	   Use_Quad(edict_t *ent, gitem_t *item);
static gtime_t quad_drop_timeout_hack;

// RAFAEL
void	   Use_QuadFire(edict_t *ent, gitem_t *item);
static gtime_t quad_fire_drop_timeout_hack;
// RAFAEL

//======================================================================

/*
===============
GetItemByIndex
===============
*/
gitem_t *GetItemByIndex(item_id_t index)
{
	if (index <= IT_NULL || index >= IT_TOTAL)
		return nullptr;

	return &itemlist[index];
}

static gitem_t *ammolist[AMMO_MAX];

gitem_t *GetItemByAmmo(ammo_t ammo)
{
	return ammolist[ammo];
}

static gitem_t *poweruplist[POWERUP_MAX];

gitem_t *GetItemByPowerup(powerup_t powerup)
{
	return poweruplist[powerup];
}

/*
===============
FindItemByClassname

===============
*/
gitem_t *FindItemByClassname(const char *classname)
{
	int		 i;
	gitem_t *it;

	it = itemlist;
	for (i = 0; i < IT_TOTAL; i++, it++)
	{
		if (!it->classname)
			continue;
		if (!Q_strcasecmp(it->classname, classname))
			return it;
	}

	return nullptr;
}

/*
===============
FindItem

===============
*/
gitem_t *FindItem(const char *pickup_name)
{
	int		 i;
	gitem_t *it;

	it = itemlist;
	for (i = 0; i < IT_TOTAL; i++, it++)
	{
		if (!it->use_name)
			continue;
		if (!Q_strcasecmp(it->use_name, pickup_name))
			return it;
	}

	return nullptr;
}

//======================================================================

THINK(DoRespawn) (edict_t *ent) -> void
{
	if (ent->team)
	{
		edict_t *master;
		int		 count;
		int		 choice;

		master = ent->teammaster;

		// ZOID
		// in ctf, when we are weapons stay, only the master of a team of weapons
		// is spawned
		if (ctf->integer && g_dm_weapons_stay->integer && master->item && (master->item->flags & IF_WEAPON))
			ent = master;
		else
		{
			// ZOID

			for (count = 0, ent = master; ent; ent = ent->chain, count++)
				;

			choice = irandom(count);

			for (count = 0, ent = master; count < choice; ent = ent->chain, count++)
				;
		}
	}

	ent->svflags &= ~SVF_NOCLIENT;
	ent->svflags &= ~SVF_RESPAWNING;
	ent->solid = SOLID_TRIGGER;
	gi.linkentity(ent);

	// send an effect
	ent->s.event = EV_ITEM_RESPAWN;

	// ROGUE
	if (g_dm_random_items->integer)
	{
		item_id_t new_item = DoRandomRespawn(ent);

		// if we've changed entities, then do some sleight of hand.
		// otherwise, the old entity will respawn
		if (new_item)
		{
			ent->item = GetItemByIndex(new_item);

			ent->classname = ent->item->classname;
			ent->s.effects = ent->item->world_model_flags;
			gi.setmodel(ent, ent->item->world_model);
		}
	}
	// ROGUE
}

void SetRespawn(edict_t *ent, gtime_t delay, bool hide_self)
{
	// already respawning
	if (ent->think == DoRespawn && ent->nextthink >= level.time)
		return;

	ent->flags |= FL_RESPAWN;

	if (hide_self)
	{
		ent->svflags |= ( SVF_NOCLIENT | SVF_RESPAWNING );
		ent->solid = SOLID_NOT;
		gi.linkentity(ent);
	}

	ent->nextthink = level.time + delay;
	ent->think = DoRespawn;
}

//======================================================================

bool IsInstantItemsEnabled()
{
	if (deathmatch->integer && g_dm_instant_items->integer)
	{
		return true;
	}

	if (!deathmatch->integer && level.instantitems)
	{
		return true;
	}

	return false;
}

bool Pickup_Powerup(edict_t *ent, edict_t *other)
{
	int quantity;

	quantity = other->client->pers.inventory[ent->item->id];
	if ((skill->integer == 0 && quantity >= 3) ||
		(skill->integer == 1 && quantity >= 2) ||
		(skill->integer >= 2 && quantity >= 1))
		return false;

	if (coop->integer && !P_UseCoopInstancedItems() && (ent->item->flags & IF_STAY_COOP) && (quantity > 0))
		return false;

	other->client->pers.inventory[ent->item->id]++;

	bool is_dropped_from_death = ent->spawnflags.has(SPAWNFLAG_ITEM_DROPPED_PLAYER) && !ent->spawnflags.has(SPAWNFLAG_ITEM_DROPPED);

	if (IsInstantItemsEnabled() ||
		((ent->item->use == Use_Quad) && is_dropped_from_death) ||
		((ent->item->use == Use_QuadFire) && is_dropped_from_death))
	{
		if ((ent->item->use == Use_Quad) && is_dropped_from_death)
			quad_drop_timeout_hack = (ent->nextthink - level.time);
		else if ((ent->item->use == Use_QuadFire) && is_dropped_from_death)
			quad_fire_drop_timeout_hack = (ent->nextthink - level.time);

		if (ent->item->use)
			ent->item->use(other, ent->item);
	}

	if (deathmatch->integer)
	{
		if (!(ent->spawnflags & SPAWNFLAG_ITEM_DROPPED))
			SetRespawn(ent, gtime_t::from_sec(ent->item->quantity));
	}

	return true;
}

bool Pickup_General(edict_t *ent, edict_t *other)
{
	if (other->client->pers.inventory[ent->item->id])
		return false;

	other->client->pers.inventory[ent->item->id]++;

	if (deathmatch->integer)
	{
		if (!(ent->spawnflags & SPAWNFLAG_ITEM_DROPPED))
			SetRespawn(ent, gtime_t::from_sec(ent->item->quantity));
	}

	return true;
}

void Drop_General(edict_t *ent, gitem_t *item)
{
	edict_t *dropped = Drop_Item(ent, item);
	dropped->spawnflags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
	dropped->svflags &= ~SVF_INSTANCED;
	ent->client->pers.inventory[item->id]--;
}

//======================================================================

void Use_Adrenaline(edict_t *ent, gitem_t *item)
{
	if (!deathmatch->integer)
		ent->max_health += 1;

	if (ent->health < ent->max_health)
		ent->health = ent->max_health;

	gi.sound(ent, CHAN_ITEM, gi.soundindex("items/n_health.wav"), 1, ATTN_NORM, 0);

	ent->client->pers.inventory[item->id]--;
}

inline bool G_AddAmmoAndCap(edict_t *other, item_id_t item, int32_t max, int32_t quantity)
{
	if (other->client->pers.inventory[item] >= max)
		return false;

	other->client->pers.inventory[item] += quantity;
	if (other->client->pers.inventory[item] > max)
		other->client->pers.inventory[item] = max;

	G_CheckPowerArmor(other);

	return true;
}

inline bool G_AddAmmoAndCapQuantity(edict_t *other, ammo_t ammo)
{
	gitem_t *item = GetItemByAmmo(ammo);
	return G_AddAmmoAndCap(other, item->id, other->client->pers.max_ammo[ammo], item->quantity);
}

inline void G_AdjustAmmoCap(edict_t *other, ammo_t ammo, int16_t new_max)
{
	other->client->pers.max_ammo[ammo] = max(other->client->pers.max_ammo[ammo], new_max);
}

//======================================================================

void Use_Quad(edict_t *ent, gitem_t *item)
{
	gtime_t timeout;

	ent->client->pers.inventory[item->id]--;

	if (quad_drop_timeout_hack)
	{
		timeout = quad_drop_timeout_hack;
		quad_drop_timeout_hack = 0_ms;
	}
	else
	{
		timeout = 30_sec;
	}

	ent->client->quad_time = max(level.time, ent->client->quad_time) + timeout;

	gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage.wav"), 1, ATTN_NORM, 0);
}

//======================================================================

bool Add_Ammo (edict_t * ent, gitem_t * item, int count)
{
	int index;
	int max = 0;


	if (!ent->client)
		return false;

	switch(item->typeNum) {
	case MK23_ANUM:
		max = ent->client->max_pistolmags;
		break;
	case SHELL_ANUM:
		max = ent->client->max_shells;
		break;
	case MP5_ANUM:
		max = ent->client->max_mp5mags;
		break;
	case M4_ANUM:
		max = ent->client->max_m4mags;
		break;
	case SNIPER_ANUM:
		max = ent->client->max_sniper_rnds;
		break;
	default:
		return false;
	}

	index = ITEM_INDEX (item);

	if (ent->client->inventory[index] == max)
		return false;

	ent->client->inventory[index] += count;

	if (ent->client->inventory[index] > max)
		ent->client->inventory[index] = max;

	return true;
}

bool Pickup_Ammo (edict_t * ent, edict_t * other)
{
	int count;
	bool weapon;

	weapon = (ent->item->flags & IT_WEAPON);
	if ((weapon) && g_infinite_ammo->integer)
		count = 1000;
	else if (ent->count)
		count = ent->count;
	else
		count = ent->item->quantity;

	if (!Add_Ammo (other, ent->item, count))
		return false;

	if (!(ent->spawnflags & (SPAWNFLAG_ITEM_DROPPED | SPAWNFLAG_ITEM_DROPPED_PLAYER)))
		SetRespawn (ent, ammo_respawn->value);

	return true;
}

void Drop_Ammo (edict_t * ent, gitem_t * item)
{
	edict_t *dropped;
	int index;

	if (ent->client->weaponstate == WEAPON_RELOADING)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "Cant drop ammo while reloading\n");
		return;
	}

	index = ITEM_INDEX (item);
	dropped = Drop_Item (ent, item);
	if (ent->client->inventory[index] >= item->quantity)
		dropped->count = item->quantity;
	else
		dropped->count = ent->client->inventory[index];
	ent->client->inventory[index] -= dropped->count;
	ValidateSelectedItem (ent);
}

void AddItem(edict_t *ent, gitem_t *item)
{
	ent->client->inventory[ITEM_INDEX (item)]++;
	ent->client->unique_item_total++;
	if (item == LASER_NUM)
	{
		SP_LaserSight(ent, item);	//ent->item->use(other, ent->item);
	}
	else if (item->typeNum == BAND_NUM)
	{

		if (ent->client->max_pistolmags < 4)
			ent->client->max_pistolmags = 4;
		if (ent->client->max_shells < 28)
			ent->client->max_shells = 28;
		if (ent->client->max_m4mags < 2)
			ent->client->max_m4mags = 2;
		if (ent->client->max_sniper_rnds < 40)
			ent->client->max_sniper_rnds = 40;
		if (ent->client->max_mp5mags < 4)
			ent->client->max_mp5mags = 4;
		if (ent->client->knife_max < 20)
			ent->client->knife_max = 20;
		if (ent->client->grenade_max < 4)
			ent->client->grenade_max = 4;
	}
}

// we just got weapon `item`, check if we should switch to it
void G_CheckAutoSwitch(edict_t *ent, gitem_t *item, bool is_new)
{
	// already using or switching to
	if (ent->client->pers.weapon == item ||
		ent->client->newweapon == item)
		return;
	// need ammo
	else if (item->ammo)
	{
		int32_t required_ammo = (item->flags & IF_AMMO) ? 1 : item->quantity;
		
		if (ent->client->pers.inventory[item->ammo] < required_ammo)
			return;
	}

	// check autoswitch setting
	if (ent->client->pers.autoswitch == auto_switch_t::NEVER)
		return;
	else if ((item->flags & IF_AMMO) && ent->client->pers.autoswitch == auto_switch_t::ALWAYS_NO_AMMO)
		return;
	else if (ent->client->pers.autoswitch == auto_switch_t::SMART)
	{
		bool using_blaster = ent->client->pers.weapon && ent->client->pers.weapon->id == IT_WEAPON_MK23;

		// smartness algorithm: in DM, we will always switch if we have the blaster out
		// otherwise leave our active weapon alone
		if (deathmatch->integer && !using_blaster)
			return;
		// in SP, only switch if it's a new weapon, or we have the blaster out
		else if (!deathmatch->integer && !using_blaster && !is_new)
			return;
	}

	// switch!
	ent->client->newweapon = item;
}

bool Pickup_Ammo(edict_t *ent, edict_t *other)
{
	int	 oldcount;
	int	 count;
	bool weapon;

	weapon = (ent->item->flags & IF_WEAPON);
	if (weapon && G_CheckInfiniteAmmo(ent->item))
		count = 1000;
	else if (ent->count)
		count = ent->count;
	else
		count = ent->item->quantity;

	oldcount = other->client->pers.inventory[ent->item->id];

	if (!Add_Ammo(other, ent->item, count))
		return false;

	if (weapon)
		G_CheckAutoSwitch(other, ent->item, !oldcount);

	if (!(ent->spawnflags & (SPAWNFLAG_ITEM_DROPPED | SPAWNFLAG_ITEM_DROPPED_PLAYER)) && deathmatch->integer)
		SetRespawn(ent, 30_sec);
	return true;
}

void Drop_Ammo(edict_t *ent, gitem_t *item)
{
	item_id_t index = item->id;
	edict_t *dropped = Drop_Item(ent, item);
	dropped->spawnflags |= SPAWNFLAG_ITEM_DROPPED_PLAYER;
	dropped->svflags &= ~SVF_INSTANCED;

	if (ent->client->pers.inventory[index] >= item->quantity)
		dropped->count = item->quantity;
	else
		dropped->count = ent->client->pers.inventory[index];

	if (ent->client->pers.weapon && ent->client->pers.weapon == item && (item->flags & IF_AMMO) &&
		ent->client->pers.inventory[index] - dropped->count <= 0)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "$g_cant_drop_weapon");
		G_FreeEdict(dropped);
		return;
	}

	ent->client->pers.inventory[index] -= dropped->count;
	G_CheckPowerArmor(ent);
}

bool Pickup_Health(edict_t *ent, edict_t *other)
{
	int health_flags = (ent->style ? ent->style : ent->item->tag);

	if (!(health_flags & HEALTH_IGNORE_MAX))
		if (other->health >= other->max_health)
			return false;

	int count = ent->count ? ent->count : ent->item->quantity;

	// ZOID
	if (deathmatch->integer && other->health >= 250 && count > 25)
		return false;
	// ZOID

	other->health += count;

	if (!(health_flags & HEALTH_IGNORE_MAX))
	{
		if (other->health > other->max_health)
			other->health = other->max_health;
	}

	if (ent->item->tag & HEALTH_TIMED)
	{
		if (!deathmatch->integer)
		{
			// mega health doesn't need to be special in SP
			// since it never respawns.
			other->client->pers.megahealth_time = 5_sec;
		}
		else
		{
			ent->think = MegaHealth_think;
			ent->nextthink = level.time + 5_sec;
			ent->owner = other;
			ent->flags |= FL_RESPAWN;
			ent->svflags |= SVF_NOCLIENT;
			ent->solid = SOLID_NOT;
		}
	}
	else
	{
		if (!(ent->spawnflags & SPAWNFLAG_ITEM_DROPPED) && deathmatch->integer)
			SetRespawn(ent, 30_sec);
	}

	return true;
}

//======================================================================

item_id_t ArmorIndex(edict_t *ent)
{
	if (ent->svflags & SVF_MONSTER)
		return ent->monsterinfo.armor_type;

	if (ent->client)
	{
		if (ent->client->pers.inventory[IT_ARMOR_JACKET] > 0)
			return IT_ARMOR_JACKET;
	}

	return IT_NULL;
}

bool Pickup_Armor(edict_t *ent, edict_t *other)
{
	item_id_t			 old_armor_index;
	const gitem_armor_t *oldinfo;
	const gitem_armor_t *newinfo;
	int					 newcount;
	float				 salvage;
	int					 salvagecount;

	// get info on new armor
	newinfo = ent->item->armor_info;

	old_armor_index = ArmorIndex(other);

	if (!old_armor_index) {
		other->client->pers.inventory[ent->item->id] = newinfo->base_count;
	}

	// use the better armor
	else
	{
		// get info on old armor
		if (old_armor_index == IT_ARMOR_JACKET)
			oldinfo = &jacketarmor_info;

		if (newinfo->normal_protection > oldinfo->normal_protection)
		{
			// calc new armor values
			salvage = oldinfo->normal_protection / newinfo->normal_protection;
			salvagecount = (int) (salvage * other->client->pers.inventory[old_armor_index]);
			newcount = newinfo->base_count + salvagecount;
			if (newcount > newinfo->max_count)
				newcount = newinfo->max_count;

			// zero count of old armor so it goes away
			other->client->pers.inventory[old_armor_index] = 0;

			// change armor to new item with computed value
			other->client->pers.inventory[ent->item->id] = newcount;
		}
		else
		{
			// calc new armor values
			salvage = newinfo->normal_protection / oldinfo->normal_protection;
			salvagecount = (int) (salvage * newinfo->base_count);
			newcount = other->client->pers.inventory[old_armor_index] + salvagecount;
			if (newcount > oldinfo->max_count)
				newcount = oldinfo->max_count;

			// if we're already maxed out then we don't need the new armor
			if (other->client->pers.inventory[old_armor_index] >= newcount)
				return false;

			// update current armor value
			other->client->pers.inventory[old_armor_index] = newcount;
		}
	}

	if (!(ent->spawnflags & SPAWNFLAG_ITEM_DROPPED) && deathmatch->integer)
		SetRespawn(ent, 20_sec);

	return true;
}

bool Entity_IsVisibleToPlayer(edict_t* ent, edict_t* player)
{
	return !ent->item_picked_up_by[player->s.number - 1];
}

/*
===============
Touch_Item
===============
*/
TOUCH(Touch_Item) (edict_t *ent, edict_t *other, const trace_t &tr, bool other_touching_self) -> void
{
	bool taken;

	if (!other->client)
		return;
	if (other->health < 1)
		return; // dead people can't pickup
	if (!ent->item->pickup)
		return; // not a grabbable item?

	// already got this instanced item
	if (coop->integer && P_UseCoopInstancedItems())
	{
		if (ent->item_picked_up_by[other->s.number - 1])
			return;
	}

	// ZOID
	if (CTFMatchSetup())
		return; // can't pick stuff up right now
				// ZOID

	taken = ent->item->pickup(ent, other);

	ValidateSelectedItem(other);

	if (taken)
	{
		// flash the screen
		other->client->bonus_alpha = 0.25;

		// show icon and name on status bar
		other->client->ps.stats[STAT_PICKUP_ICON] = gi.imageindex(ent->item->icon);
		other->client->ps.stats[STAT_PICKUP_STRING] = CS_ITEMS + ent->item->id;
		other->client->pickup_msg_time = level.time + 3_sec;

		// change selected item if we still have it
		if (ent->item->use && other->client->pers.inventory[ent->item->id])
		{
			other->client->ps.stats[STAT_SELECTED_ITEM] = other->client->pers.selected_item = ent->item->id;
			other->client->ps.stats[STAT_SELECTED_ITEM_NAME] = 0; // don't set name on pickup item since it's already there
		}

		if (ent->noise_index)
			gi.sound(other, CHAN_ITEM, ent->noise_index, 1, ATTN_NORM, 0);
		else if (ent->item->pickup_sound)
			gi.sound(other, CHAN_ITEM, gi.soundindex(ent->item->pickup_sound), 1, ATTN_NORM, 0);
		
		int32_t player_number = other->s.number - 1;

		if (coop->integer && P_UseCoopInstancedItems() && !ent->item_picked_up_by[player_number])
		{
			ent->item_picked_up_by[player_number] = true;

			// [Paril-KEX] this is to fix a coop quirk where items
			// that send a message on pick up will only print on the
			// player that picked them up, and never anybody else; 
			// when instanced items are enabled we don't need to limit
			// ourselves to this, but it does mean that relays that trigger
			// messages won't work, so we'll have to fix those
			if (ent->message)
				G_PrintActivationMessage(ent, other, false);
		}
	}

	if (!(ent->spawnflags & SPAWNFLAG_ITEM_TARGETS_USED))
	{
		// [Paril-KEX] see above msg; this also disables the message in DM
		// since there's no need to print pickup messages in DM (this wasn't
		// even a documented feature, relays were traditionally used for this)
		const char *message_backup = nullptr;

		if (deathmatch->integer || (coop->integer && P_UseCoopInstancedItems()))
			std::swap(message_backup, ent->message);

		G_UseTargets(ent, other);
		
		if (deathmatch->integer || (coop->integer && P_UseCoopInstancedItems()))
			std::swap(message_backup, ent->message);

		ent->spawnflags |= SPAWNFLAG_ITEM_TARGETS_USED;
	}

	if (taken)
	{
		bool should_remove = false;

		if (coop->integer)
		{
			// in coop with instanced items, *only* dropped 
			// player items will ever get deleted permanently.
			if (P_UseCoopInstancedItems())
				should_remove = ent->spawnflags.has(SPAWNFLAG_ITEM_DROPPED_PLAYER);
			// in coop without instanced items, IF_STAY_COOP items remain
			// if not dropped
			else
				should_remove = ent->spawnflags.has(SPAWNFLAG_ITEM_DROPPED | SPAWNFLAG_ITEM_DROPPED_PLAYER) || !(ent->item->flags & IF_STAY_COOP);
		}
		else
			should_remove = !deathmatch->integer || ent->spawnflags.has(SPAWNFLAG_ITEM_DROPPED | SPAWNFLAG_ITEM_DROPPED_PLAYER);

		if (should_remove)
		{
			if (ent->flags & FL_RESPAWN)
				ent->flags &= ~FL_RESPAWN;
			else
				G_FreeEdict(ent);
		}
	}
}

//======================================================================

TOUCH(drop_temp_touch) (edict_t *ent, edict_t *other, const trace_t &tr, bool other_touching_self) -> void
{
	if (other == ent->owner)
		return;

	Touch_Item(ent, other, tr, other_touching_self);
}

THINK(drop_make_touchable) (edict_t *ent) -> void
{
	ent->touch = Touch_Item;
	if (deathmatch->integer)
	{
		ent->nextthink = level.time + 29_sec;
		ent->think = G_FreeEdict;
	}
}

edict_t *Drop_Item(edict_t *ent, gitem_t *item)
{
	edict_t *dropped;
	vec3_t	 forward, right;
	vec3_t	 offset;

	dropped = G_Spawn();

	dropped->item = item;
	dropped->spawnflags = SPAWNFLAG_ITEM_DROPPED;
	dropped->classname = item->classname;
	dropped->s.effects = item->world_model_flags;
	gi.setmodel(dropped, dropped->item->world_model);
	dropped->s.renderfx = RF_GLOW | RF_NO_LOD | RF_IR_VISIBLE; // PGM
	dropped->mins = { -15, -15, -15 };
	dropped->maxs = { 15, 15, 15 };
	dropped->solid = SOLID_TRIGGER;
	dropped->movetype = MOVETYPE_TOSS;
	dropped->touch = drop_temp_touch;
	dropped->owner = ent;

	if (ent->client)
	{
		trace_t trace;

		AngleVectors(ent->client->v_angle, forward, right, nullptr);
		offset = { 24, 0, -16 };
		dropped->s.origin = G_ProjectSource(ent->s.origin, offset, forward, right);
		trace = gi.trace(ent->s.origin, dropped->mins, dropped->maxs, dropped->s.origin, ent, CONTENTS_SOLID);
		dropped->s.origin = trace.endpos;
	}
	else
	{
		AngleVectors(ent->s.angles, forward, right, nullptr);
		dropped->s.origin = (ent->absmin + ent->absmax) / 2;
	}

	G_FixStuckObject(dropped, dropped->s.origin);

	dropped->velocity = forward * 100;
	dropped->velocity[2] = 300;

	dropped->think = drop_make_touchable;
	dropped->nextthink = level.time + 1_sec;

	if (coop->integer && P_UseCoopInstancedItems())
		dropped->svflags |= SVF_INSTANCED;

	gi.linkentity(dropped);
	return dropped;
}

USE(Use_Item) (edict_t *ent, edict_t *other, edict_t *activator) -> void
{
	ent->svflags &= ~SVF_NOCLIENT;
	ent->use = nullptr;

	if (ent->spawnflags.has(SPAWNFLAG_ITEM_NO_TOUCH))
	{
		ent->solid = SOLID_BBOX;
		ent->touch = nullptr;
	}
	else
	{
		ent->solid = SOLID_TRIGGER;
		ent->touch = Touch_Item;
	}

	gi.linkentity(ent);
}

//======================================================================

/*
================
droptofloor
================
*/
THINK(droptofloor) (edict_t *ent) -> void
{
	trace_t tr;
	vec3_t	dest;

	// [Paril-KEX] scale foodcube based on how much we ingested
	if (strcmp(ent->classname, "item_foodcube") == 0)
	{
		ent->mins = vec3_t { -8, -8, -8 } * ent->s.scale;
		ent->maxs = vec3_t { 8, 8, 8 } * ent->s.scale;
	}
	else
	{
		ent->mins = { -15, -15, -15 };
		ent->maxs = { 15, 15, 15 };
	}

	if (ent->model)
		gi.setmodel(ent, ent->model);
	else
		gi.setmodel(ent, ent->item->world_model);
	ent->solid = SOLID_TRIGGER;
	ent->movetype = MOVETYPE_TOSS;
	ent->touch = Touch_Item;

	dest = ent->s.origin + vec3_t { 0, 0, -128 };

	tr = gi.trace(ent->s.origin, ent->mins, ent->maxs, dest, ent, MASK_SOLID);
	if (tr.startsolid)
	{
		if (G_FixStuckObject(ent, ent->s.origin) == stuck_result_t::NO_GOOD_POSITION)
		{
			// RAFAEL
			if (strcmp(ent->classname, "item_foodcube") == 0)
				ent->velocity[2] = 0;
			else
			{
				// RAFAEL
				gi.Com_PrintFmt("{}: droptofloor: startsolid\n", *ent);
				G_FreeEdict(ent);
				return;
				// RAFAEL
			}
			// RAFAEL
		}
	}
	else
		ent->s.origin = tr.endpos;

	if (ent->team)
	{
		ent->flags &= ~FL_TEAMSLAVE;
		ent->chain = ent->teamchain;
		ent->teamchain = nullptr;

		ent->svflags |= SVF_NOCLIENT;
		ent->solid = SOLID_NOT;

		if (ent == ent->teammaster)
		{
			ent->nextthink = level.time + 10_hz;
			ent->think = DoRespawn;
		}
	}

	if (ent->spawnflags.has(SPAWNFLAG_ITEM_NO_TOUCH))
	{
		ent->solid = SOLID_BBOX;
		ent->touch = nullptr;
		ent->s.effects &= ~(EF_ROTATE | EF_BOB);
		ent->s.renderfx &= ~RF_GLOW;
	}

	if (ent->spawnflags.has(SPAWNFLAG_ITEM_TRIGGER_SPAWN))
	{
		ent->svflags |= SVF_NOCLIENT;
		ent->solid = SOLID_NOT;
		ent->use = Use_Item;
	}

	ent->watertype = gi.pointcontents(ent->s.origin);
	gi.linkentity(ent);
}

/*
===============
PrecacheItem

Precaches all data needed for a given item.
This will be called for each item spawned in a level,
and for each item in each client's inventory.
===============
*/
void PrecacheItem(gitem_t *it)
{
	const char *s, *start;
	char		data[MAX_QPATH];
	ptrdiff_t	len;
	gitem_t	*ammo;

	if (!it)
		return;

	if (it->pickup_sound)
		gi.soundindex(it->pickup_sound);
	if (it->world_model)
		gi.modelindex(it->world_model);
	if (it->view_model)
		gi.modelindex(it->view_model);
	if (it->icon)
		gi.imageindex(it->icon);

	// parse everything for its ammo
	if (it->ammo)
	{
		ammo = GetItemByIndex(it->ammo);
		if (ammo != it)
			PrecacheItem(ammo);
	}

	// parse the space seperated precache string for other items
	s = it->precaches;
	if (!s || !s[0])
		return;

	while (*s)
	{
		start = s;
		while (*s && *s != ' ')
			s++;

		len = s - start;
		if (len >= MAX_QPATH || len < 5)
			gi.Com_ErrorFmt("PrecacheItem: {} has bad precache string", it->classname);
		memcpy(data, start, len);
		data[len] = 0;
		if (*s)
			s++;

		// determine type based on extension
		if (!strcmp(data + len - 3, "md2"))
			gi.modelindex(data);
		else if (!strcmp(data + len - 3, "sp2"))
			gi.modelindex(data);
		else if (!strcmp(data + len - 3, "wav"))
			gi.soundindex(data);
		if (!strcmp(data + len - 3, "pcx"))
			gi.imageindex(data);
	}
}

/*
============
SpawnItem

Sets the clipping size and plants the object on the floor.

Items can't be immediately dropped to floor, because they might
be on an entity that hasn't spawned yet.
============
*/
void SpawnItem(edict_t *ent, gitem_t *item)
{
	// [Sam-KEX]
	// Paril: allow all keys to be trigger_spawn'd (N64 uses this
	// a few different times)
	if (item->flags & IF_KEY)
	{
		if (ent->spawnflags.has(SPAWNFLAG_ITEM_TRIGGER_SPAWN))
		{
			ent->svflags |= SVF_NOCLIENT;
			ent->solid = SOLID_NOT;
			ent->use = Use_Item;
		}
		if (ent->spawnflags.has(SPAWNFLAG_ITEM_NO_TOUCH))
		{
			ent->solid = SOLID_BBOX;
			ent->touch = nullptr;
			ent->s.effects &= ~(EF_ROTATE | EF_BOB);
			ent->s.renderfx &= ~RF_GLOW;
		}
	}
	else if (ent->spawnflags.value >= SPAWNFLAG_ITEM_MAX.value) // PGM
	{
		ent->spawnflags = SPAWNFLAG_NONE;
		gi.Com_PrintFmt("{} has invalid spawnflags set\n", *ent);
	}

	// some items will be prevented in deathmatch
	if (deathmatch->integer)
	{
		// [Kex] In instagib, spawn no pickups!
		if (g_instagib->value)
		{
			if (item->pickup == Pickup_Armor || item->pickup == Pickup_PowerArmor ||
				item->pickup == Pickup_Powerup || item->pickup == Pickup_Sphere || item->pickup == Pickup_Doppleganger ||
				(item->flags & IF_HEALTH) || (item->flags & IF_AMMO) || item->pickup == Pickup_Weapon || item->pickup == Pickup_Pack)
			{
				G_FreeEdict(ent);
				return;
			}
		}

		if (g_no_armor->integer)
		{
			if (item->pickup == Pickup_Armor || item->pickup == Pickup_PowerArmor)
			{
				G_FreeEdict(ent);
				return;
			}
		}
		if (g_no_items->integer)
		{
			if (item->pickup == Pickup_Powerup)
			{
				G_FreeEdict(ent);
				return;
			}
			//=====
			// ROGUE
			if (item->pickup == Pickup_Sphere)
			{
				G_FreeEdict(ent);
				return;
			}
			if (item->pickup == Pickup_Doppleganger)
			{
				G_FreeEdict(ent);
				return;
			}
			// ROGUE
			//=====
		}
		if (g_no_health->integer)
		{
			if (item->flags & IF_HEALTH)
			{
				G_FreeEdict(ent);
				return;
			}
		}
		if (G_CheckInfiniteAmmo(item))
		{
			if (item->flags == IF_AMMO)
			{
				G_FreeEdict(ent);
				return;
			}

			// [Paril-KEX] some item swappage 
			// BFG too strong in Infinite Ammo mode
			if (item->id == IT_WEAPON_BFG)
				item = GetItemByIndex(IT_WEAPON_DISRUPTOR);
		}

		//==========
		// ROGUE
		if (g_no_mines->integer)
		{
			if (item->id == IT_WEAPON_PROXLAUNCHER || item->id == IT_AMMO_PROX || item->id == IT_AMMO_TESLA || item->id == IT_AMMO_TRAP)
			{
				G_FreeEdict(ent);
				return;
			}
		}
		if (g_no_nukes->integer)
		{
			if (item->id == IT_AMMO_NUKE)
			{
				G_FreeEdict(ent);
				return;
			}
		}
		if (g_no_spheres->integer)
		{
			if (item->pickup == Pickup_Sphere)
			{
				G_FreeEdict(ent);
				return;
			}
		}
		// ROGUE
		//==========
	}

	//==========
	// ROGUE
	// DM only items
	if (!deathmatch->integer)
	{
		if (item->pickup == Pickup_Doppleganger || item->pickup == Pickup_Nuke)
		{
			gi.Com_PrintFmt("{} spawned in non-DM; freeing...\n", *ent);
			G_FreeEdict(ent);
			return;
		}
		if ((item->use == Use_Vengeance) || (item->use == Use_Hunter))
		{
			gi.Com_PrintFmt("{} spawned in non-DM; freeing...\n", *ent);
			G_FreeEdict(ent);
			return;
		}
	}
	// ROGUE
	//==========

	// [Paril-KEX] power armor breaks infinite ammo
	if (G_CheckInfiniteAmmo(item))
	{
		if (item->id == IT_ITEM_POWER_SHIELD || item->id == IT_ITEM_POWER_SCREEN)
			item = GetItemByIndex(IT_ARMOR_BODY);
	}

	// ZOID
	// Don't spawn the flags unless enabled
	if (!ctf->integer && (item->id == IT_FLAG1 || item->id == IT_FLAG2))
	{
		G_FreeEdict(ent);
		return;
	}
	// ZOID

	// set final classname now
	ent->classname = item->classname;

	PrecacheItem(item);

	if (coop->integer && (item->id == IT_KEY_POWER_CUBE || item->id == IT_KEY_EXPLOSIVE_CHARGES))
	{
		ent->spawnflags.value |= (1 << (8 + level.power_cubes));
		level.power_cubes++;
	}

	// mark all items as instanced
	if (coop->integer)
	{
		if (P_UseCoopInstancedItems())
			ent->svflags |= SVF_INSTANCED;
	}

	ent->item = item;
	ent->nextthink = level.time + 20_hz; // items start after other solids
	ent->think = droptofloor;
	ent->s.effects = item->world_model_flags;
	ent->s.renderfx = RF_GLOW | RF_NO_LOD;
	if (ent->model)
		gi.modelindex(ent->model);

	if (ent->spawnflags.has(SPAWNFLAG_ITEM_TRIGGER_SPAWN))
		SetTriggeredSpawn(ent);

	// ZOID
	// flags are server animated and have special handling
	if (item->id == IT_FLAG1 || item->id == IT_FLAG2)
	{
		ent->think = CTFFlagSetup;
	}
	// ZOID
}

void P_ToggleFlashlight(edict_t *ent, bool state)
{
	if (!!(ent->flags & FL_FLASHLIGHT) == state)
		return;

	ent->flags ^= FL_FLASHLIGHT;

	gi.sound(ent, CHAN_AUTO, gi.soundindex(ent->flags & FL_FLASHLIGHT ? "items/flashlight_on.wav" : "items/flashlight_off.wav"), 1.f, ATTN_STATIC, 0);
}

void Use_Flashlight(edict_t *ent, gitem_t *inv)
{
	P_ToggleFlashlight(ent, !(ent->flags & FL_FLASHLIGHT));
}

constexpr size_t MAX_TEMP_POI_POINTS = 128;

void Compass_Update(edict_t *ent, bool first)
{
	vec3_t *&points = level.poi_points[ent->s.number - 1];

	// deleted for some reason
	if (!points)
		return;

	if (!ent->client->help_draw_points)
		return;
	if (ent->client->help_draw_time >= level.time)
		return;

	// don't draw too many points
	float distance = (points[ent->client->help_draw_index] - ent->s.origin).length();
	if (distance > 4096 ||
		!gi.inPHS(ent->s.origin, points[ent->client->help_draw_index], false))
	{
		ent->client->help_draw_points = false;
		return;
	}

	gi.WriteByte(svc_help_path);
	gi.WriteByte(first ? 1 : 0);
	gi.WritePosition(points[ent->client->help_draw_index]);
	
	if (ent->client->help_draw_index == ent->client->help_draw_count - 1)
		gi.WriteDir((ent->client->help_poi_location - points[ent->client->help_draw_index]).normalized());
	else
		gi.WriteDir((points[ent->client->help_draw_index + 1] - points[ent->client->help_draw_index]).normalized());
	gi.unicast(ent, false);

	P_SendLevelPOI(ent);

	gi.local_sound(ent, points[ent->client->help_draw_index], world, CHAN_AUTO, gi.soundindex("misc/help_marker.wav"), 1.0f, ATTN_NORM, 0.0f, GetUnicastKey());

	// done
	if (ent->client->help_draw_index == ent->client->help_draw_count - 1)
	{
		ent->client->help_draw_points = false;
		return;
	}

	ent->client->help_draw_index++;
	ent->client->help_draw_time = level.time + 200_ms;
}

static void Use_Compass(edict_t *ent, gitem_t *inv)
{
	if (!level.valid_poi)
	{
		gi.LocClient_Print(ent, PRINT_HIGH, "$no_valid_poi");
		return;
	}

	if (level.current_dynamic_poi)
		level.current_dynamic_poi->use(level.current_dynamic_poi, ent, ent);
	
	ent->client->help_poi_location = level.current_poi;
	ent->client->help_poi_image = level.current_poi_image;

	vec3_t *&points = level.poi_points[ent->s.number - 1];

	if (!points)
		points = (vec3_t *) gi.TagMalloc(sizeof(vec3_t) * (MAX_TEMP_POI_POINTS + 1), TAG_LEVEL);

	PathRequest request;
	request.start = ent->s.origin;
	request.goal = level.current_poi;
	request.moveDist = 64.f;
	request.pathFlags = PathFlags::All;
	request.nodeSearch.ignoreNodeFlags = true;
	request.nodeSearch.minHeight = 128.0f;
	request.nodeSearch.maxHeight = 128.0f;
	request.nodeSearch.radius = 1024.0f;
	request.pathPoints.array = points + 1;
	request.pathPoints.count = MAX_TEMP_POI_POINTS;

	PathInfo info;

	if (gi.GetPathToGoal(request, info))
	{
		// TODO: optimize points?
		ent->client->help_draw_points = true;
		ent->client->help_draw_count = min((size_t)info.numPathPoints, MAX_TEMP_POI_POINTS);
		ent->client->help_draw_index = 1;

		// remove points too close to the player so they don't have to backtrack
		for (int i = 1; i < 1 + ent->client->help_draw_count; i++)
		{
			float distance = (points[i] - ent->s.origin).length();
			if (distance > 192)
			{
				break;
			}

			ent->client->help_draw_index = i;
		}

		// create an extra point in front of us if we're facing away from the first real point
		float d = ((*(points + ent->client->help_draw_index)) - ent->s.origin).normalized().dot(ent->client->v_forward);

		if (d < 0.3f)
		{
			vec3_t p = ent->s.origin + (ent->client->v_forward * 64.f);

			trace_t tr = gi.traceline(ent->s.origin + vec3_t{0.f, 0.f, (float) ent->viewheight}, p, nullptr, MASK_SOLID);

			ent->client->help_draw_index--;
			ent->client->help_draw_count++;

			if (tr.fraction < 1.0f)
				tr.endpos += tr.plane.normal * 8.f;

			*(points + ent->client->help_draw_index) = tr.endpos;
		}

		ent->client->help_draw_time = 0_ms;
		Compass_Update(ent, true);
	}
	else
	{
		P_SendLevelPOI(ent);
		gi.local_sound(ent, CHAN_AUTO, gi.soundindex("misc/help_marker.wav"), 1.f, ATTN_NORM, 0, GetUnicastKey());
	}
}

//======================================================================

// clang-format off
gitem_t	itemlist[] = 
{
	{ },	// leave index 0 alone
	//
	// WEAPONS 
	//

/* weapon_grapple (.3 .3 1) (-16 -16 -16) (16 16 16)
always owned, never in the world
*/
	{
		/* id */ IT_WEAPON_GRAPPLE,
		/* classname */ "weapon_grapple", 
		/* pickup */ nullptr,
		/* use */ Use_Weapon,
		/* drop */ nullptr,
		/* weaponthink */ CTFWeapon_Grapple,
		/* pickup_sound */ nullptr,
		/* world_model */ nullptr,
		/* world_model_flags */ EF_NONE,
		/* view_model */ "models/weapons/grapple/tris.md2",
		/* icon */ "w_grapple",
		/* use_name */  "Grapple",
		/* pickup_name */  "$item_grapple",
		/* pickup_name_definite */ "$item_grapple_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_WEAPON | IF_NO_HASTE | IF_POWERUP_WHEEL | IF_NOT_RANDOM,
		/* vwep_model */ "#w_grapple.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "weapons/grapple/grfire.wav weapons/grapple/grpull.wav weapons/grapple/grhang.wav weapons/grapple/grreset.wav weapons/grapple/grhit.wav weapons/grapple/grfly.wav"
	},

/* weapon_blaster (.3 .3 1) (-16 -16 -16) (16 16 16)
always owned, never in the world
*/
	{
		/* id */ IT_WEAPON_MK23,
		/* classname */ "weapon_Mk23", 
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ nullptr,
		/* weaponthink */ Weapon_MK23,
		/* no pickup_sound */ nullptr,
		/* world_model */ "models/weapons/g_dual/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ "models/weapons/v_blast/tris.md2",
		/* icon */ "w_mk23",
		/* use_name */  MK23_NAME,
		/* pickup_name */  0,
		/* pickup_name_definite */ MK23_NAME,
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_WEAPON_BLASTER,
		/* flags */ IF_WEAPON,
		/* vwep_model */ "#w_blaster.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "weapons/mk23fire.wav weapons/mk23in.wav weapons/mk23out.wav weapons/mk23slap.wav weapons/mk23slide.wav misc/click.wav weapons/machgf4b.wav weapons/blastf1a.wav",
	},

	/*QUAKED weapon_chainfist (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
	*/
	{
		/* id */ IT_WEAPON_MP5,
		/* classname */ "weapon_MP5",
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_MP5,
		/* no pickup_sound */ nullptr,
		/* world_model */ "models/weapons/g_machn/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ "models/weapons/v_machn/tris.md2",
		/* icon */ "w_mp5",
		/* use_name */  MP5_NAME,
		/* pickup_name */  MP5_NAME,
		/* pickup_name_definite */ MP5_NAME,
		/* quantity */ 0,
		/* ammo */ MP5_AMMO_NAME,
		/* chain */ IT_WEAPON_BLASTER,
		/* flags */ IF_WEAPON,
		/* vwep_model */ "#w_chainfist.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */  "weapons/mp5fire1.wav weapons/mp5in.wav weapons/mp5out.wav weapons/mp5slap.wav weapons/mp5slide.wav weapons/machgf1b.wav weapons/machgf2b.wav weapons/machgf3b.wav weapons/machgf5b.wav",
	},

/*QUAKED weapon_m4 (.3 .3 1) (-16 -16 -16) (16 16 16)
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_m4/tris.md2"
*/
	{
		/* id */ IT_WEAPON_M4,
		/* classname */ "weapon_m4", 
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_M4,
		/* no pickup_sound */ nullptr,
		/* world_model */ "models/weapons/g_m4/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ "models/weapons/v_m4/tris.md2",
		/* icon */ "w_m4",
		/* use_name */  M4_NAME,
		/* pickup_name */  M4_NAME,
		/* pickup_name_definite */ M4_NAME,
		/* quantity */ 0,
		/* ammo */ IT_AMMO_SHELLS,
		/* chain */ IT_NULL,
		/* flags */ IF_WEAPON,
		/* vwep_model */ "#w_shotgun.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */  "weapons/m4a1fire.wav weapons/m4a1in.wav weapons/m4a1out.wav weapons/m4a1slide.wav weapons/rocklf1a.wav weapons/rocklr1b.wav",
	},

/*QUAKED weapon_m3 (.3 .3 1) (-16 -16 -16) (16 16 16)
-------- MODEL FOR RADIANT ONLY - DO NOT SET THIS AS A KEY --------
model="models/weapons/g_shotg/tris.md2"
*/
	{
		/* id */ IT_WEAPON_M3,
		/* classname */ "weapon_M4", 
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_M3,
		/* no pickup_sound */ nullptr,
		/* world_model */ "models/weapons/g_shotg/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ "models/weapons/v_shotg/tris.md2",
		/* icon */ "w_super90",
		/* use_name */  M3_NAME,
		/* pickup_name */  M3_NAME,
		/* pickup_name_definite */ M3_NAME,
		/* quantity */ 0,
		/* ammo */ IT_AMMO_SHELLS,
		/* chain */ IT_NULL,
		/* flags */ IF_WEAPON,
		/* vwep_model */ "#w_shotgun.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "weapons/m3in.wav weapons/shotgr1b.wav weapons/shotgf1b.wav",
	},

/*QUAKED weapon_handcannon (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_WEAPON_HANDCANNON,
		/* classname */ "weapon_HC", 
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_HC,
		/* no pickup_sound */ nullptr,
		/* world_model */ "models/weapons/g_cannon/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ "models/weapons/v_cannon/tris.md2",
		/* icon */ "w_cannon",
		/* use_name */  HC_NAME,
		/* pickup_name */  "$item_super_shotgun",
		/* pickup_name_definite */ "$item_super_shotgun_def",
		/* quantity */ 2,
		/* ammo */ IT_AMMO_SHELLS,
		/* chain */ IT_NULL,
		/* flags */ IF_WEAPON,
		/* vwep_model */ "#w_sshotgun.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "weapons/cannon_fire.wav weapons/sshotf1b.wav weapons/cclose.wav weapons/cin.wav weapons/cout.wav weapons/copen.wav",
		/* sort_id */ 0,
		/* quantity_warn */ 0
	},

/*QUAKED weapon_sniper (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_WEAPON_SNIPER,
		/* classname */ "weapon_Sniper", 
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_Sniper,
		/* no pickup_sound */ nullptr,
		/* world_model */ "models/weapons/g_sniper/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ "models/weapons/v_sniper/tris.md2",
		/* icon */ "w_sniper",
		/* use_name */  SNIPER_NAME,
		/* pickup_name */  "$item_machinegun",
		/* pickup_name_definite */ "$item_machinegun_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_BULLETS,
		/* chain */ IT_WEAPON_SNIPER,
		/* flags */ IF_WEAPON,
		/* vwep_model */ "#w_machinegun.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "weapons/ssgbolt.wav weapons/ssgfire.wav weapons/ssgin.wav misc/lensflik.wav weapons/hyprbl1a.wav weapons/hyprbf1a.wav",
		/* sort_id */ 0
	},

/*QUAKED weapon_dual (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_WEAPON_DUALMK23,
		/* classname */ "weapon_Dual", 
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_Dual,
		/* no pickup_sound */ nullptr,
		/* world_model */ "models/weapons/g_dual/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ "models/weapons/v_dual/tris.md2",
		/* icon */ "w_akimbo",
		/* use_name */  DUAL_NAME,
		/* pickup_name */  "$item_chaingun",
		/* pickup_name_definite */ "$item_chaingun_def",
		/* quantity */ 1,
		/* ammo */ IT_AMMO_BULLETS,
		/* chain */ IT_NULL,
		/* flags */ IF_WEAPON,
		/* vwep_model */ "#weapon.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */    "weapons/mk23fire.wav weapons/mk23in.wav weapons/mk23out.wav weapons/mk23slap.wav weapons/mk23slide.wav",
		/* sort_id */ 0
	},

/*QUAKED weapon_knife (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_WEAPON_KNIFE,
		/* classname */ "weapon_Knife", 
		/* pickup */ Pickup_Weapon,
		/* use */ Use_Weapon,
		/* drop */ Drop_Weapon,
		/* weaponthink */ Weapon_Knife,
		/* no pickup_sound */ nullptr,
		/* world_model */ "models/weapons/knife/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ "models/weapons/v_knife/tris.md2",
		/* icon */ "w_knife",
		/* use_name */  DUAL_NAME,
		/* pickup_name */  "$item_chaingun",
		/* pickup_name_definite */ "$item_chaingun_def",
		/* quantity */ 1,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_WEAPON,
		/* vwep_model */ "#weapon.md2",
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "weapons/throw.wav weapons/stab.wav weapons/swish.wav weapons/clank.wav",
		/* sort_id */ 0
	},

/*QUAKED weapon_grenade (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_WEAPON_GRENADES,
		/* classname */ "weapon_Grenade",
		/* pickup */ Pickup_Ammo,
		/* use */ Use_Weapon,
		/* drop */ Drop_Ammo,
		/* weaponthink */ Weapon_Gas,
		/* no pickup_sound */ nullptr,
		/* world_model */ "models/objects/grenade2/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ "models/weapons/v_handgr/tris.md2",
		/* icon */ "a_m61frag",
		/* use_name */  GRENADE_NAME,
		/* pickup_name */  "$item_grenades",
		/* pickup_name_definite */ "$item_grenades_def",
		/* quantity */ 5,
		/* ammo */ IF_AMMO,
		/* chain */ IF_AMMO,
		/* flags */ IF_WEAPON,
		/* vwep_model */ "#a_grenades.md2",
		/* armor_info */ nullptr,
		/* tag */ AMMO_GRENADES,
		/* precaches */  "misc/grenade.wav weapons/grenlb1b.wav weapons/hgrent1a.wav",
		/* sort_id */ 0
	},

	//
	// AMMO ITEMS
	//

/*QUAKED ammo_shells (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_AMMO_SHELLS,
		/* classname */ "ammo_shells",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/am_pkup.wav",
		/* world_model */ "models/items/ammo/shells/medium/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_shells",
		/* use_name */  "Shells",
		/* pickup_name */  "$item_shells",
		/* pickup_name_definite */ "$item_shells_def",
		/* quantity */ 10,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_SHELLS
	},

/*QUAKED ammo_clip (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_AMMO_BULLETS,
		/* classname */ "ammo_clip",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* no pickup_sound */ nullptr,
		/* world_model */ "models/items/ammo/clip/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_clip",
		/* use_name */  MK23_AMMO_NAME,
		/* pickup_name */  "$item_bullets",
		/* pickup_name_definite */ "$item_bullets_def",
		/* quantity */ 50,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_BULLETS
	},

/*QUAKED ammo_mag (.3 .3 1) (-16 -16 -16) (16 16 16)
model="models/items/ammo/mag/medium/tris.md2"
*/
	{
		/* id */ IT_AMMO_MAG,
		/* classname */ "ammo_mag",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* no pickup_sound */ nullptr,
		/* world_model */ "models/items/ammo/mag/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_mag",
		/* use_name */  MP5_AMMO_NAME,
		/* pickup_name */  "$item_rockets",
		/* pickup_name_definite */ "$item_rockets_def",
		/* quantity */ 1,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_MAG
	},

/*QUAKED ammo_m4 (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_AMMO_M4,
		/* classname */ "ammo_m4",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ nullptr,
		/* world_model */ "models/items/ammo/m4/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_m4",
		/* use_name */  M4_AMMO_NAME,
		/* pickup_name */  "$item_cells",
		/* pickup_name_definite */ "$item_cells_def",
		/* quantity */ 1,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_M4
	},

/*QUAKED ammo_slugs (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_AMMO_SNIPER,
		/* classname */ "ammo_sniper",
		/* pickup */ Pickup_Ammo,
		/* use */ nullptr,
		/* drop */ Drop_Ammo,
		/* weaponthink */ nullptr,
		/* pickup_sound */ nullptr,
		/* world_model */ "models/items/ammo/sniper/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "a_bullets",
		/* use_name */  SNIPER_AMMO_NAME,
		/* pickup_name */  "$item_slugs",
		/* pickup_name_definite */ "$item_slugs_def",
		/* quantity */ 10,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_AMMO,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ AMMO_SNIPER
	},
	//
	// POWERUP ITEMS
	//

/*QUAKED item_vest (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_ITEM_VEST,
		/* classname */ "item_vest", 
		/* pickup */ Pickup_Special,
		/* use */ nullptr,
		/* drop */ Drop_Special,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/veston.wav",
		/* world_model */ "models/items/armor/jacket/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "i_jacketarmor",
		/* use_name */  KEV_NAME,
		/* pickup_name */  "$item_vest",
		/* pickup_name_definite */ "$item_vest_def",
		/* quantity */ 1,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_VEST,
		/* precaches */ ""
	},

/*QUAKED item_quiet (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_ITEM_QUIET,
		/* classname */ "item_quiet", 
		/* pickup */ Pickup_Special,
		/* use */ nullptr,
		/* drop */ Drop_Special,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/screw.wav",
		/* world_model */ "models/items/quiet/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "p_silencer",
		/* use_name */  SIL_NAME,
		/* pickup_name */  "$item_quiet",
		/* pickup_name_definite */ "$item_quiet_def",
		/* quantity */ 1,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_QUIET,
		/* precaches */ ""
	},

// RAFAEL
/*QUAKED item_slipers (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
	{
		/* id */ IT_ITEM_SLIPPERS,
		/* classname */ "item_slippers", 
		/* pickup */ Pickup_Special,
		/* use */ nullptr,
		/* drop */ Drop_Special,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "misc/veston.wav",
		/* world_model */ "models/items/slippers/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "slippers",
		/* use_name */  SLIP_NAME,
		/* pickup_name */  "$item_slippers",
		/* pickup_name_definite */ "$item_slippers_def",
		/* quantity */ 1,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_POWERUP,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_SLIPPERS,
		/* precaches */ ""
	},

/*QUAKED item_health (.3 .3 1) (-16 -16 -16) (16 16 16)
model="models/items/healing/medium/tris.md2"
*/
	{
		/* id */ IT_HEALTH_MEDIUM,
		/* classname */ "item_health",
		/* pickup */ Pickup_Health,
		/* use */ nullptr,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/n_health.wav",
		/* world_model */ "models/items/healing/medium/tris.md2",
		/* world_model_flags */ EF_NONE,
		/* view_model */ nullptr,
		/* icon */ "i_health",
		/* use_name */  "Health",
		/* pickup_name */  "$item_small_medkit",
		/* pickup_name_definite */ "$item_small_medkit_def",
		/* quantity */ 10,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_HEALTH
	},

//ZOID
/*QUAKED item_flag_team1 (1 0.2 0) (-16 -16 -24) (16 16 32)
*/
	{
		/* id */ IT_FLAG1,
		/* classname */ "item_flag_team1",
		/* pickup */ CTFPickup_Flag,
		/* use */ nullptr,
		/* drop */ CTFDrop_Flag, //Should this be null if we don't want players to drop it manually?
		/* weaponthink */ nullptr,
		/* pickup_sound */ "ctf/flagtk.wav",
		/* world_model */ "players/male/flag1.md2",
		/* world_model_flags */ EF_FLAG1,
		/* view_model */ nullptr,
		/* icon */ "i_ctf1",
		/* use_name */  "Red Flag",
		/* pickup_name */  "$item_red_flag",
		/* pickup_name_definite */ "$item_red_flag_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_NONE,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "ctf/flagcap.wav"
	},

/*QUAKED item_flag_team2 (1 0.2 0) (-16 -16 -24) (16 16 32)
*/
	{
		/* id */ IT_FLAG2,
		/* classname */ "item_flag_team2",
		/* pickup */ CTFPickup_Flag,
		/* use */ nullptr,
		/* drop */ CTFDrop_Flag,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "ctf/flagtk.wav",
		/* world_model */ "players/male/flag2.md2",
		/* world_model_flags */ EF_FLAG2,
		/* view_model */ nullptr,
		/* icon */ "i_ctf2",
		/* use_name */  "Blue Flag",
		/* pickup_name */  "$item_blue_flag",
		/* pickup_name_definite */ "$item_blue_flag_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_NONE,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ 0,
		/* precaches */ "ctf/flagcap.wav"
	},
	{
		/* id */ IT_ITEM_FLASHLIGHT,
		/* classname */ "item_flashlight", 
		/* pickup */ Pickup_General,
		/* use */ Use_Flashlight,
		/* drop */ nullptr,
		/* weaponthink */ nullptr,
		/* pickup_sound */ "items/pkup.wav",
		/* world_model */ "models/items/flashlight/tris.md2",
		/* world_model_flags */ EF_ROTATE | EF_BOB,
		/* view_model */ nullptr,
		/* icon */ "p_torch",
		/* use_name */  "Flashlight",
		/* pickup_name */  "$item_flashlight",
		/* pickup_name_definite */ "$item_flashlight_def",
		/* quantity */ 0,
		/* ammo */ IT_NULL,
		/* chain */ IT_NULL,
		/* flags */ IF_STAY_COOP | IF_POWERUP_WHEEL | IF_POWERUP_ONOFF | IF_NOT_RANDOM,
		/* vwep_model */ nullptr,
		/* armor_info */ nullptr,
		/* tag */ POWERUP_FLASHLIGHT,
		/* precaches */ "items/flashlight_on.wav items/flashlight_off.wav",
		/* sort_id */ -1
	}
};
// clang-format on

void InitItems()
{
	// validate item integrity
	for (item_id_t i = IT_NULL; i < IT_TOTAL; i = static_cast<item_id_t>(i + 1))
		if (itemlist[i].id != i)
			gi.Com_ErrorFmt("Item {} has wrong enum ID {} (should be {})", itemlist[i].pickup_name, (int32_t) itemlist[i].id, (int32_t) i);

	// set up weapon chains
	for (item_id_t i = IT_NULL; i < IT_TOTAL; i = static_cast<item_id_t>(i + 1))
	{
		if (!itemlist[i].chain)
			continue;

		gitem_t *item = &itemlist[i];

		// already initialized
		if (item->chain_next)
			continue;

		gitem_t *chain_item = &itemlist[item->chain];

		if (!chain_item)
			gi.Com_ErrorFmt("Invalid item chain {} for {}", (int32_t) item->chain, item->pickup_name);

		// set up initial chain
		if (!chain_item->chain_next)
			chain_item->chain_next = chain_item;

		// if we're not the first in chain, add us now
		if (chain_item != item)
		{
			gitem_t *c;

			// end of chain is one whose chain_next points to chain_item
			for (c = chain_item; c->chain_next != chain_item; c = c->chain_next)
				continue;

			// splice us in
			item->chain_next = chain_item;
			c->chain_next = item;
		}
	}

	// set up ammo
	for (auto &it : itemlist)
	{
		if ((it.flags & IF_AMMO) && it.tag >= AMMO_BULLETS && it.tag < AMMO_MAX)
			ammolist[it.tag] = &it;
		else if ((it.flags & IF_POWERUP_WHEEL) && !(it.flags & IF_WEAPON) && it.tag >= POWERUP_SCREEN && it.tag < POWERUP_MAX)
			poweruplist[it.tag] = &it;
	}

	// in coop or DM with Weapons' Stay, remove drop ptr
	for (auto &it : itemlist)
	{
		if (coop->integer)
		{
			if (!P_UseCoopInstancedItems() && (it.flags & IF_STAY_COOP))
				it.drop = nullptr;
		}
		else if (deathmatch->integer)
		{
			if (g_dm_weapons_stay->integer && it.drop == Drop_Weapon)
				it.drop = nullptr;
		}
	}
}

/*
===============
SetItemNames

Called by worldspawn
===============
*/
void SetItemNames()
{
	for (item_id_t i = IT_NULL; i < IT_TOTAL; i = static_cast<item_id_t>(i + 1))
		gi.configstring(CS_ITEMS + i, itemlist[i].pickup_name);

	// [Paril-KEX] set ammo wheel indices first
	int32_t cs_index = 0;

	for (item_id_t i = IT_NULL; i < IT_TOTAL; i = static_cast<item_id_t>(i + 1))
	{
		if (!(itemlist[i].flags & IF_AMMO))
			continue;

		if (cs_index >= MAX_WHEEL_ITEMS)
			gi.Com_Error("out of wheel indices");

		gi.configstring(CS_WHEEL_AMMO + cs_index, G_Fmt("{}|{}", (int32_t) i, gi.imageindex(itemlist[i].icon)).data());
		itemlist[i].ammo_wheel_index = cs_index;
		cs_index++;
	}

	// set weapon wheel indices
	cs_index = 0;

	for (item_id_t i = IT_NULL; i < IT_TOTAL; i = static_cast<item_id_t>(i + 1))
	{
		if (!(itemlist[i].flags & IF_WEAPON))
			continue;

		if (cs_index >= MAX_WHEEL_ITEMS)
			gi.Com_Error("out of wheel indices");

		int32_t min_ammo = (itemlist[i].flags & IF_AMMO) ? 1 : itemlist[i].quantity;

		gi.configstring(CS_WHEEL_WEAPONS + cs_index, G_Fmt("{}|{}|{}|{}|{}|{}|{}|{}",
			(int32_t) i,
			gi.imageindex(itemlist[i].icon),
			itemlist[i].ammo ? GetItemByIndex(itemlist[i].ammo)->ammo_wheel_index : -1,
			min_ammo,
			(itemlist[i].flags & IF_POWERUP_WHEEL) ? 1 : 0,
			itemlist[i].sort_id,
			itemlist[i].quantity_warn,
			itemlist[i].drop != nullptr ? 1 : 0
		).data());
		itemlist[i].weapon_wheel_index = cs_index;
		cs_index++;
	}

	// set powerup wheel indices
	cs_index = 0;

	for (item_id_t i = IT_NULL; i < IT_TOTAL; i = static_cast<item_id_t>(i + 1))
	{
		if (!(itemlist[i].flags & IF_POWERUP_WHEEL) || (itemlist[i].flags & IF_WEAPON))
			continue;

		if (cs_index >= MAX_WHEEL_ITEMS)
			gi.Com_Error("out of wheel indices");

		gi.configstring(CS_WHEEL_POWERUPS + cs_index, G_Fmt("{}|{}|{}|{}|{}|{}",
			(int32_t) i,
			gi.imageindex(itemlist[i].icon),
			(itemlist[i].flags & IF_POWERUP_ONOFF) ? 1 : 0,
			itemlist[i].sort_id,
			itemlist[i].drop != nullptr ? 1 : 0,
			itemlist[i].ammo ? GetItemByIndex(itemlist[i].ammo)->ammo_wheel_index : -1
		).data());
		itemlist[i].powerup_wheel_index = cs_index;
		cs_index++;
	}
}
