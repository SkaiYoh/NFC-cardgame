//
// Created by Nathan Davis on 2/16/26.
//

#include "db.h"
#include <stdio.h>
#include <string.h>

bool db_init(DB *db, const char *conninfo) {
    if (!db || !conninfo) return false;

    memset(db, 0, sizeof(DB));

    db->conn = PQconnectdb(conninfo);

    if (PQstatus(db->conn) != CONNECTION_OK) {
        fprintf(stderr, "DB connection failed: %s\n",
                PQerrorMessage(db->conn));
        PQfinish(db->conn);
        db->conn = NULL;
        db->connected = false;
        return false;
    }

    db->connected = true;
    return true;
}

void db_close(DB *db) {
    if (!db || !db->connected) return;

    PQfinish(db->conn);
    db->conn = NULL;
    db->connected = false;
}

PGresult *db_query(DB *db, const char *query) {
    if (!db || !db->connected || !query) return NULL;

    PGresult *res = PQexec(db->conn, query);

    if (PQresultStatus(res) != PGRES_TUPLES_OK &&
        PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "Query failed: %s\n",
                PQerrorMessage(db->conn));
        PQclear(res);
        return NULL;
    }

    return res;
}

PGresult *db_query_params(DB *db, const char *query, int nparams, const char *const *params) {
    if (!db || !db->connected || !query) return NULL;

    PGresult *res = PQexecParams(
        db->conn,
        query,
        nparams,
        NULL,
        params,
        NULL,
        NULL,
        0
    );

    if (PQresultStatus(res) != PGRES_TUPLES_OK &&
        PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "Param query failed: %s\n",
                PQerrorMessage(db->conn));
        PQclear(res);
        return NULL;
    }

    return res;
}

const char *db_error(DB *db) {
    if (!db || !db->conn) return "No database connection";
    return PQerrorMessage(db->conn);
}
