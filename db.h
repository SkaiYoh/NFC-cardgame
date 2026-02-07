#ifndef DB_H
#define DB_H

#include <libpq-fe.h>
#include <stdbool.h>

/* Database handle */
typedef struct {
    PGconn *conn;
    bool connected;
} DB;

/* Initialize database connection
 * Returns true on success, false on failure */
bool db_init(DB *db, const char *conninfo);

/* Close database connection */
void db_close(DB *db);

/* Execute a query (simple wrapper)
 * Caller must PQclear() the result */
PGresult *db_query(DB *db, const char *query);

/* Execute parameterized query (recommended)
 * params = array of strings
 * nparams = number of params
 * Caller must PQclear() the result */
PGresult *db_query_params(
    DB *db,
    const char *query,
    int nparams,
    const char *const *params
);

/* Utility */
const char *db_error(DB *db);

#endif /* DB_H */
