/*
===========================================================================
Copyright (C) 2006-2011 Robert Beckebans <trebor_7@users.sourceforge.net>

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

/* forwardLighting_vp.glsl */

attribute vec4		attr_Position;
attribute vec4		attr_TexCoord0;
attribute vec3		attr_Tangent;
attribute vec3		attr_Binormal;
attribute vec3		attr_Normal;
attribute vec4		attr_Color;

attribute vec4		attr_Position2;
attribute vec3		attr_Tangent2;
attribute vec3		attr_Binormal2;
attribute vec3		attr_Normal2;

//#if defined(r_VertexSkinning)
attribute vec4		attr_BoneIndexes;
attribute vec4		attr_BoneWeights;
uniform int			u_VertexSkinning;
uniform mat4		u_BoneMatrix[MAX_GLSL_BONES];
//#endif

uniform float		u_VertexInterpolation;

uniform mat4		u_DiffuseTextureMatrix;
uniform mat4		u_NormalTextureMatrix;
uniform mat4		u_SpecularTextureMatrix;

uniform vec4		u_ColorModulate;
uniform vec4		u_Color;

uniform mat4		u_LightAttenuationMatrix;
uniform mat4		u_ModelMatrix;
uniform mat4		u_ModelViewProjectionMatrix;

uniform int			u_DeformGen;
uniform vec4		u_DeformWave;	// [base amplitude phase freq]
uniform vec3		u_DeformBulge;	// [width height speed]
uniform float		u_DeformSpread;
uniform float		u_Time;

varying vec3		var_Position;
varying vec4		var_TexDiffuse;
varying vec4		var_TexNormal;
#if defined(USE_NORMAL_MAPPING)
varying vec2		var_TexSpecular;
#endif

varying vec3		var_TexAttenXYZ;

#if defined(USE_NORMAL_MAPPING)
varying vec4		var_Tangent;
varying vec4		var_Binormal;
#endif
varying vec4		var_Normal;
//varying vec4		var_Color;	// Tr3B - maximum vars reached



void	main()
{
	vec4 position;
	vec3 tangent = vec3(0.0);
	vec3 binormal = vec3(0.0);
	vec3 normal = vec3(0.0);

#if defined(USE_VERTEX_SKINNING)
	{
		position = vec4(0.0);

		for(int i = 0; i < 4; i++)
		{
			int boneIndex = int(attr_BoneIndexes[i]);
			float boneWeight = attr_BoneWeights[i];
			mat4  boneMatrix = u_BoneMatrix[boneIndex];
			
			position += (boneMatrix * attr_Position) * boneWeight;
			
			#if defined(USE_NORMAL_MAPPING)
			tangent += (boneMatrix * vec4(attr_Tangent, 0.0)).xyz * boneWeight;
			binormal += (boneMatrix * vec4(attr_Binormal, 0.0)).xyz * boneWeight;
			#endif
			
			normal += (boneMatrix * vec4(attr_Normal, 0.0)).xyz * boneWeight;
		}
	}
#elif defined(USE_VERTEX_ANIMATION)
	{
		if(u_VertexInterpolation > 0.0)
		{
			#if defined(USE_NORMAL_MAPPING)
			VertexAnimation_P_TBN(	attr_Position, attr_Position2,
									attr_Tangent, attr_Tangent2,
									attr_Binormal, attr_Binormal2,
									attr_Normal, attr_Normal2,
									u_VertexInterpolation,
									position, tangent, binormal, normal);
			#else
			VertexAnimation_P_N(attr_Position, attr_Position2,
								attr_Normal, attr_Normal2,
								u_VertexInterpolation,
								position, normal);
			#endif
		}
		else
		{
			position = attr_Position;
			
			#if defined(USE_NORMAL_MAPPING)
			tangent = attr_Tangent;
			binormal = attr_Binormal;
			#endif
			
			normal = attr_Normal;
		}
	}
#else
	{
		position = attr_Position;
		
		#if defined(USE_NORMAL_MAPPING)
		tangent = attr_Tangent;
		binormal = attr_Binormal;
		#endif
		
		normal = attr_Normal;
	}
#endif

#if defined(USE_DEFORM_VERTEXES)
	position = DeformPosition(	u_DeformGen,
								u_DeformWave,	// [base amplitude phase freq]
								u_DeformBulge,	// [width height speed]
								u_DeformSpread,
								u_Time,
								position,
								normal,
								attr_TexCoord0.st);
#endif

	// transform vertex position into homogenous clip-space
	gl_Position = u_ModelViewProjectionMatrix * position;
	
	// transform position into world space
	var_Position = (u_ModelMatrix * position).xyz;

#if defined(USE_NORMAL_MAPPING)
	var_Tangent.xyz = (u_ModelMatrix * vec4(tangent, 0.0)).xyz;
	var_Binormal.xyz = (u_ModelMatrix * vec4(binormal, 0.0)).xyz;
#endif
	
	var_Normal.xyz = (u_ModelMatrix * vec4(normal, 0.0)).xyz;
		
	// calc light xy,z attenuation in light space
	var_TexAttenXYZ = (u_LightAttenuationMatrix * position).xyz;
		
	// transform diffusemap texcoords
	var_TexDiffuse.xy = (u_DiffuseTextureMatrix * attr_TexCoord0).st;
	
#if defined(USE_NORMAL_MAPPING)
	// transform normalmap texcoords
	var_TexNormal.xy = (u_NormalTextureMatrix * attr_TexCoord0).st;
	
	// transform specularmap texture coords
	var_TexSpecular = (u_SpecularTextureMatrix * attr_TexCoord0).st;
#endif
	
	// assign color
	vec4 color = attr_Color * u_ColorModulate + u_Color;
	// color = vec4(1.0);
	
	var_TexDiffuse.p = color.r;
	var_TexNormal.pq = color.gb;
}
