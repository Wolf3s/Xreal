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

attribute vec4		attr_TexCoord0;
attribute vec3		attr_Tangent;
attribute vec3		attr_Binormal;

uniform mat4		u_ModelMatrix;

varying vec3		var_Vertex;
varying vec2		var_TexNormal;
varying vec3		var_TexAttenXYZ;
varying mat3		var_TS2OSMatrix;

void	main()
{
	// transform vertex position into homogenous clip-space
	gl_Position = ftransform();
	
	// transform position into world space
	var_Vertex = (u_ModelMatrix * gl_Vertex).xyz;
	
	// transform normalmap texcoords
	var_TexNormal = (gl_TextureMatrix[1] * attr_TexCoord0).st;
	
	// construct tangent-space-to-object-space 3x3 matrix
	var_TS2OSMatrix = mat3(	attr_Tangent.x, attr_Tangent.y, attr_Tangent.z,
							attr_Binormal.x, attr_Binormal.y, attr_Binormal.z,
							gl_Normal.x, gl_Normal.y, gl_Normal.z	);
}
