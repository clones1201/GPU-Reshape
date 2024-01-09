// 
// The MIT License (MIT)
// 
// Copyright (c) 2024 Advanced Micro Devices, Inc.,
// Fatalist Development AB (Avalanche Studio Group),
// and Miguel Petersen.
// 
// All Rights Reserved.
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy 
// of this software and associated documentation files (the "Software"), to deal 
// in the Software without restriction, including without limitation the rights 
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
// of the Software, and to permit persons to whom the Software is furnished to do so, 
// subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all 
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
// PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE 
// FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, 
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 

// Common
#include <Common/Registry.h>
#include <Common/Plugin/Plugin.h>
#include <Common/Plugin/PluginInfo.h>

// Backend
#include <Bridge/IBridge.h>

// Schemas
#include <Schemas/Features/ResourceBounds.h>

// ResourceBounds
#include <Features/ResourceBounds/Listener.h>

static ComRef<ResourceBoundsListener> listener{nullptr};

DLL_EXPORT_C void PLUGIN_INFO(PluginInfo* info) {
    info->name = "ResourceBounds";
    info->description = "Presentation for resource bounds messages";
}

DLL_EXPORT_C bool PLUGIN_INSTALL(Registry* registry) {
    auto bridge = registry->Get<IBridge>();
    if (!bridge) {
        return false;
    }

    // Install the resource bounds listener
    listener = registry->New<ResourceBoundsListener>();
    if (!listener->Install()) {
        return false;
    }

    // Register with bridge
    bridge->Register(ResourceIndexOutOfBoundsMessage::kID, listener);

    // OK
    return true;
}

DLL_EXPORT_C void PLUGIN_UNINSTALL(Registry* registry) {
    auto bridge = registry->Get<IBridge>();
    if (!bridge) {
        return;
    }

    // Uninstall the listener
    bridge->Deregister(ResourceIndexOutOfBoundsMessage::kID, listener);
    listener.Release();
}
