#ifndef EVENT_H
#define EVENT_H

#include "event_private.h"

#define EVENT_IS_DECODER_PACKET_ERROR(e)    \
    ((e) < (DECODE_EVENT_PACKET_MAX))

/* supported decoder events */

struct DecodeEvents_ {
    const char *event_name;
    uint8_t code;
};
extern const struct DecodeEvents_ DEvents[DECODE_EVENT_MAX];

#endif
