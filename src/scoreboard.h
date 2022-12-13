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
// Filename: scoreboard.h
//
// Description: Contains scoreboard structures and prototypes
//
//-----------------------------------------------------------------------------

#ifndef __SCOREBOARD_H__
#define __SCOREBOARD_H__

#include <set>
#include "gamemode.h"

#include "scoreboard_enums.h"

//*****************************************************************************
//	DEFINES

// Maximum number of columns.
#define MAX_COLUMNS			8

//*****************************************************************************
//
// [AK] Column templates, either text-based, graphic-based, or composite.
//
enum COLUMNTEMPLATE_e
{
	COLUMNTEMPLATE_UNKNOWN,
	COLUMNTEMPLATE_TEXT,
	COLUMNTEMPLATE_GRAPHIC,
	COLUMNTEMPLATE_COMPOSITE,
};

//*****************************************************************************
enum
{
	COLUMN_EMPTY,
	COLUMN_NAME,
	COLUMN_TIME,
	COLUMN_PING,
	COLUMN_FRAGS,
	COLUMN_POINTS,
	COLUMN_WINS,
	COLUMN_KILLS,
	COLUMN_DEATHS,
	COLUMN_ASSISTS,
	COLUMN_SECRETS,

	NUM_COLUMN_TYPES
};

//*****************************************************************************
enum
{
	ST_FRAGCOUNT,
	ST_POINTCOUNT,
	ST_KILLCOUNT,
	ST_WINCOUNT,

	NUM_SORT_TYPES
};

//*****************************************************************************
//
// [AK] ColumnValue
//
// Allows for easy storage of different data types that a column might use.
//
//*****************************************************************************

class ColumnValue
{
public:
	ColumnValue( void ) : DataType( COLUMNDATA_UNKNOWN ) { }

	inline COLUMNDATA_e GetDataType( void ) const { return DataType; }
	template <typename T> T GetValue( void ) const;

	// Int data type.
	template <> int GetValue( void ) const { return DataType == COLUMNDATA_INT ? Int : 0; }
	void operator= ( int value ) { Int = value; DataType = COLUMNDATA_INT; }
	void operator= ( ULONG ulValue ) { Int = ulValue; DataType = COLUMNDATA_INT; }

	// Bool data type.
	template <> bool GetValue( void ) const { return DataType == COLUMNDATA_BOOL ? Bool : false; }
	void operator= ( bool value ) { Bool = value; DataType = COLUMNDATA_BOOL; }

	// Float data type.
	template <> float GetValue( void ) const { return DataType == COLUMNDATA_FLOAT ? Float : 0.0f; }
	void operator= ( float value ) { Float = value; DataType = COLUMNDATA_FLOAT; }

	// String data type.
	template <> const char *GetValue( void ) const { return DataType == COLUMNDATA_STRING ? String : NULL; }
	template <> FString GetValue( void ) const { return DataType == COLUMNDATA_STRING ? String : NULL; }
	void operator= ( const char *value ) { String = value; DataType = COLUMNDATA_STRING; }
	void operator= ( FString value ) { String = value.GetChars( ); DataType = COLUMNDATA_STRING; }

	// Color data type.
	template <> PalEntry GetValue( void ) const { return DataType == COLUMNDATA_COLOR ? Int : 0; }
	void operator= ( PalEntry value ) { Int = value; DataType = COLUMNDATA_COLOR; }

	// Texture data type.
	template <> FTexture *GetValue( void ) const { return DataType == COLUMNDATA_TEXTURE ? Texture : NULL; }
	void operator= ( FTexture *value ) { Texture = value; DataType = COLUMNDATA_TEXTURE; }

private:
	COLUMNDATA_e DataType;
	union
	{
		int Int;
		bool Bool;
		float Float;
		const char *String;
		FTexture *Texture;
	};
};

//*****************************************************************************
//
// [AK] ScoreColumn
//
// A base class for all column types (e.g. data or composite) that will appear
// on the scoreboard. Columns are responsible for updating themselves and
// drawing their contents when needed.
//
//*****************************************************************************

class ScoreColumn
{
public:
	ScoreColumn( const char *pszName );

	virtual COLUMNTEMPLATE_e GetTemplate( void ) const { return COLUMNTEMPLATE_UNKNOWN; }
	const char *GetDisplayName( void ) const { return DisplayName.GetChars( ); }
	const char *GetShortName( void ) const { return ShortName.GetChars( ); }
	FBaseCVar *GetCVar( void ) const { return pCVar; }
	ULONG GetFlags( void ) const { return ulFlags; }
	ULONG GetSizing( void ) const { return ulSizing; }
	ULONG GetShortestWidth( void ) const { return ulShortestWidth; }
	ULONG GetWidth( void ) const { return ulWidth; }
	LONG GetRelX( void ) const { return lRelX; }
	LONG GetAlignmentPosition( ULONG ulContentWidth ) const;
	bool IsDisabled( void ) const { return bDisabled; }
	bool IsHidden( void ) const { return bHidden; }
	bool ShouldUseShortName( void ) const { return bUseShortName; }
	void SetHidden( bool bEnable );
	void DrawHeader( FFont *pFont, const ULONG ulColor, const LONG lYPos, const ULONG ulHeight ) const;
	void DrawString( const char *pszString, FFont *pFont, const ULONG ulColor, const LONG lYPos, const ULONG ulHeight, const float fAlpha ) const;
	void DrawColor( const PalEntry color, const LONG lYPos, const ULONG ulHeight, const float fAlpha, const int clipWidth, const int clipHeight ) const;
	void DrawTexture( FTexture *pTexture, const LONG lYPos, const ULONG ulHeight, const float fAlpha, const int clipWidth, const int clipHeight ) const;

	virtual void Refresh( void );
	virtual void UpdateWidth( FFont *pHeaderFont, FFont *pRowFont );

protected:
	FString DisplayName;
	FString ShortName;
	COLUMNALIGN_e Alignment;
	FBaseCVar *pCVar;
	ULONG ulFlags;
	ULONG ulGameAndEarnTypeFlags;
	std::set<GAMEMODE_e> GameModeList;
	ULONG ulSizing;
	ULONG ulShortestWidth;
	ULONG ulWidth;
	LONG lRelX;
	bool bDisabled;
	bool bHidden;
	bool bUseShortName;
};

//*****************************************************************************
//	PROTOTYPES

ScoreColumn		*SCOREBOARD_GetColumn( FName Name );
bool			SCOREBOARD_ShouldDrawBoard( void );
void			SCOREBOARD_Render( ULONG ulDisplayPlayer );
void			SCOREBOARD_Refresh( void );
void			SCOREBOARD_ShouldRefreshBeforeRendering( void );
void			SCOREBOARD_BuildLimitStrings( std::list<FString> &lines, bool bAcceptColors );
LONG			SCOREBOARD_GetLeftToLimit( void );
void			SCOREBOARD_SetNextLevel( const char *pszMapName );

#endif // __SCOREBOARD_H__
