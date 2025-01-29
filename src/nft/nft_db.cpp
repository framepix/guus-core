#include "nft_db.h"
#include "cryptonote_config.h"
#include <sqlite3.h>
#include <boost/filesystem.hpp>
#include <stdexcept>

using namespace cryptonote;
using namespace boost::filesystem;

#define NFT_DB_NAME "nft.db"
#define CURRENT_SCHEMA_VERSION 1

NFTDB::NFTDB(const std::string& db_path) : m_db(nullptr)
{
    m_db_path = (path(db_path) / NFT_DB_NAME).string();
}

NFTDB::~NFTDB()
{
    close_db();
}

void NFTDB::close_db()
{
    std::lock_guard<std::mutex> lock(m_db_mutex);
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

void NFTDB::throw_on_error(int rc, const char* msg)
{
    if (rc != SQLITE_OK) {
        std::string error = std::string(msg) + ": " + sqlite3_errmsg(m_db);
        throw std::runtime_error(error);
    }
}

bool NFTDB::init()
{
    std::lock_guard<std::recursive_mutex>
    
    // Close existing connection if any
    close_db();

    // Create database directory if needed
    path db_dir = path(m_db_path).parent_path();
    if (!exists(db_dir)) {
        create_directories(db_dir);
    }

    // Open database
    int rc = sqlite3_open_v2(m_db_path.c_str(), &m_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, nullptr);
    if (rc != SQLITE_OK) return false;

    // Enable foreign keys and WAL mode
    sqlite3_exec(m_db, "PRAGMA busy_timeout = 5000;", nullptr, nullptr, nullptr);
    sqlite3_exec(m_db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    sqlite3_exec(m_db, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);

    // Create tables
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS nft_info (
            version INTEGER NOT NULL,
            nft_name TEXT NOT NULL,
            nft_description TEXT,
            nft_id INTEGER PRIMARY KEY,
            encrypted_address BLOB,
            utility_data TEXT,
            image_data BLOB,
            image_hash BLOB NOT NULL CHECK(length(image_hash) = 32),
            block_height INTEGER,
            tx_hash BLOB NOT NULL CHECK(length(tx_hash) = 32),
            output_index INTEGER NOT NULL,
            owner BLOB NOT NULL CHECK(length(owner) = 32),
            creation_height INTEGER,
            spent INTEGER DEFAULT 0 CHECK(spent IN (0, 1)),
            UNIQUE(tx_hash, output_index)
        );

        CREATE INDEX IF NOT EXISTS idx_owner ON nft_info(owner);
        CREATE INDEX IF NOT EXISTS idx_creation_height ON nft_info(creation_height);
    )";

    char* err_msg = nullptr;
    rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        sqlite3_free(err_msg);
        return false;
    }

    // Schema versioning
    rc = sqlite3_exec(m_db, "PRAGMA user_version;", [](void* data, int argc, char** argv, char** colNames) -> int {
        NFTDB* self = static_cast<NFTDB*>(data);
        self->m_schema_version = argc > 0 ? atoi(argv[0]) : 0;
        return 0;
    }, this, &err_msg);
    
    if (rc != SQLITE_OK) {
        sqlite3_free(err_msg);
        return false;
    }

    return migrate_schema();
}

bool NFTDB::migrate_schema()
{
    while (m_schema_version < CURRENT_SCHEMA_VERSION) {
        std::lock_guard<std::mutex> lock(m_db_mutex);
        char* err_msg = nullptr;
        
        // Add future migration steps here
        switch (m_schema_version) {
            case 0: // Initial version
                // Already created in init()
                break;
            // Add new case blocks for future schema versions
        }

        m_schema_version++;
        std::string pragma_sql = "PRAGMA user_version = " + std::to_string(m_schema_version) + ";";
        int rc = sqlite3_exec(m_db, pragma_sql.c_str(), nullptr, nullptr, &err_msg);
        if (rc != SQLITE_OK) {
            sqlite3_free(err_msg);
            return false;
        }
    }
    return true;
}

// Transaction management
void NFTDB::begin_transaction()
{
    std::lock_guard<std::mutex> lock(m_db_mutex);
    sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
}

void NFTDB::commit_transaction()
{
    std::lock_guard<std::mutex> lock(m_db_mutex);
    sqlite3_exec(m_db, "COMMIT TRANSACTION;", nullptr, nullptr, nullptr);
}

void NFTDB::rollback_transaction()
{
    std::lock_guard<std::mutex> lock(m_db_mutex);
    sqlite3_exec(m_db, "ROLLBACK TRANSACTION;", nullptr, nullptr, nullptr);
}

bool NFTDB::add_nft(const nft_info& nft)
{
    std::lock_guard<std::mutex> lock(m_db_mutex);
    const char* sql = R"(
        INSERT INTO nft_info (
            version, nft_name, nft_description, nft_id, encrypted_address,
            utility_data, image_data, image_hash, block_height,
            tx_hash, output_index, owner, creation_height, spent
        ) VALUES (
            ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?
        )
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    const nft_metadata& meta = nft.metadata;

    try {
        sqlite3_bind_int(stmt, 1, meta.version);
        sqlite3_bind_text(stmt, 2, meta.nft_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, meta.nft_description.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 4, meta.nft_id);
        sqlite3_bind_blob(stmt, 5, meta.encrypted_address.data(), 
                         static_cast<int>(meta.encrypted_address.size()), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 6, meta.utility_data.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_blob(stmt, 7, meta.image_data.data(), 
                         static_cast<int>(meta.image_data.size()), SQLITE_TRANSIENT);
        sqlite3_bind_blob(stmt, 8, &meta.image_hash, sizeof(crypto::hash), SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 9, meta.block_height);
        sqlite3_bind_blob(stmt, 10, &nft.tx_hash, sizeof(crypto::hash), SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 11, nft.output_index);
        sqlite3_bind_blob(stmt, 12, &nft.owner, sizeof(crypto::public_key), SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 13, nft.creation_height);
        sqlite3_bind_int(stmt, 14, nft.spent ? 1 : 0);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(m_db));
        }
    } catch (...) {
        sqlite3_finalize(stmt);
        throw;
    }

    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool NFTDB::update_nft(const nft_info& nft)
{
    std::lock_guard<std::mutex> lock(m_db_mutex);
    const char* sql = R"(
        UPDATE nft_info SET
            version = ?,
            nft_name = ?,
            nft_description = ?,
            encrypted_address = ?,
            utility_data = ?,
            image_data = ?,
            image_hash = ?,
            block_height = ?,
            tx_hash = ?,
            output_index = ?,
            owner = ?,
            creation_height = ?,
            spent = ?
        WHERE nft_id = ?
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    const nft_metadata& meta = nft.metadata;

    try {
        sqlite3_bind_int(stmt, 1, meta.version);
        sqlite3_bind_text(stmt, 2, meta.nft_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, meta.nft_description.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_blob(stmt, 4, meta.encrypted_address.data(), 
                         static_cast<int>(meta.encrypted_address.size()), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, meta.utility_data.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_blob(stmt, 6, meta.image_data.data(), 
                         static_cast<int>(meta.image_data.size()), SQLITE_TRANSIENT);
        sqlite3_bind_blob(stmt, 7, &meta.image_hash, sizeof(crypto::hash), SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 8, meta.block_height);
        sqlite3_bind_blob(stmt, 9, &nft.tx_hash, sizeof(crypto::hash), SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 10, nft.output_index);
        sqlite3_bind_blob(stmt, 11, &nft.owner, sizeof(crypto::public_key), SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 12, nft.creation_height);
        sqlite3_bind_int(stmt, 13, nft.spent ? 1 : 0);
        sqlite3_bind_int64(stmt, 14, meta.nft_id);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(m_db));
        }
    } catch (...) {
        sqlite3_finalize(stmt);
        throw;
    }

    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool NFTDB::get_nft_by_id(uint64_t nft_id, nft_info& nft)
{
    std::lock_guard<std::mutex> lock(m_db_mutex);
    const char* sql = "SELECT * FROM nft_info WHERE nft_id = ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    try {
        sqlite3_bind_int64(stmt, 1, nft_id);
        rc = sqlite3_step(stmt);
        
        if (rc == SQLITE_ROW) {
            // Parse row data
            nft.metadata.version = sqlite3_column_int(stmt, 0);
            nft.metadata.nft_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            nft.metadata.nft_description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            nft.metadata.nft_id = sqlite3_column_int64(stmt, 3);
            
            // Handle blobs with size validation
            const uint8_t* encrypted_address = static_cast<const uint8_t*>(sqlite3_column_blob(stmt, 4));
            int encrypted_address_size = sqlite3_column_bytes(stmt, 4);
            nft.metadata.encrypted_address.assign(encrypted_address, encrypted_address + encrypted_address_size);
            
            nft.metadata.utility_data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            
            const uint8_t* image_data = static_cast<const uint8_t*>(sqlite3_column_blob(stmt, 6));
            int image_data_size = sqlite3_column_bytes(stmt, 6);
            nft.metadata.image_data.assign(image_data, image_data + image_data_size);
            
            // Validate fixed-size hashes
            if (sqlite3_column_bytes(stmt, 7) != sizeof(crypto::hash)) {
                throw std::runtime_error("Invalid image_hash size");
            }
            memcpy(&nft.metadata.image_hash, sqlite3_column_blob(stmt, 7), sizeof(crypto::hash));
            
            nft.metadata.block_height = sqlite3_column_int64(stmt, 8);
            
            if (sqlite3_column_bytes(stmt, 9) != sizeof(crypto::hash)) {
                throw std::runtime_error("Invalid tx_hash size");
            }
            memcpy(&nft.tx_hash, sqlite3_column_blob(stmt, 9), sizeof(crypto::hash));
            
            nft.output_index = sqlite3_column_int64(stmt, 10);
            
            if (sqlite3_column_bytes(stmt, 11) != sizeof(crypto::public_key)) {
                throw std::runtime_error("Invalid owner size");
            }
            memcpy(&nft.owner, sqlite3_column_blob(stmt, 11), sizeof(crypto::public_key));
            
            nft.creation_height = sqlite3_column_int64(stmt, 12);
            nft.spent = sqlite3_column_int(stmt, 13) != 0;
            
            return true;
        }
    } catch (...) {
        sqlite3_finalize(stmt);
        throw;
    }

    sqlite3_finalize(stmt);
    return false;
}

bool NFTDB::get_nfts_by_owner(const crypto::public_key& owner, std::vector<nft_info>& nfts)
{
    std::lock_guard<std::mutex> lock(m_db_mutex);
    const char* sql = "SELECT * FROM nft_info WHERE owner = ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    try {
        sqlite3_bind_blob(stmt, 1, &owner, sizeof(crypto::public_key), SQLITE_TRANSIENT);
        
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            nft_info nft;
            // Similar parsing as in get_nft_by_id
            // ... (omitted for brevity, use same pattern as get_nft_by_id)
            nfts.push_back(nft);
        }
    } catch (...) {
        sqlite3_finalize(stmt);
        throw;
    }

    sqlite3_finalize(stmt);
    return !nfts.empty();
}

bool NFTDB::mark_as_spent(uint64_t nft_id)
{
    std::lock_guard<std::mutex> lock(m_db_mutex);
    const char* sql = "UPDATE nft_info SET spent = 1 WHERE nft_id = ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;

    try {
        sqlite3_bind_int64(stmt, 1, nft_id);
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(m_db));
        }
    } catch (...) {
        sqlite3_finalize(stmt);
        throw;
    }

    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}
