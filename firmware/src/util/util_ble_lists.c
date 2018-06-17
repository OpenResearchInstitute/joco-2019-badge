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
#include "../system.h"

// Seen list, each entry is an array of bytes
#define SEEN_DB_LEN 30

typedef struct {
	uint8_t address[BLE_GAP_ADDR_LEN]; // 6
	uint16_t device_id;                // 2
	char name[SETTING_NAME_LENGTH];    // 9
	uint8_t flags;                     // 1
} ble_badge_seen_entry_t;

ble_badge_seen_entry_t seen_db[SEEN_DB_LEN];


//
// Seen list
//

int previous_seen_write = 0; // last write location

int __find_seen_index(uint8_t *address, uint16_t device_id) {
	// return the index into the seen_db, or -1 for not found
	int read_position = previous_seen_write;
	int return_value = -1;

	// run over all entries (backwards)
	for (int i=0; i < SEEN_DB_LEN; i++) {


		if (!(seen_db[read_position].flags & SEEN_FLAG_USED)) {
			// this entry (and all older ones) are unused
			break;
		}
		if (seen_db[read_position].device_id == device_id) {
			if (!memcmp(&seen_db[read_position].address, address, BLE_GAP_ADDR_LEN)) {
				// we found a match
				return_value = read_position;
				break;
			}
		} else {
			// reposition pointer for next read
			read_position--;
			if (read_position < 0)
				read_position = SEEN_DB_LEN-1;
		}
	}
	return return_value;
}

static int seen_list_walk_position;
static int seen_list_walk_count;

void __get_seen_set_first() {
	// return the index of the first entry in seen_db, or -1 for not found
	seen_list_walk_position = previous_seen_write;
	seen_list_walk_count = SEEN_DB_LEN;
}

int __get_seen_next() {
	// return the index into the seen_db, or -1 for not found
	int return_value = -1;

	// run over all entries (backwards)
	if (seen_list_walk_count > 0) {
		if (!(seen_db[seen_list_walk_position].flags & SEEN_FLAG_USED)) {
			// this entry (and all older ones) are unused
			return -1;
		}

		return_value = seen_list_walk_position;

		// position for next read
		seen_list_walk_position--;
		if (seen_list_walk_position < 0)
			seen_list_walk_position = SEEN_DB_LEN-1;

		// see if we've run around the whole list
		seen_list_walk_count--;
	}
	return return_value;
}

void add_to_seen(uint8_t *address, uint16_t device_id, char *name, uint8_t type, uint8_t flags) {
	type &= SEEN_TYPE_MASK;
	flags |= (SEEN_FLAG_MASK & flags);
	flags |= SEEN_FLAG_USED;
	flags |= type;
	// We always add in a circular buffer fashion
	// overwriting the oldest entry
	previous_seen_write++;
	previous_seen_write %= SEEN_DB_LEN;
	memcpy(seen_db[previous_seen_write].address, address, BLE_GAP_ADDR_LEN);
	seen_db[previous_seen_write].device_id = device_id;
	seen_db[previous_seen_write].flags = flags;
	//    if (type == SEEN_TYPE_JOCO)
	strncpy(seen_db[previous_seen_write].name, name, SETTING_NAME_LENGTH);
	//  else
	//seen_db[previous_seen_write].name[0] = 0;
}

//
// Check to see if the badge is in the seen list
// if it's not, then see if it's in the db on disk
// If it's in the db, then add it to the seen list
//

uint8_t check_and_add_to_seen(uint8_t *address, uint16_t device_id, char *name, uint8_t type) {
	// return zero or the seen_flags field from the entry
	uint8_t return_value = 0;
	int read_position = __find_seen_index(address, device_id);
	if (read_position >= 0) {
		// found it
		return_value =  seen_db[read_position].flags;
	} else {
		// not in the seen list
		if (type == SEEN_TYPE_JOCO) {
			// if it's a joco badge, check the db on disk
			if (was_contacted(address, device_id)) {
				add_to_seen(address, device_id, name, SEEN_TYPE_JOCO, SEEN_FLAG_VISITED);
				return_value = (SEEN_FLAG_USED | SEEN_FLAG_VISITED);
			}
		} else if  (type == SEEN_TYPE_PEER) {
			add_to_seen(address, device_id, name, SEEN_TYPE_PEER, 0);
			return_value = SEEN_FLAG_USED;
		}
	}
	return return_value;
}

int set_seen_flags(uint8_t *address, uint16_t device_id, uint8_t flags) {
	int read_position = __find_seen_index(address, device_id);
	int return_value;
	uint8_t newflags;

	if (read_position < 0) {
		return_value = read_position;
	} else {
		newflags = (flags & SEEN_FLAG_MASK);
		newflags |= SEEN_FLAG_USED;
		seen_db[read_position].flags |= newflags;
		return_value = 0;
	}
	return return_value;
}


// Return a list into a pre-allocated array of entries

int get_nearby_badge_list(int size, ble_badge_list_menu_text_t *list) {
	int seen_index;
	int return_count = 0;
	uint8_t i;


	// If there are any slots left in the list, fill them from the seen list
	__get_seen_set_first();
	while(i < size) {
		seen_index = __get_seen_next();
		if (seen_index < 0) {
			// we're done
			return return_count;
		} else {
			// we found an entry
			memset(list[i].text, 0x00, SETTING_NAME_LENGTH);
			strncpy(list[i].text, seen_db[seen_index].name, SETTING_NAME_LENGTH);

			return_count++;
			i++;
		}
	}
	return return_count;
}

int get_nearby_badge_count() {
	int seen_index;
	int return_count = 0;


	// If there are any slots left in the list, fill them from the seen list
	__get_seen_set_first();
	while(true) {
		seen_index = __get_seen_next();
		if (seen_index < 0) {
			// we're done
			return return_count;
		} else {
			return_count++;
		}
	}
	return return_count;
}
