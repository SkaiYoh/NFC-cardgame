//
// Created by Nathan Davis on 2/16/26.
//

#include "arduino_protocol.h"

// TODO: Arduino serial protocol is completely unimplemented. This file is empty.
// TODO: Implement the framing/parsing layer that sits between the raw serial bytes from the
// TODO: Arduino and the NFCEvent struct consumed by nfc_reader.c. Define the wire format
// TODO: (e.g. start byte, reader index, UID length, UID bytes, checksum) and implement
// TODO: arduino_read_event() to deserialize a complete packet from the serial stream.