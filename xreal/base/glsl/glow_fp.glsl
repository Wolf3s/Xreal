/// ============================================================================
/*
Copyright (C) 2005 Robert Beckebans <trebor_7@users.sourceforge.net>
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

uniform sampler2D	u_ColorMap;
uniform vec2		u_FBufScale;
uniform vec2		u_NPotScale;
uniform float		u_BlurMagnitude;

void	main()
{
	vec2 st00 = gl_FragCoord.st;

	// calculate the screen texcoord in the 0.0 to 1.0 range
	st00 *= u_FBufScale;
	
	// scale by the screen non-power-of-two-adjust
	st00 *= u_NPotScale;
	
	// set so a magnitude of 1 is approximately 1 pixel with 640x480
	vec2 deform = u_BlurMagnitude * 0.0016;
	deform.y *= 1.33333;
	
	// fragment offsets for blur samples
	vec2 offset00 = vec2(-0.326212, -0.405805);
	vec2 offset01 = vec2(-0.840144, -0.173580);
	vec2 offset02 = vec2(-0.695914,  0.557137);
	vec2 offset03 = vec2(-0.203345,  0.720716);
	vec2 offset04 = vec2( 0.962340, -0.394983);
	vec2 offset05 = vec2( 0.473434, -0.480026);
	vec2 offset06 = vec2( 0.319456,  0.967022);
	vec2 offset07 = vec2( 0.185461, -0.893124);
	vec2 offset08 = vec2( 0.507431,  0.264425);
	vec2 offset09 = vec2( 0.896420,  0.412458);
	vec2 offset10 = vec2(-0.321940, -0.932615);
	vec2 offset11 = vec2(-0.791559, -0.597705);

	// calculate our offset texture coordinates
	vec2 st01 = deform * offset00 + st00;
	vec2 st02 = deform * offset01 + st00;
	vec2 st03 = deform * offset02 + st00;
	vec2 st04 = deform * offset03 + st00;
	vec2 st05 = deform * offset04 + st00;
	vec2 st06 = deform * offset05 + st00;
	vec2 st07 = deform * offset06 + st00;
	vec2 st08 = deform * offset07 + st00;
	vec2 st09 = deform * offset08 + st00;
	vec2 st10 = deform * offset09 + st00;
	vec2 st11 = deform * offset10 + st00;
	vec2 st12 = deform * offset11 + st00;
	
	// base color
	vec4 c00 = texture2D(u_ColorMap, st00);

	// sample the current render for each coordinate
	vec4 c01 = texture2D(u_ColorMap, st01);
	vec4 c02 = texture2D(u_ColorMap, st02);
	vec4 c03 = texture2D(u_ColorMap, st03);
	vec4 c04 = texture2D(u_ColorMap, st04);
	vec4 c05 = texture2D(u_ColorMap, st05);
	vec4 c06 = texture2D(u_ColorMap, st06);
	vec4 c07 = texture2D(u_ColorMap, st07);
	vec4 c08 = texture2D(u_ColorMap, st08);
	vec4 c09 = texture2D(u_ColorMap, st09);
	vec4 c10 = texture2D(u_ColorMap, st10);
	vec4 c11 = texture2D(u_ColorMap, st11);
	vec4 c12 = texture2D(u_ColorMap, st12);
	
//	float inv13 = 0.076923077;
	float inv12 = 1.0 / 12.0;
	vec4 sum = c01 + c02 + c03 + c04 + c05 + c06 + c07 + c08 + c09 + c10 + c11 + c12;
	sum *= inv12;
//	sum *= 0.2;
	
	gl_FragColor = c00 + sum;
}
