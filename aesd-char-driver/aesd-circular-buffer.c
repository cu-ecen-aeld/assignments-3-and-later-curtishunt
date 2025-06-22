/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#include <linux/slab.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    /**
    * TODO: implement per description
    */
    size_t running_total = 0;
    uint8_t entry_index;

    // get number of entries
    uint8_t entries_count;
    if(buffer->full) {
        entries_count = AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    } else {
        entries_count = buffer->in_offs;
    }
                                           
    // loop through all valid entries in buffer
    // oldest valid entry is buffer->out_offs, newest is buffer->out_offs+entries_count
    for (uint8_t index = 0; index < entries_count; index++) {
        // modulo division to make the output wrap around correctly
        // eg. out_offs = 9, index = 7, 9+7=16, 16%10=6
        entry_index = (buffer->out_offs + index) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        struct aesd_buffer_entry *entry = &buffer->entry[entry_index];

        // check if running_total + new entry is larger than the desired offset
        // If its larger this would mean that this particular entry is the desired entry
        if (char_offset < (running_total + entry->size)) {
            // Find how many bytes into the current entry the desired offset is
            *entry_offset_byte_rtn = char_offset - running_total;
            return entry;
        }

        running_total += entry->size;
    }

    // Offset exceeds total data stored
    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    /**
    * TODO: implement per description
    */

    // increment out_offs only if buffer is full
    if (buffer->full)  {
        // free the data that will be overwritten 
        kfree(buffer->entry[buffer->out_offs].buffptr);
        buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    // Add entry at the current write offset
    buffer->entry[buffer->in_offs] = *add_entry;

    // increment in_offs no matter what
    buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    // buffer is full if in_offs is at same spot as out_offs
    buffer->full = (buffer->in_offs == buffer->out_offs);
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}