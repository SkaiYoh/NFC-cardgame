#include "db.h"
#include <stdio.h>
#include <stdlib.h>


int main() {
    DB db; // create db struct

    // set connection to postgres db
    const char *conninfo = getenv("DB_CONNECTION");

    if (!db_init(&db, conninfo)) {
        return 1;
    }

    PGresult *res = db_query(&db, "SELECT card_id, name FROM cards");

    if (res) {
        int rows = PQntuples(res);
        for (int i = 0; i < rows; i++) {
            printf("%s - %s\n",
                   PQgetvalue(res, i, 0),
                   PQgetvalue(res, i, 1));
        }
        PQclear(res);
    }

    db_close(&db);
    return 0;
}
