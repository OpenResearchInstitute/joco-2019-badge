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

notifications_state_t   notifications_state;

void notifications_init() {
    notifications_state.state = 0;
    notifications_state.button_value = 0;

    return;
}

#if INCLUDE_CAPTURE
void capture_notification_callback() {
    // This should only be called when it is OK to display a notification.
    creature_data_t creature_data;

    //buffer for formatting text
    char temp[20];

    uint8_t button;
    bool background_was_running;

    // make sure we have a request pending
    if (notifications_state.state != NOTIFICATIONS_STATE_REQUESTED) {
        return; // This is an error
    }

    notifications_state.state = NOTIFICATIONS_STATE_IN_PROGRESS;

    // Validate that the creature index is in range
    if (notifications_state.creature_index > capture_max_index()) {
        notifications_state.button_value = BUTTON_MASK_LEFT; // so we don't try to score it
        notifications_state.state = NOTIFICATIONS_STATE_IDLE;
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

    // creature index is already populated in notification state, fill in the name and percentage
    bool ok = read_creature_data(notifications_state.creature_index, &creature_data);
    if (ok) {
        notifications_state.creature_percent = creature_data.percent;
        strncpy(notifications_state.creature_name, creature_data.name, CAPTURE_MAX_NAME_LEN + 1);
    } else {
        // we can't display this notification
        notifications_state.button_value = BUTTON_MASK_LEFT; // so we don't try to score it
        notifications_state.state = NOTIFICATIONS_STATE_IDLE;
        if (background_was_running) {
            mbp_background_led_start();
        }
        return;
    }

    //Print the name
    util_gfx_set_font(FONT_LARGE);
    util_gfx_set_color(COLOR_WHITE);
    util_gfx_set_cursor(0, NOTIFICATION_UI_MARGIN);
    util_gfx_print(notifications_state.creature_name);

    //Print points
    util_gfx_set_color(COLOR_RED);
    sprintf(temp, "POINTS: %u",  rarity_to_points(notifications_state.creature_percent));
    util_gfx_set_cursor(0, 117);
    util_gfx_print(temp);

    sprintf(temp, "CAPTURE/%04d.RAW", notifications_state.creature_index);
    button = notification_filebased_bling(temp, notifications_state.led_filename, (notifications_state.timeout * 1000));

    notifications_state.button_value = button;
    util_button_clear();    //Clean up button state

    switch (button) {
    case BUTTON_MASK_UP:
    case BUTTON_MASK_DOWN:
    case BUTTON_MASK_LEFT:
    case BUTTON_MASK_RIGHT:
    case BUTTON_MASK_ACTION:
        // user pressed a button, add to score
        add_to_score(rarity_to_points(notifications_state.creature_percent), notifications_state.creature_name);
        mbp_state_capture_set_captured(notifications_state.creature_index);
        mbp_state_capture_count_increment();
        break;
    default:
        // zero means a timeout
        break;
    }

    //Only start background LED display if previously running
    if (background_was_running) {
        mbp_background_led_start();
    }
    
    notifications_state.state = NOTIFICATIONS_STATE_IDLE;

    return;
}
#endif // INCLUDE_CAPTURE
