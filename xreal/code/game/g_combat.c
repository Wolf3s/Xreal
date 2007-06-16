/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2006 Robert Beckebans <trebor_7@users.sourceforge.net>
Copyright (C) 2007 Jeremy Hughes <Encryption767@msn.com>

This file is part of XreaL source code.

XreaL source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

XreaL source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with XreaL source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// g_combat.c

#include "g_local.h"

/*
============
ScorePlum
============
*/
void ScorePlum(gentity_t * ent, vec3_t origin, int score)
{
	gentity_t      *plum;

	plum = G_TempEntity(origin, EV_SCOREPLUM);
	// only send this temp entity to a single client
	plum->r.svFlags |= SVF_SINGLECLIENT;
	plum->r.singleClient = ent->s.number;
	//
	plum->s.otherEntityNum = ent->s.number;
	plum->s.time = score;
}

/*
============
AddScore

Adds score to both the client and his team
============
*/
void AddScore(gentity_t * ent, vec3_t origin, int score)
{
	if(!ent->client)
	{
		return;
	}
	// no scoring during pre-match warmup
	if(level.warmupTime)
	{
		return;
	}
	if(score == 0)
	{
		return;
	}
	// show score plum
	ScorePlum(ent, origin, score);
	//
	ent->client->ps.persistant[PERS_SCORE] += score;
	if(g_gametype.integer == GT_TEAM)
		level.teamScores[ent->client->ps.persistant[PERS_TEAM]] += score;
	CalculateRanks();
}

/*
=================
TossClientItems

Toss the weapon and powerups for the killed player
=================
*/
void TossClientItems(gentity_t * self)
{
	gitem_t        *item;
	int             weapon;
	float           angle;
	int             i;
	gentity_t      *drop;

	// drop the weapon if not a gauntlet or machinegun
	weapon = self->s.weapon;

	if(Instagib.integer == 1)
	{
		if(g_gametype.integer != GT_TEAM)
		{
			angle = 45;
			for(i = 1; i < PW_NUM_POWERUPS; i++)
			{
				if(self->client->ps.powerups[i] > level.time)
				{
					item = BG_FindItemForPowerup(i);
					if(!item || item->giTag == PW_SPAWNPROT)
					{
						continue;
					}
					drop = Drop_Item(self, item, angle);
					// decide how many seconds it has left
					drop->count = (self->client->ps.powerups[i] - level.time) / 1000;
					if(drop->count < 1)
					{
						drop->count = 1;
					}
					angle += 45;
				}
			}
		}
		return;
	}
	// make a special check to see if they are changing to a new
	// weapon that isn't the mg or gauntlet.  Without this, a client
	// can pick up a weapon, be killed, and not drop the weapon because
	// their weapon change hasn't completed yet and they are still holding the MG.
	if(weapon == WP_GRAPPLING_HOOK)
	{
		if(self->client->ps.weaponstate == WEAPON_DROPPING)
		{
			weapon = self->client->pers.cmd.weapon;
		}
		if(!(self->client->ps.stats[STAT_WEAPONS] & (1 << weapon)))
		{
			weapon = WP_NONE;
		}
	}

	if(weapon >= WP_MACHINEGUN && weapon != WP_GRAPPLING_HOOK && weapon != WP_BFG && self->client->ps.ammo[weapon])
	{
		// find the item type for this weapon
		item = BG_FindItemForWeapon(weapon);

		// spawn the item
		Drop_Item(self, item, 0);
	}

	// drop all the powerups if not in teamplay
	angle = 45;
	for(i = 1; i < PW_NUM_POWERUPS; i++)
	{
		if(self->client->ps.powerups[i] > level.time)
		{
			item = BG_FindItemForPowerup(i);
			if(!item || item->giTag == PW_SPAWNPROT)
			{
				continue;
			}
			drop = Drop_Item(self, item, angle);
			// decide how many seconds it has left
			drop->count = (self->client->ps.powerups[i] - level.time) / 1000;
			if(drop->count < 1)
			{
				drop->count = 1;
			}
			angle += 45;
		}
	}

	return;

}

#ifdef MISSIONPACK

/*
=================
TossClientCubes
=================
*/
extern gentity_t *neutralObelisk;

void TossClientCubes(gentity_t * self)
{
	gitem_t        *item;
	gentity_t      *drop;
	vec3_t          velocity;
	vec3_t          angles;
	vec3_t          origin;

	self->client->ps.generic1 = 0;

	// this should never happen but we should never
	// get the server to crash due to skull being spawned in
	if(!G_EntitiesFree())
	{
		return;
	}

	if(self->client->sess.sessionTeam == TEAM_RED)
	{
		item = BG_FindItem("Red Cube");
	}
	else
	{
		item = BG_FindItem("Blue Cube");
	}

	angles[YAW] = (float)(level.time % 360);
	angles[PITCH] = 0;			// always forward
	angles[ROLL] = 0;

	AngleVectors(angles, velocity, NULL, NULL);
	VectorScale(velocity, 150, velocity);
	velocity[2] += 200 + crandom() * 50;

	if(neutralObelisk)
	{
		VectorCopy(neutralObelisk->s.pos.trBase, origin);
		origin[2] += 44;
	}
	else
	{
		VectorClear(origin);
	}

	drop = LaunchItem(item, origin, velocity, angles);

	drop->nextthink = level.time + g_cubeTimeout.integer * 1000;
	drop->think = G_FreeEntity;
	drop->spawnflags = self->client->sess.sessionTeam;
}


/*
=================
TossClientPersistantPowerups
=================
*/
void TossClientPersistantPowerups(gentity_t * ent)
{
	gentity_t      *powerup;

	if(!ent->client)
	{
		return;
	}

	if(!ent->client->persistantPowerup)
	{
		return;
	}

	powerup = ent->client->persistantPowerup;

	powerup->r.svFlags &= ~SVF_NOCLIENT;
	powerup->s.eFlags &= ~EF_NODRAW;
	powerup->r.contents = CONTENTS_TRIGGER;
	trap_LinkEntity(powerup);

	ent->client->ps.stats[STAT_PERSISTANT_POWERUP] = 0;
	ent->client->persistantPowerup = NULL;
}
#endif


/*
==================
LookAtKiller
==================
*/
void LookAtKiller(gentity_t * self, gentity_t * inflictor, gentity_t * attacker)
{
	vec3_t          dir;
	vec3_t          angles;

	if(attacker && attacker != self)
	{
		VectorSubtract(attacker->s.pos.trBase, self->s.pos.trBase, dir);
	}
	else if(inflictor && inflictor != self)
	{
		VectorSubtract(inflictor->s.pos.trBase, self->s.pos.trBase, dir);
	}
	else
	{
		self->client->ps.stats[STAT_DEAD_YAW] = self->s.angles[YAW];
		return;
	}

	self->client->ps.stats[STAT_DEAD_YAW] = vectoyaw(dir);

	angles[YAW] = vectoyaw(dir);
	angles[PITCH] = dir[2];
	angles[ROLL] = dir[1];
}

/*
==================
GibEntity
==================
*/
static void GibEntity(gentity_t * self, int killer, int mod)
{
	trace_t         tr;
	gentity_t      *ent;

//  int i;
	gentity_t      *other = &g_entities[killer];
	vec3_t          dir;

	ent = &g_entities[killer];

	trap_Trace(&tr, ent->s.origin, NULL, NULL, self->s.origin, killer, MASK_SHOT);
	//if this entity still has kamikaze


	VectorClear(dir);
	if(other->inuse)
	{
		if(other->client)
		{
			VectorSubtract(self->r.currentOrigin, other->r.currentOrigin, dir);
			VectorNormalize(dir);
		}
		else if(!VectorCompare(other->s.pos.trDelta, vec3_origin))
		{
			VectorNormalize2(other->s.pos.trDelta, dir);
		}
	}

	// TEST CODE
	if(self->client && self->client->lasthurt_location == 1)
	{
		if(!(self->client->ps.generic1 & GNF_ONFIRE) && !(self->s.generic1 & GNF_ONFIRE))
		{
			if(mod == MOD_ROCKET_SPLASH)
			{
				G_AddEvent(self, EV_GIB_PLAYERFIRE20HEAD, DirToByte(dir));
			}
			else if(mod == MOD_ROCKET)
			{
				G_AddEvent(self, EV_GIB_PLAYERFIRE20HEAD, DirToByte(dir));
			}
			else if(mod == MOD_BFG_SPLASH)
			{
				G_AddEvent(self, EV_GIB_PLAYERFIRE50HEAD, DirToByte(dir));
			}
			else if(mod == MOD_BFG)
			{
				G_AddEvent(self, EV_GIB_PLAYERFIRE80HEAD, DirToByte(dir));
			}
			else if(mod == MOD_GRENADE)
			{
				G_AddEvent(self, EV_GIB_PLAYERFIRE50HEAD, DirToByte(dir));
			}
			else if(mod == MOD_GRENADE_SPLASH)
			{
				G_AddEvent(self, EV_GIB_PLAYERFIRE20HEAD, DirToByte(dir));
			}
			else
			{
				G_AddEvent(self, EV_GIB_PLAYERHEAD, DirToByte(dir));
			}
		}
		else
		{
			G_AddEvent(self, EV_GIB_PLAYERHEAD, DirToByte(dir));
		}

		self->takedamage = qtrue;
		self->client->ps.stats[STAT_DEAD] = 2;
//      self->client->ps.stats[STAT_HEALTH] = GIB_HEALTH+15;
		self->s.eType = ET_DEADPLAYERHEAD;
//      self->die = body_die;
	}
	else if(self->client && self->client->lasthurt_location <= 0)
	{
		if(!(self->client->ps.generic1 & GNF_ONFIRE) && !(self->s.generic1 & GNF_ONFIRE))
		{
			if(mod == MOD_ROCKET_SPLASH)
			{
				G_AddEvent(self, EV_GIB_PLAYERFIRE20, DirToByte(dir));
			}
			else if(mod == MOD_ROCKET)
			{
				G_AddEvent(self, EV_GIB_PLAYERFIRE20, DirToByte(dir));
			}
			else if(mod == MOD_BFG_SPLASH)
			{
				G_AddEvent(self, EV_GIB_PLAYERFIRE50, DirToByte(dir));
			}
			else if(mod == MOD_BFG)
			{
				G_AddEvent(self, EV_GIB_PLAYERFIRE80, DirToByte(dir));
			}
			else if(mod == MOD_GRENADE)
			{
				G_AddEvent(self, EV_GIB_PLAYERFIRE50, DirToByte(dir));
			}
			else if(mod == MOD_GRENADE_SPLASH)
			{
				G_AddEvent(self, EV_GIB_PLAYERFIRE20, DirToByte(dir));
			}
			else
			{
				G_AddEvent(self, EV_GIB_PLAYER, DirToByte(dir));
			}
		}
		else
		{
			G_AddEvent(self, EV_GIB_PLAYER, DirToByte(dir));
		}

		self->takedamage = qfalse;
		self->s.eType = ET_DEADPLAYER;
	}
	else if(self->client && self->client->lasthurt_location == 2)
	{
		if(!(self->client->ps.generic1 & GNF_ONFIRE) && !(self->s.generic1 & GNF_ONFIRE))
		{
			if(mod == MOD_ROCKET_SPLASH)
			{
				G_AddEvent(self, EV_GIB_PLAYERFIRE20LEGS, DirToByte(dir));
			}
			else if(mod == MOD_ROCKET)
			{
				G_AddEvent(self, EV_GIB_PLAYERFIRE20LEGS, DirToByte(dir));
			}
			else if(mod == MOD_BFG_SPLASH)
			{
				G_AddEvent(self, EV_GIB_PLAYERFIRE50LEGS, DirToByte(dir));
			}
			else if(mod == MOD_BFG)
			{
				G_AddEvent(self, EV_GIB_PLAYERFIRE80LEGS, DirToByte(dir));
			}
			else if(mod == MOD_GRENADE)
			{
				G_AddEvent(self, EV_GIB_PLAYERFIRE50LEGS, DirToByte(dir));
			}
			else if(mod == MOD_GRENADE_SPLASH)
			{
				G_AddEvent(self, EV_GIB_PLAYERFIRE20LEGS, DirToByte(dir));
			}
			else
			{
				G_AddEvent(self, EV_GIB_PLAYERLEGS, DirToByte(dir));
			}
		}
		else
		{
			G_AddEvent(self, EV_GIB_PLAYERLEGS, DirToByte(dir));
		}

		self->takedamage = qfalse;
		self->client->ps.stats[STAT_DEAD] = 1;
		self->s.eType = ET_DEADPLAYERLEGS;
	}
	else
	{
		if(self->client && !(self->client->ps.generic1 & GNF_ONFIRE) && !(self->s.generic1 & GNF_ONFIRE))
		{
			if(mod == MOD_ROCKET_SPLASH)
			{
				G_AddEvent(self, EV_GIB_PLAYERFIRE20, DirToByte(dir));
			}
			else if(mod == MOD_ROCKET)
			{
				G_AddEvent(self, EV_GIB_PLAYERFIRE20, DirToByte(dir));
			}
			else if(mod == MOD_BFG_SPLASH)
			{
				G_AddEvent(self, EV_GIB_PLAYERFIRE50, DirToByte(dir));
			}
			else if(mod == MOD_BFG)
			{
				G_AddEvent(self, EV_GIB_PLAYERFIRE80, DirToByte(dir));
			}
			else if(mod == MOD_GRENADE)
			{
				G_AddEvent(self, EV_GIB_PLAYERFIRE50, DirToByte(dir));
			}
			else if(mod == MOD_GRENADE_SPLASH)
			{
				G_AddEvent(self, EV_GIB_PLAYERFIRE20, DirToByte(dir));
			}
			else
			{
				G_AddEvent(self, EV_GIB_PLAYER, DirToByte(dir));
			}
		}
		else
		{
			G_AddEvent(self, EV_GIB_PLAYER, DirToByte(dir));
		}

		self->takedamage = qfalse;

		self->s.eType = ET_DEADPLAYER;
	}


	if(self->client && self->client->ps.generic1 & GNF_ONFIRE)
	{
		self->s.generic1 |= GNF_ONFIRE;
	}
	self->r.contents = CONTENTS_CORPSE;


}

// ENC
static void GibEntityQ(gentity_t * self, int killer, int team, int mod)
{
	trace_t         tr;
	gentity_t      *ent;

//  int i;
	gentity_t      *other = &g_entities[killer];
	vec3_t          dir;

	ent = &g_entities[killer];

	trap_Trace(&tr, ent->s.origin, NULL, NULL, self->s.origin, killer, MASK_SHOT);
	//if this entity still has kamikaze


	VectorClear(dir);
	if(other->inuse)
	{
		if(other->client)
		{
			VectorSubtract(self->r.currentOrigin, other->r.currentOrigin, dir);
			VectorNormalize(dir);
		}
		else if(!VectorCompare(other->s.pos.trDelta, vec3_origin))
		{
			VectorNormalize2(other->s.pos.trDelta, dir);
		}
	}


	if(self->client && self->client->lasthurt_location == 1)
	{
		if(!(self->client->ps.generic1 & GNF_ONFIRE) && !(self->s.generic1 & GNF_ONFIRE))
		{
			if(mod == MOD_ROCKET_SPLASH)
			{
				G_AddEvent(self, EV_GIB_PLAYERQFIRE20HEAD, DirToByte(dir));
			}
			else if(mod == MOD_ROCKET)
			{
				G_AddEvent(self, EV_GIB_PLAYERQFIRE20HEAD, DirToByte(dir));
			}
			else if(mod == MOD_BFG_SPLASH)
			{
				G_AddEvent(self, EV_GIB_PLAYERQFIRE50HEAD, DirToByte(dir));
			}
			else if(mod == MOD_BFG)
			{
				G_AddEvent(self, EV_GIB_PLAYERQFIRE80HEAD, DirToByte(dir));
			}
			else if(mod == MOD_GRENADE)
			{
				G_AddEvent(self, EV_GIB_PLAYERQFIRE50HEAD, DirToByte(dir));
			}
			else if(mod == MOD_GRENADE_SPLASH)
			{
				G_AddEvent(self, EV_GIB_PLAYERQFIRE20HEAD, DirToByte(dir));
			}
			else
			{
				G_AddEvent(self, EV_GIB_PLAYERQHEAD, DirToByte(dir));
			}
		}
		else
		{
			G_AddEvent(self, EV_GIB_PLAYERQHEAD, DirToByte(dir));
		}

		self->takedamage = qfalse;
		self->s.eType = ET_DEADQPLAYERHEAD;
		self->client->ps.stats[STAT_DEAD] = 2;
//      self->client->ps.stats[STAT_HEALTH] = GIB_HEALTH+15;
//      self->die = body_die;
	}
	else if(self->client && self->client->lasthurt_location <= 0)
	{
		if(!(self->client->ps.generic1 & GNF_ONFIRE) && !(self->s.generic1 & GNF_ONFIRE))
		{
			if(mod == MOD_ROCKET_SPLASH)
			{
				G_AddEvent(self, EV_GIB_PLAYERQFIRE20, DirToByte(dir));
			}
			else if(mod == MOD_ROCKET)
			{
				G_AddEvent(self, EV_GIB_PLAYERQFIRE20, DirToByte(dir));
			}
			else if(mod == MOD_BFG_SPLASH)
			{
				G_AddEvent(self, EV_GIB_PLAYERQFIRE50, DirToByte(dir));
			}
			else if(mod == MOD_BFG)
			{
				G_AddEvent(self, EV_GIB_PLAYERQFIRE80, DirToByte(dir));
			}
			else if(mod == MOD_GRENADE)
			{
				G_AddEvent(self, EV_GIB_PLAYERQFIRE50, DirToByte(dir));
			}
			else if(mod == MOD_GRENADE_SPLASH)
			{
				G_AddEvent(self, EV_GIB_PLAYERQFIRE20, DirToByte(dir));
			}
			else
			{
				G_AddEvent(self, EV_GIB_PLAYERQ, DirToByte(dir));
			}
		}
		else
		{
			G_AddEvent(self, EV_GIB_PLAYERQ, DirToByte(dir));
		}

		self->takedamage = qfalse;
		self->s.eType = ET_DEADQPLAYER;

	}
	else if(self->client && self->client->lasthurt_location == 2)
	{
		if(!(self->client->ps.generic1 & GNF_ONFIRE) && !(self->s.generic1 & GNF_ONFIRE))
		{
			if(mod == MOD_ROCKET_SPLASH)
			{
				G_AddEvent(self, EV_GIB_PLAYERQFIRE20LEGS, DirToByte(dir));
			}
			else if(mod == MOD_ROCKET)
			{
				G_AddEvent(self, EV_GIB_PLAYERQFIRE20LEGS, DirToByte(dir));
			}
			else if(mod == MOD_BFG_SPLASH)
			{
				G_AddEvent(self, EV_GIB_PLAYERQFIRE50LEGS, DirToByte(dir));
			}
			else if(mod == MOD_BFG)
			{
				G_AddEvent(self, EV_GIB_PLAYERQFIRE80LEGS, DirToByte(dir));
			}
			else if(mod == MOD_GRENADE)
			{
				G_AddEvent(self, EV_GIB_PLAYERQFIRE50LEGS, DirToByte(dir));
			}
			else if(mod == MOD_GRENADE_SPLASH)
			{
				G_AddEvent(self, EV_GIB_PLAYERQFIRE20LEGS, DirToByte(dir));
			}
			else
			{
				G_AddEvent(self, EV_GIB_PLAYERQLEGS, DirToByte(dir));
			}
		}
		else
		{
			G_AddEvent(self, EV_GIB_PLAYERQLEGS, DirToByte(dir));
		}

		self->takedamage = qfalse;
		self->s.eType = ET_DEADQPLAYERLEGS;
		self->client->ps.stats[STAT_DEAD] = 1;
	}
	else
	{
		if(self->client && !(self->client->ps.generic1 & GNF_ONFIRE) && !(self->s.generic1 & GNF_ONFIRE))
		{
			if(mod == MOD_ROCKET_SPLASH)
			{
				G_AddEvent(self, EV_GIB_PLAYERQFIRE20, DirToByte(dir));
			}
			else if(mod == MOD_ROCKET)
			{
				G_AddEvent(self, EV_GIB_PLAYERQFIRE20, DirToByte(dir));
			}
			else if(mod == MOD_BFG_SPLASH)
			{
				G_AddEvent(self, EV_GIB_PLAYERQFIRE50, DirToByte(dir));
			}
			else if(mod == MOD_BFG)
			{
				G_AddEvent(self, EV_GIB_PLAYERQFIRE80, DirToByte(dir));
			}
			else if(mod == MOD_GRENADE)
			{
				G_AddEvent(self, EV_GIB_PLAYERQFIRE50, DirToByte(dir));
			}
			else if(mod == MOD_GRENADE_SPLASH)
			{
				G_AddEvent(self, EV_GIB_PLAYERQFIRE20, DirToByte(dir));
			}
			else
			{
				G_AddEvent(self, EV_GIB_PLAYERQ, DirToByte(dir));
			}
		}
		else
		{
			G_AddEvent(self, EV_GIB_PLAYERQ, DirToByte(dir));
		}

		self->takedamage = qfalse;
		self->s.eType = ET_DEADQPLAYER;
	}

	self->r.contents = CONTENTS_CORPSE;
	self->s.otherEntityNum2 = team;


}

// ENC

/*
==================
body_die
==================
*/
void body_die(gentity_t * self, gentity_t * inflictor, gentity_t * attacker, int damage, int meansOfDeath)
{


	if(!g_blood.integer)
	{
		self->health = GIB_HEALTH + 1;
		return;
	}

	if(self->health > GIB_HEALTH)
	{
		return;
	}


	if(attacker->client && attacker->client->ps.powerups[PW_QUAD] >= 1 && meansOfDeath != MOD_UNKNOWN && meansOfDeath != MOD_WATER
	   && meansOfDeath != MOD_SLIME && meansOfDeath != MOD_LAVA && meansOfDeath != MOD_CRUSH && meansOfDeath != MOD_TELEFRAG
	   && meansOfDeath != MOD_FALLING && meansOfDeath != MOD_SUICIDE && meansOfDeath != MOD_TARGET_LASER &&
	   meansOfDeath != MOD_TRIGGER_HURT)
	{
		GibEntityQ(self, inflictor->s.clientNum, attacker->client->sess.sessionTeam, meansOfDeath);
	}
	else
	{
		GibEntity(self, inflictor->s.clientNum, meansOfDeath);
	}
}


// these are just for logging, the client prints its own messages
char           *modNames[] = {
	"MOD_UNKNOWN",
	"MOD_SHOTGUN",
	"MOD_GAUNTLET",
	"MOD_MACHINEGUN",
	"MOD_GRENADE",
	"MOD_GRENADE_SPLASH",
	"MOD_ROCKET",
	"MOD_ROCKET_SPLASH",
	"MOD_PLASMA",
	"MOD_PLASMA_SPLASH",
	"MOD_RAILGUN",
	"MOD_LIGHTNING",
	"MOD_BFG",
	"MOD_BFG_SPLASH",
	"MOD_WATER",
	"MOD_SLIME",
	"MOD_LAVA",
	"MOD_CRUSH",
	"MOD_TELEFRAG",
	"MOD_FALLING",
	"MOD_SUICIDE",
	"MOD_TARGET_LASER",
	"MOD_TRIGGER_HURT",
	"MOD_FLAMETHROWER",
#ifdef MISSIONPACK
	"MOD_NAIL",
	"MOD_CHAINGUN",
	"MOD_PROXIMITY_MINE",
	"MOD_KAMIKAZE",
	"MOD_JUICED",
#endif
	"MOD_GRAPPLE"
};

#ifdef MISSIONPACK
/*
==================
Kamikaze_DeathActivate
==================
*/
void Kamikaze_DeathActivate(gentity_t * ent)
{
	G_StartKamikaze(ent);
	G_FreeEntity(ent);
}

/*
==================
Kamikaze_DeathTimer
==================
*/
void Kamikaze_DeathTimer(gentity_t * self)
{
	gentity_t      *ent;

	ent = G_Spawn();
	ent->classname = "kamikaze timer";
	VectorCopy(self->s.pos.trBase, ent->s.pos.trBase);
	ent->r.svFlags |= SVF_NOCLIENT;
	ent->think = Kamikaze_DeathActivate;
	ent->nextthink = level.time + 5 * 1000;

	ent->activator = self;
}

#endif

/*
==================
CheckAlmostCapture
==================
*/
void CheckAlmostCapture(gentity_t * self, gentity_t * attacker)
{
	gentity_t      *ent;
	vec3_t          dir;
	char           *classname;

	// if this player was carrying a flag
	if(self->client->ps.powerups[PW_REDFLAG] || self->client->ps.powerups[PW_BLUEFLAG])
	{
		// get the goal flag this player should have been going for
		if(g_gametype.integer == GT_CTF)
		{
			if(self->client->sess.sessionTeam == TEAM_BLUE)
			{
				classname = "team_CTF_blueflag";
			}
			else
			{
				classname = "team_CTF_redflag";
			}
		}
		else
		{
			if(self->client->sess.sessionTeam == TEAM_BLUE)
			{
				classname = "team_CTF_redflag";
			}
			else
			{
				classname = "team_CTF_blueflag";
			}
		}
		ent = NULL;
		do
		{
			ent = G_Find(ent, FOFS(classname), classname);
		} while(ent && (ent->flags & FL_DROPPED_ITEM));
		// if we found the destination flag and it's not picked up
		if(ent && !(ent->r.svFlags & SVF_NOCLIENT))
		{
			// if the player was *very* close
			VectorSubtract(self->client->ps.origin, ent->s.origin, dir);
			if(VectorLength(dir) < 200)
			{
				self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				if(attacker->client)
				{
					attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				}
			}
		}
	}
}

/*
==================
CheckAlmostScored
==================
*/
void CheckAlmostScored(gentity_t * self, gentity_t * attacker)
{
	gentity_t      *ent;
	vec3_t          dir;
	char           *classname;

	// if the player was carrying cubes
	if(self->client->ps.generic1)
	{
		if(self->client->sess.sessionTeam == TEAM_BLUE)
		{
			classname = "team_redobelisk";
		}
		else
		{
			classname = "team_blueobelisk";
		}
		ent = G_Find(NULL, FOFS(classname), classname);
		// if we found the destination obelisk
		if(ent)
		{
			// if the player was *very* close
			VectorSubtract(self->client->ps.origin, ent->s.origin, dir);
			if(VectorLength(dir) < 200)
			{
				self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				if(attacker->client)
				{
					attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
				}
			}
		}
	}
}

// TEST CODE

/* 
============
G_LocationDamage
============
*/
void G_LocationDamage(vec3_t point, gentity_t * targ, gentity_t * attacker, int take)
{
	vec3_t          bulletPath;
	vec3_t          bulletAngle;

	int             clientHeight;
	int             clientFeetZ;
	int             clientRotation;
	int             bulletHeight;
	int             bulletRotation;	// Degrees rotation around client.

	// used to check Back of head vs. Face
	int             impactRotation;


	// First things first.  If we're not damaging them, why are we here? 
	if(!take)
	{
		return;
	}

	// Point[2] is the REAL world Z. We want Z relative to the clients feet

	// Where the feet are at [real Z]
	clientFeetZ = targ->r.currentOrigin[2] + targ->r.mins[2];
	// How tall the client is [Relative Z]
	clientHeight = targ->r.maxs[2] - targ->r.mins[2];
	// Where the bullet struck [Relative Z]
	bulletHeight = point[2] - clientFeetZ;

	// Get a vector aiming from the client to the bullet hit 
	VectorSubtract(targ->r.currentOrigin, point, bulletPath);
	// Convert it into PITCH, ROLL, YAW
	vectoangles(bulletPath, bulletAngle);

	clientRotation = targ->client->ps.viewangles[YAW];
	bulletRotation = bulletAngle[YAW];

	impactRotation = abs(clientRotation - bulletRotation);

	impactRotation += 45;		// just to make it easier to work with
	impactRotation = impactRotation % 360;	// Keep it in the 0-359 range

	if(impactRotation < 90)
	{
		targ->client->lasthurt_location = LOCATION_BACK;
	}
	else if(impactRotation < 180)
	{
		targ->client->lasthurt_location = LOCATION_RIGHT;
	}
	else if(impactRotation < 270)
	{
		targ->client->lasthurt_location = LOCATION_FRONT;
	}
	else if(impactRotation < 360)
	{
		targ->client->lasthurt_location = LOCATION_LEFT;
	}
	else
	{
		targ->client->lasthurt_location = LOCATION_NONE;
	}
	// The upper body never changes height, just distance from the feet
	if(bulletHeight > clientHeight - 2)
	{
		targ->client->lasthurt_location = 1;
	}
	else if(bulletHeight > clientHeight - 8)
	{
		targ->client->lasthurt_location = 1;
	}
	else if(bulletHeight > clientHeight - 10)
	{
		targ->client->lasthurt_location = 0;
	}
	else if(bulletHeight > clientHeight - 16)
	{
		targ->client->lasthurt_location = 0;
	}
	else if(bulletHeight > clientHeight - 26)
	{
		targ->client->lasthurt_location = 0;
	}
	else if(bulletHeight > clientHeight - 29)
	{
		targ->client->lasthurt_location = 2;
	}
	else if(bulletHeight < 4)
	{
		targ->client->lasthurt_location = 2;
	}
	else
	{
		// The leg is the only thing that changes size when you duck,
		// so we check for every other parts RELATIVE location, and
		// whats left over must be the leg. 
		targ->client->lasthurt_location = 2;
	}


	// Check the location ignoring the rotation info
	switch (targ->client->lasthurt_location & ~(LOCATION_BACK | LOCATION_LEFT | LOCATION_RIGHT | LOCATION_FRONT))
	{
		case LOCATION_HEAD:
			take = 1;
			break;
		case LOCATION_FACE:
			if(targ->client->lasthurt_location & LOCATION_FRONT)
			{
				take = 1;		// Faceshots REALLY suck
			}
			else
			{
				take = 1;
			}
			break;
		case LOCATION_SHOULDER:
			if(targ->client->lasthurt_location & (LOCATION_FRONT | LOCATION_BACK))
			{
				take = 2;		// Throat or nape of neck
			}
			else
			{
				take = 2;		// Shoulders
			}
			break;
		case LOCATION_CHEST:
			if(targ->client->lasthurt_location & (LOCATION_FRONT | LOCATION_BACK))
			{
				take = 2;		// Belly or back
			}
			else
			{
				take = 2;		// Arms
			}
			break;
		case LOCATION_STOMACH:
			take = 2;
			break;
		case LOCATION_GROIN:
			if(targ->client->lasthurt_location & LOCATION_FRONT)
			{
				take = 3;		// Groin shot
			}
			break;
		case LOCATION_LEG:
			take = 3;
			break;
		case LOCATION_FOOT:
			take = 3;
			break;

	}

}


/*
==================
player_die
==================
*/
void player_die(gentity_t * self, gentity_t * inflictor, gentity_t * attacker, int damage, int meansOfDeath)
{
	gentity_t      *ent;
	int             anim;
	int             contents;
	int             killer;
	int             i;
	char           *killerName, *obit;

	if(self->client->ps.pm_type == PM_DEAD)
	{
		return;
	}

	if(level.intermissiontime)
	{
		return;
	}
//unlagged - backward reconciliation #2
	// make sure the body shows up in the client's current position
	//G_UnTimeShiftClient( self );
//unlagged - backward reconciliation #2
	// check for an almost capture
	CheckAlmostCapture(self, attacker);
	// check for a player that almost brought in cubes
	CheckAlmostScored(self, attacker);

	if(self->client && self->client->hook)
	{
		Weapon_HookFree(self->client->hook);
	}
#ifdef MISSIONPACK
	if((self->client->ps.eFlags & EF_TICKING) && self->activator)
	{
		self->client->ps.eFlags &= ~EF_TICKING;
		self->activator->think = G_FreeEntity;
		self->activator->nextthink = level.time;
	}
#endif
	self->client->ps.pm_type = PM_DEAD;

	if(attacker)
	{
		killer = attacker->s.number;
		if(attacker->client)
		{
			killerName = attacker->client->pers.netname;
		}
		else
		{
			killerName = "<non-client>";
		}
	}
	else
	{
		killer = ENTITYNUM_WORLD;
		killerName = "<world>";
	}

	if(killer < 0 || killer >= MAX_CLIENTS)
	{
		killer = ENTITYNUM_WORLD;
		killerName = "<world>";
	}

	if(meansOfDeath < 0 || meansOfDeath >= sizeof(modNames) / sizeof(modNames[0]))
	{
		obit = "<bad obituary>";
	}
	else
	{
		obit = modNames[meansOfDeath];
	}

	G_LogPrintf("Kill: %i %i %i: %s killed %s by %s\n",
				killer, self->s.number, meansOfDeath, killerName, self->client->pers.netname, obit);

	// broadcast the death event to everyone
	ent = G_TempEntity(self->r.currentOrigin, EV_OBITUARY);
	ent->s.eventParm = meansOfDeath;
	ent->s.otherEntityNum = self->s.number;
	ent->s.otherEntityNum2 = killer;
	ent->r.svFlags = SVF_BROADCAST;	// send to everyone

	self->enemy = attacker;

	self->client->ps.persistant[PERS_KILLED]++;

	if(attacker && attacker->client)
	{
		attacker->client->lastkilled_client = self->s.number;

		if(attacker == self || OnSameTeam(self, attacker))
		{
			AddScore(attacker, self->r.currentOrigin, -1);
		}
		else
		{
			AddScore(attacker, self->r.currentOrigin, 1);

			if(meansOfDeath == MOD_GAUNTLET)
			{

				// play humiliation on player
				attacker->client->ps.persistant[PERS_GAUNTLET_FRAG_COUNT]++;

				// add the sprite over the player's head
				attacker->client->ps.eFlags &=
					~(EF_AWARD_RLRGCOMBO | EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST |
					  EF_AWARD_DEFEND | EF_AWARD_CAP);
				attacker->client->ps.eFlags |= EF_AWARD_GAUNTLET;
				attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;

				// also play humiliation on target
				self->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_GAUNTLETREWARD;
			}


			// check for two kills in a short amount of time
			// if this is close enough to the last kill, give a reward sound
			if(level.time - attacker->client->lastKillTime < CARNAGE_REWARD_TIME)
			{
				// play excellent on player
				attacker->client->ps.persistant[PERS_EXCELLENT_COUNT]++;

				// add the sprite over the player's head
				attacker->client->ps.eFlags &=
					~(EF_AWARD_RLRGCOMBO | EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST |
					  EF_AWARD_DEFEND | EF_AWARD_CAP);
				attacker->client->ps.eFlags |= EF_AWARD_EXCELLENT;
				attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;
			}
			attacker->client->lastKillTime = level.time;

		}
	}
	else
	{
		AddScore(self, self->r.currentOrigin, -1);
	}

	// Add team bonuses
	Team_FragBonuses(self, inflictor, attacker);

	// if client is in a nodrop area, don't drop anything (but return CTF flags!)
	contents = trap_PointContents(self->r.currentOrigin, -1);
	if(!(contents & CONTENTS_NODROP))
	{
		TossClientItems(self);
	}
	else
	{
		if(self->client->ps.powerups[PW_REDFLAG])
		{						// only happens in standard CTF
			Team_ReturnFlag(NULL, TEAM_RED);
		}
		if(self->client->ps.powerups[PW_BLUEFLAG])
		{						// only happens in standard CTF
			Team_ReturnFlag(NULL, TEAM_BLUE);
		}
	}
#ifdef MISSIONPACK
	TossClientPersistantPowerups(self);
	if(g_gametype.integer == GT_HARVESTER)
	{
		TossClientCubes(self);
	}
#endif

	Cmd_Score_f(self);			// show scores
	// send updated scores to any clients that are following this one,
	// or they would get stale scoreboards
	for(i = 0; i < level.maxclients; i++)
	{
		gclient_t      *client;

		client = &level.clients[i];
		if(client->pers.connected != CON_CONNECTED)
		{
			continue;
		}
		if(client->sess.sessionTeam != TEAM_SPECTATOR)
		{
			continue;
		}
		if(client->sess.spectatorClient == self->s.number)
		{
			Cmd_Score_f(g_entities + i);
		}
	}

	self->takedamage = qtrue;	// can still be gibbed
	if(Instagib.integer == 0)
	{
		self->s.weapon = WP_NONE;
	}
	self->s.powerups = 0;
	self->r.contents = CONTENTS_CORPSE;

	self->s.angles[0] = 0;
	self->s.angles[2] = 0;
	LookAtKiller(self, inflictor, attacker);

	VectorCopy(self->s.angles, self->client->ps.viewangles);

	self->s.loopSound = 0;

	self->r.maxs[2] = -8;

	// don't allow respawn until the death anim is done
	// g_forcerespawn may force spawning at some later time
	if(level.intermissionQueued == 0)
	{
		if(SpeedSpawn.integer == 1)
		{
			self->client->respawnTime = level.time + 50;
		}
		else
		{
			self->client->respawnTime = -1;
		}
	}
	else
	{
		self->client->respawnTime = level.time + 7000;
	}

	// remove powerups
	memset(self->client->ps.powerups, 0, sizeof(self->client->ps.powerups));

	if((meansOfDeath == MOD_ROCKET && !(contents & CONTENTS_NODROP) && g_blood.integer))
	{
		self->health = GIB_HEALTH - 1;
	}
	else if((meansOfDeath == MOD_GRENADE && !(contents & CONTENTS_NODROP) && g_blood.integer))
	{
		self->health = GIB_HEALTH - 1;
	}

	// never gib in a nodrop
	if((self->health <= GIB_HEALTH && !(contents & CONTENTS_NODROP) && g_blood.integer))
	{
		// gib death
		static int      i;

		switch (i)
		{
			case 0:

				anim = BOTH_DEATH1;

				break;
			case 1:

				anim = BOTH_DEATH2;

				break;
			case 2:
			default:

				anim = BOTH_DEATH3;

				break;
		}

		self->client->ps.stats[STAT_ARMOR] = 0;

		self->client->ps.legsAnim = ((self->client->ps.legsAnim & ANIM_TOGGLEBIT) ^ ANIM_TOGGLEBIT) | anim;
		self->client->ps.torsoAnim = ((self->client->ps.torsoAnim & ANIM_TOGGLEBIT) ^ ANIM_TOGGLEBIT) | anim;

		G_AddEvent(self, EV_DEATH1 + i, killer);


		if(attacker->client)
		{
			if(attacker->client->ps.powerups[PW_QUAD] >= 1)
			{
				GibEntityQ(self, killer, attacker->client->sess.sessionTeam, meansOfDeath);
			}
			else
			{
				GibEntity(self, killer, meansOfDeath);
			}
		}
		else
		{
			GibEntity(self, killer, meansOfDeath);
		}

		i = (i + 1) % 3;
	}
	else
	{
		// normal death
		static int      i;

		switch (i)
		{
			case 0:

				anim = BOTH_DEATH1;

				break;
			case 1:

				anim = BOTH_DEATH2;

				break;
			case 2:
			default:

				anim = BOTH_DEATH3;

				break;
		}

		// for the no-blood option, we need to prevent the health
		// from going to gib level
		if(self->health <= GIB_HEALTH)
		{
			self->health = GIB_HEALTH + 1;
		}

		self->client->ps.stats[STAT_ARMOR] = 0;

		self->client->ps.legsAnim = ((self->client->ps.legsAnim & ANIM_TOGGLEBIT) ^ ANIM_TOGGLEBIT) | anim;
		self->client->ps.torsoAnim = ((self->client->ps.torsoAnim & ANIM_TOGGLEBIT) ^ ANIM_TOGGLEBIT) | anim;

		G_AddEvent(self, EV_DEATH1 + i, killer);




		// the body can still be gibbed
		self->die = body_die;

		// globally cycle through the different death animations
		i = (i + 1) % 3;

	}

	trap_LinkEntity(self);

}


/*
================
CheckArmor
================
*/
int CheckArmor(gentity_t * ent, int damage, int dflags, gentity_t * attacker, int mod)
{
	gclient_t      *client;
	int             save, damage3, damage4, final;

	if(!damage)
		return 0;

	damage3 = damage;
	damage4 = damage;

	client = ent->client;

	if(!client)
		return 0;

	if(dflags & DAMAGE_NO_ARMOR)
		return 0;

	switch (mod)
	{
		case MOD_WATER:
		case MOD_SLIME:
		case MOD_LAVA:
		case MOD_CRUSH:
		case MOD_TELEFRAG:
		case MOD_FALLING:
		case MOD_SUICIDE:
		case MOD_TARGET_LASER:
		case MOD_TRIGGER_HURT:
			save = 0;
			return save;
			break;
		case MOD_MACHINEGUN:
			//  more = damage * 1.6f;
			//  damage2 = damage - more;
			//  final = damage + damage2;
			save = damage * 1.6;	//ceil( final * ARMOR_PROTECTION );
			break;
		case MOD_SHOTGUN:
			//  more = damage * 1.3f;
			//  damage2 = damage - more;
			//  final = damage + damage2;
			save = damage * 1.3;	//ceil( final * ARMOR_PROTECTION );
			break;
		case MOD_RAILGUN:
			//  more = damage * 1.7f;
			//  damage2 = damage - more;
			//  final = damage + damage2;
			save = damage * 1.6;	//ceil( final * ARMOR_PROTECTION );
			break;
		case MOD_PLASMA:
		case MOD_GAUNTLET:
		case MOD_LIGHTNING:
		case MOD_ROCKET:
			//  more = damage * 1.1f;
			//  damage2 = damage - more;
			//  final = damage + damage2;
			save = damage;		//ceil( final * ARMOR_PROTECTION );
			break;
		case MOD_GRENADE:
			final = damage3 / 1.2f;
			save = ceil(final * ARMOR_PROTECTION);
			break;
		case MOD_BFG_SPLASH:
		case MOD_BFG:
			save = 0;
			return save;
			break;
		case MOD_ROCKET_SPLASH:
		case MOD_GRENADE_SPLASH:
		case MOD_PLASMA_SPLASH:
			final = damage3 / 1.8f;
			save = ceil(final * ARMOR_PROTECTION);
			break;
		case MOD_FLAMETHROWER:
			final = damage3 / 2.5f;
			save = ceil(final * ARMOR_PROTECTION);
			break;
	}
	if(save >= client->ps.stats[STAT_ARMOR])
	{
		save = client->ps.stats[STAT_ARMOR];
	}
	if(!save)
	{
		return 0;
	}

	G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/armorbullet.wav"));

	client->ps.stats[STAT_ARMOR] -= save;

	if(client->ps.stats[STAT_ARMOR] <= 0)
	{
		client->ps.stats[STAT_ARMOR] = 0;
	}

	return save;
}

/*
================
RaySphereIntersections
================
*/
int RaySphereIntersections(vec3_t origin, float radius, vec3_t point, vec3_t dir, vec3_t intersections[2])
{
	float           b, c, d, t;

	//  | origin - (point + t * dir) | = radius
	//  a = dir[0]^2 + dir[1]^2 + dir[2]^2;
	//  b = 2 * (dir[0] * (point[0] - origin[0]) + dir[1] * (point[1] - origin[1]) + dir[2] * (point[2] - origin[2]));
	//  c = (point[0] - origin[0])^2 + (point[1] - origin[1])^2 + (point[2] - origin[2])^2 - radius^2;

	// normalize dir so a = 1
	VectorNormalize(dir);
	b = 2 * (dir[0] * (point[0] - origin[0]) + dir[1] * (point[1] - origin[1]) + dir[2] * (point[2] - origin[2]));
	c = (point[0] - origin[0]) * (point[0] - origin[0]) +
		(point[1] - origin[1]) * (point[1] - origin[1]) + (point[2] - origin[2]) * (point[2] - origin[2]) - radius * radius;

	d = b * b - 4 * c;
	if(d > 0)
	{
		t = (-b + sqrt(d)) / 2;
		VectorMA(point, t, dir, intersections[0]);
		t = (-b - sqrt(d)) / 2;
		VectorMA(point, t, dir, intersections[1]);
		return 2;
	}
	else if(d == 0)
	{
		t = (-b) / 2;
		VectorMA(point, t, dir, intersections[0]);
		return 1;
	}
	return 0;
}

#ifdef MISSIONPACK
/*
================
G_InvulnerabilityEffect
================
*/
int G_InvulnerabilityEffect(gentity_t * targ, vec3_t dir, vec3_t point, vec3_t impactpoint, vec3_t bouncedir)
{
	gentity_t      *impact;
	vec3_t          intersections[2], vec;
	int             n;

	if(!targ->client)
	{
		return qfalse;
	}
	VectorCopy(dir, vec);
	VectorInverse(vec);
	// sphere model radius = 42 units
	n = RaySphereIntersections(targ->client->ps.origin, 42, point, vec, intersections);
	if(n > 0)
	{
		impact = G_TempEntity(targ->client->ps.origin, EV_INVUL_IMPACT);
		VectorSubtract(intersections[0], targ->client->ps.origin, vec);
		vectoangles(vec, impact->s.angles);
		impact->s.angles[0] += 90;
		if(impact->s.angles[0] > 360)
			impact->s.angles[0] -= 360;
		if(impactpoint)
		{
			VectorCopy(intersections[0], impactpoint);
		}
		if(bouncedir)
		{
			VectorCopy(vec, bouncedir);
			VectorNormalize(bouncedir);
		}
		return qtrue;
	}
	else
	{
		return qfalse;
	}
}
#endif
/*
============
T_Damage

targ		entity that is being damaged
inflictor	entity that is causing the damage
attacker	entity that caused the inflictor to damage targ
	example: targ=monster, inflictor=rocket, attacker=player

dir			direction of the attack for knockback
point		point at which the damage is being inflicted, used for headshots
damage		amount of damage being inflicted
knockback	force to be applied against targ as a result of the damage

inflictor, attacker, dir, and point can be NULL for environmental effects

dflags		these flags are used to control how T_Damage works
	DAMAGE_RADIUS			damage was indirect (from a nearby explosion)
	DAMAGE_NO_ARMOR			armor does not protect from this damage
	DAMAGE_NO_KNOCKBACK		do not affect velocity, just view angles
	DAMAGE_NO_PROTECTION	kills godmode, armor, everything
============
*/

void G_Damage(gentity_t * targ, gentity_t * inflictor, gentity_t * attacker,
			  vec3_t dir, vec3_t point, int damage, int dflags, int mod)
{
	gclient_t      *client;
	int             take;
	int             save;
	int             asave;

//  int damagegt;
//  int damagert;
	int             knockback;
	int             count;
	int             max;

//  trace_t     trace;
#ifdef MISSIONPACK
	vec3_t          bouncedir, impactpoint;
#endif


	if(!targ->takedamage)
	{
		return;
	}

	// the intermission has allready been qualified for, so don't
	// allow any extra scoring
	if(level.intermissionQueued)
	{
		return;
	}
#ifdef MISSIONPACK
	if(targ->client && mod != MOD_JUICED)
	{
		if(targ->client->invulnerabilityTime > level.time)
		{
			if(dir && point)
			{
				G_InvulnerabilityEffect(targ, dir, point, impactpoint, bouncedir);
			}
			return;
		}
	}
#endif
	if(!inflictor)
	{
		inflictor = &g_entities[ENTITYNUM_WORLD];
	}
	if(!attacker)
	{
		attacker = &g_entities[ENTITYNUM_WORLD];
	}

	// shootable doors / buttons don't actually have any health
	if(targ->s.eType == ET_MOVER)
	{
		if(targ->use && targ->moverState == MOVER_POS1)
		{
			targ->use(targ, inflictor, attacker);
		}
		return;
	}
#ifdef MISSIONPACK
	if(g_gametype.integer == GT_OBELISK && CheckObeliskAttack(targ, attacker))
	{
		return;
	}
#endif
	// reduce damage by the attacker's handicap value
	// unless they are rocket jumping
	if(attacker->client && attacker != targ)
	{
		max = attacker->client->ps.stats[STAT_MAX_HEALTH];
#ifdef MISSIONPACK
		if(bg_itemlist[attacker->client->ps.stats[STAT_PERSISTANT_POWERUP]].giTag == PW_GUARD)
		{
			max /= 2;
		}
#endif
		damage = damage * max / 100;
	}

	client = targ->client;


	if(client)
	{
		if(client->noclip)
		{
			return;
		}
	}

	if(!dir)
	{
		dflags |= DAMAGE_NO_KNOCKBACK;
	}
	else
	{
		VectorNormalize(dir);
	}

	if(Instagib.integer == 1)
	{
		if(mod == MOD_ROCKET_SPLASH || mod == MOD_ROCKET)
		{
			knockback = damage * 2;
		}
		else
		{
			if(InstaWeapon.integer == 2)
			{
				knockback = 60;
			}
			else
			{
				knockback = 0;
			}
		}
		if(knockback > 200)
		{
			knockback = 200;
		}
		if(attacker->client && attacker->client->ps.powerups[PW_QUAD] >= 1)
		{
			knockback = damage / 4;
		}
	}
	else
	{
		if(attacker->client && attacker->client->ps.powerups[PW_QUAD] >= 1)
		{
			knockback = damage / 4;
		}
		else
		{
			knockback = damage;
		}
		if(knockback > 200)
		{
			knockback = 200;
		}
	}

	if(mod == MOD_RAILGUN && targ->client->ps.stats[STAT_HEALTH] <= 0)
	{
		knockback = 0;
	}

	if(targ->flags & FL_NO_KNOCKBACK)
	{
		knockback = 0;
	}
	if(dflags & DAMAGE_NO_KNOCKBACK)
	{
		knockback = 0;
	}
	if(client && client->ps.powerups[PW_SPAWNPROT])
	{
		knockback = 0;
	}

	if(mod == MOD_MACHINEGUN)
	{
		knockback += 4;
	}
	if(mod == MOD_PLASMA)
	{
		knockback += 5;
	}
	if(mod == MOD_GRENADE)
	{
		knockback += 5;
	}
	if(mod == MOD_PLASMA_SPLASH)
	{
		knockback += 7;
	}
	if(mod == MOD_GRENADE_SPLASH)
	{
		knockback += 65;
	}
	if(mod == MOD_FLAMETHROWER)
	{
		knockback = 0;
	}

	if(mod == MOD_GRENADE && targ->client->ps.stats[STAT_HEALTH] <= 0)
	{
		knockback = 5;
	}

	// figure momentum add, even if the damage won't be taken
	if(knockback && targ->client)
	{
		vec3_t          kvel;
		float           mass;

		mass = 200;

		VectorScale(dir, g_knockback.value * (float)knockback / mass, kvel);
		VectorAdd(targ->client->ps.velocity, kvel, targ->client->ps.velocity);

		// set the timer so that the other client can't cancel
		// out the movement immediately
		if(!targ->client->ps.pm_time)
		{
			int             t;

			t = knockback * 2;
			if(t < 50)
			{
				t = 50;
			}
			if(t > 200)
			{
				t = 200;
			}
			targ->client->ps.pm_time = t;
			targ->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
		}
	}

	// check for completely getting out of the damage
	if(!(dflags & DAMAGE_NO_PROTECTION))
	{

		// if TF_NO_FRIENDLY_FIRE is set, don't do damage to the target
		// if the attacker was on the same team
#ifdef MISSIONPACK
		if(mod != MOD_JUICED && targ != attacker && !(dflags & DAMAGE_NO_TEAM_PROTECTION) && OnSameTeam(targ, attacker))
		{
#else
		if(targ != attacker && OnSameTeam(targ, attacker))
		{
#endif
			if(!g_friendlyFire.integer)
			{
				return;
			}
		}
#ifdef MISSIONPACK
		if(mod == MOD_PROXIMITY_MINE)
		{
			if(inflictor && inflictor->parent && OnSameTeam(targ, inflictor->parent))
			{
				return;
			}
			if(targ == attacker)
			{
				return;
			}
		}
#endif

		// check for godmode
		if(targ->flags & FL_GODMODE)
		{
			return;
		}
		if(client && client->ps.powerups[PW_SPAWNPROT])
		{
			G_AddEvent(targ, EV_POWERUP_BATTLESUIT, 0);
			return;
		}
	}
	if(Falling.integer == 0)
	{
		if(mod == MOD_FALLING)
		{
			return;
		}
	}
	// battlesuit protects from all radius damage (but takes knockback)
	// and protects 50% against all damage
	if(client && client->ps.powerups[PW_BATTLESUIT])
	{
		G_AddEvent(targ, EV_POWERUP_BATTLESUIT, 0);
		if((dflags & DAMAGE_RADIUS) || (mod == MOD_FALLING) || (mod == MOD_FLAMETHROWER))
		{
			return;
		}
		damage *= 0.5;
	}

	// add to the attacker's hit counter (if the target isn't a general entity like a prox mine)
	if(attacker->client && targ != attacker && targ->health > 0 && targ->s.eType != ET_MISSILE && targ->s.eType != ET_GENERAL)
	{
		if(OnSameTeam(targ, attacker))
		{
			attacker->client->ps.persistant[PERS_HITS]--;
		}
		else
		{
			attacker->client->ps.persistant[PERS_HITS]++;
		}
		attacker->client->ps.persistant[PERS_ATTACKEE_ARMOR] = (targ->health << 8) | (client->ps.stats[STAT_ARMOR]);
	}

	// always give half damage if hurting self
	// calculated after knockback, so rocket jumping works
	if(targ == attacker)
	{
		if(mod != MOD_FLAMETHROWER)
		{
			damage *= 0.5;
		}
	}

	if(targ == attacker)
	{
		if(dflags & DAMAGE_NO_SELF)
		{
			return;
		}
	}

	if(Hurtself.integer == 0)
	{
		if(targ == attacker)
		{
			if(mod == MOD_BFG_SPLASH)
			{
				return;
			}
		}
	}
	if(Hurtself.integer == 0)
	{
		if(targ == attacker)
		{
			if(mod == MOD_PLASMA_SPLASH)
			{
				return;
			}
		}
	}
	if(Hurtself.integer == 0)
	{
		if(targ == attacker)
		{
			if(mod == MOD_ROCKET_SPLASH)
			{
				return;
			}
		}
	}
	if(Hurtself.integer == 0)
	{
		if(targ == attacker)
		{
			if(mod == MOD_FLAMETHROWER)
			{
				return;
			}
		}
	}
	if(Hurtself.integer == 0)
	{
		if(targ == attacker)
		{
			if(mod == MOD_GRENADE_SPLASH)
			{
				return;
			}
		}
	}



	if(damage < 1)
	{
		damage = 1;
	}



	take = damage;
	save = 0;



	if(targ->client && targ->client->ps.stats[STAT_HEALTH] > 0)
	{
		if(targ != attacker && targ->client)
		{
			switch (mod)
			{
				case MOD_MACHINEGUN:
				case MOD_SHOTGUN:
				case MOD_RAILGUN:
				case MOD_PLASMA:
				case MOD_GAUNTLET:
				case MOD_LIGHTNING:
				case MOD_GRENADE:
				case MOD_ROCKET:
				case MOD_BFG_SPLASH:
				case MOD_BFG:
				case MOD_ROCKET_SPLASH:
				case MOD_GRENADE_SPLASH:
				case MOD_PLASMA_SPLASH:
				case MOD_FLAMETHROWER:
					if(damage >= targ->client->ps.stats[STAT_HEALTH])
					{
						count = targ->client->ps.stats[STAT_HEALTH];
					}
					else if(damage < targ->client->ps.stats[STAT_HEALTH])
					{
						count = damage;
					}
					targ->client->damageRT += count;
					break;
			}
		}
		if(targ != attacker && attacker->client)
		{
			switch (mod)
			{
				case MOD_MACHINEGUN:
				case MOD_SHOTGUN:
				case MOD_RAILGUN:
				case MOD_PLASMA:
				case MOD_GAUNTLET:
				case MOD_LIGHTNING:
				case MOD_GRENADE:
				case MOD_ROCKET:
				case MOD_BFG_SPLASH:
				case MOD_BFG:
				case MOD_ROCKET_SPLASH:
				case MOD_GRENADE_SPLASH:
				case MOD_PLASMA_SPLASH:
				case MOD_FLAMETHROWER:
					if(damage >= targ->client->ps.stats[STAT_HEALTH])
					{
						count = targ->client->ps.stats[STAT_HEALTH];
					}
					else if(damage < targ->client->ps.stats[STAT_HEALTH])
					{
						count = damage;
					}
					attacker->client->damageGT += count;
					break;
			}
		}
	}

	// save some from armor
	asave = CheckArmor(targ, damage, dflags, attacker, mod);
	take -= asave;



	if(g_debugDamage.integer)
	{
		G_Printf("%i: client:%i health:%i damage:%i armor:%i\n", level.time, targ->s.number, targ->health, take, asave);
	}

	if(attacker->client)
	{
		//  who = g_entities + attacker->client->lastkilled_client;
		if(mod == MOD_RAILGUN)
		{
			if(targ->client->lasthurt_mod == (MOD_ROCKET_SPLASH || MOD_GRENADE_SPLASH || MOD_BFG_SPLASH) &&
			   targ->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				if(targ->client->ps.groundEntityNum != ENTITYNUM_NONE)
				{
					attacker->client->lasthurt_cltime = 0;
				}
				if(level.time - attacker->client->lasthurt_cltime < 1150)
				{
					if(targ->client->ps.groundEntityNum == ENTITYNUM_NONE && targ->client->ps.stats[STAT_HEALTH] >= 1)
					{
//                          attacker->client->ps.persistant[PERS_RLRGCOMBO_COUNT]++;
						targ->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
						if(attacker->client)
						{
							attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
						}
						// add the sprite over the player's head
						attacker->client->ps.eFlags &=
							~(EF_AWARD_RLRGCOMBO | EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST
							  | EF_AWARD_DEFEND | EF_AWARD_CAP);
						attacker->client->ps.eFlags |= EF_AWARD_RLRGCOMBO;
						attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;
					}
				}
			}
			else if(targ->client->lasthurt_mod == (MOD_ROCKET || MOD_GRENADE || MOD_BFG) &&
					targ->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				if(targ->client->ps.groundEntityNum != ENTITYNUM_NONE)
				{
					attacker->client->lasthurt_cltime = 0;
				}
				if(level.time - attacker->client->lasthurt_cltime < 900)
				{
					if(targ->client->ps.groundEntityNum == ENTITYNUM_NONE && targ->client->ps.stats[STAT_HEALTH] >= 1)
					{
//                          attacker->client->ps.persistant[PERS_RLRGCOMBO_COUNT]++;
						targ->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
						if(attacker->client)
						{
							attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
						}
						// add the sprite over the player's head
						attacker->client->ps.eFlags &=
							~(EF_AWARD_RLRGCOMBO | EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST
							  | EF_AWARD_DEFEND | EF_AWARD_CAP);
						attacker->client->ps.eFlags |= EF_AWARD_RLRGCOMBO;
						attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;
					}
				}
			}
		}

		attacker->client->lasthurt_cltime = level.time;

		if(mod == MOD_ROCKET)
		{
			if(targ->client->lasthurt_mod == MOD_ROCKET_SPLASH && targ->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				if(targ->client->ps.groundEntityNum != ENTITYNUM_NONE)
				{
					attacker->client->lasthurt_cl1time = 0;
				}
				if(level.time - attacker->client->lasthurt_cl1time < 2000)
				{
					if(targ->client->ps.groundEntityNum == ENTITYNUM_NONE && targ->client->ps.stats[STAT_HEALTH] >= 1)
					{
						attacker->client->ps.persistant[PERS_IMPRESSIVE_COUNT]++;
						// add the sprite over the player's head
						attacker->client->ps.eFlags &=
							~(EF_AWARD_RLRGCOMBO | EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST
							  | EF_AWARD_DEFEND | EF_AWARD_CAP);
						attacker->client->ps.eFlags |= EF_AWARD_IMPRESSIVE;
						attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;
					}
				}
			}
			else if(targ->client->lasthurt_mod == MOD_ROCKET && targ->client->ps.groundEntityNum == ENTITYNUM_NONE)
			{
				if(targ->client->ps.groundEntityNum != ENTITYNUM_NONE)
				{
					attacker->client->lasthurt_cl1time = 0;
				}
				if(level.time - attacker->client->lasthurt_cl1time < 2000)
				{
					if(targ->client->ps.groundEntityNum == ENTITYNUM_NONE && targ->client->ps.stats[STAT_HEALTH] >= 1)
					{
						attacker->client->ps.persistant[PERS_IMPRESSIVE_COUNT]++;
						// add the sprite over the player's head
						attacker->client->ps.eFlags &=
							~(EF_AWARD_RLRGCOMBO | EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST
							  | EF_AWARD_DEFEND | EF_AWARD_CAP);
						attacker->client->ps.eFlags |= EF_AWARD_IMPRESSIVE;
						attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;
					}
				}
			}
		}
		attacker->client->lasthurt_cl1time = level.time;
	}
	// add to the damage inflicted on a player this frame
	// the total will be turned into screen blends and view angle kicks
	// at the end of the frame
	if(client)
	{
		if(attacker)
		{
			client->ps.persistant[PERS_ATTACKER] = attacker->s.number;
		}
		else
		{
			client->ps.persistant[PERS_ATTACKER] = ENTITYNUM_WORLD;
		}
		client->damage_armor += asave;
		client->damage_blood += take;
		client->damage_knockback += knockback;

		if(dir)
		{
			VectorCopy(dir, client->damage_from);
			client->damage_fromWorld = qfalse;
		}
		else
		{
			VectorCopy(targ->r.currentOrigin, client->damage_from);
			client->damage_fromWorld = qtrue;
		}
	}

	// See if it's the player hurting the emeny flag carrier
#ifdef MISSIONPACK
	if(g_gametype.integer == GT_CTF || g_gametype.integer == GT_1FCTF)
	{
#else
	if(g_gametype.integer == GT_CTF)
	{
#endif
		Team_CheckHurtCarrier(targ, attacker);
	}

	if(targ->client)
	{
		// set the last client who damaged the target
		targ->client->lasthurt_client = attacker->s.number;
		targ->client->lasthurt_mod = mod;
	}

	// do the damage
	if(take)
	{
		if(targ->client)
		{
			if(mod == MOD_ROCKET_SPLASH)
			{
				if(targ->client->ps.stats[STAT_ARMOR] <= 0)
				{
					targ->health = targ->health - take;
				}
			}
			else if(mod == MOD_GRENADE_SPLASH)
			{
				if(targ->client->ps.stats[STAT_ARMOR] <= 0)
				{
					targ->health = targ->health - take;
				}
			}
			else if(mod == MOD_PLASMA_SPLASH)
			{
				if(targ->client->ps.stats[STAT_ARMOR] <= 0)
				{
					targ->health = targ->health - take;
				}
			}
			else if(mod == MOD_FLAMETHROWER)
			{
				if(targ->client->ps.stats[STAT_ARMOR] <= 0)
				{
					targ->health = targ->health - take;
				}
			}
			else if(mod == MOD_MACHINEGUN)
			{
				if(targ->client->ps.stats[STAT_ARMOR] <= 0)
				{
					targ->health = targ->health - take;
				}
			}
			else if(mod == MOD_SHOTGUN)
			{
				if(targ->client->ps.stats[STAT_ARMOR] <= 0)
				{
					targ->health = targ->health - take;
				}
			}
			else if(mod == MOD_GRENADE)
			{
				if(targ->client->ps.stats[STAT_ARMOR] <= 0)
				{
					targ->health = targ->health - take;
				}
			}
			else if(mod == MOD_PLASMA)
			{
				if(targ->client->ps.stats[STAT_ARMOR] <= 0)
				{
					targ->health = targ->health - take;
				}
			}
			else if(mod == MOD_RAILGUN)
			{
				if(targ->client->ps.stats[STAT_ARMOR] <= 0)
				{
					targ->health = targ->health - take;
				}
			}
			else if(mod == MOD_GAUNTLET)
			{
				if(targ->client->ps.stats[STAT_ARMOR] <= 0)
				{
					targ->health = targ->health - take;
				}
			}
			else if(mod == MOD_ROCKET)
			{
				if(targ->client->ps.stats[STAT_ARMOR] <= 0)
				{
					targ->health = targ->health - take;
				}
			}
			else if(mod == MOD_LIGHTNING)
			{
				if(targ->client->ps.stats[STAT_ARMOR] <= 0)
				{
					targ->health = targ->health - take;
				}
			}
			else
			{
				targ->health = targ->health - take;
			}
		}
		else
		{
			targ->health = targ->health - take;
		}
		// vampire mode
		if(vampire.integer == 1)
		{
			if(targ->client && attacker->client)
			{
				if(attacker->health >= 1 && attacker->health < 100)
				{
					if(targ->r.contents != CONTENTS_CORPSE)
					{
						if(targ != attacker)
						{
							if(targ->client->ps.stats[STAT_ARMOR] == 0)
							{
								if(targ != attacker)
								{
									if(!OnSameTeam(targ, attacker))
									{
										if(attacker->health + take / 4.0 <= 100)
										{
											attacker->health += take / 4.0;
										}
										else if(attacker->health + take / 4.0 >= 100)
										{
											attacker->health = 100;
										}
									}
								}
							}
						}
					}
				}
			}
		}

		// vampire mode
		if(targ->client)
		{

			targ->client->ps.stats[STAT_HEALTH] = targ->health;

		}

		if(targ->health <= 0)
		{
			if(client)
				//  targ->flags |= FL_NO_KNOCKBACK;

				if(targ->health < -999)
					targ->health = -999;

			targ->enemy = attacker;
			targ->die(targ, inflictor, attacker, take, mod);
			return;
		}
		else if(targ->pain)
		{
			targ->pain(targ, attacker, take);
		}
	}

}


/*
============
CanDamage

Returns qtrue if the inflictor can directly damage the target.  Used for
explosions and melee attacks.
============
*/
qboolean CanDamage(gentity_t * targ, vec3_t origin)
{
	vec3_t          dest;
	trace_t         tr;
	vec3_t          midpoint;

	// use the midpoint of the bounds instead of the origin, because
	// bmodels may have their origin is 0,0,0
	VectorAdd(targ->r.absmin, targ->r.absmax, midpoint);
	VectorScale(midpoint, 0.5, midpoint);

	VectorCopy(midpoint, dest);
	trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);
	if(tr.fraction == 1.0 || tr.entityNum == targ->s.number)
		return qtrue;

	// this should probably check in the plane of projection, 
	// rather than in world coordinate, and also include Z
	VectorCopy(midpoint, dest);
	dest[0] += 15.0;
	dest[1] += 15.0;
	trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);
	if(tr.fraction == 1.0)
		return qtrue;

	VectorCopy(midpoint, dest);
	dest[0] += 15.0;
	dest[1] -= 15.0;
	trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);
	if(tr.fraction == 1.0)
		return qtrue;

	VectorCopy(midpoint, dest);
	dest[0] -= 15.0;
	dest[1] += 15.0;
	trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);
	if(tr.fraction == 1.0)
		return qtrue;

	VectorCopy(midpoint, dest);
	dest[0] -= 15.0;
	dest[1] -= 15.0;
	trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);
	if(tr.fraction == 1.0)
		return qtrue;


	return qfalse;
}


/*
============
G_RadiusDamage
============
*/
qboolean G_RadiusDamage(vec3_t origin, gentity_t * attacker, float damage, float radius, gentity_t * ignore, int mod)
{
	float           points, dist;
	gentity_t      *ent;
	int             entityList[MAX_GENTITIES];
	int             numListedEntities;
	vec3_t          mins, maxs;
	vec3_t          v;
	vec3_t          dir;
	int             i, e;
	qboolean        hitClient = qfalse;

	if(radius < 1)
	{
		radius = 1;
	}

	for(i = 0; i < 3; i++)
	{
		mins[i] = origin[i] - radius;
		maxs[i] = origin[i] + radius;
	}

	numListedEntities = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

	for(e = 0; e < numListedEntities; e++)
	{
		ent = &g_entities[entityList[e]];

		if(ent == ignore)
			continue;
		if(!ent->takedamage)
			continue;

		// find the distance from the edge of the bounding box
		for(i = 0; i < 3; i++)
		{
			if(origin[i] < ent->r.absmin[i])
			{
				v[i] = ent->r.absmin[i] - origin[i];
			}
			else if(origin[i] > ent->r.absmax[i])
			{
				v[i] = origin[i] - ent->r.absmax[i];
			}
			else
			{
				v[i] = 0;
			}
		}

		dist = VectorLength(v);
		if(dist >= radius)
		{
			continue;
		}

		points = damage * (1.0 - dist / radius);

		if(CanDamage(ent, origin))
		{
			if(LogAccuracyHit(ent, attacker))
			{
				hitClient = qtrue;
			}
			VectorSubtract(ent->r.currentOrigin, origin, dir);
			// push the center of mass higher than the origin so players
			// get knocked into the air more
			dir[2] += 24;
			G_Damage(ent, NULL, attacker, dir, origin, (int)points, DAMAGE_RADIUS, mod);
		}
	}

	return hitClient;
}

/*
qboolean G_RadiusRailDamage ( vec3_t origin, gentity_t *attacker, float damage, float radius,
					 gentity_t *ignore, int mod,vec3_t start,const vec3_t end) {
	float		points, dist;
	gentity_t	*ent;
	int			entityList[MAX_GENTITIES];
//	int			numListedEntities;
	vec3_t		mins, maxs;
	vec3_t		v;
	vec3_t		dir;
	trace_t		*trace;
	int			i, e;
	qboolean	hitClient = qfalse;

	if ( radius < 1 ) {
		radius = 1;
	}

	for ( i = 0 ; i < 3 ; i++ ) {
		mins[i] = origin[i] - radius;
		maxs[i] = origin[i] + radius;
	}

//	trap_TraceCapsule( trace, start, mins, maxs, end, ignore->client->ps.clientNum, MASK_SHOT );
	ent = &g_entities[ trace->entityNum ];

	if( ent->client) {
	//	if( LogAccuracyHit( ent, attacker ) ) {
				hitClient = qtrue;
	//	}
		VectorSubtract (ent->r.currentOrigin, origin, dir);
			// push the center of mass higher than the origin so players
			// get knocked into the air more
		dir[2] += 24;
		G_Damage (ent, NULL, attacker, dir, origin, damage, DAMAGE_RADIUS, mod);
	}

//	numListedEntities = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );


	for ( e = 0 ; e < trace.entityNum ; e++ ) {
		ent = &g_entities[ e ];

		if (ent == ignore)
			continue;
		if (!ent->takedamage)
			continue;



	}

	return hitClient;
}
*/
