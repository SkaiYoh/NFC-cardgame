-- Migration: create nfc_tags table mapping physical NFC card UIDs to game card IDs.
-- Register each physical card once with:
--   INSERT INTO nfc_tags (uid, card_id) VALUES ('04A1B2C3', 'KNIGHT_001');

CREATE TABLE IF NOT EXISTS nfc_tags (
    uid     VARCHAR(32) PRIMARY KEY,                       -- uppercase hex UID, e.g. "04A1B2C3"
    card_id VARCHAR(64) NOT NULL REFERENCES cards(card_id) -- must match a card in the cards table
);
