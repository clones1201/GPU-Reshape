#pragma once

// Layer
#include "LLVMRecord.h"

struct LLVMRecordReader {
    LLVMRecordReader(const LLVMRecord& record) : record(record) {
        /* */
    }

    /// Consume an operand
    uint64_t ConsumeOp() {
        return record.Op(offset++);
    }

    /// Consume an operand
    uint32_t ConsumeOp32() {
        return static_cast<uint32_t>(ConsumeOp());
    }

    /// Consume an operand
    template<typename T>
    T ConsumeOpAs() {
        return record.OpAs<T>(offset++);
    }

    /// Consume an operand or return a default value if not present
    uint64_t ConsumeOpDefault(uint64_t _default) {
        return !Any() ? _default : ConsumeOp();
    }

    /// Any operands to consume?
    bool Any() const {
        return offset < record.opCount;
    }

    /// Get remaining operands
    uint32_t Remaining() const {
        return record.opCount - offset;
    }

    /// Get remaining operands
    uint32_t Offset() const {
        return offset;
    }

private:
    const LLVMRecord& record;

    /// Current offset
    uint32_t offset{0};
};
