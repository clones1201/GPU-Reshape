// 
// The MIT License (MIT)
// 
// Copyright (c) 2023 Miguel Petersen
// Copyright (c) 2023 Advanced Micro Devices, Inc
// Copyright (c) 2023 Avalanche Studios Group
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

#include <Backends/DX12/Compiler/DXBC/DXBCModule.h>
#include <Backends/DX12/Compiler/Tags.h>
#include <Backends/DX12/Compiler/DXParseJob.h>
#include <Backends/DX12/Config.h>

// Common
#include <Common/FileSystem.h>

// Std
#include <fstream>

DXBCModule::DXBCModule(const Allocators &allocators, uint64_t shaderGUID, const GlobalUID &instrumentationGUID) :
DXBCModule(allocators, new(allocators, kAllocModuleILProgram) IL::Program(allocators, shaderGUID), instrumentationGUID) {
    nested = false;

#if SHADER_COMPILER_DEBUG
    instrumentationGUIDName = instrumentationGUID.ToString();
#endif // SHADER_COMPILER_DEBUG
}

DXBCModule::DXBCModule(const Allocators &allocators, IL::Program *program, const GlobalUID &instrumentationGUID)
    : allocators(allocators),
      table(allocators, *program),
      program(program),
      instrumentationGUID(instrumentationGUID),
      dxStream(allocators) {
    /* */
}

DXBCModule::~DXBCModule() {
    if (!nested) {
        destroy(program, allocators);
    }
}

DXModule *DXBCModule::Copy() {
    // Copy program
    IL::Program *programCopy = program->Copy();

    // Create module copy
    auto module = new(allocators, kAllocModuleDXIL) DXBCModule(allocators, programCopy, instrumentationGUID);
    module->nested = nested;

    // Copy table
    table.CopyTo(module->table);

    // OK
    return module;
}

bool DXBCModule::Parse(const DXParseJob& job) {
#if SHADER_COMPILER_DEBUG_FILE
    std::ifstream in(GetIntermediateDebugPath() / "scan.dxbc", std::ios_base::binary);

    // Get file size
    in.seekg(0, std::ios_base::end);
    uint64_t size = in.tellg();
    in.seekg(0);

    // Stream data in
    std::vector<char> data(size);
    in.read(data.data(), size);

    // Set new data
    DXParseJob jobProxy = job;
    jobProxy.byteCode = data.data();
    jobProxy.byteLength = data.size();

    // Try to parse
    if (!table.Parse(jobProxy)) {
        return false;
    }
#else // SHADER_COMPILER_DEBUG_FILE
    // Try to parse
    if (!table.Parse(job)) {
        return false;
    }
#endif // SHADER_COMPILER_DEBUG_FILE

    // OK
    return true;
}

IL::Program *DXBCModule::GetProgram() {
    return program;
}

GlobalUID DXBCModule::GetInstrumentationGUID() {
    return instrumentationGUID;
}

bool DXBCModule::Compile(const DXCompileJob& job, DXStream& out) {
    // Try to recompile for the given job
    if (!table.Compile(job)) {
        return false;
    }

    // Stitch to the program
    table.Stitch(job, out);

    // OK!
    return true;
}

IDXDebugModule *DXBCModule::GetDebug() {
    return table.debugModule;
}
