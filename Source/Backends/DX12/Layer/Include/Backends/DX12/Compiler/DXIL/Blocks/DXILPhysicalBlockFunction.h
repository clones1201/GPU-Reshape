#pragma once

// Layer
#include <Backends/DX12/Compiler/DXIL/DXILFunctionDeclaration.h>
#include <Backends/DX12/Compiler/DXIL/LLVM/LLVMRecordReader.h>
#include <Backends/DX12/Compiler/DXIL/LLVM/LLVMRecordStringView.h>
#include "DXILPhysicalBlockSection.h"

// Common
#include <Common/Containers/TrivialStackVector.h>

// Std
#include <string_view>

// Forward declarations
struct DXJob;
struct DXILValueReader;

/// Function block
struct DXILPhysicalBlockFunction : public DXILPhysicalBlockSection {
    using DXILPhysicalBlockSection::DXILPhysicalBlockSection;

    /// Copy this block
    /// \param out destination block
    void CopyTo(DXILPhysicalBlockFunction& out);

public:
    /// Parse a function
    /// \param block source block
    void ParseFunction(struct LLVMBlock *block);

    /// Migrate all constant blocks to global
    void MigrateConstantBlocks();

    /// Parse a module function
    /// \param record source record
    void ParseModuleFunction(struct LLVMRecord& record);

    /// Get the declaration associated with an id
    const DXILFunctionDeclaration* GetFunctionDeclaration(uint32_t id);

public:
    /// Compile a function
    /// \param block block
    void CompileFunction(const DXJob &job, struct LLVMBlock *block);

    /// Compile a module function
    /// \param record record
    void CompileModuleFunction(struct LLVMRecord& record);

    /// Compile a standard intrinsic call
    /// \param decl given declaration
    /// \param opCount argument op count
    /// \param ops operands
    /// \return call record
    LLVMRecord CompileIntrinsicCall(IL::ID result, const DXILFunctionDeclaration* decl, uint32_t opCount, const uint64_t* ops);

private:
    struct SVOXElement {
        /// Extracted type
        const Backend::IL::Type* type{nullptr};

        /// Extracted value
        IL::ID value{IL::InvalidID};
    };

    /// Get the number of SVOX values
    /// \param value SVOX value
    /// \return component count
    uint32_t GetSVOXCount(IL::ID value);

    /// Extract an SVOX element
    /// \param block block for record emitting
    /// \param value SVOX value to be extracted from
    /// \param index element index
    /// \return SVOX element info
    SVOXElement ExtractSVOXElement(LLVMBlock* block, IL::ID value, uint32_t index);

    /// Allocate a sequential SV
    /// \param x count number of components
    /// \param x first component, required
    /// \param y second component, optional
    /// \param z third component, optional
    /// \param w fourth component, optional
    /// \return
    IL::ID AllocateSVOSequential(uint32_t count, IL::ID x, IL::ID y = IL::InvalidID, IL::ID z = IL::InvalidID, IL::ID w = IL::InvalidID);

    /// Iterate a scalar / vector-of-x operation
    /// \param lhs lhs operand
    /// \param rhs rhs operand
    /// \param functor functor(const Backend::IL::Type* type, uint32_t value, uint32_t index, uint32_t max)
    template<typename F>
    void IterateSVOX(LLVMBlock* block, IL::ID value, F&& functor);

    /// Compile a unary scalar / vector-of-x operation
    /// \param lhs lhs operand
    /// \param rhs rhs operand
    /// \param functor functor(const Backend::IL::Type* type, uint32_t result, uint32_t lhs)
    template<typename F>
    void UnaryOpSVOX(LLVMBlock* block, IL::ID result, IL::ID value, F&& functor);

    /// Compile a binary scalar / vector-of-x operation
    /// \param lhs lhs operand
    /// \param rhs rhs operand
    /// \param functor functor(const Backend::IL::Type* type, uint32_t result, uint32_t lhs, uint32_t rhs)
    template<typename F>
    void BinaryOpSVOX(LLVMBlock* block, IL::ID result, IL::ID lhs, IL::ID rhs, F&& functor);

public:
    /// Compile a module function
    /// \param record record
    void StitchModuleFunction(struct LLVMRecord& record);

    /// Compile a function
    /// \param block block
    void StitchFunction(struct LLVMBlock *block);

    /// Remap a given record
    /// \param record
    void RemapRecord(struct LLVMRecord& record);

public:
    /// Find a function declaration
    /// \param view symbol name
    /// \return nullptr if not found
    const DXILFunctionDeclaration* FindDeclaration(const std::string_view& view);

    /// Add a new function declaration
    /// \param declaration declaration template
    /// \return declaration handle
    DXILFunctionDeclaration* AddDeclaration(const DXILFunctionDeclaration& declaration);

private:
    /// Shared counter handle
    uint32_t exportCounterHandle;
    uint32_t resourcePRMTHandle;
    uint32_t samplerPRMTHandle;
    uint32_t descriptorHandle;
    uint32_t eventHandle;

    /// All stream handles
    TrivialStackVector<uint32_t, 64> exportStreamHandles;

    /// Create an export handle
    /// \param block appended block
    void CreateHandles(const DXJob &job, struct LLVMBlock* block);

    /// Create an export handle
    /// \param block appended block
    void CreateExportHandle(const DXJob &job, struct LLVMBlock* block);

    /// Create the PRMT handle
    /// \param block appended block
    void CreatePRMTHandle(const DXJob &job, struct LLVMBlock* block);

    /// Create the shader data handles
    /// \param block appended block
    void CreateShaderDataHandle(const DXJob &job, struct LLVMBlock* block);

    /// Create the descriptor handle
    /// \param block appended block
    void CreateDescriptorHandle(const DXJob &job, struct LLVMBlock* block);

    /// Create the event handle
    /// \param block appended block
    void CreateEventHandle(const DXJob &job, struct LLVMBlock* block);

private:
    struct DynamicRootSignatureUserMapping {
        /// Source mapping
        const struct RootSignatureUserMapping* source{nullptr};

        /// Dynamic, sequential, offset due to dynamic indexing
        IL::ID dynamicOffset{IL::InvalidID};
    };

    /// Get the mapping of a resource
    /// \param job parent job
    /// \param source source records
    /// \param resource resource to be fetched
    /// \return empty if not found
    DynamicRootSignatureUserMapping GetResourceUserMapping(const DXJob& job, const Vector<LLVMRecord>& source, IL::ID resource);

private:
    /// Compile an export instruction
    /// \param block destination block
    /// \param _instr instruction to be compiled
    void CompileExportInstruction(LLVMBlock* block, const IL::ExportInstruction* _instr);

    /// Compile a resource token instruction
    /// \param job source job
    /// \param block destination block
    /// \param source all source instructions
    /// \param _instr instruction to be compiled
    void CompileResourceTokenInstruction(const DXJob& job, LLVMBlock* block, const Vector<LLVMRecord>& source , const IL::ResourceTokenInstruction* _instr);

private:
    /// Does the record have a result?
    bool HasResult(const struct LLVMRecord& record);

    /// Try to parse an intrinsic
    /// \param basicBlock output bb
    /// \param reader record reader
    /// \param anchor instruction anchor
    /// \param called called function index
    /// \param declaration pulled declaration
    /// \return true if recognized intrinsic
    bool TryParseIntrinsic(IL::BasicBlock *basicBlock, uint32_t recordIdx, DXILValueReader &reader, uint32_t anchor, uint32_t called, uint32_t result, const DXILFunctionDeclaration *declaration);

private:
    /// Returns true if the program requires value map segmentation, i.e. branching over value data
    bool RequiresValueMapSegmentation() const {
        return internalLinkedFunctions.Size() > 1u;
    }

private:
    struct FunctionBlock {
        /// UID of the originating block
        uint32_t uid;

        /// Relocation table for records
        TrivialStackVector<uint32_t, 512> recordRelocation;
    };

    /// Get the function block from a UID
    /// \param uid LLVM block uid
    /// \return nullptr if not found
    FunctionBlock* GetFunctionBlock(uint32_t uid) {
        for (FunctionBlock& block : functionBlocks) {
            if (block.uid == uid) {
                return &block;
            }
        }

        return nullptr;
    }

    /// All function blocks
    TrivialStackVector<FunctionBlock, 4> functionBlocks;

private:
    /// Function visitation counters
    uint32_t stitchFunctionIndex{0};

    /// All function declarations
    TrivialStackVector<DXILFunctionDeclaration*, 32> functions;

    /// All internally linked declaration indices
    TrivialStackVector<uint32_t, 32> internalLinkedFunctions;
};
