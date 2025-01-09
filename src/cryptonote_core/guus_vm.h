#ifndef MONERO_VM_H
#define MONERO_VM_H

#include <array>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <iostream>
#include <map>
#include <algorithm>

#define MAX_CALL_DATA_SIZE 1024 * 64

// Gas pricing constants
constexpr uint64_t BASE_FEE = 2;
constexpr uint64_t VERY_LOW_FEE = 3;
constexpr uint64_t LOW_FEE = 5;
constexpr uint64_t MID_FEE = 8;
constexpr uint64_t HIGH_FEE = 10;
constexpr uint64_t EXTCODE_FEE = 700;

// Type alias for a 256-bit integer using an array of 32 bytes
using uint256_t = std::array<uint8_t, 32>;

// Helper functions for uint256_t
inline uint256_t uint256_from_uint64(uint64_t value) {
    uint256_t result = {};
    for (int i = 0; i < 8; ++i) {
        result[i] = static_cast<uint8_t>(value >> (i * 8));
    }
    return result;
}

inline uint64_t uint64_from_uint256(const uint256_t& value) {
    uint64_t result = 0;
    for (int i = 0; i < 8; ++i) {
        result |= static_cast<uint64_t>(value[i]) << (i * 8);
    }
    return result;
}

inline uint256_t add_uint256(const uint256_t& a, const uint256_t& b) {
    uint256_t result = {};
    uint16_t carry = 0;
    for (int i = 31; i >= 0; --i) {
        uint16_t sum = static_cast<uint16_t>(a[i]) + b[i] + carry;
        result[i] = static_cast<uint8_t>(sum);
        carry = sum >> 8;
    }
    return result;
}

// Enum to represent different opcodes for the VM
enum class Opcode : uint8_t {
    STOP = 0x00,
    ADD = 0x01,
    MUL = 0x02,
    SUB = 0x03,
    DIV = 0x04,
    LT = 0x10,
    GT = 0x11,
    EQ = 0x14,
    AND = 0x16,
    OR = 0x17,
    XOR = 0x18,
    MLOAD = 0x51,
    MSTORE = 0x52,
    JUMP = 0x56,
    JUMPI = 0x57,
    REVERT = 0xFD,
    // TODO: Add more opcodes as needed
};

// Account structure for state management
struct Account {
    uint256_t nonce;
    uint256_t balance;
    std::map<uint256_t, uint256_t> storage;
};

// MoneroVM class definition
class MoneroVM {
public:
    // Constructor with initial gas and optional memory limit
    explicit MoneroVM(uint64_t initial_gas, uint64_t memory_limit = 1024 * 1024);

    // Stack operations
    void push(const uint256_t& value);
    uint256_t pop();
    uint256_t peek(size_t index) const;

    // Memory operations
    void mem_store(size_t offset, const uint256_t& value);
    uint256_t mem_load(size_t offset) const;

    // Gas consumption management
    void consume_gas(uint64_t amount);

    // Execute bytecode
    bool execute(const std::vector<uint8_t>& bytecode);

    // Validate bytecode before execution
    bool validate_bytecode(const std::vector<uint8_t>& bytecode);

    // Get the remaining gas
    [[nodiscard]] uint64_t get_remaining_gas() const;

    // Set a new memory limit
    void set_memory_limit(uint64_t new_memory_limit);

    // Load bytecode into VM for deployment or execution
    void load_bytecode(const std::string& hex_bytecode);

    // Getter for bytecode
    const std::vector<uint8_t>& get_bytecode() const;

private:
    uint64_t gas;               // Available gas for contract execution
    uint64_t pc;                // Program counter (instruction pointer)
    uint64_t memory_limit;      // Memory limit (in bytes)
    std::vector<uint256_t> stack;  // The stack for operands
    std::vector<uint8_t> memory;   // The memory for storing data
    std::map<uint256_t, Account> state; // World state to manage accounts
    std::vector<uint8_t> bytecode;  // Bytecode to be executed or deployed
};

// Helper function to handle 256-bit numbers (e.g., for output or manipulation)
std::ostream& operator<<(std::ostream& os, const uint256_t& val);

#endif // MONERO_VM_H
