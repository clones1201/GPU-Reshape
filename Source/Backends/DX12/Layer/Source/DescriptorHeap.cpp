#include <Backends/DX12/DescriptorHeap.h>
#include <Backends/DX12/Table.Gen.h>
#include <Backends/DX12/States/DescriptorHeapState.h>
#include <Backends/DX12/States/DeviceState.h>
#include <Backends/DX12/Export/ShaderExportDescriptorAllocator.h>
#include <Backends/DX12/Resource/PhysicalResourceMappingTable.h>

HRESULT WINAPI HookID3D12DeviceCreateDescriptorHeap(ID3D12Device *device, const D3D12_DESCRIPTOR_HEAP_DESC *desc, REFIID riid, void **pHeap) {
    auto table = GetTable(device);

    // Create state
    auto* state = new (table.state->allocators, kAllocStateDescriptorHeap) DescriptorHeapState();
    state->allocators = table.state->allocators;
    state->parent = device;
    state->type = desc->Type;
    state->flags = desc->Flags;
    state->physicalDescriptorCount = desc->NumDescriptors;
    state->exhausted = false;

    // Object
    ID3D12DescriptorHeap* heap{nullptr};

    // Heap of interest?
    if (desc->Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) {
        // Get desired bound
        uint32_t bound = ShaderExportDescriptorAllocator::GetDescriptorBound(table.state->exportHost.GetUnsafe());

        // There is little to no insight for the internal driver limits, so, attempt various sizes
        for (uint32_t divisor = 0; divisor < 4; divisor++) {
            // Copy description
            D3D12_DESCRIPTOR_HEAP_DESC expandedHeap = *desc;
            expandedHeap.NumDescriptors += bound;

            // Pass down callchain
            HRESULT hr = table.next->CreateDescriptorHeap(&expandedHeap, __uuidof(ID3D12DescriptorHeap), reinterpret_cast<void**>(&heap));
            if (SUCCEEDED(hr)) {
                break;
            }

            // Attempt next count
            bound /= 2;

            // Invalidate
            heap = nullptr;
        }

        // Succeeded?
        if (heap) {
            // Set count
            state->physicalDescriptorCount = desc->NumDescriptors + bound;
            
            // GPU base if heap supports
            if (desc->Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) {
                state->gpuDescriptorBase = heap->GetGPUDescriptorHandleForHeapStart();
            }

            // Create unique allocator
            state->allocator = new (table.state->allocators, kAllocShaderExport) ShaderExportDescriptorAllocator(table.state->allocators.Tag(kAllocShaderExport), table.next, heap, bound);
        }
    }

    // If exhausted, fall back to user requirements
    if (!heap) {
        state->exhausted = true;

        // Pass down callchain
        HRESULT hr = table.bottom->next_CreateDescriptorHeap(table.next, desc, __uuidof(ID3D12DescriptorHeap), reinterpret_cast<void**>(&heap));
        if (FAILED(hr)) {
            return hr;
        }
    }

    // Set base
    state->cpuDescriptorBase = heap->GetCPUDescriptorHandleForHeapStart();
    state->stride = device->GetDescriptorHandleIncrementSize(desc->Type);
    
    // Create PRMT
    state->prmTable = new (table.state->allocators, kAllocPRMT) PhysicalResourceMappingTable(table.state->allocators.Tag(kAllocPRMT), table.state->deviceAllocator);

    // Initialize table with count
    state->prmTable->Install(state->physicalDescriptorCount);

    // Register heap in shared table
    table.state->cpuHeapTable.Add(state->type, state, state->cpuDescriptorBase.ptr, state->physicalDescriptorCount, state->stride);

    // GPU visibility optional
    if (desc->Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) {
        table.state->gpuHeapTable.Add(state->type, state, state->gpuDescriptorBase.ptr, state->physicalDescriptorCount, state->stride);
    }

    // Create detours
    heap = CreateDetour(state->allocators, heap, state);

    // Query to external object if requested
    if (pHeap) {
        HRESULT hr = heap->QueryInterface(riid, pHeap);
        if (FAILED(hr)) {
            return hr;
        }
    }

    // Cleanup
    heap->Release();

    // OK
    return S_OK;
}

void WINAPI HookID3D12DescriptorHeapGetCPUDescriptorHandleForHeapStart(ID3D12DescriptorHeap *heap, D3D12_CPU_DESCRIPTOR_HANDLE* out) {
    auto table = GetTable(heap);

    D3D12_CPU_DESCRIPTOR_HANDLE reg;
    table.bottom->next_GetCPUDescriptorHandleForHeapStart(table.next, &reg);

    // Advance base by heap prefix
    if (table.state->allocator) {
        reg.ptr += table.state->allocator->GetPrefixOffset();
    }

    *out = reg;
}

void WINAPI HookID3D12DescriptorHeapGetGPUDescriptorHandleForHeapStart(ID3D12DescriptorHeap *heap, D3D12_GPU_DESCRIPTOR_HANDLE* out) {
    auto table = GetTable(heap);

    D3D12_GPU_DESCRIPTOR_HANDLE reg;
    table.bottom->next_GetGPUDescriptorHandleForHeapStart(table.next, &reg);

    // Advance base by heap prefix
    if (table.state->allocator && table.state->flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) {
        reg.ptr += table.state->allocator->GetPrefixOffset();
    }

    *out = reg;
}

void WINAPI HookID3D12DeviceCreateShaderResourceView(ID3D12Device* _this, ID3D12Resource* pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC* pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) {
    auto table = GetTable(_this);
    auto resource = GetTable(pResource);

    // TODO: Null descriptors!
    if (pResource) {
        // Associated heap?
        if (DescriptorHeapState* heap = table.state->cpuHeapTable.Find(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, DestDescriptor.ptr)) {
            uint64_t offset = DestDescriptor.ptr - heap->cpuDescriptorBase.ptr;
            ASSERT(offset % heap->stride == 0, "Invalid heap offset");

            // TODO: SRB masking
            heap->prmTable->WriteMapping(static_cast<uint32_t>(offset / heap->stride), resource.state, resource.state->virtualMapping);
        }
    }

    // Pass down callchain
    table.next->CreateShaderResourceView(resource.next, pDesc, DestDescriptor);
}

void WINAPI HookID3D12DeviceCreateUnorderedAccessView(ID3D12Device* _this, ID3D12Resource* pResource, ID3D12Resource* pCounterResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) {
    auto table = GetTable(_this);
    auto resource = GetTable(pResource);

    // TODO: Null descriptors!
    if (pResource) {
        // Associated heap?
        if (DescriptorHeapState* heap = table.state->cpuHeapTable.Find(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, DestDescriptor.ptr)) {
            uint64_t offset = DestDescriptor.ptr - heap->cpuDescriptorBase.ptr;
            ASSERT(offset % heap->stride == 0, "Invalid heap offset");

            // TODO: SRB masking
            heap->prmTable->WriteMapping(static_cast<uint32_t>(offset / heap->stride), resource.state, resource.state->virtualMapping);
        }
    }

    // Pass down callchain
    table.next->CreateUnorderedAccessView(resource.next, Next(pCounterResource), pDesc, DestDescriptor);
}

void WINAPI HookID3D12DeviceCreateRenderTargetView(ID3D12Device* _this, ID3D12Resource* pResource, const D3D12_RENDER_TARGET_VIEW_DESC* pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) {
    auto table = GetTable(_this);
    auto resource = GetTable(pResource);

    // TODO: Null descriptors!
    if (pResource) {
        // Associated heap?
        if (DescriptorHeapState* heap = table.state->cpuHeapTable.Find(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, DestDescriptor.ptr)) {
            uint64_t offset = DestDescriptor.ptr - heap->cpuDescriptorBase.ptr;
            ASSERT(offset % heap->stride == 0, "Invalid heap offset");

            // TODO: SRB masking
            heap->prmTable->WriteMapping(static_cast<uint32_t>(offset / heap->stride), resource.state, resource.state->virtualMapping);
        }
    }

    // Pass down callchain
    table.next->CreateRenderTargetView(resource.next, pDesc, DestDescriptor);
}

void WINAPI HookID3D12DeviceCreateDepthStencilView(ID3D12Device* _this, ID3D12Resource* pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC* pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) {
    auto table = GetTable(_this);
    auto resource = GetTable(pResource);

    // TODO: Null descriptors!
    if (pResource) {
        // Associated heap?
        if (DescriptorHeapState* heap = table.state->cpuHeapTable.Find(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, DestDescriptor.ptr)) {
            uint64_t offset = DestDescriptor.ptr - heap->cpuDescriptorBase.ptr;
            ASSERT(offset % heap->stride == 0, "Invalid heap offset");

            // TODO: SRB masking
            heap->prmTable->WriteMapping(static_cast<uint32_t>(offset / heap->stride), resource.state, resource.state->virtualMapping);
        }
    }

    // Pass down callchain
    table.next->CreateDepthStencilView(resource.next, pDesc, DestDescriptor);
}

HRESULT HookID3D12DescriptorHeapGetDevice(ID3D12DescriptorHeap*_this, const IID &riid, void **ppDevice) {
    auto table = GetTable(_this);

    // Pass to device query
    return table.state->parent->QueryInterface(riid, ppDevice);
}

DescriptorHeapState::~DescriptorHeapState() {
    auto table = GetTable(parent);

    // Remove from table
    table.state->cpuHeapTable.Remove(type, cpuDescriptorBase.ptr);

    // GPU visibility optional
    if (flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) {
        table.state->cpuHeapTable.Remove(type, gpuDescriptorBase.ptr);
    }
}

ResourceState * DescriptorHeapState::GetStateFromHeapHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle) const {
    // Get base offset from heap
    uint64_t offset = handle.ptr - cpuDescriptorBase.ptr;
    ASSERT(offset % stride == 0, "Invalid heap offset");

    // Get from PRMT
    return prmTable->GetMappingState(static_cast<uint32_t>(offset / stride));
}

ResourceState * DescriptorHeapState::GetStateFromHeapHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle) const {
    // Get base offset from heap
    uint64_t offset = handle.ptr - gpuDescriptorBase.ptr;
    ASSERT(offset % stride == 0, "Invalid heap offset");

    // Get from PRMT
    return prmTable->GetMappingState(static_cast<uint32_t>(offset / stride));
}
