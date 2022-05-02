#pragma once

// Common
#include "Assert.h"

// Std
#include <string_view>
#include <string>
#include <cstdint>
#include <memory>

// System
#ifdef _WIN64
#include <guiddef.h>
#endif

struct GlobalUID {
    static constexpr uint32_t kSize = 16;

    /// Zero initializer
    GlobalUID() {
        std::memset(uuid, 0x0, sizeof(uuid));
    }

    /// Create a new UUID
    /// \return
    static GlobalUID New();

    ///
    /// \param view
    /// \return
    static GlobalUID FromString(const std::string_view& view);

    /// Convert to string
    /// \return
    std::string ToString() const;

#ifdef _WIN64
    /// Convert to the platform GUID
    /// \return
    GUID AsPlatformGUID() const;
#endif

    /// Check if this guid is valid
    /// \return
    bool IsValid() const {
        char set = 0x0;
        for (char byte : uuid) {
            set |= byte;
        }
        return set != 0;
    }

    /// Compare two UUIDs
    bool operator==(const GlobalUID& other) const {
        return !std::memcmp(uuid, other.uuid, sizeof(uuid));
    }

private:
    /// Guid data
    char uuid[kSize];
};
