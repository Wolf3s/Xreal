/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2006-2008 Robert Beckebans <trebor_7@users.sourceforge.net>

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

#include "server.h"

/*
===============================================================================

OPERATOR CONSOLE ONLY COMMANDS

These commands can only be entered from stdin or by a remote operator datagram
===============================================================================
*/


/*
==================
SV_GetPlayerByHandle

Returns the player with player id or name from Cmd_Argv(1)
==================
*/
static client_t *SV_GetPlayerByHandle(void)
{
	client_t       *cl;
	int             i;
	char           *s;
	char            cleanName[64];

	// make sure server is running
	if(!com_sv_running->integer)
	{
		return NULL;
	}

	if(Cmd_Argc() < 2)
	{
		Com_Printf("No player specified.\n");
		return NULL;
	}

	s = Cmd_Argv(1);

	// Check whether this is a numeric player handle
	for(i = 0; s[i] >= '0' && s[i] <= '9'; i++);

	if(!s[i])
	{
		int             plid = atoi(s);

		// Check for numeric playerid match
		if(plid >= 0 && plid < sv_maxclients->integer)
		{
			cl = &svs.clients[plid];

			if(cl->state)
				return cl;
		}
	}

	// check for a name match
	for(i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++)
	{
		if(!cl->state)
		{
			continue;
		}
		if(!Q_stricmp(cl->name, s))
		{
			return cl;
		}

		Q_strncpyz(cleanName, cl->name, sizeof(cleanName));
		Q_CleanStr(cleanName);
		if(!Q_stricmp(cleanName, s))
		{
			return cl;
		}
	}

	Com_Printf("Player %s is not on the server\n", s);

	return NULL;
}

/*
==================
SV_GetPlayerByNum

Returns the player with idnum from Cmd_Argv(1)
==================
*/
static client_t *SV_GetPlayerByNum(void)
{
	client_t       *cl;
	int             i;
	int             idnum;
	char           *s;

	// make sure server is running
	if(!com_sv_running->integer)
	{
		return NULL;
	}

	if(Cmd_Argc() < 2)
	{
		Com_Printf("No player specified.\n");
		return NULL;
	}

	s = Cmd_Argv(1);

	for(i = 0; s[i]; i++)
	{
		if(s[i] < '0' || s[i] > '9')
		{
			Com_Printf("Bad slot number: %s\n", s);
			return NULL;
		}
	}
	idnum = atoi(s);
	if(idnum < 0 || idnum >= sv_maxclients->integer)
	{
		Com_Printf("Bad client slot: %i\n", idnum);
		return NULL;
	}

	cl = &svs.clients[idnum];
	if(!cl->state)
	{
		Com_Printf("Client %i is not active\n", idnum);
		return NULL;
	}
	return cl;
}

//=========================================================


/*
==================
SV_Map_f

Restart the server on a different map
==================
*/
static void SV_Map_f(void)
{
	char           *cmd;
	char           *map;
	qboolean        killBots, cheat;
	char            expanded[MAX_QPATH];
	char            mapname[MAX_QPATH];

	map = Cmd_Argv(1);
	if(!map)
	{
		return;
	}

	// make sure the level exists before trying to change, so that
	// a typo at the server console won't end the game
	Com_sprintf(expanded, sizeof(expanded), "maps/%s.bsp", map);
	if(FS_ReadFile(expanded, NULL) == -1)
	{
		Com_Printf("Can't find map %s\n", expanded);
		return;
	}

	// force latched values to get set
	Cvar_Get("g_gametype", "0", CVAR_SERVERINFO | CVAR_USERINFO | CVAR_LATCH);

	cmd = Cmd_Argv(0);
	if(Q_stricmpn(cmd, "sp", 2) == 0)
	{
		Cvar_SetValue("g_gametype", GT_SINGLE_PLAYER);
		Cvar_SetValue("g_doWarmup", 0);
		// may not set sv_maxclients directly, always set latched
		Cvar_SetLatched("sv_maxclients", "8");
		cmd += 2;
		cheat = qfalse;
		killBots = qtrue;
	}
	else
	{
		if(!Q_stricmp(cmd, "devmap") || !Q_stricmp(cmd, "spdevmap"))
		{
			cheat = qtrue;
			killBots = qtrue;
		}
		else
		{
			cheat = qfalse;
			killBots = qfalse;
		}
		if(sv_gametype->integer == GT_SINGLE_PLAYER)
		{
			Cvar_SetValue("g_gametype", GT_FFA);
		}
	}

	// save the map name here cause on a map restart we reload the q3config.cfg
	// and thus nuke the arguments of the map command
	Q_strncpyz(mapname, map, sizeof(mapname));

	// start up the map
	SV_SpawnServer(mapname, killBots);

	// set the cheat value
	// if the level was started with "map <levelname>", then
	// cheats will not be allowed.  If started with "devmap <levelname>"
	// then cheats will be allowed
	if(cheat)
	{
		Cvar_Set("sv_cheats", "1");
	}
	else
	{
		Cvar_Set("sv_cheats", "0");
	}
}

/*
================
SV_MapRestart_f

Completely restarts a level, but doesn't send a new gamestate to the clients.
This allows fair starts with variable load times.
================
*/
static void SV_MapRestart_f(void)
{
	int             i;
	client_t       *client;
	char           *denied;
	qboolean        isBot;
	int             delay;

	// make sure we aren't restarting twice in the same frame
	if(com_frameTime == sv.serverId)
	{
		return;
	}

	// make sure server is running
	if(!com_sv_running->integer)
	{
		Com_Printf("Server is not running.\n");
		return;
	}

	if(sv.restartTime)
	{
		return;
	}

	if(Cmd_Argc() > 1)
	{
		delay = atoi(Cmd_Argv(1));
	}
	else
	{
		delay = 5;
	}
	if(delay && !Cvar_VariableValue("g_doWarmup"))
	{
		sv.restartTime = sv.time + delay * 1000;
		SV_SetConfigstring(CS_WARMUP, va("%i", sv.restartTime));
		return;
	}

	// check for changes in variables that can't just be restarted
	// check for maxclients change
	if(sv_maxclients->modified || sv_gametype->modified)
	{
		char            mapname[MAX_QPATH];

		Com_Printf("variable change -- restarting.\n");
		// restart the map the slow way
		Q_strncpyz(mapname, Cvar_VariableString("mapname"), sizeof(mapname));

		SV_SpawnServer(mapname, qfalse);
		return;
	}

	// toggle the server bit so clients can detect that a
	// map_restart has happened
	svs.snapFlagServerBit ^= SNAPFLAG_SERVERCOUNT;

	// generate a new serverid  
	// TTimo - don't update restartedserverId there, otherwise we won't deal correctly with multiple map_restart
	sv.serverId = com_frameTime;
	Cvar_Set("sv_serverid", va("%i", sv.serverId));

	// reset all the vm data in place without changing memory allocation
	// note that we do NOT set sv.state = SS_LOADING, so configstrings that
	// had been changed from their default values will generate broadcast updates
	sv.state = SS_LOADING;
	sv.restarting = qtrue;

	SV_RestartGameProgs();

	// run a few frames to allow everything to settle
	for(i = 0; i < 3; i++)
	{
		VM_Call(gvm, GAME_RUN_FRAME, sv.time);
		sv.time += 100;
		svs.time += 100;
	}

	sv.state = SS_GAME;
	sv.restarting = qfalse;

	// connect and begin all the clients
	for(i = 0; i < sv_maxclients->integer; i++)
	{
		client = &svs.clients[i];

		// send the new gamestate to all connected clients
		if(client->state < CS_CONNECTED)
		{
			continue;
		}

		if(client->netchan.remoteAddress.type == NA_BOT)
		{
			isBot = qtrue;
		}
		else
		{
			isBot = qfalse;
		}

		// add the map_restart command
		SV_AddServerCommand(client, "map_restart\n");

		// connect the client again, without the firstTime flag
		denied = VM_ExplicitArgPtr(gvm, VM_Call(gvm, GAME_CLIENT_CONNECT, i, qfalse, isBot));
		if(denied)
		{
			// this generally shouldn't happen, because the client
			// was connected before the level change
			SV_DropClient(client, denied);
			Com_Printf("SV_MapRestart_f(%d): dropped client %i - denied!\n", delay, i);
			continue;
		}

		client->state = CS_ACTIVE;

		SV_ClientEnterWorld(client, &client->lastUsercmd);
	}

	// run another frame to allow things to look at all the players
	VM_Call(gvm, GAME_RUN_FRAME, sv.time);
	sv.time += 100;
	svs.time += 100;
}

//===============================================================

/*
==================
SV_Kick_f

Kick a user off of the server  FIXME: move to game
==================
*/
static void SV_Kick_f(void)
{
	client_t       *cl;
	int             i;

	// make sure server is running
	if(!com_sv_running->integer)
	{
		Com_Printf("Server is not running.\n");
		return;
	}

	if(Cmd_Argc() != 2)
	{
		Com_Printf("Usage: kick <player name>\nkick all = kick everyone\nkick allbots = kick all bots\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if(!cl)
	{
		if(!Q_stricmp(Cmd_Argv(1), "all"))
		{
			for(i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++)
			{
				if(!cl->state)
				{
					continue;
				}
				if(cl->netchan.remoteAddress.type == NA_LOOPBACK)
				{
					continue;
				}
				SV_DropClient(cl, "was kicked");
				cl->lastPacketTime = svs.time;	// in case there is a funny zombie
			}
		}
		else if(!Q_stricmp(Cmd_Argv(1), "allbots"))
		{
			for(i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++)
			{
				if(!cl->state)
				{
					continue;
				}
				if(cl->netchan.remoteAddress.type != NA_BOT)
				{
					continue;
				}
				SV_DropClient(cl, "was kicked");
				cl->lastPacketTime = svs.time;	// in case there is a funny zombie
			}
		}
		return;
	}
	if(cl->netchan.remoteAddress.type == NA_LOOPBACK)
	{
		SV_SendServerCommand(NULL, "print \"%s\"", "Cannot kick host player\n");
		return;
	}

	SV_DropClient(cl, "was kicked");
	cl->lastPacketTime = svs.time;	// in case there is a funny zombie
}

#ifndef STANDALONE
// these functions require the auth server which of course is not available anymore for stand-alone games.

/*
==================
SV_Ban_f

Ban a user from being able to play on this server through the auth
server
==================
*/
static void SV_Ban_f(void)
{
	client_t       *cl;

	// make sure server is running
	if(!com_sv_running->integer)
	{
		Com_Printf("Server is not running.\n");
		return;
	}

	if(Cmd_Argc() != 2)
	{
		Com_Printf("Usage: banUser <player name>\n");
		return;
	}

	cl = SV_GetPlayerByHandle();

	if(!cl)
	{
		return;
	}

	if(cl->netchan.remoteAddress.type == NA_LOOPBACK)
	{
		SV_SendServerCommand(NULL, "print \"%s\"", "Cannot kick host player\n");
		return;
	}

	// look up the authorize server's IP
	if(!svs.authorizeAddress.ip[0] && svs.authorizeAddress.type != NA_BAD)
	{
		Com_Printf("Resolving %s\n", AUTHORIZE_SERVER_NAME);
		if(!NET_StringToAdr(AUTHORIZE_SERVER_NAME, &svs.authorizeAddress, NA_IP))
		{
			Com_Printf("Couldn't resolve address\n");
			return;
		}
		svs.authorizeAddress.port = BigShort(PORT_AUTHORIZE);
		Com_Printf("%s resolved to %i.%i.%i.%i:%i\n", AUTHORIZE_SERVER_NAME,
				   svs.authorizeAddress.ip[0], svs.authorizeAddress.ip[1],
				   svs.authorizeAddress.ip[2], svs.authorizeAddress.ip[3], BigShort(svs.authorizeAddress.port));
	}

	// otherwise send their ip to the authorize server
	if(svs.authorizeAddress.type != NA_BAD)
	{
		NET_OutOfBandPrint(NS_SERVER, svs.authorizeAddress,
						   "banUser %i.%i.%i.%i", cl->netchan.remoteAddress.ip[0], cl->netchan.remoteAddress.ip[1],
						   cl->netchan.remoteAddress.ip[2], cl->netchan.remoteAddress.ip[3]);
		Com_Printf("%s was banned from coming back\n", cl->name);
	}
}

/*
==================
SV_BanNum_f

Ban a user from being able to play on this server through the auth
server
==================
*/
static void SV_BanNum_f(void)
{
	client_t       *cl;

	// make sure server is running
	if(!com_sv_running->integer)
	{
		Com_Printf("Server is not running.\n");
		return;
	}

	if(Cmd_Argc() != 2)
	{
		Com_Printf("Usage: banClient <client number>\n");
		return;
	}

	cl = SV_GetPlayerByNum();
	if(!cl)
	{
		return;
	}
	if(cl->netchan.remoteAddress.type == NA_LOOPBACK)
	{
		SV_SendServerCommand(NULL, "print \"%s\"", "Cannot kick host player\n");
		return;
	}

	// look up the authorize server's IP
	if(!svs.authorizeAddress.ip[0] && svs.authorizeAddress.type != NA_BAD)
	{
		Com_Printf("Resolving %s\n", AUTHORIZE_SERVER_NAME);
		if(!NET_StringToAdr(AUTHORIZE_SERVER_NAME, &svs.authorizeAddress, NA_IP))
		{
			Com_Printf("Couldn't resolve address\n");
			return;
		}
		svs.authorizeAddress.port = BigShort(PORT_AUTHORIZE);
		Com_Printf("%s resolved to %i.%i.%i.%i:%i\n", AUTHORIZE_SERVER_NAME,
				   svs.authorizeAddress.ip[0], svs.authorizeAddress.ip[1],
				   svs.authorizeAddress.ip[2], svs.authorizeAddress.ip[3], BigShort(svs.authorizeAddress.port));
	}

	// otherwise send their ip to the authorize server
	if(svs.authorizeAddress.type != NA_BAD)
	{
		NET_OutOfBandPrint(NS_SERVER, svs.authorizeAddress,
						   "banUser %i.%i.%i.%i", cl->netchan.remoteAddress.ip[0], cl->netchan.remoteAddress.ip[1],
						   cl->netchan.remoteAddress.ip[2], cl->netchan.remoteAddress.ip[3]);
		Com_Printf("%s was banned from coming back\n", cl->name);
	}
}
#endif

/*
==================
SV_RehashBans_f

Load saved bans from file.
==================
*/
void SV_RehashBans_f(void)
{
	int             index, filelen;
	fileHandle_t    readfrom;
	char           *textbuf, *curpos, *maskpos, *newlinepos, *endpos;
	char            filepath[MAX_QPATH];

	serverBansCount = 0;

	if(!(curpos = Cvar_VariableString("fs_game")) || !*curpos)
		curpos = BASEGAME;

	Com_sprintf(filepath, sizeof(filepath), "%s/%s", curpos, SERVER_BANFILE);

	if((filelen = FS_SV_FOpenFileRead(filepath, &readfrom)) >= 0)
	{
		if(filelen < 2)
		{
			// Don't bother if file is too short.
			FS_FCloseFile(readfrom);
			return;
		}

		curpos = textbuf = Z_Malloc(filelen);

		filelen = FS_Read(textbuf, filelen, readfrom);
		FS_FCloseFile(readfrom);

		endpos = textbuf + filelen;

		for(index = 0; index < SERVER_MAXBANS && curpos + 2 < endpos; index++)
		{
			// find the end of the address string
			for(maskpos = curpos + 2; maskpos < endpos && *maskpos != ' '; maskpos++);

			if(maskpos + 1 >= endpos)
				break;

			*maskpos = '\0';
			maskpos++;

			// find the end of the subnet specifier
			for(newlinepos = maskpos; newlinepos < endpos && *newlinepos != '\n'; newlinepos++);

			if(newlinepos >= endpos)
				break;

			*newlinepos = '\0';

			if(NET_StringToAdr(curpos + 2, &serverBans[index].ip, NA_UNSPEC))
			{
				serverBans[index].isexception = !(curpos[0] == '0');
				serverBans[index].subnet = atoi(maskpos);

				if(serverBans[index].ip.type == NA_IP && (serverBans[index].subnet < 0 || serverBans[index].subnet > 32))
				{
					serverBans[index].subnet = 32;
				}
				else if(serverBans[index].ip.type == NA_IP6 && (serverBans[index].subnet < 0 || serverBans[index].subnet > 128))
				{
					serverBans[index].subnet = 128;
				}
			}

			curpos = newlinepos + 1;
		}

		serverBansCount = index;

		Z_Free(textbuf);
	}
}

/*
==================
SV_BanAddr_f

Ban a user from being able to play on this server based on his ip address.
==================
*/

static void SV_AddBanToList(qboolean isexception)
{
	char           *banstring, *suffix;
	netadr_t        ip;
	int             argc, mask;
	fileHandle_t    writeto;

	argc = Cmd_Argc();

	if(argc < 2 || argc > 3)
	{
		Com_Printf("Usage: %s (ip[/subnet] | clientnum [subnet])\n", Cmd_Argv(0));
		return;
	}

	if(serverBansCount > sizeof(serverBans) / sizeof(*serverBans))
	{
		Com_Printf("Error: Maximum number of bans/exceptions exceeded.\n");
		return;
	}

	banstring = Cmd_Argv(1);

	if(strchr(banstring, '.') || strchr(banstring, ':'))
	{
		// This is an ip address, not a client num.

		// Look for a CIDR-Notation suffix
		suffix = strchr(banstring, '/');
		if(suffix)
		{
			*suffix = '\0';
			suffix++;
		}

		if(!NET_StringToAdr(banstring, &ip, NA_UNSPEC))
		{
			Com_Printf("Error: Invalid address %s\n", banstring);
			return;
		}
	}
	else
	{
		client_t       *cl;

		// client num.
		if(!com_sv_running->integer)
		{
			Com_Printf("Server is not running.\n");
			return;
		}

		cl = SV_GetPlayerByNum();

		if(!cl)
		{
			Com_Printf("Error: Playernum %s does not exist.\n", Cmd_Argv(1));
			return;
		}

		ip = cl->netchan.remoteAddress;

		if(argc == 3)
			suffix = Cmd_Argv(2);
		else
			suffix = NULL;
	}

	if(ip.type != NA_IP && ip.type != NA_IP6)
	{
		Com_Printf("Error: Can ban players connected via the internet only.\n");
		return;
	}

	if(suffix)
	{
		mask = atoi(suffix);

		if(ip.type == NA_IP)
		{
			if(mask < 0 || mask > 32)
				mask = 32;
		}
		else
		{
			if(mask < 0 || mask > 128)
				mask = 128;
		}
	}
	else if(ip.type == NA_IP)
		mask = 32;
	else
		mask = 128;

	serverBans[serverBansCount].ip = ip;
	serverBans[serverBansCount].subnet = mask;
	serverBans[serverBansCount].isexception = isexception;

	Com_Printf("Added %s: %s/%d\n", isexception ? "ban exception" : "ban", NET_AdrToString(ip), mask);

	// Write out the ban information.
	if((writeto = FS_FOpenFileAppend(SERVER_BANFILE)))
	{
		char            writebuf[128];

		Com_sprintf(writebuf, sizeof(writebuf), "%d %s %d\n", isexception, NET_AdrToString(ip), mask);
		FS_Write(writebuf, strlen(writebuf), writeto);
		FS_FCloseFile(writeto);
	}

	serverBansCount++;
}

static void SV_DelBanFromList(qboolean isexception)
{
	int             index, count, todel;
	fileHandle_t    writeto;

	if(Cmd_Argc() != 2)
	{
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	todel = atoi(Cmd_Argv(1));

	if(todel < 0 || todel > serverBansCount)
		return;

	for(index = count = 0; index < serverBansCount; index++)
	{
		if(serverBans[index].isexception == isexception)
		{
			count++;

			if(count == todel)
				break;
		}
	}

	if(index == serverBansCount - 1)
		serverBansCount--;
	else if(index < sizeof(serverBans) / sizeof(*serverBans) - 1)
	{
		memmove(serverBans + index, serverBans + index + 1, (serverBansCount - index - 1) * sizeof(*serverBans));
		serverBansCount--;
	}
	else
	{
		Com_Printf("Error: No such entry #%d\n", todel);
		return;
	}

	// Write out the ban information.
	if((writeto = FS_FOpenFileWrite(SERVER_BANFILE)))
	{
		char            writebuf[128];
		serverBan_t    *curban;

		for(index = 0; index < serverBansCount; index++)
		{
			curban = &serverBans[index];

			Com_sprintf(writebuf, sizeof(writebuf), "%d %s %d\n",
						curban->isexception, NET_AdrToString(curban->ip), curban->subnet);
			FS_Write(writebuf, strlen(writebuf), writeto);
		}

		FS_FCloseFile(writeto);
	}
}

static void SV_ListBans_f(void)
{
	int             index, count;
	serverBan_t    *ban;

	// List all bans
	for(index = count = 0; index < serverBansCount; index++)
	{
		ban = &serverBans[index];
		if(!ban->isexception)
		{
			count++;

			Com_Printf("Ban #%d: %s/%d\n", count, NET_AdrToString(ban->ip), ban->subnet);
		}
	}
	// List all exceptions
	for(index = count = 0; index < serverBansCount; index++)
	{
		ban = &serverBans[index];
		if(ban->isexception)
		{
			count++;

			Com_Printf("Except #%d: %s/%d\n", count, NET_AdrToString(ban->ip), ban->subnet);
		}
	}
}

static void SV_FlushBans_f(void)
{
	fileHandle_t    blankf;

	serverBansCount = 0;

	// empty the ban file.
	blankf = FS_FOpenFileWrite(SERVER_BANFILE);

	if(blankf)
		FS_FCloseFile(blankf);
}

static void SV_BanAddr_f(void)
{
	SV_AddBanToList(qfalse);
}

static void SV_ExceptAddr_f(void)
{
	SV_AddBanToList(qtrue);
}

static void SV_BanDel_f(void)
{
	SV_DelBanFromList(qfalse);
}

static void SV_ExceptDel_f(void)
{
	SV_DelBanFromList(qtrue);
}

/*
==================
SV_KickNum_f

Kick a user off of the server  FIXME: move to game
==================
*/
static void SV_KickNum_f(void)
{
	client_t       *cl;

	// make sure server is running
	if(!com_sv_running->integer)
	{
		Com_Printf("Server is not running.\n");
		return;
	}

	if(Cmd_Argc() != 2)
	{
		Com_Printf("Usage: kicknum <client number>\n");
		return;
	}

	cl = SV_GetPlayerByNum();
	if(!cl)
	{
		return;
	}
	if(cl->netchan.remoteAddress.type == NA_LOOPBACK)
	{
		SV_SendServerCommand(NULL, "print \"%s\"", "Cannot kick host player\n");
		return;
	}

	SV_DropClient(cl, "was kicked");
	cl->lastPacketTime = svs.time;	// in case there is a funny zombie
}

/*
================
SV_Status_f
================
*/
static void SV_Status_f(void)
{
	int             i, j, l;
	client_t       *cl;
	playerState_t  *ps;
	const char     *s;
	int             ping;

	// make sure server is running
	if(!com_sv_running->integer)
	{
		Com_Printf("Server is not running.\n");
		return;
	}

	Com_Printf("map: %s\n", sv_mapname->string);

	Com_Printf("num score ping name            lastmsg address               qport rate\n");
	Com_Printf("--- ----- ---- --------------- ------- --------------------- ----- -----\n");
	for(i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++)
	{
		if(!cl->state)
			continue;
		Com_Printf("%3i ", i);
		ps = SV_GameClientNum(i);
		Com_Printf("%5i ", ps->persistant[PERS_SCORE]);

		if(cl->state == CS_CONNECTED)
			Com_Printf("CNCT ");
		else if(cl->state == CS_ZOMBIE)
			Com_Printf("ZMBI ");
		else
		{
			ping = cl->ping < 9999 ? cl->ping : 9999;
			Com_Printf("%4i ", ping);
		}

		Com_Printf("%s", cl->name);
		// TTimo adding a ^7 to reset the color
		// NOTE: colored names in status breaks the padding (WONTFIX)
		Com_Printf("^7");
		l = 16 - strlen(cl->name);
		for(j = 0; j < l; j++)
			Com_Printf(" ");

		Com_Printf("%7i ", svs.time - cl->lastPacketTime);

		s = NET_AdrToString(cl->netchan.remoteAddress);
		Com_Printf("%s", s);
		l = 22 - strlen(s);
		for(j = 0; j < l; j++)
			Com_Printf(" ");

		Com_Printf("%5i", cl->netchan.qport);

		Com_Printf(" %5i", cl->rate);

		Com_Printf("\n");
	}
	Com_Printf("\n");
}

/*
==================
SV_ConSay_f
==================
*/
static void SV_ConSay_f(void)
{
	char           *p;
	char            text[1024];

	// make sure server is running
	if(!com_sv_running->integer)
	{
		Com_Printf("Server is not running.\n");
		return;
	}

	if(Cmd_Argc() < 2)
	{
		return;
	}

	strcpy(text, "console: ");
	p = Cmd_Args();

	if(*p == '"')
	{
		p++;
		p[strlen(p) - 1] = 0;
	}

	strcat(text, p);

	SV_SendServerCommand(NULL, "chat \"%s\"", text);
}


/*
==================
SV_Heartbeat_f

Also called by SV_DropClient, SV_DirectConnect, and SV_SpawnServer
==================
*/
void SV_Heartbeat_f(void)
{
	svs.nextHeartbeatTime = -9999999;
}


/*
===========
SV_Serverinfo_f

Examine the serverinfo string
===========
*/
static void SV_Serverinfo_f(void)
{
	Com_Printf("Server info settings:\n");
	Info_Print(Cvar_InfoString(CVAR_SERVERINFO));
}


/*
===========
SV_Systeminfo_f

Examine or change the serverinfo string
===========
*/
static void SV_Systeminfo_f(void)
{
	Com_Printf("System info settings:\n");
	Info_Print(Cvar_InfoString(CVAR_SYSTEMINFO));
}


/*
===========
SV_DumpUser_f

Examine all a users info strings FIXME: move to game
===========
*/
static void SV_DumpUser_f(void)
{
	client_t       *cl;

	// make sure server is running
	if(!com_sv_running->integer)
	{
		Com_Printf("Server is not running.\n");
		return;
	}

	if(Cmd_Argc() != 2)
	{
		Com_Printf("Usage: info <userid>\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if(!cl)
	{
		return;
	}

	Com_Printf("userinfo\n");
	Com_Printf("--------\n");
	Info_Print(cl->userinfo);
}


/*
=================
SV_KillServer
=================
*/
static void SV_KillServer_f(void)
{
	SV_Shutdown("killserver");
}

//===========================================================

/*
==================
SV_AddOperatorCommands
==================
*/
void SV_AddOperatorCommands(void)
{
	static qboolean initialized;

	if(initialized)
	{
		return;
	}
	initialized = qtrue;

	Cmd_AddCommand("heartbeat", SV_Heartbeat_f);
	Cmd_AddCommand("kick", SV_Kick_f);
#ifndef STANDALONE
	if(!Cvar_VariableIntegerValue("com_standalone"))
	{
		Cmd_AddCommand("banUser", SV_Ban_f);
		Cmd_AddCommand("banClient", SV_BanNum_f);
	}
#endif
	Cmd_AddCommand("clientkick", SV_KickNum_f);
	Cmd_AddCommand("status", SV_Status_f);
	Cmd_AddCommand("serverinfo", SV_Serverinfo_f);
	Cmd_AddCommand("systeminfo", SV_Systeminfo_f);
	Cmd_AddCommand("dumpuser", SV_DumpUser_f);
	Cmd_AddCommand("map_restart", SV_MapRestart_f);
	Cmd_AddCommand("sectorlist", SV_SectorList_f);
	Cmd_AddCommand("map", SV_Map_f);
	Cmd_AddCommand("devmap", SV_Map_f);
	Cmd_AddCommand("spmap", SV_Map_f);
	Cmd_AddCommand("spdevmap", SV_Map_f);
	Cmd_AddCommand("killserver", SV_KillServer_f);
	if(com_dedicated->integer)
	{
		Cmd_AddCommand("say", SV_ConSay_f);
	}

	Cmd_AddCommand("rehashbans", SV_RehashBans_f);
	Cmd_AddCommand("listbans", SV_ListBans_f);
	Cmd_AddCommand("banaddr", SV_BanAddr_f);
	Cmd_AddCommand("exceptaddr", SV_ExceptAddr_f);
	Cmd_AddCommand("bandel", SV_BanDel_f);
	Cmd_AddCommand("exceptdel", SV_ExceptDel_f);
	Cmd_AddCommand("flushbans", SV_FlushBans_f);
}

/*
==================
SV_RemoveOperatorCommands
==================
*/
void SV_RemoveOperatorCommands(void)
{
#if 0
	// removing these won't let the server start again
	Cmd_RemoveCommand("heartbeat");
	Cmd_RemoveCommand("kick");
	Cmd_RemoveCommand("banUser");
	Cmd_RemoveCommand("banClient");
	Cmd_RemoveCommand("status");
	Cmd_RemoveCommand("serverinfo");
	Cmd_RemoveCommand("systeminfo");
	Cmd_RemoveCommand("dumpuser");
	Cmd_RemoveCommand("map_restart");
	Cmd_RemoveCommand("sectorlist");
	Cmd_RemoveCommand("say");
#endif
}