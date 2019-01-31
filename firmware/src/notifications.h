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
 *****************************************************************************/
#ifndef NOTIFICATIONS_H_
#define NOTIFICATIONS_H_

#define NOTIFICATION_TIMER_INTERVAL 100 // milliSeconds

#define LED_STYLE_RED_FLASH 1
#define LED_STYLE_GREEN_FLASH 2

#define NOTIFICATIONS_STATE_IDLE        0
#define NOTIFICATIONS_STATE_REQUESTED   1
#define NOTIFICATIONS_STATE_IN_PROGRESS 2
#define NOTIFICATIONS_STATE_COMPLETE    3

typedef struct {
	void        (*p_notification_callback)(void);
	uint32_t    timeout; // in seconds
	uint16_t    led_style;
	uint16_t    user_data;
	uint8_t	    state;
	uint8_t     button_value;
	char        image_filename[20]; // 8.3 plus a terminator plus any possible path
	// filename for LED bling??
} notifications_state_t;

extern notifications_state_t notifications_state;

extern void notifications_init();
extern void capture_notification_callback();

#endif /* NOTIFICATIONS_H_ */
