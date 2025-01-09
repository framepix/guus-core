#include "guus_vm.h"
#include <stdexcept>
#include <iostream>
#include <vector>
#include <map>
#include <algorithm>
#include <iomanip>
#include "common/opo.h"
#include "string_tools.h"

// Helper functions for uint256_t operations

// Constructor for MoneroVM with initial gas
MoneroVM::MoneroVM(uint64_t initial_gas, uint64_t memory_limit)
    : gas(initial_gas), pc(0), memory_limit(memory_limit) {}

// Push a value onto the stack
void MoneroVM::push(const uint256_t& value) {
    if (stack.size() >= 1024) throw std::runtime_error("Stack overflow");
    stack.push_back(value);
}

// Pop a value from the stack
uint256_t MoneroVM::pop() {
    if (stack.empty()) throw std::runtime_error("Stack underflow");
    uint256_t val = stack.back();
    stack.pop_back();
    return val;
}

// Peek a value from the stack without removing it
uint256_t MoneroVM::peek(size_t index) const {
    if (index >= stack.size()) throw std::runtime_error("Stack index out of bounds");
    return stack[stack.size() - 1 - index];
}

// Store a value in memory at a specified offset (big-endian format)
void MoneroVM::mem_store(size_t offset, const uint256_t& value) {
    size_t new_size = std::max(offset + 32, memory.size());
    size_t mem_words = (new_size + 31) / 32;
    size_t old_words = memory.size() / 32;
    uint64_t gas_cost = (mem_words * mem_words) / 512 - (old_words * old_words) / 512; // Quadratic growth
    consume_gas(gas_cost);

    if (memory.size() < new_size) {
        memory.resize(new_size, 0);
    }

    for (int i = 0; i < 32; ++i) {
        memory[offset + i] = value[i];
    }

    if (memory.size() > memory_limit) {
        throw std::runtime_error("Memory limit exceeded");
    }
}

// Load a value from memory at a specified offset
uint256_t MoneroVM::mem_load(size_t offset) const {
    if (offset + 31 >= memory.size()) throw std::runtime_error("Memory access out of bounds");
    uint256_t result = {};
    for (int i = 0; i < 32; ++i) {
        result[i] = memory[offset + i];
    }
    return result;
}

// Consume gas, throwing an error if not enough gas is available
void MoneroVM::consume_gas(uint64_t amount) {
    if (gas < amount) throw std::runtime_error("Insufficient gas");
    gas -= amount;
}

// Execute bytecode 
bool MoneroVM::execute(const std::vector<uint8_t>& bytecode) {
    while (pc < bytecode.size()) {
        if (gas == 0) throw std::runtime_error("Out of gas");

        uint8_t op = bytecode[pc++];
        switch (static_cast<Opcode>(op)) {
            case Opcode::STOP:
                return true;

            case Opcode::ADD:
                consume_gas(VERY_LOW_FEE);
                push(add_uint256(pop(), pop()));
                break;

            case Opcode::MUL:
                consume_gas(LOW_FEE);
                {
                 uint256_t b = pop(), a = pop();
                 push(multiply_uint256(a, b));
                }
                break;
            case Opcode::SUB:
                consume_gas(VERY_LOW_FEE);
                {
                 uint256_t b = pop(), a = pop();
                 push(subtract_uint256(a, b));
                }
                break;

            case Opcode::DIV:
                consume_gas(LOW_FEE);
                {
                    uint256_t b = pop(), a = pop();
                    if (b == uint256_from_uint64(0)) {
                        push(uint256_from_uint64(0)); // Ethereum's EVM returns 0 for division by zero
                    } else {
                     push(divide_uint256(a, b));
                    }
                }
                break;

            case Opcode::LT:
                consume_gas(VERY_LOW_FEE);
                {
                    uint256_t b = pop(), a = pop();
                    push(uint256_from_uint64(a < b ? 1 : 0));
                }
                break;

            case Opcode::GT:
                consume_gas(VERY_LOW_FEE);
                {
                    uint256_t b = pop(), a = pop();
                    push(uint256_from_uint64(a > b ? 1 : 0));
                }
                break;

            case Opcode::EQ:
                consume_gas(VERY_LOW_FEE);
                {
                    uint256_t b = pop(), a = pop();
                    push(uint256_from_uint64(a == b ? 1 : 0));
                }
                break;

            case Opcode::AND:
                consume_gas(VERY_LOW_FEE);
                {
                    uint256_t b = pop(), a = pop();
                    uint256_t result = {};
                    for (int i = 0; i < 32; ++i) {
                        result[i] = a[i] & b[i];
                    }
                    push(result);
                }
                break;

            case Opcode::OR:
                consume_gas(VERY_LOW_FEE);
                {
                    uint256_t b = pop(), a = pop();
                    uint256_t result = {};
                    for (int i = 0; i < 32; ++i) {
                        result[i] = a[i] | b[i];
                    }
                    push(result);
                }
                break;

            case Opcode::XOR:
                consume_gas(VERY_LOW_FEE);
                {
                    uint256_t b = pop(), a = pop();
                    uint256_t result = {};
                    for (int i = 0; i < 32; ++i) {
                        result[i] = a[i] ^ b[i];
                    }
                    push(result);
                }
                break;

            case Opcode::MLOAD:
                consume_gas(VERY_LOW_FEE);
                {
                    uint256_t offset = pop();
                    push(mem_load(uint64_from_uint256(offset)));
                }
                break;

            case Opcode::MSTORE:
                consume_gas(VERY_LOW_FEE);
                {
                    uint256_t value = pop();
                    uint256_t offset = pop();
                    mem_store(uint64_from_uint256(offset), value);
                }
                break;

            case Opcode::JUMP:
                consume_gas(MID_FEE);
                pc = uint64_from_uint256(pop());
                break;

            case Opcode::JUMPI:
                consume_gas(HIGH_FEE);
                {
                    uint256_t condition = pop();
                    uint256_t dest = pop();
                    if (uint64_from_uint256(condition) != 0) pc = uint64_from_uint256(dest);
                }
                break;

            case Opcode::REVERT:
                consume_gas(0); // No cost for revert itself, but gas used up to this point is not returned
                throw std::runtime_error("Revert");

            default:
                throw std::runtime_error("Unknown opcode");
        }
    }
    return true;
}

bool MoneroVM::validate_bytecode(const std::vector<uint8_t>& bytecode) {
    size_t pc = 0;
    uint64_t max_stack_size = 0;
    int64_t current_stack_size = 0;
    std::map<size_t, bool> jumpdests;

    while (pc < bytecode.size()) {
        uint8_t op = bytecode[pc++];
        switch (static_cast<Opcode>(op)) {
            case Opcode::STOP:
                break;

            case Opcode::JUMP:
                if (!jumpdests[uint64_from_uint256(peek(0))]) {
                    return false; // Jump to non-jumpdest
                }
                current_stack_size -= 1;
                break;

            case Opcode::JUMPI:
                if (!jumpdests[uint64_from_uint256(peek(1))]) {
                    return false; // Jump to non-jumpdest
                }
                current_stack_size -= 2;
                break;

            case Opcode::ADD:
            case Opcode::MUL:
            case Opcode::SUB:
            case Opcode::DIV:
            case Opcode::LT:
            case Opcode::GT:
            case Opcode::EQ:
            case Opcode::AND:
            case Opcode::OR:
            case Opcode::XOR:
                current_stack_size -= 1;
                break;

            case Opcode::MLOAD:
                break; // No net change in stack size

            case Opcode::MSTORE:
                current_stack_size -= 2;
                break;

            case Opcode::REVERT:
                return true; // Revert ends execution

            default:
                if (op == 0x5B) { // JUMPDEST
                    jumpdests[pc - 1] = true;
                } else {
                    return false; // Unknown opcode
                }
        }

        if (current_stack_size < 0) {
            return false; // Stack underflow
        }

        max_stack_size = std::max(max_stack_size, static_cast<uint64_t>(current_stack_size));
        if (max_stack_size > 1024) {
            return false; // Maximum stack size exceeded
        }
    }

    if (current_stack_size != 0) {
        return false; // Stack not empty at end of bytecode
    }

    return true;
}

// Get the remaining gas
uint64_t MoneroVM::get_remaining_gas() const {
    return gas;
}

// Optional: Set memory limit if it's not passed in constructor
void MoneroVM::set_memory_limit(uint64_t new_memory_limit) {
    memory_limit = new_memory_limit;
}

// Load bytecode into VM for deployment or execution
void MoneroVM::load_bytecode(const std::string& hex_bytecode) {
    std::string bytecode_str;
    if (!epee::string_tools::parse_hexstr_to_binbuff(hex_bytecode, bytecode_str)) {
        throw std::runtime_error("Failed to convert bytecode hex to binary.");
    }
    bytecode.assign(bytecode_str.begin(), bytecode_str.end());
}

// Getter for bytecode
const std::vector<uint8_t>& MoneroVM::get_bytecode() const {
    return bytecode;
}
// Helper for outputting uint256_t
std::ostream& operator<<(std::ostream& os, const uint256_t& val) {
    for (int i = 31; i >= 0; --i) {
        os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(val[i]);
    }
    return os;
}
