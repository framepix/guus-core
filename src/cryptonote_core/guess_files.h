#pragma once

#include <string>
#include <vector>
#include <sqlite3.h>
#include <cstdint>

namespace lns {

class GuessFiles {
private:
    static const std::size_t MIN_FILE_SIZE = 1 * 1024 * 1024;  // 1MB in bytes
    static sqlite3* db;

public:
    /**
     * @brief Initialize the database connection for file storage.
     * @param dbPath Path to the SQLite database file.
     * @return true if initialization was successful, false otherwise.
     */
    static bool init(const std::string& dbPath);

    /**
     * @brief Save a file's data to the database.
     * @param fileName The name to store the file under in the database.
     * @param data The binary data of the file to be saved.
     * @return true if the file was saved successfully, false otherwise.
     */
    static bool save_file(const std::string& fileName, const std::vector<uint8_t>& data);

    /**
     * @brief Clean up the database connection.
     */
    static void cleanup();

private:
    /**
     * @brief Check if the data meets the minimum size requirement.
     * @param data The vector of bytes to check.
     * @return true if the data size is at least MIN_FILE_SIZE, false otherwise.
     */
    static bool meets_size_requirement(const std::vector<uint8_t>& data);

    /**
     * @brief Execute SQL command.
     * @param sql SQL statement to execute.
     * @param context Optional context for SQL callback functions.
     * @return true if the SQL was executed successfully, false otherwise.
     */
    static bool execute_sql(const char* sql, void* context = nullptr);

    /**
     * @brief Bind parameters to SQL statement and execute it.
     * @param stmt Prepared SQLite statement.
     * @param data Data to bind to the statement.
     * @param fileName Filename to bind to the statement.
     * @return true if binding and execution were successful, false otherwise.
     */
    static bool bind_and_execute(sqlite3_stmt* stmt, const std::vector<uint8_t>& data, const std::string& fileName);
};

} // namespace lns
