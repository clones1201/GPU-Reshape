#pragma once

// Layer
#include <Backends/DX12/InstrumentationInfo.h>
#include <Backends/DX12/States/ShaderStateKey.h>
#include <Backends/DX12/States/ShaderInstrumentationKey.h>
#include <Backends/DX12/Compiler/DXStream.h>

// Common
#include <Common/Containers/ReferenceObject.h>

// Std
#include <mutex>
#include <map>

// Forward declarations
struct DeviceState;
class DXModule;

struct ShaderState : public ReferenceObject {
    /// Reference counted destructor
    virtual ~ShaderState();

    /// Add an instrument to this shader
    /// \param featureBitSet the enabled feature set
    /// \param byteCode the byteCode in question
    void AddInstrument(const ShaderInstrumentationKey& instrumentationKey, const DXStream& instrument) {
        std::lock_guard lock(mutex);
        instrumentObjects[instrumentationKey] = instrument;
    }

    /// Get an instrument
    /// \param featureBitSet the enabled feature set
    /// \return nullptr if not found
    D3D12_SHADER_BYTECODE GetInstrument(const ShaderInstrumentationKey& instrumentationKey) {
        std::lock_guard lock(mutex);
        auto&& it = instrumentObjects.find(instrumentationKey);
        if (it == instrumentObjects.end()) {
            return {};
        }

        // To bytecode
        D3D12_SHADER_BYTECODE byteCode;
        byteCode.pShaderBytecode = it->second.GetData();
        byteCode.BytecodeLength = it->second.GetByteSize();
        return byteCode;
    }

    /// Check if instrument is present
    /// \param featureBitSet the enabled feature set
    /// \return false if not found
    bool HasInstrument(const ShaderInstrumentationKey& instrumentationKey) {
        std::lock_guard lock(mutex);
        return instrumentObjects.count(instrumentationKey) > 0;
    }

    bool Reserve(const ShaderInstrumentationKey& instrumentationKey) {
        std::lock_guard lock(mutex);
        auto&& it = instrumentObjects.find(instrumentationKey);
        if (it == instrumentObjects.end()) {
            instrumentObjects[instrumentationKey] = {};
            return true;
        }

        return false;
    }

    /// Originating key
    ///   ! Shader memory owned
    ShaderStateKey key;

    /// Backwards reference
    DeviceState* parent{nullptr};

    /// Instrumentation info
    InstrumentationInfo instrumentationInfo;

    /// Instrumented objects lookup
    /// TODO: How do we manage lifetimes here?
    std::map<ShaderInstrumentationKey, DXStream> instrumentObjects;

    /// Parsing module
    ///   ! May not be indexed yet, indexing occurs during instrumentation.
    ///     Avoided during regular use to not tamper with performance.
    DXModule* module{nullptr};

    /// Unique ID
    uint64_t uid{0};

    /// byteCode specific lock
    std::mutex mutex;
};
