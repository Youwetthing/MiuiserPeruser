#ifndef SYNDICATE_DB_H
#define SYNDICATE_DB_H

void syndicate_db_init(const char* path);
void syndicate_db_log(const char* turtle, const char* level, const char* message);
void syndicate_db_close();

#endif
