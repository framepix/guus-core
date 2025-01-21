
#include <cryptonote_basic/cryptonote_basic.h>
#include <cryptonote_core/blockchain.h>
#include <cryptonote_core/cryptonote_core.h>
#include <iostream>
#include "guus_nft.h"
#include <string>
#include "guus_nftdb.h"
#include "wallet/wallet2.h"
#include <sqlite3.h>
#include <iostream>
#include <stdexcept>
#include <filesystem>

namespace fs = std::filesystem;

// Creating an NFT

    bool NFTDatabaseHandler::init(cryptonote::Blockchain* blockchain, const cryptonote::network_type nettype, sqlite3 *nft_db) {
        if (!nft_db) {
            MERROR("NFTDatabaseHandler: Attempted to initialize with null database pointer");
            return false;
        }

        try {
               cryptonote::network_type m_nettype;
            // Initialize the NFT database structure
            nft_db_management::initialize_nft_database(nft_db);

            // Apply any migrations that might be necessary
            nft_db_management::apply_migrations(nft_db);

            m_nettype = nettype;


            MINFO("NFT database initialized successfully.");
            return true;
        } catch (const std::exception &e) {
            MERROR("NFTDatabaseHandler: Initialization failed: " << e.what());
            return false;
        }
    }

void create_nft_with_address(sqlite3* db, 
                             const std::string& name,
                             const std::string& description,
                             uint64_t nft_id,
                             const std::vector<uint8_t>& encrypted_address,
                             const std::string& utility_data,
                             uint64_t block_height) {
    if (name.empty() || description.empty() || encrypted_address.empty()) {
        throw std::runtime_error("NFT name, description, or encrypted address cannot be empty!");
    }

    if (name.size() > CRYPTONOTE_NFT_MAX_NAME_LENGTH ||
        description.size() > CRYPTONOTE_NFT_MAX_DESCRIPTION_LENGTH) {
        throw std::runtime_error("NFT metadata exceeds allowed limits!");
    }

    cryptonote::nft_metadata nft;
    nft.nft_name = name;
    nft.nft_description = description;
    nft.nft_id = nft_id;
    nft.utility_data = utility_data;
    nft.encrypted_address = encrypted_address;
    nft.block_height = block_height; // Associate with block height

    // Persist the NFT to the database using the save function
    try {
        // Ensure save_nft_to_db is updated to handle all fields including encrypted_address and block_height
        save_nft_to_db(db, nft);
        std::cout << "Successfully created NFT: " << name << " (ID: " << nft_id
                  << ") by address (encrypted): " << tools::type_to_hex(encrypted_address) << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error saving NFT to database: " << e.what() << std::endl;
        throw; // Re-throw the exception to be handled by the caller
    }
}

// Retrieve an NFT's details
void get_nft_details(sqlite3* db,  // Pass the database connection as an argument
                     uint64_t nft_id, 
                     uint64_t block_height) {
    try {
        // Retrieve the NFT metadata from the database using NFT ID and block height
        cryptonote::nft_metadata nft = get_nft_from_db(db, nft_id, block_height);

        // Display NFT details
        std::cout << "NFT Details:\n"
                  << "Name: " << nft.nft_name << "\n"
                  << "Description: " << nft.nft_description << "\n"
                  << "ID: " << nft.nft_id << "\n"
                  << "Utility Data: " << nft.utility_data << "\n"
                  << "Encrypted Address: " << tools::type_to_hex(nft.encrypted_address) << "\n"
                  << "Block Height: " << nft.block_height << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error retrieving NFT: " << e.what() << std::endl;
    }
}

// Transfer ownership of an NFT
void transfer_nft(sqlite3* db,
                  uint64_t nft_id,
                  const std::vector<uint8_t>& new_encrypted_address,
                  uint64_t block_height) {
    try {
        // Ensure the new encrypted address is not empty
        if (new_encrypted_address.empty()) {
            throw std::runtime_error("New owner address cannot be empty!");
        }

        // Fetch the NFT metadata from the database using nft_id and block_height
        cryptonote::nft_metadata nft = get_nft_from_db(db, nft_id, block_height);

        // Update the encrypted address of the NFT
        nft.encrypted_address = new_encrypted_address;

        // Persist the updated NFT metadata back to the database
        update_nft_in_db(db, nft);  // Pass the db connection to update the NFT

        std::cout << "Successfully transferred NFT (ID: " << nft_id
                  << ") to new address (encrypted): " << tools::type_to_hex(new_encrypted_address) << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error transferring NFT: " << e.what() << std::endl;
    }
}

// List all NFTs owned by a specific address
std::vector<cryptonote::nft_metadata> list_nfts_by_owner(sqlite3* db, const std::vector<uint8_t>& encrypted_address, uint64_t block_height) {
    std::vector<cryptonote::nft_metadata> nfts;

    sqlite3_stmt* stmt;
    const char* sql = "SELECT nft_blob FROM nft_data WHERE encrypted_address = ? AND block_height = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db)));
    }

    sqlite3_bind_blob(stmt, 1, encrypted_address.data(), encrypted_address.size(), SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, block_height);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const void* blob_data = sqlite3_column_blob(stmt, 0);
        int blob_size = sqlite3_column_bytes(stmt, 0);

        std::vector<uint8_t> blob;
        blob.resize(blob_size);
        std::memcpy(blob.data(), blob_data, blob_size);

        cryptonote::nft_metadata nft = deserialize_nft(blob);
        nfts.push_back(nft);
    }

    sqlite3_finalize(stmt);
    return nfts;
}

// Redeem a utility associated with an NFT
void redeem_nft_utility(sqlite3* db,  // Pass the database connection as an argument
                        uint64_t nft_id, 
                        uint64_t block_height) {
    try {
        // Fetch the NFT metadata from the database using nft_id and block_height
        cryptonote::nft_metadata nft = get_nft_from_db(db, nft_id, block_height);

        // Check if the NFT has utility data
        if (nft.utility_data.empty()) {
            throw std::runtime_error("This NFT does not have any associated utility!");
        }

        // Simulate redeeming the utility
        std::cout << "Redeeming utility of NFT (ID: " << nft_id
                  << "): " << nft.utility_data << std::endl;

        // Mark the NFT utility as redeemed (clear the utility data)
        nft.utility_data.clear();

        // Update the NFT in the database to reflect the redemption of the utility
        update_nft_in_db(db, nft);

        std::cout << "Utility redeemed for NFT (ID: " << nft_id << ")." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error redeeming NFT utility: " << e.what() << std::endl;
    }
}


void nft_db_management::initialize_nft_database(sqlite3* db) {
    // SQL to create the nft_data table if it doesn't exist
    const char* sql_create_table = R"(
        CREATE TABLE IF NOT EXISTS nft_data (
            nft_id INTEGER PRIMARY KEY,
            nft_blob BLOB NOT NULL,
            encrypted_address BLOB NOT NULL,
            block_height INTEGER NOT NULL
        );
    )";

    char* err_msg = nullptr;
    if (sqlite3_exec(db, sql_create_table, nullptr, nullptr, &err_msg) != SQLITE_OK) {
        std::string error = "Failed to create or update table: ";
        error += err_msg;
        sqlite3_free(err_msg);
        throw std::runtime_error(error);
    }

    // Log that the table was created or already exists
    MINFO("NFT data table initialized or already exists.");
}

void nft_db_management::apply_migrations(sqlite3* db) {
    const char* sql_check_column = "PRAGMA table_info(nft_data);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql_check_column, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement for schema check: " + std::string(sqlite3_errmsg(db)));
    }

    bool has_utility_data = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1))) == "utility_data") {
            has_utility_data = true;
            break;
        }
    }
    sqlite3_finalize(stmt);

    if (!has_utility_data) {
        const char* sql_add_column = "ALTER TABLE nft_data ADD COLUMN utility_data TEXT;";
        char* err_msg = nullptr;
        if (sqlite3_exec(db, sql_add_column, nullptr, nullptr, &err_msg) != SQLITE_OK) {
            std::string error = "Failed to add utility_data column: ";
            error += err_msg;
            sqlite3_free(err_msg);
            throw std::runtime_error(error);
        }
        MINFO("Added utility_data column to nft_data table.");
    }

    // Add more
}
