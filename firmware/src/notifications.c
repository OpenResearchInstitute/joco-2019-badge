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
    notifications_state.requested = false;
    notifications_state.status = 0;
    return;
}

// TODO NOTE - the score code that this is based on keeps the background LED bling running while in here.
void capture_notification_callback() {

    //buffer for formatting text
    char temp[32];

    bool redraw = true;
    bool loop = false;

    notifications_state.status = 0; // this signals completion status, so make sure it's clear
    notifications_state.requested = false; // Set this false to ack that we're working on it

    while (1) {
	if (redraw || !util_gfx_is_valid_state()) {

	    //Make sure there's no clipping
	    util_gfx_cursor_area_reset();

	    //Draw background
	    mbp_ui_cls();

	    // TODO Load the graphic file as background
	    

	    //Print their name
	    util_gfx_set_font(FONT_LARGE);
	    util_gfx_set_color(COLOR_WHITE);
	    util_gfx_set_cursor(0, NOTIFICATION_UI_MARGIN);
	    mbp_state_name_get(temp);
	    util_gfx_print(temp);

	    util_gfx_set_color(COLOR_LIGHTBLUE);
	    strcpy(temp, "A NOTIFICATION");
	    util_gfx_set_cursor(NOTIFICATION_UI_MARGIN, 26);
	    util_gfx_print(temp);
	    if (mbp_notification_led_running()) {
	        strcpy(temp, "REENTERED");
	        util_gfx_set_cursor(NOTIFICATION_UI_MARGIN, 52);
	        util_gfx_print(temp);
	    }

	    //Print points
	    util_gfx_set_color(COLOR_RED);
	    util_gfx_set_cursor(NOTIFICATION_UI_MARGIN, 95);
	    util_gfx_print("Name:");

	    sprintf(temp, "    %u", mbp_state_score_get());
	    util_gfx_set_cursor(NOTIFICATION_UI_MARGIN, 110);
	    util_gfx_print(temp);

	    redraw = false;
	    loop = true;
	}

	//validate screen state
	util_gfx_validate();


	bool background_was_running = mbp_background_led_running();

	mbp_background_led_stop();

	mbp_notification_led_start();

	// Spin until we time out or the user presses a button
	uint32_t end_time = util_millis() + (notifications_state.timeout * 1000);
	do {
		if (util_button_state() > 0) {
			// we had a button press
			notifications_state.status = util_button_state();
			util_button_clear();	//Clean up button state
			mbp_notification_led_stop();
			//Only start background LED display if previously running
			if (background_was_running) {
				mbp_background_led_start();
			}
			loop = false;
		} else if (util_millis() > end_time) { // test for timeout
			notifications_state.status = BUTTON_MASK_SPECIAL;;
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

	return;
    }
}
