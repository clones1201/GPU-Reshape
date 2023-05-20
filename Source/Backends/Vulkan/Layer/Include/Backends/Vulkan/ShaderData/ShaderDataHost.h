#pragma once

// Layer
#include <Backends/Vulkan/Allocation/Allocation.h>
#include <Backends/Vulkan/ShaderData/ConstantShaderDataBuffer.h>

// Backend
#include <Backend/ShaderData/IShaderDataHost.h>
#include <Backend/ShaderData/ShaderDataInfo.h>

// Common
#include <Common/ComRef.h>

// Std
#include <vector>
#include <mutex>

// Forward declarations
struct DeviceDispatchTable;
class DeviceAllocator;

class ShaderDataHost final : public IShaderDataHost {
public:
    explicit ShaderDataHost(DeviceDispatchTable* table);
    ~ShaderDataHost();

    /// Install this host
    /// \return success state
    bool Install();

    /// Create all descriptors
    /// \param set set to be bound
    /// \param bindingOffset offset to the binding data, filled linearly from there
    void CreateDescriptors(VkDescriptorSet set, uint32_t bindingOffset);

    /// Create a constant data buffer
    /// \return buffer
    ConstantShaderDataBuffer CreateConstantDataBuffer();

    /// Create an up to date constant mapping table
    /// \return table
    ShaderConstantsRemappingTable CreateConstantMappingTable();

    /// Get the underlying buffer of a resource
    /// \param rid resource id
    /// \return buffer object
    VkBuffer GetResourceBuffer(ShaderDataID rid);

    /// Overrides
    ShaderDataID CreateBuffer(const ShaderDataBufferInfo &info) override;
    ShaderDataID CreateEventData(const ShaderDataEventInfo &info) override;
    ShaderDataID CreateDescriptorData(const ShaderDataDescriptorInfo &info) override;
    void *Map(ShaderDataID rid) override;
    void FlushMappedRange(ShaderDataID rid, size_t offset, size_t length) override;
    void Destroy(ShaderDataID rid) override;
    void Enumerate(uint32_t *count, ShaderDataInfo *out, ShaderDataTypeSet mask) override;

private:
    struct ResourceEntry {
        /// Underlying allocation
        Allocation allocation;

        /// Buffer handles
        VkBuffer buffer{VK_NULL_HANDLE};
        VkBufferView view{VK_NULL_HANDLE};

        /// Top information
        ShaderDataInfo info;
    };

private:
    /// Shared allocator
    ComRef<DeviceAllocator> deviceAllocator;

private:
    /// Parent device
    DeviceDispatchTable* table;

    /// Shared lock
    std::mutex mutex;

    /// Free indices to be used immediately
    std::vector<ShaderDataID> freeIndices;

    /// All indices, sparsely populated
    std::vector<uint32_t> indices;

    /// Linear resources
    std::vector<ResourceEntry> resources;
};
