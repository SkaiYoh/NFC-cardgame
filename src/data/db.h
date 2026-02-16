//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_DB_H
#define NFC_CARDGAME_DB_H

#include <libpq-fe.h>
#include <stdbool.h>

typedef struct {
    PGconn *conn;
    bool connected;
} DB;

bool db_init(DB *db, const char *conninfo);
void db_close(DB *db);
PGresult *db_query(DB *db, const char *query);
PGresult *db_query_params(DB *db, const char *query, int nparams, const char *const *params);
const char *db_error(DB *db);

#endif //NFC_CARDGAME_DB_H
