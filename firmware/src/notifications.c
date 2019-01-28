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

#define GAMEPLAY_UI_MARGIN         3
#define GAMEPLAY_UI_MARGIN_RIGHT  12

notifications_state_t	notifications_state;

void notifications_init() {
    notifications_state.requested = false;
    notifications_state.status = 0;
    return;
}

void capture_notification_callback() {

    //buffer for formatting text
    char temp[32];

    bool redraw = true;

    while (1) {
	if (redraw || !util_gfx_is_valid_state()) {
#if INCLUDE_QSO
	    uint16_t qso_count = mbp_state_qso_count_get();
#endif
#if INCLUDE_MM
	    uint16_t mm_count = mbp_state_mm_count_get();
#endif
#if INCLUDE_CAPTURE
	    uint16_t capture_count = mbp_state_capture_count_get();
#endif

	    //Make sure there's no clipping
	    util_gfx_cursor_area_reset();

	    //Draw background
	    mbp_ui_cls();

	    //Print their name
	    util_gfx_set_font(FONT_LARGE);
	    util_gfx_set_color(COLOR_WHITE);
	    util_gfx_set_cursor(0, GAMEPLAY_UI_MARGIN);
	    mbp_state_name_get(temp);
	    util_gfx_print(temp);

	    util_gfx_set_color(COLOR_LIGHTBLUE);
	    strcpy(temp, "GAME STATS");
	    util_gfx_set_cursor(GAMEPLAY_UI_MARGIN, 26);
	    util_gfx_print(temp);

#if INCLUDE_CAPTURE
	    //Print their capture count
	    util_gfx_set_color(COLOR_YELLOW);
	    sprintf(temp, "Captured: %u", capture_count);
	    util_gfx_set_cursor(GAMEPLAY_UI_MARGIN, 60);
	    util_gfx_print(temp);
#endif
#if INCLUDE_QSO
	    //Print their QSO count
	    util_gfx_set_color(COLOR_YELLOW);
	    sprintf(temp, "QSOs %u", qso_count);
	    util_gfx_set_cursor(GAMEPLAY_UI_MARGIN, 60);
	    util_gfx_print(temp);
#endif
#if INCLUDE_MM
	    //Print the number of Mastermind puzzles solved
	    sprintf(temp, "Codes %u", mm_count);
	    util_gfx_set_cursor(GAMEPLAY_UI_MARGIN, 75);
	    util_gfx_print(temp);
#endif
	    //Print points
	    util_gfx_set_color(COLOR_RED);
	    util_gfx_set_cursor(GAMEPLAY_UI_MARGIN, 95);
	    util_gfx_print("Points:");

		sprintf(temp, "    %u", mbp_state_score_get());
		util_gfx_set_cursor(GAMEPLAY_UI_MARGIN, 110);
		util_gfx_print(temp);

		redraw = false;
	}

	//validate screen state
	util_gfx_validate();

	util_button_wait();
	util_button_clear();
	return;
    }
}
