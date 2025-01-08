#include "guus_vm.h"
#include <stdexcept>
#include <iostream>
#include <vector>

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
    // Ensure memory is large enough, expanding with gas cost if necessary
    while (memory.size() <= offset + 31) {
        memory.push_back(0);
        consume_gas(3); // Example cost for memory expansion
    }

    // Store the value in big-endian format
    for (int i = 0; i < 32; ++i) {
        memory[offset + i] = static_cast<uint8_t>((value >> (8 * (31 - i))) & 0xFF);
    }

    // Check for memory usage limit
    if (memory.size() > memory_limit) {
        throw std::runtime_error("Memory limit exceeded");
    }
}

// Load a value from memory at a specified offset
uint256_t MoneroVM::mem_load(size_t offset) const {
    if (offset + 31 >= memory.size()) throw std::runtime_error("Memory access out of bounds");
    uint256_t result = 0;
    for (int i = 0; i < 32; ++i) {
        result |= static_cast<uint256_t>(memory[offset + i]) << (8 * (31 - i));
    }
    return result;
}

// Consume gas, throwing an error if not enough gas is available
void MoneroVM::consume_gas(uint64_t amount) {
    if (gas < amount) throw std::runtime_error("Insufficient gas");
    gas -= amount;
}

// Execute bytecode with optional memory limit
bool MoneroVM::execute(const std::vector<uint8_t>& bytecode) {
    while (pc < bytecode.size()) {
        if (gas == 0) throw std::runtime_error("Out of gas");

        uint8_t op = bytecode[pc++];
        switch (static_cast<Opcode>(op)) {
            case Opcode::STOP:
                std::cout << "Execution stopped." << std::endl;
                return true;

            case Opcode::ADD: {
                consume_gas(3); // Gas cost for ADD
                uint256_t a = pop(), b = pop();
                push(a + b);
                break;
            }

            case Opcode::MUL: {
                consume_gas(5); // Gas cost for MUL
                uint256_t a = pop(), b = pop();
                push(a * b);
                break;
            }

            case Opcode::SUB: {
                consume_gas(3); // Gas cost for SUB
                uint256_t a = pop(), b = pop();
                push(a - b);
                break;
            }

            case Opcode::DIV: {
                consume_gas(5); // Gas cost for DIV
                uint256_t a = pop(), b = pop();
                if (b == 0) throw std::runtime_error("Division by zero");
                push(a / b);
                break;
            }

            case Opcode::MLOAD: {
                consume_gas(3); // Gas cost for MLOAD
                size_t offset = static_cast<size_t>(pop());
                push(mem_load(offset));
                break;
            }

            case Opcode::MSTORE: {
                consume_gas(3); // Gas cost for MSTORE
                uint256_t value = pop();
                size_t offset = static_cast<size_t>(pop());
                mem_store(offset, value);
                break;
            }

            // Add more opcodes as needed...

            default:
                throw std::runtime_error("Unknown opcode");
        }

        // Log remaining gas after each operation
        std::cout << "Remaining gas: " << gas << std::endl;
    }
    return true;
}

bool MoneroVM::validate_bytecode(const std::vector<uint8_t>& bytecode) {
    size_t pc = 0;
    uint64_t max_stack_size = 0;
    int64_t current_stack_size = 0;

    while (pc < bytecode.size()) {
        uint8_t op = bytecode[pc++];
        switch (static_cast<Opcode>(op)) {
            case Opcode::STOP:
                // No stack change
                break;

            case Opcode::ADD:
            case Opcode::MUL:
            case Opcode::SUB:
            case Opcode::DIV:
                // Pops 2 values, pushes 1
                current_stack_size -= 1;
                break;

            case Opcode::MLOAD:
                // Pops 1 value, pushes 1 value
                break;

            case Opcode::MSTORE:
                // Pops 2 values, stores but doesn't push
                current_stack_size -= 2;
                break;

            // Add validation for other opcodes here...

            default:
                std::cerr << "Validation: Unknown opcode at position " << (pc - 1) << std::endl;
                return false;
        }

        // Check for stack underflow
        if (current_stack_size < 0) {
            std::cerr << "Validation: Stack underflow at opcode position " << (pc - 1) << std::endl;
            return false;
        }

        // Keep track of max stack size
        max_stack_size = std::max(max_stack_size, static_cast<uint64_t>(current_stack_size));

        // Check if max stack size exceeds the limit
        if (max_stack_size > 1024) {
            std::cerr << "Validation: Maximum stack size exceeded during validation" << std::endl;
            return false;
        }
    }

    // Check if there's anything left on the stack at the end.
    if (current_stack_size != 0) {
        std::cerr << "Validation: Stack not empty at end of bytecode" << std::endl;
        return false;
    }

    std::cout << "Bytecode validation successful. Max stack size was " << max_stack_size << std::endl;
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
