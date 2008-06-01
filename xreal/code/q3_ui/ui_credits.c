/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2006-2008 Robert Beckebans <trebor_7@users.sourceforge.net>
Copyright (C) 2008 Pat Raynor <raynorpat@gmail.com>

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

/*
=======================================================================

CREDITS

=======================================================================
*/

#include "ui_local.h"

#define SCROLLSPEED	3.50

typedef struct
{
	menuframework_s menu;
} creditsmenu_t;

static creditsmenu_t s_credits;

int             starttime;		// game time at which credits are started
float           mvolume;		// records the original music volume level

qhandle_t       BackgroundShader;

typedef struct
{
	char           *string;
	int             style;
	vec_t          *color;
} cr_line;

cr_line         credits[] = {
	{"XreaL", UI_CENTER | UI_GIANTFONT | UI_PULSE, colorRed},
	{"", UI_CENTER | UI_SMALLFONT, colorBlue},

	{"Project Lead", UI_CENTER | UI_BIGFONT, colorLtGrey},
	{"Robert 'Tr3B' Beckebans", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"", UI_CENTER | UI_SMALLFONT, colorBlue},

	{"Programming", UI_CENTER | UI_BIGFONT, colorLtGrey},
	{"Robert 'Tr3B' Beckebans", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"Pat 'raynorpat' Raynor", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"Josef 'cnuke' Soentgen", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"", UI_CENTER | UI_SMALLFONT, colorBlue},

	{"Development Assistance", UI_CENTER | UI_BIGFONT, colorLtGrey},
	{"Josef 'cnuke' Soentgen", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"Mathias 'skynet' Heyer", UI_CENTER|UI_SMALLFONT, colorWhite},
	{"", UI_CENTER | UI_SMALLFONT, colorBlue},

	{"Art", UI_CENTER | UI_BIGFONT, colorLtGrey},
	{"", UI_CENTER | UI_SMALLFONT, colorWhite},

	{"XreaL Team", UI_CENTER | UI_BIGFONT, colorLtGrey},
	{"Robert 'Tr3B' Beckebans", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"Adrian 'otty' Fuhrmann", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"Michael 'mic' Denno", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"Stefan 'ReFlex' Lautner", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"", UI_CENTER | UI_SMALLFONT, colorWhite},

	{"Quake II: Lost Marine Team", UI_CENTER | UI_BIGFONT, colorLtGrey},
	{"Thearrel 'Kiltron' McKinney", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"Jim 'Revility' Kern", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"", UI_CENTER | UI_SMALLFONT, colorWhite},

	{"OpenArena Team", UI_CENTER | UI_BIGFONT, colorLtGrey},
	{"jzero - powerup models", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"leileilol", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"Mancubus", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"", UI_CENTER | UI_SMALLFONT, colorWhite},

	{"Tenebrae Team", UI_CENTER | UI_BIGFONT, colorLtGrey},
	{"Willi 'whammes' Hammes", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"", UI_CENTER | UI_SMALLFONT, colorWhite},

	//{"Sapphire Scar Team", UI_CENTER | UI_BIGFONT, colorWhite},
	//{ "Paul 'JTR' Steffens, Lee David Ash,", UI_CENTER|UI_SMALLFONT, &colorWhite },
	//{ "James 'HarlequiN' Taylor,", UI_CENTER|UI_SMALLFONT, &colorWhite },
	//{ "Michael 'mic' Denno", UI_CENTER|UI_SMALLFONT, &colorWhite },
	//{"", UI_CENTER | UI_SMALLFONT, colorBlue},

	//{ "Level Design", UI_CENTER|UI_SMALLFONT, &colorLtGrey },
	//{ "Michael 'mic' Denno", UI_CENTER|UI_SMALLFONT, &colorWhite },
	//{ "'Dominic 'cha0s' Szablewski", UI_CENTER|UI_SMALLFONT, &colorWhite },
	//{ "", UI_CENTER|UI_SMALLFONT, &colorBlue },

	{"The Freesound Project", UI_CENTER | UI_BIGFONT, colorLtGrey},
	{"AdcBicycle", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"Abyssmal", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"Andrew Duke", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"aust_paul", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"b0bd0bbs", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"dobroide", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"Erdie", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"HcTrancer", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"ignotus", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"JimPurbrick", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"Jovica", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"kijjaz", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"loofa", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"man", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"nkuitse", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"Oppusum", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"PoisedToGlitch", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"room", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"suonho", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"swelk", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"young_daddy", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"", UI_CENTER | UI_SMALLFONT, colorWhite},

	{"Others", UI_CENTER | UI_BIGFONT, colorLtGrey},
	{"Yan 'Method' Ostretsov - www.methodonline.com", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"Ken 'kat' Beyer - www.katsbits.com", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"Mikael 'Ruohis' Ruohomaa", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"Julian 'Plaque' Morris", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"Yves 'evillair' Allaire - Quake4 eX texture set", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"Philip Klevestav - Quake4 pk01 texture set", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"Matt 'Lunaran' Breit - Quake4 Powerplant texture set", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"Lee David Ash", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"", UI_CENTER | UI_SMALLFONT, colorWhite},

	{"Special Thanks To:", UI_CENTER | UI_BIGFONT, colorLtGrey},
	{"id Software", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"IOQuake3 project", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"", UI_CENTER | UI_SMALLFONT, colorBlue},

	{"Contributors", UI_CENTER | UI_BIGFONT, colorLtGrey},
	{"For a list of contributors,", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"see the accompanying CONTRIBUTORS.txt", UI_CENTER | UI_SMALLFONT, colorWhite},
	{"", UI_CENTER | UI_SMALLFONT, colorBlue},

	{"Websites:", UI_CENTER | UI_BIGFONT, colorLtGrey},
	{"www.sourceforge.net/projects/xreal", UI_CENTER | UI_BIGFONT, colorBlue},
	{"xreal.sourceforge.net", UI_CENTER | UI_BIGFONT, colorBlue},
	{"", UI_CENTER | UI_SMALLFONT, colorBlue},

	{"XreaL(c) 2005-2008, XreaL Team and Contributors", UI_CENTER | UI_SMALLFONT, colorRed},

	{NULL}
};


/*
=================
UI_CreditMenu_Key
=================
*/
static sfxHandle_t UI_CreditMenu_Key(int key)
{
	if(key & K_CHAR_FLAG)
		return 0;

	// pressing the escape key or clicking the mouse will exit
	// we also reset the music volume to the user's original
	// choice here,  by setting s_musicvolume to the stored var
	trap_Cmd_ExecuteText(EXEC_APPEND, va("s_musicvolume %f; quit\n", mvolume));
	return 0;
}

/*
=================
ScrollingCredits_Draw

Main drawing function
=================
*/
static void ScrollingCredits_Draw(void)
{
	int             x = 320, y, n, ysize = 0, fadetime = 0;
	int             textWidth, textHeight;
	float           textScale = 0.25f;
	vec4_t          fadecolour = { 0.00, 0.00, 0.00, 0.00 };

	// first, fill the background with the specified shader
	UI_DrawHandlePic(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BackgroundShader);

	// draw the stuff by settting the initial y location
	y = 480 - SCROLLSPEED * (float)(uis.realtime - starttime) / 100;

	// loop through the entire credits sequence
	for(n = 0; n <= sizeof(credits) - 1; n++)
	{
		// this NULL string marks the end of the credits struct
		if(credits[n].string == NULL)
		{
			/*
			   // credits sequence is completely off screen
			   if(y < -16)
			   {
			   // TODO: bring up XreaL plaque and fade-in and wait for keypress?
			   break;
			   }
			 */
			break;
		}

		if(credits[n].style & UI_GIANTFONT)
			textScale = 0.4f;
		else if(credits[n].style & UI_BIGFONT)
			textScale = 0.3f;
		else
			textScale = 0.25f;

		textWidth = UI_Text_Width(credits[n].string, textScale, 0, &uis.textFont);
		UI_Text_Paint(x - (textWidth) / 2, y, textScale, credits[n].color, credits[n].string, 0, 0, credits[n].style,
					  &uis.textFont);

		// re-adjust y for next line
		//textHeight = UI_Text_Height(credits[n].string, textScale, 0, &uis.textFont);
		//y += textHeight * 3;

		y += SMALLCHAR_HEIGHT;

		/*
		if(credits[n].style & UI_SMALLFONT)
		{
			y += SMALLCHAR_HEIGHT;// * PROP_SMALL_SIZE_SCALE;
		}
		else if(credits[n].style & UI_BIGFONT)
		{
			y += BIGCHAR_HEIGHT;
		}
		else if(credits[n].style & UI_GIANTFONT)
		{
			y += GIANTCHAR_HEIGHT;// * (1 / PROP_SMALL_SIZE_SCALE);
		}
		*/

		// if y is off the screen, break out of loop
		//if(y > 480)
	}

	if(y < 0)
	{
		// repeat the credits
		starttime = uis.realtime;
	}
}

/*
===============
UI_CreditMenu
===============
*/
void UI_CreditMenu(void)
{
	memset(&s_credits, 0, sizeof(s_credits));

	s_credits.menu.draw = ScrollingCredits_Draw;
	s_credits.menu.key = UI_CreditMenu_Key;
	s_credits.menu.fullscreen = qtrue;
	UI_PushMenu(&s_credits.menu);

	starttime = uis.realtime;	// record start time for credits to scroll properly
	mvolume = trap_Cvar_VariableValue("s_musicvolume");
	if(mvolume < 0.5)
		trap_Cmd_ExecuteText(EXEC_APPEND, "s_musicvolume 0.5\n");
	trap_Cmd_ExecuteText(EXEC_APPEND, "music music/credits\n");

	// load the background shader
	BackgroundShader = trap_R_RegisterShaderNoMip("menubackcredits");
}
