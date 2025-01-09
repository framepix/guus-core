#include "guess_files.h"
#include "misc_log_ex.h"  // Assuming Monero uses this for logging
#include <iostream>

namespace lns {

sqlite3* GuessFiles::db = nullptr;

bool GuessFiles::init(const std::string& dbPath) {
    int rc = sqlite3_open(dbPath.c_str(), &db);
    if (rc != SQLITE_OK) {
        MERROR("Cannot open database: " << sqlite3_errmsg(db));
        return false;
    }

    const char* sql = "CREATE TABLE IF NOT EXISTS files (id INTEGER PRIMARY KEY, name TEXT NOT NULL, data BLOB NOT NULL);";
    return execute_sql(sql);
}

bool GuessFiles::save_file(const std::string& fileName, const std::vector<uint8_t>& data) {
    if (!meets_size_requirement(data)) {
        MERROR("File data does not meet minimum size requirement.");
        return false;
    }

    const char* sql = "INSERT INTO files (name, data) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        MERROR("Failed to prepare statement: " << sqlite3_errmsg(db));
        return false;
    }

    MDEBUG("Attempting to bind parameters...");
    if (sqlite3_bind_text(stmt, 1, fileName.c_str(), -1, SQLITE_STATIC) != SQLITE_OK) {
        MERROR("Failed to bind filename: " << sqlite3_errmsg(db));
        return false;
    }
    if (sqlite3_bind_blob(stmt, 2, data.data(), static_cast<int>(data.size()), SQLITE_STATIC) != SQLITE_OK) {
        MERROR("Failed to bind file data: " << sqlite3_errmsg(db));
        return false;
    }

    MDEBUG("Executing SQL...");
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        MERROR("Failed to execute insert: Error code " << rc << ", " << sqlite3_errmsg(db));
        return false;
    }

    MINFO("File saved successfully.");
    return true;
}

void GuessFiles::cleanup() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}

bool GuessFiles::meets_size_requirement(const std::vector<uint8_t>& data) {
    return data.size() >= MIN_FILE_SIZE;
}

bool GuessFiles::execute_sql(const char* sql, void* context) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, context, &errMsg);
    if (rc != SQLITE_OK) {
        MERROR("SQL error: " << errMsg << " (SQL: " << sql << ")");
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

bool GuessFiles::bind_and_execute(sqlite3_stmt* stmt, const std::vector<uint8_t>& data, const std::string& fileName) {
    int rc = sqlite3_bind_text(stmt, 1, fileName.c_str(), -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        MERROR("Failed to bind filename: " << sqlite3_errmsg(db));
        return false;
    }

    rc = sqlite3_bind_blob(stmt, 2, data.data(), static_cast<int>(data.size()), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        MERROR("Failed to bind file data: " << sqlite3_errmsg(db));
        return false;
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        MERROR("Failed to execute insert: " << sqlite3_errmsg(db));
        return false;
    }

    return true;
}

} // namespace lns
