#pragma once

/// Diagnostic logging on/off?
#ifndef NDEBUG
#   define DX12_DIAGNOSTIC 1
#else
#   define DX12_DIAGNOSTIC 0
#endif

/// Enable debugging mode for shader compiler
#define SHADER_COMPILER_DEBUG (DX12_DIAGNOSTIC && 1)

/// Enable serial compilation for debugging purposes
#define SHADER_COMPILER_SERIAL (DX12_DIAGNOSTIC && 1)

/// Log allocations
#define LOG_ALLOCATION (DX12_DIAGNOSTIC && 0)

/// Log instrumentation information
#define LOG_INSTRUMENTATION (DX12_DIAGNOSTIC)

/// Log instrumentation keys that have been rejected
#define LOG_REJECTED_KEYS (DX12_DIAGNOSTIC)

/// Enable instrumentation of a specific file for debugging purposes
///  ? Instrumentation of large applications can be difficult to debug and even harder to reproduce under the same conditions.
///    When such a fault occurs, it is very useful to simply be able to iterate on a binary file.
#define SHADER_COMPILER_DEBUG_FILE (DX12_DIAGNOSTIC && 0)
