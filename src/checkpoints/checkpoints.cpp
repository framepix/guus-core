// Copyright (c) 2014-2022, The Monero Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#include "checkpoints.h"

#include "common/dns_utils.h"
#include "string_tools.h"
#include "storages/portable_storage_template_helper.h" // epee json include
#include "serialization/keyvalue_serialization.h"
#include <vector>

#include <iostream>
#include <sstream>
#include <fstream>

using namespace epee;

#include "common/guus_integration_test_hooks.h"

#undef GUUS_DEFAULT_LOG_CATEGORY
#define GUUS_DEFAULT_LOG_CATEGORY "checkpoints"

namespace cryptonote
{
  /**
   * @brief struct for loading a checkpoint from json
   */
  struct t_hashline
  {
    uint64_t height; //!< the height of the checkpoint
    std::string hash; //!< the hash for the checkpoint
        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(height)
          KV_SERIALIZE(hash)
        END_KV_SERIALIZE_MAP()
  };

  /**
   * @brief struct for loading many checkpoints from json
   */
  struct t_hash_json {
    std::vector<t_hashline> hashlines; //!< the checkpoint lines from the file
        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(hashlines)
        END_KV_SERIALIZE_MAP()
  };

  //---------------------------------------------------------------------------
  checkpoints::checkpoints()
  {
        add_checkpoints(super_checkpoint_height, super_checkpoint_hash);
  }
  //---------------------------------------------------------------------------
  bool checkpoints::add_checkpoint(uint64_t height, const std::string& hash_str, const std::string& difficulty_str)
  {
    crypto::hash h = crypto::null_hash;
    bool r = epee::string_tools::hex_to_pod(hash_str, h);
    CHECK_AND_ASSERT_MES(r, false, "Failed to parse checkpoint hash string into binary representation!");

    // return false if adding at a height we already have AND the hash is different
    if (m_points.count(height))
    {
      CHECK_AND_ASSERT_MES(h == m_points[height], false, "Checkpoint at given height already exists, and hash for new checkpoint was different!");
    }
    m_points[height] = h;
    if (!difficulty_str.empty())
    {
      try
      {
        difficulty_type difficulty(difficulty_str);
        if (m_difficulty_points.count(height))
        {
          CHECK_AND_ASSERT_MES(difficulty == m_difficulty_points[height], false, "Difficulty checkpoint at given height already exists, and difficulty for new checkpoint was different!");
        }
        m_difficulty_points[height] = difficulty;
      }
      catch (...)
      {
        LOG_ERROR("Failed to parse difficulty checkpoint: " << difficulty_str);
        return false;
      }
    }
    return true;
  }
  //---------------------------------------------------------------------------
  bool checkpoints::is_in_checkpoint_zone(uint64_t height) const
  {
    return !m_points.empty() && (height <= (--m_points.end())->first);
  }
  //---------------------------------------------------------------------------
  bool checkpoints::check_block(uint64_t height, const crypto::hash& h, bool& is_a_checkpoint) const
  {
    auto it = m_points.find(height);
    is_a_checkpoint = it != m_points.end();
    if(!is_a_checkpoint)
      return true;

    if(it->second == h)
    {
      MINFO("CHECKPOINT PASSED FOR HEIGHT " << height << " " << h);
      return true;
    }else
    {
      MWARNING("CHECKPOINT FAILED FOR HEIGHT " << height << ". EXPECTED HASH: " << it->second << ", FETCHED HASH: " << h);
      return false;
    }
  }
  //---------------------------------------------------------------------------
  bool checkpoints::check_block(uint64_t height, const crypto::hash& h) const
  {
    bool ignored;
    return check_block(height, h, ignored);
  }
  //---------------------------------------------------------------------------
  //FIXME: is this the desired behavior?
  bool checkpoints::is_alternative_block_allowed(uint64_t blockchain_height, uint64_t block_height) const
  {
    if (0 == block_height)
      return false;

    auto it = m_points.upper_bound(blockchain_height);
    // Is blockchain_height before the first checkpoint?
    if (it == m_points.begin())
      return true;

    --it;
    uint64_t checkpoint_height = it->first;
    return checkpoint_height < block_height;
  }
  //---------------------------------------------------------------------------
  uint64_t checkpoints::get_max_height() const
  {
    std::map< uint64_t, crypto::hash >::const_iterator highest = 
        std::max_element( m_points.begin(), m_points.end(),
                         ( boost::bind(&std::map< uint64_t, crypto::hash >::value_type::first, _1) < 
                           boost::bind(&std::map< uint64_t, crypto::hash >::value_type::first, _2 ) ) );
    return highest->first;
  }
  //---------------------------------------------------------------------------
  const std::map<uint64_t, crypto::hash>& checkpoints::get_points() const
  {
    return m_points;
  }
  //---------------------------------------------------------------------------
  const std::map<uint64_t, difficulty_type>& checkpoints::get_difficulty_points() const
  {
    return m_difficulty_points;
  }

  bool checkpoints::check_for_conflicts(const checkpoints& other) const
  {
    for (auto& pt : other.get_points())
    {
      if (m_points.count(pt.first))
      {
        CHECK_AND_ASSERT_MES(pt.second == m_points.at(pt.first), false, "Checkpoint at given height already exists, and hash for new checkpoint was different!");
      }
    }
    return true;
  }

  bool checkpoints::init_default_checkpoints(network_type nettype)
  {
    if (nettype == TESTNET)
    {
      ADD_CHECKPOINT2(0,     "48ca7cd3c8de5b6a4d53d2861fbdaedca141553559f9be9520068053cda8430b", "0x1");
      ADD_CHECKPOINT2(1000000, "46b690b710a07ea051bc4a6b6842ac37be691089c0f7758cfeec4d5fc0b4a258", "0x7aaad7153");
      ADD_CHECKPOINT2(1058600, "12904f6b4d9e60fd875674e07147d2c83d6716253f046af7b894c3e81da7e1bd", "0x971efd119");
      ADD_CHECKPOINT2(1450000, "87562ca6786f41556b8d5b48067303a57dc5ca77155b35199aedaeca1550f5a0", "0xa639e2930e");
      return true;
    }
    if (nettype == STAGENET)
    {
      ADD_CHECKPOINT2(0,       "76ee3cc98646292206cd3e86f74d88b4dcc1d937088645e9b0cbca84b7ce74eb", "0x1");
      ADD_CHECKPOINT2(10000,   "1f8b0ce313f8b9ba9a46108bfd285c45ad7c2176871fd41c3a690d4830ce2fd5", "0x1d73ba");
      ADD_CHECKPOINT2(550000,  "409f68cddd8e74b37469b41c1e61250d81c5776b42264f416d5d27c4626383ed", "0x5f3d4d03e");
      return true;
    }
     ADD_CHECKPOINT2(0,   "e9a4e09fa64d650fd72d713c56172fa7e85ce43727b66c21396bc69daa89db28", "0x1");
     ADD_CHECKPOINT2(50,  "2a1fc9088cdd3caa2e800d24f234ecae4334628f42534a034f5cda113bb3796b", "0x62f61");
     ADD_CHECKPOINT2(100, "78bde018f42c52a7578503cb7c76963f8f4f8f37765b57b7d860791585ca759d", "0x89d6c");
     ADD_CHECKPOINT2(150, "5eaa0cebb4b6f511f728911c88910a3f9cc8a23056f659c1f0f5cd3121304bb8", "0xbba03");
     ADD_CHECKPOINT2(200, "ab4f816856f7da69483f431d5e7c7761746d30f7a2edc216d4bb27e984e9b562", "0xf98eb");
     ADD_CHECKPOINT2(250, "05bc86bb0044ea72ea97bed2ea9ef72c41cddae75614f6b7d44bc8e21dd3763d", "0x145800");
     ADD_CHECKPOINT2(300, "15fe17d79ebc8caccd5000c6336ca61c9b82b6f97c4beafa5d962120ef590a8d", "0x1a02dd");
     ADD_CHECKPOINT2(350, "64825514f2393cdf0ac5e9a77da90a3f9480441f689a027a2d90e0d30c169258", "0x207e78");

    return true;
  }

  bool checkpoints::load_checkpoints_from_json(const std::string &json_hashfile_fullpath)
  {
    boost::system::error_code errcode;
    if (! (boost::filesystem::exists(json_hashfile_fullpath, errcode)))
    {
      LOG_PRINT_L1("Blockchain checkpoints file not found");
      return true;
    }

    LOG_PRINT_L1("Adding checkpoints from blockchain hashfile");

    uint64_t prev_max_height = get_max_height();
    LOG_PRINT_L1("Hard-coded max checkpoint height is " << prev_max_height);
    t_hash_json hashes;
    if (!epee::serialization::load_t_from_json_file(hashes, json_hashfile_fullpath))
    {
      MERROR("Error loading checkpoints from " << json_hashfile_fullpath);
      return false;
    }
    for (std::vector<t_hashline>::const_iterator it = hashes.hashlines.begin(); it != hashes.hashlines.end(); )
    {
      uint64_t height;
      height = it->height;
      if (height <= prev_max_height) {
	LOG_PRINT_L1("ignoring checkpoint height " << height);
      } else {
	std::string blockhash = it->hash;
	LOG_PRINT_L1("Adding checkpoint height " << height << ", hash=" << blockhash);
	ADD_CHECKPOINT(height, blockhash);
      }
      ++it;
    }

    return true;
  }

  bool checkpoints::load_checkpoints_from_dns(network_type nettype)
  {
    return true;
  }

  bool checkpoints::load_new_checkpoints(const std::string &json_hashfile_fullpath, network_type nettype, bool dns)
  {
    bool result;

    result = load_checkpoints_from_json(json_hashfile_fullpath);
    if (dns)
    {
      result &= load_checkpoints_from_dns(nettype);
    }

    return result;
  }

    bool checkpoints::add_checkpoints(uint64_t height, const std::string& hash) {
        checkpoint_list[height] = hash;
        return true;
    }

    // Loads checkpoints from a file (each line should be "height, hash")
    bool checkpoints::load_checkpoints_from_file(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to open file " << filename << std::endl;
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            uint64_t height;
            std::string hash;

            // Read height and hash from the line
            if (std::getline(ss, hash, ',') && ss >> height) {
                add_checkpoint(height, hash);
            }
        }

        file.close();
        return true;
    }

    // Saves checkpoints to a file (each line will be "height, hash")
    bool checkpoints::save_checkpoints_to_file(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to open file " << filename << std::endl;
            return false;
        }

        for (const auto& checkpoint : checkpoint_list) {
            file << checkpoint.second << "," << checkpoint.first << std::endl;
        }

        file.close();
        return true;
    }

    bool checkpoints::validate_block(uint64_t height, const std::string& block_hash) {
        // Check if the block matches the super checkpoint hash
        if (height == super_checkpoint_height && block_hash == super_checkpoint_hash) {
            std::cout << "Super checkpoint validated!" << std::endl;
            return true;
        }

        // Check if the block is in the checkpoint list
        if (checkpoint_list.find(height) != checkpoint_list.end()) {
            const std::string& stored_hash = checkpoint_list[height];
            if (stored_hash == block_hash) {
                std::cout << "Block at height " << height << " is valid!" << std::endl;
                return true;
            }
        }

        std::cerr << "Block at height " << height << " is invalid!" << std::endl;
        return false;
    }

}
