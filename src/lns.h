#pragma once
#include <cstdint>

namespace lns
{
enum struct mapping_type : uint16_t
{
  session,
  wallet,
  guusnet_1year,
  guusnet_2years,
  guusnet_5years,
  guusnet_10years,
  _count,
  update_record_internal,
};

constexpr uint64_t burn_needed(uint8_t /*hf_version*/, mapping_type type)
{
  uint64_t result = 0;
  switch (type)
  {
    case mapping_type::update_record_internal:
      result = 0;
      break;

    case mapping_type::guusnet_1year: /* FALLTHRU */
    case mapping_type::session: /* FALLTHRU */
    case mapping_type::wallet: /* FALLTHRU */
    default:
      result = 100 * COIN;
      break;

    case mapping_type::guusnet_2years: result = 100 * COIN; break;
    case mapping_type::guusnet_5years: result = 180 * COIN; break;
    case mapping_type::guusnet_10years: result = 220 * COIN; break;
  }
  return result;
}
}; // namespace lns

