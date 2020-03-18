/*©mit**************************************************************************
*                                                                              *
* This file is part of FRIEND UNIFYING PLATFORM.                               *
* Copyright (c) Friend Software Labs AS. All rights reserved.                  *
*                                                                              *
* Licensed under the Source EULA. Please refer to the copy of the MIT License, *
* found in the file license_mit.txt.                                           *
*                                                                              *
*****************************************************************************©*/

#include "element_list.h"
#include "string.h"

/**
 * Parse string and return list of entries (int)
 *
 * @param str pointer to string
 * @return new pointer to root element IntListEl
 */

IntListEl *ILEParseString( char *str )
{
	if( str == NULL )
	{
		return NULL;
	}
	int i;
	char *startToken = str;
	char *curToken = str+1;
	
	IntListEl *rootEl = NULL;
	
	while( TRUE )
	{
		//printf("'%c'-'%d'   ===== ", *curToken, *curToken );
		if( *curToken == 0 || *curToken == ',' )
		{
			char *end;
			
			if( *curToken != 0 )
			{
				*curToken = 0;
			}
			curToken++;
			
			//printf("Entry found: %s\n", startToken );
			int64_t var = strtol( startToken, &end, 0 );
			IntListEl *el = FCalloc( 1, sizeof( IntListEl ) );
			if( el != NULL )
			{
				el->i_Data = var;
				el->node.mln_Succ = (MinNode *)rootEl;
				rootEl = el;
			}
			// do something here
		
			startToken = curToken;
		
			if( *curToken == 0 )
			{
				break;
			}
			curToken++;
		}
		else
		{
			curToken++;
		}
	}
	return rootEl;
}

/**
 * Parse string and return list of entries (unsigned int)
 *
 * @param str pointer to string
 * @return new pointer to root element IntListEl
 */

UIntListEl *UILEParseString( char *str )
{
	if( str == NULL )
	{
		return NULL;
	}
	int i;
	char *startToken = str;
	char *curToken = str+1;
	
	UIntListEl *rootEl = NULL;
	
	while( TRUE )
	{
		if( *curToken == 0 || *curToken == ',' )
		{
			char *end;
			
			if( *curToken != 0 )
			{
				*curToken = 0;
			}
			curToken++;
			
			//printf("Entry found: %s\n", startToken );
			uint64_t var = strtoul( startToken, &end, 0 );
			UIntListEl *el = FCalloc( 1, sizeof( UIntListEl ) );
			if( el != NULL )
			{
				el->i_Data = var;
				el->node.mln_Succ = (MinNode *)rootEl;
				rootEl = el;
			}
			// do something here
		
			startToken = curToken;
		
			if( *curToken == 0 )
			{
				break;
			}
			curToken++;
		}
		else
		{
			curToken++;
		}
	}
	return rootEl;
}

/**
 * Parse string and return list of entries (char *)
 *
 * @param str pointer to string
 * @return new pointer to root element IntListString
 */

StringListEl *SLEParseString( char *str )
{
	if( str == NULL )
	{
		return NULL;
	}
	int i;
	char *startToken = str;
	char *curToken = str+1;
	
	StringListEl *rootEl = NULL;
	
	while( TRUE )
	{
		if( *curToken == 0 || *curToken == ',' )
		{
			char *end;
			
			if( *curToken != 0 )
			{
				*curToken = 0;
			}
			
			curToken++;
			int64_t var = strtol( startToken, &end, 0 );
			StringListEl *el = FCalloc( 1, sizeof( StringListEl ) );
			if( el != NULL )
			{
				el->s_Data = StringDuplicate( startToken );
				el->node.mln_Succ = (MinNode *)rootEl;
				rootEl = el;
			}
			// do something here
		
			startToken = curToken;
		
			if( *curToken == 0 )
			{
				break;
			}
			curToken++;
		}
		else
		{
			curToken++;
		}
	}
	return rootEl;
}


