/*****************************************************************************
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Contributors:
 * 	@sconklin
 * 	@mustbeart
 * 	@abraxas3d
 *****************************************************************************/
#ifndef CAPTURE_GAME_H_
#define CAPTURE_GAME_H_
#if INCLUDE_CAPTURE

typedef struct {
    bool        initialized;
    //bool        sending;
    uint16_t    sending_state;
    uint16_t    max_index;
    uint16_t    c_index;
    uint16_t    countdown;
} capture_state_t;

#define CAPTURE_SENDING_STATE_IDLE 0
#define CAPTURE_SENDING_STATE_PENDING_START 1
#define CAPTURE_SENDING_STATE_SENDING 2
#define CAPTURE_SENDING_STATE_SKIP 3

extern capture_state_t capture_state;

extern uint16_t capture_internal_broadcast;

extern bool show_all_creatures;

// Testing
//#define DEBUG_USE_SEQUENTIAL_CREATURES
//#define DEBUG_CAPTURE_ALWAYS_SCORE
//#define CAPTURE_FAST_TEST

// Times are in seconds
#define MINUTES_10 600
#define MINUTES_20 1200
#define MINUTES_30 1800
#define MINUTES_60 3600

// General parameters
#define CAPTURE_MAX_INDEX 255 // cannot exceed 9999
#define CAPTURE_BITMASK_ARRAY_LEN ((CAPTURE_MAX_INDEX/32)+1) // used for tracking which creatures we've captured
#define CAPTURE_TIMER_INTERVAL 1000 // milliSeconds
#define CAPTURE_MAX_NAME_LEN 10
#define CAPTURE_MAX_INDEX_DIGITS 4
#define CAPTURE_MAX_DATA_FILE_LEN (CAPTURE_MAX_NAME_LEN + CAPTURE_MAX_INDEX_DIGITS + 2) // data plus one or two line feeds

// Scoring
#define POINTS_4_CAPTURE 100
#define POINTS_4_RARITY  20 // Max points miltiplier available for rarity

// Sending parameters
// Remember when setting these that each badge can 'capture' the creatures that it broadcasts, so every badge has
// an opportunity at least this often. 
#ifdef CAPTURE_FAST_TEST
#define CAPTURE_SENDING_INTERVAL 60
#define CAPTURE_SENDING_INTERVAL_JITTER 0
#define CAPTURE_SENDING_LENGTH 6
#else
#define CAPTURE_SENDING_INTERVAL MINUTES_60
#define CAPTURE_SENDING_INTERVAL_JITTER MINUTES_20
#define CAPTURE_SENDING_LENGTH 6
#endif

// cooldown during which we will not notify for th emost recently heard creature
#define CAPTURE_DUPE_COOLDOWN 60

typedef struct {
	char        name[CAPTURE_MAX_NAME_LEN + 1];
	uint8_t     percent;
} creature_data_t;

// receiving and display parameters
#define CAPTURE_UNSEEN_NOTIFICATION_DISPLAY_LENGTH 40 // Seconds
#define CAPTURE_SEEN_NOTIFICATION_DISPLAY_LENGTH 5 // Seconds

// How long to display each creature in bling
#define CAPTURE_BLING_DELAY 3000 // in mS

extern uint16_t decode_creature_name(char *);
extern uint16_t rarity_to_points(uint8_t);
extern bool read_creature_data(uint16_t, creature_data_t *);
extern void capture_init(void);
extern uint16_t capture_max_index(void);
extern void capture_process_heard_index(uint16_t);
extern void capture_process_heard(char *name);
extern void mbp_bling_captured(void *data);
extern void capture_do_something_special(bool allow_notifications);


#endif /* CAPTURE_GAME_H_ */
#endif //INCLUDE_CAPTURE
