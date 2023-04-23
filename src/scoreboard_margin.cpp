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

#include "scoreboard.h"

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
