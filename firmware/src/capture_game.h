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
 * 	@mustbeart
 * 	@abraxas3d
 *****************************************************************************/
#ifndef CAPTURE_GAME_H_
#define CAPTURE_GAME_H_

// Times are in seconds
#define MINUTES_10 600
#define MINUTES_20 1200
#define MINUTES_30 1800

// General parameters
#define CAPTURE_MAX_INDEX 255 // cannot exceed 9999
#define CAPTURE_TIMER_INTERVAL 1000 // milliSeconds

// Sending parameters
#define CAPTURE_SENDING_INTERVAL MINUTES_20
#define CAPTURE_SENDING_INTERVAL_JITTER MINUTES_20

// receiving and display parameters
#define CAPTURE_DISPLAY_TIME_LENGTH 60 // Seconds

// Initialize anything
extern void capture_init(void);
extern bool capture_is_sending(void);


// handle hearing a creature advertisement
extern void capture_process_heard(char *name);

// handle starting a cycle of sending creature advertisements
extern void capture_send_creature(void);

// handle cleaning up after a cycle of sending creature advertisements
extern void capture_stop_send_creature();

#endif /* CAPTURE_GAME_H_ */
