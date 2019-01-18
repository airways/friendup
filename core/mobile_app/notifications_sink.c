/*©mit**************************************************************************
*                                                                              *
* This file is part of FRIEND UNIFYING PLATFORM.                               *
* Copyright (c) Friend Software Labs AS. All rights reserved.                  *
*                                                                              *
* Licensed under the Source EULA. Please refer to the copy of the MIT License, *
* found in the file license_mit.txt.                                           *
*                                                                              *
*****************************************************************************©*/

#include "notifications_sink_websocket.h"
#include "mobile_app.h"
#include <pthread.h>
#include <util/hashmap.h>
#include <system/json/jsmn.h>
#include <system/systembase.h>
#include <system/user/user_mobile_app.h>

extern SystemBase *SLIB;

//static Hashmap *globalSocketAuthMap; //maps websockets to boolean values that are true then the websocket is authenticated
//static char *_auth_key;

#define WEBSOCKET_SEND_QUEUE

static FBOOL VerifyAuthKey( const char *key_name, const char *key_to_verify );

//
// Data Queue WSI Mutex
//

typedef struct DataQWSIM{
	struct lws			*d_Wsi;
	pthread_mutex_t		d_Mutex;
	FQueue				d_Queue;
	FBOOL				d_Authenticated;
}DataQWSIM;

static int ReplyError( DataQWSIM *d, int error_code);

int ProcessIncomingRequest( DataQWSIM *d, char *data, size_t len, void *udata );

int globalServerEntriesNr = 0;
char **globalServerEntries = NULL;

/**
 * Write message to websocket
 *
 * @param d pointer to DataQWSIM structure
 * @param msg pointer to string where message is stored
 * @param len length of message
 * @return number of bytes written to websocket
 */

static inline int WriteMessageSink( DataQWSIM *d, unsigned char *msg, int len )
{
	//MobileAppNotif *man = (MobileAppNotif *) mac->user_data;
	//if( man != NULL )
	{
		FQEntry *en = FCalloc( 1, sizeof( FQEntry ) );
		if( en != NULL )
		{
			DEBUG("Message added to queue: '%s'\n", msg );
			en->fq_Data = FMalloc( len+64+LWS_SEND_BUFFER_PRE_PADDING+LWS_SEND_BUFFER_POST_PADDING );
			memcpy( en->fq_Data+LWS_SEND_BUFFER_PRE_PADDING, msg, len );
			
			en->fq_Size = len;
			FERROR("\t\t\t\t\t\t\t\t\t\t\tSENDMESSSAGE\n<%s> size: %d\n\n\n\n", msg, len );
	
			//FQPushFIFO( &(man->man_Queue), en );
			//lws_callback_on_writable( mac->websocket_ptr );
			if( FRIEND_MUTEX_LOCK( &d->d_Mutex ) == 0 )
			{
				FQPushFIFO( &(d->d_Queue), en );
				lws_callback_on_writable( d->d_Wsi );
				FRIEND_MUTEX_UNLOCK( &(d->d_Mutex) );
			}
		}
	}
	return len;
}

/*
 * Error types
 */

enum {
	WS_NOTIF_SINK_SUCCESS = 0,
	WS_NOTIF_SINK_ERROR_BAD_JSON,
	WS_NOTIF_SINK_ERROR_WS_NOT_AUTHENTICATED,
	WS_NOTIF_SINK_ERROR_NOTIFICATION_TYPE_NOT_FOUND,
	WS_NOTIF_SINK_ERROR_AUTH_FAILED,
	WS_NOTIF_SINK_ERROR_NO_AUTH_ELEMENTS,
	WS_NOTIF_SINK_ERROR_PARAMETERS_NOT_FOUND,
	WS_NOTIF_SINK_ERROR_TOKENS_NOT_FOUND
};

/**
 * Main Websocket notification sink callback
 *
 * @param wsi pointer to Websocket connection
 * @param reason callback reason
 * @param user pointer to user data
 * @param in pointer to message
 * @param len size of provided message
 * @return 0 when everything is ok, otherwise return different value
 */
int WebsocketNotificationsSinkCallback( struct lws *wsi, int reason, void *user, void *in, size_t len )
{
	MobileAppNotif *man = (MobileAppNotif *)user;
	//DEBUG("notifications websocket callback, reason %d, len %zu, wsi %p\n", reason, len, wsi);
	
	switch( reason )
	{
		case LWS_CALLBACK_PROTOCOL_INIT:
		{

			return 0;
		}
		break;
	
		case LWS_CALLBACK_ESTABLISHED:
		{
			if( man != NULL )
			{
				DataQWSIM *locd = FCalloc( 1, sizeof( DataQWSIM ) );
				if( locd != NULL )
				{
					pthread_mutex_init( &locd->d_Mutex, NULL );
					locd->d_Wsi = wsi;
					memset( &(locd->d_Queue), 0, sizeof( locd->d_Queue ) );
					FQInit( &(locd->d_Queue) );
					locd->d_Authenticated = TRUE;
					man->man_Data = locd;
				}
			}
			return 0;
		}
		break;
	
		case LWS_CALLBACK_CLOSED:
		{
			MobileAppNotif *man = (MobileAppNotif *)user;
			if( man != NULL && man->man_Data != NULL )
			{
				DataQWSIM *d = (DataQWSIM *)man->man_Data;
				if( d != NULL )
				{
					//HashmapRemove( globalSocketAuthMap, websocketHash );
					pthread_mutex_destroy( &(d->d_Mutex) );
					FQDeInitFree( &(d->d_Queue) );
					FFree( d );
				}	
				man->man_Data = NULL;
			}
			return 0;
		}
		break;
		
		case LWS_CALLBACK_SERVER_WRITEABLE:
		{
#ifdef WEBSOCKET_SEND_QUEUE
			MobileAppNotif *man = (MobileAppNotif *)user;
			if( man != NULL && man->man_Data != NULL )
			{
				DataQWSIM *d = (DataQWSIM *)man->man_Data;
				FQEntry *e = NULL;
				FRIEND_MUTEX_LOCK( &d->d_Mutex );
				FQueue *q = &(d->d_Queue);
			
				//DEBUG("[websocket_app_callback] WRITABLE CALLBACK, q %p\n", q );
			
				if( ( e = FQPop( q ) ) != NULL )
				{
					FRIEND_MUTEX_UNLOCK( &d->d_Mutex );
					unsigned char *t = e->fq_Data+LWS_SEND_BUFFER_PRE_PADDING;
					t[ e->fq_Size+1 ] = 0;

					INFO("\t\t\t\t\t\t\t\t\t\t\tSENDMESSSAGE\n<%s> size: %d\n\n\n\n", e->fq_Data+LWS_SEND_BUFFER_PRE_PADDING, e->fq_Size );
					int res = lws_write( wsi, e->fq_Data+LWS_SEND_BUFFER_PRE_PADDING, e->fq_Size, LWS_WRITE_TEXT );
					//DEBUG("[websocket_app_callback] message sent: %s len %d\n", e->fq_Data, res );

					int v = lws_send_pipe_choked( wsi );
				
					if( e != NULL )
					{
						//DEBUG("Release: %p\n", e->fq_Data );
						if( e->fq_Data != NULL )
						{
							FFree( e->fq_Data );
							e->fq_Data = NULL;
						}
						FFree( e );
					}
				}
				else
				{
					//DEBUG("[websocket_app_callback] No message in queue\n");
					FRIEND_MUTEX_UNLOCK( &d->d_Mutex );
				}
			
				if( d != NULL && d->d_Queue.fq_First != NULL )
				{
					//DEBUG("We have message to send, calling writable\n");
					lws_callback_on_writable( wsi );
				}
			}
#endif
		}
		break;
		
		case LWS_CALLBACK_RECEIVE:
		{
			MobileAppNotif *man = (MobileAppNotif *)user;
			if( man != NULL && man->man_Data != NULL )
			{
				DataQWSIM *d = (DataQWSIM *)man->man_Data;
				int ret = ProcessIncomingRequest( d, (char*)in, len, user );
			}
		}
		break;
	}
	
	return 0;
}

/**
 * Process incoming request
 *
 * @param d pointer to DataQWSIM
 * @param data pointer to string where message is stored
 * @param len length of message
 * @return 0 when success, otherwise error number
 */

int ProcessIncomingRequest( DataQWSIM *d, char *data, size_t len, void *udata )
{
	DEBUG("Incoming notification request: <%*s>\n", (unsigned int)len, data);

	jsmn_parser parser;
	jsmn_init( &parser );
	jsmntok_t t[512]; //should be enough

	int tokens_found = jsmn_parse( &parser, data, len, t, sizeof(t)/sizeof(t[0]) );
	
	DEBUG( "Token found: %d", tokens_found );
	if( tokens_found < 1 )
	{
		return ReplyError( d, WS_NOTIF_SINK_ERROR_TOKENS_NOT_FOUND );
	}
	
	//
	//{ (0)
	// type (1): 'service' (2),
	// data (3): { (4)
	//    type (5): 'notification' (6),
	//    data : {
	//        <notie data>
	//		}
	// OR
	//type (1): 'authenticate' (2),
	//   serviceKey OR serviceName
	// OR
	//type (1): 'ping (2)
	//	}
	//}
	
	json_t json = { .string = data, .string_length = len, .token_count = tokens_found, .tokens = t };
	
	if( t[0].type == JSMN_OBJECT ) 
	{
		if( strncmp( data + t[1].start, "type", t[1].end - t[1].start) == 0) 
		{
			int msize = t[2].end - t[2].start;
			
			if( strncmp( data + t[2].start, "authenticate", msize ) == 0) 
			{
				int p;
				char *authKey = NULL;
				char *authName = NULL;
				// first check if service is already authenticated maybe?
				
				for( p = 5; p < 9 ; p++ )
				{
					int firstSize = t[p].end - t[p].start;

					if ( strncmp( data + t[p].start, "serviceKey", firstSize ) == 0 )
					{
						p++;
						int secondSize = t[p].end - t[p].start;
						authKey = FCalloc( secondSize + 16, sizeof(char) );
						strncpy( authKey, data + t[p].start, secondSize );
					}
					else if ( strncmp( data + t[p].start, "serviceName", firstSize ) == 0 )
					{
						p++;
						int secondSize = t[p].end - t[p].start;
						authName = FCalloc( secondSize + 16, sizeof(char) );
						strncpy( authName, data + t[p].start, secondSize );
					}
				}
				
				// we need both "serviceKey" and "serviceName"
				//json_get_element_string(&json, "key");
				if( authKey == NULL || authName == NULL )
				{
					if( authKey != NULL ){ FFree( authKey ); }
					if( authName != NULL ){ FFree( authName ); }
					return ReplyError( d, WS_NOTIF_SINK_ERROR_NO_AUTH_ELEMENTS );
				}
				
				if( VerifyAuthKey( authName, authKey ) == false )
				{
					FFree( authKey );
					FFree( authName );
					return ReplyError( d, WS_NOTIF_SINK_ERROR_AUTH_FAILED );
				}
				
				d->d_Authenticated = TRUE;
				
				char reply[ 256 ];
				int msize = snprintf( reply + LWS_PRE, sizeof(reply), "{ \"type\" : \"authenticate\", \"data\" : { \"status\" : 0 }}" );
				
#ifdef WEBSOCKET_SEND_QUEUE
				WriteMessageSink( d, (unsigned char *)(reply)+LWS_PRE, msize );
#else
				
				unsigned int json_message_length = strlen( reply + LWS_PRE );
				lws_write( wsi, (unsigned char*)reply+LWS_PRE, json_message_length, LWS_WRITE_TEXT );
#endif
				FFree( authKey );
				FFree( authName );
				
				return 0;
			}
			else if( d->d_Authenticated ) 
			{
				
				int dlen =  t[3].end - t[3].start;
				if( strncmp( data + t[2].start, "ping", msize ) == 0 && strncmp( data + t[3].start, "data", dlen ) == 0 ) 
				{
					DEBUG( "do Ping things\n" );

					char reply[ 128 ];
					int locmsglen = snprintf( reply + LWS_PRE, sizeof( reply ) ,"{ \"type\" : \"pong\", \"data\" : \"%.*s\" }", t[4].end-t[4].start,data + t[4].start );
#ifdef WEBSOCKET_SEND_QUEUE
					WriteMessageSink( d, (unsigned char *)reply+LWS_PRE, locmsglen );
#else
					unsigned int json_message_length = strlen( reply + LWS_PRE );
					lws_write( wsi, (unsigned char*)reply+LWS_PRE, json_message_length, LWS_WRITE_TEXT );				
#endif
				}
				else if( strncmp( data + t[2].start, "service", msize ) == 0 && strncmp( data + t[3].start, "data", dlen ) == 0 ) 
				{
					// check object type
				
					if( strncmp( data + t[5].start, "type", t[5].end - t[5].start) == 0) 
					{
						if( strncmp( data + t[6].start, "notification", t[6].end - t[6].start) == 0) 
						{
							// 6 notification, 7 data, 8 object, 9 variables
							
							DEBUG( "\n\nnotification \\o/\n" );
							int p;
							int notification_type = -1;
							//char *username = NULL;
							char *channel_id = NULL;
							char *title = NULL;
							char *message = NULL;
							char *application = NULL;
							char *extra = NULL;
							
							List *usersList = ListNew(); // list of users
							
							// 8 -> 25
							for( p = 8 ; p < tokens_found ; p++ )
							{
								int size = t[p].end - t[p].start;
								if( strncmp( data + t[p].start, "notification_type", size) == 0) 
								{
									p++;
									if( (t[p].end - t[p].start) >= 1 )
									{
										char *tmp = StringDuplicateN( data + t[p].start, t[p].end - t[p].start );
										if( tmp != NULL )
										{
											DEBUG("TYPE: %s\n", tmp );
											notification_type = atoi( tmp );
											FFree( tmp );
										}
									}
								}
								
								// username -  users
								// "users":[ "username", "pawel", ....]
								
								else if( strncmp( data + t[p].start, "users", size) == 0) 
								{
									DEBUG("Found array of users\n");
									p++;
									if( t[p].type == JSMN_ARRAY ) 
									{
										int j;
										int locsize = t[p].size;
										p++;
										for( j=0 ; j < locsize ; j++ )
										{
											char *username = StringDuplicateN( data + t[p].start, t[p].end - t[p].start );
											DEBUG("This user will get message: %s\n", username );
											ListAdd( usersList, username );
											p++;
										}
										p--;
									}
									
								}
								else if( strncmp( data + t[p].start, "channel_id", size) == 0) 
								{
									p++;
									channel_id = StringDuplicateN( data + t[p].start, t[p].end - t[p].start );
								}
								else if( strncmp( data + t[p].start, "title", size) == 0) 
								{
									p++;
									title = StringDuplicateN( data + t[p].start, t[p].end - t[p].start );
								}
								else if( strncmp( data + t[p].start, "message", size) == 0) 
								{
									p++;
									message = StringDuplicateN( data + t[p].start, t[p].end - t[p].start );
								}
								else if( strncmp( data + t[p].start, "application", size) == 0) 
								{
									p++;
									application = StringDuplicateN( data + t[p].start, t[p].end - t[p].start );
								}
								else if( strncmp( data + t[p].start, "extra", size) == 0) 
								{
									p++;
									extra = StringDuplicateN( data + t[p].start, t[p].end - t[p].start );
								}
							}
							
							if( notification_type >= 0 )
							{
								if( usersList == NULL || channel_id == NULL || title == NULL || message == NULL )
								{
									DEBUG( "channel_id: %s title: %s message: %s\n", channel_id, title , message );
									ListFree( usersList );
									if( channel_id != NULL ) FFree( channel_id );
									if( title != NULL ) FFree( title );
									if( message != NULL ) FFree( message );
									if( application != NULL ) FFree( application );
									if( extra != NULL ) FFree( extra );
									return ReplyError( d, WS_NOTIF_SINK_ERROR_PARAMETERS_NOT_FOUND );
								}
								
								List *le = usersList;
								while( le != NULL )
								{
									if( le->data != NULL )
									{
										int status = MobileAppNotifyUserRegister( SLIB, (char *)le->data, channel_id, application, title, message, (MobileNotificationTypeT)notification_type, extra );

										char reply[256];
										int msize = sprintf(reply + LWS_PRE, "{ \"type\" : \"service\", \"data\" : { \"type\" : \"notification\", \"data\" : { \"status\" : %d }}}", status);
#ifdef WEBSOCKET_SEND_QUEUE
										WriteMessageSink( d, (unsigned char *)reply+LWS_PRE, msize );
#else
										unsigned int json_message_length = strlen( reply + LWS_PRE );
										lws_write( wsi, (unsigned char*)reply+LWS_PRE, json_message_length, LWS_WRITE_TEXT );
#endif
									}
									
									le = le->next;
								}
							}
							else
							{
								return ReplyError( d, WS_NOTIF_SINK_ERROR_NOTIFICATION_TYPE_NOT_FOUND );
							}
							
							ListFree( usersList );
							
							//if( username != NULL ) FFree( username );
							if( channel_id != NULL ) FFree( channel_id );
							if( title != NULL ) FFree( title );
							if( message != NULL ) FFree( message );
							if( application != NULL ) FFree( application );
							if( extra != NULL ) FFree( extra );
						}
					}
				}	// is authenticated
				else
				{
					DEBUG( "Not authenticated! omg!!!" );
					return ReplyError( d, WS_NOTIF_SINK_ERROR_WS_NOT_AUTHENTICATED );
				}
			}
		}	// "type" in json
		else
		{
			return ReplyError( d, WS_NOTIF_SINK_ERROR_BAD_JSON );
		}
	}	// JSON OBJECT

	return 0;
}

/**
 * Set websocket notification key
 *
 * @param key pointer to new notification key
 */
void WebsocketNotificationsSetAuthKey( const char *key )
{
	unsigned int len = strlen(key);
	if( len < 10 )
	{ //effectively disable the whole module if the key is too weak or non-existent
		//_auth_key = NULL;
		DEBUG("Notifications key not set, the service will be disabled\n");
		return;
	}
	//_auth_key = FCalloc(len+1, sizeof(char));
	//strcpy(_auth_key, key);
	//DEBUG("Notifications key is <%s>\n", _auth_key);
}

/**
 * Reply error code to user
 *
 * @param d pointer to DataQWSIM
 * @param error_code error code
 * @return -1
 */
static int ReplyError( DataQWSIM *d, int error_code )
{
#ifdef WEBSOCKET_SEND_QUEUE
	char response[ LWS_PRE+64 ];
	int size = snprintf(response, sizeof(response), "{\"type\":\"error\",\"data\":{\"status\":%d}}", error_code);
	WriteMessageSink( d, (unsigned char *)response, size );
#else
	char response[ LWS_PRE+64 ];
	snprintf(response+LWS_PRE, sizeof(response)-LWS_PRE, "{\"type\":\"error\",\"data\":{\"status\":%d}}", error_code);
	DEBUG("Error response: %s\n", response+LWS_PRE);

	DEBUG("WSI %p\n", wsi);
	lws_write(wsi, (unsigned char*)response+LWS_PRE, strlen(response+LWS_PRE), LWS_WRITE_TEXT);

	WebsocketRemove(wsi);
#endif
	return error_code;
}

/**
 * Verify authentication key
 *
 * @param keyName name of the server key
 * @param keyToVerify pointer to string with key
 * @return true when key passed verification, otherwise false
 */
static FBOOL VerifyAuthKey( const char *keyName, const char *keyToVerify )
{
	DEBUG("VerifyAuthKey - keyName <%s> VerifyAuthKey - keyToVerify <%s>\n", keyName, keyToVerify );

	//TODO: verify against key name 
	if( keyName != NULL && keyToVerify != NULL )
	{
		int i;
		for( i = 0 ; i < SLIB->l_ServerKeysNum ; i++ )
		{
			if( SLIB->l_ServerKeys[i] != NULL && strcmp( keyName, SLIB->l_ServerKeys[i] ) == 0 )
			{
				if( SLIB->l_ServerKeyValues[i] != NULL && strcmp( SLIB->l_ServerKeyValues[i], keyToVerify) == 0 )
				{
					return TRUE;
				}
			}
		}
	}
	return false;
}
