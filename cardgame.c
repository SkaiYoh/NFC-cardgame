#include "db.h"
#include <stdio.h>


int main() {
    DB db; // create db struct

    // set connection to postgres db
    const char *conninfo = "host=localhost port=5432 dbname=appdb user=postgres password=postgres";

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
