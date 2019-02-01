# Badge LED and LCD Display flow

## Things which are displayed on the LEDs and LCD:

* Boot sequence
* A menu (displayed initially then never agin)
* The "Game Over" LCD display, reached by exiting the top level menu
* A "Hello", which is displayed when acknowledging another badge (displayed from within the background bling repeated timer callback)
* A "custom" bling Selected from a menu (displays until exited)
* A "Notification", which displays until it times out or a buntton is pressed. These display no matter what else is displaying (not during the boot sequence)



## Background Bling

Background LED bling runs at all times unless something stops it (games typically stop it). Anything that stops the LED background bling restarts it when finished.
Places in the code that stop/restart the background LED bling are:
* Starting any custom bling from the menus
* Some custom bling that is started/stoppped in mbp_term.c in __bling_schedule_handler()
* games - chip8, mastermind, capture, transio QSL, skifree
* during execution of TCL scripts

Code is in bling/mbp_bling.c
The timer handler __background_led_sch_handler() is called every BACKGROUND_LED_TIME_MS
Background bling is started, stopped, and tested with:
* mbp_background_led_stop()
* mbp_background_led_start()
* mbp_background_led_running()

In the main loop, mbp_system_unlock_state() [in mbp_system.c] is called, and this displays which unlocks have happened (on the LEDs), and the top level LCD bling animation (currently magic eye).
This is like all other menu-selected blings, and blocks until the user exits with a keypress. When this exits, the background LED bling is restarted and the main menu is displayed again.

## "Hello" displays

Each time the background LED animation sequence is complete in __background_led_sch_handler(), hello_background_handler() is called to determine whether to display a "hello".
If a hello is displayed, it is done in a blocking manner from here, and after the hello is done, control returns to the led background timer handler. Since it's only possible for a "Hello" to be displayed while the background LED pattern is running, it's never the case that a "hello" interrupts a custom bling.

## Custom (Menu-Selected) Bling

Menu selected bling is "custom" bling. Background LED bling is always stopped before th emenu selections are made for running custom bling. Most of the bling selection is done in menus in bling/mbp_custom_bling.c, although the functions called for each bling are in bling/mbp_bling.c.

The majority of custom bling displays are started by simple_filebased_bling(), which simply loops through a RAW file for the LCD and one for the LED patterns.

Eventually, this results in a call to util_gfx_draw_raw_file(), from util/util_gfx.c, which accepts a file name for a .RAW file to display on the LCD, and a callback, which is called every frame.
Each time through the frame loop in util_gfx_draw_raw_file(), the buttons are tested and if any button has been pressed, control is returned to the caller.

The boolean 'blinging' is set while a bling is being displayed

## Notifications

Notifications must be able to 'interrupt' either custom bling or the background bling, and upon completion, return to the previous display.

Notifications can "interrupt" either background bling or custom bling. All other times where background bling is stopped, notifications will not be displayed.

The following logic should work:

A Notification display call does the following:

1. Clear the LCD and LEDs
2. Display a static image on the LCD
3. Overlay text on that image
4. Wait until the user presses a button and return the button code

Since this needs to be able to pre-empt either background bling or custom bling, and both of those operate in a loop, there is an async method of creating a notification. This method uses the following global variables. The first three are generic, and the last two will be customized for each notification

* notification_requested (initialized to false)
* notification_status (initialized to zero and remains zero until notification is complete)
* p_notification_callback (a pointer to the notification callback)
* notification_data (a struct containing the notification data such as image file, text, etc). Must contain a timeout in seconds.
* notification_callback (a function to perform steps 1-4 above)

When a notification is desired:
1. Store the notification information in the notification_data structure
2. Set the pointer to the notification callback
3. Clear notification_status
4. Set notification_requested to true
5. loop or block until notification_status is nonzero
6. notification_status will contain the button id for the button that terminated the notification, or a special value if the time limit was reached.

In the notification_callback function:
1. clear the display
2. start a timer for the timeout value
3. start the desired LED bling
4. display the desired image and text
5. if a button is pressed, return that button ID. If the timer fires, return some invalid button value TBD

In the background bling callback and custom bling display loop:

1. Periodically test the notification_requested flag
2. If is is true, then save all state and call the notification callback
3. When control returns, restore all state and continue.
