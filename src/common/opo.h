#pragma once

// Helper function to handle carry in multiplication
uint16_t multiply_bytes(uint8_t a, uint8_t b, uint8_t& carry) {
    uint16_t result = static_cast<uint16_t>(a) * b + carry;
    carry = result >> 8;
    return result & 0xFF;
}

// Multiplication for uint256_t
uint256_t multiply_uint256(const uint256_t& a, const uint256_t& b) {
    uint256_t result = {};
    uint8_t carry = 0;
    
    for (int i = 0; i < 32; ++i) {
        for (int j = 0; j <= i; ++j) {
            uint8_t this_carry = 0;
            uint16_t partial = multiply_bytes(a[j], b[i - j], carry);
            partial += result[i] + this_carry;
            result[i] = partial & 0xFF;
            carry = partial >> 8;
        }
        for (int j = i + 1; j < 32; ++j) {
            result[j] += carry;
            carry = result[j] < carry ? 1 : 0; // Handle overflow for this byte
        }
    }

    // Handle any remaining carry
    for (int i = 32; carry != 0 && i < 32; ++i) {
        result[i] += carry;
        carry = result[i] < carry ? 1 : 0; // Handle overflow for this byte
    }

    return result;
}


// Helper function to handle borrowing in subtraction
uint8_t subtract_bytes(uint8_t& a, uint8_t b, bool& borrow) {
    uint16_t temp = static_cast<uint16_t>(a) - b - (borrow ? 1 : 0);
    borrow = temp > a || (borrow && temp == a);
    a = temp & 0xFF; // Only keep the least significant byte
    return a;
}

// Subtraction for uint256_t
uint256_t subtract_uint256(const uint256_t& a, const uint256_t& b) {
    uint256_t result = a;
    bool borrow = false;
    
    for (int i = 31; i >= 0; --i) {
        result[i] = subtract_bytes(result[i], b[i], borrow);
    }

    return result;
}

// Helper function to convert uint256_t to uint64_t for partial operations
uint64_t uint256_to_uint64(const uint256_t& a) {
    uint64_t result = 0;
    for (int i = 0; i < 8; ++i) {
        result |= static_cast<uint64_t>(a[i]) << (i * 8);
    }
    return result;
}

// Helper function to shift uint256_t left by one bit
void shift_left_one_bit(uint256_t& a) {
    bool carry = false;
    for (int i = 0; i < 32; ++i) {
        bool new_carry = (a[i] & 0x80) != 0;
        a[i] = (a[i] << 1) | (carry ? 1 : 0);
        carry = new_carry;
    }
}

// Division for uint256_t
uint256_t divide_uint256(const uint256_t& dividend, const uint256_t& divisor) {
    uint256_t quotient = {};
    uint256_t remainder = {};

    // Check for division by zero
    if (divisor == uint256_t{}) {
        throw std::runtime_error("Division by zero");
    }

    // Handle simple cases
    if (dividend == uint256_t{}) {
        return quotient;
    }
    if (divisor == uint256_t{1}) {
        return dividend;
    }

    // Long division algorithm
    for (int i = 255; i >= 0; --i) {
        shift_left_one_bit(remainder);
        remainder[0] |= (dividend[i / 8] >> (i % 8)) & 1;

        uint64_t temp = uint256_to_uint64(remainder);
        uint64_t divisor_part = uint256_to_uint64(divisor);

        if (temp >= divisor_part) {
            uint64_t count = 0;
            while (temp >= divisor_part && count < 0xFF) {
                temp -= divisor_part;
                ++count;
            }
            remainder = uint256_from_uint64(temp);
            quotient[i / 8] |= (count << (i % 8));
        }
    }

    return quotient;
}

// Modulo operation for uint256_t
uint256_t modulo_uint256(const uint256_t& dividend, const uint256_t& divisor) {
    uint256_t quotient = divide_uint256(dividend, divisor);
    uint256_t product = multiply_uint256(quotient, divisor);
    return subtract_uint256(dividend, product);
}
