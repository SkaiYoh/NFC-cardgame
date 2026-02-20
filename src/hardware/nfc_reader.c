//
// Created by Nathan Davis on 2/16/26.
//

#include "nfc_reader.h"

// TODO: NFC hardware integration is completely unimplemented. This file is empty.
// TODO: Implement nfc_init() to open serial connections to each of the 6 Arduino NFC readers
// TODO: (3 per player), nfc_poll() to read scan events, and nfc_shutdown() for cleanup.
// TODO: Each reader should emit an NFCEvent with the scanned card_id and the reader index (0–2)
// TODO: so the game loop can call card_action_play() with the correct player and slot.