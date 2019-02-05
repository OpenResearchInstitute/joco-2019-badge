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
 *****************************************************************************/

#include "system.h"

#define NOTIFICATION_UI_MARGIN         3
#define NOTIFICATION_UI_MARGIN_RIGHT  12

notifications_state_t   notifications_state;

void notifications_init() {
    notifications_state.state = 0;
    notifications_state.button_value = 0;

    return;
}

void capture_notification_callback() {
    // This should only be called when it is OK to display a notification.

    //buffer for formatting text
    char temp[32];

    uint8_t button;
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

    // We use large font, which is mishmash7pt7b,
    // which has a vertical stride of 12 pixel lines.
    // 128 - 24 = 104, for a 128 x 104 image
    // Top text line is at Y coordinate 0
    // Bottom text line is at Y coordinate 

    //Make sure there's no clipping
    util_gfx_cursor_area_reset();

    //Draw background
    mbp_ui_cls();

    background_was_running = mbp_background_led_running();

    mbp_background_led_stop();

    //Print their name
    util_gfx_set_font(FONT_LARGE);
    util_gfx_set_color(COLOR_WHITE);
    util_gfx_set_cursor(0, NOTIFICATION_UI_MARGIN);
    util_gfx_print(creature_data.name);

    //Print points
    util_gfx_set_color(COLOR_RED);
    sprintf(temp, "POINTS: %u",  rarity_to_points(creature_data.percent));
    util_gfx_set_cursor(0, 117);
    util_gfx_print(temp);

    sprintf(temp, "CAPTURE/%04d.RAW", notifications_state.user_data);
    char rgbfile[] = "BLING/KIT.RGB";
    button = notification_filebased_bling(temp, rgbfile, (notifications_state.timeout * 1000));

    notifications_state.button_value = button;
    util_button_clear();    //Clean up button state

    switch (button) {
    case BUTTON_MASK_UP:
    case BUTTON_MASK_DOWN:
    case BUTTON_MASK_LEFT:
    case BUTTON_MASK_RIGHT:
    case BUTTON_MASK_ACTION:
        // user pressed a button
        ok = read_creature_data(notifications_state.user_data, &creature_data);
        if (ok) {
            // add to score
            add_to_score(rarity_to_points(creature_data.percent), creature_data.name);
            mbp_state_capture_set_captured(notifications_state.user_data);
            mbp_state_capture_count_increment();
            notifications_state.user_data = 0;
        }
        break;
    default:
        // zero means a timeout
        break;
    }

    mbp_notification_led_stop();
    //Only start background LED display if previously running
    if (background_was_running) {
        mbp_background_led_start();
    }
    
    notifications_state.state = NOTIFICATIONS_STATE_IDLE;

    return;
}
