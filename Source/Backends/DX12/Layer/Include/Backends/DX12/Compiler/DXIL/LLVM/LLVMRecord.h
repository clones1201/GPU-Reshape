#pragma once

// Common
#include <Common/Assert.h>

// Std
#include <cstdint>

struct LLVMRecord {
    /// Check if this record is of reserved id
    template<typename T>
    bool Is(T value) const {
        return value == static_cast<T>(id);
    }

    /// Interpret this record as a reserved type
    template<typename T>
    T As() const {
        return static_cast<T>(id);
    }

    /// Convert an operand to a type
    template<typename T>
    T OpAs(uint32_t i) const {
        ASSERT(i < opCount, "Operand out of bounds");
        return static_cast<T>(ops[i]);
    }

    /// Fill all operands sequentially to a given array
    /// \param out length must be the number of operands
    template<typename T>
    void FillOperands(T* out) {
        for (uint32_t i = 0; i < opCount; i++) {
            out[i] = static_cast<T>(ops[i]);
        }
    }

    /// Identifier of this record, may be reserved
    uint32_t id{~0u};

    /// Number of operands within this record
    uint32_t opCount{0};

    /// All operands
    uint64_t* ops{nullptr};

    /// Optional blob size associated
    uint64_t blobSize{0};

    /// Blob size, lifetime owned by parent module
    const uint8_t* blob{nullptr};
};
