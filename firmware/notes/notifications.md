# Badge LED and LCD Display flow

## Things which are displayed on the LEDs and LCD:

* Boot sequence
* A menu (displayed initially then never again)
* The "Game Over" LCD display, reached by exiting the top level menu
* A "Hello", which is displayed when acknowledging another badge (displayed from within the background bling repeated timer callback)
* A "custom" bling Selected from a menu (displays until exited)
* A "Notification", which displays until it times out or a buntton is pressed. These display no matter what else is displaying (not during the boot sequence)
* Various menus. The top level one is presented by mbp_menu_main(), called from main.c

## Background Bling

Fomerly, LED bling ran at all times unless something stoped it (games typically stop it). Anything that stops the LED background bling restarts it when finished.
Places in the code that usewd to stop/restart the background LED bling were:
* Starting any custom bling from the menus stops it
* Entering menus below the main menu seems to stop it but not clear the LEDs (check this)
* Some custom bling that is started/stoppped in mbp_term.c in __bling_schedule_handler()
* games - chip8, mastermind, capture, transio QSL, skifree
* during execution of TCL scripts

This has been changed to only display background LED bling in the top level menu, which is where most people tend to 'park' their display if they're not displaying custom bling. This made it easier to not have to worry about when to turn off and on the background bling. However, in the final implementation of notifications this matters less, and it should be possible to add back more places that display background bling if needed.

The background LED bling code is in bling/mbp_bling.c
The timer handler __background_led_sch_handler() is called every BACKGROUND_LED_TIME_MS
Background bling is started, stopped, and tested with:
* mbp_background_led_stop()
* mbp_background_led_start()
* mbp_background_led_running()

This background LED bling code has been duplicated for notification LED bling, to make it easy to start and stop each type of LED bling.

In the main loop, mbp_system_unlock_state() [in mbp_system.c] is called, and this displays which unlocks have happened (on the LEDs), and the top level LCD bling animation (currently magic eye).
This is like all other menu-selected blings, and blocks until the user exits with a keypress. When this exits, the background LED bling is restarted and the main menu is displayed again.

## "Hello" displays

Each time the background LED animation sequence is complete in __background_led_sch_handler(), hello_background_handler() is called to determine whether to display a "hello".
If a hello is displayed, it is done in a blocking manner from here, and after the hello is done, control returns to the led background timer handler. Since it's only possible for a "Hello" to be displayed while the background LED pattern is running, it's never the case that a "hello" interrupts a custom bling.
We do not want Hello displays to interrupt notifications

## Custom (Menu-Selected) Bling

Menu selected bling is "custom" bling. Background LED bling is always stopped before the menu selections are made for running custom bling. Most of the bling selection is done in menus in bling/mbp_custom_bling.c, although the functions called for each bling are in bling/mbp_bling.c.

The majority of custom bling displays are started by simple_filebased_bling(), which simply loops through a RAW file for the LCD and one for the LED patterns.

Eventually, this results in a call to util_gfx_draw_raw_file(), from util/util_gfx.c, which accepts a file name for a .RAW file to display on the LCD, and a callback, which is called every frame.
Each time through the frame loop in util_gfx_draw_raw_file(), the buttons are tested and if any button has been pressed, control is returned to the caller.

In order to facilitate notifications, a timeout parameter was added to util_gfx_draw_raw_file(). If the timeout is reached, a button code of zer is returned.

# Notifications

Notifications must be able to 'interrupt' either custom bling or menus, and upon completion, return to the previous display. All other times where background bling is stopped, notifications will not be displayed. Upon return from a notification, the custom bling or menu that was interrupted is displayed from the beginning. Code to detect and break for notifications was only added to mbp_menu() and not mbp_submenu(), and is maskable. It is only enabled for the topmost menu. This is because it could be very annoying to get interrupted in some of the deeper, longer menus and have them redisplay from the top after a notification.

A Notification does the following, from the user POV:

0. See if a notification is in progress and ignore if one is.
1. Clear the LCD and LEDs
3. Display text text on the LCD
4. Play a RAW animation in a window on the LCD (which excludes the text area), with a timeout
5. Wait until the user presses a button or the timeout is reached
6. Read the button code for the button pressed (0 == timeout)

## Notification Implementation

The intention is for notifications to be usable by multiple games and functionality on the badge, so it's abstracted a bit, with the code to do the actual notification residing alongside the code for whatever is using it. But the generic parts are in notifications.[ch], and spread throughout the rest of the badge code.

Since this needs to be able to pre-empt either background bling or custom bling, and both of those operate in a loop, there is an async method of creating a notification. This method uses some state information stored in the notifications_state structure.

```
	void        (*p_notification_callback)(void);
	uint32_t    timeout; // in seconds
	uint16_t    led_style;
	uint16_t    user_data;
	uint8_t	    state;
	uint8_t     button_value;
	char        led_filename[20]; // 8.3 plus a terminator plus any possible path
```

* p_notification_callback - is called to do the actual bling, and is called by a looping bling that detects that a notification has been requested
* timeout - is the number of seconds that the notification will display before timing out
* led_style - is used to pass different LED flashing styles for use during the notification
* user_data - contains the ID of the creature we're alerting for. This is used to retrieve the image and score information for the notification.
* state - tracks state and is set by various steps in the process
* button_value - when the notification is complete, this contains the ID of the button the user pressed to ack the notification, or BUTTON_MASK_SPECIAL if it timed out.
* led_filename - aa filename for .RGB bling to play during the notification

State and timers are used to manage notifications because:
Notifications are triggered in the advertising receive code, but we don't want to block there.
The capture game software needs to know whether the user pressed a button, in order to know whether to score the creature.

## Notification flow for the capture game

When a creature packet is received, the creature id (a one-based index) is extracted from the name field, and capture_process_heard() in capture_game.c is called.

capture_process_heard does the following:

1. Return immediately if the notification state isn't "IDLE"
2. in the notification_state structure, set the callback to point to capture_notification_callback() in notifications.c
3. in the notification_state struct, store the creature ID, timeout, and led bling file name
4. in the notification state struct, set state to REQUESTED
5. return

Inside the custom bling display loop and during menu keypress check, the notification state is tested, and if it is REQUESTED, then the current state is saved, and the callback is called.

The callback:
* sets notification state to IN_PROGRESS
* displays the image of the creature plus some text
* stops background LED bling if it's running
* starts the notification background LED bling
* displays desired text
* starts playing a RAW file (can be an animation or a single frame static image)
* spins until the user presses a key, or until it times out
* stops the notification background LED bling is stopped
* if background LED bling was previously running, it is restarted
* The keypress is written to notification_state.button_value, and is BUTTON_MASK_SPECIAL if the notification timed out
* acts on the user button press (or timeout)
* The notification state is then set to IDLE, skipping the optional COMPLETE state
* control returns to the caller, which is expected to restore the current bling or menu state and continue.

Other implementations may wish to handle the completion of the notification in a separate thread, or in a period timer.
If this is desired, then the callback can set the state to COMPLETE and return, and the notification state of COMPLETE can trigger
the actions in another context, after which state is set to IDLE.

## How custom bling handles saving state and restoring it during a notification

All custom bling eventually resolves to a call to util_gfx_draw_raw_file() in util/util_gfx.c.
So does any call to display a static image on the LCD, but (I hope) the 'loop' boolean is only set for animated custom bling displays.

I renamed this function to util_gfx_draw_raw_file_inner() and made the originally named function a wrapper that call it. The old functionality was that the bling looped until the user pressed a button, and the button value was returned. I added a check each frame which looks to see if notification state is REQUESTED, and returns BUTTON_MASK_SPECIAL in that case.

The wrapper function util_gfx_draw_raw_file() detects this, and calls the notification callback. When that completes, it simply calls the inner function again, which reumes the original bling.

## How mbp_menu handles saving state and restoring it during a notification

This is identical to the way the bling was done. The old function is now a wrapper to an inner function, which checks for a pending notification and returns control, allowing display of the notification.

