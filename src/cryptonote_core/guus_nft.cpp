
#include <cryptonote_basic/cryptonote_basic.h>
#include <cryptonote_core/blockchain.h>
#include <cryptonote_core/cryptonote_core.h>
#include <iostream>
#include "guus_nft.h"
#include <string>
#include "guus_nftdb.h"
#include "wallet/wallet2.h"

// Creating an NFT

void create_nft_with_address(sqlite3* db,  // Pass the database connection as an argument
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
        save_nft_to_db(db, nft);  // Pass the database connection to the save function
        std::cout << "Successfully created NFT: " << name << " (ID: " << nft_id
                  << ") by address (encrypted): " << tools::type_to_hex(encrypted_address) << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error saving NFT to database: " << e.what() << std::endl;
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
std::vector<cryptonote::nft_metadata> list_nfts_by_owner(sqlite3* db,
                        const std::vector<uint8_t>& encrypted_address,
                        uint64_t block_height) {
    std::vector<cryptonote::nft_metadata> nfts;
    try {
        // Retrieve the NFTs owned by the given address from the database
         nfts = get_nfts_by_address_from_db(db, encrypted_address, block_height);

        // Check if no NFTs are found
        if (nfts.empty()) {
            std::cout << "No NFTs found for the given address." << std::endl;
         
        }

        // Display the NFTs owned by the address
        std::cout << "NFTs owned by address (encrypted): " << tools::type_to_hex(encrypted_address) << std::endl;
        for (const auto& nft : nfts) {
            std::cout << "- NFT ID: " << nft.nft_id 
                      << ", Name: " << nft.nft_name
                      << ", Description: " << nft.nft_description 
                      << std::endl;
        }
    } catch (const std::exception& e) {
        // Use cerr for error messages
        std::cerr << "Error listing NFTs by owner: " << e.what() << std::endl;
    }
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

