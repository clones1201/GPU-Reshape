#pragma once

// Backend
#include <Backends/DX12/DX12.h>
#include <Backends/DX12/Layer.h>

// Forward declarations
struct DeviceState;

struct GlobalDeviceDetour {
public:
    bool Install();
    void Uninstall();
};

/// Hooks
DX12_C_LINKAGE HRESULT WINAPI HookID3D12CreateDevice(_In_opt_ IUnknown *pAdapter, D3D_FEATURE_LEVEL minimumFeatureLevel, _In_ REFIID riid, _COM_Outptr_opt_ void **ppDevice);
DX12_C_LINKAGE HRESULT WINAPI HookD3D12EnableExperimentalFeatures(UINT NumFeatures, const IID *riid, void *pConfigurationStructs, UINT *pConfigurationStructSizes);

/// Extension hooks
DX12_C_LINKAGE AGSReturnCode HookAMDAGSCreateDevice(AGSContext* context, const AGSDX12DeviceCreationParams* creationParams, const AGSDX12ExtensionParams* extensionParams, AGSDX12ReturnedParams* returnedParams);

/// Commit all bridge activity on a device
/// \param device device to be committed
void BridgeDeviceSyncPoint(DeviceState *device);
