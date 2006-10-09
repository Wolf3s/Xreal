/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2006 Robert Beckebans <trebor_7@users.sourceforge.net>

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
//
#ifndef __TR_TYPES_H
#define __TR_TYPES_H


#define	MAX_LIGHTS		1024
#define	MAX_ENTITIES	1023	// can't be increased without changing drawsurf bit packing
#define MAX_BONES       128

// renderfx flags
#define	RF_MINLIGHT			1	// allways have some light (viewmodel, some items)
#define	RF_THIRD_PERSON		2	// don't draw through eyes, only mirrors (player bodies, chat sprites)
#define	RF_FIRST_PERSON		4	// only draw through eyes (view weapon, damage blood blob)
#define	RF_DEPTHHACK		8	// for view weapon Z crunching
#define	RF_NOSHADOW			64	// don't add stencil shadows

#define RF_LIGHTING_ORIGIN	128	// use refEntity->lightingOrigin instead of refEntity->origin
									// for lighting.  This allows entities to sink into the floor
									// with their origin going solid, and allows all parts of a
									// player to get the same lighting
#define	RF_SHADOW_PLANE		256	// use refEntity->shadowPlane
#define	RF_WRAP_FRAMES		512	// mod the model frames by the maxframes to allow continuous
									// animation without needing to know the frame count

#define RF_SKELETON			1024 // use refExtEntity->skeleton

// refdef flags
#define RDF_NOWORLDMODEL	1	// used for player configuration screen
#define RDF_NOSHADOWS		2	// force renderer to use faster only lighting path
#define RDF_HYPERSPACE		4	// teleportation effect

typedef struct
{
	vec3_t          xyz;
	float           st[2];
	byte            modulate[4];
} polyVert_t;

typedef struct poly_s
{
	qhandle_t       hShader;
	int             numVerts;
	polyVert_t     *verts;
} poly_t;

typedef enum
{
	RT_MODEL,
	RT_POLY,
	RT_SPRITE,
	RT_BEAM,
	RT_RAIL_CORE,
	RT_RAIL_RINGS,
	RT_LIGHTNING,
	RT_PORTALSURFACE,			// doesn't draw anything, just info for portals

	RT_MAX_REF_ENTITY_TYPE
} refEntityType_t;

typedef struct
{
	refEntityType_t reType;
	int             renderfx;

	qhandle_t       hModel;		// opaque type outside refresh

	// most recent data
	vec3_t          lightingOrigin;	// so multi-part models can be lit identically (RF_LIGHTING_ORIGIN)
	float           shadowPlane;	// projection shadows go here, stencils go slightly lower

	vec3_t          axis[3];	// rotation vectors
	qboolean        nonNormalizedAxes;	// axis are not normalized, i.e. they have scale
	float           origin[3];	// also used as MODEL_BEAM's "from"
	int             frame;		// also used as MODEL_BEAM's diameter

	// previous data for frame interpolation
	float           oldorigin[3];	// also used as MODEL_BEAM's "to"
	int             oldframe;
	float           backlerp;	// 0.0 = current, 1.0 = old

	// texturing
	int             skinNum;	// inline skin index
	qhandle_t       customSkin;	// NULL for default skin
	qhandle_t       customShader;	// use one image for the entire thing

	// misc
	byte            shaderRGBA[4];	// colors used by rgbgen entity shaders
	float           shaderTexCoord[2];	// texture coordinates used by tcMod entity modifiers
	float           shaderTime;	// subtracted from refdef time to control effect start times

	// extra sprite information
	float           radius;
	float           rotation;
} refEntity_t;

//#define USE_BONEMATRIX
typedef struct
{
#ifdef USE_BONEMATRIX
	matrix_t        transform;	// contains rotation and translation
#else
	vec3_t          origin;
	quat_t          rotation;
#endif
} refBone_t;

typedef struct
{
	int             numBones;
	refBone_t       bones[MAX_BONES];
	
	vec3_t          bounds[2];	// bounds of all applied animations
} refSkeleton_t;

typedef struct
{
	// NOTE: Tr3B - don't change the following variables because
	// the renderer uses Com_Memcpy in RE_AddRefEntityToScene
	// so we need the same order as in refEntity_t for the first part
	
	refEntityType_t reType;
	int             renderfx;

	qhandle_t       hModel;		// opaque type outside refresh

	// most recent data
	vec3_t          lightingOrigin;	// so multi-part models can be lit identically (RF_LIGHTING_ORIGIN)
	float           shadowPlane;	// projection shadows go here, stencils go slightly lower

	vec3_t          axis[3];	// rotation vectors
	qboolean        nonNormalizedAxes;	// axis are not normalized, i.e. they have scale
	float           origin[3];	// also used as MODEL_BEAM's "from"
	int             frame;		// also used as MODEL_BEAM's diameter

	// previous data for frame interpolation
	float           oldorigin[3];	// also used as MODEL_BEAM's "to"
	int             oldframe;
	float           backlerp;	// 0.0 = current, 1.0 = old

	// texturing
	int             skinNum;	// inline skin index
	qhandle_t       customSkin;	// NULL for default skin
	qhandle_t       customShader;	// use one image for the entire thing

	// misc
	byte            shaderRGBA[4];	// colors used by rgbgen entity shaders
	float           shaderTexCoord[2];	// texture coordinates used by tcMod entity modifiers
	float           shaderTime;	// subtracted from refdef time to control effect start times

	// extra sprite information
	float           radius;
	float           rotation;
	
	// NOTE: Tr3B - now we are save to add new variables
	
	// animation information
	refSkeleton_t   skeleton;
} refExtEntity_t;


// lightfx flags

typedef enum
{
	RL_OMNI,
	RL_PROJ,
	RL_DIRECT,

	RL_MAX_REF_LIGHT_TYPE
} refLightType_t;

typedef struct
{
	refLightType_t  rlType;
//	int             lightfx;
	
	qhandle_t       attenuationShader;
	
	vec3_t          origin;
	vec3_t          center;
	vec3_t          color;		// range from 0.0 to 1.0, should be color normalized
	vec3_t          radius;

	vec3_t          target;
	vec3_t          right;
	vec3_t          up;
	
	vec3_t          axis[3];	// rotation vectors
	qboolean        nonNormalizedAxes;	// axis are not normalized, i.e. they have scale
	qboolean        noShadows;
} refLight_t;


#define	MAX_RENDER_STRINGS			8
#define	MAX_RENDER_STRING_LENGTH	32

typedef struct
{
	int             x, y, width, height;
	float           fov_x, fov_y;
	vec3_t          vieworg;
	vec3_t          viewaxis[3];	// transformation matrix

	// time in milliseconds for shader effects and other time dependent rendering issues
	int             time;

	int             rdflags;	// RDF_NOWORLDMODEL, etc

	// 1 bits will prevent the associated area from rendering at all
	byte            areamask[MAX_MAP_AREA_BYTES];

	// text messages for deform text shaders
	char            text[MAX_RENDER_STRINGS][MAX_RENDER_STRING_LENGTH];
} refdef_t;


typedef enum
{
	STEREO_CENTER,
	STEREO_LEFT,
	STEREO_RIGHT
} stereoFrame_t;


/*
** glConfig_t
**
** Contains variables specific to the OpenGL configuration
** being run right now.  These are constant once the OpenGL
** subsystem is initialized.
*/
typedef enum
{
	TC_NONE,
	TC_S3TC,
	TC_ARB
} textureCompression_t;

typedef enum
{
	GLDRV_ICD,					// driver is integrated with window system
	// WARNING: there are tests that check for
	// > GLDRV_ICD for minidriverness, so this
	// should always be the lowest value in this
	// enum set
	GLDRV_STANDALONE,			// driver is a non-3Dfx standalone driver
	GLDRV_VOODOO				// driver is a 3Dfx standalone driver
} glDriverType_t;

typedef enum
{
	GLHW_GENERIC,				// where everthing works the way it should
	GLHW_3DFX_2D3D,				// Voodoo Banshee or Voodoo3, relevant since if this is
	// the hardware type then there can NOT exist a secondary
	// display adapter
	GLHW_RIVA128,				// where you can't interpolate alpha
	GLHW_RAGEPRO,				// where you can't modulate alpha on alpha textures
	GLHW_PERMEDIA2,				// where you don't have src*dst
	GLHW_ATI					// where you don't have proper GLSL support
} glHardwareType_t;

typedef struct
{
	char            renderer_string[MAX_STRING_CHARS];
	char            vendor_string[MAX_STRING_CHARS];
	char            version_string[MAX_STRING_CHARS];
	char            extensions_string[BIG_INFO_STRING];

	int             maxTextureSize;		// queried from GL
	int             maxTextureUnits;	// multitexture ability

	int             colorBits, depthBits, stencilBits;

	glDriverType_t  driverType;
	glHardwareType_t hardwareType;

	qboolean        deviceSupportsGamma;
	textureCompression_t textureCompression;
	qboolean        textureEnvAddAvailable;
	
	int             maxCubeMapTextureSize;
	qboolean        textureCubeAvailable;
	qboolean        vertexProgramAvailable;
	qboolean        vertexBufferObjectAvailable;
	qboolean        occlusionQueryAvailable;
	int             occlusionQueryBits;
	qboolean        shaderObjectsAvailable;
	qboolean        vertexShaderAvailable;
	qboolean        fragmentShaderAvailable;
	qboolean        shadingLanguage100Available;
	char            shadingLanguageVersion[MAX_STRING_CHARS];
	qboolean        textureNPOTAvailable;
	qboolean        drawBuffersAvailable;
	qboolean        textureFloatAvailable;
	int             maxDrawBuffers;
	qboolean		stencilWrapAvailable;
	float			maxTextureAnisotropy;
	qboolean		textureAnisotropyAvailable;
	qboolean        framebufferObjectAvailable;
	int             maxRenderbufferSize;
	int             maxColorAttachments;

	int             vidWidth, vidHeight;
	// aspect is the screen's physical width / height, which may be different
	// than scrWidth / scrHeight if the pixels are non-square
	// normal screens should be 4/3, but wide aspect monitors may be 16/9
	float           windowAspect;

	int             displayFrequency;

	// synonymous with "does rendering consume the entire screen?", therefore
	// a Voodoo or Voodoo2 will have this set to TRUE, as will a Win32 ICD that
	// used CDS.
	qboolean        isFullscreen;
	qboolean        stereoEnabled;
	qboolean        smpActive;	// dual processor
} glConfig_t;


// FIXME: VM should be OS agnostic .. in theory

#if defined(Q3_VM) || defined(_WIN32)

#define _3DFX_DRIVER_NAME	"3dfxvgl"
#define OPENGL_DRIVER_NAME	"opengl32"

#else

#define _3DFX_DRIVER_NAME	"libMesaVoodooGL.so"
// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=524
#define OPENGL_DRIVER_NAME	"libGL.so.1"

#endif							// !defined _WIN32

#endif							// __TR_TYPES_H
