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

Background LED bling runs at all times unless something stops it (games typically stop it). Anything that stops the LED background bling restarts it when finished.
Places in the code that stop/restart the background LED bling are:
* Starting any custom bling from the menus stops it
* Entering menus below the main menu seems to stop it but not clear the LEDs (check this)
* Some custom bling that is started/stoppped in mbp_term.c in __bling_schedule_handler()
* games - chip8, mastermind, capture, transio QSL, skifree
* during execution of TCL scripts

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

The newly added global boolean 'blinging' is set while a bling is being displayed

# Notifications

Notifications must be able to 'interrupt' either custom bling or the background bling, and upon completion, return to the previous display. All other times where background bling is stopped, notifications will not be displayed.

A Notification does the following, from the user POV:

0. See if a notification is in progress and ignore if one is.
1. Clear the LCD and LEDs
2. Display a static image on the LCD
3. Overlay text on that image
4. Wait until the user presses a button and return the button code

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
	char        image_filename[20]; // 8.3 plus a terminator plus any possible path
```

* p_notification_callback - is called to do the actual bling, and is called by a looping bling that detects that a notification has been requested
* timeout - is the number of seconds that the notification will display before timing out
* led_style - is used to pass different LED flashing styles for use during the notification
* user_data - contains the ID of the creature we're alerting for. This is used to retrieve the image and score information for the notification.
* state - tracks state and is set by various steps in the process
* button_value - when the notification is complete, this contains the ID of the button the user pressed to ack the notification, or BUTTON_MASK_SPECIAL if it timed out.
* image_filename - is redundant and not used, and will be removed

State and timers are used to manage notifications because:
Notifications are triggered in the advertising receive code, but we don't want to block there.
The capture game software needs to know whether the user pressed a button, in order to know whether to score the creature.

## Notification flow

When a creature packet is received, the creature id (a one-based index) is extracted from the name field, and capture_process_heard() in capture_game.c is called.

capture_process_heard does the following:

0. Return immediately if the notification state isn't "IDLE"
1. Return if we're not blinging or displaying background LEDs. This check avoinds interrupting games and menus.
2. in the notification_state structure, set the callback to point to capture_notification_callback() in notifications.c
3. in the notification_state struct, store the creature ID, timeout, and led style
4. in th enotification state struct, set state to REQUESTED
5. return

Inside bling loops (currently only implemented for custom bling, see below), the notification state is tested, and if it is REQUESTED, then the current bling state is saved, and the callback is called.

The callback:
* sets notification state to IN_PROGRESS
* displays the image of the creature plus some text
* stops background LED bling if it's running
* starts the notification background LED bling
* spins until the user presses a key, or until it times out
* stops the notification background LED bling is stopped
* if background LED bling was previously running, it is restarted
* The keypress is written to notification_state.button_value, and is BUTTON_MASK_SPECIAL if the notification timed out
* The notification state is then set to COMPLETE
* control returns to the caller, which is expected to restore the current bling state and continue.

At this point, all that's left to do is act on the user button press (or timeout. This is handled inside a repeated timer which calls \__capture_timer_handler() in capture_game.c.
This timer fires once per second and handles periodically sending random creature packets, but it also checks for completion of a notification.

In this timer handler, if the notification state is COMPLETE, then the following are done:
* if the user acknowledged the notification, add the appropriate score and flasg the creature as caught
* Set the notification state to IDLE

## How custom bling handles saving state and restoring it during a notification

All custom bling eventually resolves to a call to util_gfx_draw_raw_file() in util/util_gfx.c.
So does any call to display a static image on the LCD, but (I hope) the 'loop' boolean is only set for animated custom bling displays.

I renamed this function to util_gfx_draw_raw_file_inner() and made the originally named function a wrapper that call it. The old functionality was that the bling looped until the user pressed a button, and the button value was returned. I added a check each frame which looks to see if notification state is REQUESTED, and returns BUTTON_MASK_SPECIAL in that case.

The wrapper function util_gfx_draw_raw_file() detects this, and calls the notification callback. When that completes, it simply calls the inner function again, which reumes the original bling.

This works! As long a a notification arrives while a custom bling is playing, you get the notification and it works, although the notification LED flashing is not right, for reasons I haven't yet found.

## But what about times when there's no custom bling and we're displaying background LED bling?

This is totally broken. What I did (naively) was create a repeated timer in capture_game.c which calls __notify_check_timer_handler(), which detects the case of displaying background LED bling while NOT in a custome bling, and calls the notification callback.

The problem with this is that background bling runs independently of a lot of user interaction. It's on during a lot of different menu displays, although it's turned off for some. But it doesn't seem to be on OUTSIDE of when menus are being displayed. Also, background LED bling is sometimes turned off without clearing the LEDs, but we can and should fix that.

Here's my thinking so far on this, but I want to make sure there's not a better way.

menus all resolve to mbp_menu(). Button presses are checked continuously in this code with calls to util_button_wait(). We could wrap mbp_menu the same way I did with util_gfx_draw_raw_file(), and add a check for pending notification to util_button_wait() that returns BUTTON_MASK_SPECIAL if there's a notification needed. We may have to add a new function util_button_wait_menu() which is used by menus, since some games also use the call, and I don't want to return unexpected button value to them.

As long as background bling is only displayed while menus are active, I think this will work.

## (for bonus points) and what about a better way to display notifications with LEDs flashing?

I think maybe there's a better way to display notification LED bling than to duplicate the background LED code. 

In the notification callback, we could load an LED bling file, as is done in simple_filebased_bling(), and then as we spin waiting for a user keypress, we can call util_play_rgb_frame(). If we make the spin loop delay the right value, it'll look good - we're not trying to sync with an LCD animation.

If we do this, the notification state can contain the name of an LED bling file, and not just a style, and I think it'll be more flexible.




