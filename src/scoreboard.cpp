//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2002 Brad Carney
// Copyright (C) 2007-2012 Skulltag Development Team
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
// 3. Neither the name of the Skulltag Development Team nor the names of its
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
// Filename: scoreboard.cpp
//
// Description: Contains scoreboard routines and globals
//
//-----------------------------------------------------------------------------

#include <set>
#include "a_pickups.h"
#include "c_dispatch.h"
#include "callvote.h"
#include "chat.h"
#include "cl_demo.h"
#include "cooperative.h"
#include "deathmatch.h"
#include "duel.h"
#include "doomtype.h"
#include "d_player.h"
#include "gamemode.h"
#include "gi.h"
#include "invasion.h"
#include "joinqueue.h"
#include "lastmanstanding.h"
#include "network.h"
#include "sbar.h"
#include "scoreboard.h"
#include "st_stuff.h"
#include "team.h"
#include "templates.h"
#include "v_text.h"
#include "v_video.h"
#include "w_wad.h"
#include "c_bind.h"	// [RC] To tell user what key to press to vote.
#include "st_hud.h"
#include "wi_stuff.h"
#include "c_console.h"
#include "g_game.h"
#include "d_netinf.h"
#include "v_palette.h"
#include "r_data/r_translate.h"

// [AK] Implement the string table and the conversion functions for the scoreboard enums.
#define GENERATE_ENUM_STRINGS  // Start string generation
#include "scoreboard_enums.h"
#undef GENERATE_ENUM_STRINGS   // Stop string generation

//*****************************************************************************
//	VARIABLES

// [AK] A list of all defined columns.
static	TMap<FName, ScoreColumn *>	g_Columns;

// [AK] The main scoreboard object.
static	Scoreboard	g_Scoreboard;

// Player list according to rank.
static	int		g_iSortedPlayers[MAXPLAYERS];

// [AK] The level we are entering, to be shown on the intermission screen.
static	level_info_t *g_pNextLevel;

// Current position of our "pen".
static	ULONG		g_ulCurYPos;

// How many columns are we using in our scoreboard display?
static	ULONG		g_ulNumColumnsUsed = 0;

// Array that has the type of each column.
static	ULONG		g_aulColumnType[MAX_COLUMNS];

// X position of each column.
static	ULONG		g_aulColumnX[MAX_COLUMNS];

// What font are the column headers using?
static	FFont		*g_pColumnHeaderFont = NULL;

// [AK] Do we need to update the scoreboard before we draw it on the screen?
static	bool		g_bRefreshBeforeRendering = false;

// This is the header for each column type.
static	const char	*g_pszColumnHeaders[NUM_COLUMN_TYPES] =
{
	"",
	"NAME",
	"TIME",
	"PING",
	"FRAGS",
	"POINTS",
	"WINS",
	"KILLS",
	"DEATHS",
	"ASSISTS",
	"SECRETS",
};

//*****************************************************************************
//	PROTOTYPES

static	void			scoreboard_SortPlayers( ULONG ulSortType );
static	int	STACK_ARGS	scoreboard_FragCompareFunc( const void *arg1, const void *arg2 );
static	int	STACK_ARGS	scoreboard_PointsCompareFunc( const void *arg1, const void *arg2 );
static	int	STACK_ARGS	scoreboard_KillsCompareFunc( const void *arg1, const void *arg2 );
static	int	STACK_ARGS	scoreboard_WinsCompareFunc( const void *arg1, const void *arg2 );
static	void			scoreboard_RenderIndividualPlayer( ULONG ulDisplayPlayer, ULONG ulPlayer );
static	void			scoreboard_DrawHeader( ULONG ulPlayer );
static	void			scoreboard_ClearColumns( void );
static	void			scoreboard_Prepare5ColumnDisplay( void );
static	void			scoreboard_Prepare4ColumnDisplay( void );
static	void			scoreboard_Prepare3ColumnDisplay( void );
static	void			scoreboard_DoRankingListPass( ULONG ulPlayer, LONG lSpectators, LONG lDead, LONG lNotPlaying, LONG lNoTeam, LONG lWrongTeam, ULONG ulDesiredTeam );
static	void			scoreboard_DrawRankings( ULONG ulPlayer );
static	void			scoreboard_DrawText( const char *pszString, EColorRange Color, ULONG &ulXPos, ULONG ulOffset, bool bOffsetRight = false );
static	void			scoreboard_DrawIcon( const char *pszPatchName, ULONG &ulXPos, ULONG ulYPos, ULONG ulOffset, bool bOffsetRight = false );
static	ScoreColumn		*scoreboard_ScanForColumn( FScanner &sc, const bool bMustBeDataColumn );

template<typename ColumnType>
static	bool			scoreboard_TryPushingColumnToList( FScanner &sc, TArray<ColumnType *> &ColumnList, ColumnType *pColumn, const char *pszColumnName );

//*****************************************************************************
//	CONSOLE VARIABLES

// [JS] Display the amount of time left on the intermission screen.
CVAR( Bool, cl_intermissiontimer, false, CVAR_ARCHIVE );

// [AK] Prints everyone's pings in different colours, indicating how severe their connection is.
CVAR( Bool, cl_colorizepings, false, CVAR_ARCHIVE );

// [AK] If true, then columns will use their short names in the headers.
CUSTOM_CVAR( Bool, cl_useshortcolumnnames, false, CVAR_ARCHIVE )
{
	SCOREBOARD_ShouldRefreshBeforeRendering( );
}

// [AK] Controls the opacity of the entire scoreboard.
CUSTOM_CVAR( Float, cl_scoreboardalpha, 1.0f, CVAR_ARCHIVE )
{
	float fClampedValue = clamp<float>( self, 0.0f, 1.0f );

	if ( self != fClampedValue )
		self = fClampedValue;
}

//*****************************************************************************
//
// [AK] ScoreColumn::ScoreColumn
//
// Initializes the members of a ScoreColumn object to their default values.
//
//*****************************************************************************

ScoreColumn::ScoreColumn( const char *pszName ) :
	DisplayName( pszName ),
	Alignment( COLUMNALIGN_LEFT ),
	pCVar( NULL ),
	ulFlags( 0 ),
	ulSizing( 0 ),
	ulShortestWidth( 0 ),
	ulWidth( 0 ),
	lRelX( 0 ),
	bUsableInCurrentGame( false ),
	bDisabled( false ),
	bHidden( false ),
	bUseShortName( false ),
	pScoreboard( NULL )
{
	// [AK] By default, this column is active in all game types and earn types.
	ulGameAndEarnTypeFlags = ( GAMETYPE_MASK | EARNTYPE_MASK );

	// [AK] By default, this column is active in all game modes.
	for ( ULONG ulGameMode = 0; ulGameMode < NUM_GAMEMODES; ulGameMode++ )
		GameModeList.insert( static_cast<GAMEMODE_e>( ulGameMode ));
}

//*****************************************************************************
//
// [AK] ScoreColumn::GetAlignmentPosition
//
// Uses the width of some content (e.g. a string, color box, or texture) and
// determines where the left-most part of that content should start with
// respect to the column's own position, width, and alignment.
//
//*****************************************************************************

LONG ScoreColumn::GetAlignmentPosition( ULONG ulContentWidth ) const
{
	if ( Alignment == COLUMNALIGN_LEFT )
		return lRelX;
	else if ( Alignment == COLUMNALIGN_CENTER )
		return lRelX + ( ulWidth - ulContentWidth ) / 2;
	else
		return lRelX + ulWidth - ulContentWidth;
}

//*****************************************************************************
//
// [AK] ScoreColumn::SetHidden
//
// Hides (or unhides) the column, requiring the scoreboard to be refreshed.
//
//*****************************************************************************

void ScoreColumn::SetHidden( bool bEnable )
{
	if ( bHidden == bEnable )
		return;

	bHidden = bEnable;
	SCOREBOARD_ShouldRefreshBeforeRendering( );
}

//*****************************************************************************
//
// [AK] ScoreColumn::Parse
//
// Parses a "column" or "compositecolumn" block in SCORINFO.
//
//*****************************************************************************

void ScoreColumn::Parse( const FName Name, FScanner &sc )
{
	sc.MustGetToken( '{' );

	while ( sc.CheckToken( '}' ) == false )
	{
		sc.MustGetString( );

		if ( stricmp( sc.String, "addflag" ) == 0 )
		{
			ulFlags |= sc.MustGetEnumName( "column flag", "COLUMNFLAG_", GetValueCOLUMNFLAG_e );
		}
		else if ( stricmp( sc.String, "removeflag" ) == 0 )
		{
			ulFlags &= ~sc.MustGetEnumName( "column flag", "COLUMNFLAG_", GetValueCOLUMNFLAG_e );
		}
		else
		{
			COLUMNCMD_e Command = static_cast<COLUMNCMD_e>( sc.MustGetEnumName( "column command", "COLUMNCMD_", GetValueCOLUMNCMD_e, true ));
			FString CommandName = sc.String;

			sc.MustGetToken( '=' );
			ParseCommand( Name, sc, Command, CommandName );
		}
	}

	// [AK] Unless the ALWAYSUSESHORTESTWIDTH flag is enabled, columns must have a non-zero width.
	if ((( ulFlags & COLUMNFLAG_ALWAYSUSESHORTESTWIDTH ) == false ) && ( ulSizing == 0 ))
		sc.ScriptError( "Column '%s' needs a size that's greater than zero.", Name.GetChars( ));

	// [AK] If the short name is longer than the display name, throw a fatal error.
	if ( DisplayName.Len( ) < ShortName.Len( ))
		sc.ScriptError( "Column '%s' has a short name that's greater than its display name.", Name.GetChars( ));
}

//*****************************************************************************
//
// [AK] ScoreColumn::ParseCommand
//
// Parses commands that are shared by all (data and composite) columns.
//
//*****************************************************************************

void ScoreColumn::ParseCommand( const FName Name, FScanner &sc, const COLUMNCMD_e Command, const FString CommandName )
{
	switch ( Command )
	{
		case COLUMNCMD_DISPLAYNAME:
		case COLUMNCMD_SHORTNAME:
		{
			sc.MustGetString( );

			// [AK] If the name begins with a '$', look up the string in the LANGUAGE lump.
			const char *pszString = sc.String[0] == '$' ? GStrings[sc.String] : sc.String;

			if ( Command == COLUMNCMD_DISPLAYNAME )
				DisplayName = pszString;
			else
				ShortName = pszString;

			break;
		}

		case COLUMNCMD_ALIGNMENT:
		{
			Alignment = static_cast<COLUMNALIGN_e>( sc.MustGetEnumName( "alignment", "COLUMNALIGN_", GetValueCOLUMNALIGN_e ));
			break;
		}

		case COLUMNCMD_SIZE:
		{
			sc.MustGetNumber( );
			ulSizing = MAX( sc.Number, 0 );
			break;
		}

		case COLUMNCMD_GAMEMODE:
		case COLUMNCMD_GAMETYPE:
		case COLUMNCMD_EARNTYPE:
		{
			// [AK] Clear all game modes.
			if ( Command == COLUMNCMD_GAMEMODE )
				GameModeList.clear( );
			// ...or reset all game type flags.
			else if ( Command == COLUMNCMD_GAMETYPE )
				ulGameAndEarnTypeFlags &= ~GAMETYPE_MASK;
			// ...or reset all earn type flags.
			else
				ulGameAndEarnTypeFlags &= ~EARNTYPE_MASK;

			do
			{
				sc.MustGetToken( TK_Identifier );

				if ( Command == COLUMNCMD_GAMEMODE )
				{
					GameModeList.insert( static_cast<GAMEMODE_e>( sc.MustGetEnumName( "game mode", "GAMEMODE_", GetValueGAMEMODE_e, true )));
				}
				else if ( Command == COLUMNCMD_GAMETYPE )
				{
					ULONG ulFlag = sc.MustGetEnumName( "game type", "GMF_", GetValueGMF, true );

					// [AK] Make sure there aren't other constants besides COOPERATIVE, DEATHMATCH, or TEAMGAME.
					if (( ulFlag & GAMETYPE_MASK ) == 0 )
						sc.ScriptError( "A game type list must contain only COOPERATIVE, DEATHMATCH, or TEAMGAME. Using '%s' is invalid.", sc.String );

					ulGameAndEarnTypeFlags |= ulFlag;
				}
				else
				{
					ulGameAndEarnTypeFlags |= sc.MustGetEnumName( "earn type", "GMF_PLAYERSEARN", GetValueGMF, true );
				}
			} while ( sc.CheckToken( ',' ));

			break;
		}

		case COLUMNCMD_CVAR:
		{
			sc.MustGetString( );

			// [AK] Specifying "none" for the CVar clears any CVar being used by the column.
			// This also means that a CVar named "none" (if one actually existed) can never be used.
			if (( stricmp( sc.String, "none" ) == 0 ) && ( pCVar != NULL ))
			{
				pCVar->SetRefreshScoreboardBit( false );
				pCVar = NULL;
			}
			else
			{
				FBaseCVar *pFoundCVar = FindCVar( sc.String, NULL );

				// [AK] Throw an error if this CVar doesn't exist.
				if ( pFoundCVar == NULL )
					sc.ScriptError( "'%s' is not a CVar.", sc.String );

				// [AK] Throw an error if this CVar isn't a boolean, integer, or flag.
				if (( pFoundCVar->GetRealType( ) != CVAR_Bool ) && ( pFoundCVar->IsFlagCVar( ) == false ))
					sc.ScriptError( "'%s' is not a boolean or flag CVar.", sc.String );

				pCVar = pFoundCVar;
				pCVar->SetRefreshScoreboardBit( true );
			}

			break;
		}

		default:
			sc.ScriptError( "Couldn't process column command '%s' for column '%s'.", CommandName.GetChars( ), Name.GetChars( ));
	}
}

//*****************************************************************************
//
// [AK] ScoreColumn::CheckIfUsable
//
// Checks if this column works at all in the current game, including whether
// or not the current game mode is compatible, if the column is allowed in
// offline or online games, or if the column should (not) appear in-game or on
// the intermission screen.
//
//*****************************************************************************

void ScoreColumn::CheckIfUsable( void )
{
	bUsableInCurrentGame = false;

	// [AK] If the current game mode isn't allowed for this column, then it can't be active.
	if ( GameModeList.find( GAMEMODE_GetCurrentMode( )) == GameModeList.end( ))
		return;

	const ULONG ulGameModeFlags = GAMEMODE_GetCurrentFlags( );

	// [AK] Check if the current game type won't allow this column to be active.
	if ( ulGameAndEarnTypeFlags & GAMETYPE_MASK )
	{
		if ((( ulGameModeFlags & ulGameAndEarnTypeFlags ) & GAMETYPE_MASK ) == 0 )
			return;
	}

	// [AK] Check if the current game mode's earn type won't allow this column to be active.
	if ( ulGameAndEarnTypeFlags & EARNTYPE_MASK )
	{
		if ((( ulGameModeFlags & ulGameAndEarnTypeFlags ) & EARNTYPE_MASK ) == 0 )
			return;
	}

	ULONG ulRequiredFlags = 0;

	// [AK] We'll check if the column requires the PLAYERONTEAMS, USEMAXLIVES, and USETEAMITEM
	// game mode flags to be enabled. Make sure that the current game mode has all the required flags.
	if ( ulFlags & COLUMNFLAG_REQUIRESTEAMS )
		ulRequiredFlags |= GMF_PLAYERSONTEAMS;
	if ( ulFlags & COLUMNFLAG_REQUIRESLIVES )
		ulRequiredFlags |= GMF_USEMAXLIVES;
	if ( ulFlags & COLUMNFLAG_REQUIRESTEAMITEMS )
		ulRequiredFlags |= GMF_USETEAMITEM;

	if (( ulRequiredFlags != 0 ) && (( ulRequiredFlags & ulGameModeFlags ) != ulRequiredFlags ))
		return;

	ULONG ulForbiddenFlags = 0;

	// [AK] We'll also check if the column requires the aforementioned game mode flags to be disabled.
	// If the current game mode has at least one of the flags that the column forbids, stop here.
	if ( ulFlags & COLUMNFLAG_FORBIDTEAMS )
		ulForbiddenFlags |= GMF_PLAYERSONTEAMS;
	if ( ulFlags & COLUMNFLAG_FORBIDLIVES )
		ulForbiddenFlags |= GMF_USEMAXLIVES;
	if ( ulFlags & COLUMNFLAG_FORBIDTEAMITEMS )
		ulForbiddenFlags |= GMF_USETEAMITEM;

	if ( ulForbiddenFlags & ulGameModeFlags )
		return;

	// [AK] Next, we'll check if the column is only active in offline or online games. A column should
	// be disabled in online games if OFFLINEONLY is enabled, and offline games if ONLINEONLY is enabled.
	if ( NETWORK_InClientMode( ))
	{
		if ( ulFlags & COLUMNFLAG_OFFLINEONLY )
			return;
	}
	else if ( ulFlags & COLUMNFLAG_ONLINEONLY )
	{
		return;
	}

	// [AK] Disable this column if it's supposed to be invisible on the intermission screen, or if it's
	// supposed to be invisible in-game.
	if ((( gamestate == GS_INTERMISSION ) && ( ulFlags & COLUMNFLAG_NOINTERMISSION )) ||
		(( gamestate == GS_LEVEL ) && ( ulFlags & COLUMNFLAG_INTERMISSIONONLY )))
	{
		return;
	}

	bUsableInCurrentGame = true;
}

//*****************************************************************************
//
// [AK] ScoreColumn::Refresh
//
// Performs checks to see if a column should be active or disabled.
//
//*****************************************************************************

void ScoreColumn::Refresh( void )
{
	bDisabled = true;

	// [AK] If the column's currently unusable or hidden, stop here.
	if (( bUsableInCurrentGame == false ) || ( bHidden ))
		return;

	// [AK] If this column has a CVar associated with it, check to see if the column should be
	// active based on the CVar's value. If the conditions fail, stop here.
	if ( pCVar != NULL )
	{
		const bool bValue = pCVar->GetGenericRep( CVAR_Bool ).Bool;

		if ( ulFlags & COLUMNFLAG_CVARMUSTBEZERO )
		{
			if ( bValue != false )
				return;
		}
		else if ( bValue == false )
		{
			return;
		}
	}

	bDisabled = false;

	// [AK] Should this column use its short or normal display name?
	if (( cl_useshortcolumnnames ) && ( ShortName.Len( ) > 0 ))
		bUseShortName = true;
	else
		bUseShortName = false;
}

//*****************************************************************************
//
// [AK] ScoreColumn::UpdateWidth
//
// Determines what the width of the column should be right now.
//
//*****************************************************************************

void ScoreColumn::UpdateWidth( FFont *pHeaderFont, FFont *pRowFont )
{
	// [AK] Check if the column must be disabled if its contents are empty.
	if (( ulShortestWidth == 0 ) && ( ulFlags & COLUMNFLAG_DISABLEIFEMPTY ))
	{
		bDisabled = true;
		return;
	}

	ULONG ulHeaderWidth = 0;

	// [AK] If the header is visible on this column, then grab its width.
	if ((( ulFlags & COLUMNFLAG_DONTSHOWHEADER ) == false ) && ( pHeaderFont != NULL ))
		ulHeaderWidth = pHeaderFont->StringWidth( bUseShortName ? ShortName.GetChars( ) : DisplayName.GetChars( ));

	// [AK] Always use the shortest (or header) width if required. In this case,
	// the sizing is added onto the shortest width as padding instead.
	if ( ulFlags & COLUMNFLAG_ALWAYSUSESHORTESTWIDTH )
		ulWidth = MAX( ulShortestWidth, ulHeaderWidth ) + ulSizing;
	// [AK] Otherwise, set the column's width to whichever is bigger: the sizing
	// (which becomes the default width of the column), the shortest width, or
	// the header's width.
	else
		ulWidth = MAX( ulSizing, MAX( ulShortestWidth, ulHeaderWidth ));

	// [AK] If the column's width is still zero, just disable it.
	if ( ulWidth == 0 )
		bDisabled = true;
}

//*****************************************************************************
//
// [AK] ScoreColumn::DrawHeader
//
// Draws the column's header with the specified font and color.
//
//*****************************************************************************

void ScoreColumn::DrawHeader( FFont *pFont, const ULONG ulColor, const LONG lYPos, const ULONG ulHeight, const float fAlpha ) const
{
	if (( bDisabled ) || ( ulFlags & COLUMNFLAG_DONTSHOWHEADER ))
		return;

	DrawString( bUseShortName ? ShortName.GetChars( ) : DisplayName.GetChars( ), pFont, ulColor, lYPos, ulHeight, fAlpha );
}

//*****************************************************************************
//
// [AK] ScoreColumn::DrawString
//
// Draws a string within the body of the column.
//
//*****************************************************************************

void ScoreColumn::DrawString( const char *pszString, FFont *pFont, const ULONG ulColor, const LONG lYPos, const ULONG ulHeight, const float fAlpha ) const
{
	if (( pszString == NULL ) || ( pFont == NULL ))
		return;

	const ULONG ulLength = strlen( pszString );

	// [AK] Don't bother drawing the string if it's empty.
	if ( ulLength == 0 )
		return;

	LONG lXPos = GetAlignmentPosition( pFont->StringWidth( pszString ));
	ULONG ulLargestCharHeight = 0;

	// [AK] Get the largest character height so the string is aligned within the centre of the specified height.
	for ( unsigned int i = 0; i < ulLength; i++ )
	{
		FTexture *pCharTexture = pFont->GetChar( pszString[i], NULL );

		if ( pCharTexture != NULL )
		{
			const ULONG ulTextureHeight = pCharTexture->GetScaledHeight( );

			if ( ulTextureHeight > ulLargestCharHeight )
				ulLargestCharHeight = ulTextureHeight;
		}
	}

	int clipLeft = lRelX;
	int clipWidth = ulWidth;
	int clipTop = lYPos;
	int clipHeight = ulHeight;

	LONG lNewYPos = lYPos + ( clipHeight - ulLargestCharHeight ) / 2;

	// [AK] We must take into account the virtual screen's size when setting up the clipping rectangle.
	// Nothing should be drawn outside of this rectangle (i.e. the column's boundaries).
	if ( g_bScale )
		screen->VirtualToRealCoordsInt( clipLeft, clipTop, clipWidth, clipHeight, con_virtualwidth, con_virtualheight, false, !con_scaletext_usescreenratio );

	screen->DrawText( pFont, ulColor, lXPos, lNewYPos, pszString,
		DTA_UseVirtualScreen, g_bScale,
		DTA_ClipLeft, clipLeft,
		DTA_ClipRight, clipLeft + clipWidth,
		DTA_ClipTop, clipTop,
		DTA_ClipBottom, clipTop + clipHeight,
		DTA_Alpha, FLOAT2FIXED( fAlpha ),
		TAG_DONE );
}

//*****************************************************************************
//
// [AK] ScoreColumn::DrawColor
//
// Draws a hexadecimal color within the body of the column.
//
//*****************************************************************************

void ScoreColumn::DrawColor( const PalEntry color, const LONG lYPos, const ULONG ulHeight, const float fAlpha, const int clipWidth, const int clipHeight ) const
{
	int clipWidthToUse;
	int clipHeightToUse;

	FixClipRectSize( clipWidth, clipHeight, ulHeight, clipWidthToUse, clipHeightToUse );

	int clipLeft = GetAlignmentPosition( clipWidthToUse );
	int clipTop = lYPos + ( ulHeight - clipHeightToUse ) / 2;

	// [AK] We must take into account the virtual screen's size.
	if ( g_bScale )
		screen->VirtualToRealCoordsInt( clipLeft, clipTop, clipWidthToUse, clipHeightToUse, con_virtualwidth, con_virtualheight, false, !con_scaletext_usescreenratio );

	screen->Dim( color, fAlpha, clipLeft, clipTop, clipWidthToUse, clipHeightToUse );
}

//*****************************************************************************
//
// [AK] ScoreColumn::DrawTexture
//
// Draws a texture within the body of the column.
//
//*****************************************************************************

void ScoreColumn::DrawTexture( FTexture *pTexture, const LONG lYPos, const ULONG ulHeight, const float fAlpha, const int clipWidth, const int clipHeight ) const
{
	int clipWidthToUse;
	int clipHeightToUse;

	if ( pTexture == NULL )
		return;

	LONG lXPos = GetAlignmentPosition( pTexture->GetScaledWidth( ));

	FixClipRectSize( clipWidth, clipHeight, ulHeight, clipWidthToUse, clipHeightToUse );

	int clipLeft = GetAlignmentPosition( clipWidthToUse );
	int clipTop = lYPos + ( ulHeight - clipHeightToUse ) / 2;

	LONG lNewYPos = lYPos + ( ulHeight - pTexture->GetScaledHeight( )) / 2;

	// [AK] We must take into account the virtual screen's size.
	if ( g_bScale )
		screen->VirtualToRealCoordsInt( clipLeft, clipTop, clipWidthToUse, clipHeightToUse, con_virtualwidth, con_virtualheight, false, !con_scaletext_usescreenratio );

	screen->DrawTexture( pTexture, lXPos, lNewYPos,
		DTA_UseVirtualScreen, g_bScale,
		DTA_ClipLeft, clipLeft,
		DTA_ClipRight, clipLeft + clipWidthToUse,
		DTA_ClipTop, clipTop,
		DTA_ClipBottom, clipTop + clipHeightToUse,
		DTA_Alpha, FLOAT2FIXED( fAlpha ),
		TAG_DONE );
}

//*****************************************************************************
//
// [AK] ScoreColumn::CanDrawForPlayer
//
// Checks if a column can be drawn for a particular player.
//
//*****************************************************************************

bool ScoreColumn::CanDrawForPlayer( const ULONG ulPlayer ) const
{
	// [AK] Don't draw if the column's disabled, or the player's invalid.
	if (( bDisabled ) || ( PLAYER_IsValidPlayer( ulPlayer ) == false ))
		return false;

	// [AK] Don't draw for true spectators if they're meant to be excluded.
	if (( ulFlags & COLUMNFLAG_NOSPECTATORS ) && ( PLAYER_IsTrueSpectator( &players[ulPlayer] )))
		return false;

	return true;
}

//*****************************************************************************
//
// [AK] ScoreColumn::FixClipRectSize
//
// Takes an input width and height for a clipping rectangle and ensures that
// the "fixed" width and height aren't less than zero, or greater than the
// column's width and the height passed into this function respectively.
//
//*****************************************************************************

void ScoreColumn::FixClipRectSize( const int clipWidth, const int clipHeight, const ULONG ulHeight, int &fixedWidth, int &fixedHeight ) const
{
	if (( clipWidth <= 0 ) || ( static_cast<ULONG>( clipWidth ) > ulWidth ))
		fixedWidth = ulWidth;
	else
		fixedWidth = clipWidth;

	if (( clipHeight <= 0 ) || ( static_cast<ULONG>( clipHeight ) > ulHeight ))
		fixedHeight = ulHeight;
	else
		fixedHeight = clipHeight;
}

//*****************************************************************************
//
// [AK] DataScoreColumn::GetTemplate
//
// Returns the template (i.e. either text-based or graphic-based) based on the
// data type of the column. If the data type's unknown for any reason, then
// the template is also unknown.
//
//*****************************************************************************

COLUMNTEMPLATE_e DataScoreColumn::GetTemplate( void ) const
{
	switch ( GetDataType( ))
	{
		case COLUMNDATA_INT:
		case COLUMNDATA_BOOL:
		case COLUMNDATA_FLOAT:
		case COLUMNDATA_STRING:
			return COLUMNTEMPLATE_TEXT;

		case COLUMNDATA_COLOR:
		case COLUMNDATA_TEXTURE:
			return COLUMNTEMPLATE_GRAPHIC;

		default:
			return COLUMNTEMPLATE_UNKNOWN;
	}
}

//*****************************************************************************
//
// [AK] DataScoreColumn::GetDataType
//
// Returns the column's data type based on its native type. For example, frags
// are stored as integers, so the frags column would use the integer data type.
//
//*****************************************************************************

COLUMNDATA_e DataScoreColumn::GetDataType( void ) const
{
	switch ( NativeType )
	{
		case COLUMNTYPE_INDEX:
		case COLUMNTYPE_TIME:
		case COLUMNTYPE_PING:
		case COLUMNTYPE_FRAGS:
		case COLUMNTYPE_POINTS:
		case COLUMNTYPE_WINS:
		case COLUMNTYPE_KILLS:
		case COLUMNTYPE_DEATHS:
		case COLUMNTYPE_SECRETS:
		case COLUMNTYPE_LIVES:
		case COLUMNTYPE_DAMAGE:
		case COLUMNTYPE_HANDICAP:
		case COLUMNTYPE_JOINQUEUE:
			return COLUMNDATA_INT;

		case COLUMNTYPE_NAME:
		case COLUMNTYPE_VOTE:
			return COLUMNDATA_STRING;

		case COLUMNTYPE_PLAYERCOLOR:
			return COLUMNDATA_COLOR;

		case COLUMNTYPE_STATUSICON:
		case COLUMNTYPE_READYTOGOICON:
		case COLUMNTYPE_SCOREICON:
		case COLUMNTYPE_ARTIFACTICON:
		case COLUMNTYPE_BOTSKILLICON:
			return COLUMNDATA_TEXTURE;

		default:
			return COLUMNDATA_UNKNOWN;
	}
}

//*****************************************************************************
//
// [AK] DataScoreColumn::GetValueString
//
// This is intended to be used only with text-based column data types like
// integers, boolean, floats, and strings. It takes the input value and formats
// it into a string, while respecting the column's maximum length. Read further
// down to know how some data types are affected by this limit.
//
// Afterwards, it includes the prefix and suffix text, if applicable.
//
//*****************************************************************************

FString DataScoreColumn::GetValueString( const ColumnValue &Value ) const
{
	FString text;

	if ( Value.GetDataType( ) == COLUMNDATA_INT )
	{
		// [AK] A column's maximum length doesn't apply to integers.
		text.Format( "%d", Value.GetValue<int>( ));
	}
	else if ( Value.GetDataType( ) == COLUMNDATA_FLOAT )
	{
		// [AK] If the maximum length of a column is non-zero, then the floating point
		// number is rounded to the same number of decimals. Otherwise, the number is
		// left as it is. For example, if only two decimals are allowed, then a value
		// like 3.14159 is rounded to 3.14.
		if ( ulMaxLength == 0 )
			text.Format( "%f", Value.GetValue<float>( ));
		else
			text.Format( "%.*f", ulMaxLength, Value.GetValue<float>( ));
	}
	else if (( Value.GetDataType( ) == COLUMNDATA_BOOL ) || ( Value.GetDataType( ) == COLUMNDATA_STRING ))
	{
		// [AK] If the data type is boolean, use the column's true or false text instead.
		if ( Value.GetDataType( ) == COLUMNDATA_BOOL )
		{
			text = Value.GetValue<bool>( ) ? TrueText : FalseText;

			// [AK] If the true or false text are empty, then use "true" or "false" instead.
			if ( text.IsEmpty( ))
				text = Value.GetValue<bool>( ) ? "True" : "False";
		}
		else
		{
			text = Value.GetValue<const char *>( );
		}

		// [AK] If the number of characters in the passed string exceed the maximum
		// length that's allowed by the column, then the string is truncated to the
		// same length. Any trailing crap is also removed and an ellipsis is added.
		if (( ulMaxLength > 0 ) && ( text.Len( ) > ulMaxLength ))
		{
			text.Truncate( ulMaxLength );
			V_RemoveTrailingCrapFromFString( text );
			text += "...";
		}
	}

	if ( PrefixText.Len( ) > 0 )
		text.Insert( 0, PrefixText.GetChars( ));

	if ( SuffixText.Len( ) > 0 )
		text += SuffixText;

	return text;
}

//*****************************************************************************
//
// [AK] DataScoreColumn::GetValueWidth
//
// Gets the width of a value inside a ColumnValue object.
//
//*****************************************************************************

ULONG DataScoreColumn::GetValueWidth( const ColumnValue &Value, FFont *pFont ) const
{
	switch ( Value.GetDataType( ))
	{
		case COLUMNDATA_INT:
		case COLUMNDATA_BOOL:
		case COLUMNDATA_FLOAT:
		case COLUMNDATA_STRING:
		{
			if ( pFont == NULL )
				return 0;

			return pFont->StringWidth( GetValueString( Value ).GetChars( ));
		}

		case COLUMNDATA_COLOR:
		{
			// [AK] If this column must always use the shortest possible width, then return the
			// clipping rectangle's width, whether it's zero or not.
			if ( ulFlags & COLUMNFLAG_ALWAYSUSESHORTESTWIDTH )
				return ulClipRectWidth;

			// [AK] If the clipping rectangle's width is non-zero, return whichever is smaller:
			// the column's size (the default width here) or the clipping rectangle's width.
			return ulClipRectWidth > 0 ? MIN( ulSizing, ulClipRectWidth ) : ulSizing;
		}

		case COLUMNDATA_TEXTURE:
		{
			FTexture *pTexture = Value.GetValue<FTexture *>( );

			if ( pTexture == NULL )
				return 0;

			const ULONG ulTextureWidth = pTexture->GetScaledWidth( );
			return ulClipRectWidth > 0 ? MIN( ulTextureWidth, ulClipRectWidth ) : ulTextureWidth;
		}
	}

	return 0;
}

//*****************************************************************************
//
// [AK] DataScoreColumn::GetValue
//
// Returns a ColumnValue object containing the value associated with a player.
//
//*****************************************************************************

ColumnValue DataScoreColumn::GetValue( const ULONG ulPlayer ) const
{
	// [AK] By default, a ColumnValue object's data type is initialized to COLUMNDATA_UNKNOWN.
	// If the result's data type is still unknown in the end, then no value was retrieved.
	ColumnValue Result;

	if ( PLAYER_IsValidPlayer( ulPlayer ))
	{
		switch ( NativeType )
		{
			case COLUMNTYPE_NAME:
				Result = players[ulPlayer].userinfo.GetName( );
				break;

			case COLUMNTYPE_INDEX:
				Result = ulPlayer;
				break;

			case COLUMNTYPE_TIME:
				Result = players[ulPlayer].ulTime / ( TICRATE * 60 );
				break;

			case COLUMNTYPE_PING:
				if ( players[ulPlayer].bIsBot )
					Result = "BOT";
				else
					Result = players[ulPlayer].ulPing;
				break;

			case COLUMNTYPE_FRAGS:
				Result = players[ulPlayer].fragcount;
				break;

			case COLUMNTYPE_POINTS:
			case COLUMNTYPE_DAMAGE:
				Result = players[ulPlayer].lPointCount;
				break;

			case COLUMNTYPE_WINS:
				Result = players[ulPlayer].ulWins;
				break;

			case COLUMNTYPE_KILLS:
				Result = players[ulPlayer].killcount;
				break;

			case COLUMNTYPE_DEATHS:
				Result = players[ulPlayer].ulDeathCount;
				break;

			case COLUMNTYPE_SECRETS:
				Result = players[ulPlayer].secretcount;
				break;

			case COLUMNTYPE_LIVES:
				Result = players[ulPlayer].ulLivesLeft + 1;
				break;

			case COLUMNTYPE_HANDICAP:
			{
				int handicap = players[ulPlayer].userinfo.GetHandicap( );

				// [AK] Only show a player's handicap if it's greater than zero.
				if ( handicap > 0 )
				{
					if (( lastmanstanding ) || ( teamlms ))
						Result = deh.MaxSoulsphere - handicap < 1 ? 1 : deh.MaxArmor - handicap;
					else
						Result = deh.StartHealth - handicap < 1 ? 1 : deh.StartHealth - handicap;
				}

				break;
			}

			case COLUMNTYPE_JOINQUEUE:
			{
				int position = JOINQUEUE_GetPositionInLine( ulPlayer );

				// [AK] Only return the position if the player is in the join queue.
				if ( position != -1 )
					Result = position + 1;

				break;
			}

			case COLUMNTYPE_VOTE:
			{
				ULONG ulVoteChoice = CALLVOTE_GetPlayerVoteChoice( ulPlayer );

				// [AK] Check if this player either voted yes or no.
				if ( ulVoteChoice != VOTE_UNDECIDED )
					Result = ulVoteChoice == VOTE_YES ? "Yes" : "No";

				break;
			}

			case COLUMNTYPE_PLAYERCOLOR:
			{
				float h, s, v, r, g, b;
				D_GetPlayerColor( ulPlayer, &h, &s, &v, NULL );
				HSVtoRGB( &r, &g, &b, h, s, v );

				Result = PalEntry( clamp( static_cast<int>( r * 255.f ), 0, 255 ), clamp( static_cast<int>( g * 255.f ), 0, 255 ), clamp( static_cast<int>( b * 255.f ), 0, 255 ));
				break;
			}

			case COLUMNTYPE_STATUSICON:
				if ( players[ulPlayer].bLagging )
					Result = TexMan.FindTexture( "LAGMINI" );
				else if ( players[ulPlayer].bChatting )
					Result = TexMan.FindTexture( "TLKMINI" );
				else if ( players[ulPlayer].bInConsole )
					Result = TexMan.FindTexture( "CONSMINI" );
				else if ( players[ulPlayer].bInMenu )
					Result = TexMan.FindTexture( "MENUMINI" );
				break;

			case COLUMNTYPE_READYTOGOICON:
				if ( players[ulPlayer].bReadyToGoOn )
					Result = TexMan.FindTexture( "RDYTOGO" );
				break;

			case COLUMNTYPE_SCOREICON:
				if ( players[ulPlayer].mo != NULL )
					Result = TexMan[players[ulPlayer].mo->ScoreIcon];
				break;

			case COLUMNTYPE_ARTIFACTICON:
			{
				player_t *pCarrier = NULL;

				// [AK] In one-flag CTF, terminator, or (team) possession, check if this player is
				// carrying the white flag, terminator sphere, or hellstone respectively.
				if (( oneflagctf ) || ( terminator ) || ( possession ) || ( teampossession ))
				{
					pCarrier = GAMEMODE_GetArtifactCarrier( );

					if (( pCarrier != NULL ) && ( pCarrier - players == ulPlayer ))
					{
						if ( oneflagctf )
							Result = TexMan.FindTexture( "STFLA3" );
						else if ( terminator )
							Result = TexMan.FindTexture( "TERMINAT" );
						else
							Result = TexMan.FindTexture( "HELLSTON" );
					}
				}
				// [AK] In CTF or skulltag, check if this player is carrying an enemy team's item.
				else if (( ctf ) || ( skulltag ))
				{
					for ( ULONG ulTeam = 0; ulTeam < teams.Size( ); ulTeam++ )
					{
						pCarrier = TEAM_GetCarrier( ulTeam );

						if (( pCarrier != NULL ) && ( pCarrier - players == ulPlayer ))
						{
							Result = TexMan.FindTexture( TEAM_GetSmallHUDIcon( ulTeam ));
							break;
						}
					}
				}

				break;
			}

			case COLUMNTYPE_BOTSKILLICON:
			{
				if ( players[ulPlayer].bIsBot )
				{
					FString IconName;
					IconName.Format( "BOTSKIL%d", botskill.GetGenericRep( CVAR_Int ).Int );

					Result = TexMan.FindTexture( IconName.GetChars( ));
				}

				break;
			}
		}
	}

	return Result;
}

//*****************************************************************************
//
// [AK] DataScoreColumn::ParseCommand
//
// Parses commands that are only used for data columns.
//
//*****************************************************************************

void DataScoreColumn::ParseCommand( const FName Name, FScanner &sc, const COLUMNCMD_e Command, const FString CommandName )
{
	switch ( Command )
	{
		case COLUMNCMD_MAXLENGTH:
		case COLUMNCMD_PREFIX:
		case COLUMNCMD_SUFFIX:
		{
			// [AK] These commands are only available for text-based columns.
			if ( GetTemplate( ) != COLUMNTEMPLATE_TEXT )
				sc.ScriptError( "Option '%s' is only available for text-based columns.", CommandName.GetChars( ));

			if ( Command == COLUMNCMD_MAXLENGTH )
			{
				// [AK] Since maximum length doesn't apply to integer columns, we should
				// make sure that this command cannot be used for them.
				if ( GetDataType( ) == COLUMNDATA_INT )
					sc.ScriptError( "Option '%s' cannot be used with integer columns.", CommandName.GetChars( ));

				sc.MustGetNumber( );
				ulMaxLength = MAX( sc.Number, 0 );
			}
			else
			{
				sc.MustGetString( );

				if ( Command == COLUMNCMD_PREFIX )
					PrefixText = sc.String;
				else
					SuffixText = sc.String;
			}

			break;
		}

		case COLUMNCMD_CLIPRECTWIDTH:
		case COLUMNCMD_CLIPRECTHEIGHT:
		{
			// [AK] These commands are only available for graphic-based columns.
			if ( GetTemplate( ) != COLUMNTEMPLATE_GRAPHIC )
				sc.ScriptError( "Option '%s' is only available for graphic-based columns.", CommandName.GetChars( ));

			sc.MustGetNumber( );

			if ( Command == COLUMNCMD_CLIPRECTWIDTH )
				ulClipRectWidth = MAX( sc.Number, 0 );
			else
				ulClipRectHeight = MAX( sc.Number, 0 );

			break;
		}

		case COLUMNCMD_TRUETEXT:
		case COLUMNCMD_FALSETEXT:
		{
			// [AK] True and false text are only available for boolean columns.
			if ( GetDataType( ) != COLUMNDATA_BOOL )
				sc.ScriptError( "Option '%s' is only available for boolean columns.", CommandName.GetChars( ));

			sc.MustGetString( );

			// [AK] If the name begins with a '$', look up the string in the LANGUAGE lump.
			const char *pszString = sc.String[0] == '$' ? GStrings[sc.String] : sc.String;

			if ( Command == COLUMNCMD_TRUETEXT )
				TrueText = pszString;
			else
				FalseText = pszString;

			break;
		}

		// [AK] Parse any generic column commands if we reach here.
		default:
			ScoreColumn::ParseCommand( Name, sc, Command, CommandName );
			break;
	}
}

//*****************************************************************************
//
// [AK] DataScoreColumn::CheckIfUsable
//
// Checks if the data column is usable in the current game. Refer to
// ScoreColumn::CheckIfUsable for more information.
//
//*****************************************************************************

void DataScoreColumn::CheckIfUsable( )
{
	// [AK] If this column isn't part of a scoreboard, and it's not inside a composite
	// column that's part of the scoreboard either, then stop here.
	if (( IsInsideScoreboard( ) == false ) && (( pCompositeColumn == NULL ) || ( pCompositeColumn->IsInsideScoreboard( ) == false )))
		return;

	ScoreColumn::CheckIfUsable( );
}

//*****************************************************************************
//
// [AK] DataScoreColumn::UpdateWidth
//
// Gets the smallest width that will fit the contents in all player rows.
//
//*****************************************************************************

void DataScoreColumn::UpdateWidth( FFont *pHeaderFont, FFont *pRowFont )
{
	ulShortestWidth = 0;

	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( CanDrawForPlayer( ulIdx ) == false )
			continue;

		ColumnValue Value = GetValue( ulIdx );
		ulShortestWidth = MAX( ulShortestWidth, GetValueWidth( Value, pRowFont ));
	}

	// [AK] Call the superclass's function to finish updating the width.
	ScoreColumn::UpdateWidth( pHeaderFont, pRowFont );
}

//*****************************************************************************
//
// [AK] DataScoreColumn::DrawValue
//
// Draws the value of a particular player passed into a ColumnValue object.
//
//*****************************************************************************

void DataScoreColumn::DrawValue( const ULONG ulPlayer, FFont *pFont, const ULONG ulColor, const LONG lYPos, const ULONG ulHeight, const float fAlpha ) const
{
	if ( CanDrawForPlayer( ulPlayer ) == false )
		return;

	ColumnValue Value = GetValue( ulPlayer );
	ULONG ulColorToUse;

	// [AK] The text color used in the join queue and vote columns changes depending
	// on whether the player is first in line or the vote caller respectively. Also,
	// the text color in the ping column changes depending on cl_colorizepings.
	if (( NativeType == COLUMNTYPE_PING ) && ( cl_colorizepings ) && ( players[ulPlayer].bIsBot == false ))
	{
		if ( players[ulPlayer].ulPing >= 200 )
			ulColorToUse = CR_RED;
		else if ( players[ulPlayer].ulPing >= 150 )
			ulColorToUse = CR_ORANGE;
		else if ( players[ulPlayer].ulPing >= 100 )
			ulColorToUse = CR_GOLD;
		else
			ulColorToUse = CR_GREEN;
	}
	else if ( NativeType == COLUMNTYPE_JOINQUEUE )
	{
		if ( JOINQUEUE_GetPositionInLine( ulPlayer ) == 0 )
			ulColorToUse = CR_RED;
		else
			ulColorToUse = CR_GOLD;
	}
	else if ( NativeType == COLUMNTYPE_VOTE )
	{
		if ( CALLVOTE_GetVoteCaller( ) == ulPlayer )
			ulColorToUse = CR_RED;
		else
			ulColorToUse = CR_GOLD;
	}
	else
	{
		ulColorToUse = ulColor;
	}

	switch( Value.GetDataType( ))
	{
		case COLUMNDATA_INT:
		case COLUMNDATA_BOOL:
		case COLUMNDATA_FLOAT:
		case COLUMNDATA_STRING:
			DrawString( GetValueString( Value ).GetChars( ), pFont, ulColorToUse, lYPos, ulHeight, fAlpha );
			break;

		case COLUMNDATA_COLOR:
			DrawColor( Value.GetValue<PalEntry>( ), lYPos, ulHeight, fAlpha, ulClipRectWidth, ulClipRectHeight );
			break;

		case COLUMNDATA_TEXTURE:
			DrawTexture( Value.GetValue<FTexture *>( ), lYPos, ulHeight, fAlpha, ulClipRectWidth, ulClipRectHeight );
			break;
	}
}

//*****************************************************************************
//
// [AK] CompositeScoreColumn::ParseCommand
//
// Parses commands that are only used for composite columns.
//
//*****************************************************************************

void CompositeScoreColumn::ParseCommand( const FName Name, FScanner &sc, const COLUMNCMD_e Command, const FString CommandName )
{
	switch ( Command )
	{
		case COLUMNCMD_COLUMNS:
		{
			SubColumns.Clear( );

			do
			{
				// [AK] Make sure that the next column we scan is a data column.
				DataScoreColumn *pDataColumn = static_cast<DataScoreColumn *>( scoreboard_ScanForColumn( sc, true ));

				// [AK] Don't add a data column that's already inside a scoreboard's column order.
				if ( pDataColumn->IsInsideScoreboard( ))
					sc.ScriptError( "Tried to put data column '%s' into composite column '%s', but it's already inside a scoreboard's column order.", sc.String, Name.GetChars( ) );

				// [AK] Don't add a data column that's already inside another composite column.
				if (( pDataColumn->pCompositeColumn != NULL ) && ( pDataColumn->pCompositeColumn != this ))
					sc.ScriptError( "Tried to put data column '%s' into composite column '%s', but it's already inside another composite column.", sc.String, Name.GetChars( ));

				if ( scoreboard_TryPushingColumnToList( sc, SubColumns, pDataColumn, sc.String ))
					pDataColumn->pCompositeColumn = this;
			} while ( sc.CheckToken( ',' ));

			break;
		}

		// [AK] Parse any generic column commands if we reach here.
		default:
			ScoreColumn::ParseCommand( Name, sc, Command, CommandName );
			break;
	}
}

//*****************************************************************************
//
// [AK] CompositeScoreColumn::CheckIfUsable
//
// Checks if the composite column and its sub-columns are usable in the current
// game. Refer to ScoreColumn::CheckIfUsable for more information.
//
//*****************************************************************************

void CompositeScoreColumn::CheckIfUsable( void )
{
	// [AK] If the composite column isn't part of a scoreboard, then stop here.
	if ( IsInsideScoreboard( ) == false )
		return;

	// [AK] Call the superclass's function first.
	ScoreColumn::CheckIfUsable( );

	// [AK] If the composite column is usable, then check the sub-columns too.
	if ( bUsableInCurrentGame )
	{
		for ( unsigned int i = 0; i < SubColumns.Size( ); i++ )
			SubColumns[i]->CheckIfUsable( );
	}
}

//*****************************************************************************
//
// [AK] CompositeScoreColumn::Refresh
//
// Refreshes the composite column and its sub-columns.
//
//*****************************************************************************

void CompositeScoreColumn::Refresh( void )
{
	// [AK] Call the superclass's refresh function first.
	ScoreColumn::Refresh( );

	// [AK] If the composite column isn't disabled, then refresh the sub-columns.
	if ( bDisabled == false )
	{
		for ( unsigned int i = 0; i < SubColumns.Size( ); i++ )
		{
			SubColumns[i]->Refresh( );

			// [AK] Sub-columns cannot show their headers and need to be aligned to the left.
			if ( SubColumns[i]->IsDisabled( ) == false )
			{
				if (( SubColumns[i]->GetFlags( ) & COLUMNFLAG_DONTSHOWHEADER ) == false )
					SubColumns[i]->ulFlags |= COLUMNFLAG_DONTSHOWHEADER;

				if ( SubColumns[i]->Alignment != COLUMNALIGN_LEFT )
					SubColumns[i]->Alignment = COLUMNALIGN_LEFT;
			}
		}
	}
}

//*****************************************************************************
//
// [AK] CompositeScoreColumn::UpdateWidth
//
// Gets the smallest width that can fit the contents of all active sub-columns
// in all player rows.
//
//*****************************************************************************

void CompositeScoreColumn::UpdateWidth( FFont *pHeaderFont, FFont *pRowFont )
{
	ulShortestWidth = 0;

	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( CanDrawForPlayer( ulIdx ) == false )
			continue;

		ulShortestWidth = MAX( ulShortestWidth, GetRowWidth( ulIdx, pRowFont ));
	}

	// [AK] Call the superclass's function to finish updating the width.
	ScoreColumn::UpdateWidth( pHeaderFont, pRowFont );
}

//*****************************************************************************
//
// [AK] CompositeScoreColumn::DrawValue
//
// Draws the values of a particular player from all active sub-columns.
//
//*****************************************************************************

void CompositeScoreColumn::DrawValue( const ULONG ulPlayer, FFont *pFont, const ULONG ulColor, const LONG lYPos, const ULONG ulHeight, const float fAlpha ) const
{
	ColumnValue Value;

	if ( CanDrawForPlayer( ulPlayer ) == false )
		return;

	const bool bIsTrueSpectator = PLAYER_IsTrueSpectator( &players[ulPlayer] );
	const ULONG ulRowWidth = GetRowWidth( ulPlayer, pFont );

	// [AK] If this row's width is zero, then there's nothing to draw, so stop here.
	if ( ulRowWidth == 0 )
		return;

	// [AK] Determine at what position we should start drawing the contents.
	LONG lXPos = GetAlignmentPosition( ulRowWidth );

	// [AK] Draw the contents of the sub-columns!
	for ( unsigned int i = 0; i < SubColumns.Size( ); i++ )
	{
		if (( SubColumns[i]->IsDisabled( )) || (( SubColumns[i]->GetFlags( ) & COLUMNFLAG_NOSPECTATORS ) && ( bIsTrueSpectator )))
			continue;

		Value = SubColumns[i]->GetValue( ulPlayer );

		if ( Value.GetDataType( ) != COLUMNDATA_UNKNOWN )
		{
			const ULONG ulValueWidth = SubColumns[i]->GetValueWidth( Value, pFont );

			// [AK] We didn't update the sub-column's x-position or width since they're part of
			// a composite column, but we need to make sure that the contents appear properly.
			// (i.e. Scoreboard::DrawString and Scoreboard::DrawTexture use these members to
			// form the clipping rectangle's boundaries). What we'll do is temporarily set the
			// members to what they need to be now, draw the value, then set them back to zero.
			SubColumns[i]->lRelX = lXPos;
			SubColumns[i]->ulWidth = ulValueWidth;
			SubColumns[i]->DrawValue( ulPlayer, pFont, ulColor, lYPos, ulHeight, fAlpha );

			lXPos += GetSubColumnWidth( i, ulValueWidth );
			SubColumns[i]->lRelX = SubColumns[i]->ulWidth = 0;
		}
	}
}

//*****************************************************************************
//
// [AK] CompositeScoreColumn::GetRowWidth
//
// Gets the width of an entire row for a particular player.
//
//*****************************************************************************

ULONG CompositeScoreColumn::GetRowWidth( const ULONG ulPlayer, FFont *pFont ) const
{
	if ( PLAYER_IsValidPlayer( ulPlayer ) == false )
		return 0;

	const bool bIsTrueSpectator = PLAYER_IsTrueSpectator( &players[ulPlayer] );
	ULONG ulRowWidth = 0;

	for ( unsigned int i = 0; i < SubColumns.Size( ); i++ )
	{
		// [AK] Ignore sub-columns that are disabled or cannot be shown for true spectators.
		if (( SubColumns[i]->IsDisabled( )) || (( SubColumns[i]->GetFlags( ) & COLUMNFLAG_NOSPECTATORS ) && ( bIsTrueSpectator )))
			continue;

		ColumnValue Value = SubColumns[i]->GetValue( ulPlayer );

		if ( Value.GetDataType( ) != COLUMNDATA_UNKNOWN )
			ulRowWidth += GetSubColumnWidth( i, SubColumns[i]->GetValueWidth( Value, pFont ));
	}

	return ulRowWidth;
}

//*****************************************************************************
//
// [AK] CompositeScoreColumn::GetSubColumnWidth
//
// Gets the width of a sub-column. This requires that a ColumnValue's width is
// determined first (using DataScoreColumn::GetValueWidth) and passed into this
// function to work.
//
//*****************************************************************************

ULONG CompositeScoreColumn::GetSubColumnWidth( const ULONG ulSubColumn, const ULONG ulValueWidth ) const
{
	if ( ulSubColumn >= SubColumns.Size( ))
		return 0;

	// [AK] If the sub-column always uses its shortest width, then sizing is treated as padding.
	if ( SubColumns[ulSubColumn]->GetFlags( ) & COLUMNFLAG_ALWAYSUSESHORTESTWIDTH )
		return ulValueWidth + SubColumns[ulSubColumn]->GetSizing( );
	// [AK] Otherwise, the sizing is treated as the default width of the sub-column.
	// Use whichever's bigger: the value's width or the sizing.
	else
		return MAX( SubColumns[ulSubColumn]->GetSizing( ), ulValueWidth );
}

//*****************************************************************************
//
// [AK] Scoreboard::Scoreboard
//
// Initializes the members of a Scoreboard object to their default values.
//
//*****************************************************************************

Scoreboard::Scoreboard( void ) :
	lRelX( 0 ),
	lRelY( 0 ),
	ulWidth( 0 ),
	ulHeight( 0 ),
	ulFlags( 0 ),
	pHeaderFont( NULL ),
	pRowFont( NULL ),
	HeaderColor( CR_UNTRANSLATED ),
	RowColor( CR_UNTRANSLATED ),
	LocalRowColors{ CR_UNTRANSLATED },
	pBorderTexture( NULL ),
	BorderColors{ CR_UNTRANSLATED },
	BackgroundColor( 0 ),
	RowBackgroundColors{ 0 },
	TeamRowBackgroundColors{ 0 },
	fBackgroundAmount( 0.0f ),
	fRowBackgroundAmount( 0.0f ),
	fDeadRowBackgroundAmount( 0.0f ),
	fDeadTextAlpha( 0.0f ),
	ulBackgroundBorderSize( 0 ),
	ulGapBetweenHeaderAndRows( 0 ),
	ulGapBetweenColumns( 0 ),
	ulGapBetweenRows( 0 ),
	lHeaderHeight( 0 ),
	lRowHeight( 0 ),
	bDisabled( false ),
	bHidden( false ) { }

//*****************************************************************************
//
// [AK] Scoreboard::Parse
//
// Parses a "scoreboard" block in a SCORINFO lump.
//
//*****************************************************************************

int scoreboard_GetLuminance( const int r, const int g, const int b )
{
	return static_cast<int>( 0.3f * r + 0.59f * g + 0.11f * b );
}

//*****************************************************************************
//
int scoreboard_GetLuminance( const PalEntry color )
{
	return scoreboard_GetLuminance( color.r, color.g, color.b );
}

//*****************************************************************************
//
void Scoreboard::Parse( FScanner &sc )
{
	sc.MustGetToken( '{' );

	while ( sc.CheckToken( '}' ) == false )
	{
		sc.MustGetString( );

		if ( stricmp( sc.String, "addflag" ) == 0 )
		{
			ulFlags |= sc.MustGetEnumName( "scoreboard flag", "SCOREBOARDFLAG_", GetValueSCOREBOARDFLAG_e );
		}
		else if ( stricmp( sc.String, "removeflag" ) == 0 )
		{
			ulFlags &= ~sc.MustGetEnumName( "scoreboard flag", "SCOREBOARDFLAG_", GetValueSCOREBOARDFLAG_e );
		}
		else
		{
			SCOREBOARDCMD_e Command = static_cast<SCOREBOARDCMD_e>( sc.MustGetEnumName( "scoreboard command", "SCOREBOARDCMD_", GetValueSCOREBOARDCMD_e, true ));
			FString CommandName = sc.String;

			sc.MustGetToken( '=' );

			switch ( Command )
			{
				case SCOREBOARDCMD_BORDERTEXTURE:
				{
					sc.MustGetString( );
					pBorderTexture = TexMan.FindTexture( sc.String );

					// [AK] If the texture wasn't found, throw a fatal error.
					if ( pBorderTexture == NULL )
						sc.ScriptError( "Couldn't find border texture '%s'.", sc.String );

					break;
				}

				case SCOREBOARDCMD_HEADERFONT:
				case SCOREBOARDCMD_ROWFONT:
				{
					sc.MustGetToken( TK_StringConst );

					// [AK] Throw a fatal error if an empty string was passed.
					if ( sc.StringLen == 0 )
						sc.ScriptError( "Got an empty string for a font name." );

					FFont *pFont = V_GetFont( sc.String );

					// [AK] If the font was invalid, throw a fatal error.
					if ( pFont == NULL )
						sc.ScriptError( "Couldn't find font '%s'.", sc.String );

					if ( Command == SCOREBOARDCMD_HEADERFONT )
						pHeaderFont = pFont;
					else
						pRowFont = pFont;

					break;
				}

				case SCOREBOARDCMD_HEADERCOLOR:
				case SCOREBOARDCMD_ROWCOLOR:
				case SCOREBOARDCMD_LOCALROWCOLOR:
				case SCOREBOARDCMD_LOCALROWDEMOCOLOR:
				{
					sc.MustGetToken( TK_StringConst );
					EColorRange color;

					// [AK] If an empty string was passed, inform the user of the error and switch to untranslated.
					if ( sc.StringLen == 0 )
					{
						sc.ScriptMessage( "Got an empty string for a text color, using untranslated instead." );
						color = CR_UNTRANSLATED;
					}
					else
					{
						color = V_FindFontColor( sc.String );

						// [AK] If the text color name was invalid, let the user know about it.
						if (( color == CR_UNTRANSLATED ) && ( stricmp( sc.String, "untranslated" ) != 0 ))
							sc.ScriptMessage( "'%s' is an unknown text color, using untranslated instead.", sc.String );
					}

					if ( Command == SCOREBOARDCMD_HEADERCOLOR )
						HeaderColor = color;
					else if ( Command == SCOREBOARDCMD_ROWCOLOR )
						RowColor = color;
					else if ( Command == SCOREBOARDCMD_LOCALROWCOLOR )
						LocalRowColors[LOCALROW_COLOR_INGAME] = color;
					else
						LocalRowColors[LOCALROW_COLOR_INDEMO] = color;

					break;
				}

				case SCOREBOARDCMD_DEADPLAYERTEXTALPHA:
				case SCOREBOARDCMD_BACKGROUNDAMOUNT:
				case SCOREBOARDCMD_ROWBACKGROUNDAMOUNT:
				case SCOREBOARDCMD_DEADPLAYERROWBACKGROUNDAMOUNT:
				{
					sc.MustGetFloat( );
					const float fClampedValue = clamp( static_cast<float>( sc.Float ), 0.0f, 1.0f );

					if ( Command == SCOREBOARDCMD_DEADPLAYERTEXTALPHA )
						fDeadTextAlpha = fClampedValue;
					else if ( Command == SCOREBOARDCMD_BACKGROUNDAMOUNT )
						fBackgroundAmount = fClampedValue;
					else if ( Command == SCOREBOARDCMD_ROWBACKGROUNDAMOUNT )
						fRowBackgroundAmount = fClampedValue;
					else
						fDeadRowBackgroundAmount = fClampedValue;

					break;
				}

				case SCOREBOARDCMD_LIGHTBORDERCOLOR:
				case SCOREBOARDCMD_DARKBORDERCOLOR:
				case SCOREBOARDCMD_BACKGROUNDCOLOR:
				case SCOREBOARDCMD_LIGHTROWBACKGROUNDCOLOR:
				case SCOREBOARDCMD_DARKROWBACKGROUNDCOLOR:
				case SCOREBOARDCMD_LOCALROWBACKGROUNDCOLOR:
				{
					sc.MustGetToken( TK_StringConst );

					// [AK] If an empty string was passed, inform the user about the error.
					// This doesn't have to be a fatal error since the color will just be black anyways.
					if ( sc.StringLen == 0 )
						sc.ScriptMessage( "Got an empty string for a color." );

					FString ColorString = V_GetColorStringByName( sc.String );
					PalEntry color = V_GetColorFromString( NULL, ColorString.IsNotEmpty( ) ? ColorString.GetChars( ) : sc.String );

					if ( Command == SCOREBOARDCMD_LIGHTBORDERCOLOR )
						BorderColors[BORDER_COLOR_LIGHT] = color;
					else if ( Command == SCOREBOARDCMD_DARKBORDERCOLOR )
						BorderColors[BORDER_COLOR_DARK] = color;
					else if ( Command == SCOREBOARDCMD_BACKGROUNDCOLOR )
						BackgroundColor = color;
					else if ( Command == SCOREBOARDCMD_LIGHTROWBACKGROUNDCOLOR )
						RowBackgroundColors[ROWBACKGROUND_COLOR_LIGHT] = color;
					else if ( Command == SCOREBOARDCMD_DARKROWBACKGROUNDCOLOR )
						RowBackgroundColors[ROWBACKGROUND_COLOR_DARK] = color;
					else
						RowBackgroundColors[ROWBACKGROUND_COLOR_LOCAL] = color;

					break;
				}

				case SCOREBOARDCMD_BACKGROUNDBORDERSIZE:
				case SCOREBOARDCMD_GAPBETWEENHEADERANDROWS:
				case SCOREBOARDCMD_GAPBETWEENCOLUMNS:
				case SCOREBOARDCMD_GAPBETWEENROWS:
				{
					sc.MustGetNumber( );
					const ULONG ulCappedValue = MAX( sc.Number, 0 );

					if ( Command == SCOREBOARDCMD_BACKGROUNDBORDERSIZE )
						ulBackgroundBorderSize = ulCappedValue;
					else if ( Command == SCOREBOARDCMD_GAPBETWEENHEADERANDROWS )
						ulGapBetweenHeaderAndRows = ulCappedValue;
					else if ( Command == SCOREBOARDCMD_GAPBETWEENCOLUMNS )
						ulGapBetweenColumns = ulCappedValue;
					else
						ulGapBetweenRows = ulCappedValue;

					break;
				}

				case SCOREBOARDCMD_HEADERHEIGHT:
				case SCOREBOARDCMD_ROWHEIGHT:
				{
					sc.MustGetNumber( );

					if ( Command == SCOREBOARDCMD_HEADERHEIGHT )
						lHeaderHeight = sc.Number;
					else
						lRowHeight = sc.Number;

					break;
				}

				case SCOREBOARDCMD_COLUMNORDER:
				case SCOREBOARDCMD_RANKORDER:
				{
					// [AK] Clear the list before adding the new columns to it.
					if ( Command == SCOREBOARDCMD_COLUMNORDER )
						ColumnOrder.Clear( );
					else
						RankOrder.Clear( );

					do
					{
						AddColumnToList( sc, Command == SCOREBOARDCMD_RANKORDER );
					} while ( sc.CheckToken( ',' ));

					break;
				}

				case SCOREBOARDCMD_ADDTOCOLUMNORDER:
				case SCOREBOARDCMD_ADDTORANKORDER:
				{
					AddColumnToList( sc, Command == SCOREBOARDCMD_ADDTORANKORDER );
					break;
				}

				default:
					sc.ScriptError( "Couldn't process scoreboard command '%s'.", CommandName.GetChars( ));
			}
		}
	}

	if ( pHeaderFont == NULL )
		sc.ScriptError( "There's no header font for the scoreboard." );

	if ( pRowFont == NULL )
		sc.ScriptError( "There's no row font for the scoreboard." );

	// [AK] A negative header or row height means setting the height with respect to the
	// height of the header or row font's respectively, if valid.
	if ( lHeaderHeight <= 0 )
		lHeaderHeight = pHeaderFont->GetHeight( ) - lHeaderHeight;

	if ( lRowHeight <= 0 )
		lRowHeight = pRowFont->GetHeight( ) - lRowHeight;

	// [AK] Generate row background colors for each team through color blending.
	// This uses the color blend mode explained in section 7.2.4, "Blend Mode", in
	// "PDF Reference" fifth edition, version 1.6.
	for ( ULONG ulTeam = 0; ulTeam < teams.Size( ); ulTeam++ )
	{
		const PalEntry TeamColor = teams[ulTeam].lPlayerColor;

		for ( unsigned int i = 0; i < NUM_ROWBACKGROUND_COLORS; i++ )
		{
			const int delta = scoreboard_GetLuminance( RowBackgroundColors[i] ) - scoreboard_GetLuminance( TeamColor );

			int rgb[3] = { TeamColor.r + delta, TeamColor.g + delta, TeamColor.b + delta };

			const int luminosity = scoreboard_GetLuminance( rgb[0], rgb[1], rgb[2] );
			const int minColor = MIN( MIN( rgb[0], rgb[1] ), rgb[2] );
			const int maxColor = MAX( MAX( rgb[0], rgb[1] ), rgb[2] );

			if ( minColor < 0 )
			{
				for ( unsigned int i = 0; i < 3; i++ )
					rgb[i] = luminosity + ((( rgb[i] - luminosity ) * luminosity ) / ( luminosity - minColor ));
			}

			if ( maxColor > UCHAR_MAX )
			{
				for ( unsigned int i = 0; i < 3; i++ )
					rgb[i] = luminosity + ((( rgb[i] - luminosity ) * ( UCHAR_MAX - luminosity )) / ( maxColor - luminosity ));
			}

			TeamRowBackgroundColors[ulTeam][i] = MAKERGB( rgb[0], rgb[1], rgb[2] );
		}
	}
}

//*****************************************************************************
//
// [AK] Scoreboard::AddColumnToList
//
// This is intended to be called only when parsing scoreboard commands like
// "ColumnOrder", "RankOrder", "AddToColumnOrder", and "AddToRankOrder". It
// scans for a string, finds a column using that string, and adds it to the
// rank or column order lists (depending on the value of bAddToRankOrder).
//
//*****************************************************************************

void Scoreboard::AddColumnToList( FScanner &sc, const bool bAddToRankOrder )
{
	// [AK] Note that if we're adding a column to the rank order, then it must be a data column.
	ScoreColumn *pColumn = scoreboard_ScanForColumn( sc, bAddToRankOrder );

	// [AK] The column's name should still be saved in sc.String, even after calling
	// the helper function above.
	const char *pszColumnName = sc.String;

	if ( bAddToRankOrder )
	{
		// [AK] Double-check that this is a data column. Otherwise, throw a fatal error.
		if ( pColumn->IsDataColumn( ) == false )
			sc.ScriptError( "Column '%s' is not a data column.", pszColumnName );

		DataScoreColumn *pDataColumn = static_cast<DataScoreColumn *>( pColumn );

		// [AK] Columns must be inside the scoreboard's column order first before they're
		// added to the rank order list. If this column is inside a composite column, then
		// the composite column needs to be in the column order instead.
		if ( pColumn->pScoreboard != this )
		{
			if ( pDataColumn->pCompositeColumn == NULL )
				sc.ScriptError( "Column '%s' must be added to the column order before added to the rank order.", pszColumnName );
			else if ( pDataColumn->pCompositeColumn->pScoreboard != this )
				sc.ScriptError( "Column '%s' is part of a composite column that must be added to the column order before it can be added to the rank order.", pszColumnName );
		}

		scoreboard_TryPushingColumnToList( sc, RankOrder, pDataColumn, pszColumnName );
	}
	else
	{
		// [AK] If this is a data column, make sure that it isn't inside a composite column.
		// The composite column must be added to the list instead.
		if (( pColumn->IsDataColumn( )) && ( static_cast<DataScoreColumn *>( pColumn )->pCompositeColumn != NULL ))
			sc.ScriptError( "Column '%s' is part of a composite column and can't be added to the order list.", pszColumnName );

		if ( scoreboard_TryPushingColumnToList( sc, ColumnOrder, pColumn, pszColumnName ))
			pColumn->pScoreboard = this;
	}
}

//*****************************************************************************
//
// [AK] Scoreboard::PlayerComparator
//
// Orders players on the scoreboard, from top to bottom, using the rank order list.
//
//*****************************************************************************

bool Scoreboard::PlayerComparator::operator( )( const int &arg1, const int &arg2 ) const
{
	int result = 0;

	// [AK] Sanity check: make sure that we're pointing to a scoreboard.
	if ( pScoreboard == NULL )
		return false;

	// [AK] Always return false if the first player index is invalid,
	// or true if the second player index is invalid.
	if ( PLAYER_IsValidPlayer( arg1 ) == false )
		return false;
	else if ( PLAYER_IsValidPlayer( arg2 ) == false )
		return true;

	// [AK] Always return false if the first player is a true spectator,
	// or true if the second player is a true spectator.
	if ( PLAYER_IsTrueSpectator( &players[arg1] ))
		return false;
	else if ( PLAYER_IsTrueSpectator( &players[arg2] ))
		return true;

	// [AK] In team-based game modes, order players by team. Players with lower
	// team indices should come before those with higher indices.
	if ( pScoreboard->ShouldSeparateTeams( ))
	{
		result = players[arg1].Team - players[arg2].Team;

		if ( result != 0 )
			return ( result < 0 );
	}

	for ( unsigned int i = 0; i < pScoreboard->RankOrder.Size( ); i++ )
	{
		if ( pScoreboard->RankOrder[i]->IsDisabled( ))
			continue;

		const ColumnValue Value1 = pScoreboard->RankOrder[i]->GetValue( arg1 );
		const ColumnValue Value2 = pScoreboard->RankOrder[i]->GetValue( arg2 );

		// [AK] Always return false if the data type of the first value is unknown.
		// This is also the case when both values have unknown data types.
		if ( Value1.GetDataType( ) == COLUMNDATA_UNKNOWN )
			return false;

		// [AK] Always return true if the second value is unknown.
		if ( Value2.GetDataType( ) == COLUMNDATA_UNKNOWN )
			return true;

		switch ( Value1.GetDataType( ))
		{
			case COLUMNDATA_INT:
				result = Value1.GetValue<int>( ) - Value2.GetValue<int>( );
				break;

			case COLUMNDATA_BOOL:
				result = static_cast<int>( Value1.GetValue<bool>( ) - Value2.GetValue<bool>( ));
				break;

			case COLUMNDATA_FLOAT:
				result = static_cast<int>( Value1.GetValue<float>( ) - Value2.GetValue<float>( ));

			case COLUMNDATA_STRING:
			{
				FString firstString = Value1.GetValue<const char *>( );
				FString secondString = Value2.GetValue<const char *>( );

				// [AK] Remove color codes from both strings before comparing them.
				V_RemoveColorCodes( firstString );
				V_RemoveColorCodes( secondString );

				result = secondString.Compare( firstString );
				break;
			}

			default:
				break;
		}

		// [AK] If the values for this column aren't the same for both players, return the result.
		if ( result != 0 )
			return ( pScoreboard->RankOrder[i]->GetFlags( ) & COLUMNFLAG_REVERSEORDER ) ? ( result < 0 ) : ( result > 0 );
	}

	return false;
}

//*****************************************************************************
//
// [AK] Scoreboard::Refresh
//
// Updates the scoreboard's width and height, re-positions the columns, and
// sorts the players using the rank order.
//
//*****************************************************************************

void Scoreboard::Refresh( const ULONG ulDisplayPlayer )
{
	bDisabled = false;

	// [AK] If the scoreboard's supposed to be hidden, then disable it and stop here.
	if ( bHidden )
	{
		bDisabled = true;
		return;
	}

	// [AK] Refresh all of the scoreboard's columns, then update the widths of any active columns.
	for ( unsigned int i = 0; i < ColumnOrder.Size( ); i++ )
	{
		ColumnOrder[i]->Refresh( );

		if ( ColumnOrder[i]->IsDisabled( ))
			continue;

		ColumnOrder[i]->UpdateWidth( pHeaderFont, pRowFont );
	}

	UpdateWidth( );

	// [AK] If the scoreboard's width is zero, then disable it and stop here.
	if ( ulWidth == 0 )
	{
		bDisabled = true;
		return;
	}

	UpdateHeight( );

	// [AK] Reset the player list then sort players based on the scoreboard's rank order.
	for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		ulPlayerList[ulIdx] = ulIdx;

	std::stable_sort( ulPlayerList, ulPlayerList + MAXPLAYERS, PlayerComparator( this ));
}

//*****************************************************************************
//
// [AK] Scoreboard::UpdateWidth
//
// Determines what the width of the scoreboard should be right now, then
// re-positions all of the active columns based on that width.
//
//*****************************************************************************

void Scoreboard::UpdateWidth( void )
{
	const ULONG ulGameModeFlags = GAMEMODE_GetCurrentFlags( );
	ULONG ulNumActiveColumns = 0;

	ulWidth = 0;

	for ( unsigned int i = 0; i < ColumnOrder.Size( ); i++ )
	{
		if ( ColumnOrder[i]->IsDisabled( ))
			continue;

		ulWidth += ColumnOrder[i]->GetWidth( );
		ulNumActiveColumns++;
	}

	// [AK] If the width is still zero, then no columns are visible, stop here.
	if ( ulWidth == 0 )
		return;

	// [AK] Add the gaps between each of the active columns and the background border size to the total width.
	ulWidth += ( ulNumActiveColumns - 1 ) * ulGapBetweenColumns + 2 * ulBackgroundBorderSize;
	lRelX = ( HUD_GetWidth( ) - ulWidth ) / 2;

	LONG lCurXPos = lRelX + ulBackgroundBorderSize;

	// [AK] We got the width of the scoreboard. Now update the positions of all the columns.
	// Do this here because we already know how many columns are active in this function.
	for ( unsigned int i = 0; i < ColumnOrder.Size( ); i++ )
	{
		if ( ColumnOrder[i]->IsDisabled( ))
			continue;

		ColumnOrder[i]->lRelX = lCurXPos;
		lCurXPos += ColumnOrder[i]->GetWidth( );

		if ( --ulNumActiveColumns > 0 )
			lCurXPos += ulGapBetweenColumns;
	}
}

//*****************************************************************************
//
// [AK] Scoreboard::UpdateHeight
//
// Determines what the height of the scoreboard should be right now.
//
//*****************************************************************************

void Scoreboard::UpdateHeight( void )
{
	const ULONG ulRowYOffset = lRowHeight + ulGapBetweenRows;
	const ULONG ulNumActivePlayers = HUD_GetNumPlayers( );
	const ULONG ulNumSpectators = HUD_GetNumSpectators( );

	ulHeight = 2 * ulBackgroundBorderSize + lHeaderHeight + ulGapBetweenHeaderAndRows;

	if (( ulFlags & SCOREBOARDFLAG_DONTDRAWBORDERS ) == false )
	{
		// [AK] The borders are drawn in three places: above and below the column headers, and
		// underneath all player rows. If using textures for the borders, then we must add
		// its height three times.
		if (( ulFlags & SCOREBOARDFLAG_USETEXTUREFORBORDERS ) && ( pBorderTexture != NULL ))
			ulHeight += pBorderTexture->GetScaledHeight( ) * 3;
		// [AK] Otherwise, add 6 pixels for lined borders (each border is 2 pixels tall).
		else
			ulHeight += 6;
	}

	// [AK] Add the total height of all rows for active players.
	if ( ulNumActivePlayers > 0 )
	{
		ulHeight += ulNumActivePlayers * ulRowYOffset;

		if ( ShouldSeparateTeams( ))
		{
			const ULONG ulNumTeamsWithPlayers = TEAM_TeamsWithPlayersOn( );

			if ( ulNumTeamsWithPlayers > 0 )
				ulHeight += lRowHeight * ( ulNumTeamsWithPlayers - 1 );
		}
	}

	// [AK] Do the same for any true spectators.
	if ( ulNumSpectators > 0 )
	{
		if ( ulNumActivePlayers > 0 )
			ulHeight += lRowHeight;

		ulHeight += ulNumSpectators * ulRowYOffset;
	}

	lRelY = ( HUD_GetHeight( ) - ulHeight ) / 2;
}

//*****************************************************************************
//
// [AK] Scoreboard::Render
//
// Draws the scoreboard's background, then everything else.
//
//*****************************************************************************

void Scoreboard::Render( const ULONG ulDisplayPlayer, const float fAlpha )
{
	int clipLeft = lRelX;
	int clipTop = lRelY;
	int clipWidth = ulWidth;
	int clipHeight = ulHeight;

	// [AK] We can't draw anything if the opacity is zero or less.
	if ( fAlpha <= 0.0f )
		return;

	// [AK] We must take into account the virtual screen's size.
	if ( g_bScale )
		screen->VirtualToRealCoordsInt( clipLeft, clipTop, clipWidth, clipHeight, con_virtualwidth, con_virtualheight, false, !con_scaletext_usescreenratio );

	screen->Dim( BackgroundColor, fBackgroundAmount * fAlpha, clipLeft, clipTop, clipWidth, clipHeight );

	const ULONG ulNumActivePlayers = HUD_GetNumPlayers( );
	const ULONG ulNumTrueSpectators = HUD_GetNumSpectators( );
	LONG lYPos = lRelY + ulBackgroundBorderSize;
	bool bUseLightBackground = true;

	// [AK] Draw a border above the column headers.
	DrawBorder( HeaderColor, lYPos, fAlpha, false );

	// [AK] Draw all of the column headers.
	for ( unsigned int i = 0; i < ColumnOrder.Size( ); i++ )
		ColumnOrder[i]->DrawHeader( pHeaderFont, HeaderColor, lYPos, lHeaderHeight, fAlpha );

	lYPos += lHeaderHeight;

	// [AK] Draw another border below the headers.
	DrawBorder( HeaderColor, lYPos, fAlpha, true );
	lYPos += ulGapBetweenHeaderAndRows;

	// [AK] Draw rows for all active players.
	for ( ULONG ulIdx = 0; ulIdx < ulNumActivePlayers; ulIdx++ )
	{
		const ULONG ulPlayer = ulPlayerList[ulIdx];

		// [AK] In team-based game modes, if the previous player is on a different team than
		// the current player, leave a gap between both teams and make the row background light.
		if (( ShouldSeparateTeams( )) && ( ulIdx > 0 ) && ( players[ulPlayer].Team != players[ulPlayerList[ulIdx - 1]].Team ))
		{
			lYPos += lRowHeight;
			bUseLightBackground = true;
		}

		DrawRow( ulPlayer, ulDisplayPlayer, lYPos, fAlpha, bUseLightBackground );
	}

	// [AK] Draw rows for any true spectators.
	if ( ulNumTrueSpectators )
	{
		const ULONG ulTotalPlayers = ulNumActivePlayers + ulNumTrueSpectators;

		// [AK] If there are any active players, leave a gap between them and the true
		// spectators, and make the row background light.
		if ( ulNumActivePlayers > 0 )
		{
			lYPos += lRowHeight;
			bUseLightBackground = true;
		}

		// [AK] The index of the first true spectator should be the same as the number of active
		// players. The list is organized such that all active players come before any true spectators.
		for ( ULONG ulIdx = ulNumActivePlayers; ulIdx < ulTotalPlayers; ulIdx++ )
			DrawRow( ulPlayerList[ulIdx], ulDisplayPlayer, lYPos, fAlpha, bUseLightBackground );
	}

	// [AK] Finally, draw a border at the bottom of the scoreboard. We must subtract ulGapBetweenRows here (a bit hacky)
	// because SCOREBOARD_s::DrawPlayerRow adds it every time a row is drawn. This isn't necessary for the last row.
	lYPos += ulGapBetweenHeaderAndRows - ulGapBetweenRows;
	DrawBorder( HeaderColor, lYPos, fAlpha, false );
}

//*****************************************************************************
//
// [AK] Scoreboard::DrawRow
//
// Draws a player's values and the background of their row on the scoreboard.
//
//*****************************************************************************

void Scoreboard::DrawRow( const ULONG ulPlayer, const ULONG ulDisplayPlayer, LONG &lYPos, const float fAlpha, bool &bUseLightBackground ) const
{
	const bool bIsDisplayPlayer = ( ulPlayer == ulDisplayPlayer );
	const bool bIsTrueSpectator = PLAYER_IsTrueSpectator( &players[ulPlayer] );
	const bool bPlayerIsDead = (( players[ulPlayer].playerstate == PST_DEAD ) || ( players[ulPlayer].bDeadSpectator ));
	ULONG ulColor = RowColor;

	// [AK] Change the text color to red if we're carrying a terminator sphere.
	if (( terminator ) && ( players[ulPlayer].cheats2 & CF2_TERMINATORARTIFACT ))
	{
		ulColor = CR_RED;
	}
	// [AK] Change the text color to match the player's team if we should.
	else if ( ulFlags & SCOREBOARDFLAG_USETEAMTEXTCOLOR )
	{
		if ( PLAYER_IsTrueSpectator( &players[ulPlayer] ))
			ulColor = CR_GREY;
		else if ( players[ulPlayer].bOnTeam )
			ulColor = TEAM_GetTextColor( players[ulPlayer].Team );
	}
	// [AK] Change the text color if this is the player we're spying.
	else if ( bIsDisplayPlayer )
	{
		if ( CLIENTDEMO_IsPlaying( ))
			ulColor = LocalRowColors[LOCALROW_COLOR_INDEMO];
		else
			ulColor = LocalRowColors[LOCALROW_COLOR_INGAME];
	}

	const float fBackgroundAlpha = ( bPlayerIsDead ? fDeadRowBackgroundAmount : fRowBackgroundAmount ) * fAlpha;

	// [AK] Draw the background of the row, but only if the alpha is non-zero. In team-based game modes,
	// the color of the background is to be the team's own color.
	if ( fBackgroundAlpha > 0.0f )
	{
		ROWBACKGROUND_COLOR_e RowBackground;

		if (( ulPlayer == ulDisplayPlayer ) && (( ulFlags & SCOREBOARDFLAG_DONTUSELOCALROWBACKGROUNDCOLOR ) == false ))
			RowBackground = ROWBACKGROUND_COLOR_LOCAL;
		else
			RowBackground = bUseLightBackground ? ROWBACKGROUND_COLOR_LIGHT : ROWBACKGROUND_COLOR_DARK;

		// [AK] If the player is on a team, blend the team's colour into the row background.
		if (( GAMEMODE_GetCurrentFlags( ) & GMF_PLAYERSONTEAMS ) && ( players[ulPlayer].bOnTeam ))
			DrawRowBackground( TeamRowBackgroundColors[players[ulPlayer].Team][RowBackground], lYPos, fBackgroundAlpha );
		// [AK] If the player isn't on a team, use the two background colors that are defined.
		else
			DrawRowBackground( RowBackgroundColors[RowBackground], lYPos, fBackgroundAlpha );
	}

	const float fTextAlpha = ( bPlayerIsDead ? fDeadTextAlpha : 1.0f ) * fAlpha;

	// Draw the data for each column, but only if the text alpha is non-zero.
	if ( fTextAlpha > 0.0f )
	{
		for ( unsigned int i = 0; i < ColumnOrder.Size( ); i++ )
			ColumnOrder[i]->DrawValue( ulPlayer, pRowFont, ulColor, lYPos, lRowHeight, fTextAlpha );
	}

	lYPos += lRowHeight + ulGapBetweenRows;
	bUseLightBackground = !bUseLightBackground;
}

//*****************************************************************************
//
// [AK] Scoreboard::DrawBorder
//
// Draws a border on the scoreboard.
//
//*****************************************************************************

void Scoreboard::DrawBorder( const EColorRange Color, LONG &lYPos, const float fAlpha, const bool bReverse ) const
{
	if ( ulFlags & SCOREBOARDFLAG_DONTDRAWBORDERS )
		return;

	int x = lRelX + ulBackgroundBorderSize;
	int y = lYPos;
	int width = ulWidth - 2 * ulBackgroundBorderSize;
	int height = 0;

	if (( ulFlags & SCOREBOARDFLAG_USETEXTUREFORBORDERS ) && ( pBorderTexture != NULL ))
	{
		LONG lXPos = x;
		const LONG lRight = x + width;

		height = pBorderTexture->GetScaledHeight( );

		if ( g_bScale )
			screen->VirtualToRealCoordsInt( x, y, width, height, con_virtualwidth, con_virtualheight, false, !con_scaletext_usescreenratio );

		while ( lXPos < lRight )
		{
			screen->DrawTexture( pBorderTexture, lXPos, lYPos,
				DTA_UseVirtualScreen, g_bScale,
				DTA_ClipLeft, x,
				DTA_ClipRight, x + width,
				DTA_ClipTop, y,
				DTA_ClipBottom, y + height,
				DTA_Alpha, FLOAT2FIXED( fAlpha ),
				TAG_DONE );

			lXPos += pBorderTexture->GetScaledWidth( );
		}

		lYPos += pBorderTexture->GetScaledHeight( );
	}
	else
	{
		uint32 lightColor, darkColor;
		height = 1;

		if ( g_bScale )
			screen->VirtualToRealCoordsInt( x, y, width, height, con_virtualwidth, con_virtualheight, false, !con_scaletext_usescreenratio );

		// [AK] Do we want to use the font's translation table and text color to colorize the border,
		// or the predetermined hexadecimal colors for the border?
		if ( ulFlags & SCOREBOARDFLAG_USEHEADERCOLORFORBORDERS )
		{
			// [AK] Get the translation table of the (team) header font with its corresponding color.
			const FRemapTable *trans = pHeaderFont->GetColorTranslation( Color );

			// [AK] The light color can be somewhere just past the middle of the remap table.
			lightColor = trans->Palette[trans->NumEntries * 2 / 3];

			// [AK] The dark color should be somewhere at the beginning of the remap table.
			darkColor = trans->Palette[MIN( 1, trans->NumEntries )];
		}
		else
		{
			lightColor = BorderColors[BORDER_COLOR_LIGHT];
			darkColor = BorderColors[BORDER_COLOR_DARK];
		}

		// [AK] The dark color goes above the light one, unless it's reversed.
		screen->Dim( bReverse ? lightColor : darkColor, fAlpha, x, y, width, height );
		screen->Dim( bReverse ? darkColor : lightColor, fAlpha, x, y + height, width, height );
		lYPos += 2;
	}
}

//*****************************************************************************
//
// [AK] Scoreboard::DrawRowBackground
//
// Draws a row's background on the scoreboard.
//
//*****************************************************************************

void Scoreboard::DrawRowBackground( const PalEntry color, int x, int y, int width, int height, const float fAlpha ) const
{
	if (( fAlpha <= 0.0f ) || ( fRowBackgroundAmount <= 0.0f ))
		return;

	if ( g_bScale )
		screen->VirtualToRealCoordsInt( x, y, width, height, con_virtualwidth, con_virtualheight, false, !con_scaletext_usescreenratio );

	screen->Dim( color, fAlpha * fRowBackgroundAmount, x, y, width, height );
}

//*****************************************************************************
//
void Scoreboard::DrawRowBackground( const PalEntry color, const int y, const float fAlpha ) const
{
	if (( fAlpha <= 0.0f ) || ( fRowBackgroundAmount <= 0.0f ))
		return;

	const int height = lRowHeight;

	// [AK] If gaps must be shown in the row's background, then only draw the background where
	// the active columns are. Otherwise, draw a single background across the scoreboard.
	if ( ulFlags & SCOREBOARDFLAG_SHOWGAPSINROWBACKGROUND )
	{
		for ( unsigned int i = 0; i < ColumnOrder.Size( ); i++ )
		{
			if ( ColumnOrder[i]->IsDisabled( ))
				continue;

			DrawRowBackground( color, ColumnOrder[i]->GetRelX( ), y, ColumnOrder[i]->GetWidth( ), height, fAlpha );
		}
	}
	else
	{
		DrawRowBackground( color, lRelX + ulBackgroundBorderSize, y, ulWidth - 2 * ulBackgroundBorderSize, height, fAlpha );
	}
}

//*****************************************************************************
//
// [AK] Scoreboard::ShouldSeparateTeams
//
// Checks if the scoreboard should separate players into their respective teams.
//
//*****************************************************************************

bool Scoreboard::ShouldSeparateTeams( void ) const
{
	return (( GAMEMODE_GetCurrentFlags( ) & GMF_PLAYERSONTEAMS ) && (( ulFlags & SCOREBOARDFLAG_DONTSEPARATETEAMS ) == false ));
}

//*****************************************************************************
//
// [AK] SCOREBOARD_GetColumn
//
// Returns a pointer to a column by searching for its name.
//
//*****************************************************************************

ScoreColumn *SCOREBOARD_GetColumn( FName Name )
{
	ScoreColumn **pColumn = g_Columns.CheckKey( Name );
	return ( pColumn != NULL ) ? *pColumn : NULL;
}

//*****************************************************************************
//
// [AK] SCOREBOARD_IsDisabled
//
// Checks if the scoreboard is disabled.
//
//*****************************************************************************

bool SCOREBOARD_IsDisabled( void )
{
	return g_Scoreboard.bDisabled;
}

//*****************************************************************************
//
// [AK] SCOREBOARD_IsHidden
//
// Checks if the scoreboard is hidden.
//
//*****************************************************************************

bool SCOREBOARD_IsHidden( void )
{
	return g_Scoreboard.bHidden;
}

//*****************************************************************************
//
// [AK] SCOREBOARD_SetHidden
//
// Hides (or unhides) the scoreboard, which also requires it to be refreshed.
//
//*****************************************************************************

void SCOREBOARD_SetHidden( bool bEnable )
{
	if ( g_Scoreboard.bHidden == bEnable )
		return;

	g_Scoreboard.bHidden = bEnable;
	SCOREBOARD_ShouldRefreshBeforeRendering( );
}

//*****************************************************************************
//
// SCOREBOARD_ShouldDrawBoard
//
// Checks if the user wants to see the scoreboard and is allowed to.
//
//*****************************************************************************

bool SCOREBOARD_ShouldDrawBoard( void )
{
	// [AK] If the user isn't pressing their scoreboard key then return false.
	if ( Button_ShowScores.bDown == false )
		return false;

	// [AK] We generally don't want to draw the scoreboard in singleplayer games unless we're
	// watching a demo. However, we still want to draw it in deathmatch, teamgame, or invasion.
	if (( NETWORK_GetState( ) == NETSTATE_SINGLE ) && ( CLIENTDEMO_IsPlaying( ) == false ) && (( deathmatch || teamgame || invasion ) == false ))
		return false;

	return true;
}

//*****************************************************************************
//
// [AK] SCOREBOARD_Reset
//
// This should only be executed at the start of a new game or level, or at the
// start of the intermission screen. It checks if any columns that are part of
// the scoreboard are usable in the current game.
//
//*****************************************************************************

void SCOREBOARD_Reset( void )
{
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	TMapIterator<FName, ScoreColumn *> it( g_Columns );
	TMap<FName, ScoreColumn *>::Pair *pair;

	while ( it.NextPair( pair ))
		pair->Value->CheckIfUsable( );

	// [AK] It would be a good idea to refresh the scoreboard after resetting.
	SCOREBOARD_ShouldRefreshBeforeRendering( );
}

//*****************************************************************************
//
void SCOREBOARD_Render( ULONG ulDisplayPlayer )
{
	// Make sure the display player is valid.
	if ( ulDisplayPlayer >= MAXPLAYERS )
		return;

	// [AK] If we need to update the scoreboard, do so before rendering it.
	if ( g_bRefreshBeforeRendering )
	{
		SCOREBOARD_Refresh( );
		g_bRefreshBeforeRendering = false;
	}

	// [AK] Draw the scoreboard header at the top.
	scoreboard_DrawHeader( ulDisplayPlayer );

	// Draw the headers, list, entries, everything.
	scoreboard_DrawRankings( ulDisplayPlayer );
}

//*****************************************************************************
//
void SCOREBOARD_Refresh( void )
{
	// First, determine how many columns we can use, based on our screen resolution.
	ULONG ulNumIdealColumns = 3;

	if ( HUD_GetWidth( ) >= 600 )
		ulNumIdealColumns = 5;
	else if ( HUD_GetWidth( ) >= 480 )
		ulNumIdealColumns = 4;

	// The 5 column display is only availible for modes that support it.
	if (( ulNumIdealColumns == 5 ) && !( GAMEMODE_GetCurrentFlags( ) & ( GMF_PLAYERSEARNPOINTS | GMF_PLAYERSEARNWINS )))
		ulNumIdealColumns = 4;

	if ( ulNumIdealColumns == 5 )
		scoreboard_Prepare5ColumnDisplay( );
	else if ( ulNumIdealColumns == 4 )
		scoreboard_Prepare4ColumnDisplay( );
	else
		scoreboard_Prepare3ColumnDisplay( );
}

//*****************************************************************************
//
void SCOREBOARD_ShouldRefreshBeforeRendering( void )
{
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	g_bRefreshBeforeRendering = true;
}

//*****************************************************************************
//
LONG SCOREBOARD_GetLeftToLimit( void )
{
	ULONG	ulIdx;

	// If we're not in a level, then clearly there's no need for this.
	if ( gamestate != GS_LEVEL )
		return ( 0 );

	// KILL-based mode. [BB] This works indepently of any players in game.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNKILLS )
	{
		if ( invasion )
			return (LONG) INVASION_GetNumMonstersLeft( );
		else if ( dmflags2 & DF2_KILL_MONSTERS )
		{
			if ( level.total_monsters > 0 )
				return ( 100 * ( level.total_monsters - level.killed_monsters ) / level.total_monsters );
			else
				return 0;
		}
		else
			return ( level.total_monsters - level.killed_monsters );
	}

	// [BB] In a team game with only empty teams or if there are no players at all, just return the appropriate limit.
	if ( ( ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
	     && ( TEAM_TeamsWithPlayersOn() == 0 ) )
		 || ( SERVER_CalcNumNonSpectatingPlayers( MAXPLAYERS ) == 0 ) )
	{
		if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNWINS )
			return winlimit;
		else if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNPOINTS )
			return pointlimit;
		else if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNFRAGS )
			return fraglimit;
		else
			return 0;
	}

	// FRAG-based mode.
	if ( fraglimit && GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNFRAGS )
	{
		LONG	lHighestFragcount;
				
		if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
			lHighestFragcount = TEAM_GetHighestFragCount( );		
		else
		{
			lHighestFragcount = INT_MIN;
			for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				if ( playeringame[ulIdx] && !players[ulIdx].bSpectating && players[ulIdx].fragcount > lHighestFragcount )
					lHighestFragcount = players[ulIdx].fragcount;
			}
		}

		return ( fraglimit - lHighestFragcount );
	}

	// POINT-based mode.
	else if ( pointlimit && GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNPOINTS )
	{
		if ( teamgame || teampossession )
			return ( pointlimit - TEAM_GetHighestPointCount( ));
		else // Must be possession mode.
		{
			LONG lHighestPointCount = INT_MIN;
			for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				if ( playeringame[ulIdx] && !players[ulIdx].bSpectating && players[ulIdx].lPointCount > lHighestPointCount )
					lHighestPointCount = players[ulIdx].lPointCount;
			}

			return pointlimit - (ULONG) lHighestPointCount;
		}
	}

	// WIN-based mode (LMS).
	else if ( winlimit && GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNWINS )
	{
		bool	bFoundPlayer = false;
		LONG	lHighestWincount = 0;

		if ( teamlms )
			lHighestWincount = TEAM_GetHighestWinCount( );
		else
		{
			for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
			{
				if ( playeringame[ulIdx] == false || players[ulIdx].bSpectating )
					continue;

				if ( bFoundPlayer == false )
				{
					lHighestWincount = players[ulIdx].ulWins;
					bFoundPlayer = true;
					continue;
				}
				else if ( players[ulIdx].ulWins > (ULONG)lHighestWincount )
					lHighestWincount = players[ulIdx].ulWins;
			}
		}

		return ( winlimit - lHighestWincount );
	}

	// None of the above.
	return ( -1 );
}

//*****************************************************************************
//*****************************************************************************
//
static void scoreboard_SortPlayers( ULONG ulSortType )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		g_iSortedPlayers[ulIdx] = ulIdx;

	if ( ulSortType == ST_FRAGCOUNT )
		qsort( g_iSortedPlayers, MAXPLAYERS, sizeof( int ), scoreboard_FragCompareFunc );
	else if ( ulSortType == ST_POINTCOUNT )
		qsort( g_iSortedPlayers, MAXPLAYERS, sizeof( int ), scoreboard_PointsCompareFunc );
	else if ( ulSortType == ST_WINCOUNT )
		qsort( g_iSortedPlayers, MAXPLAYERS, sizeof( int ), scoreboard_WinsCompareFunc );
	else
		qsort( g_iSortedPlayers, MAXPLAYERS, sizeof( int ), scoreboard_KillsCompareFunc );
}

//*****************************************************************************
//
static int STACK_ARGS scoreboard_FragCompareFunc( const void *arg1, const void *arg2 )
{
	return ( players[*(int *)arg2].fragcount - players[*(int *)arg1].fragcount );
}

//*****************************************************************************
//
static int STACK_ARGS scoreboard_PointsCompareFunc( const void *arg1, const void *arg2 )
{
	return ( players[*(int *)arg2].lPointCount - players[*(int *)arg1].lPointCount );
}

//*****************************************************************************
//
static int STACK_ARGS scoreboard_KillsCompareFunc( const void *arg1, const void *arg2 )
{
	return ( players[*(int *)arg2].killcount - players[*(int *)arg1].killcount );
}

//*****************************************************************************
//
static int STACK_ARGS scoreboard_WinsCompareFunc( const void *arg1, const void *arg2 )
{
	return ( players[*(int *)arg2].ulWins - players[*(int *)arg1].ulWins );
}

//*****************************************************************************
//
static void scoreboard_DrawText( const char *pszString, EColorRange Color, ULONG &ulXPos, ULONG ulOffset, bool bOffsetRight )
{
	ulXPos += SmallFont->StringWidth( pszString ) * ( bOffsetRight ? 1 : -1 );
	HUD_DrawText( SmallFont, Color, ulXPos, g_ulCurYPos, pszString, g_bScale );
	ulXPos += ulOffset * ( bOffsetRight ? 1 : -1 );
}

//*****************************************************************************
//
static void scoreboard_DrawIcon( const char *pszPatchName, ULONG &ulXPos, ULONG ulYPos, ULONG ulOffset, bool bOffsetRight )
{
	ulXPos += TexMan[pszPatchName]->GetWidth( ) * ( bOffsetRight ? 1 : -1 );
	ulYPos -= (( TexMan[pszPatchName]->GetHeight( ) - SmallFont->GetHeight( )) >> 1 );

	HUD_DrawTexture( TexMan[pszPatchName], ulXPos, ulYPos, g_bScale );
	ulXPos += ulOffset * ( bOffsetRight ? 1 : -1 );
}

//*****************************************************************************
//
static void scoreboard_RenderIndividualPlayer( ULONG ulDisplayPlayer, ULONG ulPlayer )
{
	ULONG ulColor = CR_GRAY;
	FString patchName;
	FString text;

	// [AK] Change the text color if we're carrying a terminator sphere or on a team.
	if (( terminator ) && ( players[ulPlayer].cheats2 & CF2_TERMINATORARTIFACT ))
		ulColor = CR_RED;
	else if ( players[ulPlayer].bOnTeam )
		ulColor = TEAM_GetTextColor( players[ulPlayer].Team );
	else if ( ulDisplayPlayer == ulPlayer )
		ulColor = demoplayback ? CR_GOLD : CR_GREEN;

	// Draw the data for each column.
	for ( ULONG ulColumn = 0; ulColumn < g_ulNumColumnsUsed; ulColumn++ )
	{
		// [AK] Determine the x-position of the text for this column.
		ULONG ulXPos = static_cast<ULONG>( g_aulColumnX[ulColumn] * g_fXScale );

		// [AK] We need to display icons and some extra text in the name column.
		if ( g_aulColumnType[ulColumn] == COLUMN_NAME )
		{
			// Track where we are to draw multiple icons.
			ULONG ulXPosOffset = ulXPos - SmallFont->StringWidth( "  " );

			// [TP] If this player is in the join queue, display the position.
			int position = JOINQUEUE_GetPositionInLine( ulPlayer );
			if ( position != -1 )
			{
				text.Format( "%d.", position + 1 );
				scoreboard_DrawText( text, position == 0 ? CR_RED : CR_GOLD, ulXPosOffset, 4 );
			}

			// Draw the user's handicap, if any.
			int handicap = players[ulPlayer].userinfo.GetHandicap( );
			if ( handicap > 0 )
			{
				if (( lastmanstanding ) || ( teamlms ))
					text.Format( "(%d)", deh.MaxSoulsphere - handicap < 1 ? 1 : deh.MaxArmor - handicap );
				else
					text.Format( "(%d)", deh.StartHealth - handicap < 1 ? 1 : deh.StartHealth - handicap );

				scoreboard_DrawText( text, static_cast<EColorRange>( ulColor ), ulXPosOffset, 4 );
			}

			// Draw an icon if this player is a ready to go on.
			if ( players[ulPlayer].bReadyToGoOn )
				scoreboard_DrawIcon( "RDYTOGO", ulXPosOffset, g_ulCurYPos, 4 );

			// Draw a bot icon if this player is a bot.
			if ( players[ulPlayer].bIsBot )
			{
				patchName.Format( "BOTSKIL%d", botskill.GetGenericRep( CVAR_Int ).Int );
				scoreboard_DrawIcon( patchName, ulXPosOffset, g_ulCurYPos, 4 );
			}

			// Draw a chat icon if this player is chatting.
			// [Cata] Also shows who's in the console.
			// [AK] Also show who's in the menu.
			if (( players[ulPlayer].bChatting ) || ( players[ulPlayer].bInConsole ) || ( players[ulPlayer].bInMenu ))
			{
				if ( players[ulPlayer].bChatting )
					patchName = "TLKMINI";
				else if ( players[ulPlayer].bInConsole )
					patchName = "CONSMINI";
				else
					patchName = "MENUMINI";

				scoreboard_DrawIcon( patchName, ulXPosOffset, g_ulCurYPos, 4 );
			}

			// [AK] Also show an icon if the player is lagging to the server.
			if (( players[ulPlayer].bLagging ) && ( gamestate == GS_LEVEL ))
				scoreboard_DrawIcon( "LAGMINI", ulXPosOffset, g_ulCurYPos, 4 );

			// Draw text if there's a vote on and this player voted.
			if ( CALLVOTE_GetVoteState( ) == VOTESTATE_INVOTE )
			{
				ULONG ulVoteChoice = CALLVOTE_GetPlayerVoteChoice( ulPlayer );

				// [AK] Check if this player either voted yes or no.
				if ( ulVoteChoice != VOTE_UNDECIDED )
				{
					text.Format( "(%s)", ulVoteChoice == VOTE_YES ? "Yes" : "No" );
					scoreboard_DrawText( text, CALLVOTE_GetVoteCaller( ) == ulPlayer ? CR_RED : CR_GOLD, ulXPosOffset, 4 );
				}
			}

			text = players[ulPlayer].userinfo.GetName( );
		}
		else if ( g_aulColumnType[ulColumn] == COLUMN_TIME )
		{
			text.Format( "%d", static_cast<unsigned int>( players[ulPlayer].ulTime / ( TICRATE * 60 )));
		}
		else if ( g_aulColumnType[ulColumn] == COLUMN_PING )
		{
			text.Format( "%d", static_cast<unsigned int>( players[ulPlayer].ulPing ));
		}
		else if ( g_aulColumnType[ulColumn] == COLUMN_DEATHS )
		{
			text.Format( "%d", static_cast<unsigned int>( players[ulPlayer].ulDeathCount ));
		}
		else
		{
			switch ( g_aulColumnType[ulColumn] )
			{
				case COLUMN_FRAGS:
					text.Format( "%d", players[ulPlayer].fragcount );
					break;

				case COLUMN_POINTS:
					text.Format( "%d", static_cast<int>( players[ulPlayer].lPointCount ));
					break;

				case COLUMN_ASSISTS:
					text.Format( "%d", static_cast<unsigned int>( players[ulPlayer].ulMedalCount[14] ));
					break;

				case COLUMN_WINS:
					text.Format( "%d", static_cast<unsigned int>( players[ulPlayer].ulWins ));
					break;

				case COLUMN_KILLS:
					text.Format( "%d", players[ulPlayer].killcount );
					break;

				case COLUMN_SECRETS:
					text.Format( "%d", players[ulPlayer].secretcount );
					break;
			}

			// If the player isn't really playing, change this.
			if ( PLAYER_IsTrueSpectator( &players[ulPlayer] ))
				text = "Spect";
			else if (( GAMEMODE_GetCurrentFlags( ) & GMF_PLAYERSONTEAMS ) && ( players[ulPlayer].bOnTeam == false ))
				text = "No Team";
			else if (( GAMEMODE_GetCurrentFlags( ) & GMF_DEADSPECTATORS ) && (( players[ulPlayer].health <= 0 ) || ( players[ulPlayer].bDeadSpectator )) && ( gamestate != GS_INTERMISSION ))
				text = "Dead";
		}

		HUD_DrawText( SmallFont, ulColor, ulXPos, g_ulCurYPos, text, g_bScale );
	}
}

//*****************************************************************************
//
void SCOREBOARD_SetNextLevel( const char *pszMapName )
{
	g_pNextLevel = ( pszMapName != NULL ) ? FindLevelInfo( pszMapName ) : NULL;
}

//*****************************************************************************
//
static void scoreboard_DrawHeader( ULONG ulPlayer )
{
	g_ulCurYPos = 4;

	// Draw the "RANKINGS" text at the top. Don't draw it if we're in the intermission.
	if ( gamestate == GS_LEVEL )
		HUD_DrawTextCentered( BigFont, CR_RED, g_ulCurYPos, "RANKINGS", g_bScale );

	g_ulCurYPos += BigFont->GetHeight( ) + 6;

	if ( gamestate == GS_LEVEL )
	{
		// [AK] Draw the name of the server if we're in an online game.
		if ( NETWORK_InClientMode( ))
		{
			FString hostName = sv_hostname.GetGenericRep( CVAR_String ).String;
			V_ColorizeString( hostName );
			HUD_DrawTextCentered( SmallFont, CR_GREY, g_ulCurYPos, hostName, g_bScale );
			g_ulCurYPos += SmallFont->GetHeight( ) + 1;
		}

		// [AK] Draw the name of the current game mode.
		HUD_DrawTextCentered( SmallFont, CR_GOLD, g_ulCurYPos, GAMEMODE_GetCurrentName( ), g_bScale );
		g_ulCurYPos += SmallFont->GetHeight( ) + 1;

		// Draw the time, frags, points, or kills we have left until the level ends.
		// Generate the limit strings.
		std::list<FString> lines;
		SCOREBOARD_BuildLimitStrings( lines, true );

		// Now, draw them.
		for ( std::list<FString>::iterator i = lines.begin( ); i != lines.end( ); i++ )
		{
			HUD_DrawTextCentered( SmallFont, CR_GREY, g_ulCurYPos, *i, g_bScale );
			g_ulCurYPos += SmallFont->GetHeight( ) + 1;
		}
	}

	// Draw the team scores and their relation (tied, red leads, etc).
	if (( GAMEMODE_GetCurrentFlags( ) & GMF_PLAYERSONTEAMS ) && (( GAMEMODE_GetCurrentFlags( ) & GMF_PLAYERSEARNKILLS ) == false ))
	{
		if ( gamestate != GS_LEVEL )
			g_ulCurYPos += SmallFont->GetHeight( ) + 1;

		HUD_DrawTextCentered( SmallFont, CR_GREY, g_ulCurYPos, HUD_BuildPointString( ), g_bScale );
		g_ulCurYPos += SmallFont->GetHeight( ) + 1;
	}
	// Draw my rank and my frags, points, etc. Don't draw it if we're in the intermission.
	else if (( gamestate == GS_LEVEL ) && ( HUD_ShouldDrawRank( ulPlayer )))
	{
		HUD_DrawTextCentered( SmallFont, CR_GREY, g_ulCurYPos, HUD_BuildPlaceString( ulPlayer ), g_bScale );
		g_ulCurYPos += SmallFont->GetHeight( ) + 1;
	}

	// [JS] Intermission countdown display.
	if (( gamestate == GS_INTERMISSION ) && ( NETWORK_GetState( ) == NETSTATE_CLIENT ) && ( cl_intermissiontimer ))
	{
		FString countdownMessage = "Entering ";

		// [AK] Display the name of the level we're entering if possible.
		if ( g_pNextLevel != NULL )
			countdownMessage.AppendFormat( "%s: %s", g_pNextLevel->mapname, g_pNextLevel->LookupLevelName( ).GetChars() );
		else
			countdownMessage += "next map";

		countdownMessage.AppendFormat( " in %d seconds", MAX( static_cast<int>( WI_GetStopWatch( )) / TICRATE + 1, 1 ));
		HUD_DrawTextCentered( SmallFont, CR_GREEN, g_ulCurYPos, countdownMessage, g_bScale );
		g_ulCurYPos += SmallFont->GetHeight( ) + 1;
	}
}

//*****************************************************************************
// [AK] Checks if there's already a limit string on the list, removes it from the list, then
// prepends it to the string we passed into the function.
//
void scoreboard_TryToPrependLimit( std::list<FString> &lines, FString &limit )
{
	// [AK] This shouldn't be done on the server console.
	if (( NETWORK_GetState( ) != NETSTATE_SERVER ) && ( lines.empty( ) == false ))
	{
		FString prevLimitString = lines.back( );
		lines.pop_back( );

		prevLimitString += TEXTCOLOR_DARKGRAY " - " TEXTCOLOR_NORMAL;
		limit.Insert( 0, prevLimitString );
	}
}

//*****************************************************************************
// [RC] Helper method for SCOREBOARD_BuildLimitStrings. Creates a "x things remaining" message.
// [AK] Added the bWantToPrepend parameter.
//
void scoreboard_AddSingleLimit( std::list<FString> &lines, bool condition, int remaining, const char *pszUnitName, bool bWantToPrepend = false )
{
	if ( condition && remaining > 0 )
	{
		FString limitString;
		limitString.Format( "%d %s%s left", static_cast<int>( remaining ), pszUnitName, remaining == 1 ? "" : "s" );

		// [AK] Try to make this string appear on the same line as a previous string if we want to.
		if ( bWantToPrepend )
			scoreboard_TryToPrependLimit( lines, limitString );

		lines.push_back( limitString );
	}
}

//*****************************************************************************
// [AK] Creates the time limit message to be shown on the scoreboard or server console.
//
void scoreboard_AddTimeLimit( std::list<FString> &lines )
{
	FString TimeLeftString;
	GAMEMODE_GetTimeLeftString( TimeLeftString );

	// [AK] Also print "round" when there's more than one duel match to be played.
	FString limitString = (( duel && duellimit > 1 ) || ( GAMEMODE_GetCurrentFlags( ) & GMF_PLAYERSEARNWINS )) ? "Round" : "Level";
	limitString.AppendFormat( " ends in %s", TimeLeftString.GetChars( ));

	// [AK] Try to put the time limit string on the same line as a previous string.
	scoreboard_TryToPrependLimit( lines, limitString );
	lines.push_back( limitString );
}

//*****************************************************************************
//
// [RC] Builds the series of "x frags left / 3rd match between the two / 15:10 remain" strings. Used here and in serverconsole.cpp
//
void SCOREBOARD_BuildLimitStrings( std::list<FString> &lines, bool bAcceptColors )
{
	if ( gamestate != GS_LEVEL )
		return;

	ULONG ulFlags = GAMEMODE_GetCurrentFlags( );
	LONG lRemaining = SCOREBOARD_GetLeftToLimit( );
	const bool bTimeLimitActive = GAMEMODE_IsTimelimitActive( );
	bool bTimeLimitAdded = false;
	FString text;

	// Build the fraglimit string.
	scoreboard_AddSingleLimit( lines, fraglimit && ( ulFlags & GMF_PLAYERSEARNFRAGS ), lRemaining, "frag" );

	// Build the duellimit and "wins" string.
	if ( duel && duellimit )
	{
		ULONG ulWinner = MAXPLAYERS;
		LONG lHighestFrags = LONG_MIN;
		const bool bInResults = GAMEMODE_IsGameInResultSequence( );
		bool bDraw = true;

		// [AK] If there's a fraglimit and a duellimit string, the timelimit string should be put in-between them
		// on the scoreboard to organize the info better (frags left on the left, duels left on the right).
		if (( bTimeLimitActive ) && ( lines.empty( ) == false ) && ( NETWORK_GetState( ) != NETSTATE_SERVER ))
		{
			scoreboard_AddTimeLimit( lines );
			bTimeLimitAdded = true;
		}

		// [TL] The number of duels left is the maximum number of duels less the number of duels fought.
		// [AK] We already confirmed we're using duel limits, so we can now add this string unconditionally.
		scoreboard_AddSingleLimit( lines, true, duellimit - DUEL_GetNumDuels( ), "duel", true );

		// [AK] If we haven't added the timelimit string yet, make it appear next to the duellimit string.
		if (( bTimeLimitActive ) && ( bTimeLimitAdded == false ) && ( NETWORK_GetState( ) != NETSTATE_SERVER ))
		{
			scoreboard_AddTimeLimit( lines );
			bTimeLimitAdded = true;
		}

		for ( ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			if (( playeringame[ulIdx] ) && ( players[ulIdx].ulWins > 0 ))
			{
				// [AK] In case both duelers have at least one win during the results sequence the,
				// champion should be the one with the higher frag count.
				if ( bInResults )
				{
					if ( players[ulIdx].fragcount > lHighestFrags )
					{
						ulWinner = ulIdx;
						lHighestFrags = players[ulIdx].fragcount;
					}
				}
				else
				{
					ulWinner = ulIdx;
					break;
				}
			}
		}

		if ( ulWinner == MAXPLAYERS )
		{
			if ( GAME_CountActivePlayers( ) == 2 )
				text = "First match between the two";
			else
				bDraw = false;
		}
		else
		{
			text.Format( "Champion is %s", players[ulWinner].userinfo.GetName( ));
			text.AppendFormat( " with %d win%s", static_cast<unsigned int>( players[ulWinner].ulWins ), players[ulWinner].ulWins == 1 ? "" : "s" );
		}

		if ( bDraw )
		{
			if ( !bAcceptColors )
				V_RemoveColorCodes( text );

			lines.push_back( text );
		}
	}

	// Build the pointlimit, winlimit, and/or wavelimit strings.
	scoreboard_AddSingleLimit( lines, pointlimit && ( ulFlags & GMF_PLAYERSEARNPOINTS ), lRemaining, "point" );
	scoreboard_AddSingleLimit( lines, winlimit && ( ulFlags & GMF_PLAYERSEARNWINS ), lRemaining, "win" );
	scoreboard_AddSingleLimit( lines, invasion && wavelimit, wavelimit - INVASION_GetCurrentWave( ), "wave" );

	// [AK] Build the coop strings.
	if ( ulFlags & GMF_COOPERATIVE )
	{
		ULONG ulNumLimits = 0;

		// Render the number of monsters left in coop.
		// [AK] Unless we're playing invasion, only do this when there are actually monsters on the level.
		if (( ulFlags & GMF_PLAYERSEARNKILLS ) && (( invasion ) || ( level.total_monsters > 0 )))
		{
			if (( invasion ) || (( dmflags2 & DF2_KILL_MONSTERS ) == false ))
				text.Format( "%d monster%s left", static_cast<int>( lRemaining ), lRemaining == 1 ? "" : "s" );
			else
				text.Format( "%d%% monsters left", static_cast<int>( lRemaining ));

			// [AK] Render the number of monsters left on the same line as the number of waves left in invasion.
			if ( invasion && wavelimit )			
				scoreboard_TryToPrependLimit( lines, text );

			lines.push_back( text );
			ulNumLimits++;
		}

		// [AK] If there's monsters and secrets on the current level, the timelimit string should be put in-between
		// them on the scoreboard to organize the info better (monsters left on the left, secrets left on the right).
		if (( bTimeLimitActive ) && ( lines.empty( ) == false ) && ( NETWORK_GetState( ) != NETSTATE_SERVER ))
		{
			scoreboard_AddTimeLimit( lines );
			bTimeLimitAdded = true;
			ulNumLimits++;
 		}

		// [AK] Render the number of secrets left.
		if ( level.total_secrets > 0 )
		{
			lRemaining = level.total_secrets - level.found_secrets;
			text.Format( "%d secret%s left", static_cast<int>( lRemaining ), lRemaining == 1 ? "" : "s" );
			scoreboard_TryToPrependLimit( lines, text );
			lines.push_back( text );
			ulNumLimits++;
		}

		// [AK] If we haven't added the timelimit string yet, make it appear next to the "secrets left" string.
		if (( bTimeLimitActive ) && ( bTimeLimitAdded == false ) && ( NETWORK_GetState( ) != NETSTATE_SERVER ))
		{
			scoreboard_AddTimeLimit( lines );
			bTimeLimitAdded = true;
			ulNumLimits++;
 		}

		// [WS] Show the damage factor.
		if ( sv_coop_damagefactor != 1.0f )
		{
			text.Format( "Damage factor is %.2f", static_cast<float>( sv_coop_damagefactor ));

			// [AK] If there aren't too many limits already, try to make the damage factor appear on the same
			// line as a previous string.
			if ( ulNumLimits == 1 )
				scoreboard_TryToPrependLimit( lines, text );

			lines.push_back( text );
		}
	}

	// Render the timelimit string. - [BB] if the gamemode uses it.
	// [AK] Don't add this if we've already done so.
	if (( bTimeLimitActive ) && ( bTimeLimitAdded == false ))
		scoreboard_AddTimeLimit( lines );
}

//*****************************************************************************
//
static void scoreboard_ClearColumns( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_COLUMNS; ulIdx++ )
		g_aulColumnType[ulIdx] = COLUMN_EMPTY;

	g_ulNumColumnsUsed = 0;
}

//*****************************************************************************
//
static void scoreboard_Prepare5ColumnDisplay( void )
{
	// Set all to empty.
	scoreboard_ClearColumns( );

	g_ulNumColumnsUsed = 5;
	g_pColumnHeaderFont = BigFont;

	// Set up the location of each column.
	g_aulColumnX[0] = 8;
	g_aulColumnX[1] = 56;
	g_aulColumnX[2] = 106;
	g_aulColumnX[3] = 222;
	g_aulColumnX[4] = 286;

	// Build columns for modes in which players try to earn points.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNPOINTS )
	{
		g_aulColumnType[0] = COLUMN_POINTS;
		// [BC] Doesn't look like this is being used right now (at least not properly).
/*
		// Can have assists.
		if ( ctf || skulltag )
			g_aulColumnType[0] = COL_POINTSASSISTS;
*/
		g_aulColumnType[1] = COLUMN_FRAGS;
		g_aulColumnType[2] = COLUMN_NAME;
		g_aulColumnType[3] = COLUMN_DEATHS;
		if ( NETWORK_InClientMode() )
			g_aulColumnType[3] = COLUMN_PING;
		g_aulColumnType[4] = COLUMN_TIME;

		// Sort players based on their pointcount.
		scoreboard_SortPlayers( ST_POINTCOUNT );
	}

	// Build columns for modes in which players try to earn wins.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNWINS )
	{
		g_aulColumnType[0] = COLUMN_WINS;
		g_aulColumnType[1] = COLUMN_FRAGS;
		g_aulColumnType[2] = COLUMN_NAME;
		g_aulColumnType[3] = COLUMN_DEATHS;
		if ( NETWORK_InClientMode() )
			g_aulColumnType[3] = COLUMN_PING;
		g_aulColumnType[4] = COLUMN_TIME;

		// Sort players based on their pointcount.
		scoreboard_SortPlayers( ST_WINCOUNT );
	}
}

//*****************************************************************************
//
static void scoreboard_SetColumnZeroToKillsAndSortPlayers( void )
{
	if ( zadmflags & ZADF_AWARD_DAMAGE_INSTEAD_KILLS )
	{
		g_aulColumnType[0] = COLUMN_POINTS;

		// Sort players based on their points.
		scoreboard_SortPlayers( ST_POINTCOUNT );
	}
	else
	{
		g_aulColumnType[0] = COLUMN_KILLS;

		// Sort players based on their killcount.
		scoreboard_SortPlayers( ST_KILLCOUNT );
	}
}
//*****************************************************************************
//
static void scoreboard_Prepare4ColumnDisplay( void )
{
	// Set all to empty.
	scoreboard_ClearColumns( );

	g_ulNumColumnsUsed = 4;
	g_pColumnHeaderFont = BigFont;

	// Set up the location of each column.
	g_aulColumnX[0] = 24;
	g_aulColumnX[1] = 84;
	g_aulColumnX[2] = 192;
	g_aulColumnX[3] = 256;
	
	// Build columns for modes in which players try to earn kills.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNKILLS )
	{
		scoreboard_SetColumnZeroToKillsAndSortPlayers();
		g_aulColumnType[1] = COLUMN_NAME;
		g_aulColumnType[2] = COLUMN_DEATHS;
		if ( NETWORK_InClientMode() )
			g_aulColumnType[2] = COLUMN_PING;
		g_aulColumnType[3] = COLUMN_TIME;
	}

	// Build columns for modes in which players try to earn frags.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNFRAGS )
	{
		g_aulColumnType[0] = COLUMN_FRAGS;
		g_aulColumnType[1] = COLUMN_NAME;
		g_aulColumnType[2] = COLUMN_DEATHS;
		if ( NETWORK_InClientMode() )
			g_aulColumnType[2] = COLUMN_PING;
		g_aulColumnType[3] = COLUMN_TIME;

		// Sort players based on their fragcount.
		scoreboard_SortPlayers( ST_FRAGCOUNT );
	}
	
	// Build columns for modes in which players try to earn points.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNPOINTS )
	{
//		if ( ctf || skulltag ) // Can have assists
//			g_aulColumnType[0] = COL_POINTSASSISTS;

		g_aulColumnType[0] = COLUMN_POINTS;
		g_aulColumnType[1] = COLUMN_NAME;
		g_aulColumnType[2] = COLUMN_DEATHS;
		if ( NETWORK_InClientMode() )
			g_aulColumnType[2] = COLUMN_PING;
		g_aulColumnType[3] = COLUMN_TIME;

		// Sort players based on their pointcount.
		scoreboard_SortPlayers( ST_POINTCOUNT );
	}

	// Build columns for modes in which players try to earn wins.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNWINS )
	{
		g_aulColumnType[0] = COLUMN_WINS;
		g_aulColumnType[1] = COLUMN_NAME;
		g_aulColumnType[2] = COLUMN_FRAGS;
		if ( NETWORK_InClientMode() )
			g_aulColumnType[2] = COLUMN_PING;
		g_aulColumnType[3] = COLUMN_TIME;

		// Sort players based on their wincount.
		scoreboard_SortPlayers( ST_WINCOUNT );
	}

}

//*****************************************************************************
//
static void scoreboard_Prepare3ColumnDisplay( void )
{
	// Set all to empty.
	scoreboard_ClearColumns( );

	g_ulNumColumnsUsed = 3;
	g_pColumnHeaderFont = SmallFont;

	// Set up the location of each column.
	g_aulColumnX[0] = 16;
	g_aulColumnX[1] = 96;
	g_aulColumnX[2] = 272;

	// All boards share these two columns. However, you can still deviant on these columns if you want.
	g_aulColumnType[1] = COLUMN_NAME;
	g_aulColumnType[2] = COLUMN_TIME;
	if ( NETWORK_InClientMode() )
		g_aulColumnType[2] = COLUMN_PING;

	// Build columns for modes in which players try to earn kills.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNKILLS )
	{
		scoreboard_SetColumnZeroToKillsAndSortPlayers();
	}

	// Build columns for modes in which players try to earn frags.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNFRAGS )
	{
		g_aulColumnType[0] = COLUMN_FRAGS;

		// Sort players based on their fragcount.
		scoreboard_SortPlayers( ST_FRAGCOUNT );
	}
	
	// Build columns for modes in which players try to earn points.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNPOINTS )
	{
//		if ( ctf || skulltag ) // Can have assists
//			g_aulColumnType[0] = COL_POINTSASSISTS;

		g_aulColumnType[0] = COLUMN_POINTS;

		// Sort players based on their pointcount.
		scoreboard_SortPlayers( ST_POINTCOUNT );
	}

	// Build columns for modes in which players try to earn wins.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNWINS )
	{
		g_aulColumnType[0] = COLUMN_WINS;

		// Sort players based on their wincount.
		scoreboard_SortPlayers( ST_WINCOUNT );
	}

}

//*****************************************************************************
//	These parameters are filters.
//	If 1, players with this trait will be skipped.
//	If 2, players *without* this trait will be skipped.
static void scoreboard_DoRankingListPass( ULONG ulPlayer, LONG lSpectators, LONG lDead, LONG lNotPlaying, LONG lNoTeam, LONG lWrongTeam, ULONG ulDesiredTeam )
{
	ULONG	ulIdx;
	ULONG	ulNumPlayers;

	ulNumPlayers = 0;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		// Skip or require players not in the game.
		if (((lNotPlaying == 1) && (playeringame[g_iSortedPlayers[ulIdx]] == false )) ||
			((lNotPlaying == 2) && (!playeringame[g_iSortedPlayers[ulIdx]] == false )))
			continue;

		// Skip or require players not on a team.
		 if(((lNoTeam == 1) && (!players[g_iSortedPlayers[ulIdx]].bOnTeam)) ||
			 ((lNoTeam == 2) && (players[g_iSortedPlayers[ulIdx]].bOnTeam)))
			continue;

		// Skip or require spectators.
		if (((lSpectators == 1) && PLAYER_IsTrueSpectator( &players[g_iSortedPlayers[ulIdx]])) ||
			((lSpectators == 2) && !PLAYER_IsTrueSpectator( &players[g_iSortedPlayers[ulIdx]])))
			continue;

		// In LMS, skip or require dead players.
		if( gamestate != GS_INTERMISSION ){
			/*(( lastmanstanding ) && (( LASTMANSTANDING_GetState( ) == LMSS_INPROGRESS ) || ( LASTMANSTANDING_GetState( ) == LMSS_WINSEQUENCE ))) ||
			(( survival ) && (( SURVIVAL_GetState( ) == SURVS_INPROGRESS ) || ( SURVIVAL_GetState( ) == SURVS_MISSIONFAILED )))*/
			
			// If we don't want to draw dead players, and this player is dead, skip this player.
			if (( lDead == 1 ) &&
				(( players[g_iSortedPlayers[ulIdx]].health <= 0 ) || ( players[g_iSortedPlayers[ulIdx]].bDeadSpectator )))
			{
				continue;
			}

			// If we don't want to draw living players, and this player is alive, skip this player.
			if (( lDead == 2 ) &&
				( players[g_iSortedPlayers[ulIdx]].health > 0 ) &&
				( players[g_iSortedPlayers[ulIdx]].bDeadSpectator == false ))
			{
				continue;
			}
		}

		// Skip or require players that aren't on this team.
		if (((lWrongTeam == 1) && (players[g_iSortedPlayers[ulIdx]].Team != ulDesiredTeam)) ||
			((lWrongTeam == 2) && (players[g_iSortedPlayers[ulIdx]].Team == ulDesiredTeam)))
			continue;

		scoreboard_RenderIndividualPlayer( ulPlayer, g_iSortedPlayers[ulIdx] );
		g_ulCurYPos += SmallFont->GetHeight( ) + 1;
		ulNumPlayers++;
	}

	if ( ulNumPlayers )
		g_ulCurYPos += SmallFont->GetHeight( ) + 1;
}

//*****************************************************************************
//
static void scoreboard_DrawRankings( ULONG ulPlayer )
{
	// Nothing to do.
	if ( g_ulNumColumnsUsed < 1 )
		return;

	g_ulCurYPos += 8;

	// Center this a little better in intermission
	if ( gamestate != GS_LEVEL )
		g_ulCurYPos = static_cast<LONG>( 48 * ( g_bScale ? g_fYScale : CleanYfac ));

	// Draw the titles for the columns.
	for ( ULONG ulColumn = 0; ulColumn < g_ulNumColumnsUsed; ulColumn++ )
		HUD_DrawText( g_pColumnHeaderFont, CR_RED, static_cast<LONG>( g_aulColumnX[ulColumn] * g_fXScale ), g_ulCurYPos, g_pszColumnHeaders[g_aulColumnType[ulColumn]] );

	// Draw the player list.
	g_ulCurYPos += 24;

	// Team-based games: Divide up the teams.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
	{
		// Draw players on teams.
		for ( ULONG ulTeamIdx = 0; ulTeamIdx < teams.Size( ); ulTeamIdx++ )
		{
			// In team LMS, separate the dead players from the living.
			if (( teamlms ) && ( gamestate != GS_INTERMISSION ) && ( LASTMANSTANDING_GetState( ) != LMSS_COUNTDOWN ) && ( LASTMANSTANDING_GetState( ) != LMSS_WAITINGFORPLAYERS ))
			{
				scoreboard_DoRankingListPass( ulPlayer, 1, 1, 1, 1, 1, ulTeamIdx ); // Living in this team
				scoreboard_DoRankingListPass( ulPlayer, 1, 2, 1, 1, 1, ulTeamIdx ); // Dead in this team
			}
			// Otherwise, draw all players all in one group.
			else
				scoreboard_DoRankingListPass( ulPlayer, 1, 0, 1, 1, 1, ulTeamIdx ); 

		}

		// Players that aren't on a team.
		scoreboard_DoRankingListPass( ulPlayer, 1, 1, 1, 2, 0, 0 ); 

		// Spectators are last.
		scoreboard_DoRankingListPass( ulPlayer, 2, 0, 1, 0, 0, 0 );
	}
	// Other modes: Just players and spectators.
	else
	{
		// [WS] Does the gamemode we are in use lives?
		// If so, dead players are drawn after living ones.
		if (( gamestate != GS_INTERMISSION ) && GAMEMODE_AreLivesLimited( ) && GAMEMODE_IsGameInProgress( ) )
		{
			scoreboard_DoRankingListPass( ulPlayer, 1, 1, 1, 0, 0, 0 ); // Living
			scoreboard_DoRankingListPass( ulPlayer, 1, 2, 1, 0, 0, 0 ); // Dead
		}
		// Othrwise, draw all active players in the game together.
		else
			scoreboard_DoRankingListPass( ulPlayer, 1, 0, 1, 0, 0, 0 );

		// Spectators are last.
		scoreboard_DoRankingListPass( ulPlayer, 2, 0, 1, 0, 0, 0 );
	}

	V_SetBorderNeedRefresh();
}

//*****************************************************************************
//
// [AK] scoreboard_ScanForColumn
//
// Scans for a column by name, throwing a fatal error if the column couldn't
// be found. This also has the option of throwing a fatal error if a data
// column must be returned, but the column that was found isn't one.
//
//*****************************************************************************

static ScoreColumn *scoreboard_ScanForColumn( FScanner &sc, const bool bMustBeDataColumn )
{
	sc.MustGetToken( TK_StringConst );

	// [AK] Throw a fatal error if an empty string was passed.
	if ( sc.StringLen == 0 )
		sc.ScriptError( "Got an empty string for a column name." );

	// [AK] Find a column from the main scoreboard object.
	ScoreColumn *pColumn = SCOREBOARD_GetColumn( sc.String );

	if ( pColumn == NULL )
		sc.ScriptError( "Column '%s' wasn't found.", sc.String );

	// [AK] Make sure that the pointer is of a DataScoreColumn object
	// (i.e. the template isn't unknown or a composite).
	if (( bMustBeDataColumn ) && ( pColumn->IsDataColumn( ) == false ))
		sc.ScriptError( "Column '%s' is not a data column.", sc.String );

	return pColumn;
}

//*****************************************************************************
//
// [AK] Scoreboard_TryPushingColumnToList
//
// Tries pushing a pointer to a column object into a list, but only if that
// pointer isn't in the list already. Returns true if successful, or false if
// it wasn't added to the list.
//
//*****************************************************************************

template<typename ColumnType>
static bool scoreboard_TryPushingColumnToList( FScanner &sc, TArray<ColumnType *> &ColumnList, ColumnType *pColumn, const char *pszColumnName )
{
	// [AK] Make sure the pointer to the column isn't NULL.
	if ( pColumn == NULL )
		return false;

	// [AK] Make sure that this column isn't already inside this list.
	for ( unsigned int i = 0; i < ColumnList.Size( ); i++ )
	{
		if ( ColumnList[i] == pColumn )
		{
			// [AK] Print an error message to let the user know the issue.
			if ( pszColumnName != NULL )
				sc.ScriptMessage( "Tried to put column '%s' into a list more than once.", pszColumnName );

			return false;
		}
	}

	ColumnList.Push( pColumn );
	return true;
}
