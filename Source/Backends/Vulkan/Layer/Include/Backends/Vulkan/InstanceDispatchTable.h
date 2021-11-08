#pragma once

// Layer
#include "Vulkan.h"

// Common
#include <Common/Allocators.h>

// Std
#include <mutex>
#include <map>

// Forward declarations
class Registry;

struct InstanceDispatchTable {
    /// Add a new table
    /// \param key the given dispatch key
    /// \param table the table to be added
    static InstanceDispatchTable* Add(void *key, InstanceDispatchTable *table) {
        std::lock_guard<std::mutex> lock(Mutex);
        Table[key] = table;
        return table;
    }

    /// Get a table
    /// \param key the dispatch key
    /// \return the table
    static InstanceDispatchTable *Get(void *key) {
        if (!key) {
            return nullptr;
        }

        std::lock_guard<std::mutex> lock(Mutex);
        return Table.at(key);
    }

    /// Populate this table
    /// \param getProcAddr the device proc address fn for the next layer
    void Populate(VkInstance instance, PFN_vkGetInstanceProcAddr getProcAddr);

    /// Get the hook address for a given name
    /// \param name the name to hook
    /// \return the hooked address, may be nullptr
    static PFN_vkVoidFunction GetHookAddress(const char *name);

    /// States
    VkInstance object;

    /// Allocators
    Allocators allocators;

    /// Shared registry
    Registry* registry;

    /// Callbacks
    PFN_vkGetInstanceProcAddr next_vkGetInstanceProcAddr;
    PFN_vkDestroyInstance     next_vkDestroyInstance;

private:
    /// Lookup
    static std::mutex                                Mutex;
    static std::map<void *, InstanceDispatchTable *> Table;
};
