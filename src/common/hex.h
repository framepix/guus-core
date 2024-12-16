#ifndef HEX_H
#define HEX_H

#include <string>
#include <sstream>
#include <iomanip>

namespace common {

// Convert binary data to hexadecimal string
std::string iss_hex(const unsigned char* data, size_t size);

} // namespace common

#endif // HEX_H
