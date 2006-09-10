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
uniform sampler2D	u_LightMap;
uniform sampler2D	u_DeluxeMap;
uniform vec3		u_ViewOrigin;
uniform float		u_SpecularExponent;

varying vec3		var_Vertex;
varying vec2		var_TexDiffuse;
varying vec2		var_TexNormal;
varying vec2		var_TexSpecular;
varying vec2		var_TexLight;
varying mat3		var_OS2TSMatrix;

void	main()
{
	// compute normal in tangent space from normalmap
	vec3 N = 2.0 * (texture2D(u_NormalMap, var_TexNormal).xyz - 0.5);
	N = normalize(N);
	
	// compute view direction in tangent space
	vec3 V = normalize(var_OS2TSMatrix * (u_ViewOrigin - var_Vertex));
	
	// compute light direction from object space deluxe map into tangent space
	vec3 L = normalize(var_OS2TSMatrix * (2.0 * (texture2D(u_DeluxeMap, var_TexLight).xyz - 0.5)));
	
	// compute half angle in tangent space
	vec3 H = normalize(L + V);
	
	// compute light color from object space lightmap
	vec3 C = texture2D(u_LightMap, var_TexLight).rgb;
	
	// compute the diffuse term
	vec4 diffuse = texture2D(u_DiffuseMap, var_TexDiffuse);
	diffuse.rgb *= C * clamp(dot(N, L), 0.0, 1.0);
	
	// compute the specular term
	vec3 specular = texture2D(u_SpecularMap, var_TexSpecular).rgb * C * pow(clamp(dot(N, H), 0.0, 1.0), u_SpecularExponent);
	
	// compute final color
	vec4 color = diffuse;
	color.rgb += specular;
	
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
