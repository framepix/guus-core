#include "nft_db.h"
#include "cryptonote_basic/nft_types.h"
#include "cryptonote_config.h"
#include <sqlite3.h>
#include <boost/filesystem.hpp>

using namespace cryptonote;

#define NFT_DB_NAME "nft.db"

NFTDB::NFTDB(const std::string& db_path) : m_db(nullptr)
{
    m_db_path = db_path + "/" + NFT_DB_NAME;
}

NFTDB::~NFTDB()
{
    if (m_db) sqlite3_close(m_db);
}

bool NFTDB::init()
{
    int rc = sqlite3_open(m_db_path.c_str(), &m_db);
    if (rc != SQLITE_OK) return false;

    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS nft_info (
            version INTEGER NOT NULL,
            nft_name TEXT NOT NULL,
            nft_description TEXT,
            nft_id INTEGER PRIMARY KEY,
            encrypted_address BLOB,
            utility_data TEXT,
            image_data BLOB,
            image_hash BLOB NOT NULL,
            block_height INTEGER,
            tx_hash BLOB NOT NULL,
            output_index INTEGER NOT NULL,
            owner BLOB NOT NULL,
            creation_height INTEGER,
            spent INTEGER DEFAULT 0
        );
    )";

    char* err_msg = nullptr;
    rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        sqlite3_free(err_msg);
        return false;
    }
    return true;
}

bool NFTDB::add_nft(const nft_info& nft)
{
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

    sqlite3_bind_int(stmt, 1, meta.version);
    sqlite3_bind_text(stmt, 2, meta.nft_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, meta.nft_description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, meta.nft_id);
    sqlite3_bind_blob(stmt, 5, meta.encrypted_address.data(), meta.encrypted_address.size(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, meta.utility_data.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 7, meta.image_data.data(), meta.image_data.size(), SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 8, &meta.image_hash, sizeof(crypto::hash), SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 9, meta.block_height);
    sqlite3_bind_blob(stmt, 10, &nft.tx_hash, sizeof(crypto::hash), SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 11, nft.output_index);
    sqlite3_bind_blob(stmt, 12, &nft.owner, sizeof(crypto::public_key), SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 13, nft.creation_height);
    sqlite3_bind_int(stmt, 14, nft.spent ? 1 : 0);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

// TODO:Implement other methods similarly with prepared statements
