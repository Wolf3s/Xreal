/// ============================================================================
/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2003 Robert Beckebans <trebor_7@users.sourceforge.net>
Copyright (C) 2003, 2004  contributors of the XreaL project
Please see the file "AUTHORS" for a list of contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
/// ============================================================================


/// includes ===================================================================
// system -------------------------------------------------------------------
// xreal --------------------------------------------------------------------
#include "cg_local.h"





//
// cvars
//
cvar_t	*cg_gun;
cvar_t	*cg_footsteps;
cvar_t	*cg_crosshair;
cvar_t	*cg_crosshair_size;
cvar_t	*cg_stats;
cvar_t	*cg_vwep;
cvar_t	*cg_noskins;
cvar_t	*cg_showclamp;
cvar_t	*cg_shownet;
cvar_t	*cg_predict;
cvar_t	*cg_showmiss;
cvar_t	*cg_viewsize;
cvar_t	*cg_centertime;
cvar_t	*cg_showturtle;
cvar_t	*cg_showfps;
cvar_t	*cg_showlayout;
cvar_t	*cg_printspeed;
cvar_t	*cg_paused;
cvar_t	*cg_gravity;


//
// interface
//
cg_import_t	cgi;
cg_export_t	cg_globals;


cg_state_t	cg;
cg_static_t	cgs;


d_world_c*		cg_ode_world;
d_space_c*		cg_ode_space;
d_joint_group_c*	cg_ode_contact_group;




void	CG_ParseLayout(bitmessage_c &msg)
{
	const char *s = msg.readString();
	
	strncpy(cg.layout, s, sizeof(cg.layout)-1);
}


/*
=================
CL_Skins_f

Load or download any custom player skins and models
=================
*/
/*
static void	CG_Skins_f()
{
	for(int i=0; i<MAX_CLIENTS; i++)
	{
		if(!cgi.CL_GetConfigString(CS_PLAYERSKINS+i)[0])
			continue;
			
		Com_Printf("client %i: %s\n", i, cgi.CL_GetConfigString(CS_PLAYERSKINS+i));
		CG_UpdateScreen();
		cgi.Sys_SendKeyEvents();	// pump message loop
		CG_ParseClientinfo(i);
	}
}
*/


/*
=================
CL_Snd_Restart_f

Restart the sound subsystem so it can pick up
new parameters and flush all sounds
=================
*/
static void	CG_Snd_Restart_f()
{
	cgi.S_Shutdown();
	cgi.S_Init();
	CG_RegisterSounds();
}



static void	CG_ClearEntities()
{
	// clear all entities
	memset(&cg.entities, 0, sizeof(cg.entities));
	memset(&cg.entities_parse, 0, sizeof(cg.entities_parse));
	cg.entities_first		= 0;		// index (not anded off) into cl_parse_entities[]
}


void	CG_ClearState()
{
	cg.frame_lerp			= 0;			// between oldframe and frame
	
	for(int i=0; i<CMD_BACKUP; i++)
		cg.predicted_origins[i].clear();
	
	cg.predicted_step		= 0;				// for stair up smoothing
	cg.predicted_step_time		= 0;

	cg.predicted_origin.clear();	// generated by CL_PredictMovement
	cg.predicted_angles.clear();
	cg.prediction_error.clear();
	
	
	cg.refdef.clear();

	cg.v_blend.clear();
	
	cg.v_forward.clear();
	cg.v_right.clear();
	cg.v_up.clear();


	CG_ClearEntities();

	CG_ClearParticles();
	//CG_ClearLights();
	CG_ClearTEnts();
	
	for(int i=0; i<MAX_CLIENTS; i++)
		cg.clientinfo[i].clear();
		
	cg.baseclientinfo.clear();
	
	
	memset(cg.layout, 0, sizeof(cg.layout));
	memset(cg.inventory, 0, sizeof(cg.inventory));
	
	
	memset(cg.model_draw, -1, sizeof(cg.model_draw));
	memset(cg.model_clip, 0, sizeof(cg.model_clip));

	memset(cg.shader_precache, -1, sizeof(cg.shader_precache));
	memset(cg.animation_precache, -1, sizeof(cg.animation_precache));
	memset(cg.sound_precache, -1, sizeof(cg.sound_precache));
	memset(cg.light_precache, -1, sizeof(cg.light_precache));
}

void	CG_RegisterSounds()
{
	cgi.S_BeginRegistration();
	
	CG_RegisterTEntSounds();
	
	for(int i=1; i<MAX_SOUNDS; i++)
	{
		if(!cgi.CL_GetConfigString(CS_SOUNDS+i)[0])
			break;
			
		cg.sound_precache[i] = cgi.S_RegisterSound(cgi.CL_GetConfigString(CS_SOUNDS+i));
	}
	
	cgi.S_EndRegistration();
}



static void	CG_InitClientGame()
{
	//
	// register our variables
	//
	cg_gun 			= cgi.Cvar_Get("cg_gun", "1", CVAR_ARCHIVE);
	cg_footsteps		= cgi.Cvar_Get("cg_footsteps", "1", CVAR_NONE);
	cg_crosshair 		= cgi.Cvar_Get("cg_crosshair", "0", CVAR_ARCHIVE);
	cg_crosshair_size 	= cgi.Cvar_Get("cg_crosshair_size", "24", CVAR_ARCHIVE);
	cg_stats 		= cgi.Cvar_Get("cg_stats", "0", 0);
	cg_vwep			= cgi.Cvar_Get("cg_vwep", "1", CVAR_ARCHIVE);
	cg_noskins		= cgi.Cvar_Get("cg_noskins", "0", 0);
	cg_showclamp		= cgi.Cvar_Get("showclamp", "0", 0);
	cg_shownet		= cgi.Cvar_Get("shownet", "0", 0);
	cg_predict		= cgi.Cvar_Get("cg_predict", "0", 0);
	cg_showmiss		= cgi.Cvar_Get("cg_showmiss", "0", 0);
	cg_viewsize		= cgi.Cvar_Get("cg_viewsize", "100", CVAR_ARCHIVE);
	cg_centertime		= cgi.Cvar_Get("cg_centertime", "2.5", 0);
	cg_showturtle		= cgi.Cvar_Get("cg_showturtle", "0", 0);
	cg_showfps		= cgi.Cvar_Get("cg_showfps", "1", CVAR_ARCHIVE);
	cg_showlayout		= cgi.Cvar_Get("cg_showlayout", "1", CVAR_ARCHIVE);
	cg_printspeed		= cgi.Cvar_Get("cg_printspeed", "8", CVAR_NONE);
	cg_paused		= cgi.Cvar_Get("paused", "0", CVAR_NONE);
	cg_gravity		= cgi.Cvar_Get("cg_gravity", "1", CVAR_NONE);
	
//	cgi.Cmd_AddCommand("skins", 		CG_Skins_f);
	cgi.Cmd_AddCommand("snd_restart",	CG_Snd_Restart_f);


	//
	// initialize subsystems
	//
	CG_InitScreen();
	
	CG_InitDynamics();
	
	CG_InitView();
	
	CG_InitWeapon();
	
	//CG_InitEntities();
	
	
	//
	// create new dynamic lists
	//
	/*
	for(int i=0; i<MAX_EDICTS; i++)
	{
		centity_t *ent = new centity_t;
		memset(ent, 0, sizeof(*ent));
		cl_entities.pushBack(ent);
	}
	
	for(int i=0; i<MAX_PARSE_EDICTS; i++)
	{
		entity_state_t *ent = new entity_state_t;
		memset(ent, 0, sizeof(*ent));
		cl_parse_entities.pushBack(ent);
	}
	*/
}

static void	CG_ShutdownClientGame()
{
	CG_ShutdownDynamics();
}


static void	CG_RunFrame()
{
	// predict all unacknowledged movements
	CG_PredictMovement();

	CG_CalcVrect();
	
	CG_TileClear();

	CG_RenderView();
	
	// update audio
	cgi.S_Update(cg.refdef.view_origin, cg.v_velocity, cg.v_forward, cg.v_right, cg.v_up);
}


static void	CG_UpdateConfig(int index, const std::string &configstring)
{
	//cgi.Com_Printf("CG_UpdateConfig: %i '%s'\n", index, configstring.c_str());

	if(index == CS_MAPNAME)
	{
		std::string mapname = cgi.CL_GetConfigString(CS_MAPNAME);
		
		cgi.Com_Printf("CG_UpdateConfig: map '%s'\n", mapname.c_str());
		
		unsigned	map_checksum;		// for detecting cheater maps
		cgi.CM_BeginRegistration(mapname, true, &map_checksum, 0);
		
		if((int)map_checksum != atoi(cgi.CL_GetConfigString(CS_MAPCHECKSUM)))
		{
			cgi.Com_Error(ERR_DROP, "Local map version differs from server: %i != '%s'\n", map_checksum, cgi.CL_GetConfigString(CS_MAPCHECKSUM));
			return;
		}
		cgi.R_BeginRegistration(mapname);
	}
	else if(index == CS_SKY)
	{
		cgi.R_SetSky(cgi.CL_GetConfigString(CS_SKY));
	}
	else if(index >= CS_MODELS && index < CS_MODELS+MAX_MODELS)
	{
		//if(cgi.CL_GetRefreshPrepped())
		{
			cg.model_draw[index-CS_MODELS] = cgi.R_RegisterModel(configstring);
			
			cg.model_clip[index-CS_MODELS] = cgi.CM_RegisterModel(configstring);
		}
	}
	else if(index >= CS_SHADERS && index < CS_SHADERS+MAX_SHADERS)
	{
		//if(cgi.CL_GetRefreshPrepped())
			cg.shader_precache[index-CS_SHADERS] = cgi.R_RegisterPic(configstring);
	}
	else if(index >= CS_ANIMATIONS && index < CS_ANIMATIONS+MAX_ANIMATIONS)
	{
		//if(cgi.CL_GetRefreshPrepped())
			cg.animation_precache[index-CS_ANIMATIONS] = cgi.R_RegisterAnimation(configstring);
	}
	else if(index >= CS_SOUNDS && index < CS_SOUNDS+MAX_SOUNDS)
	{
		//if(cgi.CL_GetRefreshPrepped())
			cg.sound_precache[index-CS_SOUNDS] = cgi.S_RegisterSound(configstring);
	}
	else if(index >= CS_LIGHTS && index < CS_LIGHTS+MAX_LIGHTS)
	{
		//if(cgi.CL_GetRefreshPrepped())
			cg.light_precache[index-CS_LIGHTS] = cgi.R_RegisterLight(configstring);
	}
	else if(index >= CS_PLAYERSKINS && index < CS_PLAYERSKINS+MAX_CLIENTS)
	{
		//if(cgi.CL_GetRefreshPrepped())
			CG_ParseClientinfo(index-CS_PLAYERSKINS);
	}
}



/*
=================
GetCCameAPI

Returns a pointer to the structure with all entry points
and global variables
=================
*/

#ifdef __cplusplus
extern "C" {
#endif

cg_export_t*	GetCGameAPI(cg_import_t *import)
{
	cgi = *import;

	cg_globals.apiversion			= CG_API_VERSION;

	cg_globals.Init				= CG_InitClientGame;
	cg_globals.Shutdown			= CG_ShutdownClientGame;
	
	cg_globals.CG_BeginFrame		= CG_BeginFrame;
	cg_globals.CG_AddEntity			= CG_AddEntity;
	cg_globals.CG_UpdateEntity		= CG_UpdateEntity;
	cg_globals.CG_RemoveEntity		= CG_RemoveEntity;
	cg_globals.CG_EndFrame			= CG_EndFrame;
	
	cg_globals.CG_RunFrame			= CG_RunFrame;
	cg_globals.CG_UpdateConfig		= CG_UpdateConfig;
	
	cg_globals.CG_ParseLayout		= CG_ParseLayout;
	cg_globals.CG_ParseTEnt			= CG_ParseTEnt;
	cg_globals.CG_ParseMuzzleFlash		= CG_ParseMuzzleFlash;
	cg_globals.CG_PrepRefresh		= CG_PrepRefresh;
	cg_globals.CG_GetEntitySoundOrigin	= CG_GetEntitySoundOrigin;
	
	cg_globals.CG_ClearState		= CG_ClearState;
			
	cg_globals.CG_ParseInventory		= CG_ParseInventory;
	
	cg_globals.CG_DrawChar			= CG_DrawChar;
	cg_globals.CG_DrawString		= CG_DrawString;
	
	cg_globals.CG_CenterPrint		= CG_CenterPrint;
			
	cg_globals.CG_ParseClientinfo		= CG_ParseClientinfo;
	
	Swap_Init();

	return &cg_globals;
}


#ifdef __cplusplus
}
#endif



#ifndef CGAME_HARD_LINKED
// this is only here so the functions in q_shared.c and q_shwin.c can link
void 	Com_Error(err_type_e type, const char *fmt, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start(argptr, fmt);
	vsprintf(text, fmt, argptr);
	va_end(argptr);

	cgi.Com_Error(type, "%s", text);
}

void 	Com_Printf(const char *fmt, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start(argptr, fmt);
	vsprintf(text, fmt, argptr);
	va_end(argptr);

	cgi.Com_Printf("%s", text);
}
#endif



