#pragma once

// Layer
#include <Backends/DX12/Detour.Gen.h>
#include <Backends/DX12/TrackedObject.h>
#include <Backends/DX12/DependentObject.h>
#include <Backends/DX12/Compiler/ShaderSet.h>
#include <Backends/DX12/Resource/HeapTable.h>
#include <Backends/DX12/Resource/PhysicalResourceIdentifierMap.h>
#include <Backends/DX12/FeatureProxies.Gen.h>

// Backend
#include <Backend/Environment.h>
#include <Backend/EventDataStack.h>

// Bridge
#include <Bridge/Log/LogBuffer.h>

// Common
#include <Common/ComRef.h>
#include <Common/Registry.h>
#include <Common/Allocators.h>

// Forward declarations
class ShaderSet;
class IFeature;
struct ShaderState;
struct CommandQueueState;
struct PipelineState;
class InstrumentationController;
class FeatureController;
class MetadataController;
class IBridge;
class ShaderExportHost;
class ShaderDataHost;
class ShaderExportStreamer;
class ShaderSGUIDHost;
class DeviceAllocator;
class ShaderProgramHost;

struct __declspec(uuid("548FDFD6-37E2-461C-A599-11DA5290F06E")) DeviceState {
    DeviceState(const Allocators& allocators)
        : allocators(allocators),
          states_Shaders(allocators.Tag(kAllocTracking)),
          states_Pipelines(allocators.Tag(kAllocTracking)),
          states_Queues(allocators.Tag(kAllocTracking)),
          heapTable(allocators.Tag(kAllocTracking)),
          physicalResourceIdentifierMap(allocators.Tag(kAllocPRMT)),
          dependencies_shaderPipelines(allocators.Tag(kAllocTracking)),
          features(allocators) { }
    
    ~DeviceState();

    /// Owned object
    ID3D12Device* object{nullptr};
    
    /// Shared allocators
    Allocators allocators;

    /// Shared registry
    Registry registry;

    /// All shared shader sets
    ShaderSet shaderSet;

    /// Message bridge
    ComRef<IBridge> bridge;

    /// Shared hosts
    ComRef<ShaderExportHost> exportHost;
    ComRef<ShaderDataHost> shaderDataHost;

    /// Shared export streamer
    ComRef<ShaderExportStreamer> exportStreamer;

    /// Shared device allocator
    ComRef<DeviceAllocator> deviceAllocator;

    /// Shared SGUID host
    ComRef<ShaderSGUIDHost> sguidHost{nullptr};

    /// Tracked objects
    TrackedObject<ShaderState> states_Shaders;
    TrackedObject<PipelineState> states_Pipelines;
    TrackedObject<CommandQueueState> states_Queues;

    /// Sorted heap table
    HeapTable heapTable;

    /// Physical identifier map
    PhysicalResourceIdentifierMap physicalResourceIdentifierMap;

    /// Dependency objects
    DependentObject<ShaderState, PipelineState> dependencies_shaderPipelines;

    /// Controllers
    ComRef<InstrumentationController> instrumentationController{nullptr};
    ComRef<FeatureController> featureController{nullptr};
    ComRef<MetadataController> metadataController{nullptr};

    /// User programs
    ComRef<ShaderProgramHost> shaderProgramHost{nullptr};

    /// Shared remapping table
    EventDataStack::RemappingTable eventRemappingTable;

    /// Pre-populated proxies
    ID3D12GraphicsCommandListFeatureProxies commandListProxies;

    /// Shared logging buffer
    LogBuffer logBuffer;

    /// Optional environment, ignored if creation parameters supply a registry
    Backend::Environment environment;

    /// All features
    Vector<ComRef<IFeature>> features;
};
