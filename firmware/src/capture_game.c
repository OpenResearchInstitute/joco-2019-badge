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
	bool	sending;
	uint8_t		unused;
} capture_timer_state_t;
capture_timer_state_t	capture_state;

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
	// TODO Do some things
	app_sched_execute();
}

void capture_init(void) {
	uint32_t err_code;
	err_code = app_timer_create(&m_capture_timer, APP_TIMER_MODE_REPEATED, __capture_timer_handler);
	APP_ERROR_CHECK(err_code);

	uint32_t ticks = APP_TIMER_TICKS(CAPTURE_TIMER_INTERVAL, UTIL_TIMER_PRESCALER);
	err_code = app_timer_start(m_capture_timer, ticks, NULL);
	APP_ERROR_CHECK(err_code);

	capture_state.sending = false;
}

bool capture_is_sending() {
	return capture_state.sending;
}

void capture_process_heard(char *name) {
	uint16_t creature_id;
	// parse the creature index from the name field
	creature_id = __decode_name(name);
	if (creature_id != 0) {
		// alert the user that a creature is in the area
		//Stop background LED display
		util_led_clear();
		mbp_background_led_stop();
		app_sched_pause();
		// TODO Display the notification

	} else {
		// TODO This shouldn't happen
	}
}

void capture_send_creature(void) {
	char name[SETTING_NAME_LENGTH];
	uint16_t creature_id;
	// Set up to send some advertising packets identifying as a creature, instead of our normal info
	// Disable advertising
	util_ble_off();
	// TODO Select a creature ID to send
	creature_id = 12345;
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

