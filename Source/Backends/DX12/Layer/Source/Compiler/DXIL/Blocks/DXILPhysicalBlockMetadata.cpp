#include <Backends/DX12/Compiler/DXIL/Blocks/DXILPhysicalBlockMetadata.h>
#include <Backends/DX12/Compiler/DXIL/DXILPhysicalBlockTable.h>
#include <Backends/DX12/Compiler/DXIL/DXILPhysicalBlockScan.h>

/*
 * LLVM DXIL Specification
 *   https://github.com/microsoft/DirectXShaderCompiler/blob/main/docs/DXIL.rst
 */

void DXILPhysicalBlockMetadata::CopyTo(DXILPhysicalBlockMetadata &out) {
    out.resources = resources;
    out.metadataBlocks = metadataBlocks;
    out.registerSpaces = registerSpaces;
}

void DXILPhysicalBlockMetadata::ParseMetadata(const struct LLVMBlock *block) {
    MetadataBlock& metadataBlock = metadataBlocks.emplace_back();
    metadataBlock.uid = block->uid;

    // Empty out previous block data
    metadataBlock.metadata.resize(block->records.Size());

    // Current name
    LLVMRecordStringView recordName;

    // Visit records
    for (size_t i = 0; i < block->records.Size(); i++) {
        const LLVMRecord &record = block->records[i];

        // Get metadata
        Metadata &md = metadataBlock.metadata[i];
        md.source = i;

        // Handle record
        switch (static_cast<LLVMMetadataRecord>(record.id)) {
            default: {
                // Ignored
                break;
            }

            case LLVMMetadataRecord::Name: {
                // Set name
                recordName = LLVMRecordStringView(record, 0);

                // Validate next
                ASSERT(i + 1 != block->records.Size(), "Expected succeeding metadata record");
                ASSERT(block->records[i + 1].Is(LLVMMetadataRecord::NamedNode), "Succeeding record to Name must be NamedNode");
                break;
            }

                // String name?
            case LLVMMetadataRecord::StringOld: {
                md.name.resize(record.opCount);
                record.FillOperands(md.name.data());
                break;
            }

                // Named successor
            case LLVMMetadataRecord::NamedNode: {
                ParseNamedNode(metadataBlock, block, record, recordName, i);
                break;
            }

                // Constant value
            case LLVMMetadataRecord::Value: {
                if (table.idMap.GetType(record.Op(1)) == DXILIDType::Constant) {
                    auto mapped = table.idMap.GetMappedCheckType(record.Op(1), DXILIDType::Constant);
                    md.value.type = table.type.typeMap.GetType(record.Op(0));
                    md.value.constant = program.GetConstants().GetConstant(mapped);
                    ASSERT(md.value.type, "Expected type");
                    ASSERT(md.value.constant, "Expected constant");
                }
                break;
            }
        }
    }
}

void DXILPhysicalBlockMetadata::ParseNamedNode(MetadataBlock& metadataBlock, const struct LLVMBlock *block, const LLVMRecord &record, const LLVMRecordStringView &name, uint32_t index) {
    switch (name.GetHash()) {
        // Resource declaration record
        case CRC64("dx.resources"): {
            if (name != "dx.resources") {
                return;
            }

            // Get list
            ASSERT(record.opCount == 1, "Expected a single value for dx.resources");
            const LLVMRecord &list = block->records[record.Op(0)];

            // Set ids
            resources.uid = block->uid;
            resources.source = index;

            // Check SRVs
            if (resources.srvs = list.Op(static_cast<uint32_t>(DXILShaderResourceClass::SRVs)); resources.srvs != 0) {
                ParseResourceList(metadataBlock, block, DXILShaderResourceClass::SRVs, resources.srvs);
            }

            // Check UAVs
            if (resources.uavs = list.Op(static_cast<uint32_t>(DXILShaderResourceClass::UAVs)); resources.uavs != 0) {
                ParseResourceList(metadataBlock, block, DXILShaderResourceClass::UAVs, resources.uavs);
            }

            // Check CBVs
            if (resources.cbvs = list.Op(static_cast<uint32_t>(DXILShaderResourceClass::CBVs)); resources.cbvs != 0) {
                ParseResourceList(metadataBlock, block, DXILShaderResourceClass::CBVs, resources.cbvs);
            }

            // Check samplers
            if (resources.samplers = list.Op(static_cast<uint32_t>(DXILShaderResourceClass::Samplers)); resources.samplers != 0) {
                ParseResourceList(metadataBlock, block, DXILShaderResourceClass::Samplers, resources.samplers);
            }

            break;
        }
    }
}

void DXILPhysicalBlockMetadata::ParseResourceList(struct MetadataBlock& metadataBlock, const struct LLVMBlock *block, DXILShaderResourceClass type, uint32_t id) {
    const LLVMRecord &record = block->records[id - 1];

    RegisterSpace& space = FindOrAddRegisterSpace(type);

    // For each resource
    for (uint32_t i = 0; i < record.opCount; i++) {
        const LLVMRecord &resource = block->records[record.ops[i] - 1];

        // Prepare entry
        HandleEntry entry;
        entry.record = &resource;

        // Get handle id
        uint64_t resourceID = GetOperandU32Constant(metadataBlock, resource.Op(0));

        // Undef constant
        const IL::Constant *constantPointer = GetOperandConstant(metadataBlock, resource.Op(1));

        // TODO: How on earth are the names stored?
        // uint64_t name = GetOperandU32Constant(resource.Op(2))->value;

        // Resource binding
        uint64_t bindSpace = GetOperandU32Constant(metadataBlock, resource.Op(3));
        uint64_t rsBase = GetOperandU32Constant(metadataBlock, resource.Op(4));
        uint64_t rsRange = GetOperandU32Constant(metadataBlock, resource.Op(5));

        ASSERT(space.bindSpace == ~0u || space.bindSpace == bindSpace, "Invalid bind space");
        space.bindSpace = bindSpace;

        // Set the bound
        space.registerBound = std::max<uint32_t>(space.registerBound, rsBase + rsRange);

        // Handle based on type
        switch (type) {
            default:
            ASSERT(false, "Invalid resource type");
                break;
            case DXILShaderResourceClass::SRVs: {
                // Unique ops
                auto shape = resource.OpAs<DXILShaderResourceShape>(6);
                uint64_t sampleCount = GetOperandU32Constant(metadataBlock, resource.Op(7));

                // Get extended metadata
                Metadata &extendedMetadata = metadataBlock.metadata[resource.Op(8) - 1];

                // Get extended record
                const LLVMRecord& extendedRecord = block->records[extendedMetadata.source];
                ASSERT(extendedRecord.opCount == 2, "Expected 2 operands for extended metadata");

                // Buffer shape?
                if (IsBuffer(shape)) {
                    Backend::IL::BufferType buffer{};
                    buffer.samplerMode = Backend::IL::ResourceSamplerMode::RuntimeOnly;
                    entry.type = program.GetTypeMap().FindTypeOrAdd(buffer);
                } else {
                    Backend::IL::TextureType texture{};
                    texture.samplerMode = Backend::IL::ResourceSamplerMode::RuntimeOnly;
                    entry.type = program.GetTypeMap().FindTypeOrAdd(texture);
                }
                break;
            }
            case DXILShaderResourceClass::UAVs: {
                // Unique ops
                auto shape = static_cast<DXILShaderResourceShape>(GetOperandU32Constant(metadataBlock, resource.Op(6)));
                uint64_t globallyCoherent = GetOperandBoolConstant(metadataBlock, resource.Op(7));
                uint64_t counter = GetOperandBoolConstant(metadataBlock, resource.Op(8));
                uint64_t rasterizerOrderedView = GetOperandBoolConstant(metadataBlock, resource.Op(9));

                // Get extended metadata
                Metadata &extendedMetadata = metadataBlock.metadata[resource.Op(10) - 1];

                // Get extended record
                const LLVMRecord& extendedRecord = block->records[extendedMetadata.source];
                ASSERT(extendedRecord.opCount == 2, "Expected 2 operands for extended metadata");

                // Optional element type
                const Backend::IL::Type* elementType{nullptr};

                // Optional texel format
                Backend::IL::Format format{Backend::IL::Format::None};

                // Parse tags
                for (uint32_t kv = 0; kv < extendedRecord.opCount; kv += 2) {
                    switch (static_cast<DXILUAVTag>(GetOperandU32Constant(metadataBlock, resource.Op(kv + 0)))) {
                        case DXILUAVTag::ElementType: {
                            // Get type
                            auto componentType = GetOperandU32Constant<ComponentType>(metadataBlock, extendedRecord.Op(kv + 1));

                            // Get type and format
                            elementType = GetComponentType(componentType);
                            format = GetComponentFormat(componentType);
                            break;
                        }
                        case DXILUAVTag::ByteStride: {
                            break;
                        }
                    }
                }

                // Buffer shape?
                if (IsBuffer(shape)) {
                    Backend::IL::BufferType buffer{};
                    buffer.samplerMode = Backend::IL::ResourceSamplerMode::Writable;
                    buffer.elementType = elementType;
                    buffer.texelType = format;
                    entry.type = program.GetTypeMap().FindTypeOrAdd(buffer);
                } else {
                    Backend::IL::TextureType texture{};
                    texture.samplerMode = Backend::IL::ResourceSamplerMode::Writable;
                    texture.sampledType = elementType;
                    texture.format = format;

                    // Translate shape
                    switch (shape) {
                        default:
                            texture.dimension = Backend::IL::TextureDimension::Unexposed;
                            break;
                        case DXILShaderResourceShape::Invalid:
                            break;
                        case DXILShaderResourceShape::Texture1D:
                            texture.dimension = Backend::IL::TextureDimension::Texture1D;
                            break;
                        case DXILShaderResourceShape::Texture2D:
                            texture.dimension = Backend::IL::TextureDimension::Texture2D;
                            break;
                        case DXILShaderResourceShape::Texture2DMS:
                            texture.dimension = Backend::IL::TextureDimension::Texture2D;
                            texture.multisampled = true;
                            break;
                        case DXILShaderResourceShape::Texture3D:
                            texture.dimension = Backend::IL::TextureDimension::Texture3D;
                            break;
                        case DXILShaderResourceShape::TextureCube:
                            texture.dimension = Backend::IL::TextureDimension::Texture2DCube;
                            break;
                        case DXILShaderResourceShape::Texture1DArray:
                            texture.dimension = Backend::IL::TextureDimension::Texture1DArray;
                            break;
                        case DXILShaderResourceShape::Texture2DArray:
                            texture.dimension = Backend::IL::TextureDimension::Texture2DArray;
                            break;
                        case DXILShaderResourceShape::Texture2DMSArray:
                            texture.dimension = Backend::IL::TextureDimension::Texture2DArray;
                            texture.multisampled = true;
                            break;
                        case DXILShaderResourceShape::TextureCubeArray:
                            texture.dimension = Backend::IL::TextureDimension::Texture2DCubeArray;
                            break;
                    }

                    entry.type = program.GetTypeMap().FindTypeOrAdd(texture);
                }
                break;
            }
            case DXILShaderResourceClass::CBVs: {
                // Unique ops
                uint64_t byteSize = GetOperandU32Constant(metadataBlock, resource.Op(6));
                uint64_t extendedMetadata = GetOperandU32Constant(metadataBlock, resource.Op(7));
                break;
            }
            case DXILShaderResourceClass::Samplers: {
                // Unique ops
                uint64_t samplerType = GetOperandU32Constant(metadataBlock, resource.Op(6));
                uint64_t extendedMetadata = GetOperandU32Constant(metadataBlock, resource.Op(7));
                break;
            }
        }

        // Ensure space
        if (space.handles.size() <= resourceID) {
            space.handles.resize(resourceID + 1);
        }

        // Set entry at id
        space.handles[resourceID] = entry;
    }
}

const Backend::IL::Type *DXILPhysicalBlockMetadata::GetHandleType(DXILShaderResourceClass _class, uint32_t handleID) {
    RegisterSpace& space = FindOrAddRegisterSpace(_class);

    if (space.handles.size() <= handleID || !space.handles[handleID].record) {
        return nullptr;
    }

    // Get entry
    return space.handles[handleID].type;
}

const Backend::IL::Type *DXILPhysicalBlockMetadata::GetComponentType(ComponentType type) {
    switch (type) {
        default:
            return program.GetTypeMap().FindTypeOrAdd(Backend::IL::UnexposedType{});
        case ComponentType::Int16:
            return program.GetTypeMap().FindTypeOrAdd(Backend::IL::IntType{
                .bitWidth = 16,
                .signedness = true
            });
        case ComponentType::UInt16:
            return program.GetTypeMap().FindTypeOrAdd(Backend::IL::IntType{
                .bitWidth = 16,
                .signedness = false
            });
        case ComponentType::Int32:
            return program.GetTypeMap().FindTypeOrAdd(Backend::IL::IntType{
                .bitWidth = 32,
                .signedness = true
            });
        case ComponentType::UInt32:
            return program.GetTypeMap().FindTypeOrAdd(Backend::IL::IntType{
                .bitWidth = 32,
                .signedness = false
            });
        case ComponentType::Int64:
            return program.GetTypeMap().FindTypeOrAdd(Backend::IL::IntType{
                .bitWidth = 64,
                .signedness = true
            });
        case ComponentType::UInt64:
            return program.GetTypeMap().FindTypeOrAdd(Backend::IL::IntType{
                .bitWidth = 64,
                .signedness = false
            });
        case ComponentType::FP16:
            return program.GetTypeMap().FindTypeOrAdd(Backend::IL::FPType{
                .bitWidth = 16,
            });
        case ComponentType::FP32:
            return program.GetTypeMap().FindTypeOrAdd(Backend::IL::FPType{
                .bitWidth = 32,
            });
        case ComponentType::FP64:
            return program.GetTypeMap().FindTypeOrAdd(Backend::IL::FPType{
                .bitWidth = 64,
            });
        case ComponentType::UNormFP16:
        case ComponentType::SNormFP32:
        case ComponentType::UNormFP32:
        case ComponentType::SNormFP64:
        case ComponentType::UNormFP64:
        case ComponentType::SNormFP16:
            return program.GetTypeMap().FindTypeOrAdd(Backend::IL::FPType{
                .bitWidth = 16,
            });
            break;
    }
}

Backend::IL::Format DXILPhysicalBlockMetadata::GetComponentFormat(ComponentType type) {
    switch (type) {
        default:
            return Backend::IL::Format::Unexposed;
        case ComponentType::None:
            return Backend::IL::Format::None;
        case ComponentType::Int16:
            return Backend::IL::Format::R16Int;
        case ComponentType::UInt16:
            return Backend::IL::Format::R16UInt;
        case ComponentType::Int32:
            return Backend::IL::Format::R32Int;
        case ComponentType::UInt32:
            return Backend::IL::Format::R32UInt;
        case ComponentType::Int64:
            return Backend::IL::Format::Unexposed;
        case ComponentType::UInt64:
            return Backend::IL::Format::Unexposed;
        case ComponentType::FP16:
            return Backend::IL::Format::R16Float;
        case ComponentType::FP32:
            return Backend::IL::Format::R32Float;
        case ComponentType::FP64:
            return Backend::IL::Format::Unexposed;
        case ComponentType::SNormFP16:
            return Backend::IL::Format::R16Snorm;
        case ComponentType::UNormFP16:
            return Backend::IL::Format::R16Unorm;
        case ComponentType::SNormFP32:
            return Backend::IL::Format::R32Snorm;
        case ComponentType::UNormFP32:
            return Backend::IL::Format::R32Unorm;
        case ComponentType::SNormFP64:
            return Backend::IL::Format::Unexposed;
        case ComponentType::UNormFP64:
            return Backend::IL::Format::Unexposed;
        case ComponentType::PackedS8x32:
            return Backend::IL::Format::Unexposed;
        case ComponentType::PackedU8x32:
            return Backend::IL::Format::Unexposed;
    }
}

void DXILPhysicalBlockMetadata::CompileMetadata(struct LLVMBlock *block) {

}

void DXILPhysicalBlockMetadata::StitchMetadata(struct LLVMBlock *block) {
    // Set source result for stitching
    for (size_t i = 0; i < block->records.Size(); i++) {
        LLVMRecord &record = block->records[i];
        record.result = i;
    }

    // Source to stitched mappings
    std::vector<uint64_t> sourceMappings;
    sourceMappings.resize(block->records.Size(), ~0u);

    // Swap source data
    TrivialStackVector<LLVMRecord, 32> source;
    block->records.Swap(source);

    // Swap element data
    TrivialStackVector<LLVMBlockElement, 128> elements;
    block->elements.Swap(elements);

    // Reserve
    block->elements.Reserve(elements.Size());

    // Filter all records
    for (const LLVMBlockElement &element: elements) {
        if (!element.Is(LLVMBlockElementType::Record)) {
            block->elements.Add(element);
        }
    }

    // Resolver loop
    for (;;) {
        bool mutated = false;

        for (size_t i = 0; i < source.Size(); i++) {
            LLVMRecord &record = source[i];

            if (record.result == ~0u) {
                continue;
            }

            // Handle record
            switch (static_cast<LLVMMetadataRecord>(record.id)) {
                default: {
                    // Set new mapping
                    sourceMappings.at(record.result) = block->records.Size();

                    // Append
                    block->AddRecord(record);
                    record.result = ~0u;

                    // OK
                    mutated = true;
                    break;
                }

                case LLVMMetadataRecord::Name: {
                    // Ignore named blocks
                    i++;
                    break;
                }

                case LLVMMetadataRecord::Node: {
                    bool resolved = true;

                    // Ensure all operands are resolved
                    for (uint32_t opId = 0; opId < record.opCount; opId++) {
                        if (record.ops[opId] != 0) {
                            resolved &= sourceMappings.at(record.ops[opId] - 1) != ~0u;
                        }
                    }

                    // Ready?
                    if (resolved) {
                        // Set new mapping
                        sourceMappings.at(record.result) = block->records.Size();

                        // Stitch ops
                        for (uint32_t opId = 0; opId < record.opCount; opId++) {
                            if (record.ops[opId] != 0) {
                                record.ops[opId] = sourceMappings.at(record.ops[opId] - 1) + 1;
                            }
                        }

                        // Append
                        block->AddRecord(record);
                        record.result = ~0u;

                        // OK
                        mutated = true;
                    }
                    break;
                }

                    // Constant value
                case LLVMMetadataRecord::Value: {
                    // Stitch the value id
                    table.idRemapper.Remap(record.Op(1));

                    // Set new mapping
                    sourceMappings.at(record.result) = block->records.Size();

                    // Append
                    block->AddRecord(record);
                    record.result = ~0u;

                    // OK
                    mutated = true;
                    break;
                }
            }
        }

        // No changes since last iteration?
        if (!mutated) {
            break;
        }
    }

    // Append named blocks
    for (size_t i = 0; i < source.Size(); i++) {
        // Name?
        if (!source[i].Is(LLVMMetadataRecord::Name)) {
            continue;
        }

        LLVMRecord &name = source[i];
        LLVMRecord &node = source[i + 1];

        // Stitch ops
        for (uint32_t opId = 0; opId < node.opCount; opId++) {
            if (node.ops[opId] != 0) {
                node.ops[opId] = sourceMappings.at(node.ops[opId]);
            }
        }

        // Append
        block->AddRecord(name);
        block->AddRecord(node);
    }

    // Must have resolved all
    if (block->records.Size() != source.Size()) {
        ASSERT(false, "Failed to resolve metadata");
    }
}

void DXILPhysicalBlockMetadata::SetDeclarationBlock(struct LLVMBlock *block) {
    declarationBlock = block;
}

uint32_t DXILPhysicalBlockMetadata::FindOrAddString(DXILPhysicalBlockMetadata::MetadataBlock &metadata, LLVMBlock *block, const std::string_view& str) {
    // Check if exists
    for (uint32_t i = 0; i < metadata.metadata.size(); i++) {
        if (metadata.metadata[i].name == str) {
            return i + 1;
        }
    }

    // Add value md
    Metadata& md = metadata.metadata.emplace_back();
    md.source = block->records.Size();
    md.name = str;

    // Insert value record
    LLVMRecord record(LLVMMetadataRecord::StringOld);
    record.opCount = str.length();
    record.ops = table.recordAllocator.AllocateArray<uint64_t>(record.opCount);

    // Copy string
    for (size_t i = 0; i < str.length(); i++) {
        record.ops[i] = str[i];
    }

    block->AddRecord(record);

    // OK
    return metadata.metadata.size();
}

uint32_t DXILPhysicalBlockMetadata::FindOrAddOperandConstant(DXILPhysicalBlockMetadata::MetadataBlock &metadata, LLVMBlock *block, const Backend::IL::Constant *constant) {
    // Check if exists
    for (uint32_t i = 0; i < metadata.metadata.size(); i++) {
        if (metadata.metadata[i].value.constant == constant) {
            return i + 1;
        }
    }

    // Add value md
    Metadata& md = metadata.metadata.emplace_back();
    md.source = block->records.Size();
    md.value.type = constant->type;
    md.value.constant = constant;

    // Insert value record
    LLVMRecord record(LLVMMetadataRecord::Value);
    record.opCount = 2;
    record.ops = table.recordAllocator.AllocateArray<uint64_t>(2);
    record.ops[0] = table.type.typeMap.GetType(constant->type);
    record.ops[1] = DXILIDRemapper::EncodeUserOperand(constant->id);
    block->AddRecord(record);

    // OK
    return metadata.metadata.size();
}

void DXILPhysicalBlockMetadata::CompileShaderExportResources() {
    // Get the metadata
    MetadataBlock* metadataBlock = GetMetadataBlock(resources.uid);

    // Get the block
    LLVMBlock* block = declarationBlock->GetBlockWithUID(resources.uid);

    RegisterSpace& space = FindOrAddRegisterSpace(DXILShaderResourceClass::UAVs);

    // Handle id
    exportHandleId = space.handles.size();

    table.bindingInfo.handleId = exportHandleId;
    table.bindingInfo.space = space.bindSpace;
    table.bindingInfo._register = space.registerBound;
    table.bindingInfo.count = 2;

    // i32
    const Backend::IL::Type* i32 = program.GetTypeMap().FindTypeOrAdd(Backend::IL::IntType{.bitWidth=32,.signedness=true});

    // {i32}
    Backend::IL::StructType retTyDecl;
    retTyDecl.memberTypes.push_back(i32);
    const Backend::IL::Type* retTy = program.GetTypeMap().FindTypeOrAdd(retTyDecl);

    const Backend::IL::Type* retArrayTy = program.GetTypeMap().FindTypeOrAdd(Backend::IL::ArrayType{
        .elementType = retTy,
        .count = table.bindingInfo.count
    });

    // Compile as named
    table.type.typeMap.CompileNamedType(retTy, "class.RWBuffer<unsigned int>");

    // {i32}*
    const Backend::IL::Type* retTyPtr = program.GetTypeMap().FindTypeOrAdd(Backend::IL::PointerType{
        .pointee = retArrayTy,
        .addressSpace = Backend::IL::AddressSpace::Function
    });

    // Insert extended record node
    LLVMRecord extendedMetadata(LLVMMetadataRecord::Node);
    extendedMetadata.SetUser(false, ~0u, ~0u);
    extendedMetadata.opCount = 2;
    extendedMetadata.ops = table.recordAllocator.AllocateArray<uint64_t>(extendedMetadata.opCount);
    extendedMetadata.ops[0] = FindOrAddOperandU32Constant(*metadataBlock, block, static_cast<uint32_t>(DXILUAVTag::ElementType));
    extendedMetadata.ops[1] = FindOrAddOperandU32Constant(*metadataBlock, block, static_cast<uint32_t>(ComponentType::UInt32));
    block->AddRecord(extendedMetadata);

    // Create extended metadata row
    Metadata extendedMd = metadataBlock->metadata.emplace_back();
    extendedMd.source = block->records.Size();

    // Index of extended node
    uint32_t extendedMdIndex = metadataBlock->metadata.size();

    // Insert resource record node
    LLVMRecord resource(LLVMMetadataRecord::Node);
    resource.SetUser(false, ~0u, ~0u);
    resource.opCount = 11;
    resource.ops = table.recordAllocator.AllocateArray<uint64_t>(resource.opCount);
    resource.ops[0] = FindOrAddOperandU32Constant(*metadataBlock, block, exportHandleId);
    resource.ops[1] = FindOrAddOperandConstant(*metadataBlock, block, program.GetConstants().FindConstantOrAdd(retTyPtr, Backend::IL::UndefConstant{}));
    resource.ops[2] = FindOrAddString(*metadataBlock, block, "ShaderExportBuffers");

    resource.ops[3] = FindOrAddOperandU32Constant(*metadataBlock, block, table.bindingInfo.space);
    resource.ops[4] = FindOrAddOperandU32Constant(*metadataBlock, block, table.bindingInfo._register);
    resource.ops[5] = FindOrAddOperandU32Constant(*metadataBlock, block, table.bindingInfo.count);

    resource.ops[6] = FindOrAddOperandU32Constant(*metadataBlock, block, static_cast<uint32_t>(DXILShaderResourceShape::TypedBuffer));
    resource.ops[7] = FindOrAddOperandBoolConstant(*metadataBlock, block, false);
    resource.ops[8] = FindOrAddOperandBoolConstant(*metadataBlock, block, false);
    resource.ops[9] = FindOrAddOperandBoolConstant(*metadataBlock, block, false);
    resource.ops[10] = extendedMdIndex;
    block->AddRecord(resource);

    // Create resource metadata row
    Metadata resourceMd = metadataBlock->metadata.emplace_back();
    extendedMd.source = block->records.Size();

    // Index of resource node
    uint32_t resourceIndex = metadataBlock->metadata.size();

    // List record
    LLVMRecord* uavNode{nullptr};

    // Existing uav list?
    if (resources.uavs != ~0u) {
        uavNode = &block->records[resources.uavs - 1];

        // Extend operands
        auto* ops = table.recordAllocator.AllocateArray<uint64_t>(++uavNode->opCount);
        std::memcpy(ops, uavNode->ops, sizeof(uint64_t) * (uavNode->opCount - 1));
        uavNode->ops = ops;
    } else {
        // Allocate new list
        uavNode = &block->records.Add();
        uavNode->id = static_cast<uint32_t>(LLVMMetadataRecord::Node);
        uavNode->opCount = 1;
        uavNode->ops = table.recordAllocator.AllocateArray<uint64_t>(1);
    }

    // Append resource
    uavNode->ops[uavNode->opCount - 1] = resourceIndex;
}

DXILPhysicalBlockMetadata::RegisterSpace &DXILPhysicalBlockMetadata::FindOrAddRegisterSpace(DXILShaderResourceClass _class) {
    for (RegisterSpace& space : registerSpaces) {
        if (space._class == _class) {
            return space;
        }
    }

    RegisterSpace& space = registerSpaces.emplace_back();
    space._class = _class;
    space.bindSpace = registerSpaces.size() - 1;
    return space;
}
