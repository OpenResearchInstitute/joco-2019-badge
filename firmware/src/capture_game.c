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
 *  @sconklin
 *  @mustbeart
 *  @abraxas3d
 *****************************************************************************/

#include "system.h"

typedef struct {
	bool        initialized;
	bool	    sending;
	uint16_t    max_index;
	uint16_t    countdown;
} capture_state_t;
capture_state_t	capture_state;

APP_TIMER_DEF(m_capture_timer);

void __encode_name(uint16_t cid, char *name) {
	if (cid > 9999) {
		cid = 9999;
	}
	// assumes the string destination is SETTING_NAME_LENGTH
	*name++ = 0; // Start with a Null Character to make this not valid for display (i.e. the wall)
	*name++ = 42; // a tag
	sprintf(name, "%04d", cid);
}

uint16_t __decode_name(char *name) {
	if ((name[0] != 0) || (name[1] != 42)) {
		return 0;
	}
	if (strlen(&name[2]) != 4) {
		return 0;
	}
	return atoi(&name[2]);
}

static void __capture_timer_handler(void * p_data) {
	// This should fire once per second.
	if (capture_state.initialized) {
		if (--capture_state.countdown == 0) {
			if (!capture_state.sending) {
				// Start sending
				capture_send_creature();
				// Set countdown so we stop sending
				capture_state.countdown = util_math_rand16_max(CAPTURE_SENDING_LENGTH);
			} else {
				// Stop sending
				capture_stop_send_creature();
				// Set countdown to start next sending time
				capture_state.countdown = util_math_rand16_max(CAPTURE_SENDING_INTERVAL-(CAPTURE_SENDING_INTERVAL_JITTER/2));
				capture_state.countdown += util_math_rand16_max(CAPTURE_SENDING_INTERVAL_JITTER);
			}
		}
	}
	app_sched_execute();
}

void capture_init(void) {
	capture_state.initialized = false;
	capture_state.sending = false;
	capture_state.countdown = util_math_rand16_max(CAPTURE_SENDING_INTERVAL-(CAPTURE_SENDING_INTERVAL_JITTER/2));
	capture_state.countdown += util_math_rand16_max(CAPTURE_SENDING_INTERVAL_JITTER);
		
	// Find out how many creature we have available to send
	char creaturedat[CAPTURE_MAX_INDEX_DIGITS + 2];
	FRESULT result = util_sd_load_file("CAPTURE/NUM.DAT", (uint8_t *) creaturedat, CAPTURE_MAX_INDEX_DIGITS + 1);
	if (result != FR_OK) {
		return;
	}
	creaturedat[CAPTURE_MAX_INDEX_DIGITS + 1] = 0; // provide a hard stop for strtol
	uint32_t num_creatures = strtol(creaturedat, NULL, 10);
	if (num_creatures > CAPTURE_MAX_INDEX) {
		return;
	} else {
		capture_state.max_index = (uint16_t) num_creatures;
		capture_state.initialized = true;
	}

	uint32_t err_code;
	err_code = app_timer_create(&m_capture_timer, APP_TIMER_MODE_REPEATED, __capture_timer_handler);
	APP_ERROR_CHECK(err_code);

	uint32_t ticks = APP_TIMER_TICKS(CAPTURE_TIMER_INTERVAL, UTIL_TIMER_PRESCALER);
	err_code = app_timer_start(m_capture_timer, ticks, NULL);
	APP_ERROR_CHECK(err_code);
}

bool capture_is_sending() {
	return capture_state.sending;
}

//
// TODO move the code to read creature file(s) out of __choose_creature, so that they can be used to retrieve
// information about whether a creature when you have the creature number.
//

uint16_t __choose_creature(void) {
	uint8_t ttl = 50;
	bool creature_found = false;
	uint16_t creature_id = 0; // 0 is invalid
	uint32_t tmp_l;
	char creature_data[CAPTURE_MAX_DAT_FILE_LEN+1];

	while ((!creature_found) && (--ttl > 0)) {
		// Generate a random number in the range of 1 <= index <= max_index
		creature_id = util_math_rand16_max(capture_state.max_index-1);
		creature_id++; // because 0 is invalid

		// read the data file for that creature
		char fname[20];
		sprintf(fname, "CAPTURE/%04d.DAT", creature_id);
		FRESULT result = util_sd_load_file(fname, (uint8_t *) creature_data, CAPTURE_MAX_DAT_FILE_LEN);
		if (result != FR_OK) {
			mbp_ui_error("Could not read creature data.");
			return 0;
		}
		// Parse the data file. the name is first and ends in a newline (0x0A)
		// we don't really care about the name here, only the rarity value from 0-100 which follows
		creature_data[CAPTURE_MAX_DAT_FILE_LEN] = 0;
		char *pch = creature_data;
		uint16_t cctr;
		while ((*pch != 0x0A) && (cctr > CAPTURE_MAX_DAT_FILE_LEN)) {
			pch++;
			cctr++;
		}
		if (*pch == 0x0A) {
			pch++;
			tmp_l = strtol(pch, NULL, 10);
		} else {
			return 0;
		}
		if ((tmp_l > 0) && (tmp_l <=100)) {
			uint8_t chance;
			chance = util_math_rand8_max(100);
			if (tmp_l >= chance) {
				return creature_id;
			}
		} else {
			return 0;
		}
	}
	return 0;
}

void capture_process_heard(char *name) {
	uint16_t creature_id;
	// parse the creature index from the name field
	creature_id = __decode_name(name);
	if (creature_id != 0) {
		// make sure we're not already presenting a notification
		// TODO we need a mutex or semaphore here
		//if ((notifications_state.requested == true) || (notifications_state.status )) {
		//	// TODO
		//}
		// notify the user that a creature is in the area
		notifications_state.p_notification_callback = capture_notification_callback;
		notifications_state.timeout = CAPTURE_DISPLAY_TIME_LENGTH;
		// TODO change the notification LED style based on whether we've seen this creature before
		// or maybe how rare it is
		notifications_state.led_style = LED_STYLE_RED_FLASH;
		// Creature filenames are "nnnn.RAW", based on the creature number
		sprintf(notifications_state.image_filename, "CAPTURE/%04d.RAW", creature_id); // make sure the destination is long enough if you change this.
		notifications_state.status = 0;
		notifications_state.requested = true; // this triggers either background or bling loops to pick this up
		while (notifications_state.requested == true); // spin until we know that the request has been picked up
		while (notifications_state.status == 0); // spin until we get a result
		switch (notifications_state.status) {
		case BUTTON_MASK_SPECIAL:
			// the notification timed out
			break;
		case BUTTON_MASK_UP:
		case BUTTON_MASK_DOWN:
		case BUTTON_MASK_LEFT:
		case BUTTON_MASK_RIGHT:
		case BUTTON_MASK_ACTION:
			// Count anything else as accepting the capture
			// TODO complete the capture
			break;
		default:
			break;
		}
		// Clear up any state
		notifications_state.status = 0;


	} else {
		// TODO This shouldn't happen
	}
}

void capture_send_creature(void) {
	char name[SETTING_NAME_LENGTH];
	uint16_t creature_id;
	// Set up to send some advertising packets identifying as a creature, instead of our normal info
	// Select a creature ID to send
	creature_id = __choose_creature();
	if (creature_id == 0) {
		return;
	}

	// Disable advertising
	util_ble_off();
	// Encode it into the name field
	__encode_name(creature_id, name);
	// Change the Appearance ID to make this a 'creature' advertisement
	util_ble_appearance_set(APPEARANCE_ID_CREATURE);
	// TODO Set a timer or counter to only send a limited number of these creature advertisements
	// TODO Set the advertising data packet data
	// Enable advertising
	capture_state.sending = true;
	util_ble_on();
}

void capture_stop_send_creature() {
	char name[SETTING_NAME_LENGTH];
	// Disable advertising
	util_ble_off();
	// Restore the name in the advertisement from the state information
	mbp_state_name_get(name);
	util_ble_name_set(name);
	// restore the appearance ID to BADGE_APPEARANCE
	util_ble_appearance_set(BADGE_APPEARANCE);
	// Enable advertising
	capture_state.sending = false;
	util_ble_on();
}

