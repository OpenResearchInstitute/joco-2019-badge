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
	uint16_t    c_index;
	uint16_t    countdown;
} capture_state_t;
capture_state_t	capture_state;

typedef struct {
	char        name[CAPTURE_MAX_NAME_LEN + 1];
	uint8_t     percent;
} creature_data_t;

uint16_t __choose_creature(void);


APP_TIMER_DEF(m_capture_timer);
APP_TIMER_DEF(m_notify_check_timer);

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

uint16_t __rarity_to_points(uint8_t percent) {
	return (POINTS_4_CAPTURE + ((100 - percent) * POINTS_4_RARITY));
}

void __write_bad_file_flag(uint16_t index, char *text) {
	char fname[20];
	FIL file;
	UINT count;
	FRESULT result;

	sprintf(fname, "CAPTURE/%04d.BAD", index);
	result = f_open(&file, fname, FA_CREATE_NEW | FA_WRITE);
	if (result != FR_OK) {
		// probably FR_EXIST, but we ignore all errors
		return;
	}

	result = f_write(&file, (void *) text, strlen(text), &count); // ignore result

	result = f_close(&file);
}

bool __read_creature_data(uint16_t id, creature_data_t *creature_data) {
	char file_data[CAPTURE_MAX_DAT_FILE_LEN+1];
	char fname[20];

	// read the data file for that creature
	sprintf(fname, "CAPTURE/%04d.DAT", id);
	FRESULT result = util_sd_load_file(fname, (uint8_t *) file_data, CAPTURE_MAX_DAT_FILE_LEN);
	if (result != FR_OK) {
		__write_bad_file_flag(id, "read");
		return false;
	}
	// Parse the data file. the name is first and ends in a newline (0x0A)
	file_data[CAPTURE_MAX_DAT_FILE_LEN] = 0;
	uint16_t cctr = 0;
	char *pch = &file_data[0];
	char *pdst = &creature_data->name[0];

	while ((*pch != 0x0A) && (cctr < CAPTURE_MAX_NAME_LEN)) {
		// we should probably test for nonprintables here
		if (*pch == 0x0A) {
			*pdst = 0; // null terminate the name string
			break;
		} else {
			*pdst++ = *pch++;
			cctr++;
		}
	}

	if (cctr >= CAPTURE_MAX_NAME_LEN) {
		__write_bad_file_flag(id, "name");
		return false;
	} else {
		pch++;
		uint32_t tpct = strtol(pch, NULL, 10);
		if (tpct > 100) {
			tpct = 100;
		}
		creature_data->percent = (uint8_t) tpct;
		return true;
	}
}

static void __capture_timer_handler(void * p_data) {
	creature_data_t creature_data;
	char name[SETTING_NAME_LENGTH];
	bool ok;
	
	// This should fire once per second. It handles two tasks
	if (capture_state.initialized) {
		// If it's time to send a random creature, do that
		if (--capture_state.countdown == 0) {
			if (!capture_state.sending) {
				// Set up to send some advertising packets identifying as a creature, instead of our normal info
				// Select a creature ID to send
				do {
					capture_state.c_index = __choose_creature();
				} while (capture_state.c_index == 0);

				// Encode it into the name field
				__encode_name(capture_state.c_index, name);

				// Disable advertising
				util_ble_off();

				// Change the Appearance ID to make this a 'creature' advertisement
				util_ble_appearance_set(APPEARANCE_ID_CREATURE);

				// The field is seven characters long, and has leading and trailing nulls
				util_ble_name_set_special(name, 7);

				// Enable advertising
				capture_state.sending = true;
				util_ble_on();

				// Set countdown so we stop sending
				capture_state.countdown = CAPTURE_SENDING_LENGTH;
			} else {
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

				// Set countdown to start next sending time
				capture_state.countdown = CAPTURE_SENDING_INTERVAL-(CAPTURE_SENDING_INTERVAL_JITTER/2);
				capture_state.countdown += util_math_rand16_max(CAPTURE_SENDING_INTERVAL_JITTER);
			}
		}
		// See if we have a notification request that has completed.
		if (notifications_state.state == NOTIFICATIONS_STATE_COMPLETE) {
			switch (notifications_state.button_value) {
			case BUTTON_MASK_UP:
			case BUTTON_MASK_DOWN:
			case BUTTON_MASK_LEFT:
			case BUTTON_MASK_RIGHT:
			case BUTTON_MASK_ACTION:
				// user pressed a button
				ok = __read_creature_data(notifications_state.user_data, &creature_data);
				if (ok) {
					// add to score
					add_to_score(__rarity_to_points(creature_data.percent), creature_data.name);
					mbp_state_capture_set_captured(notifications_state.user_data);
					notifications_state.user_data = 0;
				}
				break;
			case BUTTON_MASK_SPECIAL:
				// the notification timed out, so do nothing
				break;
			default:
				break;
			}
			notifications_state.state = NOTIFICATIONS_STATE_IDLE; // ready for another notification
		}
	}
	app_sched_execute();
}

static void __notify_check_timer_handler(void * p_data) {
	//
	// The only purpose of this timer is to detect that a notification is needed for the case when custom bling
	// isn't running, and run it here.
	//

	// make a final check that conditions are ok
	if (notifications_state.state != NOTIFICATIONS_STATE_REQUESTED) {
		return;
	}
	if ((blinging) || (!mbp_background_led_running())) {
		return;
	}
	notifications_state.p_notification_callback();

	app_sched_execute();
}

void capture_init(void) {
	capture_state.initialized = false;
	capture_state.sending = false;
	capture_state.countdown = util_math_rand16_max(CAPTURE_SENDING_INTERVAL-(CAPTURE_SENDING_INTERVAL_JITTER/2));
	capture_state.countdown += util_math_rand16_max(CAPTURE_SENDING_INTERVAL_JITTER);

	// Find out how many creatures we have available to send
	char creaturedat[CAPTURE_MAX_INDEX_DIGITS + 2];
	FRESULT result = util_sd_load_file("CAPTURE/NUM.DAT", (uint8_t *) creaturedat, CAPTURE_MAX_INDEX_DIGITS + 1);
	if (result != FR_OK) {
		return;
	}

	creaturedat[CAPTURE_MAX_INDEX_DIGITS + 1] = 0; // provide a hard stop for strtol
	uint32_t num_creatures = strtol(creaturedat, NULL, 10);
	if (num_creatures > (CAPTURE_MAX_INDEX + 1)) {
		return;
	} else {
		capture_state.max_index = (uint16_t) num_creatures;
		capture_state.initialized = true;
	}

	uint32_t err_code;
	uint32_t ticks;

	err_code = app_timer_create(&m_capture_timer, APP_TIMER_MODE_REPEATED, __capture_timer_handler);
	APP_ERROR_CHECK(err_code);

	ticks = APP_TIMER_TICKS(CAPTURE_TIMER_INTERVAL, UTIL_TIMER_PRESCALER);
	err_code = app_timer_start(m_capture_timer, ticks, NULL);
	APP_ERROR_CHECK(err_code);

	err_code = app_timer_create(&m_notify_check_timer, APP_TIMER_MODE_REPEATED, __notify_check_timer_handler);
	APP_ERROR_CHECK(err_code);

	ticks = APP_TIMER_TICKS(NOTIFY_CHECK_TIMER_INTERVAL, UTIL_TIMER_PRESCALER);
	err_code = app_timer_start(m_notify_check_timer, ticks, NULL);
	APP_ERROR_CHECK(err_code);


}

bool capture_is_sending() {
	return capture_state.sending;
}

uint16_t __choose_creature(void) {
#ifdef USE_SEQUENTIAL_CREATURES
	static uint16_t seq_id = 1;
#endif

	uint8_t ttl = 50;
	bool creature_found = false;
	uint16_t creature_id = 0; // 0 is invalid
	creature_data_t creature_data;
	while ((!creature_found) && (--ttl > 0)) {
#ifdef USE_SEQUENTIAL_CREATURES
		creature_id = seq_id++;
		if (seq_id > capture_state.max_index) {
			seq_id = 1;
		}

#else
		// Generate a random number in the range of 1 <= index <= max_index
		creature_id = util_math_rand16_max(capture_state.max_index-1);
		creature_id++; // because 0 is invalid
#endif
		bool ok = __read_creature_data(creature_id, &creature_data);
		if (!ok) {
			return 0;
		}

		if ((creature_data.percent > 0) && (creature_data.percent <= 100)) {
			uint32_t chance;
			chance = util_math_rand32_max(100);
			if (creature_data.percent >= chance) {
				return creature_id;
			} else {
				return 0;
			}
		} else {
			__write_bad_file_flag(creature_id, "percent");
			return 0;
		}
	}

	return 0;
}

void capture_process_heard(char *name) {
	// Called when we've received a creature packet. If we haven't captured this creature, then
	// trigger the display of a notification, allowing the user to capture the creature. We do not block here,
	// so that we return to the ble advertising receive handler.
	uint16_t creature_id;

	// return if we're not ready to present another notification
	if (notifications_state.state != NOTIFICATIONS_STATE_IDLE) {
		return;
	}

	// We want to display a notification only if the background LED bling is
	// displaying, or if there's a custom bling playing.
	// These two conditions are mutually exclusive, since custom bling is started
	// through a menu choice, and background LED bling is disabled when any submenu
	// is entered.
	//
	// We can tell if background bling is running with mbp_background_led_running().
	// If it is, it'll get picked up by the timer routine provided here.
	//
	// We can tell whether custom bling is running because the 'blinging' boolean will be true.
	// There's a 'hook' in that code that checks to see if a notification has been requested (using
	// notification_state.state == NOTIFICATION_STATE_REQUESTED), and if it has,
	// then it runs the notification callback.

	if ((blinging) || mbp_background_led_running()) {
		// parse the creature index from the name field
		creature_id = __decode_name(name);
		if ((creature_id > 0) && (creature_id <= capture_state.max_index)) {
			if (mbp_state_captured_is_captured(creature_id)) {
				// Don't notify more than once for each creature, because sending lasts a while for each one
				return;
			}
			notifications_state.p_notification_callback = capture_notification_callback;

			// possibly TODO make changes in appearance based on how rare it is
			notifications_state.timeout = CAPTURE_SEEN_NOTIFICATION_DISPLAY_LENGTH;
			notifications_state.led_style = LED_STYLE_RED_FLASH;

			// Creature filenames are "nnnn.RAW", based on the creature number
			sprintf(notifications_state.image_filename, "CAPTURE/%04d.RAW", creature_id); // make sure the destination is long enough if you change this.
			notifications_state.user_data = creature_id;
			notifications_state.state = NOTIFICATIONS_STATE_REQUESTED;

		} else {
		}
	}
}
