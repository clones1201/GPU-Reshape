#pragma once

// System
#include <d3d12.h>

// AMD
#define AGS_GCC 1
#include <AMDAGS/amd_ags.h>

// Std
#include <optional>

// Cleanup
#undef OPAQUE
#undef min
#undef max

// Linkage
#ifdef DX12_PRIVATE
#   define DX12_LINKAGE __declspec(dllexport)
#   define DX12_C_LINKAGE extern "C" __declspec(dllexport)
#else // DX12_PRIVATE
#   define DX12_LINKAGE __declspec(dllimport)
#   define DX12_C_LINKAGE extern "C" __declspec(dllimport)
#endif // DX12_PRIVATE

// Forward declarations
class Registry;

/// Function pointer types
using PFN_CREATE_DXGI_FACTORY = HRESULT(WINAPI*)(REFIID riid, _COM_Outptr_ void **ppFactory);
using PFN_CREATE_DXGI_FACTORY1 = HRESULT(WINAPI*)(REFIID riid, _COM_Outptr_ void **ppFactory);
using PFN_CREATE_DXGI_FACTORY2 = HRESULT(WINAPI*)(UINT flags, REFIID riid, _COM_Outptr_ void **ppFactory);
using PFN_D3D12_SET_DEVICE_GPUOPEN_GPU_VALIDATION_INFO = HRESULT (WINAPI *)(const struct D3D12_DEVICE_GPUOPEN_GPU_VALIDATION_INFO* info);
using PFN_D3D12_CREATE_DEVICE_GPUOPEN = HRESULT (WINAPI *)(IUnknown *pAdapter, D3D_FEATURE_LEVEL minimumFeatureLevel, REFIID riid, void **ppDevice, const struct D3D12_DEVICE_GPUOPEN_GPU_VALIDATION_INFO* info);
using PFN_D3D12_SET_FUNCTION_TABLE_GPUOPEN = HRESULT(WINAPI *)(const struct D3D12GPUOpenFunctionTable* table);

/// Extension pointer types
using PFN_AMD_AGS_CREATE_DEVICE = AGSReturnCode(__stdcall *)(AGSContext* context, const AGSDX12DeviceCreationParams* creationParams, const AGSDX12ExtensionParams* extensionParams, AGSDX12ReturnedParams* returnedParams);
using PFN_AMD_AGS_DESTRIY_DEVICE = AGSReturnCode(__stdcall *)(AGSContext* context, ID3D12Device* device, unsigned int* deviceReferences);
using PFN_AMD_AGS_PUSH_MARKER = AGSReturnCode(__stdcall *)(AGSContext* context, ID3D12GraphicsCommandList* commandList, const char* data);
using PFN_AMD_AGS_POP_MARKER = AGSReturnCode(__stdcall *)(AGSContext* context, ID3D12GraphicsCommandList* commandList);
using PFN_AMD_AGS_SET_MARKER = AGSReturnCode(__stdcall *)(AGSContext* context, ID3D12GraphicsCommandList* commandList, const char* data);

/// Optional gpu validation information
struct D3D12_DEVICE_GPUOPEN_GPU_VALIDATION_INFO {
    /// Shared registry
    Registry* registry;
};

/// Optional function table
struct D3D12GPUOpenFunctionTable {
    PFN_D3D12_CREATE_DEVICE  next_D3D12CreateDeviceOriginal{nullptr};
    PFN_CREATE_DXGI_FACTORY  next_CreateDXGIFactoryOriginal{nullptr};
    PFN_CREATE_DXGI_FACTORY1 next_CreateDXGIFactory1Original{nullptr};
    PFN_CREATE_DXGI_FACTORY2 next_CreateDXGIFactory2Original{nullptr};

    /// Extensions
    PFN_AMD_AGS_CREATE_DEVICE  next_AMDAGSCreateDevice{nullptr};
    PFN_AMD_AGS_DESTRIY_DEVICE next_AMDAGSDestroyDevice{nullptr};
    PFN_AMD_AGS_PUSH_MARKER    next_AMDAGSPushMarker{nullptr};
    PFN_AMD_AGS_POP_MARKER     next_AMDAGSPopMarker{nullptr};
    PFN_AMD_AGS_SET_MARKER     next_AMDAGSSetMarker{nullptr};
};

/// Shared validation info
extern std::optional<D3D12_DEVICE_GPUOPEN_GPU_VALIDATION_INFO> D3D12DeviceGPUOpenGPUValidationInfo;

/// Shared function table
extern D3D12GPUOpenFunctionTable D3D12GPUOpenFunctionTableNext;

/// Prototypes
/// Set the shared validation info
DX12_C_LINKAGE HRESULT WINAPI D3D12SetDeviceGPUOpenGPUValidationInfo(const D3D12_DEVICE_GPUOPEN_GPU_VALIDATION_INFO* info);

/// Extended device creation
///   ! While cleaner, this has the unintended problem that further hooking will not work, and causes *serious* havok
DX12_C_LINKAGE HRESULT WINAPI D3D12CreateDeviceGPUOpen(
    IUnknown *pAdapter,
    D3D_FEATURE_LEVEL minimumFeatureLevel,
    REFIID riid, void **ppDevice,
    const D3D12_DEVICE_GPUOPEN_GPU_VALIDATION_INFO* info
);

/// Set the internal function table
DX12_C_LINKAGE HRESULT WINAPI D3D12SetFunctionTableGPUOpen(const D3D12GPUOpenFunctionTable* table);
