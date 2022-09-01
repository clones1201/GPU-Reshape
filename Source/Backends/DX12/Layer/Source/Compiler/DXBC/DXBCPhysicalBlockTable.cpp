#include <Backends/DX12/Compiler/DXBC/DXBCPhysicalBlockTable.h>

// DXIL Extension
#include <Backends/DX12/Compiler/DXIL/DXILModule.h>
#include <Backends/DX12/Compiler/DXIL/DXILDebugModule.h>

// Common
#include <Common/Allocators.h>

DXBCPhysicalBlockTable::DXBCPhysicalBlockTable(const Allocators &allocators, IL::Program &program) :
    allocators(allocators),
    program(program),
    scan(allocators),
    shader(allocators, program, *this),
    pipelineStateValidation(allocators, program, *this) {
    /* */
}

bool DXBCPhysicalBlockTable::Parse(const void *byteCode, uint64_t byteLength) {
    if (!scan.Scan(byteCode, byteLength)) {
        return false;
    }

    // Parse blocks
    shader.Parse();
    pipelineStateValidation.Parse();

    // Parse canonical program
    if (DXBCPhysicalBlock *dxilBlock = scan.GetPhysicalBlock(DXBCPhysicalBlockType::DXIL)) {
        dxilModule = new(Allocators{}) DXILModule(allocators, &program);

        // Attempt to parse the module
        if (!dxilModule->Parse(dxilBlock->ptr, dxilBlock->length)) {
            return false;
        }
    }

    // The ILDB physical block contains the canonical program and debug information.
    // Unfortunately basing the main program off the ILDB is more trouble than it's worth,
    // as stripping the debug data after recompilation is quite troublesome.
    if (DXBCPhysicalBlock *ildbBlock = scan.GetPhysicalBlock(DXBCPhysicalBlockType::ILDB)) {
        auto* dxilDebugModule = new(Allocators{}) DXILDebugModule(allocators);

        // Attempt to parse the module
        if (!dxilDebugModule->Parse(ildbBlock->ptr, ildbBlock->length)) {
            return false;
        }

        // Set interface
        debugModule = dxilDebugModule;
    }

    // OK
    return true;
}

bool DXBCPhysicalBlockTable::Compile(const DXJob &job) {
    // DXIL?
    if (dxilModule) {
        DXBCPhysicalBlock *block = scan.GetPhysicalBlock(DXBCPhysicalBlockType::DXIL);

        // Attempt to compile contained module
        if (!dxilModule->Compile(job, block->stream)) {
            return false;
        }
    }

    // Compile validation
    pipelineStateValidation.Compile();

    // OK
    return true;
}

void DXBCPhysicalBlockTable::Stitch(const DXJob& job, DXStream &out) {
    scan.Stitch(job, out);
}

void DXBCPhysicalBlockTable::CopyTo(DXBCPhysicalBlockTable &out) {
    scan.CopyTo(out.scan);

    // Copy blocks
    pipelineStateValidation.CopyTo(out.pipelineStateValidation);

    // Keep the debug interface
    out.debugModule = debugModule;

    // Copy submodule if present
    if (dxilModule) {
        out.dxilModule = new(Allocators{}) DXILModule(allocators, &out.program);

        // Copy to new module
        dxilModule->CopyTo(out.dxilModule);
    }
}
