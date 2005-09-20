/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "tr_local.h"

// tr_shader.c -- this file deals with the parsing and definition of shaders

static char    *s_shaderText;

// the shader is parsed into these global variables, then copied into
// dynamically allocated memory if it is valid.
static shaderStage_t	stages[MAX_SHADER_STAGES];
static shader_t			shader;
static texModInfo_t		texMods[MAX_SHADER_STAGES][TR_MAX_TEXMODS];
static qboolean			deferLoad;

#define FILE_HASH_SIZE		1024
static shader_t		   *shaderHashTable[FILE_HASH_SIZE];

#define MAX_SHADERTEXT_HASH		2048
static char  		  **shaderTextHashTable[MAX_SHADERTEXT_HASH];

/*
================
return a hash value for the filename
================
*/
static long generateHashValue(const char *fname, const int size)
{
	int             i;
//	int             len;
	long            hash;
	char            letter;

	hash = 0;
	i = 0;
//	len = strlen(fname);
	
	while(fname[i] != '\0')
//	for(i = 0; i < len; i++)
	{
		letter = tolower(fname[i]);
		
		if(letter == '.')
			break;				// don't include extension
		
		if(letter == '\\')
			letter = '/';		// damn path names
		
		if(letter == PATH_SEP)
			letter = '/';		// damn path names
		
		hash += (long)(letter) * (i + 119);
		i++;
	}
	hash = (hash ^ (hash >> 10) ^ (hash >> 20));
	hash &= (size - 1);
	return hash;
}

void R_RemapShader(const char *shaderName, const char *newShaderName, const char *timeOffset)
{
	char            strippedName[MAX_QPATH];
	int             hash;
	shader_t       *sh, *sh2;
	qhandle_t       h;

	sh = R_FindShaderByName(shaderName);
	if(sh == NULL || sh == tr.defaultShader)
	{
		h = RE_RegisterShaderLightMap(shaderName, 0);
		sh = R_GetShaderByHandle(h);
	}
	if(sh == NULL || sh == tr.defaultShader)
	{
		ri.Printf(PRINT_WARNING, "WARNING: R_RemapShader: shader %s not found\n", shaderName);
		return;
	}

	sh2 = R_FindShaderByName(newShaderName);
	if(sh2 == NULL || sh2 == tr.defaultShader)
	{
		h = RE_RegisterShaderLightMap(newShaderName, 0);
		sh2 = R_GetShaderByHandle(h);
	}

	if(sh2 == NULL || sh2 == tr.defaultShader)
	{
		ri.Printf(PRINT_WARNING, "WARNING: R_RemapShader: new shader %s not found\n", newShaderName);
		return;
	}

	// remap all the shaders with the given name
	// even tho they might have different lightmaps
	COM_StripExtension(shaderName, strippedName);
	hash = generateHashValue(strippedName, FILE_HASH_SIZE);
	for(sh = shaderHashTable[hash]; sh; sh = sh->next)
	{
		if(Q_stricmp(sh->name, strippedName) == 0)
		{
			if(sh != sh2)
			{
				sh->remappedShader = sh2;
			}
			else
			{
				sh->remappedShader = NULL;
			}
		}
	}
	if(timeOffset)
	{
		sh2->timeOffset = atof(timeOffset);
	}
}

/*
===============
ParseVector
===============
*/
static qboolean ParseVector(char **text, int count, float *v)
{
	char           *token;
	int             i;

	token = COM_ParseExt(text, qfalse);
	if(strcmp(token, "("))
	{
		ri.Printf(PRINT_WARNING, "WARNING: missing parenthesis in shader '%s'\n", shader.name);
		return qfalse;
	}

	for(i = 0; i < count; i++)
	{
		token = COM_ParseExt(text, qfalse);
		if(!token[0])
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing vector element in shader '%s'\n", shader.name);
			return qfalse;
		}
		v[i] = atof(token);
	}

	token = COM_ParseExt(text, qfalse);
	if(strcmp(token, ")"))
	{
		ri.Printf(PRINT_WARNING, "WARNING: missing parenthesis in shader '%s'\n", shader.name);
		return qfalse;
	}

	return qtrue;
}


/*
===============
ParseExpression
===============
*/
static void ParseExpression(char **text, expression_t *exp)
{
	char           *token;
	char           *p;
	int             c;

	token = COM_ParseExt(text, qfalse);
	if(token[0] == 0)
	{
		ri.Printf(PRINT_WARNING, "WARNING: missing expression parameter in shader '%s'\n", shader.name);
//		return;
	}
	
	// TODO: write a simple lexer
//	SkipRestOfLine(text);
	
	p = *text;
	while((c = *p++) != 0)
	{
		if(c == '\n' || c == ',')
		{
			break;
		}
	}

	*text = p;
}


/*
===============
NameToAFunc
===============
*/
static unsigned NameToAFunc(const char *funcname)
{
	if(!Q_stricmp(funcname, "GT0"))
	{
		return GLS_ATEST_GT_0;
	}
	else if(!Q_stricmp(funcname, "LT128"))
	{
		return GLS_ATEST_LT_80;
	}
	else if(!Q_stricmp(funcname, "GE128"))
	{
		return GLS_ATEST_GE_80;
	}

	ri.Printf(PRINT_WARNING, "WARNING: invalid alphaFunc name '%s' in shader '%s'\n", funcname, shader.name);
	return 0;
}


/*
===============
NameToSrcBlendMode
===============
*/
static int NameToSrcBlendMode(const char *name)
{
	if(!Q_stricmp(name, "GL_ONE"))
	{
		return GLS_SRCBLEND_ONE;
	}
	else if(!Q_stricmp(name, "GL_ZERO"))
	{
		return GLS_SRCBLEND_ZERO;
	}
	else if(!Q_stricmp(name, "GL_DST_COLOR"))
	{
		return GLS_SRCBLEND_DST_COLOR;
	}
	else if(!Q_stricmp(name, "GL_ONE_MINUS_DST_COLOR"))
	{
		return GLS_SRCBLEND_ONE_MINUS_DST_COLOR;
	}
	else if(!Q_stricmp(name, "GL_SRC_ALPHA"))
	{
		return GLS_SRCBLEND_SRC_ALPHA;
	}
	else if(!Q_stricmp(name, "GL_ONE_MINUS_SRC_ALPHA"))
	{
		return GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else if(!Q_stricmp(name, "GL_DST_ALPHA"))
	{
		return GLS_SRCBLEND_DST_ALPHA;
	}
	else if(!Q_stricmp(name, "GL_ONE_MINUS_DST_ALPHA"))
	{
		return GLS_SRCBLEND_ONE_MINUS_DST_ALPHA;
	}
	else if(!Q_stricmp(name, "GL_SRC_ALPHA_SATURATE"))
	{
		return GLS_SRCBLEND_ALPHA_SATURATE;
	}

	ri.Printf(PRINT_WARNING, "WARNING: unknown blend mode '%s' in shader '%s', substituting GL_ONE\n", name,
			  shader.name);
	return GLS_SRCBLEND_ONE;
}

/*
===============
NameToDstBlendMode
===============
*/
static int NameToDstBlendMode(const char *name)
{
	if(!Q_stricmp(name, "GL_ONE"))
	{
		return GLS_DSTBLEND_ONE;
	}
	else if(!Q_stricmp(name, "GL_ZERO"))
	{
		return GLS_DSTBLEND_ZERO;
	}
	else if(!Q_stricmp(name, "GL_SRC_ALPHA"))
	{
		return GLS_DSTBLEND_SRC_ALPHA;
	}
	else if(!Q_stricmp(name, "GL_ONE_MINUS_SRC_ALPHA"))
	{
		return GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else if(!Q_stricmp(name, "GL_DST_ALPHA"))
	{
		return GLS_DSTBLEND_DST_ALPHA;
	}
	else if(!Q_stricmp(name, "GL_ONE_MINUS_DST_ALPHA"))
	{
		return GLS_DSTBLEND_ONE_MINUS_DST_ALPHA;
	}
	else if(!Q_stricmp(name, "GL_SRC_COLOR"))
	{
		return GLS_DSTBLEND_SRC_COLOR;
	}
	else if(!Q_stricmp(name, "GL_ONE_MINUS_SRC_COLOR"))
	{
		return GLS_DSTBLEND_ONE_MINUS_SRC_COLOR;
	}

	ri.Printf(PRINT_WARNING, "WARNING: unknown blend mode '%s' in shader '%s', substituting GL_ONE\n", name,
			  shader.name);
	return GLS_DSTBLEND_ONE;
}

/*
===============
NameToGenFunc
===============
*/
static genFunc_t NameToGenFunc(const char *funcname)
{
	if(!Q_stricmp(funcname, "sin"))
	{
		return GF_SIN;
	}
	else if(!Q_stricmp(funcname, "square"))
	{
		return GF_SQUARE;
	}
	else if(!Q_stricmp(funcname, "triangle"))
	{
		return GF_TRIANGLE;
	}
	else if(!Q_stricmp(funcname, "sawtooth"))
	{
		return GF_SAWTOOTH;
	}
	else if(!Q_stricmp(funcname, "inversesawtooth"))
	{
		return GF_INVERSE_SAWTOOTH;
	}
	else if(!Q_stricmp(funcname, "noise"))
	{
		return GF_NOISE;
	}

	ri.Printf(PRINT_WARNING, "WARNING: invalid genfunc name '%s' in shader '%s'\n", funcname, shader.name);
	return GF_SIN;
}


/*
===================
ParseWaveForm
===================
*/
static void ParseWaveForm(char **text, waveForm_t * wave)
{
	char           *token;

	token = COM_ParseExt(text, qfalse);
	if(token[0] == 0)
	{
		ri.Printf(PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name);
		return;
	}
	wave->func = NameToGenFunc(token);

	// BASE, AMP, PHASE, FREQ
	token = COM_ParseExt(text, qfalse);
	if(token[0] == 0)
	{
		ri.Printf(PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name);
		return;
	}
	wave->base = atof(token);

	token = COM_ParseExt(text, qfalse);
	if(token[0] == 0)
	{
		ri.Printf(PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name);
		return;
	}
	wave->amplitude = atof(token);

	token = COM_ParseExt(text, qfalse);
	if(token[0] == 0)
	{
		ri.Printf(PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name);
		return;
	}
	wave->phase = atof(token);

	token = COM_ParseExt(text, qfalse);
	if(token[0] == 0)
	{
		ri.Printf(PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name);
		return;
	}
	wave->frequency = atof(token);
}


/*
===================
ParseTexMod
===================
*/
void ParseTexMod(char **text, shaderStage_t * stage)
{
	const char     *token;
//	char          **text = &_text;
	texModInfo_t   *tmi;

	if(stage->bundle[0].numTexMods == TR_MAX_TEXMODS)
	{
		ri.Error(ERR_DROP, "ERROR: too many tcMod stages in shader '%s'\n", shader.name);
		return;
	}

	tmi = &stage->bundle[0].texMods[stage->bundle[0].numTexMods];
	stage->bundle[0].numTexMods++;

	token = COM_ParseExt(text, qfalse);

	// turb
	if(!Q_stricmp(token, "turb"))
	{
		token = COM_ParseExt(text, qfalse);
		if(token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing tcMod turb parms in shader '%s'\n", shader.name);
			return;
		}
		tmi->wave.base = atof(token);
		token = COM_ParseExt(text, qfalse);
		if(token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", shader.name);
			return;
		}
		tmi->wave.amplitude = atof(token);
		token = COM_ParseExt(text, qfalse);
		if(token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", shader.name);
			return;
		}
		tmi->wave.phase = atof(token);
		token = COM_ParseExt(text, qfalse);
		if(token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", shader.name);
			return;
		}
		tmi->wave.frequency = atof(token);

		tmi->type = TMOD_TURBULENT;
	}
	// scale
	else if(!Q_stricmp(token, "scale"))
	{
		token = COM_ParseExt(text, qfalse);
		if(token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing scale parms in shader '%s'\n", shader.name);
			return;
		}
		tmi->scale[0] = atof(token);

		token = COM_ParseExt(text, qfalse);
		if(token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing scale parms in shader '%s'\n", shader.name);
			return;
		}
		tmi->scale[1] = atof(token);
		tmi->type = TMOD_SCALE;
	}
	// scroll
	else if(!Q_stricmp(token, "scroll"))
	{
		token = COM_ParseExt(text, qfalse);
		if(token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing scale scroll parms in shader '%s'\n", shader.name);
			return;
		}
		tmi->scroll[0] = atof(token);
		token = COM_ParseExt(text, qfalse);
		if(token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing scale scroll parms in shader '%s'\n", shader.name);
			return;
		}
		tmi->scroll[1] = atof(token);
		
	}
	// stretch
	else if(!Q_stricmp(token, "stretch"))
	{
		token = COM_ParseExt(text, qfalse);
		if(token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name);
			return;
		}
		tmi->wave.func = NameToGenFunc(token);

		token = COM_ParseExt(text, qfalse);
		if(token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name);
			return;
		}
		tmi->wave.base = atof(token);

		token = COM_ParseExt(text, qfalse);
		if(token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name);
			return;
		}
		tmi->wave.amplitude = atof(token);

		token = COM_ParseExt(text, qfalse);
		if(token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name);
			return;
		}
		tmi->wave.phase = atof(token);

		token = COM_ParseExt(text, qfalse);
		if(token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name);
			return;
		}
		tmi->wave.frequency = atof(token);

		tmi->type = TMOD_STRETCH;
	}
	// transform
	else if(!Q_stricmp(token, "transform"))
	{
		token = COM_ParseExt(text, qfalse);
		if(token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name);
			return;
		}
		tmi->matrix[0][0] = atof(token);

		token = COM_ParseExt(text, qfalse);
		if(token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name);
			return;
		}
		tmi->matrix[0][1] = atof(token);

		token = COM_ParseExt(text, qfalse);
		if(token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name);
			return;
		}
		tmi->matrix[1][0] = atof(token);

		token = COM_ParseExt(text, qfalse);
		if(token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name);
			return;
		}
		tmi->matrix[1][1] = atof(token);

		token = COM_ParseExt(text, qfalse);
		if(token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name);
			return;
		}
		tmi->translate[0] = atof(token);

		token = COM_ParseExt(text, qfalse);
		if(token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name);
			return;
		}
		tmi->translate[1] = atof(token);

		tmi->type = TMOD_TRANSFORM;
	}
	// rotate
	else if(!Q_stricmp(token, "rotate"))
	{
		token = COM_ParseExt(text, qfalse);
		if(token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing tcMod rotate parms in shader '%s'\n", shader.name);
			return;
		}
		tmi->rotateSpeed = atof(token);
		tmi->type = TMOD_ROTATE;
	}
	// entityTranslate
	else if(!Q_stricmp(token, "entityTranslate"))
	{
		tmi->type = TMOD_ENTITY_TRANSLATE;
	}
	else
	{
		ri.Printf(PRINT_WARNING, "WARNING: unknown tcMod '%s' in shader '%s'\n", token, shader.name);
		SkipRestOfLine(text);
	}
}



static qboolean ParseMap(shaderStage_t * stage, char **text)
{
	int				len;
	char           *token;
	char            buffer[1024] = "";
	char           *buffer_p = &buffer[0];
	int				imageBits = 0;
	wrapType_t		wrapType;
	
	// examples
	// map textures/caves/tembrick1crum_local.tga
	// addnormals (textures/caves/tembrick1crum_local.tga, heightmap (textures/caves/tembrick1crum_bmp.tga, 3 ))
	// heightmap( textures/hell/hellbones_d07bbump.tga, 8)

	while(1)
	{
		token = COM_ParseExt(text, qfalse);
				
		if(token[0] == 0)
			break;
				
		strcat(buffer, token);
		strcat(buffer, " ");
	}
			
	if(!buffer[0])
	{
		ri.Printf(PRINT_WARNING, "WARNING: missing parameter in shader '%s'\n", shader.name);
		return qfalse;
	}
	
	len = strlen(buffer);
	buffer[len -1] = 0;	// replace last ' ' with tailing zero
	
	token = COM_ParseExt(&buffer_p, qfalse);
	
//	ri.Printf(PRINT_ALL, "ParseMap: buffer '%s'\n", buffer);
//	ri.Printf(PRINT_ALL, "ParseMap: token '%s'\n", token);
	
	if(!Q_stricmp(token, "$whiteimage") || !Q_stricmp(token, "$white") || !Q_stricmp(token, "_white"))
	{
		stage->bundle[0].image[0] = tr.whiteImage;
		return qtrue;
	}
	else if(!Q_stricmp(token, "$blackimage") || !Q_stricmp(token, "$black") || !Q_stricmp(token, "_black"))
	{
		stage->bundle[0].image[0] = tr.blackImage;
		return qtrue;
	}
	else if(!Q_stricmp(token, "$flatimage") || !Q_stricmp(token, "$flat") || !Q_stricmp(token, "_flat"))
	{
		stage->bundle[0].isNormalMap = qtrue;
		stage->bundle[0].image[0] = tr.flatImage;
		return qtrue;
	}
	else if(!Q_stricmp(token, "$lightmap") || !Q_stricmp(token, "_lightmap"))
	{
		stage->bundle[0].isLightMap = qtrue;
		if(shader.lightmapIndex < 0)
		{
			stage->bundle[0].image[0] = tr.whiteImage;
		}
		else
		{
			stage->bundle[0].image[0] = tr.lightmaps[shader.lightmapIndex];
		}
		return qtrue;
	}
	/*
	else if(!Q_stricmp(buffer, "addnormals"))
	{
		buffer = COM_ParseExt(text, qfalse);
		if(buffer[0] != '(')
		{
			ri.Printf(PRINT_WARNING, "WARNING: no matching '(' found\n");
			return qfalse;
		}
		
		buffer = COM_ParseExt(text, qfalse);
		Q_strncpyz(filename, buffer, sizeof(filename));
		
		buffer = COM_ParseExt(text, qfalse);
		if(buffer[0] != ',')
		{
			ri.Printf(PRINT_WARNING, "WARNING: no matching ',' found\n");
			return qfalse;
		}
		
		// TODO: support heightmap
		
		SkipRestOfLine(text);
	}
	else if(!Q_stricmp(buffer, "heightmap"))
	{
		ParseHeightMap(stage, text);
	}
	else
	{
		Q_strncpyz(filename, buffer, sizeof(filename));
	}
	*/
	
	// determine image options	
	if(stage->overrideNoMipMaps || shader.noMipMaps)
	{
		imageBits |= IF_NOMIPMAPS;	
	}
	
	if(stage->overrideNoPicMip || shader.noPicMip)
	{
		imageBits |= IF_NOPICMIP;
	}
	
	if(stage->type == ST_NORMALMAP)
	{
		imageBits |= IF_NORMALMAP;	
	}
	
	if(stage->overrideWrapType)
	{
		wrapType = stage->wrapType;	
	}
	else
	{
		wrapType = shader.wrapType;
	}
	
	// try to load the image
	switch(stage->type)
	{
		case ST_COLORMAP:
		default:
		{
			stage->bundle[0].image[0] = R_FindImageFile(buffer, imageBits, wrapType);
			
			if(!stage->bundle[0].image[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: R_FindImageFile could not find colormap '%s' in shader '%s'\n", buffer, shader.name);
				return qfalse;
			}
			break;
		}
		
		case ST_DIFFUSEMAP:
		{
			stage->bundle[0].image[0] = R_FindImageFile(buffer, imageBits, wrapType);
			
			if(!stage->bundle[0].image[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: R_FindImageFile could not find diffusemap '%s' in shader '%s'\n", buffer, shader.name);
				return qfalse;
			}
			break;
		}
		
		case ST_NORMALMAP:
		{
			stage->bundle[0].image[0] = R_FindImageFile(buffer, imageBits, wrapType);
			
			if(!stage->bundle[0].image[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: R_FindImageFile could not find normalmap '%s' in shader '%s'\n", buffer, shader.name);
				return qfalse;
			}
			break;
		}
		
		case ST_SPECULARMAP:
		{
			stage->bundle[0].image[0] = R_FindImageFile(buffer, imageBits, wrapType);
			
			if(!stage->bundle[0].image[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: R_FindImageFile could not find specularmap '%s' in shader '%s'\n", buffer, shader.name);
				return qfalse;
			}
			break;
		}
	}
	
	// finally set image type
	switch(stage->type)
	{
		case ST_DIFFUSEMAP:
			stage->bundle[0].isDiffuseMap = qtrue;
			break;
		
		case ST_NORMALMAP:
			stage->bundle[0].isNormalMap = qtrue;
			break;
			
		case ST_SPECULARMAP:
			stage->bundle[0].isSpecularMap = qtrue;
			break;
			
//		case ST_HEATHAZEMAP,				// heatHaze post process effect
		case ST_LIGHTMAP:
			stage->bundle[0].isLightMap = qtrue;
			break;
		
//		case ST_DELUXEMAP:
//			stage->bundle[0].isDeluxeMap = qtrue;
//			break;
			
//		case ST_REFLECTIONMAP
//		case ST_REFRACTIONMAP,
//		case ST_DISPERSIONMAP,
//		case ST_SKYBOXMAP,
//			stage->bundle[0].isCubeMap = qtrue;
//			break;

//		case ST_SKYCLOUDMAP,
//		case ST_LIQUIDMAP
		default:
			break;
	}
	
	return qtrue;
}

/*
===================
ParseStage
===================
*/
static qboolean ParseStage(shaderStage_t * stage, char **text)
{
	char           *token;
	int				colorMaskBits = 0;
	int             depthMaskBits = GLS_DEPTHMASK_TRUE, blendSrcBits = 0, blendDstBits = 0, atestBits = 0, depthFuncBits = 0;
	qboolean        depthMaskExplicit = qfalse;
	int				imageBits = 0;

	while(1)
	{
		token = COM_ParseExt(text, qtrue);
		if(!token[0])
		{
			ri.Printf(PRINT_WARNING, "WARNING: no matching '}' found\n");
			return qfalse;
		}

		if(token[0] == '}')
		{
			break;
		}
		// if(<condition>)
		else if(!Q_stricmp(token, "if"))
		{
			/*
			token = COM_ParseExt(text, qfalse);
			if(token[0] != '(')
			{
				ri.Printf(PRINT_WARNING, "WARNING: expecting '(', found '%s' instead for if condition in shader '%s'\n", token, shader.name);
				continue;
			}
			*/
			ParseExpression(text, &stage->ifExp);
			continue;
		}
		// map <name>
		else if(!Q_stricmp(token, "map"))
		{
			if(!ParseMap(stage, text))
			{
				//ri.Printf(PRINT_WARNING, "WARNING: ParseMap could not create '%s' in shader '%s'\n", token, shader.name);
				return qfalse;
			}
		}
		// clampmap <name>
		else if(!Q_stricmp(token, "clampmap"))
		{
			token = COM_ParseExt(text, qfalse);
			if(!token[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameter for 'clampmap' keyword in shader '%s'\n", shader.name);
				return qfalse;
			}
			
			imageBits = 0;
			if(stage->overrideNoMipMaps || shader.noMipMaps)
			{
				imageBits |= IF_NOMIPMAPS;	
			}
			if(stage->overrideNoPicMip || shader.noPicMip)
			{
				imageBits |= IF_NOPICMIP;
			}

			stage->bundle[0].image[0] = R_FindImageFile(token, imageBits, WT_CLAMP);
			if(!stage->bundle[0].image[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, shader.name);
				return qfalse;
			}
		}
		// animMap <frequency> <image1> .... <imageN>
		else if(!Q_stricmp(token, "animMap"))
		{
			token = COM_ParseExt(text, qfalse);
			if(!token[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameter for 'animMmap' keyword in shader '%s'\n", shader.name);
				return qfalse;
			}
			stage->bundle[0].imageAnimationSpeed = atof(token);
			
			imageBits = 0;
			if(stage->overrideNoMipMaps || shader.noMipMaps)
			{
				imageBits |= IF_NOMIPMAPS;	
			}
			if(stage->overrideNoPicMip || shader.noPicMip)
			{
				imageBits |= IF_NOPICMIP;
			}

			// parse up to MAX_IMAGE_ANIMATIONS animations
			while(1)
			{
				int             num;

				token = COM_ParseExt(text, qfalse);
				if(!token[0])
				{
					break;
				}
				num = stage->bundle[0].numImageAnimations;
				if(num < MAX_IMAGE_ANIMATIONS)
				{
					stage->bundle[0].image[num] = R_FindImageFile(token, imageBits, WT_REPEAT);
					if(!stage->bundle[0].image[num])
					{
						ri.Printf(PRINT_WARNING,
								  "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, shader.name);
						return qfalse;
					}
					stage->bundle[0].numImageAnimations++;
				}
			}
		}
		else if(!Q_stricmp(token, "videoMap"))
		{
			token = COM_ParseExt(text, qfalse);
			if(!token[0])
			{
				ri.Printf(PRINT_WARNING,
						  "WARNING: missing parameter for 'videoMap' keyword in shader '%s'\n", shader.name);
				return qfalse;
			}
			stage->bundle[0].videoMapHandle = ri.CIN_PlayCinematic(token, 0, 0, 256, 256, (CIN_loop | CIN_silent | CIN_shader));
			if(stage->bundle[0].videoMapHandle != -1)
			{
				stage->bundle[0].isVideoMap = qtrue;
				stage->bundle[0].image[0] = tr.scratchImage[stage->bundle[0].videoMapHandle];
			}
		}
		else if(!Q_stricmp(token, "cubeMap"))
		{
			token = COM_ParseExt(text, qfalse);
			if(!token[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameter for 'cubeMap' keyword in shader '%s'\n", shader.name);
				return qfalse;
			}
			
			imageBits = 0;
			if(stage->overrideNoMipMaps || shader.noMipMaps)
			{
				imageBits |= IF_NOMIPMAPS;	
			}
			if(stage->overrideNoPicMip || shader.noPicMip)
			{
				imageBits |= IF_NOPICMIP;
			}
			
			stage->bundle[0].image[0] = R_FindCubeImage(token, imageBits, WT_CLAMP);
			if(!stage->bundle[0].image[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: R_FindCubeImage could not find '%s' in shader '%s'\n", token, shader.name);
				return qfalse;
			}
		}
		// alphafunc <func>
		else if(!Q_stricmp(token, "alphaFunc"))
		{
			token = COM_ParseExt(text, qfalse);
			if(!token[0])
			{
				ri.Printf(PRINT_WARNING,
						  "WARNING: missing parameter for 'alphaFunc' keyword in shader '%s'\n", shader.name);
				return qfalse;
			}

			atestBits = NameToAFunc(token);
		}
		// alphaTest <exp>
		else if(!Q_stricmp(token, "alphaTest"))
		{
			atestBits = GLS_ATEST_GT_CUSTOM;
			ParseExpression(text, &stage->alphaTestExp);
		}
		// depthFunc <func>
		else if(!Q_stricmp(token, "depthfunc"))
		{
			token = COM_ParseExt(text, qfalse);

			if(!token[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameter for 'depthfunc' keyword in shader '%s'\n", shader.name);
				return qfalse;
			}

			if(!Q_stricmp(token, "lequal"))
			{
				depthFuncBits = 0;
			}
			else if(!Q_stricmp(token, "equal"))
			{
				depthFuncBits = GLS_DEPTHFUNC_EQUAL;
			}
			else
			{
				ri.Printf(PRINT_WARNING, "WARNING: unknown depthfunc '%s' in shader '%s'\n", token, shader.name);
				continue;
			}
		}
		// ignoreAlphaTest
		else if(!Q_stricmp(token, "ignoreAlphaTest"))
		{
			depthFuncBits = 0;
			continue;
		}
		// nearest
		else if(!Q_stricmp(token, "nearest"))
		{
			stage->overrideNoMipMaps = qtrue;
			continue;
		}
		// linear
		else if(!Q_stricmp(token, "linear"))
		{
			stage->overrideNoMipMaps = qtrue;
			stage->overrideNoPicMip = qtrue;
			continue;
		}
		// noPicMip
		else if(!Q_stricmp(token, "noPicMip"))
		{
			stage->overrideNoPicMip = qtrue;
			continue;
		}
		// clamp
		else if(!Q_stricmp(token, "clamp"))
		{
			stage->overrideWrapType = qtrue;
			stage->wrapType = WT_CLAMP;
			continue;
		}
		// edgeClamp
		else if(!Q_stricmp(token, "edgeClamp"))
		{
			stage->overrideWrapType = qtrue;
			stage->wrapType = WT_EDGE_CLAMP;
			continue;
		}
		// zeroClamp
		else if(!Q_stricmp(token, "zeroClamp"))
		{
			stage->overrideWrapType = qtrue;
			stage->wrapType = WT_ZERO_CLAMP;
			continue;
		}
		// alphaZeroClamp
		else if(!Q_stricmp(token, "alphaZeroClamp"))
		{
			stage->overrideWrapType = qtrue;
			stage->wrapType = WT_ALPHA_ZERO_CLAMP;
			continue;
		}
		// noClamp
		else if(!Q_stricmp(token, "noClamp"))
		{
			stage->overrideWrapType = qtrue;
			stage->wrapType = WT_REPEAT;
			continue;
		}
		// uncompressed
		else if(!Q_stricmp(token, "uncompressed"))
		{
			stage->uncompressed = qtrue;
			continue;
		}
		// highQuality
		else if(!Q_stricmp(token, "highQuality"))
		{
			stage->highQuality = qtrue;
			continue;
		}
		// forceHighQuality
		else if(!Q_stricmp(token, "forceHighQuality"))
		{
			stage->forceHighQuality = qtrue;
			continue;
		}
		// detail
		else if(!Q_stricmp(token, "detail"))
		{
			stage->isDetail = qtrue;
			continue;
		}
		// blendfunc <srcFactor> <dstFactor>
		// or blendfunc <add|filter|blend>
		else if(!Q_stricmp(token, "blendfunc"))
		{
			token = COM_ParseExt(text, qfalse);
			if(token[0] == 0)
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parm for blendFunc in shader '%s'\n", shader.name);
				continue;
			}
			// check for "simple" blends first
			if(!Q_stricmp(token, "add"))
			{
				blendSrcBits = GLS_SRCBLEND_ONE;
				blendDstBits = GLS_DSTBLEND_ONE;
			}
			else if(!Q_stricmp(token, "filter"))
			{
				blendSrcBits = GLS_SRCBLEND_DST_COLOR;
				blendDstBits = GLS_DSTBLEND_ZERO;
			}
			else if(!Q_stricmp(token, "blend"))
			{
				blendSrcBits = GLS_SRCBLEND_SRC_ALPHA;
				blendDstBits = GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
			}
			else
			{
				// complex double blends
				blendSrcBits = NameToSrcBlendMode(token);

				token = COM_ParseExt(text, qfalse);
				if(token[0] == 0)
				{
					ri.Printf(PRINT_WARNING, "WARNING: missing parm for blendFunc in shader '%s'\n",
							  shader.name);
					continue;
				}
				blendDstBits = NameToDstBlendMode(token);
			}

			// clear depth mask for blended surfaces
			if(!depthMaskExplicit)
			{
				depthMaskBits = 0;
			}
		}
		// blend <srcFactor> , <dstFactor>
		// or blend <add | filter | blend>
		// or blend <diffusemap | bumpmap | specularmap | lightmap>
		else if(!Q_stricmp(token, "blend"))
		{
			token = COM_ParseExt(text, qfalse);
			if(token[0] == 0)
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parm for blend in shader '%s'\n", shader.name);
				continue;
			}
			
			// check for "simple" blends first
			if(!Q_stricmp(token, "add"))
			{
				blendSrcBits = GLS_SRCBLEND_ONE;
				blendDstBits = GLS_DSTBLEND_ONE;
			}
			else if(!Q_stricmp(token, "filter"))
			{
				blendSrcBits = GLS_SRCBLEND_DST_COLOR;
				blendDstBits = GLS_DSTBLEND_ZERO;
			}
			else if(!Q_stricmp(token, "modulate"))
			{
				blendSrcBits = GLS_SRCBLEND_DST_COLOR;
				blendDstBits = GLS_DSTBLEND_ZERO;
			}
			else if(!Q_stricmp(token, "blend"))
			{
				blendSrcBits = GLS_SRCBLEND_SRC_ALPHA;
				blendDstBits = GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
			}
			else if(!Q_stricmp(token, "none"))
			{
				blendSrcBits = GLS_SRCBLEND_ZERO;
				blendDstBits = GLS_DSTBLEND_ONE;
			}
			// check for other semantic meanings
			else if(!Q_stricmp(token, "diffusemap"))
			{
				stage->type = ST_DIFFUSEMAP;
			}
			else if(!Q_stricmp(token, "bumpmap"))
			{
				stage->type = ST_NORMALMAP;
			}
			else if(!Q_stricmp(token, "specularmap"))
			{
				stage->type = ST_SPECULARMAP;
			}
			else if(!Q_stricmp(token, "lightmap"))
			{
				stage->type = ST_LIGHTMAP;
			}
			else
			{
				// complex double blends
				blendSrcBits = NameToSrcBlendMode(token);

				token = COM_ParseExt(text, qfalse);
				if(token[0] != ',')
				{
					ri.Printf(PRINT_WARNING, "WARNING: expecting ',', found '%s' instead for blend in shader '%s'\n", token, shader.name);
					continue;
				}
				
				token = COM_ParseExt(text, qfalse);
				if(token[0] == 0)
				{
					ri.Printf(PRINT_WARNING, "WARNING: missing parm for blend in shader '%s'\n", shader.name);
					continue;
				}
				blendDstBits = NameToDstBlendMode(token);
			}

			// clear depth mask for blended surfaces
			if(!depthMaskExplicit && stage->type == ST_COLORMAP)
			{
				depthMaskBits = 0;
			}
		}
		// stage <type>
		else if(!Q_stricmp(token, "stage"))
		{
			token = COM_ParseExt(text, qfalse);
			if(token[0] == 0)
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameters for stage in shader '%s'\n", shader.name);
				continue;
			}

			if(!Q_stricmp(token, "colormap"))
			{
				stage->type = ST_COLORMAP;
			}
			else if(!Q_stricmp(token, "diffusemap"))
			{
				stage->type = ST_DIFFUSEMAP;
			}
			else if(!Q_stricmp(token, "normalmap") || !Q_stricmp(token, "bumpmap"))
			{
				stage->type = ST_NORMALMAP;
			}
			else if(!Q_stricmp(token, "specularmap"))
			{
				stage->type = ST_SPECULARMAP;
			}
			else if(!Q_stricmp(token, "heathazemap"))
			{
				stage->type = ST_HEATHAZEMAP;
			}
			else if(!Q_stricmp(token, "lightmap"))
			{
				stage->type = ST_LIGHTMAP;
			}
			else if(!Q_stricmp(token, "reflectionmap"))
			{
				stage->type = ST_REFLECTIONMAP;
			}
			else if(!Q_stricmp(token, "refractionmap"))
			{
				stage->type = ST_REFRACTIONMAP;
			}
			else if(!Q_stricmp(token, "dispersionmap"))
			{
				stage->type = ST_DISPERSIONMAP;
			}
			else if(!Q_stricmp(token, "skyboxmap"))
			{
				stage->type = ST_SKYBOXMAP;
			}
			else if(!Q_stricmp(token, "liquidmap"))
			{
				stage->type = ST_LIQUIDMAP;
			}
			else
			{
				ri.Printf(PRINT_WARNING, "WARNING: unknown stage parameter '%s' in shader '%s'\n", token, shader.name);
				continue;
			}
		}
		// rgbGen
		else if(!Q_stricmp(token, "rgbGen"))
		{
			token = COM_ParseExt(text, qfalse);
			if(token[0] == 0)
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameters for rgbGen in shader '%s'\n",
						  shader.name);
				continue;
			}

			if(!Q_stricmp(token, "wave"))
			{
				ParseWaveForm(text, &stage->rgbWave);
				stage->rgbGen = CGEN_WAVEFORM;
			}
			else if(!Q_stricmp(token, "const"))
			{
				vec3_t          color;

				ParseVector(text, 3, color);
				stage->constantColor[0] = 255 * color[0];
				stage->constantColor[1] = 255 * color[1];
				stage->constantColor[2] = 255 * color[2];

				stage->rgbGen = CGEN_CONST;
			}
			else if(!Q_stricmp(token, "identity"))
			{
				stage->rgbGen = CGEN_IDENTITY;
			}
			else if(!Q_stricmp(token, "identityLighting"))
			{
				stage->rgbGen = CGEN_IDENTITY_LIGHTING;
			}
			else if(!Q_stricmp(token, "entity"))
			{
				stage->rgbGen = CGEN_ENTITY;
			}
			else if(!Q_stricmp(token, "oneMinusEntity"))
			{
				stage->rgbGen = CGEN_ONE_MINUS_ENTITY;
			}
			else if(!Q_stricmp(token, "vertex"))
			{
				stage->rgbGen = CGEN_VERTEX;
				if(stage->alphaGen == 0)
				{
					stage->alphaGen = AGEN_VERTEX;
				}
			}
			else if(!Q_stricmp(token, "exactVertex"))
			{
				stage->rgbGen = CGEN_EXACT_VERTEX;
			}
			else if(!Q_stricmp(token, "lightingDiffuse"))
			{
				stage->rgbGen = CGEN_LIGHTING_DIFFUSE;
			}
			else if(!Q_stricmp(token, "oneMinusVertex"))
			{
				stage->rgbGen = CGEN_ONE_MINUS_VERTEX;
			}
			else
			{
				ri.Printf(PRINT_WARNING, "WARNING: unknown rgbGen parameter '%s' in shader '%s'\n", token, shader.name);
				continue;
			}
		}
		// rgb <arithmetic expression>
		else if(!Q_stricmp(token, "rgb"))
		{
			stage->rgbGen = CGEN_CUSTOM_RGB;
			ParseExpression(text, &stage->rgbExp);
			continue;
		}
		// red <arithmetic expression>
		else if(!Q_stricmp(token, "red"))
		{
			stage->rgbGen = CGEN_CUSTOM_RGBs;
			ParseExpression(text, &stage->redExp);
			continue;
		}
		// green <arithmetic expression>
		else if(!Q_stricmp(token, "green"))
		{
			stage->rgbGen = CGEN_CUSTOM_RGBs;
			ParseExpression(text, &stage->greenExp);
			continue;
		}
		// blue <arithmetic expression>
		else if(!Q_stricmp(token, "blue"))
		{
			stage->rgbGen = CGEN_CUSTOM_RGBs;
			ParseExpression(text, &stage->blueExp);
			continue;
		}
		// colored
		else if(!Q_stricmp(token, "colored"))
		{
			stage->rgbGen = CGEN_ENTITY;
			continue;
		}
		// vertexColor
		else if(!Q_stricmp(token, "vertexColor"))
		{
			stage->rgbGen = CGEN_VERTEX;
		}
		// inverseVertexColor
		else if(!Q_stricmp(token, "inverseVertexColor"))
		{
			stage->rgbGen = CGEN_ONE_MINUS_VERTEX;
		}
		// alphaGen 
		else if(!Q_stricmp(token, "alphaGen"))
		{
			token = COM_ParseExt(text, qfalse);
			if(token[0] == 0)
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameters for alphaGen in shader '%s'\n",
						  shader.name);
				continue;
			}

			if(!Q_stricmp(token, "wave"))
			{
				ParseWaveForm(text, &stage->alphaWave);
				stage->alphaGen = AGEN_WAVEFORM;
			}
			else if(!Q_stricmp(token, "const"))
			{
				token = COM_ParseExt(text, qfalse);
				stage->constantColor[3] = 255 * atof(token);
				stage->alphaGen = AGEN_CONST;
			}
			else if(!Q_stricmp(token, "identity"))
			{
				stage->alphaGen = AGEN_IDENTITY;
			}
			else if(!Q_stricmp(token, "entity"))
			{
				stage->alphaGen = AGEN_ENTITY;
			}
			else if(!Q_stricmp(token, "oneMinusEntity"))
			{
				stage->alphaGen = AGEN_ONE_MINUS_ENTITY;
			}
			else if(!Q_stricmp(token, "vertex"))
			{
				stage->alphaGen = AGEN_VERTEX;
			}
			else if(!Q_stricmp(token, "lightingSpecular"))
			{
				stage->alphaGen = AGEN_LIGHTING_SPECULAR;
			}
			else if(!Q_stricmp(token, "oneMinusVertex"))
			{
				stage->alphaGen = AGEN_ONE_MINUS_VERTEX;
			}
			else if(!Q_stricmp(token, "portal"))
			{
				stage->alphaGen = AGEN_PORTAL;
				token = COM_ParseExt(text, qfalse);
				if(token[0] == 0)
				{
					shader.portalRange = 256;
					ri.Printf(PRINT_WARNING,
							  "WARNING: missing range parameter for alphaGen portal in shader '%s', defaulting to 256\n",
							  shader.name);
				}
				else
				{
					shader.portalRange = atof(token);
				}
			}
			else
			{
				ri.Printf(PRINT_WARNING, "WARNING: unknown alphaGen parameter '%s' in shader '%s'\n", token, shader.name);
				continue;
			}
		}
		// alpha <arithmetic expression>
		else if(!Q_stricmp(token, "alpha"))
		{
			stage->alphaGen = AGEN_CUSTOM;
			ParseExpression(text, &stage->alphaExp);
			continue;
		}
		// color <exp>, <exp>, <exp>, <exp>
		else if(!Q_stricmp(token, "color"))
		{
			stage->rgbGen = CGEN_CUSTOM_RGBs;
			stage->alphaGen = AGEN_CUSTOM;
			ParseExpression(text, &stage->redExp);
			ParseExpression(text, &stage->greenExp);
			ParseExpression(text, &stage->blueExp);
			ParseExpression(text, &stage->alphaExp);
			continue;
		}
		// privatePolygonOffset <float>
		else if(!Q_stricmp(token, "privatePolygonOffset"))
		{
			token = COM_ParseExt(text, qfalse);
			if(!token[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameter for 'privatePolygonOffset' keyword in shader '%s'\n", shader.name);
				return qfalse;
			}
			
			stage->privatePolygonOffset = qtrue;
			stage->privatePolygonOffsetValue = atof(token);
		}
		// tcGen <function>
		else if(!Q_stricmp(token, "texGen") || !Q_stricmp(token, "tcGen"))
		{
			token = COM_ParseExt(text, qfalse);
			if(token[0] == 0)
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing texgen parm in shader '%s'\n", shader.name);
				continue;
			}

			if(!Q_stricmp(token, "environment"))
			{
				stage->bundle[0].tcGen = TCGEN_ENVIRONMENT_MAPPED;
			}
			else if(!Q_stricmp(token, "lightmap"))
			{
				stage->bundle[0].tcGen = TCGEN_LIGHTMAP;
			}
			else if(!Q_stricmp(token, "texture") || !Q_stricmp(token, "base"))
			{
				stage->bundle[0].tcGen = TCGEN_TEXTURE;
			}
			else if(!Q_stricmp(token, "vector"))
			{
				ParseVector(text, 3, stage->bundle[0].tcGenVectors[0]);
				ParseVector(text, 3, stage->bundle[0].tcGenVectors[1]);

				stage->bundle[0].tcGen = TCGEN_VECTOR;
			}
			else if(!Q_stricmp(token, "reflect"))
			{
				stage->type = ST_REFLECTIONMAP;
			}
			else if(!Q_stricmp(token, "skybox"))
			{
				stage->type = ST_SKYBOXMAP;
			}
			else
			{
				ri.Printf(PRINT_WARNING, "WARNING: unknown texgen parm in shader '%s'\n", shader.name);
			}
		}
		// tcMod <type> <...>
		else if(!Q_stricmp(token, "tcMod"))
		{
			/*
			char            buffer[1024] = "";

			while(1)
			{
				token = COM_ParseExt(text, qfalse);
				
				if(token[0] == 0)
					break;
				
				strcat(buffer, token);
				strcat(buffer, " ");
			}
			
			ParseTexMod(buffer, stage);
			*/
			
			// NOTE: temporarily disabled because of parser problems
			SkipRestOfLine(text);
			//ParseTexMod(text, stage);
			continue;
		}
		// scroll
		else if(!Q_stricmp(token, "scroll"))
		{
			texModInfo_t   *tmi;
			
			if(stage->bundle[0].numTexMods == TR_MAX_TEXMODS)
			{
				ri.Error(ERR_DROP, "ERROR: too many tcMod stages in shader '%s'\n", shader.name);
				return qfalse;
			}

			tmi = &stage->bundle[0].texMods[stage->bundle[0].numTexMods];
			stage->bundle[0].numTexMods++;	
			
			ParseExpression(text, &tmi->sExp);
			ParseExpression(text, &tmi->tExp);
			
			tmi->type = TMOD_SCROLL2;
		}
		// translate
		else if(!Q_stricmp(token, "translate"))
		{
			texModInfo_t   *tmi;
			
			if(stage->bundle[0].numTexMods == TR_MAX_TEXMODS)
			{
				ri.Error(ERR_DROP, "ERROR: too many tcMod stages in shader '%s'\n", shader.name);
				return qfalse;
			}

			tmi = &stage->bundle[0].texMods[stage->bundle[0].numTexMods];
			stage->bundle[0].numTexMods++;	
			
			ParseExpression(text, &tmi->sExp);
			ParseExpression(text, &tmi->tExp);
			
			tmi->type = TMOD_TRANSLATE;
		}
		// scale
		else if(!Q_stricmp(token, "scale"))
		{
			texModInfo_t   *tmi;
			
			if(stage->bundle[0].numTexMods == TR_MAX_TEXMODS)
			{
				ri.Error(ERR_DROP, "ERROR: too many tcMod stages in shader '%s'\n", shader.name);
				return qfalse;
			}

			tmi = &stage->bundle[0].texMods[stage->bundle[0].numTexMods];
			stage->bundle[0].numTexMods++;	
			
			ParseExpression(text, &tmi->sExp);
			ParseExpression(text, &tmi->tExp);
			
			tmi->type = TMOD_SCALE2;
		}
		// centerScale
		else if(!Q_stricmp(token, "centerScale"))
		{
			texModInfo_t   *tmi;
			
			if(stage->bundle[0].numTexMods == TR_MAX_TEXMODS)
			{
				ri.Error(ERR_DROP, "ERROR: too many tcMod stages in shader '%s'\n", shader.name);
				return qfalse;
			}

			tmi = &stage->bundle[0].texMods[stage->bundle[0].numTexMods];
			stage->bundle[0].numTexMods++;	
			
			ParseExpression(text, &tmi->sExp);
			ParseExpression(text, &tmi->tExp);
			
			tmi->type = TMOD_CENTERSCALE;
		}
		// shear
		else if(!Q_stricmp(token, "shear"))
		{
			texModInfo_t   *tmi;
			
			if(stage->bundle[0].numTexMods == TR_MAX_TEXMODS)
			{
				ri.Error(ERR_DROP, "ERROR: too many tcMod stages in shader '%s'\n", shader.name);
				return qfalse;
			}

			tmi = &stage->bundle[0].texMods[stage->bundle[0].numTexMods];
			stage->bundle[0].numTexMods++;	
			
			ParseExpression(text, &tmi->sExp);
			ParseExpression(text, &tmi->tExp);
			
			tmi->type = TMOD_SHEAR;
		}
		// rotate
		else if(!Q_stricmp(token, "rotate"))
		{
			texModInfo_t   *tmi;
			
			if(stage->bundle[0].numTexMods == TR_MAX_TEXMODS)
			{
				ri.Error(ERR_DROP, "ERROR: too many tcMod stages in shader '%s'\n", shader.name);
				return qfalse;
			}

			tmi = &stage->bundle[0].texMods[stage->bundle[0].numTexMods];
			stage->bundle[0].numTexMods++;	
			
			ParseExpression(text, &tmi->rExp);
			
			tmi->type = TMOD_ROTATE2;
		}
		// depthwrite
		else if(!Q_stricmp(token, "depthwrite"))
		{
			depthMaskBits = GLS_DEPTHMASK_TRUE;
			depthMaskExplicit = qtrue;
			continue;
		}
		// maskRed
		else if(!Q_stricmp(token, "maskRed"))
		{
			colorMaskBits |= GLS_REDMASK_FALSE;
			continue;
		}
		// maskGreen
		else if(!Q_stricmp(token, "maskGreen"))
		{
			colorMaskBits |= GLS_GREENMASK_FALSE;
			continue;
		}
		// maskBlue
		else if(!Q_stricmp(token, "maskBlue"))
		{
			colorMaskBits |= GLS_BLUEMASK_FALSE;
			continue;
		}
		// maskAlpha
		else if(!Q_stricmp(token, "maskAlpha"))
		{
			colorMaskBits |= GLS_ALPHAMASK_FALSE;
			continue;
		}
		// maskColor
		else if(!Q_stricmp(token, "maskColor"))
		{
			colorMaskBits |= GLS_REDMASK_FALSE | GLS_GREENMASK_FALSE | GLS_BLUEMASK_FALSE;
			continue;
		}
		// maskDepth
		else if(!Q_stricmp(token, "maskDepth"))
		{
			depthMaskBits &= ~GLS_DEPTHMASK_TRUE;
			depthMaskExplicit = qfalse;
			continue;
		}
		else
		{
			ri.Printf(PRINT_WARNING, "WARNING: unknown parameter '%s' in shader '%s'\n", token, shader.name);
			return qfalse;
		}
	}
	
	// parsing succeeded
	stage->active = qtrue;

	// if cgen isn't explicitly specified, use either identity or identitylighting
	if(stage->rgbGen == CGEN_BAD)
	{
		if(blendSrcBits == 0 || blendSrcBits == GLS_SRCBLEND_ONE || blendSrcBits == GLS_SRCBLEND_SRC_ALPHA)
		{
			stage->rgbGen = CGEN_IDENTITY_LIGHTING;
		}
		else
		{
			stage->rgbGen = CGEN_IDENTITY;
		}
	}

	// implicitly assume that a GL_ONE GL_ZERO blend mask disables blending
	if((blendSrcBits == GLS_SRCBLEND_ONE) && (blendDstBits == GLS_DSTBLEND_ZERO))
	{
		blendDstBits = blendSrcBits = 0;
		depthMaskBits = GLS_DEPTHMASK_TRUE;
	}

	// decide which agens we can skip
	if(stage->alphaGen == CGEN_IDENTITY)
	{
		if(stage->rgbGen == CGEN_IDENTITY || stage->rgbGen == CGEN_LIGHTING_DIFFUSE)
		{
			stage->alphaGen = AGEN_SKIP;
		}
	}

	// compute state bits
	stage->stateBits = colorMaskBits | depthMaskBits | blendSrcBits | blendDstBits | atestBits | depthFuncBits;

	return qtrue;
}

/*
===============
ParseDeform

deformVertexes wave <spread> <waveform> <base> <amplitude> <phase> <frequency>
deformVertexes normal <frequency> <amplitude>
deformVertexes move <vector> <waveform> <base> <amplitude> <phase> <frequency>
deformVertexes bulge <bulgeWidth> <bulgeHeight> <bulgeSpeed>
deformVertexes projectionShadow
deformVertexes autoSprite
deformVertexes autoSprite2
deformVertexes text[0-7]
===============
*/
static void ParseDeform(char **text)
{
	char           *token;
	deformStage_t  *ds;

	token = COM_ParseExt(text, qfalse);
	if(token[0] == 0)
	{
		ri.Printf(PRINT_WARNING, "WARNING: missing deform parm in shader '%s'\n", shader.name);
		return;
	}

	if(shader.numDeforms == MAX_SHADER_DEFORMS)
	{
		ri.Printf(PRINT_WARNING, "WARNING: MAX_SHADER_DEFORMS in '%s'\n", shader.name);
		return;
	}

	ds = &shader.deforms[shader.numDeforms];
	shader.numDeforms++;

	if(!Q_stricmp(token, "projectionShadow"))
	{
		ds->deformation = DEFORM_PROJECTION_SHADOW;
		return;
	}

	if(!Q_stricmp(token, "autosprite"))
	{
		ds->deformation = DEFORM_AUTOSPRITE;
		return;
	}

	if(!Q_stricmp(token, "autosprite2"))
	{
		ds->deformation = DEFORM_AUTOSPRITE2;
		return;
	}
	
	if(!Q_stricmp(token, "sprite"))
	{
		ds->deformation = DEFORM_SPRITE;
		return;
	}

	if(!Q_stricmpn(token, "text", 4))
	{
		int             n;

		n = token[4] - '0';
		if(n < 0 || n > 7)
		{
			n = 0;
		}
		ds->deformation = DEFORM_TEXT0 + n;
		return;
	}

	if(!Q_stricmp(token, "bulge"))
	{
		token = COM_ParseExt(text, qfalse);
		if(token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name);
			return;
		}
		ds->bulgeWidth = atof(token);

		token = COM_ParseExt(text, qfalse);
		if(token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name);
			return;
		}
		ds->bulgeHeight = atof(token);

		token = COM_ParseExt(text, qfalse);
		if(token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name);
			return;
		}
		ds->bulgeSpeed = atof(token);

		ds->deformation = DEFORM_BULGE;
		return;
	}

	if(!Q_stricmp(token, "wave"))
	{
		token = COM_ParseExt(text, qfalse);
		if(token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name);
			return;
		}

		if(atof(token) != 0)
		{
			ds->deformationSpread = 1.0f / atof(token);
		}
		else
		{
			ds->deformationSpread = 100.0f;
			ri.Printf(PRINT_WARNING, "WARNING: illegal div value of 0 in deformVertexes command for shader '%s'\n", shader.name);
		}

		ParseWaveForm(text, &ds->deformationWave);
		ds->deformation = DEFORM_WAVE;
		return;
	}

	if(!Q_stricmp(token, "normal"))
	{
		token = COM_ParseExt(text, qfalse);
		if(token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name);
			return;
		}
		ds->deformationWave.amplitude = atof(token);

		token = COM_ParseExt(text, qfalse);
		if(token[0] == 0)
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name);
			return;
		}
		ds->deformationWave.frequency = atof(token);

		ds->deformation = DEFORM_NORMALS;
		return;
	}

	if(!Q_stricmp(token, "move"))
	{
		int             i;

		for(i = 0; i < 3; i++)
		{
			token = COM_ParseExt(text, qfalse);
			if(token[0] == 0)
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name);
				return;
			}
			ds->moveVector[i] = atof(token);
		}

		ParseWaveForm(text, &ds->deformationWave);
		ds->deformation = DEFORM_MOVE;
		return;
	}

	ri.Printf(PRINT_WARNING, "WARNING: unknown deformVertexes subtype '%s' found in shader '%s'\n", token, shader.name);
}


/*
===============
ParseSkyParms

skyParms <outerbox> <cloudheight> <innerbox>
===============
*/
static void ParseSkyParms(char **text)
{
	char           *token;
	static char    *suf[6] = { "rt", "bk", "lf", "ft", "up", "dn" };
	char            pathname[MAX_QPATH];
	int             i;

	// outerbox
	token = COM_ParseExt(text, qfalse);
	if(token[0] == 0)
	{
		ri.Printf(PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader.name);
		return;
	}
	if(strcmp(token, "-"))
	{
		for(i = 0; i < 6; i++)
		{
			Com_sprintf(pathname, sizeof(pathname), "%s_%s.tga", token, suf[i]);
			shader.sky.outerbox[i] = R_FindImageFile((char *)pathname, IF_NONE, WT_CLAMP);
			if(!shader.sky.outerbox[i])
			{
				shader.sky.outerbox[i] = tr.defaultImage;
			}
		}
	}

	// cloudheight
	token = COM_ParseExt(text, qfalse);
	if(token[0] == 0)
	{
		ri.Printf(PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader.name);
		return;
	}
	shader.sky.cloudHeight = atof(token);
	if(!shader.sky.cloudHeight)
	{
		shader.sky.cloudHeight = 512;
	}
	R_InitSkyTexCoords(shader.sky.cloudHeight);


	// innerbox
	token = COM_ParseExt(text, qfalse);
	if(token[0] == 0)
	{
		ri.Printf(PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader.name);
		return;
	}
	if(strcmp(token, "-"))
	{
		for(i = 0; i < 6; i++)
		{
			Com_sprintf(pathname, sizeof(pathname), "%s_%s.tga", token, suf[i]);
			shader.sky.innerbox[i] = R_FindImageFile((char *)pathname, IF_NONE, WT_REPEAT);
			if(!shader.sky.innerbox[i])
			{
				shader.sky.innerbox[i] = tr.defaultImage;
			}
		}
	}

	shader.isSky = qtrue;
}


/*
=================
ParseSort
=================
*/
static void ParseSort(char **text)
{
	char           *token;

	token = COM_ParseExt(text, qfalse);
	if(token[0] == 0)
	{
		ri.Printf(PRINT_WARNING, "WARNING: missing sort parameter in shader '%s'\n", shader.name);
		return;
	}

	if(!Q_stricmp(token, "portal") || !Q_stricmp(token, "subview"))
	{
		shader.sort = SS_PORTAL;
	}
	else if(!Q_stricmp(token, "sky") || !Q_stricmp(token, "environment"))
	{
		shader.sort = SS_ENVIRONMENT;
	}
	else if(!Q_stricmp(token, "opaque"))
	{
		shader.sort = SS_OPAQUE;
	}
	else if(!Q_stricmp(token, "decal"))
	{
		shader.sort = SS_DECAL;
	}
	else if(!Q_stricmp(token, "seeThrough"))
	{
		shader.sort = SS_SEE_THROUGH;
	}
	else if(!Q_stricmp(token, "banner"))
	{
		shader.sort = SS_BANNER;
	}
	else if(!Q_stricmp(token, "underwater"))
	{
		shader.sort = SS_UNDERWATER;
	}
	else if(!Q_stricmp(token, "far"))
	{
		shader.sort = SS_FAR;
	}
	else if(!Q_stricmp(token, "medium"))
	{
		shader.sort = SS_MEDIUM;
	}
	else if(!Q_stricmp(token, "close"))
	{
		shader.sort = SS_CLOSE;
	}
	else if(!Q_stricmp(token, "additive"))
	{
		shader.sort = SS_BLEND1;
	}
	else if(!Q_stricmp(token, "almostNearest"))
	{
		shader.sort = SS_ALMOST_NEAREST;
	}
	else if(!Q_stricmp(token, "nearest"))
	{
		shader.sort = SS_NEAREST;
	}
	else if(!Q_stricmp(token, "postProcess"))
	{
		shader.sort = SS_POST_PROCESS;
	}
	else
	{
		shader.sort = atof(token);
	}
}



// this table is also present in q3map

typedef struct
{
	char           *name;
	int             clearSolid, surfaceFlags, contents;
} infoParm_t;

// *INDENT-OFF*
infoParm_t	infoParms[] = {
	// server relevant contents
	{"water",			1,	0,	CONTENTS_WATER},
	{"slime",			1,	0,	CONTENTS_SLIME},		// mildly damaging
	{"lava",			1,	0,	CONTENTS_LAVA},			// very damaging
	{"playerclip",		1,	0,	CONTENTS_PLAYERCLIP},
	{"monsterclip",		1,	0,	CONTENTS_MONSTERCLIP},
	{"nodrop",			1,	0,	CONTENTS_NODROP},		// don't drop items or leave bodies (death fog, lava, etc)
	{"nonsolid",		1,	SURF_NONSOLID,	0},			// clears the solid flag
	
	{"blood",			1,	0,	CONTENTS_WATER},

	// utility relevant attributes
	{"origin",			1,	0,	CONTENTS_ORIGIN},		// center of rotating brushes
	{"trans",			0,	0,	CONTENTS_TRANSLUCENT},	// don't eat contained surfaces
	{"translucent",		0,	0,	CONTENTS_TRANSLUCENT},	// don't eat contained surfaces
	{"detail",			0,	0,	CONTENTS_DETAIL},		// don't include in structural bsp
	{"structural",		0,	0,	CONTENTS_STRUCTURAL},	// force into structural bsp even if trnas
	{"areaportal",		1,	0,	CONTENTS_AREAPORTAL},	// divides areas
	{"clusterportal",	1,	0,  CONTENTS_CLUSTERPORTAL},	// for bots
	{"donotenter",  	1,  0,  CONTENTS_DONOTENTER},		// for bots

	{"fog",				1,	0,	CONTENTS_FOG},			// carves surfaces entering
	{"sky",				0,	SURF_SKY,		0},			// emit light from an environment map
	{"lightfilter",		0,	SURF_LIGHTFILTER, 0},		// filter light going through it
	{"alphashadow",		0,	SURF_ALPHASHADOW, 0},		// test light on a per-pixel basis
	{"hint",			0,	SURF_HINT,		0},			// use as a primary splitter
	
	{"discrete",		0,	0,				0},

	// server attributes
	{"slick",			0,	SURF_SLICK,		0},
	{"noimpact",		0,	SURF_NOIMPACT,	0},			// don't make impact explosions or marks
	{"nomarks",			0,	SURF_NOMARKS,	0},			// don't make impact marks, but still explode
	{"ladder",			0,	SURF_LADDER,	0},
	{"nodamage",		0,	SURF_NODAMAGE,	0},
	{"metalsteps",		0,	SURF_METALSTEPS,0},
	{"flesh",			0,	SURF_FLESH,		0},
	{"nosteps",			0,	SURF_NOSTEPS,	0},
	
	// surface types for sound effects and blood splats
	{"metal",			0,	0,				0},
	{"stone",			0,	0,				0},
	{"wood",			0,	0,				0},
	{"cardboard",		0,	0,				0},
	{"liquid",			0,	0,				0},
	{"glass",			0,	0,				0},
	{"plastic",			0,	0,				0},
	{"ricochet",		0,	0,				0},

	// drawsurf attributes
	{"nodraw",			0,	SURF_NODRAW,		0},		// don't generate a drawsurface (or a lightmap)
	{"pointlight",		0,	SURF_POINTLIGHT,	0},		// sample lighting at vertexes
	{"nolightmap",		0,	SURF_NOLIGHTMAP,	0},		// don't generate a lightmap
	{"nodlight",		0,	SURF_NODLIGHT,		0},		// don't ever add dynamic lights
	{"dust",			0,	SURF_DUST,			0},		// leave a dust trail when walking on this surface
	
	{"noshadows",		0,	0,				0},
	{"noselfshadow",	0,	0,				0},
	{"forceshadows",	0,	0,				0},
	{"nooverlays",		0,	0,				0},
	{"forceoverlays",	0,	0,				0},
};
// *INDENT-ON*

/*
===============
ParseSurfaceParm

surfaceparm <name>
===============
*/
static qboolean SurfaceParm(const char *token)
{
	int             numInfoParms = sizeof(infoParms) / sizeof(infoParms[0]);
	int             i;
	
	for(i = 0; i < numInfoParms; i++)
	{
		if(!Q_stricmp(token, infoParms[i].name))
		{
			shader.surfaceFlags |= infoParms[i].surfaceFlags;
			shader.contentFlags |= infoParms[i].contents;
#if 0
			if(infoParms[i].clearSolid)
			{
				si->contents &= ~CONTENTS_SOLID;
			}
#endif
			return qtrue;
		}
	}
	
	return qfalse;
}

static void ParseSurfaceParm(char **text)
{
	char           *token;

	token = COM_ParseExt(text, qfalse);
	SurfaceParm(token);
}

static void ParseDiffuseMap(shaderStage_t * stage, char **text)
{
	stage->active = qtrue;
	stage->type = ST_DIFFUSEMAP;
	stage->rgbGen = CGEN_IDENTITY;
	stage->stateBits = GLS_DEFAULT;
	
	ParseMap(stage, text);
}

static void ParseNormalMap(shaderStage_t * stage, char **text)
{
	stage->active = qtrue;
	stage->type = ST_NORMALMAP;
	stage->rgbGen = CGEN_IDENTITY;
	stage->stateBits = GLS_DEFAULT;
	
	ParseMap(stage, text);
}

static void ParseSpecularMap(shaderStage_t * stage, char **text)
{
	stage->active = qtrue;
	stage->type = ST_SPECULARMAP;
	stage->rgbGen = CGEN_IDENTITY;
	stage->stateBits = GLS_DEFAULT;
	
	ParseMap(stage, text);
}

static void ParseLightMap(shaderStage_t * stage, char **text)
{
	stage->active = qtrue;
	stage->type = ST_LIGHTMAP;
	stage->rgbGen = CGEN_IDENTITY;
	stage->stateBits = GLS_DEFAULT;
	
	ParseMap(stage, text);
}

/*
=================
ParseShader

The current text pointer is at the explicit text definition of the
shader.  Parse it into the global shader variable.  Later functions
will optimize it.
=================
*/
static qboolean ParseShader(char **text)
{
	char           *token;
	int             s;

	s = 0;
	shader.explicitlyDefined = qtrue;

	token = COM_ParseExt(text, qtrue);
	if(token[0] != '{')
	{
		ri.Printf(PRINT_WARNING, "WARNING: expecting '{', found '%s' instead in shader '%s'\n", token, shader.name);
		return qfalse;
	}

	while(1)
	{
		token = COM_ParseExt(text, qtrue);
		if(!token[0])
		{
			ri.Printf(PRINT_WARNING, "WARNING: no concluding '}' in shader %s\n", shader.name);
			return qfalse;
		}

		// end of shader definition
		if(token[0] == '}')
		{
			break;
		}
		// stage definition
		else if(token[0] == '{')
		{
			if(!ParseStage(&stages[s], text))
			{
				return qfalse;
			}
			stages[s].active = qtrue;
			s++;
			continue;
		}
		// skip stuff that only the QuakeEdRadient needs
		else if(!Q_stricmpn(token, "qer", 3))
		{
			SkipRestOfLine(text);
			continue;
		}
		// skip description
		else if(!Q_stricmp(token, "description"))
		{
			SkipRestOfLine(text);
			continue;
		}
		// skip renderbump
		else if(!Q_stricmp(token, "renderbump"))
		{
			SkipRestOfLine(text);
			continue;
		}
		// skip unsmoothedTangents
		else if(!Q_stricmp(token, "unsmoothedTangents"))
		{
			continue;
		}
		// skip guiSurf
		else if(!Q_stricmp(token, "guiSurf"))
		{
			SkipRestOfLine(text);
			continue;
		}
		// skip decalInfo
		else if(!Q_stricmp(token, "decalInfo"))
		{
			SkipRestOfLine(text);
			continue;
		}
		// sun parms
		else if(!Q_stricmp(token, "q3map_sun"))
		{
			float           a, b;

			token = COM_ParseExt(text, qfalse);
			if(!token[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parm for 'q3map_sun' keyword in shader '%s'\n", shader.name);
				continue;
			}
			tr.sunLight[0] = atof(token);
			
			token = COM_ParseExt(text, qfalse);
			if(!token[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parm for 'q3map_sun' keyword in shader '%s'\n", shader.name);
				continue;
			}
			tr.sunLight[1] = atof(token);
			
			
			token = COM_ParseExt(text, qfalse);
			if(!token[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parm for 'q3map_sun' keyword in shader '%s'\n", shader.name);
				continue;
			}
			tr.sunLight[2] = atof(token);

			VectorNormalize(tr.sunLight);

			token = COM_ParseExt(text, qfalse);
			if(!token[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parm for 'q3map_sun' keyword in shader '%s'\n", shader.name);
				continue;
			}
			a = atof(token);
			VectorScale(tr.sunLight, a, tr.sunLight);

			token = COM_ParseExt(text, qfalse);
			if(!token[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parm for 'q3map_sun' keyword in shader '%s'\n", shader.name);
				continue;
			}
			a = atof(token);
			a = a / 180 * M_PI;

			token = COM_ParseExt(text, qfalse);
			if(!token[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parm for 'q3map_sun' keyword in shader '%s'\n", shader.name);
				continue;
			}
			b = atof(token);
			b = b / 180 * M_PI;

			tr.sunDirection[0] = cos(a) * cos(b);
			tr.sunDirection[1] = sin(a) * cos(b);
			tr.sunDirection[2] = sin(b);
		}
		// translucent
		else if(!Q_stricmp(token, "translucent"))
		{
			shader.translucent = qtrue;
			continue;
		}
		// forceOpaque
		else if(!Q_stricmp(token, "forceOpaque"))
		{
			shader.forceOpaque = qtrue;
			continue;
		}
		// forceSolid
		else if(!Q_stricmp(token, "forceSolid") || !Q_stricmp(token, "solid"))
		{
			continue;
		}
		else if(!Q_stricmp(token, "deformVertexes") || !Q_stricmp(token, "deform"))
		{
			ParseDeform(text);
			continue;
		}
		else if(!Q_stricmp(token, "tesssize"))
		{
			SkipRestOfLine(text);
			continue;
		}
		// skip noFragment
		if(!Q_stricmp(token, "noFragment"))
		{
			continue;
		}
		else if(!Q_stricmp(token, "clampTime"))
		{
			token = COM_ParseExt(text, qfalse);
			if(token[0])
			{
				shader.clampTime = atof(token);
			}
		}
		// skip stuff that only the q3map needs
		else if(!Q_stricmpn(token, "q3map", 5))
		{
			SkipRestOfLine(text);
			continue;
		}
		// skip stuff that only q3map or the server needs
		else if(!Q_stricmp(token, "surfaceParm"))
		{
			ParseSurfaceParm(text);
			continue;
		}
		// no mip maps
		else if(!Q_stricmp(token, "nomipmaps"))
		{
			shader.noMipMaps = qtrue;
			shader.noPicMip = qtrue;
			continue;
		}
		// no picmip adjustment
		else if(!Q_stricmp(token, "nopicmip"))
		{
			shader.noPicMip = qtrue;
			continue;
		}
		// polygonOffset
		else if(!Q_stricmp(token, "polygonOffset"))
		{
			shader.polygonOffset = qtrue;
			
			token = COM_ParseExt(text, qfalse);
			if(token[0])
			{
				shader.polygonOffsetValue = atof(token);
			}
			continue;
		}
		// entityMergable, allowing sprite surfaces from multiple entities
		// to be merged into one batch.  This is a savings for smoke
		// puffs and blood, but can't be used for anything where the
		// shader calcs (not the surface function) reference the entity color or scroll
		else if(!Q_stricmp(token, "entityMergable"))
		{
			shader.entityMergable = qtrue;
			continue;
		}
		// fogParms
		else if(!Q_stricmp(token, "fogParms"))
		{
			if(!ParseVector(text, 3, shader.fogParms.color))
			{
				return qfalse;
			}

			token = COM_ParseExt(text, qfalse);
			if(!token[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parm for 'fogParms' keyword in shader '%s'\n", shader.name);
				continue;
			}
			shader.fogParms.depthForOpaque = atof(token);

			// skip any old gradient directions
			SkipRestOfLine(text);
			continue;
		}
		// noFog
		else if(!Q_stricmp(token, "noFog"))
		{
			// TODO: implement ?
			continue;
		}
		// portal
		else if(!Q_stricmp(token, "portal") || !Q_stricmp(token, "mirror"))
		{
			shader.sort = SS_PORTAL;
			continue;
		}
		// skyparms <cloudheight> <outerbox> <innerbox>
		else if(!Q_stricmp(token, "skyparms"))
		{
			ParseSkyParms(text);
			continue;
		}
		// light <value> determines flaring in q3map, not needed here
		else if(!Q_stricmp(token, "light"))
		{
			token = COM_ParseExt(text, qfalse);
			continue;
		}
		// cull <face>
		else if(!Q_stricmp(token, "cull"))
		{
			token = COM_ParseExt(text, qfalse);
			if(token[0] == 0)
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing cull parms in shader '%s'\n", shader.name);
				continue;
			}

			if(!Q_stricmp(token, "none") || !Q_stricmp(token, "twoSided") || !Q_stricmp(token, "disable"))
			{
				shader.cullType = CT_TWO_SIDED;
			}
			else if(!Q_stricmp(token, "back") || !Q_stricmp(token, "backside") ||
					!Q_stricmp(token, "backsided"))
			{
				shader.cullType = CT_BACK_SIDED;
			}
			else
			{
				ri.Printf(PRINT_WARNING, "WARNING: invalid cull parm '%s' in shader '%s'\n", token,
						  shader.name);
			}
			continue;
		}
		// twoSided
		else if(!Q_stricmp(token, "twoSided"))
		{
			shader.cullType = CT_TWO_SIDED;
			continue;
		}
		// backSided
		else if(!Q_stricmp(token, "backSided"))
		{
			shader.cullType = CT_BACK_SIDED;
			continue;
		}
		// clamp
		else if(!Q_stricmp(token, "clamp"))
		{
			shader.wrapType = WT_CLAMP;
			continue;
		}
		// edgeClamp
		else if(!Q_stricmp(token, "edgeClamp"))
		{
			shader.wrapType = WT_EDGE_CLAMP;
			continue;
		}
		// zeroClamp
		else if(!Q_stricmp(token, "zeroclamp"))
		{
			shader.wrapType = WT_ZERO_CLAMP;
			continue;
		}
		// alphaZeroClamp
		else if(!Q_stricmp(token, "alphaZeroClamp"))
		{
			shader.wrapType = WT_ALPHA_ZERO_CLAMP;
			continue;
		}
		// sort
		else if(!Q_stricmp(token, "sort"))
		{
			ParseSort(text);
			continue;
		}
		// spectrum
		else if(!Q_stricmp(token, "spectrum"))
		{
			token = COM_ParseExt(text, qfalse);
			if(!token[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parm for 'spectrum' keyword in shader '%s'\n", shader.name);
				continue;
			}
			shader.spectrum = qtrue;
			shader.spectrumValue = atoi(token);
			continue;
		}
		// diffuseMap <image>
		else if(!Q_stricmp(token, "diffuseMap"))
		{
			ParseDiffuseMap(&stages[s], text);
			s++;
			continue;
		}
		// normalMap <image>
		else if(!Q_stricmp(token, "normalMap") || !Q_stricmp(token, "bumpMap"))
		{
			ParseNormalMap(&stages[s], text);
			s++;
			continue;
		}
		// specularMap <image>
		else if(!Q_stricmp(token, "specularMap"))
		{
			ParseSpecularMap(&stages[s], text);
			s++;
			continue;
		}
		// lightMap <image>
		else if(!Q_stricmp(token, "lightMap"))
		{
			ParseLightMap(&stages[s], text);
			s++;
			continue;
		}
		// DECAL_MACRO
		else if(!Q_stricmp(token, "DECAL_MACRO"))
		{
			shader.polygonOffset = qtrue;
			shader.polygonOffsetValue = 1;
			shader.sort = SS_DECAL;
			SurfaceParm("discrete");
			SurfaceParm("noShadows");
			continue;
		}
		else
		{
			if(!SurfaceParm(token))
			{
				ri.Printf(PRINT_WARNING, "WARNING: unknown general shader parameter '%s' in '%s'\n", token, shader.name);
				return qfalse;
			}
		}
	}

	// ignore shaders that don't have any stages, unless it is a sky or fog
	if(s == 0 && !shader.forceOpaque && !shader.isSky && !(shader.contentFlags & CONTENTS_FOG))
	{
		return qfalse;
	}

	return qtrue;
}

/*
========================================================================================

SHADER OPTIMIZATION AND FOGGING

========================================================================================
*/

/*
===================
ComputeStageIteratorFunc

See if we can use on of the simple fastpath stage functions,
otherwise set to the generic stage function
===================
*/
/*
static void ComputeStageIteratorFunc(void)
{
	shader.optimalStageIteratorFunc = RB_StageIteratorGeneric;

	//
	// see if this should go into the sky path
	//
	if(shader.isSky)
	{
		shader.optimalStageIteratorFunc = RB_StageIteratorSky;
		goto done;
	}

	if(r_ignoreFastPath->integer)
	{
		return;
	}

	//
	// see if this can go into the vertex lit or per pixel lit fast path
	//
	if(shader.numUnfoggedPasses == 1)
	{
		if(stages[0].rgbGen == CGEN_LIGHTING_DIFFUSE)
		{
			if(stages[0].alphaGen == AGEN_IDENTITY)
			{
				if(stages[0].bundle[0].tcGen == TCGEN_TEXTURE)
				{
					if(!shader.polygonOffset)
					{
						if(!shader.multitextureEnv)
						{
							if(!shader.numDeforms)
							{
								if(glConfig2.shadingLanguage100Available)
								{
									if(stages[0].bundle[TB_NORMALMAP].isNormalMap && r_bumpMapping->integer)
									{
										if(stages[0].bundle[TB_SPECULARMAP].isSpecularMap && r_specular->integer)
										{
											shader.optimalStageIteratorFunc = RB_StageIteratorPerPixelLit_DBS;
										}
										else
										{
											shader.optimalStageIteratorFunc = RB_StageIteratorPerPixelLit_DB;
										}
									}
									else
									{
										shader.optimalStageIteratorFunc = RB_StageIteratorPerPixelLit_D;
									}
									goto done;
								}
								else
								{
									shader.optimalStageIteratorFunc = RB_StageIteratorVertexLitTexture;
									goto done;
								}
							}
						}
					}
				}
			}
		}
	}

	//
	// see if this can go into an optimized LM, multitextured path
	//
	if(shader.numUnfoggedPasses == 1)
	{
		if((stages[0].rgbGen == CGEN_IDENTITY) && (stages[0].alphaGen == AGEN_IDENTITY))
		{
			if(stages[0].bundle[0].tcGen == TCGEN_TEXTURE && stages[0].bundle[1].tcGen == TCGEN_LIGHTMAP)
			{
				if(!shader.polygonOffset)
				{
					if(!shader.numDeforms)
					{
						if(shader.multitextureEnv)
						{
							shader.optimalStageIteratorFunc = RB_StageIteratorLightmappedMultitexture;
							goto done;
						}
					}
				}
			}
		}
	}

  done:
	return;
}
*/

typedef struct
{
	int             blendA;
	int             blendB;

	int             multitextureEnv;
	int             multitextureBlend;
} collapse_t;

static collapse_t collapse[] = {
	{0, GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO,
	 GL_MODULATE, 0},

	{0, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR,
	 GL_MODULATE, 0},

	{GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR,
	 GL_MODULATE, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR},

	{GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR,
	 GL_MODULATE, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR},

	{GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR, GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO,
	 GL_MODULATE, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR},

	{GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO, GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO,
	 GL_MODULATE, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR},

	{0, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE,
	 GL_ADD, 0},

	{GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE,
	 GL_ADD, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE},
#if 0
	{0, GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_SRCBLEND_SRC_ALPHA,
	 GL_DECAL, 0},
#endif
	{-1}
};

/*
================
CollapseMultitexture

Attempt to combine two color stages into a single multitexture stage
FIXME: I think modulated add + modulated add collapses incorrectly
=================
*/
// *INDENT-OFF*
static void CollapseStages(void)
{
	int             abits, bbits;
	int             i;
	textureBundle_t tmpBundle;
	
	qboolean		hasDiffuseStage;
	qboolean		hasNormalStage;
	qboolean		hasSpecularStage;
	qboolean		hasLightStage;
	
	shaderStage_t	tmpDiffuseStage;
	shaderStage_t	tmpNormalStage;
	shaderStage_t	tmpSpecularStage;
	shaderStage_t	tmpLightStage;
	
	if(!qglActiveTextureARB || !r_collapseMultitexture->integer)
	{
		return;
	}
	
	hasDiffuseStage = qfalse;
	hasNormalStage = qfalse;
	hasSpecularStage = qfalse;
	hasLightStage = qfalse;
	
	for(i = 0; i < 4; i++)
	{
		if(!stages[i].active)
			continue;
		
		if(stages[i].type == ST_DIFFUSEMAP && !hasDiffuseStage)
		{
			hasDiffuseStage = qtrue;
			tmpDiffuseStage = stages[i];
		}
		else if(stages[i].type == ST_NORMALMAP && !hasNormalStage)
		{
			hasNormalStage = qtrue;
			tmpNormalStage = stages[i];
		}
		else if(stages[i].type == ST_SPECULARMAP && !hasSpecularStage)
		{
			hasSpecularStage = qtrue;
			tmpSpecularStage = stages[i];
		}
		else if(stages[i].type == ST_LIGHTMAP && !hasLightStage)
		{
			hasLightStage = qtrue;
			tmpLightStage = stages[i];
		}
	}

	
	// NOTE: Tr3B - merge as many stages as possible
	
	// try to merge diffuse/normal/specular/light
	if(	hasDiffuseStage		&&
		hasNormalStage		&&
		hasSpecularStage	&&
		hasLightStage
	)
	{
		shader.collapseType = COLLAPSE_lighting_DBS_radiosity;
		
		stages[0] = tmpDiffuseStage;
		stages[0].type = ST_COLLAPSE_lighting_DBS_radiosity;
		
		stages[0].bundle[TB_NORMALMAP] = tmpNormalStage.bundle[0];
		stages[0].bundle[TB_SPECULARMAP] = tmpSpecularStage.bundle[0];
		stages[0].bundle[TB_LIGHTMAP] = tmpLightStage.bundle[0];
		
		// make sure that the diffuse stage won't have any blending bits
		//stages[0].stateBits &= ~(GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS);

		// move down subsequent stages
		memmove(&stages[1], &stages[2], sizeof(stages[0]) * (MAX_SHADER_STAGES - 4));
		Com_Memset(&stages[MAX_SHADER_STAGES - 3], 0, sizeof(stages[0]));
		
		shader.numUnfoggedPasses -= 3;
	}
	// try to merge diffuse/bump/light
	else if(hasDiffuseStage		&&
			hasNormalStage		&&
			hasLightStage
	)
	{
		shader.collapseType = COLLAPSE_lighting_DB_radiosity;
		
		stages[0] = tmpDiffuseStage;
		stages[0].type = ST_COLLAPSE_lighting_DB_radiosity;
		
		stages[0].bundle[TB_NORMALMAP] = tmpNormalStage.bundle[0];
		stages[0].bundle[TB_LIGHTMAP] = tmpLightStage.bundle[0];

		// move down subsequent stages
		memmove(&stages[1], &stages[2], sizeof(stages[0]) * (MAX_SHADER_STAGES - 3));
		Com_Memset(&stages[MAX_SHADER_STAGES - 2], 0, sizeof(stages[0]));
		
		shader.numUnfoggedPasses -= 2;
	}
	else if(hasDiffuseStage		&&
			hasSpecularStage	&&
			hasLightStage
	)
	{
		shader.collapseType = COLLAPSE_lighting_D_radiosity;
		
		stages[0] = tmpDiffuseStage;
		stages[0].type = ST_COLLAPSE_lighting_D_radiosity;
		
		stages[0].bundle[TB_LIGHTMAP] = tmpLightStage.bundle[0];

		// move down subsequent stages, kill specular stage
		memmove(&stages[1], &stages[3], sizeof(stages[0]) * (MAX_SHADER_STAGES - 3));
		Com_Memset(&stages[MAX_SHADER_STAGES - 2], 0, sizeof(stages[0]));
		
		shader.numUnfoggedPasses -= 2;
	}
	// try to merge diffuse/light
	else if(hasDiffuseStage		&&
			hasLightStage
	)
	{
		shader.collapseType = COLLAPSE_lighting_D_radiosity;
		
		stages[0] = tmpDiffuseStage;
		stages[0].type = ST_COLLAPSE_lighting_D_radiosity;
		
		stages[0].bundle[TB_LIGHTMAP] = tmpLightStage.bundle[0];

		// move down subsequent stages
		memmove(&stages[1], &stages[2], sizeof(stages[0]) * (MAX_SHADER_STAGES - 2));
		Com_Memset(&stages[MAX_SHADER_STAGES - 1], 0, sizeof(stages[0]));
		
		shader.numUnfoggedPasses -= 1;
	}
	// try to merge diffuse/normal/specular
	else if(hasDiffuseStage		&&
			hasNormalStage		&&
			hasSpecularStage	&&
			tmpDiffuseStage.rgbGen == CGEN_LIGHTING_DIFFUSE
	)
	{
		shader.collapseType = COLLAPSE_lighting_DBS_direct;
		
		stages[0] = tmpDiffuseStage;
		stages[0].type = ST_COLLAPSE_lighting_DBS_direct;
		
		stages[0].bundle[TB_NORMALMAP] = tmpNormalStage.bundle[0];
		stages[0].bundle[TB_SPECULARMAP] = tmpSpecularStage.bundle[0];

		// move down subsequent stages
		memmove(&stages[1], &stages[2], sizeof(stages[0]) * (MAX_SHADER_STAGES - 3));
		Com_Memset(&stages[MAX_SHADER_STAGES - 2], 0, sizeof(stages[0]));
		
		shader.numUnfoggedPasses -= 2;
	}
	// try to merge diffuse/normal
	else if(hasDiffuseStage		&&
			hasNormalStage		&&
			tmpDiffuseStage.rgbGen == CGEN_LIGHTING_DIFFUSE
	)
	{
		shader.collapseType = COLLAPSE_lighting_DB_direct;
		
		stages[0] = tmpDiffuseStage;
		stages[0].type = ST_COLLAPSE_lighting_DB_direct;
		
		stages[0].bundle[TB_NORMALMAP] = tmpNormalStage.bundle[0];

		// move down subsequent stages
		memmove(&stages[1], &stages[2], sizeof(stages[0]) * (MAX_SHADER_STAGES - 2));
		Com_Memset(&stages[MAX_SHADER_STAGES - 1], 0, sizeof(stages[0]));
		
		shader.numUnfoggedPasses -= 1;
	}
	// try to merge color/color
	else if(stages[0].active					&&
			stages[0].type == ST_COLORMAP		&&
			stages[1].active					&&
			stages[1].type == ST_COLORMAP
	)
	{
		abits = stages[0].stateBits;
		bbits = stages[1].stateBits;

		// make sure that both stages have identical state other than blend modes
		if((abits & ~(GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS | GLS_DEPTHMASK_TRUE)) !=
		   (bbits & ~(GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS | GLS_DEPTHMASK_TRUE)))
		{
			return;
		}

		abits &= (GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS);
		bbits &= (GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS);

		// search for a valid multitexture blend function
		for(i = 0; collapse[i].blendA != -1; i++)
		{
			if(abits == collapse[i].blendA && bbits == collapse[i].blendB)
			{
				break;
			}
		}

		// nothing found
		if(collapse[i].blendA == -1)
		{
			return;
		}

		// GL_ADD is a separate extension
		if(collapse[i].multitextureEnv == GL_ADD && !glConfig.textureEnvAddAvailable)
		{
			return;
		}

		// make sure waveforms have identical parameters
		if((stages[0].rgbGen != stages[1].rgbGen) || (stages[0].alphaGen != stages[1].alphaGen))
		{
			return;
		}

		// an add collapse can only have identity colors
		if(collapse[i].multitextureEnv == GL_ADD && stages[0].rgbGen != CGEN_IDENTITY)
		{
			return;
		}

		if(stages[0].rgbGen == CGEN_WAVEFORM)
		{
			if(memcmp(&stages[0].rgbWave, &stages[1].rgbWave, sizeof(stages[0].rgbWave)))
			{
				return;
			}
		}
		if(stages[0].alphaGen == CGEN_WAVEFORM)
		{
			if(memcmp(&stages[0].alphaWave, &stages[1].alphaWave, sizeof(stages[0].alphaWave)))
			{
				return;
			}
		}

		// make sure that lightmaps are in bundle 1 for 3dfx
		if(stages[0].bundle[0].isLightMap)
		{
			tmpBundle = stages[0].bundle[0];
			stages[0].bundle[0] = stages[1].bundle[0];
			stages[0].bundle[1] = tmpBundle;
		}
		else
		{
			stages[0].bundle[1] = stages[1].bundle[0];
		}

		// set the new blend state bits
		shader.collapseType = COLLAPSE_Generic_multi;
		shader.collapseTextureEnv = collapse[i].multitextureEnv;
		stages[0].type = ST_COLLAPSE_Generic_multi;
		stages[0].stateBits &= ~(GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS);
		stages[0].stateBits |= collapse[i].multitextureBlend;
	
		// move down subsequent stages
		memmove(&stages[1], &stages[2], sizeof(stages[0]) * (MAX_SHADER_STAGES - 2));
		Com_Memset(&stages[MAX_SHADER_STAGES - 1], 0, sizeof(stages[0]));

		shader.numUnfoggedPasses -= 1;
	}
}
// *INDENT-ON*

/*
=============

FixRenderCommandList
https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=493
Arnout: this is a nasty issue. Shaders can be registered after drawsurfaces are generated
but before the frame is rendered. This will, for the duration of one frame, cause drawsurfaces
to be rendered with bad shaders. To fix this, need to go through all render commands and fix
sortedIndex.
==============
*/
static void FixRenderCommandList(int newShader)
{
	renderCommandList_t *cmdList = &backEndData[tr.smpFrame]->commands;

	if(cmdList)
	{
		const void     *curCmd = cmdList->cmds;

		while(1)
		{
			switch (*(const int *)curCmd)
			{
				case RC_SET_COLOR:
				{
					const setColorCommand_t *sc_cmd = (const setColorCommand_t *)curCmd;

					curCmd = (const void *)(sc_cmd + 1);
					break;
				}
				case RC_STRETCH_PIC:
				{
					const stretchPicCommand_t *sp_cmd = (const stretchPicCommand_t *)curCmd;

					curCmd = (const void *)(sp_cmd + 1);
					break;
				}
				case RC_DRAW_SURFS:
				{
					int             i;
					drawSurf_t     *drawSurf;
					shader_t       *shader;
					int             fogNum;
					int             entityNum;
					int             dlightMap;
					int             sortedIndex;
					const drawSurfsCommand_t *ds_cmd = (const drawSurfsCommand_t *)curCmd;

					for(i = 0, drawSurf = ds_cmd->drawSurfs; i < ds_cmd->numDrawSurfs; i++, drawSurf++)
					{
						R_DecomposeSort(drawSurf->sort, &entityNum, &shader, &fogNum, &dlightMap);
						sortedIndex = ((drawSurf->sort >> QSORT_SHADERNUM_SHIFT) & (MAX_SHADERS - 1));
						if(sortedIndex >= newShader)
						{
							sortedIndex++;
							drawSurf->sort =
								(sortedIndex << QSORT_SHADERNUM_SHIFT) | entityNum | (fogNum <<
																					  QSORT_FOGNUM_SHIFT) |
								(int)dlightMap;
						}
					}
					curCmd = (const void *)(ds_cmd + 1);
					break;
				}
				case RC_DRAW_BUFFER:
				{
					const drawBufferCommand_t *db_cmd = (const drawBufferCommand_t *)curCmd;

					curCmd = (const void *)(db_cmd + 1);
					break;
				}
				case RC_SWAP_BUFFERS:
				{
					const swapBuffersCommand_t *sb_cmd = (const swapBuffersCommand_t *)curCmd;

					curCmd = (const void *)(sb_cmd + 1);
					break;
				}
				case RC_END_OF_LIST:
				default:
					return;
			}
		}
	}
}

/*
==============
SortNewShader

Positions the most recently created shader in the tr.sortedShaders[]
array so that the shader->sort key is sorted reletive to the other
shaders.

Sets shader->sortedIndex
==============
*/
static void SortNewShader(void)
{
	int             i;
	float           sort;
	shader_t       *newShader;

	newShader = tr.shaders[tr.numShaders - 1];
	sort = newShader->sort;

	for(i = tr.numShaders - 2; i >= 0; i--)
	{
		if(tr.sortedShaders[i]->sort <= sort)
		{
			break;
		}
		tr.sortedShaders[i + 1] = tr.sortedShaders[i];
		tr.sortedShaders[i + 1]->sortedIndex++;
	}

	// Arnout: fix rendercommandlist
	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=493
	FixRenderCommandList(i + 1);

	newShader->sortedIndex = i + 1;
	tr.sortedShaders[i + 1] = newShader;
}


/*
====================
GeneratePermanentShader
====================
*/
static shader_t *GeneratePermanentShader(void)
{
	shader_t       *newShader;
	int             i, b;
	int             size, hash;

	if(tr.numShaders == MAX_SHADERS)
	{
		ri.Printf(PRINT_WARNING, "WARNING: GeneratePermanentShader - MAX_SHADERS hit\n");
		return tr.defaultShader;
	}

	newShader = ri.Hunk_Alloc(sizeof(shader_t), h_low);

	*newShader = shader;

	if(shader.sort <= SS_OPAQUE)
	{
		newShader->fogPass = FP_EQUAL;
	}
	else if(shader.contentFlags & CONTENTS_FOG)
	{
		newShader->fogPass = FP_LE;
	}

	tr.shaders[tr.numShaders] = newShader;
	newShader->index = tr.numShaders;

	tr.sortedShaders[tr.numShaders] = newShader;
	newShader->sortedIndex = tr.numShaders;

	tr.numShaders++;

	for(i = 0; i < newShader->numUnfoggedPasses; i++)
	{
		if(!stages[i].active)
		{
			break;
		}
		newShader->stages[i] = ri.Hunk_Alloc(sizeof(stages[i]), h_low);
		*newShader->stages[i] = stages[i];

		for(b = 0; b < MAX_TEXTURE_BUNDLES; b++)
		{
			size = newShader->stages[i]->bundle[b].numTexMods * sizeof(texModInfo_t);
			newShader->stages[i]->bundle[b].texMods = ri.Hunk_Alloc(size, h_low);
			Com_Memcpy(newShader->stages[i]->bundle[b].texMods, stages[i].bundle[b].texMods, size);
		}
	}

	SortNewShader();

	hash = generateHashValue(newShader->name, FILE_HASH_SIZE);
	newShader->next = shaderHashTable[hash];
	shaderHashTable[hash] = newShader;

	return newShader;
}

/*
=================
VertexLightingCollapse

If vertex lighting is enabled, only render a single
pass, trying to guess which is the correct one to best aproximate
what it is supposed to look like.
=================
*/
/*
static void VertexLightingCollapse(void)
{
	int             stage;
	shaderStage_t  *bestStage;
	int             bestImageRank;
	int             rank;

	// if we aren't opaque, just use the first pass
	if(shader.sort == SS_OPAQUE)
	{

		// pick the best texture for the single pass
		bestStage = &stages[0];
		bestImageRank = -999999;

		for(stage = 0; stage < MAX_SHADER_STAGES; stage++)
		{
			shaderStage_t  *pStage = &stages[stage];

			if(!pStage->active)
			{
				break;
			}
			rank = 0;

			if(pStage->bundle[0].isLightmap)
			{
				rank -= 100;
			}
			if(pStage->bundle[0].tcGen != TCGEN_TEXTURE)
			{
				rank -= 5;
			}
			if(pStage->bundle[0].numTexMods)
			{
				rank -= 5;
			}
			if(pStage->rgbGen != CGEN_IDENTITY && pStage->rgbGen != CGEN_IDENTITY_LIGHTING)
			{
				rank -= 3;
			}

			if(rank > bestImageRank)
			{
				bestImageRank = rank;
				bestStage = pStage;
			}
		}

		stages[0].bundle[0] = bestStage->bundle[0];
		stages[0].stateBits &= ~(GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS);
		stages[0].stateBits |= GLS_DEPTHMASK_TRUE;
		if(shader.lightmapIndex == LIGHTMAP_NONE)
		{
			stages[0].rgbGen = CGEN_LIGHTING_DIFFUSE;
		}
		else
		{
			stages[0].rgbGen = CGEN_EXACT_VERTEX;
		}
		stages[0].alphaGen = AGEN_SKIP;
	}
	else
	{
		// don't use a lightmap (tesla coils)
		if(stages[0].bundle[0].isLightmap)
		{
			stages[0] = stages[1];
		}

		// if we were in a cross-fade cgen, hack it to normal
		if(stages[0].rgbGen == CGEN_ONE_MINUS_ENTITY || stages[1].rgbGen == CGEN_ONE_MINUS_ENTITY)
		{
			stages[0].rgbGen = CGEN_IDENTITY_LIGHTING;
		}
		if((stages[0].rgbGen == CGEN_WAVEFORM && stages[0].rgbWave.func == GF_SAWTOOTH)
		   && (stages[1].rgbGen == CGEN_WAVEFORM && stages[1].rgbWave.func == GF_INVERSE_SAWTOOTH))
		{
			stages[0].rgbGen = CGEN_IDENTITY_LIGHTING;
		}
		if((stages[0].rgbGen == CGEN_WAVEFORM && stages[0].rgbWave.func == GF_INVERSE_SAWTOOTH)
		   && (stages[1].rgbGen == CGEN_WAVEFORM && stages[1].rgbWave.func == GF_SAWTOOTH))
		{
			stages[0].rgbGen = CGEN_IDENTITY_LIGHTING;
		}
	}

	for(stage = 1; stage < MAX_SHADER_STAGES; stage++)
	{
		shaderStage_t  *pStage = &stages[stage];

		if(!pStage->active)
		{
			break;
		}

		Com_Memset(pStage, 0, sizeof(*pStage));
	}
}
*/

/*
=========================
FinishShader

Returns a freshly allocated shader with all the needed info
from the current global working shader
=========================
*/
static shader_t *FinishShader(void)
{
	int             stage;
	qboolean		hasDiffuseMapStage;
	qboolean        hasLightMapStage;

	hasDiffuseMapStage = qfalse;
	hasLightMapStage = qfalse;

	// set sky stuff appropriate
	if(shader.isSky)
	{
		shader.sort = SS_ENVIRONMENT;
	}

	// set polygon offset
	if(shader.polygonOffset && !shader.sort)
	{
		shader.sort = SS_DECAL;
	}

	// set appropriate stage information
	for(stage = 0; stage < MAX_SHADER_STAGES; stage++)
	{
		shaderStage_t  *pStage = &stages[stage];

		if(!pStage->active)
		{
			break;
		}

		// check for a missing texture
		switch(pStage->type)
		{
			case ST_COLORMAP:
			default:
			{
				if(!pStage->bundle[0].image[0])
				{
					ri.Printf(PRINT_WARNING, "Shader %s has a colormap stage with no image\n", shader.name);
					pStage->active = qfalse;
					continue;
				}
				break;
			}
			
			case ST_DIFFUSEMAP:
			{
				if(!pStage->bundle[0].image[0])
				{
					ri.Printf(PRINT_WARNING, "Shader %s has a diffusemap stage with no image\n", shader.name);
					pStage->bundle[0].isDiffuseMap = qtrue;
					pStage->bundle[0].image[0] = tr.defaultImage;
				}
				break;
			}
			
			case ST_NORMALMAP:
			{
				if(!pStage->bundle[0].image[0])
				{
					ri.Printf(PRINT_WARNING, "Shader %s has a normalmap stage with no image\n", shader.name);
					pStage->active = qfalse;
					continue;
				}
				break;
			}
			
			case ST_SPECULARMAP:
			{
				if(!pStage->bundle[0].image[0])
				{
					ri.Printf(PRINT_WARNING, "Shader %s has a specularmap stage with no image\n", shader.name);
					pStage->active = qfalse;
					continue;
				}
				break;
			}
			
			case ST_LIGHTMAP:
			{
				if(!pStage->bundle[0].image[0] || !pStage->bundle[0].isLightMap)
				{
					ri.Printf(PRINT_WARNING, "Shader %s has a lightmap stage with no image\n", shader.name);
					pStage->active = qfalse;
					continue;
				}
				break;
			}
		}

		// ditch this stage if it's detail and detail textures are disabled
		if(pStage->isDetail && !r_detailTextures->integer)
		{
			if(stage < (MAX_SHADER_STAGES - 1))
			{
				memmove(pStage, pStage + 1, sizeof(*pStage) * (MAX_SHADER_STAGES - stage - 1));
				Com_Memset(pStage + 1, 0, sizeof(*pStage));
			}
			continue;
		}

		// default texture coordinate generation
		switch(pStage->type)
		{
			case ST_COLORMAP:
			{
				if(pStage->bundle[0].isLightMap)
				{
					if(pStage->bundle[0].tcGen == TCGEN_BAD)
					{
						pStage->bundle[0].tcGen = TCGEN_LIGHTMAP;
					}
				}
				else
				{
					if(pStage->bundle[0].tcGen == TCGEN_BAD)
					{
						pStage->bundle[0].tcGen = TCGEN_TEXTURE;
					}
				}
				break;
			}
			
			case ST_DIFFUSEMAP:
				hasDiffuseMapStage = qtrue;
			case ST_NORMALMAP:
			case ST_SPECULARMAP:
			{
				if(pStage->bundle[0].tcGen == TCGEN_BAD)
				{
					pStage->bundle[0].tcGen = TCGEN_TEXTURE;
				}
				break;
			}
			
			case ST_LIGHTMAP:
			{
				if(pStage->bundle[0].tcGen == TCGEN_BAD)
				{
					pStage->bundle[0].tcGen = TCGEN_LIGHTMAP;
				}
				hasLightMapStage = qtrue;
				break;
			}
		
			default:
			{
				break;	
			}
		}


		// determine sort order and fog color adjustment
		if((pStage->stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) &&
		   (stages[0].stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)))
		{
			int             blendSrcBits = pStage->stateBits & GLS_SRCBLEND_BITS;
			int             blendDstBits = pStage->stateBits & GLS_DSTBLEND_BITS;

			// fog color adjustment only works for blend modes that have a contribution
			// that aproaches 0 as the modulate values aproach 0 --
			// GL_ONE, GL_ONE
			// GL_ZERO, GL_ONE_MINUS_SRC_COLOR
			// GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA

			// modulate, additive
			if(((blendSrcBits == GLS_SRCBLEND_ONE) && (blendDstBits == GLS_DSTBLEND_ONE)) ||
			   ((blendSrcBits == GLS_SRCBLEND_ZERO) && (blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR)))
			{
				pStage->adjustColorsForFog = ACFF_MODULATE_RGB;
			}
			// strict blend
			else if((blendSrcBits == GLS_SRCBLEND_SRC_ALPHA) &&
					(blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA))
			{
				pStage->adjustColorsForFog = ACFF_MODULATE_ALPHA;
			}
			// premultiplied alpha
			else if((blendSrcBits == GLS_SRCBLEND_ONE) && (blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA))
			{
				pStage->adjustColorsForFog = ACFF_MODULATE_RGBA;
			}
			else
			{
				// we can't adjust this one correctly, so it won't be exactly correct in fog
			}

			// don't screw with sort order if this is a portal or environment
			if(!shader.sort)
			{
				// see through item, like a grill or grate
				if(pStage->stateBits & GLS_DEPTHMASK_TRUE)
				{
					shader.sort = SS_SEE_THROUGH;
				}
				else
				{
					shader.sort = SS_BLEND0;
				}
			}
		}
	}
	shader.numUnfoggedPasses = stage;

	// there are times when you will need to manually apply a sort to
	// opaque alpha tested shaders that have later blend passes
	if(!shader.sort)
	{
		if(shader.translucent)
			shader.sort = SS_DECAL;
		else
			shader.sort = SS_OPAQUE;
	}

	// look for multitexture potential
	CollapseStages();
	
	if(hasDiffuseMapStage && shader.lightmapIndex >= 0 && !hasLightMapStage)
	{
		ri.Printf(PRINT_WARNING, "WARNING: shader '%s' has lightmap but no lightmap stage!\n", shader.name);
//		ri.Printf(PRINT_DEVELOPER, "WARNING: shader '%s' has lightmap but no lightmap stage!\n", shader.name);
		shader.lightmapIndex = LIGHTMAP_NONE;
	}

	// fogonly shaders don't have any normal passes
	if(shader.numUnfoggedPasses == 0)
	{
		shader.sort = SS_FOG;
	}

	// determine which stage iterator function is appropriate
//	ComputeStageIteratorFunc();

	return GeneratePermanentShader();
}

//========================================================================================

/*
====================
FindShaderInShaderText

Scans the combined text description of all the shader files for
the given shader name.

return NULL if not found

If found, it will return a valid shader
=====================
*/
static char    *FindShaderInShaderText(const char *shadername)
{

	char           *token, *p;

	int             i, hash;

	hash = generateHashValue(shadername, MAX_SHADERTEXT_HASH);

	for(i = 0; shaderTextHashTable[hash][i]; i++)
	{
		p = shaderTextHashTable[hash][i];
		token = COM_ParseExt(&p, qtrue);
		if(!Q_stricmp(token, shadername))
		{
			return p;
		}
	}

	p = s_shaderText;

	if(!p)
	{
		return NULL;
	}

	// look for label
	while(1)
	{
		token = COM_ParseExt(&p, qtrue);
		if(token[0] == 0)
		{
			break;
		}

		if(!Q_stricmp(token, shadername))
		{
			return p;
		}
		else
		{
			// skip the definition
			SkipBracedSection(&p);
		}
	}

	return NULL;
}


/*
==================
R_FindShaderByName

Will always return a valid shader, but it might be the
default shader if the real one can't be found.
==================
*/
shader_t       *R_FindShaderByName(const char *name)
{
	char            strippedName[MAX_QPATH];
	int             hash;
	shader_t       *sh;

	if((name == NULL) || (name[0] == 0))
	{							// bk001205
		return tr.defaultShader;
	}

	COM_StripExtension(name, strippedName);

	hash = generateHashValue(strippedName, FILE_HASH_SIZE);

	// see if the shader is already loaded
	for(sh = shaderHashTable[hash]; sh; sh = sh->next)
	{
		// NOTE: if there was no shader or image available with the name strippedName
		// then a default shader is created with lightmapIndex == LIGHTMAP_NONE, so we
		// have to check all default shaders otherwise for every call to R_FindShader
		// with that same strippedName a new default shader is created.
		if(Q_stricmp(sh->name, strippedName) == 0)
		{
			// match found
			return sh;
		}
	}

	return tr.defaultShader;
}


/*
===============
R_FindShader

Will always return a valid shader, but it might be the
default shader if the real one can't be found.

In the interest of not requiring an explicit shader text entry to
be defined for every single image used in the game, three default
shader behaviors can be auto-created for any image:

If lightmapIndex == LIGHTMAP_NONE, then the image will have
dynamic diffuse lighting applied to it, as apropriate for most
entity skin surfaces.

If lightmapIndex == LIGHTMAP_2D, then the image will be used
for 2D rendering unless an explicit shader is found

If lightmapIndex == LIGHTMAP_BY_VERTEX, then the image will use
the vertex rgba modulate values, as apropriate for misc_model
pre-lit surfaces.

Other lightmapIndex values will have a lightmap stage created
and src*dest blending applied with the texture, as apropriate for
most world construction surfaces.

===============
*/
shader_t       *R_FindShader(const char *name, int lightmapIndex, qboolean mipRawImage)
{
	char            strippedName[MAX_QPATH];
	char            fileName[MAX_QPATH];
	int             i, hash;
	char           *shaderText;
	image_t        *image;
	shader_t       *sh;

	if(name[0] == 0)
	{
		return tr.defaultShader;
	}

	// use (fullbright) vertex lighting if the bsp file doesn't have
	// lightmaps
	if(lightmapIndex >= 0 && lightmapIndex >= tr.numLightmaps)
	{
		lightmapIndex = LIGHTMAP_BY_VERTEX;
	}

	COM_StripExtension(name, strippedName);

	hash = generateHashValue(strippedName, FILE_HASH_SIZE);

	// see if the shader is already loaded
	for(sh = shaderHashTable[hash]; sh; sh = sh->next)
	{
		// NOTE: if there was no shader or image available with the name strippedName
		// then a default shader is created with lightmapIndex == LIGHTMAP_NONE, so we
		// have to check all default shaders otherwise for every call to R_FindShader
		// with that same strippedName a new default shader is created.
		if((sh->lightmapIndex == lightmapIndex || sh->defaultShader) && !Q_stricmp(sh->name, strippedName))
		{
			// match found
			return sh;
		}
	}

	// make sure the render thread is stopped, because we are probably
	// going to have to upload an image
	if(r_smp->integer)
	{
		R_SyncRenderThread();
	}

	// clear the global shader
	Com_Memset(&shader, 0, sizeof(shader));
	Com_Memset(&stages, 0, sizeof(stages));
	Q_strncpyz(shader.name, strippedName, sizeof(shader.name));
	shader.lightmapIndex = lightmapIndex;
	for(i = 0; i < MAX_SHADER_STAGES; i++)
	{
		stages[i].bundle[0].texMods = texMods[i];
	}

	// FIXME: set these "need" values apropriately
	shader.needsTangent = qtrue;
	shader.needsBinormal = qtrue;
	shader.needsNormal = qtrue;
	shader.needsST1 = qtrue;
	shader.needsST2 = qtrue;
	shader.needsColor = qtrue;

	// attempt to define shader from an explicit parameter file
	shaderText = FindShaderInShaderText(strippedName);
	if(shaderText)
	{
		// enable this when building a pak file to get a global list
		// of all explicit shaders
		if(r_printShaders->integer)
		{
			ri.Printf(PRINT_DEVELOPER, "...loading explicit shader '%s'\n", strippedName);
			//ri.Printf(PRINT_ALL, "*SHADER* %s\n", name);
		}

		if(!ParseShader(&shaderText))
		{
			// had errors, so use default shader
			shader.defaultShader = qtrue;
		}
		sh = FinishShader();
		return sh;
	}

	// if not defined in the in-memory shader descriptions,
	// look for a single TGA, BMP, or PCX
	Q_strncpyz(fileName, name, sizeof(fileName));
	COM_DefaultExtension(fileName, sizeof(fileName), ".tga");
	image = R_FindImageFile(fileName, mipRawImage ? IF_NONE : (IF_NOMIPMAPS | IF_NOPICMIP), mipRawImage ? WT_REPEAT : WT_CLAMP);
	if(!image)
	{
		ri.Printf(PRINT_DEVELOPER, "Couldn't find image for shader %s\n", name);
		shader.defaultShader = qtrue;
		return FinishShader();
	}

	// create the default shading commands
	if(shader.lightmapIndex == LIGHTMAP_NONE)
	{
		// dynamic colors at vertexes
		stages[0].bundle[0].image[0] = image;
		stages[0].active = qtrue;
		stages[0].rgbGen = CGEN_LIGHTING_DIFFUSE;
		stages[0].stateBits = GLS_DEFAULT;
	}
	else if(shader.lightmapIndex == LIGHTMAP_BY_VERTEX)
	{
		// explicit colors at vertexes
		stages[0].bundle[0].image[0] = image;
		stages[0].active = qtrue;
		stages[0].rgbGen = CGEN_EXACT_VERTEX;
		stages[0].alphaGen = AGEN_SKIP;
		stages[0].stateBits = GLS_DEFAULT;
	}
	else if(shader.lightmapIndex == LIGHTMAP_2D)
	{
		// GUI elements
		stages[0].bundle[0].image[0] = image;
		stages[0].active = qtrue;
		stages[0].rgbGen = CGEN_VERTEX;
		stages[0].alphaGen = AGEN_VERTEX;
		stages[0].stateBits = GLS_DEPTHTEST_DISABLE |
			GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else if(shader.lightmapIndex == LIGHTMAP_WHITEIMAGE)
	{
		// fullbright level
		stages[0].bundle[0].image[0] = tr.whiteImage;
		stages[0].active = qtrue;
		stages[0].rgbGen = CGEN_IDENTITY_LIGHTING;
		stages[0].stateBits = GLS_DEFAULT;

		stages[1].bundle[0].image[0] = image;
		stages[1].active = qtrue;
		stages[1].rgbGen = CGEN_IDENTITY;
		stages[1].stateBits |= GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
	}
	else
	{
		// two pass lightmap
		stages[0].bundle[0].image[0] = tr.lightmaps[shader.lightmapIndex];
		stages[0].bundle[0].isLightMap = qtrue;
		stages[0].active = qtrue;
		stages[0].rgbGen = CGEN_IDENTITY;	// lightmaps are scaled on creation
		// for identitylight
		stages[0].stateBits = GLS_DEFAULT;

		stages[1].bundle[0].image[0] = image;
		stages[1].active = qtrue;
		stages[1].rgbGen = CGEN_IDENTITY;
		stages[1].stateBits |= GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
	}

	return FinishShader();
}


qhandle_t RE_RegisterShaderFromImage(const char *name, int lightmapIndex, image_t * image, qboolean mipRawImage)
{
	int             i, hash;
	shader_t       *sh;

	hash = generateHashValue(name, FILE_HASH_SIZE);

	// see if the shader is already loaded
	for(sh = shaderHashTable[hash]; sh; sh = sh->next)
	{
		// NOTE: if there was no shader or image available with the name strippedName
		// then a default shader is created with lightmapIndex == LIGHTMAP_NONE, so we
		// have to check all default shaders otherwise for every call to R_FindShader
		// with that same strippedName a new default shader is created.
		if((sh->lightmapIndex == lightmapIndex || sh->defaultShader) &&
		   // index by name
		   !Q_stricmp(sh->name, name))
		{
			// match found
			return sh->index;
		}
	}

	// make sure the render thread is stopped, because we are probably
	// going to have to upload an image
	if(r_smp->integer)
	{
		R_SyncRenderThread();
	}

	// clear the global shader
	Com_Memset(&shader, 0, sizeof(shader));
	Com_Memset(&stages, 0, sizeof(stages));
	Q_strncpyz(shader.name, name, sizeof(shader.name));
	shader.lightmapIndex = lightmapIndex;
	for(i = 0; i < MAX_SHADER_STAGES; i++)
	{
		stages[i].bundle[0].texMods = texMods[i];
	}

	// FIXME: set these "need" values apropriately
	shader.needsTangent = qtrue;
	shader.needsBinormal = qtrue;
	shader.needsNormal = qtrue;
	shader.needsST1 = qtrue;
	shader.needsST2 = qtrue;
	shader.needsColor = qtrue;

	// create the default shading commands
	if(shader.lightmapIndex == LIGHTMAP_NONE)
	{
		// dynamic colors at vertexes
		stages[0].bundle[0].image[0] = image;
		stages[0].active = qtrue;
		stages[0].rgbGen = CGEN_LIGHTING_DIFFUSE;
		stages[0].stateBits = GLS_DEFAULT;
	}
	else if(shader.lightmapIndex == LIGHTMAP_BY_VERTEX)
	{
		// explicit colors at vertexes
		stages[0].bundle[0].image[0] = image;
		stages[0].active = qtrue;
		stages[0].rgbGen = CGEN_EXACT_VERTEX;
		stages[0].alphaGen = AGEN_SKIP;
		stages[0].stateBits = GLS_DEFAULT;
	}
	else if(shader.lightmapIndex == LIGHTMAP_2D)
	{
		// GUI elements
		stages[0].bundle[0].image[0] = image;
		stages[0].active = qtrue;
		stages[0].rgbGen = CGEN_VERTEX;
		stages[0].alphaGen = AGEN_VERTEX;
		stages[0].stateBits = GLS_DEPTHTEST_DISABLE |
			GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else if(shader.lightmapIndex == LIGHTMAP_WHITEIMAGE)
	{
		// fullbright level
		stages[0].bundle[0].image[0] = tr.whiteImage;
		stages[0].active = qtrue;
		stages[0].rgbGen = CGEN_IDENTITY_LIGHTING;
		stages[0].stateBits = GLS_DEFAULT;

		stages[1].bundle[0].image[0] = image;
		stages[1].active = qtrue;
		stages[1].rgbGen = CGEN_IDENTITY;
		stages[1].stateBits |= GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
	}
	else
	{
		// two pass lightmap
		stages[0].bundle[0].image[0] = tr.lightmaps[shader.lightmapIndex];
		stages[0].bundle[0].isLightMap = qtrue;
		stages[0].active = qtrue;
		stages[0].rgbGen = CGEN_IDENTITY;	// lightmaps are scaled on creation
		// for identitylight
		stages[0].stateBits = GLS_DEFAULT;

		stages[1].bundle[0].image[0] = image;
		stages[1].active = qtrue;
		stages[1].rgbGen = CGEN_IDENTITY;
		stages[1].stateBits |= GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
	}

	sh = FinishShader();
	return sh->index;
}


/* 
====================
RE_RegisterShader

This is the exported shader entry point for the rest of the system
It will always return an index that will be valid.

This should really only be used for explicit shaders, because there is no
way to ask for different implicit lighting modes (vertex, lightmap, etc)
====================
*/
qhandle_t RE_RegisterShaderLightMap(const char *name, int lightmapIndex)
{
	shader_t       *sh;

	if(strlen(name) >= MAX_QPATH)
	{
		Com_Printf("Shader name exceeds MAX_QPATH\n");
		return 0;
	}

	sh = R_FindShader(name, lightmapIndex, qtrue);

	// we want to return 0 if the shader failed to
	// load for some reason, but R_FindShader should
	// still keep a name allocated for it, so if
	// something calls RE_RegisterShader again with
	// the same name, we don't try looking for it again
	if(sh->defaultShader)
	{
		return 0;
	}

	return sh->index;
}


/* 
====================
RE_RegisterShader

This is the exported shader entry point for the rest of the system
It will always return an index that will be valid.

This should really only be used for explicit shaders, because there is no
way to ask for different implicit lighting modes (vertex, lightmap, etc)
====================
*/
qhandle_t RE_RegisterShader(const char *name)
{
	shader_t       *sh;

	if(strlen(name) >= MAX_QPATH)
	{
		Com_Printf("Shader name exceeds MAX_QPATH\n");
		return 0;
	}

	sh = R_FindShader(name, LIGHTMAP_2D, qtrue);

	// we want to return 0 if the shader failed to
	// load for some reason, but R_FindShader should
	// still keep a name allocated for it, so if
	// something calls RE_RegisterShader again with
	// the same name, we don't try looking for it again
	if(sh->defaultShader)
	{
		return 0;
	}

	return sh->index;
}


/*
====================
RE_RegisterShaderNoMip

For menu graphics that should never be picmiped
====================
*/
qhandle_t RE_RegisterShaderNoMip(const char *name)
{
	shader_t       *sh;

	if(strlen(name) >= MAX_QPATH)
	{
		Com_Printf("Shader name exceeds MAX_QPATH\n");
		return 0;
	}

	sh = R_FindShader(name, LIGHTMAP_2D, qfalse);

	// we want to return 0 if the shader failed to
	// load for some reason, but R_FindShader should
	// still keep a name allocated for it, so if
	// something calls RE_RegisterShader again with
	// the same name, we don't try looking for it again
	if(sh->defaultShader)
	{
		return 0;
	}

	return sh->index;
}


/*
====================
R_GetShaderByHandle

When a handle is passed in by another module, this range checks
it and returns a valid (possibly default) shader_t to be used internally.
====================
*/
shader_t       *R_GetShaderByHandle(qhandle_t hShader)
{
	if(hShader < 0)
	{
		ri.Printf(PRINT_WARNING, "R_GetShaderByHandle: out of range hShader '%d'\n", hShader);	// bk: FIXME name
		return tr.defaultShader;
	}
	if(hShader >= tr.numShaders)
	{
		ri.Printf(PRINT_WARNING, "R_GetShaderByHandle: out of range hShader '%d'\n", hShader);
		return tr.defaultShader;
	}
	return tr.shaders[hShader];
}

/*
===============
R_ShaderList_f

Dump information on all valid shaders to the console
A second parameter will cause it to print in sorted order
===============
*/
void R_ShaderList_f(void)
{
	int             i;
	int             count;
	shader_t       *shader;

	ri.Printf(PRINT_ALL, "-----------------------\n");

	count = 0;
	for(i = 0; i < tr.numShaders; i++)
	{
		if(ri.Cmd_Argc() > 1)
		{
			shader = tr.sortedShaders[i];
		}
		else
		{
			shader = tr.shaders[i];
		}

		ri.Printf(PRINT_ALL, "%i ", shader->numUnfoggedPasses);

		if(shader->lightmapIndex >= 0)
		{
			ri.Printf(PRINT_ALL, "L ");
		}
		else
		{
			ri.Printf(PRINT_ALL, "  ");
		}
	
		if(shader->collapseType == COLLAPSE_Generic_multi)
		{
			if(shader->collapseTextureEnv == GL_ADD)
			{
				ri.Printf(PRINT_ALL, "MT(a)          ");
			}
			else if(shader->collapseTextureEnv == GL_MODULATE)
			{
				ri.Printf(PRINT_ALL, "MT(m)          ");
			}
			else if(shader->collapseTextureEnv == GL_DECAL)
			{
				ri.Printf(PRINT_ALL, "MT(d)          ");
			}
		}
		else if(shader->collapseType == COLLAPSE_lighting_D_radiosity)
		{
			ri.Printf(PRINT_ALL, "D_radiosity    ");
		}
		else if(shader->collapseType == COLLAPSE_lighting_DB_radiosity)
		{
			ri.Printf(PRINT_ALL, "DB_radiosity   ");
		}
		else if(shader->collapseType == COLLAPSE_lighting_DBS_radiosity)
		{
			ri.Printf(PRINT_ALL, "DBS_radiosity  ");
		}
		else if(shader->collapseType == COLLAPSE_lighting_DB_direct)
		{
			ri.Printf(PRINT_ALL, "DB_direct      ");
		}
		else if(shader->collapseType == COLLAPSE_lighting_DBS_direct)
		{
			ri.Printf(PRINT_ALL, "DBS_direct     ");
		}
		else
		{
			ri.Printf(PRINT_ALL, "               ");
		}
		
		if(shader->explicitlyDefined)
		{
			ri.Printf(PRINT_ALL, "E ");
		}
		else
		{
			ri.Printf(PRINT_ALL, "  ");
		}

		if(shader->optimalStageIteratorFunc == RB_StageIteratorGeneric)
		{
			ri.Printf(PRINT_ALL, "gen ");
		}
		else if(shader->optimalStageIteratorFunc == RB_StageIteratorSky)
		{
			ri.Printf(PRINT_ALL, "sky ");
		}
		/*
		else if(shader->optimalStageIteratorFunc == RB_StageIteratorLightmappedMultitexture)
		{
			ri.Printf(PRINT_ALL, "lmmt");
		}
		else if(shader->optimalStageIteratorFunc == RB_StageIteratorVertexLitTexture)
		{
			ri.Printf(PRINT_ALL, "vlt ");
		}
		else if(shader->optimalStageIteratorFunc == RB_StageIteratorPerPixelLit_D)
		{
			ri.Printf(PRINT_ALL, "pplD ");
		}
		else if(shader->optimalStageIteratorFunc == RB_StageIteratorPerPixelLit_DB)
		{
			ri.Printf(PRINT_ALL, "pplDB ");
		}
		else if(shader->optimalStageIteratorFunc == RB_StageIteratorPerPixelLit_DBS)
		{
			ri.Printf(PRINT_ALL, "pplDBS ");
		}
		*/
		else
		{
			ri.Printf(PRINT_ALL, "    ");
		}

		if(shader->defaultShader)
		{
			ri.Printf(PRINT_ALL, ": %s (DEFAULTED)\n", shader->name);
		}
		else
		{
			ri.Printf(PRINT_ALL, ": %s\n", shader->name);
		}
		count++;
	}
	ri.Printf(PRINT_ALL, "%i total shaders\n", count);
	ri.Printf(PRINT_ALL, "------------------\n");
}


/*
====================
ScanAndLoadShaderFiles

Finds and loads all .shader files, combining them into
a single large text block that can be scanned for shader names
=====================
*/
#define	MAX_SHADER_FILES	4096
#define USE_MTR
static void ScanAndLoadShaderFiles(void)
{
	char          **shaderFiles;
	char           *buffers[MAX_SHADER_FILES];
	char           *p;
	int             numShaders;
	int             i;
	char           *oldp, *token, *hashMem;
	int             shaderTextHashTableSizes[MAX_SHADERTEXT_HASH], hash, size;

	long            sum = 0;

	// scan for shader files
#ifdef USE_MTR
	shaderFiles = ri.FS_ListFiles("materials", ".mtr", &numShaders);
#else
	shaderFiles = ri.FS_ListFiles("scripts", ".shader", &numShaders);
#endif
	if(!shaderFiles || !numShaders)
	{
		ri.Printf(PRINT_WARNING, "WARNING: no shader files found\n");
		return;
	}

	if(numShaders > MAX_SHADER_FILES)
	{
		numShaders = MAX_SHADER_FILES;
	}

	// load and parse shader files
	for(i = 0; i < numShaders; i++)
	{
		char            filename[MAX_QPATH];

#ifdef USE_MTR
		Com_sprintf(filename, sizeof(filename), "materials/%s", shaderFiles[i]);
#else
		Com_sprintf(filename, sizeof(filename), "scripts/%s", shaderFiles[i]);
#endif
		ri.Printf(PRINT_ALL, "...loading '%s'\n", filename);
		sum += ri.FS_ReadFile(filename, (void **)&buffers[i]);
		if(!buffers[i])
		{
			ri.Error(ERR_DROP, "Couldn't load %s", filename);
		}
	}

	// build single large buffer
	s_shaderText = ri.Hunk_Alloc(sum + numShaders * 2, h_low);

	// free in reverse order, so the temp files are all dumped
	for(i = numShaders - 1; i >= 0; i--)
	{
		strcat(s_shaderText, "\n");
		p = &s_shaderText[strlen(s_shaderText)];
		strcat(s_shaderText, buffers[i]);
		ri.FS_FreeFile(buffers[i]);
		buffers[i] = p;
		COM_Compress(p);
	}

	// free up memory
	ri.FS_FreeFileList(shaderFiles);

	Com_Memset(shaderTextHashTableSizes, 0, sizeof(shaderTextHashTableSizes));
	size = 0;
	//
	for(i = 0; i < numShaders; i++)
	{
		// pointer to the first shader file
		p = buffers[i];
		
		// look for label
		while(1)
		{
			token = COM_ParseExt(&p, qtrue);
			if(token[0] == 0)
			{
				break;
			}
#if 1
			// skip shader tables
			if(!Q_stricmp(token, "table"))
			{
				// skip table name
				token = COM_ParseExt(&p, qtrue);
				
				SkipBracedSection(&p);
				continue;
			}
#endif
			hash = generateHashValue(token, MAX_SHADERTEXT_HASH);
			shaderTextHashTableSizes[hash]++;
			size++;
			
			SkipBracedSection(&p);
			
			// if we passed the pointer to the next shader file
			if(i < numShaders - 1)
			{
				if(p > buffers[i + 1])
				{
					break;
				}
			}
		}
	}

	size += MAX_SHADERTEXT_HASH;

	hashMem = ri.Hunk_Alloc(size * sizeof(char *), h_low);

	for(i = 0; i < MAX_SHADERTEXT_HASH; i++)
	{
		shaderTextHashTable[i] = (char **)hashMem;
		hashMem = ((char *)hashMem) + ((shaderTextHashTableSizes[i] + 1) * sizeof(char *));
	}

	Com_Memset(shaderTextHashTableSizes, 0, sizeof(shaderTextHashTableSizes));
	//
	for(i = 0; i < numShaders; i++)
	{
		// pointer to the first shader file
		p = buffers[i];
		
		// look for label
		while(1)
		{
			oldp = p;
			token = COM_ParseExt(&p, qtrue);
			if(token[0] == 0)
			{
				break;
			}
#if 1
			// skip shader tables
			if(!Q_stricmp(token, "table"))
			{
				// skip table name
				token = COM_ParseExt(&p, qtrue);
				
				SkipBracedSection(&p);
				continue;
			}
#endif
			hash = generateHashValue(token, MAX_SHADERTEXT_HASH);
			shaderTextHashTable[hash][shaderTextHashTableSizes[hash]++] = oldp;

			SkipBracedSection(&p);
			
			// if we passed the pointer to the next shader file
			if(i < numShaders - 1)
			{
				if(p > buffers[i + 1])
				{
					break;
				}
			}
		}
	}
}


/*
====================
CreateInternalShaders
====================
*/
static void CreateInternalShaders(void)
{
	tr.numShaders = 0;

	// init the default shader
	Com_Memset(&shader, 0, sizeof(shader));
	Com_Memset(&stages, 0, sizeof(stages));

	Q_strncpyz(shader.name, "<default>", sizeof(shader.name));

	shader.lightmapIndex = LIGHTMAP_NONE;
	stages[0].bundle[0].image[0] = tr.defaultImage;
	stages[0].active = qtrue;
	stages[0].stateBits = GLS_DEFAULT;
	tr.defaultShader = FinishShader();

	// shadow shader is just a marker
	Q_strncpyz(shader.name, "<stencil shadow>", sizeof(shader.name));
	shader.sort = SS_STENCIL_SHADOW;
	tr.shadowShader = FinishShader();
}

static void CreateExternalShaders(void)
{
	tr.projectionShadowShader = R_FindShader("projectionShadow", LIGHTMAP_NONE, qtrue);
	tr.flareShader = R_FindShader("flareShader", LIGHTMAP_NONE, qtrue);
	tr.sunShader = R_FindShader("sun", LIGHTMAP_NONE, qtrue);
}

/*
==================
R_InitShaders
==================
*/
void R_InitShaders(void)
{
	ri.Printf(PRINT_ALL, "Initializing Shaders\n");

	Com_Memset(shaderHashTable, 0, sizeof(shaderHashTable));

	deferLoad = qfalse;

	CreateInternalShaders();

	ScanAndLoadShaderFiles();

	CreateExternalShaders();
}
