/*
===========================================================================
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

uniform sampler2D	u_DiffuseMap;
uniform sampler2D	u_NormalMap;
uniform sampler2D	u_SpecularMap;
uniform sampler2D	u_AttenuationMapXY;
uniform sampler2D	u_AttenuationMapZ;
//uniform samplerCube	u_AttenuationMapCube;
uniform vec3		u_ViewOrigin;
uniform vec3		u_LightOrigin;
uniform vec3		u_LightColor;
uniform float       u_LightScale;
uniform float		u_SpecularExponent;
uniform float		u_HeightScale;
uniform float		u_HeightBias;

varying vec3		var_Vertex;
varying vec2		var_TexDiffuse;
varying vec2		var_TexNormal;
varying vec2		var_TexSpecular;
varying vec3		var_TexAttenXYZ;
//varying vec3		var_TexAttenCube;
varying mat3		var_OS2TSMatrix;

void	main()
{
	// compute view direction in tangent space
	vec3 V = normalize(var_OS2TSMatrix * (u_ViewOrigin - var_Vertex));
	
	// compute height
	float height = texture2D(u_NormalMap, var_TexNormal).a;
	
	// compute texcoords offset
	vec2 texOffset = (height * u_HeightScale + u_HeightBias) * V.xy;
	
	// compute light direction in tangent space
	vec3 L = normalize(var_OS2TSMatrix * (u_LightOrigin - var_Vertex));
	
	// compute half angle in tangent space
	vec3 H = normalize(L + V);
	
	// compute normal in tangent space from normalmap
	vec3 N = 2.0 * (texture2D(u_NormalMap, var_TexNormal + texOffset).xyz - 0.5);
	N = normalize(N);
	
	// compute the diffuse term
	vec4 diffuse = texture2D(u_DiffuseMap, var_TexDiffuse + texOffset);
	diffuse.rgb *= u_LightColor * clamp(dot(N, L), 0.0, 1.0);
	
	// compute the specular term
	vec3 specular = texture2D(u_SpecularMap, var_TexSpecular + texOffset).rgb * u_LightColor * pow(clamp(dot(N, H), 0.0, 1.0), u_SpecularExponent);
	
	// compute attenuation
	vec3 attenuationXY		= texture2D(u_AttenuationMapXY, var_TexAttenXYZ.xy).rgb;
	vec3 attenuationZ		= texture2D(u_AttenuationMapZ, vec2(var_TexAttenXYZ.z, 0)).rgb;
//	vec3 attenuationCube	= textureCube(u_AttenuationMapCube, var_tex_atten_cube).rgb;
					
	// compute final color
	vec4 color = diffuse;
	color.rgb += specular;
	color.rgb *= attenuationXY;
	color.rgb *= attenuationZ;
//	color.rgb *= attenuationCube;
	color.rgb *= u_LightScale;

#if defined(GL_ARB_draw_buffers)
	gl_FragData[0] = color;
	vec4 black = vec4(0.0, 0.0, 0.0, color.a);
	gl_FragData[1] = black;
	gl_FragData[2] = black;
	gl_FragData[3] = black;
#else
	gl_FragColor = color;
#endif
}
