#include "RDTPPacket.h"
#include <string.h>

const char *RDTP_SearchingMessage = "RDTP Search";
const char *RDTP_DiscoverResponse = "RDTP Response";
#define COMMAND_AVAILABLE_MAGIC (INT8_MAX)

void RDTPPacket_init(RDTPPacket *packet)
{
    memset(packet, 0, sizeof(RDTPPacket));
}

void RDTPPacket_initWithBytes(RDTPPacket *packet, RDTPPacketBuffer *bytes, int length)
{
    packet->count = 0;
    packet->valueCount = 0;
    packet->header = bytes->header;
    for (size_t i = 0; i < (length - sizeof(packet->header)) / sizeof(packet->values[0]); ++i) {
        packet->values[i] = bytes->values[i];
    }
}

void RDTPPacket_updateValue(RDTPPacket *packet, RDTPPacketComponent component, int32_t value)
{
    packet->header |= 1 << component;
    packet->values[component] = value;
}

void RDTPPacket_setCommand(RDTPPacket *packet, RDTPPacketCommand command)
{
    packet->header = 0;
    packet->values[0] = command;
    packet->values[1] = COMMAND_AVAILABLE_MAGIC;
}

void RDTPPacket_getSendData(RDTPPacket *packet, RDTPPacketBuffer *buf, int *length)
{
    int valueCount = 0;
    buf->header = packet->header;
    if (packet->header == 0 && packet->values[1] == COMMAND_AVAILABLE_MAGIC) {
        buf->values[0] = packet->values[0];
        *length = sizeof(buf->header) + sizeof(buf->values[0]);
    } else {
        for (int i = 0; packet->header; packet->header >>= 1, ++i) {
            if (packet->header & 1) {
                buf->values[valueCount] = packet->values[i];
                ++valueCount;
            }
        }
        if (valueCount) {
            *length = valueCount * sizeof(buf->values[0]) + sizeof(buf->header);
        } else {
            *length = 0;
        }
    }
    packet->values[1] = 0; /* Clear the magic */
}

RDTPPacketResult RDTPPacket_getReceiveData(RDTPPacket *packet, int32_t *value, RDTPPacketComponent *component)
{
    if (packet->valueCount > 20) {
        return EndOfPacket;
    }
    if (packet->header) {
        while ((packet->header & 1) == 0) {
            ++packet->count;
            packet->header >>= 1;
        }
        *component = packet->count;
        *value = packet->values[packet->valueCount];
        packet->header >>= 1;
        ++packet->valueCount;
        ++packet->count;
        return DataAvailable;
    } else if (packet->count) {
        return EndOfPacket;
    } else {
        return CommandAvailable;
    }
}

RDTPPacketCommand RDTPPacket_getReceiveCommand(RDTPPacket *packet)
{
    return packet->values[0];
}
