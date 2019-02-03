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
 *****************************************************************************/

#include "system.h"

#define NOTIFICATION_UI_MARGIN         3
#define NOTIFICATION_UI_MARGIN_RIGHT  12

notifications_state_t	notifications_state;

void notifications_init() {
    notifications_state.state = 0;
    notifications_state.button_value = 0;

    return;
}

void capture_notification_callback() {
    // This should only be called when it is OK to display a notification.

    //buffer for formatting text
    char temp[32];

    bool redraw = true;
    bool loop = false;
    bool background_was_running;

    creature_data_t creature_data;

    
    // make sure we have a request pending
    if (notifications_state.state != NOTIFICATIONS_STATE_REQUESTED) {
	    return; // This is an error
    }

    notifications_state.state = NOTIFICATIONS_STATE_IN_PROGRESS;

    bool ok = read_creature_data(notifications_state.user_data, &creature_data);
    if (!ok) {
	    // not much we can do, so don't display the notification
	    notifications_state.button_value = BUTTON_MASK_SPECIAL;;
	    notifications_state.state = NOTIFICATIONS_STATE_COMPLETE;
	    return;
    }

    while (1) {
	if (redraw || !util_gfx_is_valid_state()) {

	    //Make sure there's no clipping
	    util_gfx_cursor_area_reset();

	    //Draw background
	    mbp_ui_cls();

	    // TODO Load the graphic file as background

	    sprintf(temp, "CAPTURE/%04d.RAW", notifications_state.user_data);
	    util_gfx_draw_raw_file(temp, 0, 0, 128, 128, NULL, false, NULL);

	    //Print their name
	    util_gfx_set_font(FONT_LARGE);
	    util_gfx_set_color(COLOR_WHITE);
	    util_gfx_set_cursor(0, NOTIFICATION_UI_MARGIN);
	    util_gfx_print(creature_data.name);

	    if (mbp_notification_led_running()) {
	        strcpy(temp, "REENTERED");
	        util_gfx_set_cursor(NOTIFICATION_UI_MARGIN, 52);
	        util_gfx_print(temp);
	    }

	    //Print points
	    util_gfx_set_color(COLOR_RED);
	    sprintf(temp, "POINTS: %u",  rarity_to_points(creature_data.percent));
	    util_gfx_set_cursor(NOTIFICATION_UI_MARGIN, 110);
	    util_gfx_print(temp);

	    background_was_running = mbp_background_led_running();

	    mbp_background_led_stop();

	    mbp_notification_led_start();

	    redraw = false;
	    loop = true;
	}

	//validate screen state
	util_gfx_validate();

	// Spin until we time out or the user presses a button
	uint32_t end_time = util_millis() + (notifications_state.timeout * 1000);
	do {
		if (util_button_state() > 0) {
			// we had a button press
			notifications_state.button_value = util_button_state();
			util_button_clear();	//Clean up button state
			mbp_notification_led_stop();
			//Only start background LED display if previously running
			if (background_was_running) {
				mbp_background_led_start();
			}
			loop = false;
		} else if (util_millis() > end_time) { // test for timeout
			notifications_state.button_value = BUTTON_MASK_SPECIAL;;
			mbp_notification_led_stop();
			//Only start background LED display if previously running
			if (background_was_running) {
				mbp_background_led_start();
			}
			loop = false;
		}
		// spin for some period
		nrf_delay_ms(100);
	} while (loop);
	notifications_state.state = NOTIFICATIONS_STATE_COMPLETE;

	return;
    }
}
