//-----------------------------------------------------------------------------
//
// Zandronum Source
// Copyright (C) 2021-2023 Adam Kaminski
// Copyright (C) 2021-2023 Zandronum Development Team
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. Neither the name of the Zandronum Development Team nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
// 4. Redistributions in any form must be accompanied by information on how to
//    obtain complete source code for the software and any accompanying
//    software that uses the software. The source code must either be included
//    in the distribution or be available for no more than the cost of
//    distribution plus a nominal fee, and must be freely redistributable
//    under reasonable conditions. For an executable file, complete source
//    code means the source code for all modules it contains. It does not
//    include source code for modules or files that typically accompany the
//    major components of the operating system on which the executable file
//    runs.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//
//
// Filename: scoreboard_margin.cpp
//
// Description: Contains everything that controls the scoreboard's margins
// (i.e. the main header, team/spectator headers, and the footer).
//
//-----------------------------------------------------------------------------

#include <map>
#include <tuple>
#include "c_console.h"
#include "scoreboard.h"
#include "team.h"
#include "st_hud.h"
#include "v_video.h"
#include "wi_stuff.h"

EXTERN_CVAR( Int, con_virtualwidth )
EXTERN_CVAR( Int, con_virtualheight )
EXTERN_CVAR( Bool, con_scaletext_usescreenratio )

//*****************************************************************************
//	DEFINITIONS

// What kind of parameter is this?
#define PARAMETER_CONSTANT		0
// Which command can this parameter be used in?
#define COMMAND_CONSTANT		1
// Must this parameter be initialized?
#define MUST_BE_INITIALIZED		2

//*****************************************************************************
//
// [AK] All parameters used by DrawBaseCommand and its derivatives.
//
enum PARAMETER_e
{
	// The value used when drawing the contents.
	PARAMETER_VALUE,
	// How much the contents are offset horizontally.
	PARAMETER_XOFFSET,
	// How much the contents are offset vertically.
	PARAMETER_YOFFSET,
	// How the contents are aligned horizontally (left, center, or right).
	PARAMETER_HORIZALIGN,
	// How the contents are aligned vertically (top, center, or bottom).
	PARAMETER_VERTALIGN,
	// The transparency of the contents.
	PARAMETER_ALPHA,
	// What font to use when drawing a string.
	PARAMETER_FONT,
	// What text color to use when drawing a string.
	PARAMETER_TEXTCOLOR,
	// How large are the gaps between each separate line.
	PARAMETER_GAPSIZE,
	// The width of a color box.
	PARAMETER_WIDTH,
	// The height of a color box.
	PARAMETER_HEIGHT,

	NUM_PARAMETERS
};

//*****************************************************************************
//
// [AK] The command (DrawString, DrawColor, or DrawTexture) a parameter is intended for.
//
enum COMMAND_e
{
	COMMAND_ALL,
	COMMAND_STRING,
	COMMAND_COLOR,
	COMMAND_TEXTURE,
};

//*****************************************************************************
//	VARIABLES

// [AK] A map of all of the parameters used by DrawBaseCommand and its derivatives.
static const std::map<FName, std::tuple<PARAMETER_e, COMMAND_e, bool>> g_NamedParameters =
{
	{ "value",				{ PARAMETER_VALUE,			COMMAND_ALL,		true  }},
	{ "x",					{ PARAMETER_XOFFSET,		COMMAND_ALL,		false }},
	{ "y",					{ PARAMETER_YOFFSET,		COMMAND_ALL,		false }},
	{ "horizontalalign",	{ PARAMETER_HORIZALIGN,		COMMAND_ALL,		false }},
	{ "verticalalign",		{ PARAMETER_VERTALIGN,		COMMAND_ALL,		false }},
	{ "alpha",				{ PARAMETER_ALPHA,			COMMAND_ALL,		false }},
	{ "font",				{ PARAMETER_FONT,			COMMAND_STRING,		false }},
	{ "textcolor",			{ PARAMETER_TEXTCOLOR,		COMMAND_STRING,		false }},
	{ "gapsize",			{ PARAMETER_GAPSIZE,		COMMAND_STRING,		false }},
	{ "width",				{ PARAMETER_WIDTH,			COMMAND_COLOR,		true  }},
	{ "height",				{ PARAMETER_HEIGHT,			COMMAND_COLOR,		true  }},
};

//*****************************************************************************
//	CLASSES

//*****************************************************************************
//*****************************************************************************
//
// [AK] DrawBaseCommand
//
// An abstract class that is shared by all margin commands that are responsible
// for drawing something.
//
//*****************************************************************************
//*****************************************************************************

class DrawBaseCommand : public ScoreMargin::BaseCommand
{
public:
	DrawBaseCommand( ScoreMargin *pMargin, COMMAND_e Type ) : BaseCommand( pMargin ),
		Command( Type ),
		HorizontalAlignment( HORIZALIGN_LEFT ),
		VerticalAlignment( VERTALIGN_TOP ),
		lXOffset( 0 ),
		lYOffset( 0 ),
		fTranslucency( 1.0f ) { }

	//*************************************************************************
	//
	// [AK] Scans for any parameters until it reaches the end of the command.
	//
	//*************************************************************************

	virtual void Parse( FScanner &sc )
	{
		bool bParameterInitialized[NUM_PARAMETERS] = { false };

		do
		{
			sc.MustGetToken( TK_Identifier );
			auto parameter = g_NamedParameters.find( sc.String );

			// [AK] Make sure that the user entered a valid parameter.
			if ( parameter == g_NamedParameters.end( ))
				sc.ScriptError( "Unknown parameter '%s'.", sc.String );

			const PARAMETER_e ParameterConstant = std::get<PARAMETER_CONSTANT>( parameter->second );
			const COMMAND_e CommandConstant = std::get<COMMAND_CONSTANT>( parameter->second );

			// [AK] Make sure that the parameter can be used by this command.
			if (( CommandConstant != COMMAND_ALL ) && ( CommandConstant != Command ))
				sc.ScriptError( "Parameter '%s' cannot be used inside this command.", sc.String );

			// [AK] Don't allow the same parameter to be initialized more than once.
			if ( bParameterInitialized[ParameterConstant] )
				sc.ScriptError( "Parameter '%s' is already initialized.", sc.String );

			sc.MustGetToken( '=' );
			ParseParameter( sc, parameter->first, ParameterConstant );

			// [AK] This parameter has been initialized now, so mark it.
			bParameterInitialized[ParameterConstant] = true;

		} while ( sc.CheckToken( ',' ));

		sc.MustGetToken( ')' );

		// [AK] Throw an error if there are parameters that were supposed to be initialized, but aren't.
		for ( auto it = g_NamedParameters.begin( ); it != g_NamedParameters.end( ); it++ )
		{
			const COMMAND_e CommandConstant = std::get<COMMAND_CONSTANT>( it->second );

			// [AK] Skip parameters that aren't associated with this command.
			if (( CommandConstant != COMMAND_ALL ) && ( CommandConstant != Command ))
				continue;

			if (( std::get<MUST_BE_INITIALIZED>( it->second )) && ( bParameterInitialized[std::get<PARAMETER_CONSTANT>( it->second )] == false ))
				sc.ScriptError( "Parameter '%s' isn't initialized.", it->first.GetChars( ));
		}
	}

protected:
	template <typename EnumType>
	using SpecialValue = std::pair<EnumType, MARGINTYPE_e>;

	template <typename EnumType>
	using SpecialValueList = std::map<FName, SpecialValue<EnumType>, std::less<FName>, std::allocator<std::pair<FName, SpecialValue<EnumType>>>>;

	//*************************************************************************
	//
	// [AK] Parses any parameters that every draw command can have. Derived
	// classes can handle their own parameters by overriding this function.
	//
	//*************************************************************************

	virtual void ParseParameter( FScanner &sc, const FName ParameterName, const PARAMETER_e Parameter )
	{
		switch ( Parameter )
		{
			case PARAMETER_XOFFSET:
			case PARAMETER_YOFFSET:
			{
				sc.MustGetToken( TK_IntConst );

				if ( Parameter == PARAMETER_XOFFSET )
					lXOffset = sc.Number;
				else
					lYOffset = sc.Number;

				break;
			}

			case PARAMETER_HORIZALIGN:
			case PARAMETER_VERTALIGN:
			{
				sc.MustGetToken( TK_Identifier );

				if ( Parameter == PARAMETER_HORIZALIGN )
					HorizontalAlignment = static_cast<HORIZALIGN_e>( sc.MustGetEnumName( "alignment", "HORIZALIGN_", GetValueHORIZALIGN_e, true ));
				else
					VerticalAlignment = static_cast<VERTALIGN_e>( sc.MustGetEnumName( "alignment", "VERTALIGN_", GetValueVERTALIGN_e, true ));

				break;
			}

			case PARAMETER_ALPHA:
			{
				sc.MustGetToken( TK_FloatConst );
				fTranslucency = clamp( static_cast<float>( sc.Float ), 0.0f, 1.0f );

				break;
			}

			default:
				sc.ScriptError( "Couldn't process parameter '%s'.", ParameterName.GetChars( ));
		}

		// [AK] Don't offset to the left when aligned to the left, or to the right when aligned to the right.
		if ((( HorizontalAlignment == HORIZALIGN_LEFT ) || ( HorizontalAlignment == HORIZALIGN_RIGHT )) && ( lXOffset < 0 ))
			sc.ScriptError( "Can't have a negative x-offset when aligned to the left or right." );

		// [AK] Don't offset upward when aligned to the top, or downward when aligned to the bottom.
		if ((( VerticalAlignment == VERTALIGN_TOP ) || ( VerticalAlignment == VERTALIGN_BOTTOM )) && ( lYOffset < 0 ))
			sc.ScriptError( "Can't have a negative y-offset when aligned to the top or bottom." );
	}

	//*************************************************************************
	//
	// [AK] Checks for identifiers that correspond to "special" values. These
	// values can only be used in the margins they're intended for, which is
	// also checked. If no identifier was passed, then the value is assumed to
	// be "static", which is parsed in the form of a string.
	//
	//*************************************************************************

	template <typename EnumType>
	EnumType GetSpecialValue( FScanner &sc, const SpecialValueList<EnumType> &ValueList )
	{
		if ( sc.CheckToken( TK_Identifier ))
		{
			auto value = ValueList.find( sc.String );

			if ( value != ValueList.end( ))
			{
				const MARGINTYPE_e MarginType = value->second.second;

				// [AK] Throw an error if this value can't be used in the margin that the commands belongs to.
				if ( MarginType != pParentMargin->GetType( ))
					sc.ScriptError( "Special value '%s' can't be used inside a '%s' margin.", sc.String, pParentMargin->GetName( ));

				// [AK] Return the constant that corresponds to this special value.
				return value->second.first;
			}
			else
			{
				sc.ScriptError( "Unknown special value '%s'.", sc.String );
			}
		}

		sc.MustGetToken( TK_StringConst );

		// [AK] Throw a fatal error if an empty string was passed.
		if ( sc.StringLen == 0 )
			sc.ScriptError( "Got an empty string for a value." );

		// [AK] Return the constant that indicates the value as "static".
		return static_cast<EnumType>( -1 );
	}

	//*************************************************************************
	//
	// [AK] Determines the position to draw the contents on the screen.
	//
	//*************************************************************************

	TVector2<LONG> GetDrawingPosition( const ULONG ulWidth, const ULONG ulHeight ) const
	{
		const ULONG ulHUDWidth = HUD_GetWidth( );
		TVector2<LONG> result;

		// [AK] Get the x-position based on the horizontal alignment.
		if ( HorizontalAlignment == HORIZALIGN_LEFT )
			result.X = ( ulHUDWidth - pParentMargin->GetWidth( )) / 2 + lXOffset;
		else if ( HorizontalAlignment == HORIZALIGN_CENTER )
			result.X = ( ulHUDWidth - ulWidth ) / 2 + lXOffset;
		else
			result.X = ( ulHUDWidth + pParentMargin->GetWidth( )) / 2 - ulWidth - lXOffset;

		// [AK] Next, get the y-position based on the vertical alignment.
		if ( VerticalAlignment == VERTALIGN_TOP )
			result.Y = lYOffset;
		else if ( VerticalAlignment == VERTALIGN_CENTER )
			result.Y = ( pParentMargin->GetHeight( ) - ulHeight ) / 2 + lYOffset;
		else
			result.Y = pParentMargin->GetHeight( ) - ulHeight - lYOffset;

		return result;
	}

	//*************************************************************************
	//
	// [AK] Increases the margin's height to fit the contents, if necessary.
	//
	//*************************************************************************

	void EnsureContentFitsInMargin( const ULONG ulHeight )
	{
		if ( ulHeight > 0 )
		{
			LONG lAbsoluteOffset = abs( lYOffset );

			// [AK] Double the y-offset if the content is aligned to the center.
			if ( VerticalAlignment == VERTALIGN_CENTER )
				lAbsoluteOffset *= 2;

			const LONG lHeightDiff = lAbsoluteOffset + ulHeight - pParentMargin->GetHeight( );

			if ( lHeightDiff > 0 )
				pParentMargin->IncreaseHeight( lHeightDiff );
		}
	}

	const COMMAND_e Command;
	HORIZALIGN_e HorizontalAlignment;
	VERTALIGN_e VerticalAlignment;
	LONG lXOffset;
	LONG lYOffset;
	float fTranslucency;
};

//*****************************************************************************
//*****************************************************************************
//
// [AK] DrawString
//
// Draws text somewhere in the margin.
//
//*****************************************************************************
//*****************************************************************************

class DrawString : public DrawBaseCommand
{
public:
	DrawString( ScoreMargin *pMargin ) : DrawBaseCommand( pMargin, COMMAND_STRING ),
		pFont( SmallFont ),
		Color( CR_UNTRANSLATED ),
		ulGapSize( 1 ),
		bUsingTeamColor( false ) { }

	//*************************************************************************
	//
	// [AK] Creates the text that will be drawn on the margin beforehand.
	//
	//*************************************************************************

	virtual void Refresh( const ULONG ulDisplayPlayer )
	{
		for ( unsigned int i = 0; i < PreprocessedStrings.Size( ); i++ )
			V_FreeBrokenLines( PreprocessedStrings[i].pLines );

		PreprocessedStrings.Clear( );

		// [AK] If this command belongs in a team header, create a string for each valid team.
		if ( pParentMargin->GetType( ) == MARGINTYPE_TEAM )
		{
			for ( ULONG ulTeam = 0; ulTeam < TEAM_GetNumAvailableTeams( ); ulTeam++ )
				CreateString( ulDisplayPlayer, ulTeam );
		}
		else
		{
			CreateString( ulDisplayPlayer, ScoreMargin::NO_TEAM );
		}
	}

	//*************************************************************************
	//
	// [AK] Draws the string on the margin.
	//
	//*************************************************************************

	virtual void Draw( const ULONG ulDisplayPlayer, const ULONG ulTeam, const LONG lYPos, const float fAlpha ) const
	{
		const PreprocessedString *pString = RetrieveString( ulTeam );
		const EColorRange TextColorToUse = bUsingTeamColor ? static_cast<EColorRange>( TEAM_GetTextColor( ulTeam )) : Color;
		const fixed_t combinedAlpha = FLOAT2FIXED( fAlpha * fTranslucency );

		int clipLeft = ( HUD_GetWidth( ) - pParentMargin->GetWidth( )) / 2;
		int clipWidth = pParentMargin->GetWidth( );
		int clipTop = lYPos;
		int clipHeight = pParentMargin->GetHeight( );

		// [AK] We must take into account the virtual screen's size when setting up the clipping rectangle.
		if ( g_bScale )
			screen->VirtualToRealCoordsInt( clipLeft, clipTop, clipWidth, clipHeight, con_virtualwidth, con_virtualheight, false, !con_scaletext_usescreenratio );

		for ( unsigned int i = 0; pString->pLines[i].Width >= 0; i++ )
		{
			TVector2<LONG> Pos = GetDrawingPosition( pString->pLines[i].Width, pString->ulTotalHeight );

			if ( i > 0 )
				Pos.Y += ( pFont->GetHeight( ) + ulGapSize ) * i;

			screen->DrawText( pFont, TextColorToUse, Pos.X, Pos.Y + lYPos, pString->pLines[i].Text.GetChars( ),
				DTA_UseVirtualScreen, g_bScale,
				DTA_ClipLeft, clipLeft,
				DTA_ClipRight, clipLeft + clipWidth,
				DTA_ClipTop, clipTop,
				DTA_ClipBottom, clipTop + clipHeight,
				DTA_Alpha, combinedAlpha,
				TAG_DONE );
		}
	}

protected:
	enum DRAWSTRINGVALUE_e
	{
		// The name of the server we're connected to.
		DRAWSTRING_HOSTNAME,
		// The name of the current game mode.
		DRAWSTRING_GAMEMODE,
		// The name of the current level.
		DRAWSTRING_LEVELNAME,
		// The lump of the current level.
		DRAWSTRING_LEVELLUMP,
		// The name of the current skill.
		DRAWSTRING_SKILLNAME,
		// The time, frags, points, wins, or kills left until the level ends.
		DRAWSTRING_LIMITSTRINGS,
		// The team scores and their relation.
		DRAWSTRING_POINTSTRING,
		// The current player's rank and score.
		DRAWSTRING_PLACESTRING,
		// The amount of time that has passed since the level started.
		DRAWSTRING_LEVELTIME,
		// The amount of time left in the round (or level).
		DRAWSTRING_LEVELTIMELEFT,
		// The amount of time left on the intermission screen.
		DRAWSTRING_INTERMISSIONTIMELEFT,
		// The number of players in the server.
		DRAWSTRING_TOTALPLAYERS,
		// The number of players that are in the game.
		DRAWSTRING_PLAYERSINGAME,
		// The name of a team.
		DRAWSTRING_TEAMNAME,
		// The total number of players on a team.
		DRAWSTRING_TEAMPLAYERCOUNT,
		// The number of players still alive on a team.
		DRAWSTRING_TEAMLIVEPLAYERCOUNT,
		// How many frags this team has.
		DRAWSTRING_TEAMFRAGCOUNT,
		// How many points this team has.
		DRAWSTRING_TEAMPOINTCOUNT,
		// How many wins this team has.
		DRAWSTRING_TEAMWINCOUNT,
		// How many deaths this team has.
		DRAWSTRING_TEAMDEATHCOUNT,
		// The number of true spectators.
		DRAWSTRING_SPECTATORCOUNT,

		DRAWSTRING_STATIC = -1
	};

	struct PreprocessedString
	{
		FBrokenLines *pLines;
		ULONG ulTotalHeight;
	};

	//*************************************************************************
	//
	// [AK] Parses the string, font, or text color, or parses the parameters
	// from the DrawBaseCommand class.
	//
	//*************************************************************************

	virtual void ParseParameter( FScanner &sc, const FName ParameterName, const PARAMETER_e Parameter )
	{
		// [AK] All special values supported by the "DrawString" command.
		const SpecialValueList<DRAWSTRINGVALUE_e> SpecialValues
		{
			{ "hostname",				{ DRAWSTRING_HOSTNAME,				MARGINTYPE_HEADER_OR_FOOTER }},
			{ "gamemode",				{ DRAWSTRING_GAMEMODE,				MARGINTYPE_HEADER_OR_FOOTER }},
			{ "levelname",				{ DRAWSTRING_LEVELNAME,				MARGINTYPE_HEADER_OR_FOOTER }},
			{ "levellump",				{ DRAWSTRING_LEVELLUMP,				MARGINTYPE_HEADER_OR_FOOTER }},
			{ "skillname",				{ DRAWSTRING_SKILLNAME,				MARGINTYPE_HEADER_OR_FOOTER }},
			{ "limitstrings",			{ DRAWSTRING_LIMITSTRINGS,			MARGINTYPE_HEADER_OR_FOOTER }},
			{ "pointstring",			{ DRAWSTRING_POINTSTRING,			MARGINTYPE_HEADER_OR_FOOTER }},
			{ "placestring",			{ DRAWSTRING_PLACESTRING,			MARGINTYPE_HEADER_OR_FOOTER }},
			{ "leveltime",				{ DRAWSTRING_LEVELTIME,				MARGINTYPE_HEADER_OR_FOOTER }},
			{ "leveltimeleft",			{ DRAWSTRING_LEVELTIMELEFT,			MARGINTYPE_HEADER_OR_FOOTER }},
			{ "intermissiontimeleft",	{ DRAWSTRING_INTERMISSIONTIMELEFT,	MARGINTYPE_HEADER_OR_FOOTER }},
			{ "totalplayers",			{ DRAWSTRING_TOTALPLAYERS,			MARGINTYPE_HEADER_OR_FOOTER }},
			{ "playersingame",			{ DRAWSTRING_PLAYERSINGAME,			MARGINTYPE_HEADER_OR_FOOTER }},
			{ "teamname",				{ DRAWSTRING_TEAMNAME,				MARGINTYPE_TEAM }},
			{ "teamplayercount",		{ DRAWSTRING_TEAMPLAYERCOUNT,		MARGINTYPE_TEAM }},
			{ "teamliveplayercount",	{ DRAWSTRING_TEAMLIVEPLAYERCOUNT,	MARGINTYPE_TEAM }},
			{ "teamfragcount",			{ DRAWSTRING_TEAMFRAGCOUNT,			MARGINTYPE_TEAM }},
			{ "teampointcount",			{ DRAWSTRING_TEAMPOINTCOUNT,		MARGINTYPE_TEAM }},
			{ "teamwincount",			{ DRAWSTRING_TEAMWINCOUNT,			MARGINTYPE_TEAM }},
			{ "teamdeathcount",			{ DRAWSTRING_TEAMDEATHCOUNT,		MARGINTYPE_TEAM }},
			{ "spectatorcount",			{ DRAWSTRING_SPECTATORCOUNT,		MARGINTYPE_SPECTATOR }},
		};

		switch ( Parameter )
		{
			case PARAMETER_VALUE:
			{
				// [AK] Keep processing the string "chunks", each separated by a '+'.
				do
				{
					const DRAWSTRINGVALUE_e SpecialValue = GetSpecialValue( sc, SpecialValues );
					StringChunks.Push( { SpecialValue, ( SpecialValue != DRAWSTRING_STATIC ) ? "" : sc.String } );

				} while ( sc.CheckToken( '+' ));

				break;
			}

			case PARAMETER_FONT:
			{
				sc.MustGetToken( TK_StringConst );

				// [AK] Throw a fatal error if an empty font name was passed.
				if ( sc.StringLen == 0 )
					sc.ScriptError( "Got an empty string for a font name." );

				pFont = V_GetFont( sc.String );

				// [AK] Throw a fatal error if the font wasn't found.
				if ( pFont == NULL )
					sc.ScriptError( "Couldn't find font '%s'.", sc.String );

				break;
			}

			case PARAMETER_TEXTCOLOR:
			{
				if ( sc.CheckToken( TK_Identifier ))
				{
					// [AK] A team's text colour can be used inside a team header.
					if ( stricmp( sc.String, "teamtextcolor" ) == 0 )
					{
						if ( pParentMargin->GetType( ) != MARGINTYPE_TEAM )
							sc.ScriptError( "'teamtextcolor' can't be used inside a '%s' margin.", pParentMargin->GetName( ));

						bUsingTeamColor = true;
					}
					else
					{
						sc.ScriptError( "Unknown identifier '%s'. Did you mean to use 'teamtextcolor'?", sc.String );
					}
				}
				else
				{
					sc.MustGetToken( TK_StringConst );

					// [AK] If an empty string was passed, inform the user of the error and switch to untranslated.
					if ( sc.StringLen == 0 )
					{
						sc.ScriptMessage( "Got an empty string for a text color, using untranslated instead." );
					}
					else
					{
						Color = V_FindFontColor( sc.String );

						// [AK] If the text color name was invalid, let the user know about it.
						if (( Color == CR_UNTRANSLATED ) && ( stricmp( sc.String, "untranslated" ) != 0 ))
							sc.ScriptMessage( "'%s' is an unknown text color, using untranslated instead.", sc.String );
					}
				}

				break;
			}

			case PARAMETER_GAPSIZE:
			{
				sc.MustGetToken( TK_IntConst );
				ulGapSize = MAX( sc.Number, 0 );

				break;
			}

			default:
				DrawBaseCommand::ParseParameter( sc, ParameterName, Parameter );
		}
	}

	//*************************************************************************
	//
	// [AK] Processes a string.
	//
	//*************************************************************************

	void CreateString( const ULONG ulDisplayPlayer, const ULONG ulTeam )
	{
		PreprocessedString String;
		FString text;

		// [AK] Create the final string using all of the string chunks.
		for ( unsigned int i = 0; i < StringChunks.Size( ); i++ )
		{
			if ( StringChunks[i].first != DRAWSTRING_STATIC )
			{
				const DRAWSTRINGVALUE_e Value = StringChunks[i].first;

				switch ( Value )
				{
					case DRAWSTRING_HOSTNAME:
					{
						FString HostName = sv_hostname.GetGenericRep( CVAR_String ).String;
						V_ColorizeString( HostName );

						text += HostName;
						break;
					}

					case DRAWSTRING_GAMEMODE:
						text += GAMEMODE_GetCurrentName( );
						break;

					case DRAWSTRING_LEVELNAME:
						text += level.LevelName;
						break;

					case DRAWSTRING_LEVELLUMP:
						text += level.mapname;
						break;

					case DRAWSTRING_SKILLNAME:
						text += G_SkillName( );
						break;

					case DRAWSTRING_LIMITSTRINGS:
					{
						std::list<FString> lines;
						SCOREBOARD_BuildLimitStrings( lines, true );

						for ( std::list<FString>::iterator it = lines.begin( ); it != lines.end( ); it++ )
						{
							if ( it != lines.begin( ))
								text += '\n';

							text += *it;
						}

						break;
					}

					case DRAWSTRING_POINTSTRING:
						text += HUD_BuildPointString( );
						break;

					case DRAWSTRING_PLACESTRING:
						text += HUD_BuildPlaceString( ulDisplayPlayer );
						break;

					case DRAWSTRING_LEVELTIME:
					case DRAWSTRING_LEVELTIMELEFT:
					{
						if ( Value == DRAWSTRING_LEVELTIME )
						{
							// [AK] The level time is only active while in the level.
							if ( gamestate == GS_LEVEL )
							{
								const int levelTime = level.time / TICRATE;
								text.AppendFormat( "%02d:%02d:%02d", levelTime / 3600, ( levelTime % 3600 ) / 60, levelTime % 60 );

								break;
							}
						}
						else
						{
							// [AK] Make sure that the time limit is active right now.
							if ( GAMEMODE_IsTimelimitActive( ))
							{
								FString TimeLeft;
								GAMEMODE_GetTimeLeftString( TimeLeft );

								text += TimeLeft;
								break;
							}
						}

						text += "00:00:00";
						break;
					}

					case DRAWSTRING_INTERMISSIONTIMELEFT:
						text.AppendFormat( "%d", ( gamestate == GS_INTERMISSION ) ? WI_GetStopWatch( ) / TICRATE + 1 : 0 );
						break;

					case DRAWSTRING_TOTALPLAYERS:
						text.AppendFormat( "%d", SERVER_CountPlayers( true ));
						break;

					case DRAWSTRING_PLAYERSINGAME:
						text.AppendFormat( "%d", HUD_GetNumPlayers( ));
						break;

					case DRAWSTRING_TEAMNAME:
						text += TEAM_GetName( ulTeam );
						break;

					case DRAWSTRING_TEAMPLAYERCOUNT:
						text.AppendFormat( "%d", TEAM_CountPlayers( ulTeam ));
						break;

					case DRAWSTRING_TEAMLIVEPLAYERCOUNT:
						text.AppendFormat( "%d", TEAM_CountLivingAndRespawnablePlayers( ulTeam ));
						break;

					case DRAWSTRING_TEAMFRAGCOUNT:
						text.AppendFormat( "%d", TEAM_GetFragCount( ulTeam ));
						break;

					case DRAWSTRING_TEAMPOINTCOUNT:
						text.AppendFormat( "%d", TEAM_GetPointCount( ulTeam ));
						break;

					case DRAWSTRING_TEAMWINCOUNT:
						text.AppendFormat( "%d", TEAM_GetWinCount( ulTeam ));
						break;

					case DRAWSTRING_TEAMDEATHCOUNT:
						text.AppendFormat( "%d", TEAM_GetDeathCount( ulTeam ));
						break;

					case DRAWSTRING_SPECTATORCOUNT:
						text.AppendFormat( "%d", HUD_GetNumSpectators( ));
						break;
				}
			}
			else
			{
				text += StringChunks[i].second;
			}
		}

		String.pLines = V_BreakLines( pFont, pParentMargin->GetWidth( ), text.GetChars( ));
		String.ulTotalHeight = 0;

		// [AK] Determine the total height of the string.
		for ( unsigned int i = 0; String.pLines[i].Width >= 0; i++ )
		{
			if ( i > 0 )
				String.ulTotalHeight += ulGapSize;

			String.ulTotalHeight += pFont->GetHeight( );
		}

		PreprocessedStrings.Push( String );
		EnsureContentFitsInMargin( String.ulTotalHeight );
	}

	//*************************************************************************
	//
	// [AK] Returns a pointer to a preprocessed string belonging to a specific
	// team (or no team). Includes checks to ensure that the string exists.
	//
	//*************************************************************************

	PreprocessedString *RetrieveString( const ULONG ulTeam ) const
	{
		if ( ulTeam == ScoreMargin::NO_TEAM )
		{
			// [AK] If there's no string at all, then something went wrong.
			if ( PreprocessedStrings.Size( ) == 0 )
				I_Error( "DrawString::RetrieveString: there is no string to retrieve." );

			return &PreprocessedStrings[0];
		}
		else
		{
			// [AK] If this team has no string, then something went wrong.
			if ( ulTeam >= PreprocessedStrings.Size( ))
				I_Error( "DrawString::RetrieveString: there is no string to retrieve for team %d.", ulTeam );

			return &PreprocessedStrings[ulTeam];
		}
	}

	TArray<std::pair<DRAWSTRINGVALUE_e, FString>> StringChunks;
	TArray<PreprocessedString> PreprocessedStrings;
	FFont *pFont;
	EColorRange Color;
	ULONG ulGapSize;
	bool bUsingTeamColor;
};

//*****************************************************************************
//*****************************************************************************
//
// [AK] DrawColor
//
// Draws a rectangular box of a color somewhere in the margin.
//
//*****************************************************************************
//*****************************************************************************

class DrawColor : public DrawBaseCommand
{
public:
	DrawColor( ScoreMargin *pMargin ) : DrawBaseCommand( pMargin, COMMAND_COLOR ),
		ValueType( DRAWCOLOR_STATIC ),
		Color( 0 ),
		ulWidth( 0 ),
		ulHeight( 0 ) { }

	//*************************************************************************
	//
	// [AK] Increases the height of the margin to ensure that it fits the box.
	//
	//*************************************************************************

	virtual void Refresh( const ULONG ulDisplayPlayer )
	{
		EnsureContentFitsInMargin( ulHeight );
	}

	//*************************************************************************
	//
	// [AK] Draws the color box on the margin.
	//
	//*************************************************************************

	virtual void Draw( const ULONG ulDisplayPlayer, const ULONG ulTeam, const LONG lYPos, const float fAlpha ) const
	{
		const ULONG ulWidthToUse = MIN( ulWidth, pParentMargin->GetWidth( ));
		const TVector2<LONG> Pos = GetDrawingPosition( ulWidthToUse, ulHeight );
		const PalEntry ColorToDraw = ( ValueType == DRAWCOLOR_TEAMCOLOR ) ? TEAM_GetColor( ulTeam ) : Color;

		int clipLeft = Pos.X;
		int clipWidth = ulWidthToUse;
		int clipTop = Pos.Y + lYPos;
		int clipHeight = ulHeight;

		// [AK] We must take into account the virtual screen's size when setting up the clipping rectangle.
		if ( g_bScale )
			screen->VirtualToRealCoordsInt( clipLeft, clipTop, clipWidth, clipHeight, con_virtualwidth, con_virtualheight, false, !con_scaletext_usescreenratio );

		screen->Dim( ColorToDraw, fAlpha * fTranslucency, clipLeft, clipTop, clipWidth, clipHeight );
	}

protected:
	enum DRAWCOLORVALUE_e
	{
		// The color of a team.
		DRAWCOLOR_TEAMCOLOR,

		DRAWCOLOR_STATIC = -1
	};

	//*************************************************************************
	//
	// [AK] Parses the color, width, or height, or parses any parameters
	// from the DrawBaseCommand class.
	//
	//*************************************************************************

	virtual void ParseParameter( FScanner &sc, const FName ParameterName, const PARAMETER_e Parameter )
	{
		// [AK] All special values supported by the "DrawColor" command.
		const SpecialValueList<DRAWCOLORVALUE_e> SpecialValues
		{
			{ "teamcolor",	{ DRAWCOLOR_TEAMCOLOR,	MARGINTYPE_TEAM }},
		};

		switch ( Parameter )
		{
			case PARAMETER_VALUE:
			{
				ValueType = GetSpecialValue( sc, SpecialValues );

				if ( ValueType == DRAWCOLOR_STATIC )
				{
					FString ColorString = V_GetColorStringByName( sc.String );
					Color = V_GetColorFromString( NULL, ColorString.IsNotEmpty( ) ? ColorString.GetChars( ) : sc.String );
				}

				break;
			}

			case PARAMETER_WIDTH:
			case PARAMETER_HEIGHT:
			{
				sc.MustGetToken( TK_IntConst );

				if ( Parameter == PARAMETER_WIDTH )
					ulWidth = MAX( sc.Number, 1 );
				else
					ulHeight = MAX( sc.Number, 1 );

				break;
			}

			default:
				DrawBaseCommand::ParseParameter( sc, ParameterName, Parameter );
		}
	}

	DRAWCOLORVALUE_e ValueType;
	PalEntry Color;
	ULONG ulWidth;
	ULONG ulHeight;
};

//*****************************************************************************
//*****************************************************************************
//
// [AK] DrawTexture
//
// Draws a graphic or image somewhere in the margin.
//
//*****************************************************************************
//*****************************************************************************

class DrawTexture : public DrawBaseCommand
{
public:
	DrawTexture( ScoreMargin *pMargin ) : DrawBaseCommand( pMargin, COMMAND_TEXTURE ),
		ValueType( DRAWTEXTURE_STATIC ),
		pTexture( NULL ) { }

	//*************************************************************************
	//
	// [AK] Increases the height of the margin so that it can fit the texture.
	//
	//*************************************************************************

	virtual void Refresh( const ULONG ulDisplayPlayer )
	{
		if ( pParentMargin->GetType( ) == MARGINTYPE_TEAM )
		{
			for ( ULONG ulTeam = 0; ulTeam < TEAM_GetNumAvailableTeams( ); ulTeam++ )
			{
				FTexture *pTeamTexture = RetrieveTexture( ulTeam );

				if ( pTeamTexture != NULL )
					EnsureContentFitsInMargin( pTeamTexture->GetScaledHeight( ));
			}
		}
		else if ( pTexture != NULL )
		{
			EnsureContentFitsInMargin( pTexture->GetScaledHeight( ));
		}
	}

	//*************************************************************************
	//
	// [AK] Draws the texture on the margin.
	//
	//*************************************************************************

	virtual void Draw( const ULONG ulDisplayPlayer, const ULONG ulTeam, const LONG lYPos, const float fAlpha ) const
	{
		FTexture *pTextureToDraw = RetrieveTexture( ulTeam );

		// [AK] Stop here if the texture doesn't exist for some reason.
		if ( pTextureToDraw == NULL )
			return;

		const TVector2<LONG> Pos = GetDrawingPosition( pTextureToDraw->GetScaledWidth( ), pTextureToDraw->GetScaledHeight( ));

		int clipLeft = ( HUD_GetWidth( ) - pParentMargin->GetWidth( )) / 2;
		int clipWidth = pParentMargin->GetWidth( );
		int clipTop = lYPos;
		int clipHeight = pParentMargin->GetHeight( );

		// [AK] We must take into account the virtual screen's size when setting up the clipping rectangle.
		if ( g_bScale )
			screen->VirtualToRealCoordsInt( clipLeft, clipTop, clipWidth, clipHeight, con_virtualwidth, con_virtualheight, false, !con_scaletext_usescreenratio );

		screen->DrawTexture( pTextureToDraw, Pos.X, Pos.Y + lYPos,
			DTA_UseVirtualScreen, g_bScale,
			DTA_ClipLeft, clipLeft,
			DTA_ClipRight, clipLeft + clipWidth,
			DTA_ClipTop, clipTop,
			DTA_ClipBottom, clipTop + clipHeight,
			DTA_Alpha, FLOAT2FIXED( fAlpha * fTranslucency ),
			TAG_DONE );
	}

protected:
	enum DRAWTEXTUREVALUE_e
	{
		// The logo of a team.
		DRAWTEXTURE_TEAMLOGO,

		DRAWTEXTURE_STATIC = -1
	};

	//*************************************************************************
	//
	// [AK] Parses the texture, also making sure that it's valid, or parses
	// any parameters from the DrawBaseCommand class.
	//
	//*************************************************************************

	virtual void ParseParameter( FScanner &sc, const FName ParameterName, const PARAMETER_e Parameter )
	{
		// [AK] All special values supported by the "DrawTexture" command.
		const SpecialValueList<DRAWTEXTUREVALUE_e> SpecialValues
		{
			{ "teamlogo",	{ DRAWTEXTURE_TEAMLOGO,		MARGINTYPE_TEAM }},
		};

		if ( Parameter == PARAMETER_VALUE )
		{
			ValueType = GetSpecialValue( sc, SpecialValues );

			if ( ValueType == DRAWTEXTURE_STATIC )
			{
				pTexture = TexMan.FindTexture( sc.String );

				// [AK] If the texture wasn't found, throw a fatal error.
				if ( pTexture == NULL )
					sc.ScriptError( "Couldn't find texture '%s'.", sc.String );
			}
		}
		else
		{
			DrawBaseCommand::ParseParameter( sc, ParameterName, Parameter );
		}
	}

	//*************************************************************************
	//
	// [AK] Returns a pointer to the texture that should be used.
	//
	//*************************************************************************

	FTexture *RetrieveTexture( const ULONG ulTeam ) const
	{
		if ( ulTeam != ScoreMargin::NO_TEAM )
		{
			if (( ulTeam < teams.Size( )) && ( ValueType == DRAWTEXTURE_TEAMLOGO ))
				return TexMan.FindTexture( teams[ulTeam].Logo );
		}
		else
		{
			return pTexture;
		}

		return NULL;
	}

	DRAWTEXTUREVALUE_e ValueType;
	FTexture *pTexture;
};

//*****************************************************************************
//	FUNCTIONS

//*****************************************************************************
//
// [AK] ScoreMargin::BaseCommand::BaseCommand
//
// Initializes a margin command.
//
//*****************************************************************************

ScoreMargin::BaseCommand::BaseCommand( ScoreMargin *pMargin ) : pParentMargin( pMargin )
{
	// [AK] This should never happen, but throw a fatal error if it does.
	if ( pParentMargin == NULL )
		I_Error( "ScoreMargin::BaseCommand: parent margin is NULL." );
}

//*****************************************************************************
//
// [AK] ScoreMargin::ScoreMargin
//
// Initializes a margin's members to their default values.
//
//*****************************************************************************

ScoreMargin::ScoreMargin( MARGINTYPE_e MarginType, const char *pszName ) :
	Type( MarginType ),
	Name( pszName ),
	ulWidth( 0 ),
	ulHeight( 0 ) { }

//*****************************************************************************
//
// [AK] ScoreMargin::Parse
//
// Parses a margin (e.g. "mainheader", "teamheader", "spectatorheader", or
// "footer") block in SCORINFO.
//
//*****************************************************************************

void ScoreMargin::Parse( FScanner &sc )
{
	ClearCommands( );
	sc.MustGetToken( '{' );

	while ( sc.CheckToken( '}' ) == false )
		Commands.Push( CreateCommand( sc, this ));
}

//*****************************************************************************
//
// [AK] ScoreMargin::Refresh
//
// Updates the margin's width and height, then refreshes its command list.
//
//*****************************************************************************

void ScoreMargin::Refresh( const ULONG ulDisplayPlayer, const ULONG ulNewWidth )
{
	// [AK] If there's no commands, then don't do anything.
	if ( Commands.Size( ) == 0 )
		return;

	// [AK] Never accept a width of zero, throw a fatal error if this happens.
	if ( ulNewWidth == 0 )
		I_Error( "ScoreMargin::Refresh: tried assigning a width of zero to '%s'.", GetName( ));

	ulWidth = ulNewWidth;
	ulHeight = 0;

	for ( unsigned int i = 0; i < Commands.Size( ); i++ )
		Commands[i]->Refresh( ulDisplayPlayer );
}

//*****************************************************************************
//
// [AK] ScoreMargin::Render
//
// Draws all commands that are defined inside the margin.
//
//*****************************************************************************

void ScoreMargin::Render( const ULONG ulDisplayPlayer, const ULONG ulTeam, LONG &lYPos, const float fAlpha ) const
{
	// [AK] If this is supposed to be a team header, then we can't draw for invalid teams!
	if ( Type == MARGINTYPE_TEAM )
	{
		if (( ulTeam == NO_TEAM ) || ( ulTeam >= teams.Size( )))
			I_Error( "ScoreMargin::Render: '%s' can't be drawn for invalid teams.", GetName( ));
	}
	// [AK] Otherwise, if this is a non-team header, then we can't draw for any specific team!
	else if ( ulTeam != NO_TEAM )
	{
		I_Error( "ScoreMargin::Render: '%s' must not be drawn for any specific team.", GetName( ));
	}

	// [AK] If there's no commands, or the width or height are zero, then we can't draw anything.
	if (( Commands.Size( ) == 0 ) || ( ulWidth == 0 ) || ( ulHeight == 0 ))
		return;

	for ( unsigned int i = 0; i < Commands.Size( ); i++ )
		Commands[i]->Draw( ulDisplayPlayer, ulTeam, lYPos, fAlpha );

	lYPos += ulHeight;
}

//*****************************************************************************
//
// [AK] ScoreMargin::CreateCommand
//
// A "factory" function that's responsible for creating new margin commands.
//
//*****************************************************************************

ScoreMargin::BaseCommand *ScoreMargin::CreateCommand( FScanner &sc, ScoreMargin *pMargin )
{
	const MARGINCMD_e Command = static_cast<MARGINCMD_e>( sc.MustGetEnumName( "margin command", "MARGINCMD_", GetValueMARGINCMD_e ));
	ScoreMargin::BaseCommand *pNewCommand = NULL;

	switch ( Command )
	{
		case MARGINCMD_DRAWSTRING:
			pNewCommand = new DrawString( pMargin );
			break;

		case MARGINCMD_DRAWCOLOR:
			pNewCommand = new DrawColor( pMargin );
			break;

		case MARGINCMD_DRAWTEXTURE:
			pNewCommand = new DrawTexture( pMargin );
			break;
	}

	// [AK] If the command wasn't created, then something went wrong.
	if ( pNewCommand == NULL )
		sc.ScriptError( "Couldn't create margin command '%s'.", sc.String );

	// [AK] A command's arguments must always be prepended by a '('.
	sc.MustGetToken( '(' );
	pNewCommand->Parse( sc );

	return pNewCommand;
}

//*****************************************************************************
//
// [AK] ScoreMargin::ClearCommands
//
// Deletes all of the margin's commands from memory.
//
//*****************************************************************************

void ScoreMargin::ClearCommands( void )
{
	for ( unsigned int i = 0; i < Commands.Size( ); i++ )
	{
		delete Commands[i];
		Commands[i] = NULL;
	}

	Commands.Clear( );
}
