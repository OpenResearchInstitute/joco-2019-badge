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
#if INCLUDE_CAPTURE

typedef struct {
	bool        initialized;
	bool	    sending;
	uint16_t    max_index;
	uint16_t    c_index;
	uint16_t    countdown;
} capture_state_t;
capture_state_t	capture_state;

char tmp_fname[20];
uint16_t capture_internal_broadcast;
uint16_t dupe_cooldown = 0;

uint16_t __choose_creature(void);


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

uint16_t decode_creature_name(char *name) {
	if ((name[0] != 0) || (name[1] != 42)) {
		return 0;
	}
	if (strlen(&name[2]) != 4) {
		return 0;
	}
	return atoi(&name[2]);
}

uint16_t rarity_to_points(uint8_t percent) {
	return (POINTS_4_CAPTURE + ((100 - percent) * POINTS_4_RARITY));
}

bool read_creature_data(uint16_t id, creature_data_t *creature_data) {
	char file_data[CAPTURE_MAX_DATA_FILE_LEN+1];
	UINT bytes_read;
    FRESULT result;
    FIL dat_file;

    printf("reading %d\n", id);
	// read the data file for that creature
    // This seemed to fail often and end in a bad file system state
    // So, copy the recover code from util_gfx.c
	sprintf(tmp_fname, "CAPTURE/%04d.DAT", id);

	result = f_open(&dat_file, tmp_fname, FA_READ | FA_OPEN_EXISTING);
   	if (result != FR_OK) {
        return false;
    }
/*   	if (result != FR_OK) {
        printf("retrying fopen %s\n", tmp_fname);
        CRITICAL_REGION_ENTER();
        nrf_delay_ms(2000);
        util_sd_recover();
        CRITICAL_REGION_EXIT();

        result = f_open(&dat_file, tmp_fname, FA_READ | FA_OPEN_EXISTING);
        if (result != FR_OK) {
            printf("Could not fopen %s on second try.", tmp_fname);
            return false;
        }
	}
*/
    result = f_read(&dat_file, (uint8_t *) file_data, CAPTURE_MAX_DATA_FILE_LEN, &bytes_read);

    //Check for error
    if (result != FR_OK) {
        printf("retrying fread %s\n", tmp_fname);
        CRITICAL_REGION_ENTER();
        nrf_delay_ms(2000);
        util_sd_recover();
        CRITICAL_REGION_EXIT();

        result = f_open(&dat_file, tmp_fname, FA_READ | FA_OPEN_EXISTING);
        if (result != FR_OK) {
            printf("Could not open %s.", tmp_fname);
            return false;
        }
        result = f_read(&dat_file, (uint8_t *) file_data, CAPTURE_MAX_DATA_FILE_LEN, &bytes_read);
        if (result != FR_OK) {
            printf("could not read %s on 2nd try\n", tmp_fname);
            f_close(&dat_file);
            return false;
        }
    }
    f_close(&dat_file);

	// Parse the data file. the name is first and ends in a newline (0x0A)
    printf("read %d bytes of %s\n", bytes_read, tmp_fname);
	file_data[bytes_read] = 0;
	uint16_t cctr = 0;
	char *pch = &file_data[0];
	char *pdst = &creature_data->name[0];

	while ((*pch != 0x0A) && (cctr < CAPTURE_MAX_NAME_LEN)) {
        // we should probably test for nonprintables here
        *pdst++ = *pch++;
        cctr++;
	}

	if (cctr >= CAPTURE_MAX_NAME_LEN) {
        printf("excessive name length in %s\n", tmp_fname);
		return false;
	} else {
        *pdst = 0; // null terminate the name string
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
	char name[SETTING_NAME_LENGTH];

    if (dupe_cooldown > 0) {
        dupe_cooldown--;
    } else if (dupe_cooldown > CAPTURE_DUPE_COOLDOWN) {
        dupe_cooldown = CAPTURE_DUPE_COOLDOWN; // just in case we have a decrement race
    }

	// This should fire once per second. It handles one task, which is deciding whethre to broadcast a creature
	if (capture_state.initialized) {
        if (notifications_state.state == NOTIFICATIONS_STATE_INITIATED) {
            /*
             * We handle this step in the timer handler here because it requires reading the
             * creature data with a call to read_creature_data(). The only other place that creature data is
             * also in this timer handler. This is because the fatfs code is not re-entrant for operations on
             * the same file, and it has proven to be true that we can receive a creature advertisement for
             * a creature ID and also be in th eprocess of picking that one to randomly send. Having both
             * calls to read_creature_data() in this handler avoids a re-entrancy problem.
             */
            creature_data_t creature_data;
            // creature index is already populated in notification state, fill in the name and percentage
            bool ok = read_creature_data(notifications_state.creature_index, &creature_data);
            if (ok) {
                notifications_state.creature_percent = creature_data.percent;
                strncpy(notifications_state.creature_name, creature_data.name, CAPTURE_MAX_NAME_LEN + 1);
                notifications_state.state = NOTIFICATIONS_STATE_REQUESTED;
            } else {
                // we can't display this notification
                printf("Not notifying %d\n", notifications_state.creature_index);
                notifications_state.state = NOTIFICATIONS_STATE_IDLE;
            }
        }

        if (--capture_state.countdown == 0) {
            // If it's time to send a random creature, do that
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

                // Make it available to our own badge
                capture_internal_broadcast = capture_state.c_index;

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
	}
//	app_sched_execute();
}

void capture_init(void) {
	capture_state.initialized = false;
	capture_state.sending = false;
	capture_state.countdown = util_math_rand16_max(CAPTURE_SENDING_INTERVAL-(CAPTURE_SENDING_INTERVAL_JITTER/2));
	capture_state.countdown += util_math_rand16_max(CAPTURE_SENDING_INTERVAL_JITTER);

	// Find out how many creatures we have available to send
	char creaturedat[CAPTURE_MAX_INDEX_DIGITS + 2];
	uint16_t bytes_read;
	FRESULT result = util_sd_load_file("CAPTURE/NUM.DAT", (uint8_t *) creaturedat, CAPTURE_MAX_INDEX_DIGITS + 1, &bytes_read);
	if (result != FR_OK) {
		return;
	}

	creaturedat[bytes_read] = 0; // provide a hard stop for strtol
	uint32_t num_creatures = strtol(creaturedat, NULL, 10);
	if (num_creatures > CAPTURE_MAX_INDEX) {
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
}

uint16_t capture_max_index() {
    if (capture_state.initialized) {
        return capture_state.max_index;
    } else {
        return 0;
    }
}


bool capture_is_sending() {
	return capture_state.sending;
}

uint16_t __choose_creature(void) {
#ifdef DEBUG_USE_SEQUENTIAL_CREATURES
	static uint16_t seq_id = 1;
#endif

	uint8_t ttl = 20;
	uint16_t creature_id = 0; // 0 is invalid
	creature_data_t creature_data;
	while ((creature_id == 0) && (--ttl > 0)) {
#ifdef DEBUG_USE_SEQUENTIAL_CREATURES
		creature_id = seq_id++;
		if (seq_id > capture_state.max_index) {
			seq_id = 1;
		}
#else
		// Generate a random number in the range of 1 <= index <= max_index
		creature_id = util_math_rand16_max(capture_state.max_index-1) + 1; // because 0 is invalid
#endif
		bool ok = read_creature_data(creature_id, &creature_data);
        if (ok) {

            if ((creature_data.percent > 0) && (creature_data.percent <= 100)) {
                uint32_t chance;
                chance = util_math_rand32_max(100);
                if (creature_data.percent >= chance) {
                    return creature_id;
                } else {
                    creature_id = 0;
                }
            } else {
                printf("bad percentage in for creature ID %d\n", creature_data.percent);
                creature_id = 0;
            }
        } else {
            // we couldn't read creature data from SD card
            creature_id = 0;
        }
	}

	return creature_id;
}

void capture_process_heard_index(uint16_t creature_id) {
	// return if we're not ready to present another notification
	if (notifications_state.state != NOTIFICATIONS_STATE_IDLE) {
		return;
	}

	if ((creature_id > 0) && (creature_id <= capture_state.max_index)) {
#if ! defined (DEBUG_CAPTURE_ALWAYS_SCORE)
		if (mbp_state_captured_is_captured(creature_id)) {
			// Don't notify more than once for each creature, because sending lasts a while for each one
			return;
		}
#endif
        if ((notifications_state.creature_index == creature_id) && (dupe_cooldown > 0)) {
            // this is the same one we last notified for, skip it
            return;
        }
		notifications_state.p_notification_callback = capture_notification_callback;

		// possibly TODO make changes in LED flash appearance based on how rare it is
		notifications_state.timeout = CAPTURE_UNSEEN_NOTIFICATION_DISPLAY_LENGTH;
        strcpy(notifications_state.led_filename, "BLING/KIT.RGB");

		notifications_state.creature_index = creature_id;
        dupe_cooldown = CAPTURE_DUPE_COOLDOWN;
		notifications_state.state = NOTIFICATIONS_STATE_INITIATED;
	}
}

void capture_process_heard(char *name) {
	// Called when we've received a creature packet. If we haven't captured this creature, then
	// trigger the display of a notification, allowing the user to capture the creature. We do not block here,
	// so that we return to the ble advertising receive handler. This should only be called from
    // the ble advertising receive code, otherwise we have a race condition
	uint16_t creature_id;

	// parse the creature index from the name field
	creature_id = decode_creature_name(name);
    capture_process_heard_index(creature_id);
    }

void mbp_bling_captured(void *data) {
    // Display all captures creatures in sequence
    char temp[20];

    uint8_t button;
    bool background_was_running;
	creature_data_t creature_data;

    if (mbp_state_capture_count_get() == 0) {
        util_gfx_cursor_area_reset();
        mbp_ui_cls();
        util_gfx_set_font(FONT_LARGE);
        util_gfx_set_color(COLOR_WHITE);
        util_gfx_set_cursor(0, NOTIFICATION_UI_MARGIN);
        util_gfx_print("None Yet");
        util_button_wait();
        return;
    }

    for (uint16_t i = 1; i <= capture_state.max_index; i++) {
        if (mbp_state_captured_is_captured(i)) {
            if (!read_creature_data(i, &creature_data)) {
                continue;
            }

            util_gfx_cursor_area_reset();
            mbp_ui_cls();

            background_was_running = mbp_background_led_running();
            mbp_background_led_stop();
            
            util_gfx_set_font(FONT_LARGE);
            util_gfx_set_color(COLOR_WHITE);
            util_gfx_set_cursor(0, NOTIFICATION_UI_MARGIN);
            util_gfx_print(creature_data.name);

            //Print points
            util_gfx_set_color(COLOR_RED);
            sprintf(temp, "POINTS: %u",  rarity_to_points(creature_data.percent));
            util_gfx_set_cursor(0, 117);
            util_gfx_print(temp);

            sprintf(temp, "CAPTURE/%04d.RAW", i);
            button = notification_filebased_bling(temp, "BLING/CIRCLES.RGB", CAPTURE_BLING_DELAY);
            util_button_clear();    //Clean up button state

            switch (button) {
            case BUTTON_MASK_UP:
            case BUTTON_MASK_DOWN:
            case BUTTON_MASK_LEFT:
            case BUTTON_MASK_RIGHT:
            case BUTTON_MASK_ACTION:
                // user pressed a button
                break; // stop displaying the bling
            default:
                continue; // it was a timeout
                break;
            }
        }

        //Only start background LED display if previously running
        if (background_was_running) {
            mbp_background_led_start();
        }
    }
}
#endif //INCLUDE_CAPTURE
