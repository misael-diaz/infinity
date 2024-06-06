/*
VBL.C
Friday, August 21, 1992 7:06:54 PM

Tuesday, November 17, 1992 3:53:29 PM
	the new task of the vbl controller is only to move the player.  this is necessary for
	good control of the game.  everything else (doors, monsters, projectiles, etc) will
	be moved immediately before the next frame is drawn, based on delta-time values.
	collisions (including the player with walls) will also be handled at this time.
Thursday, November 19, 1992 1:27:23 AM
	the enumeration 'turning_head' had to be changed to '_turn_not_rotate' to make this
	file compile.  go figure.
Wednesday, December 2, 1992 2:31:05 PM
	the world doesn�t change while the mouse button is pressed.
Friday, January 15, 1993 11:19:11 AM
	the world doesn�t change after 14 ticks have passed without a screen refresh.
Friday, January 22, 1993 3:06:32 PM
	world_ticks was never being initialized to zero.  hmmm.
Saturday, March 6, 1993 12:23:48 PM
	at exit, we remove our vbl task.
Sunday, May 16, 1993 4:07:47 PM
	finally recoding everything
Monday, August 16, 1993 10:22:17 AM
	#ifdef CHARLES added.
Saturday, August 21, 1993 12:35:29 PM
	from pathways VBL_CONTROLLER.C.
Sunday, May 22, 1994 8:51:15 PM
	all the world physics has been moved into PHYSICS.C; all we do now is maintain and
	distribute a circular queue of keyboard flags (we're the keyboard_controller, not the
	movement_controller).
Thursday, June 2, 1994 12:55:52 PM
	gee, now we don�t even maintain the queue we just send our actions to PLAYER.C.
Tuesday, July 5, 1994 9:27:49 PM
	nuked most of the shit in here. changed the vbl task to a time
	manager task. the only functions from the old vbl.c that remain are precalculate_key_information()
	and parse_keymap().
Thursday, July 7, 1994 11:59:32 AM
	Added recording/replaying
Wednesday, August 10, 1994 2:44:57 PM
	added caching system for FSRead.
Friday, January 13, 1995 11:38:51 AM  (Jason')
	fixed the 'a' key getting blacklisted.
*/

#ifdef SUPPORT_INPUT_SPROCKET
#include "macintosh_cseries.h"
#else
#include "cseries.h"
#endif
#include <string.h>

#include "map.h"
#include "interface.h"
#include "mouse.h"
#include "player.h"
#include "key_definitions.h"
#include "tags.h"
#include "portable_files.h"
#include "vbl.h"
// for no particular reason, cseries is built with PPC alignment
#ifdef envppc
#pragma options align=power
#endif
#include "mytm.h"	// for ludicrous speed
#ifdef envppc
#pragma options align=reset
#endif

#ifdef SUPPORT_INPUT_SPROCKET
#include "InputSprocket.h"
extern ISpElementReference *input_sprocket_elements;
#include "macintosh_input.h"
#include "shell.h"
#endif

#ifdef mpwc
#pragma segment input
#endif

/* ---------- constants */

#define RECORD_CHUNK_SIZE            (MAXIMUM_QUEUE_SIZE/2)
#define END_OF_RECORDING_INDICATOR  (RECORD_CHUNK_SIZE+1)
#define MAXIMUM_TIME_DIFFERENCE     15 // allowed between heartbeat_count and dynamic_world->tick_count
#define MAXIMUM_NET_QUEUE_SIZE       8
#define DISK_CACHE_SIZE             ((sizeof(short)+sizeof(long))*100)
#define MAXIMUM_REPLAY_SPEED         5
#define MINIMUM_REPLAY_SPEED        -5

/* ---------- macros */

#define INCREMENT_QUEUE_COUNTER(c) { (c)++; if ((c)>=MAXIMUM_QUEUE_SIZE) (c) = 0; }

/* ---------- structures */
#include "vbl_definitions.h"

/* ---------- globals */

static long heartbeat_count;
static boolean input_task_active;
static timer_task_proc input_task;

/* Not static because vbl_macintosh.c uses this.. */
struct key_definition current_key_definitions[NUMBER_OF_STANDARD_KEY_DEFINITIONS];

struct replay_private_data replay;

#ifdef DEBUG
ActionQueue *get_player_recording_queue(
	short player_index)
{
	assert(replay.recording_queues);
	assert(player_index>=0 && player_index<MAXIMUM_NUMBER_OF_PLAYERS);
	
	return (replay.recording_queues+player_index);
}
#endif

/* ---------- private prototypes */
static void remove_input_controller(void);
static void precalculate_key_information(void);
static void save_recording_queue_chunk(short player_index);
static void read_recording_queue_chunks(void);
static boolean pull_flags_from_recording(short count);
static FileError vblFSRead(short refnum, long *count, void *dest);
static void record_action_flags(short player_identifier, long *action_flags, short count);
static short get_recording_queue_size(short which_queue);

// #define DEBUG_REPLAY

#ifdef DEBUG_REPLAY
static void open_stream_file(void);
static void debug_stream_of_flags(long action_flag, short player_index);
static void close_stream_file(void);
#endif

/* ---------- code */
void initialize_keyboard_controller(
	void)
{
	ActionQueue *queue;
	short player_index;
	
//	vassert(NUMBER_OF_KEYS == NUMBER_OF_STANDARD_KEY_DEFINITIONS,
//		csprintf(temporary, "NUMBER_OF_KEYS == %d, NUMBER_OF_KEY_DEFS = %d. Not Equal!", NUMBER_OF_KEYS, NUMBER_OF_STANDARD_KEY_DEFINITIONS));
	assert(NUMBER_OF_STANDARD_KEY_DEFINITIONS==NUMBER_OF_LEFT_HANDED_KEY_DEFINITIONS);
	assert(NUMBER_OF_LEFT_HANDED_KEY_DEFINITIONS==NUMBER_OF_POWERBOOK_KEY_DEFINITIONS);
	
	// get globals initialized
	heartbeat_count= 0;
	input_task_active= FALSE;
	memset(&replay, 0, sizeof(struct replay_private_data));

	input_task= install_timer_task(TICKS_PER_SECOND, input_controller);
	assert(input_task);
	
	atexit(remove_input_controller);
	set_keys_to_match_preferences();
	
	/* Allocate the recording queues */	
	replay.recording_queues = (ActionQueue *) malloc(MAXIMUM_NUMBER_OF_PLAYERS * sizeof(ActionQueue));
	assert(replay.recording_queues);
	if(!replay.recording_queues) alert_user(fatalError, strERRORS, outOfMemory, memory_error());
	
	/* Allocate the individual ones */
	for (player_index= 0; player_index<MAXIMUM_NUMBER_OF_PLAYERS; player_index++)
	{
		queue= get_player_recording_queue(player_index);
		queue->read_index= queue->write_index = 0;
		queue->buffer= (long *) malloc(MAXIMUM_QUEUE_SIZE*sizeof(long));
		if(!queue->buffer) alert_user(fatalError, strERRORS, outOfMemory, memory_error());
	}
	enter_mouse(0);
	
	return;
}

void toggle_ludicrous_speed(
	boolean ludicrous_speed)
{
	if (input_task_active && input_task)
	{
		((myTMTaskPtr)input_task)->period=
			((ludicrous_speed && (((myTMTaskPtr)input_task)->period == 1000/TICKS_PER_SECOND))?
				450/TICKS_PER_SECOND : 1000/TICKS_PER_SECOND);
	}
}

void set_keyboard_controller_status(
	boolean active)
{
#ifdef SUPPORT_INPUT_SPROCKET
	long int itr;
#endif

	// if already set then drop out
	if (input_task_active == active) { return; }

	input_task_active= active;
#ifdef SUPPORT_INPUT_SPROCKET
	if (use_input_sprocket)
	{
		for(itr = 0; itr < NUMBER_OF_INPUT_SPROCKET_NEEDS; itr++)
		{
			ISpElement_Flush(input_sprocket_elements[itr]);
		}
		
		active ? (void) ISpResume() : (void) ISpSuspend();
	}
#endif
	return;
}

boolean get_keyboard_controller_status(
	void)
{
	return input_task_active;
}

long get_heartbeat_count(
	void)
{
	return heartbeat_count;
}

void sync_heartbeat_count(
	void)
{
	heartbeat_count= dynamic_world->tick_count;

	return;
}

void increment_replay_speed(
	void)
{
	if (replay.replay_speed < MAXIMUM_REPLAY_SPEED) replay.replay_speed++;

	return;
}

void decrement_replay_speed(
	void)
{
	if (replay.replay_speed > MINIMUM_REPLAY_SPEED) replay.replay_speed--;

	return;
}

/* Returns NONE if it is custom.. */
short find_key_setup(
	short *keycodes)
{
	short index, jj;
	short key_setup= NONE;
	
	for (index= 0; key_setup==NONE && index<NUMBER_OF_KEY_SETUPS; index++)
	{
		struct key_definition *definition = all_key_definitions[index];

		for (jj= 0; jj<NUMBER_OF_STANDARD_KEY_DEFINITIONS; jj++)
		{
			if (definition[jj].offset != keycodes[jj]) break;
		}

		if (jj==NUMBER_OF_STANDARD_KEY_DEFINITIONS)
		{
			key_setup= index;
		}
	}
	
	return key_setup;
}

void set_default_keys(
	short *keycodes, 
	short which_default)
{
	short i;
	struct key_definition *definitions;
	
	assert(which_default >= 0 && which_default < NUMBER_OF_KEY_SETUPS);
	definitions= all_key_definitions[which_default];
	for (i= 0; i < NUMBER_OF_STANDARD_KEY_DEFINITIONS; i++)
	{
		keycodes[i] = definitions[i].offset;
	}
}

void set_keys(
	short *keycodes)
{
	short index;
	struct key_definition *definitions;
	
	/* all of them have the same ordering, so which one we pick is moot. */
	definitions = all_key_definitions[_standard_keyboard_setup]; 
	
	for (index= 0; index<NUMBER_OF_STANDARD_KEY_DEFINITIONS; index++)
	{
		current_key_definitions[index].offset= keycodes[index];
		current_key_definitions[index].action_flag= definitions[index].action_flag;
		assert(current_key_definitions[index].offset <= 0x7f);
	}
	precalculate_key_information();
	
	return;
}

boolean has_recording_file(
	void)
{
	FileDesc spec;

	return get_recording_filedesc(&spec);
}

/* Called by the time manager task in vbl_macintosh.c */
boolean input_controller(
	void)
{
	if (input_task_active)
	{
		if((heartbeat_count-dynamic_world->tick_count) < MAXIMUM_TIME_DIFFERENCE)
		{
			if (game_is_networked) // input from network
			{
				; // all handled elsewhere now. (in network.c)
			}
			else if (replay.game_is_being_replayed) // input from recorded game file
			{
				static short phase= 0; /* When this gets to 0, update the world */

				/* Minimum replay speed is a pause. */
				if(replay.replay_speed != MINIMUM_REPLAY_SPEED)
				{
					if (replay.replay_speed > 0 || (--phase<=0))
					{
						short flag_count= MAX(replay.replay_speed, 1);
					
						if (!pull_flags_from_recording(flag_count)) // oops. silly me.
						{
							if (replay.have_read_last_chunk)
							{
								assert(get_game_state()==_game_in_progress || get_game_state()==_switch_demo);
								set_game_state(_switch_demo);
							}
						}
						else
						{	
							/* Increment the heartbeat.. */
							heartbeat_count+= flag_count;
						}
	
						/* Reset the phase-> doesn't matter if the replay speed is positive */					
						/* +1 so that replay_speed 0 is different from replay_speed 1 */
						phase= -(replay.replay_speed) + 1;
					}
				}
			}
			else // then getting input from the keyboard/mouse
			{
				long action_flags= parse_keymap();
				
				process_action_flags(local_player_index, &action_flags, 1);
				heartbeat_count++; // ba-doom
			}
		} else {
// dprintf("Out of phase.. (%d);g", heartbeat_count - dynamic_world->tick_count);
		}
	}
	
	return TRUE; // tells the time manager library to reschedule this task
}

void process_action_flags(
	short player_identifier, 
	long *action_flags, 
	short count)
{
	if (replay.game_is_being_recorded)
	{
		record_action_flags(player_identifier, action_flags, count);
	}

	queue_action_flags(player_identifier, action_flags, count);
}

static void record_action_flags(
	short player_identifier, 
	long *action_flags, 
	short count)
{
	short index;
	ActionQueue  *queue;
	
	queue= get_player_recording_queue(player_identifier);
	assert(queue && queue->write_index >= 0 && queue->write_index < MAXIMUM_QUEUE_SIZE);
	for (index= 0; index<count; index++)
	{
		*(queue->buffer + queue->write_index) = *action_flags++;
		INCREMENT_QUEUE_COUNTER(queue->write_index);
		if (queue->write_index == queue->read_index)
		{
			dprintf("blew recording queue for player %d", player_identifier);
		}
	}
}

/*********************************************************************************************
 *
 * Function: save_recording_queue_chunk
 * Purpose:  saves one chunk of the queue to the recording file, using run-length encoding.
 *
 *********************************************************************************************/
void save_recording_queue_chunk(
	short player_index)
{
	long *location;
	long last_flag, count, flag = 0;
	short i, run_count, num_flags_saved, max_flags;
	static long *buffer= NULL;
	ActionQueue *queue;

	if (buffer == NULL)
	{
		buffer = (long *)malloc((RECORD_CHUNK_SIZE * sizeof(long)) + RECORD_CHUNK_SIZE * sizeof(short));
	}
	
	location= buffer;
	count= 0; // keeps track of how many bytes we'll save.
	last_flag= NONE;

	queue= get_player_recording_queue(player_index);
	
	// don't want to save too much stuff
	max_flags= MIN(RECORD_CHUNK_SIZE, get_recording_queue_size(player_index)); 

	// save what's in the queue
	run_count= num_flags_saved= 0;
	for (i = 0; i<max_flags; i++)
	{
		flag = *(queue->buffer + queue->read_index);
		INCREMENT_QUEUE_COUNTER(queue->read_index);
		
		if (i && flag != last_flag)
		{
			*(short*)location = run_count;
			((short*)location)++;
			*location++ = last_flag;
			count += sizeof(short) + sizeof(long);
			num_flags_saved += run_count;
			run_count = 1;
		}
		else
		{
			run_count++;
		}
		last_flag = flag;
	}
	
	// now save the final run
	*(short*)location = run_count;
	((short*)location)++;
	*location++ = last_flag;
	count += sizeof(short) + sizeof(long);
	num_flags_saved += run_count;
	
	if (max_flags<RECORD_CHUNK_SIZE)
	{
		*(short*)location = END_OF_RECORDING_INDICATOR;
		((short*)location)++;
		*location++ = 0;
		count += sizeof(short) + sizeof(long);
		num_flags_saved += RECORD_CHUNK_SIZE-max_flags;
	}
	
	write_file(replay.recording_file_refnum, count, buffer);
	replay.header.length+= count;
		
	vwarn(num_flags_saved == RECORD_CHUNK_SIZE,
		csprintf(temporary, "bad recording: %d flags, max=%d, count = %d;dm #%d #%d", num_flags_saved, max_flags, 
			count, buffer, count));
}

/*********************************************************************************************
 *
 * Function: pull_flags_from_recording
 * Purpose:  remove one flag from each queue from the recording buffer.
 * Returns:  TRUE if it pulled the flags, FALSE if it didn't
 *
 *********************************************************************************************/
static boolean pull_flags_from_recording(
	short count)
{
	short player_index;
	boolean success= TRUE;
	
	// first check that we can pull something from each player�s queue
	// (at the beginning of the game, we won�t be able to)
	// i'm not sure that i really need to do this check. oh well.
	for (player_index = 0; success && player_index<dynamic_world->player_count; player_index++)
	{
		if(get_recording_queue_size(player_index)==0) success= FALSE;
	}

	if(success)
	{
		for (player_index = 0; player_index < dynamic_world->player_count; player_index++)
		{
			short index;
			ActionQueue  *queue;
		
			queue= get_player_recording_queue(player_index);
			for (index= 0; index<count; index++)
			{
				if (queue->read_index != queue->write_index)
				{
#ifdef DEBUG_REPLAY
					debug_stream_of_flags(*(queue->buffer+queue->read_index), player_index);
#endif
					queue_action_flags(player_index, queue->buffer+queue->read_index, 1);
					INCREMENT_QUEUE_COUNTER(queue->read_index);
				} else {
dprintf("Dropping flag?");
				}
			}
		}
	}
	
	return success;
}

static short get_recording_queue_size(
	short which_queue)
{
	short size;
	ActionQueue *queue= get_player_recording_queue(which_queue);

	/* Note that this is a circular queue */
	size= queue->write_index-queue->read_index;
	if(size<0) size+= MAXIMUM_QUEUE_SIZE;
	
	return size;
}

static void precalculate_key_information(
	void)
{
	short i;
	
	/* convert raw key codes to offets and masks */
	for (i = 0; i < NUMBER_OF_STANDARD_KEY_DEFINITIONS; ++i)
	{
		current_key_definitions[i].mask = 1 << (current_key_definitions[i].offset&7);
		current_key_definitions[i].offset >>= 3;
	}
	
	return;
}

void set_recording_header_data(
	short number_of_players, 
	short level_number, 
	unsigned long map_checksum,
	short version, 
	struct player_start_data *starts, 
	struct game_data *game_information)
{
	assert(!replay.valid);
	memset(&replay.header, 0, sizeof(struct recording_header));
	replay.header.num_players= number_of_players;
	replay.header.level_number= level_number;
	replay.header.map_checksum= map_checksum;
	replay.header.version= version;
	memcpy(replay.header.starts, starts, MAXIMUM_NUMBER_OF_PLAYERS*sizeof(struct player_start_data));
	memcpy(&replay.header.game_information, game_information, sizeof(struct game_data));
	replay.header.length= sizeof(struct recording_header);

	return;
}

void get_recording_header_data(
	short *number_of_players, 
	short *level_number, 
	unsigned long *map_checksum,
	short *version, 
	struct player_start_data *starts, 
	struct game_data *game_information)
{
	assert(replay.valid);
	*number_of_players= replay.header.num_players;
	*level_number= replay.header.level_number;
	*map_checksum= replay.header.map_checksum;
	*version= replay.header.version;
	memcpy(starts, replay.header.starts, MAXIMUM_NUMBER_OF_PLAYERS*sizeof(struct player_start_data));
	memcpy(game_information, &replay.header.game_information, sizeof(struct game_data));
	
	return;
}

boolean setup_for_replay_from_file(
	FileDesc *file,
	unsigned long map_checksum)
{
	boolean successful= FALSE;

#pragma unused(map_checksum)
	replay.recording_file_refnum= open_file_for_reading(file);
	if(replay.recording_file_refnum > 0)
	{
		replay.valid= TRUE;
		replay.have_read_last_chunk = FALSE;
		replay.game_is_being_replayed = TRUE;
		assert(!replay.resource_data);
		replay.resource_data= NULL;
		replay.resource_data_size= 0l;
		replay.film_resource_offset= NONE;
		
		read_file(replay.recording_file_refnum, sizeof(struct recording_header), &replay.header);
	
		/* Set to the mapfile this replay came from.. */
		if(use_map_file(replay.header.map_checksum))
		{
			replay.fsread_buffer= (char *)malloc(DISK_CACHE_SIZE); 
			assert(replay.fsread_buffer);
			if(!replay.fsread_buffer) alert_user(fatalError, strERRORS, outOfMemory, memory_error());
			
			replay.location_in_cache= NULL;
			replay.bytes_in_cache= 0;
			replay.replay_speed= 1;
			
#ifdef DEBUG_REPLAY
			open_stream_file();
#endif
			successful= TRUE;
		} else {
			/* Tell them that this map wasn't found.  They lose. */
			alert_user(infoError, strERRORS, cantFindReplayMap, 0);
		}
	}
	
	return successful;
}

/* Note that we _must_ set the header information before we start recording!! */
void start_recording(
	void)
{
	FileDesc recording_file;
	long count;
	FileError error;
	
	assert(!replay.valid);
	replay.valid= TRUE;
	
	if(get_recording_filedesc(&recording_file))
	{
		delete_file(&recording_file);
	}

	error= create_file(&recording_file, FILM_FILE_TYPE);	
	if(!error)
	{
		/* I debate the validity of fsCurPerm here, but Alain had it, and it has been working */
		replay.recording_file_refnum= open_file_for_writing(&recording_file);
		if(replay.recording_file_refnum != NONE)
		{
			replay.game_is_being_recorded= TRUE;
	
			// save a header containing information about the game.
			count= sizeof(struct recording_header); 
			write_file(replay.recording_file_refnum, count, &replay.header);
		}
	}
}

void stop_recording(
	void)
{
	if (replay.game_is_being_recorded)
	{
		short player_index;
		long count;
		long total_length;
		FileError error;

		assert(replay.valid);
		for (player_index= 0; player_index<dynamic_world->player_count; player_index++)
		{
			save_recording_queue_chunk(player_index);
		}

		replay.game_is_being_recorded = FALSE;
		
		/* Rewrite the header, since it has the new length */
		set_fpos(replay.recording_file_refnum, 0l); 
		count= sizeof(struct recording_header);
		error= write_file(replay.recording_file_refnum, count, &replay.header);
		assert(!error);

		total_length= get_file_length(replay.recording_file_refnum);
		assert(total_length==replay.header.length);
		
		close_file(replay.recording_file_refnum);
	}

	replay.valid= FALSE;
	
	return;
}

void rewind_recording(
	void)
{
	if(replay.game_is_being_recorded)
	{
		/* This is unnecessary, because it is called from reset_player_queues, */
		/* which is always called from revert_game */
		set_eof(replay.recording_file_refnum, sizeof(struct recording_header));
		set_fpos(replay.recording_file_refnum, sizeof(struct recording_header));

		replay.header.length= sizeof(struct recording_header);
	}
	
	return;
}

void check_recording_replaying(
	void)
{
	short player_index, queue_size;

	if (replay.game_is_being_recorded)
	{
		boolean enough_data_to_save= TRUE;
	
		// it's time to save the queues if all of them have >= RECORD_CHUNK_SIZE flags in them.
		for (player_index= 0; enough_data_to_save && player_index<dynamic_world->player_count; player_index++)
		{
			queue_size= get_recording_queue_size(player_index);
			if (queue_size < RECORD_CHUNK_SIZE)	enough_data_to_save= FALSE;
		}
		
		if(enough_data_to_save)
		{
			boolean success;
			unsigned long freespace;
			FileDesc recording_file;
			
			get_recording_filedesc(&recording_file);

			success= get_freespace_on_disk(&recording_file, &freespace);
			if (success && freespace>(RECORD_CHUNK_SIZE*sizeof(short)*sizeof(long)*dynamic_world->player_count))
			{
				for (player_index= 0; player_index<dynamic_world->player_count; player_index++)
				{
					save_recording_queue_chunk(player_index);
				}
			}
		}
	}
	else if (replay.game_is_being_replayed)
	{
		boolean load_new_data= TRUE;
	
		// it's time to refill the requeues if they all have < RECORD_CHUNK_SIZE flags in them.
		for (player_index= 0; load_new_data && player_index<dynamic_world->player_count; player_index++)
		{
			queue_size= get_recording_queue_size(player_index);
			if(queue_size>= RECORD_CHUNK_SIZE) load_new_data= FALSE;
		}
		
		if(load_new_data)
		{
			// at this point, we�ve determined that the queues are sufficently empty, so
			// we�ll fill �em up.
			read_recording_queue_chunks();
		}
	}

	return;
}

void reset_recording_and_playback_queues(
	void)
{
	short index;
	
	for(index= 0; index<MAXIMUM_NUMBER_OF_PLAYERS; ++index)
	{
		replay.recording_queues[index].read_index= replay.recording_queues[index].write_index= 0;
	}
}

void stop_replay(
	void)
{
	assert(replay.valid);
	if (replay.game_is_being_replayed)
	{
		replay.game_is_being_replayed= FALSE;
		if (replay.resource_data)
		{
			free(replay.resource_data);
			replay.resource_data= NULL;
		}
		else
		{
			close_file(replay.recording_file_refnum);
			assert(replay.fsread_buffer);
			free(replay.fsread_buffer);
		}
#ifdef DEBUG_REPLAY
		close_stream_file();
#endif
	}

	/* Unecessary, because reset_player_queues calls this. */
	replay.valid= FALSE;
	
	return;
}

static void read_recording_queue_chunks(
	void)
{
	long i, sizeof_read, action_flags;
	short count, player_index, num_flags;
	ActionQueue *queue;
	short error;
	
	for (player_index = 0; player_index < dynamic_world->player_count; player_index++)
	{
		queue= get_player_recording_queue(player_index);
		for (count = 0; count < RECORD_CHUNK_SIZE; )
		{
			if (replay.resource_data)
			{
				boolean hit_end= FALSE;
				
				if (replay.film_resource_offset >= replay.resource_data_size)
				{
					hit_end = TRUE;
				}
				else
				{
					num_flags = * (short *) (replay.resource_data + replay.film_resource_offset);
					replay.film_resource_offset += sizeof(num_flags);
					action_flags = *(long *) (replay.resource_data + replay.film_resource_offset);
					replay.film_resource_offset+= sizeof(action_flags);
				}
				
				if (hit_end || num_flags == END_OF_RECORDING_INDICATOR)
				{
					replay.have_read_last_chunk= TRUE;
					break;
				}
			}
			else
			{
				sizeof_read = sizeof(num_flags);
				error= vblFSRead(replay.recording_file_refnum, &sizeof_read, &num_flags);
				if (!error)
				{
					sizeof_read = sizeof(action_flags);
					error= vblFSRead(replay.recording_file_refnum, &sizeof_read, &action_flags);
					assert(!error || (error == errHitFileEOF && sizeof_read == sizeof(action_flags)));
				}
				
				if ((error == errHitFileEOF && sizeof_read != sizeof(long)) || num_flags == END_OF_RECORDING_INDICATOR)
				{
					replay.have_read_last_chunk = TRUE;
					break;
				}
			}
			assert(replay.have_read_last_chunk || num_flags);
			count += num_flags;
			vassert((num_flags != 0 && count <= RECORD_CHUNK_SIZE) || replay.have_read_last_chunk, 
				csprintf(temporary, "num_flags = %d, count = %d", num_flags, count));
			for (i = 0; i < num_flags; i++)
			{
				*(queue->buffer + queue->write_index) = action_flags;
				INCREMENT_QUEUE_COUNTER(queue->write_index);
				assert(queue->read_index != queue->write_index);
			}
		}
		assert(replay.have_read_last_chunk || count == RECORD_CHUNK_SIZE);
	}

	return;
}

/* This is gross, (Alain wrote it, not me!) but I don't have time to clean it up */
static FileError vblFSRead(short 
	refnum, 
	long *count, 
	void *dest)
{
	long fsread_count;
	FileError error= 0;
	
	assert(replay.fsread_buffer);

	if (replay.bytes_in_cache < *count)
	{
		assert(replay.bytes_in_cache + *count < DISK_CACHE_SIZE);
		if (replay.bytes_in_cache)
		{
			memcpy(replay.fsread_buffer, replay.location_in_cache, replay.bytes_in_cache);
		}
		replay.location_in_cache = replay.fsread_buffer;
		fsread_count= DISK_CACHE_SIZE - replay.bytes_in_cache;
		assert(fsread_count > 0);
		error= read_file(refnum, fsread_count, replay.fsread_buffer+replay.bytes_in_cache);
		if(!error) replay.bytes_in_cache += fsread_count;
	}

	if (error == errHitFileEOF && replay.bytes_in_cache < *count)
	{
		*count= replay.bytes_in_cache;
	}
	else
	{
		error= 0;
	}
	
	memcpy(dest, replay.location_in_cache, *count);
	replay.bytes_in_cache -= *count;
	replay.location_in_cache += *count;
	
	return error;
}

static void remove_input_controller(
	void)
{
	remove_timer_task(input_task);
	if (replay.game_is_being_recorded)
	{
		stop_recording();
	}
	else if (replay.game_is_being_replayed)
	{
		if (replay.resource_data)
		{
			free(replay.resource_data);
			replay.resource_data= NULL;
			replay.resource_data_size= 0l;
			replay.film_resource_offset= NONE;
		}
		else
		{
			assert(replay.recording_file_refnum>0);
			close_file(replay.recording_file_refnum);
		}
	}

	replay.valid= FALSE;

	return;
}
