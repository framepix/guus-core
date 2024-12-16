#ifndef MONERO_VM_H
#define MONERO_VM_H

#include <vector>
#include <cstdint>
#include <stdexcept>
#include <iostream>

// Define the uint256_t type
using uint256_t = __uint128_t;

#define MAX_CALL_DATA_SIZE 1024 * 64

// Enum to represent different opcodes for the VM
enum class Opcode : uint8_t {
    STOP = 0x00,
    ADD = 0x01,
    MUL = 0x02,
    SUB = 0x03,
    DIV = 0x04,
    MLOAD = 0x10,
    MSTORE = 0x11,
    // TODO: Add more opcodes as needed
};

// MoneroVM class definition
class MoneroVM {
public:
    // Constructor with initial gas and optional memory limit
    MoneroVM(uint64_t initial_gas, uint64_t memory_limit = 1024 * 1024);

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

    // Get the remaining gas
    uint64_t get_remaining_gas() const;

    // Set a new memory limit
    void set_memory_limit(uint64_t new_memory_limit);

    bool validate_bytecode(const std::vector<uint8_t>& bytecode);

private:
    uint64_t gas;               // Available gas for contract execution
    uint64_t pc;                // Program counter (instruction pointer)
    uint64_t memory_limit;      // Memory limit (in bytes)
    std::vector<uint256_t> stack;  // The stack for operands
    std::vector<uint8_t> memory;   // The memory for storing data
};

// Helper function to handle 256-bit numbers (e.g., for output or manipulation)
std::ostream& operator<<(std::ostream& os, const uint256_t& val);

#endif // MONERO_VM_H
