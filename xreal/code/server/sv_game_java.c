/*
===========================================================================
Copyright (C) 2009 Robert Beckebans <trebor_7@users.sourceforge.net>

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

#if defined(USE_JAVA)


#include "server.h"
#include "../qcommon/vm_java.h"


/*
typedef struct gentity_s
{
	entityState_t   s;			// communicated by server to clients
	entityShared_t  r;			// shared by both the server system and game
	playerState_t  *ps;			// NULL if not a client
} gentity_t;
*/

static int				g_numEntities;
static sharedEntity_t	g_entities[MAX_GENTITIES];
static playerState_t	g_clients[MAX_CLIENTS];


// these functions must be used instead of pointer arithmetic, because
// the game allocates gentities with private information after the server shared part
int SV_NumForGentity(sharedEntity_t * ent)
{
	int             num;

//	num = ((byte *) ent - (byte *) sv.gentities) / sv.gentitySize;
	num = ent - sv.gentities;

	return num;
}

sharedEntity_t *SV_GentityNum(int num)
{
	sharedEntity_t *ent;

//	ent = (sharedEntity_t *) ((byte *) sv.gentities + sv.gentitySize * (num));
	ent = &sv.gentities[num];	// &g_entities[num];

	return ent;
}

playerState_t  *SV_GameClientNum(int num)
{
	playerState_t  *ps;

//	ps = (playerState_t *) ((byte *) sv.gameClients + sv.gameClientSize * (num));
	ps = &sv.gameClients[num]; // &g_clients[num];

	return ps;
}

svEntity_t     *SV_SvEntityForGentity(sharedEntity_t * gEnt)
{
	if(!gEnt || gEnt->s.number < 0 || gEnt->s.number >= MAX_GENTITIES)
	{
		Com_Error(ERR_DROP, "SV_SvEntityForGentity: bad gEnt");
	}

	return &sv.svEntities[gEnt->s.number];
}

sharedEntity_t *SV_GEntityForSvEntity(svEntity_t * svEnt)
{
	int             num;

	num = svEnt - sv.svEntities;
	return SV_GentityNum(num);
}

/*
===============
SV_GameSendServerCommand

Sends a command string to a client
===============
*/
void SV_GameSendServerCommand(int clientNum, const char *text)
{
	if(clientNum == -1)
	{
		SV_SendServerCommand(NULL, "%s", text);
	}
	else
	{
		if(clientNum < 0 || clientNum >= sv_maxclients->integer)
		{
			return;
		}
		SV_SendServerCommand(svs.clients + clientNum, "%s", text);
	}
}


/*
===============
SV_GameDropClient

Disconnects the client with a message
===============
*/
void SV_GameDropClient(int clientNum, const char *reason)
{
	if(clientNum < 0 || clientNum >= sv_maxclients->integer)
	{
		return;
	}
	SV_DropClient(svs.clients + clientNum, reason);
}


/*
=================
SV_SetBrushModel

sets mins and maxs for inline bmodels
=================
*/
void SV_SetBrushModel(sharedEntity_t * ent, const char *name)
{
	clipHandle_t    h;
	vec3_t          mins, maxs;

	if(!name)
	{
		Com_Error(ERR_DROP, "SV_SetBrushModel: NULL");
	}

	if(name[0] != '*')
	{
		Com_Error(ERR_DROP, "SV_SetBrushModel: %s isn't a brush model", name);
	}


	ent->s.modelindex = atoi(name + 1);

	h = CM_InlineModel(ent->s.modelindex);
	CM_ModelBounds(h, mins, maxs);
	VectorCopy(mins, ent->r.mins);
	VectorCopy(maxs, ent->r.maxs);
	ent->r.bmodel = qtrue;

	ent->r.contents = -1;		// we don't know exactly what is in the brushes

	// Tr3B: uncommented
	//SV_LinkEntity(ent);           // FIXME: remove
}



/*
=================
SV_inPVS

Also checks portalareas so that doors block sight
=================
*/
qboolean SV_inPVS(const vec3_t p1, const vec3_t p2)
{
	int             leafnum;
	int             cluster;
	int             area1, area2;
	byte           *mask;

	leafnum = CM_PointLeafnum(p1);
	cluster = CM_LeafCluster(leafnum);
	area1 = CM_LeafArea(leafnum);
	mask = CM_ClusterPVS(cluster);

	leafnum = CM_PointLeafnum(p2);
	cluster = CM_LeafCluster(leafnum);
	area2 = CM_LeafArea(leafnum);
	if(mask && (!(mask[cluster >> 3] & (1 << (cluster & 7)))))
		return qfalse;
	if(!CM_AreasConnected(area1, area2))
		return qfalse;			// a door blocks sight
	return qtrue;
}


/*
=================
SV_inPVSIgnorePortals

Does NOT check portalareas
=================
*/
qboolean SV_inPVSIgnorePortals(const vec3_t p1, const vec3_t p2)
{
	int             leafnum;
	int             cluster;
	int             area1, area2;
	byte           *mask;

	leafnum = CM_PointLeafnum(p1);
	cluster = CM_LeafCluster(leafnum);
	area1 = CM_LeafArea(leafnum);
	mask = CM_ClusterPVS(cluster);

	leafnum = CM_PointLeafnum(p2);
	cluster = CM_LeafCluster(leafnum);
	area2 = CM_LeafArea(leafnum);

	if(mask && (!(mask[cluster >> 3] & (1 << (cluster & 7)))))
		return qfalse;

	return qtrue;
}


/*
========================
SV_AdjustAreaPortalState
========================
*/
void SV_AdjustAreaPortalState(sharedEntity_t * ent, qboolean open)
{
	svEntity_t     *svEnt;

	svEnt = SV_SvEntityForGentity(ent);
	if(svEnt->areanum2 == -1)
	{
		return;
	}
	CM_AdjustAreaPortalState(svEnt->areanum, svEnt->areanum2, open);
}


/*
==================
SV_GameAreaEntities
==================
*/
qboolean SV_EntityContact(vec3_t mins, vec3_t maxs, const sharedEntity_t * gEnt, traceType_t type)
{
	const float    *origin, *angles;
	clipHandle_t    ch;
	trace_t         trace;

	// check for exact collision
	origin = gEnt->r.currentOrigin;
	angles = gEnt->r.currentAngles;

	ch = SV_ClipHandleForEntity(gEnt);
	CM_TransformedBoxTrace(&trace, vec3_origin, vec3_origin, mins, maxs, ch, -1, origin, angles, type);

	return trace.startsolid;
}


/*
===============
SV_GetServerinfo
===============
*/
void SV_GetServerinfo(char *buffer, int bufferSize)
{
	if(bufferSize < 1)
	{
		Com_Error(ERR_DROP, "SV_GetServerinfo: bufferSize == %i", bufferSize);
	}
	Q_strncpyz(buffer, Cvar_InfoString(CVAR_SERVERINFO), bufferSize);
}




/*
===============
SV_GetUsercmd
===============
*/
void SV_GetUsercmd(int clientNum, usercmd_t * cmd)
{
	if(clientNum < 0 || clientNum >= sv_maxclients->integer)
	{
		Com_Error(ERR_DROP, "SV_GetUsercmd: bad clientNum:%i", clientNum);
	}
	*cmd = svs.clients[clientNum].lastUsercmd;
}

//==============================================

static int FloatAsInt(float f)
{
	floatint_t      fi;

	fi.f = f;
	return fi.i;
}

/*
====================
SV_GameSystemCalls

The module is making a system call
====================
*/
/*
intptr_t SV_GameSystemCalls(intptr_t * args)
{
	switch (args[0])
	{
		case G_PRINT:
			Com_Printf("%s", (const char *)VMA(1));
			return 0;
		case G_ERROR:
			Com_Error(ERR_DROP, "%s", (const char *)VMA(1));
			return 0;
		case G_MILLISECONDS:
			return Sys_Milliseconds();
		case G_CVAR_REGISTER:
			Cvar_Register(VMA(1), VMA(2), VMA(3), args[4]);
			return 0;
		case G_CVAR_UPDATE:
			Cvar_Update(VMA(1));
			return 0;
		case G_CVAR_SET:
			Cvar_Set((const char *)VMA(1), (const char *)VMA(2));
			return 0;
		case G_CVAR_VARIABLE_INTEGER_VALUE:
			return Cvar_VariableIntegerValue((const char *)VMA(1));
		case G_CVAR_VARIABLE_STRING_BUFFER:
			Cvar_VariableStringBuffer(VMA(1), VMA(2), args[3]);
			return 0;
		case G_ARGC:
			return Cmd_Argc();
		case G_ARGV:
			Cmd_ArgvBuffer(args[1], VMA(2), args[3]);
			return 0;
		case G_SEND_CONSOLE_COMMAND:
			Cbuf_ExecuteText(args[1], VMA(2));
			return 0;

		case G_FS_FOPEN_FILE:
			return FS_FOpenFileByMode(VMA(1), VMA(2), args[3]);
		case G_FS_READ:
			FS_Read2(VMA(1), args[2], args[3]);
			return 0;
		case G_FS_WRITE:
			FS_Write(VMA(1), args[2], args[3]);
			return 0;
		case G_FS_FCLOSE_FILE:
			FS_FCloseFile(args[1]);
			return 0;
		case G_FS_GETFILELIST:
			return FS_GetFileList(VMA(1), VMA(2), VMA(3), args[4]);
		case G_FS_SEEK:
			return FS_Seek(args[1], args[2], args[3]);

		case G_LOCATE_GAME_DATA:
			SV_LocateGameData(VMA(1), args[2], args[3], VMA(4), args[5]);
			return 0;
		case G_DROP_CLIENT:
			SV_GameDropClient(args[1], VMA(2));
			return 0;
		case G_SEND_SERVER_COMMAND:
			SV_GameSendServerCommand(args[1], VMA(2));
			return 0;
		case G_LINKENTITY:
			SV_LinkEntity(VMA(1));
			return 0;
		case G_UNLINKENTITY:
			SV_UnlinkEntity(VMA(1));
			return 0;
		case G_ENTITIES_IN_BOX:
			return SV_AreaEntities(VMA(1), VMA(2), VMA(3), args[4]);
		case G_ENTITY_CONTACT:
			return SV_EntityContact(VMA(1), VMA(2), VMA(3), TT_AABB);
		case G_ENTITY_CONTACTCAPSULE:
			return SV_EntityContact(VMA(1), VMA(2), VMA(3), TT_CAPSULE);
		case G_TRACE:
			SV_Trace(VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7], TT_AABB);
			return 0;
		case G_TRACECAPSULE:
			SV_Trace(VMA(1), VMA(2), VMA(3), VMA(4), VMA(5), args[6], args[7], TT_CAPSULE);
			return 0;
		case G_POINT_CONTENTS:
			return SV_PointContents(VMA(1), args[2]);
		case G_SET_BRUSH_MODEL:
			SV_SetBrushModel(VMA(1), VMA(2));
			return 0;
		case G_IN_PVS:
			return SV_inPVS(VMA(1), VMA(2));
		case G_IN_PVS_IGNORE_PORTALS:
			return SV_inPVSIgnorePortals(VMA(1), VMA(2));

		case G_SET_CONFIGSTRING:
			SV_SetConfigstring(args[1], VMA(2));
			return 0;
		case G_GET_CONFIGSTRING:
			SV_GetConfigstring(args[1], VMA(2), args[3]);
			return 0;
		case G_SET_USERINFO:
			SV_SetUserinfo(args[1], VMA(2));
			return 0;
		case G_GET_USERINFO:
			SV_GetUserinfo(args[1], VMA(2), args[3]);
			return 0;
		case G_GET_SERVERINFO:
			SV_GetServerinfo(VMA(1), args[2]);
			return 0;
		case G_ADJUST_AREA_PORTAL_STATE:
			SV_AdjustAreaPortalState(VMA(1), args[2]);
			return 0;
		case G_AREAS_CONNECTED:
			return CM_AreasConnected(args[1], args[2]);

		case G_BOT_ALLOCATE_CLIENT:
			return SV_BotAllocateClient();
		case G_BOT_FREE_CLIENT:
			SV_BotFreeClient(args[1]);
			return 0;

		case G_GET_USERCMD:
			SV_GetUsercmd(args[1], VMA(2));
			return 0;
		case G_GET_ENTITY_TOKEN:
		{
			const char     *s;

			s = Com_Parse(&sv.entityParsePoint);
			Q_strncpyz(VMA(1), s, args[2]);
			if(!sv.entityParsePoint && !s[0])
			{
				return qfalse;
			}
			else
			{
				return qtrue;
			}
		}

		case G_REAL_TIME:
			return Com_RealTime(VMA(1));

			//====================================

		case BOTLIB_GET_SNAPSHOT_ENTITY:
			return SV_BotGetSnapshotEntity(args[1], args[2]);
		case BOTLIB_GET_CONSOLE_MESSAGE:
			return SV_BotGetConsoleMessage(args[1], VMA(2), args[3]);
		case BOTLIB_USER_COMMAND:
			SV_ClientThink(&svs.clients[args[1]], VMA(2));
			return 0;
		case BOTLIB_CLIENT_COMMAND:
			SV_BotClientCommand(args[1], VMA(2));
			return 0;

		case TRAP_MEMSET:
			Com_Memset(VMA(1), args[2], args[3]);
			return 0;

		case TRAP_MEMCPY:
			Com_Memcpy(VMA(1), VMA(2), args[3]);
			return 0;

		case TRAP_STRNCPY:
			strncpy(VMA(1), VMA(2), args[3]);
			return args[1];

		case TRAP_SIN:
			return FloatAsInt(sin(VMF(1)));

		case TRAP_COS:
			return FloatAsInt(cos(VMF(1)));

		case TRAP_ATAN2:
			return FloatAsInt(atan2(VMF(1), VMF(2)));

		case TRAP_SQRT:
			return FloatAsInt(sqrt(VMF(1)));

		case TRAP_MATRIXMULTIPLY:
			MatrixMultiply(VMA(1), VMA(2), VMA(3));
			return 0;

		case TRAP_ANGLEVECTORS:
			AngleVectors(VMA(1), VMA(2), VMA(3), VMA(4));
			return 0;

		case TRAP_PERPENDICULARVECTOR:
			PerpendicularVector(VMA(1), VMA(2));
			return 0;

		case TRAP_FLOOR:
			return FloatAsInt(floor(VMF(1)));

		case TRAP_CEIL:
			return FloatAsInt(ceil(VMF(1)));


		default:
			Com_Error(ERR_DROP, "Bad game system trap: %ld", (long int)args[0]);
	}
	return -1;
}
*/


// ====================================================================================



// handle to Game class
static jclass   class_Game = NULL;
static jobject  object_Game = NULL;
static jclass   interface_GameListener;
static jmethodID method_Game_ctor;
static jmethodID method_Game_initGame;
static jmethodID method_Game_shutdownGame;
static jmethodID method_Game_clientConnect;
static jmethodID method_Game_clientBegin;
static jmethodID method_Game_clientUserInfoChanged;
static jmethodID method_Game_clientDisconnect;
static jmethodID method_Game_clientCommand;
static jmethodID method_Game_clientThink;
static jmethodID method_Game_runFrame;
static jmethodID method_Game_runAIFrame;
static jmethodID method_Game_consoleCommand;

void Game_javaRegister()
{
	Com_DPrintf("Game_javaRegister()\n");

	// load the interface GameListener
	interface_GameListener = (*javaEnv)->FindClass(javaEnv, "xreal/game/GameListener");
	if(CheckException() || !interface_GameListener)
	{
		Com_Error(ERR_DROP, "Couldn't find class xreal.game.GameListener");
	}

	// load the class Game
	class_Game = (*javaEnv)->FindClass(javaEnv, "xreal/game/Game");
	if(CheckException() || !class_Game)
	{
		Com_Error(ERR_DROP, "Couldn't find class xreal.game.Game");
	}

	// check class Game against interface GameListener
	if(!((*javaEnv)->IsAssignableFrom(javaEnv, class_Game, interface_GameListener)))
	{
		Com_Error(ERR_DROP, "The specified game class doesn't implement xreal.game.GameListener");
	}

	// remove old game if existing
	(*javaEnv)->DeleteLocalRef(javaEnv, interface_GameListener);

	// load game interface methods
	method_Game_initGame = (*javaEnv)->GetMethodID(javaEnv, class_Game, "initGame", "(IIZ)V");
	method_Game_shutdownGame = (*javaEnv)->GetMethodID(javaEnv, class_Game, "shutdownGame", "(Z)V");
	method_Game_clientConnect = (*javaEnv)->GetMethodID(javaEnv, class_Game, "clientConnect", "(IZZ)Ljava/lang/String;");
	method_Game_clientBegin = (*javaEnv)->GetMethodID(javaEnv, class_Game, "clientBegin", "(I)V");
	method_Game_clientUserInfoChanged = (*javaEnv)->GetMethodID(javaEnv, class_Game, "clientUserInfoChanged", "(I)V");
	method_Game_clientDisconnect = (*javaEnv)->GetMethodID(javaEnv, class_Game, "clientDisconnect", "(I)V");
	method_Game_clientCommand = (*javaEnv)->GetMethodID(javaEnv, class_Game, "clientCommand", "(I)V");
	method_Game_clientThink = (*javaEnv)->GetMethodID(javaEnv, class_Game, "clientThink", "(I)V");
	method_Game_runFrame = (*javaEnv)->GetMethodID(javaEnv, class_Game, "runFrame", "(I)V");
	method_Game_runAIFrame = (*javaEnv)->GetMethodID(javaEnv, class_Game, "runAIFrame", "(I)V");
	method_Game_consoleCommand = (*javaEnv)->GetMethodID(javaEnv, class_Game, "consoleCommand", "()Z");
	if(CheckException())
	{
		Com_Error(ERR_DROP, "Problem getting handle for one or more of the game methods\n");
	}

	// load constructor
	method_Game_ctor = (*javaEnv)->GetMethodID(javaEnv, class_Game, "<init>", "()V");

	object_Game = (*javaEnv)->NewObject(javaEnv, class_Game, method_Game_ctor);
	if(CheckException())
	{
		Com_Error(ERR_DROP, "Couldn't create instance of game object");
	}
}


void Game_javaDetach()
{
	Com_DPrintf("Game_javaDetach()\n");

	if(javaEnv)
	{
		if(class_Game)
		{
			(*javaEnv)->DeleteLocalRef(javaEnv, class_Game);
			class_Game = NULL;
		}

		if(object_Game)
		{
			(*javaEnv)->DeleteLocalRef(javaEnv, object_Game);
			object_Game = NULL;
		}
	}
}


/*
===============
SV_ShutdownGameProgs

Called every time a map changes
===============
*/
void SV_ShutdownGameProgs(void)
{
	if(!javaEnv)
	{
		Com_Printf("Can't stop Java VM, javaEnv pointer was null\n");
		return;
	}

	Java_G_ShutdownGame(qfalse);

	Game_javaDetach();
}



void Java_G_GameInit(int levelTime, int randomSeed, qboolean restart)
{
#if 1
	if(!object_Game)
		return;

	(*javaEnv)->CallVoidMethod(javaEnv, object_Game, method_Game_initGame, levelTime, randomSeed, restart);

	CheckException();
#endif
}

void Java_G_ShutdownGame(qboolean restart)
{
#if 1
	if(!object_Game)
		return;

	(*javaEnv)->CallVoidMethod(javaEnv, object_Game, method_Game_shutdownGame, restart);

	CheckException();
#endif
}

char           *Java_G_ClientConnect(int clientNum, qboolean firstTime, qboolean isBot)
{
#if 1
	static char	string[MAX_STRING_CHARS];
	jobject 	result;

	if(!object_Game)
		return NULL;

	result = (*javaEnv)->CallObjectMethod(javaEnv, object_Game, method_Game_clientConnect, firstTime, isBot);

	CheckException();

	if(result == NULL)
		return NULL;

	ConvertJavaString(string, result, sizeof(string));

	CheckException();

	//Com_Printf("Java_G_ClientConnect '%s'\n", string);

	return string;
#else
	return NULL;
#endif
}

void Java_G_ClientBegin(int clientNum)
{
	if(!object_Game)
		return;

	(*javaEnv)->CallVoidMethod(javaEnv, object_Game, method_Game_clientBegin, clientNum);

	CheckException();
}

void Java_G_ClientUserInfoChanged(int clientNum)
{
	if(!object_Game)
		return;

	(*javaEnv)->CallVoidMethod(javaEnv, object_Game, method_Game_clientUserInfoChanged, clientNum);

	CheckException();
}

void Java_G_ClientDisconnect(int clientNum)
{
	if(!object_Game)
		return;

	(*javaEnv)->CallVoidMethod(javaEnv, object_Game, method_Game_clientDisconnect, clientNum);

	CheckException();
}

void Java_G_ClientCommand(int clientNum)
{
	if(!object_Game)
			return;

	(*javaEnv)->CallVoidMethod(javaEnv, object_Game, method_Game_clientCommand, clientNum);

	CheckException();
}

void Java_G_ClientThink(int clientNum)
{
	if(!object_Game)
		return;

	(*javaEnv)->CallVoidMethod(javaEnv, object_Game, method_Game_clientThink, clientNum);

	CheckException();
}

void Java_G_RunFrame(int time)
{
	if(!object_Game)
		return;

	(*javaEnv)->CallVoidMethod(javaEnv, object_Game, method_Game_runFrame, time);

	CheckException();
}

void Java_G_RunAIFrame(int time)
{
	if(!object_Game)
		return;

	(*javaEnv)->CallVoidMethod(javaEnv, object_Game, method_Game_runAIFrame, time);

	CheckException();
}

qboolean Java_G_ConsoleCommand(void)
{
	jboolean result;

	if(!object_Game)
		return qfalse;

	result = (*javaEnv)->CallBooleanMethod(javaEnv, object_Game, method_Game_consoleCommand);

	CheckException();

	return result;
}

/*
==================
SV_InitGameVM

Called for both a full init and a restart
==================
*/
static void SV_InitGameVM(qboolean restart)
{
	int             i;

	Com_DPrintf("SV_InitGameVM(restart = %i)\n", restart);

	// start the entity parsing at the beginning
	sv.entityParsePoint = CM_EntityString();

	// clear all gentity pointers that might still be set from
	// a previous level
	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=522
	//   now done before GAME_INIT call
	for(i = 0; i < sv_maxclients->integer; i++)
	{
		svs.clients[i].gentity = NULL;
	}

	// initialize all entities for this game
	memset(g_entities, 0, MAX_GENTITIES * sizeof(g_entities[0]));
	sv.gentities = g_entities;

	// initialize all clients for this game
//	level.maxclients = g_maxclients.integer;
	memset(g_clients, 0, MAX_CLIENTS * sizeof(g_clients[0]));
	sv.gameClients = g_clients;

	// set client fields on player ents
	/*
	for(i = 0; i < sv_maxclients->integer; i++)
	{
		g_entities[i].ps = g_clients + i;
	}
	*/

	// always leave room for the max number of clients,
	// even if they aren't all used, so numbers inside that
	// range are NEVER anything but clients
	g_numEntities = MAX_CLIENTS;
	sv.num_entities = g_numEntities;

	// use the current msec count for a random seed
	// init for this gamestate
	Java_G_GameInit(sv.time, Com_Milliseconds(), restart);
}



/*
===================
SV_RestartGameProgs

Called on a map_restart, but not on a normal map change
===================
*/
void SV_RestartGameProgs(void)
{
	Com_DPrintf("SV_RestartGameProgs()\n");

	if(!javaEnv)
	{
		return;
	}

	Java_G_ShutdownGame(qtrue);
	SV_InitGameVM(qtrue);
}




/*
===============
SV_InitGameProgs

Called on a normal map change, not on a map_restart
===============
*/
void SV_InitGameProgs(void)
{
	Com_DPrintf("SV_InitGameProgs()\n");

	Game_javaRegister();

	SV_InitGameVM(qfalse);
}


/*
====================
SV_GameCommand

See if the current console command is claimed by the game
====================
*/
qboolean SV_GameCommand(void)
{
	if(sv.state != SS_GAME)
	{
		return qfalse;
	}

	return Java_G_ConsoleCommand();
}

#endif							//defined(USE_JAVA
