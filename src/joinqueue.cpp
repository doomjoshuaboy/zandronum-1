//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2004 Brad Carney
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
// Date created:  3/25/04
//
//
// Filename: joinqueue.cpp
//
// Description: Contains join queue routines
//
//-----------------------------------------------------------------------------

#include "announcer.h"
#include "c_dispatch.h"
#include "c_cvars.h"
#include "cl_demo.h"
#include "cooperative.h"
#include "deathmatch.h"
#include "doomstat.h"
#include "duel.h"
#include "g_game.h"
#include "g_level.h"
#include "gamemode.h"
#include "invasion.h"
#include "lastmanstanding.h"
#include "joinqueue.h"
#include "network.h"
#include "possession.h"
#include "survival.h"
#include "sv_commands.h"
#include "team.h"

//*****************************************************************************
//	VARIABLES

static	TArray<JOINSLOT_t> g_JoinQueue;
static	LONG		g_lClientQueuePosition;

//*****************************************************************************
//	FUNCTIONS

void JOINQUEUE_Construct( void )
{
	g_lClientQueuePosition = -1;

	// Initialize the join queue.
	JOINQUEUE_ClearList( );
}

//*****************************************************************************
//
void JOINQUEUE_RemovePlayerAtPosition ( ULONG ulPosition )
{
	if ( ulPosition < g_JoinQueue.Size() )
		g_JoinQueue.Delete( ulPosition );
}

//*****************************************************************************
//
void JOINQUEUE_RemovePlayerFromQueue ( ULONG ulPlayer, bool bBroadcast )
{
	// [BB] Invalid player.
	if ( ulPlayer >= MAXPLAYERS )
		return;

	LONG lJoinqueuePosition = JOINQUEUE_GetPositionInLine ( ulPlayer );
	if ( lJoinqueuePosition != -1 )
	{
		JOINQUEUE_RemovePlayerAtPosition ( lJoinqueuePosition );

		// If we're the server, tell everyone their new position in line
		// (but only if we are supposed to broadcast).
		if ( ( NETWORK_GetState( ) == NETSTATE_SERVER ) && bBroadcast )
			SERVERCOMMANDS_SetQueuePosition( );
	}
}

//*****************************************************************************
//
void JOINQUEUE_PlayerLeftGame( bool bWantPop )
{
	bool	bPop = true;

	// If we're in a duel, revert to the "waiting for players" state.
	// [BB] But only do so if there are less then two duelers left (probably JOINQUEUE_PlayerLeftGame was called mistakenly).
	if ( duel && ( DUEL_CountActiveDuelers( ) < 2 ) )
		DUEL_SetState( DS_WAITINGFORPLAYERS );

	// If only one (or zero) person is left, go back to "waiting for players".
	if ( lastmanstanding )
	{
		// Someone just won by default!
		if (( GAME_CountLivingAndRespawnablePlayers( ) == 1 ) && ( LASTMANSTANDING_GetState( ) == LMSS_INPROGRESS ))
		{
			LONG	lWinner;

			lWinner = LASTMANSTANDING_GetLastManStanding( );
			if ( lWinner != -1 )
			{
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVER_Printf( PRINT_HIGH, "%s \\c-wins!\n", players[lWinner].userinfo.GetName() );
				else
				{
					Printf( "%s \\c-wins!\n", players[lWinner].userinfo.GetName() );

					if ( lWinner == consoleplayer )
						ANNOUNCER_PlayEntry( cl_announcer, "YouWin" );
				}

				// Give the winner a win.
				PLAYER_SetWins( &players[lWinner], players[lWinner].ulWins + 1 );

				// Pause for five seconds for the win sequence.
				LASTMANSTANDING_DoWinSequence( lWinner );
			}

			// Join queue will be popped upon state change.
			bPop = false;

			GAME_SetEndLevelDelay( 5 * TICRATE );
		}
		else if ( SERVER_CalcNumNonSpectatingPlayers( MAXPLAYERS ) <= 1 )
			LASTMANSTANDING_SetState( LMSS_WAITINGFORPLAYERS );
	}

	if ( teamlms )
	{
		// Someone just won by default!
		if (( LASTMANSTANDING_GetState( ) == LMSS_INPROGRESS ) && LASTMANSTANDING_TeamsWithAlivePlayersOn( ) <= 1)
		{
			LONG	lWinner;

			lWinner = LASTMANSTANDING_TeamGetLastManStanding( );
			if ( lWinner != -1 )
			{
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVER_Printf( PRINT_HIGH, "%s \\c-wins!\n", TEAM_GetName( lWinner ));
				else
				{
					Printf( "%s \\c-wins!\n", TEAM_GetName( lWinner ));

					if ( players[consoleplayer].bOnTeam && ( lWinner == (LONG)players[consoleplayer].ulTeam ))
						ANNOUNCER_PlayEntry( cl_announcer, "YouWin" );
				}

				// Give the team a win.
				TEAM_SetWinCount( lWinner, TEAM_GetWinCount( lWinner ) + 1, false );

				// Pause for five seconds for the win sequence.
				LASTMANSTANDING_DoWinSequence( lWinner );
			}

			// Join queue will be popped upon state change.
			bPop = false;

			GAME_SetEndLevelDelay( 5 * TICRATE );
		}
		else if ( TEAM_TeamsWithPlayersOn( ) <= 1 )
			LASTMANSTANDING_SetState( LMSS_WAITINGFORPLAYERS );
	}

	// If we're in possession mode, revert to the "waiting for players" state
	// [BB] when there are less than two players now.
	if ( possession && ( SERVER_CalcNumNonSpectatingPlayers( MAXPLAYERS ) < 2 ))
		POSSESSION_SetState( PSNS_WAITINGFORPLAYERS );

	if ( teampossession && ( TEAM_TeamsWithPlayersOn( ) <= 1 ) )
		POSSESSION_SetState( PSNS_WAITINGFORPLAYERS );

	// If we're in invasion mode, revert to the "waiting for players" state.
	if ( invasion && ( SERVER_CalcNumNonSpectatingPlayers( MAXPLAYERS ) < 1 ))
		INVASION_SetState( IS_WAITINGFORPLAYERS );

	// If we're in survival co-op mode, revert to the "waiting for players" state.
	if ( survival && ( SERVER_CalcNumNonSpectatingPlayers( MAXPLAYERS ) < 1 ))
		SURVIVAL_SetState( SURVS_WAITINGFORPLAYERS );

	// Potentially let one person join the game.
	if ( bPop && bWantPop )
		JOINQUEUE_PopQueue( 1 );
}

//*****************************************************************************
//
void JOINQUEUE_SpectatorLeftGame( ULONG ulPlayer )
{
	// [BB] The only things we have to do if a spectator leaves the game are
	// to remove him from the queue and to notify the others about their updated
	// queue positions.
	JOINQUEUE_RemovePlayerFromQueue ( ulPlayer, true );
}

//*****************************************************************************
//
static void JOINQUEUE_CheckSanity()
{
	// [TP] This routine should never have to clean up anything but it's here to preserve data sanity in case.
	for ( unsigned int i = 0; i < g_JoinQueue.Size(); )
	{
		const JOINSLOT_t& slot = g_JoinQueue[i];
		if ( slot.ulPlayer >= MAXPLAYERS || playeringame[slot.ulPlayer] == false )
		{
			Printf( "Warning: Invalid join queue entry detected at position %d. Player idx: %lu\n", i, slot.ulPlayer );
			g_JoinQueue.Delete( i );
		}
		else
			++i;
	}
}

//*****************************************************************************
//
void JOINQUEUE_PopQueue( LONG lNumSlots )
{
	// [BB] Players are not allowed to join.
	if ( GAMEMODE_PreventPlayersFromJoining() )
		return;

	JOINQUEUE_CheckSanity();

	// Try to find the next person in line.
	for ( ULONG ulIdx = 0; ulIdx < g_JoinQueue.Size(); )
	{
		if ( lNumSlots == 0 )
			break;

		// [BB] Since we possibly just let somebody waiting in line join, check if more persons are allowed to join now.
		if ( GAMEMODE_PreventPlayersFromJoining() )
			break;

		const JOINSLOT_t& slot = g_JoinQueue[ulIdx];

		// Found a player waiting in line. They will now join the game!
		if ( playeringame[slot.ulPlayer] )
		{
			// [K6] Reset their AFK timer now - they may have been waiting in the queue silently and we don't want to kick them.
			player_t* player = &players[slot.ulPlayer];
			CLIENT_s* client = SERVER_GetClient( slot.ulPlayer );
			client->lLastActionTic = gametic;
			PLAYER_SpectatorJoinsGame ( player );

			// [BB/Spleen] The "lag interval" is only half of the "spectate info send" interval. Account for this here.
			if (( gametic - client->ulLastCommandTic ) <= 2*TICRATE )
				client->ulClientGameTic += ( gametic - client->ulLastCommandTic );

			if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
			{
				if ( TEAM_CheckIfValid ( slot.ulTeam ) )
					PLAYER_SetTeam( player, slot.ulTeam, true );
				else
					PLAYER_SetTeam( player, TEAM_ChooseBestTeamForPlayer( ), true );
			}

			// Begin the duel countdown.
			if ( duel )
			{
				// [BB] Skip countdown and map reset if the map is supposed to be a lobby.
				if ( GAMEMODE_IsLobbyMap( ) )
					DUEL_SetState( DS_INDUEL );
				else if ( sv_duelcountdowntime > 0 )
					DUEL_StartCountdown(( sv_duelcountdowntime * TICRATE ) - 1 );
				else
					DUEL_StartCountdown(( 10 * TICRATE ) - 1 );
			}
			// Begin the LMS countdown.
			else if ( lastmanstanding )
			{
				if ( sv_lmscountdowntime > 0 )
					LASTMANSTANDING_StartCountdown(( sv_lmscountdowntime * TICRATE ) - 1 );
				else
					LASTMANSTANDING_StartCountdown(( 10 * TICRATE ) - 1 );
			}
			else
			{
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVER_Printf( PRINT_HIGH, "%s \\c-joined the game.\n", player->userinfo.GetName() );
				else
					Printf( "%s \\c-joined the game.\n", player->userinfo.GetName() );
			}

			JOINQUEUE_RemovePlayerAtPosition ( ulIdx );

			if ( lNumSlots > 0 )
				lNumSlots--;
		}
		else
			ulIdx++;
	}

	// If we're the server, tell everyone their new position in line.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetQueuePosition( );
}

//*****************************************************************************
//
ULONG JOINQUEUE_AddPlayer( JOINSLOT_t JoinSlot )
{
	LONG idx = JOINQUEUE_GetPositionInLine( JoinSlot.ulPlayer );

	if ( idx >= 0 )
	{
		// Player is already in the queue!
		return idx;
	}

	return g_JoinQueue.Push( JoinSlot );
}

//*****************************************************************************
//
void JOINQUEUE_ClearList( void )
{
	g_JoinQueue.Clear();
}

//*****************************************************************************
//
LONG JOINQUEUE_GetPositionInLine( ULONG ulPlayer )
{
	if ( NETWORK_InClientMode() )
	{
		return ( g_lClientQueuePosition );
	}

	for ( unsigned int i = 0; i < g_JoinQueue.Size(); i++ )
	{
		if ( g_JoinQueue[i].ulPlayer == ulPlayer )
			return ( i );
	}

	return ( -1 );
}

//*****************************************************************************
//
void JOINQUEUE_SetClientPositionInLine( LONG lPosition )
{
	g_lClientQueuePosition = lPosition;
}

//*****************************************************************************
//
void JOINQUEUE_AddConsolePlayer( ULONG ulDesiredTeam )
{
	JOINSLOT_t	JoinSlot;
	JoinSlot.ulPlayer = consoleplayer;
	JoinSlot.ulTeam = ulDesiredTeam;
	const ULONG ulResult = JOINQUEUE_AddPlayer( JoinSlot );
	if ( ulResult == MAXPLAYERS )
		Printf( "Join queue full!\n" );
	else
		Printf( "Your position in line is: %d\n", static_cast<unsigned int> (ulResult + 1) );
}
//*****************************************************************************
//
void JOINQUEUE_PrintQueue( void )
{
	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		Printf ( "Only the server can print the join queue\n" );
		return;
	}

	JOINQUEUE_CheckSanity();

	bool bQueueEmpty = true;
	for ( ULONG ulIdx = 0; ulIdx < g_JoinQueue.Size(); ulIdx++ )
	{
		const JOINSLOT_t& slot = g_JoinQueue[ulIdx];
		player_t* pPlayer = &players[slot.ulPlayer];
		bQueueEmpty = false;
		Printf ( "%02lu - %s", ulIdx + 1, pPlayer->userinfo.GetName() );
		if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
			Printf ( " - %s", TEAM_CheckIfValid ( slot.ulTeam ) ? TEAM_GetName ( slot.ulTeam ) : "auto team selection" );
		Printf ( "\n" );
	}

	if ( bQueueEmpty )
		Printf ( "The join queue is empty\n" );
}

//*****************************************************************************
//
CCMD( printjoinqueue )
{
	JOINQUEUE_PrintQueue();
}
