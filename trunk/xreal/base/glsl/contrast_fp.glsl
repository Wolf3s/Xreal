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

uniform sampler2D	u_ColorMap;

void	main()
{
	vec2 st00 = gl_FragCoord.st;

	// calculate the screen texcoord in the 0.0 to 1.0 range
	st00 *= r_FBufScale;
	
#if defined(ATI_flippedImageFix)
	// BUGFIX: the ATI driver flips the image
	st00.t = 1.0 - st00.t;
#endif
	
	// scale by the screen non-power-of-two-adjust
	st00 *= r_NPOTScale;
	
	// calculate contrast color
#if 0
	vec4 color = texture2D(u_ColorMap, st00);
	vec4 contrast = color * color;
	contrast.x += contrast.y;
	contrast.x += contrast.z;
	contrast.x *= 0.33333333;
	contrast *= 0.5;
	gl_FragColor = contrast;
#elif 1
	// Perform a box filter for the downsample
	vec3 color = texture2D(u_ColorMap, st00).rgb;
	color += texture2D(u_ColorMap, st00 + 0.0025).rgb;
	color += texture2D(u_ColorMap, st00 + 0.005).rgb;
	color += texture2D(u_ColorMap, st00 + 0.0075).rgb;

	color *= 0.25;

	// Compute luminance
	float luminance = (color.r * 0.2125) + (color.g * 0.7154) + (color.b * 0.0721);

	// Adjust contrast
	luminance = pow(luminance, 1.32);

	// Filter out dark pixels
	luminance = max(luminance - 0.067, 0.0);

	// Write the final color
	gl_FragColor.rgb = color * luminance;
#else
	vec4 color = texture2D(u_ColorMap, st00);
//	vec3 contrast = dot(color.rgb, vec3(0.11, 0.55, 0.33));
//	vec3 contrast = dot(color.rgb, vec3(0.27, 0.67, 0.06));
	vec3 contrast = dot(color.rgb, vec3(0.33, 0.55, 0.11));
	gl_FragColor.rgb = contrast;
	gl_FragColor.a = color.a;
#endif
}