#pragma once

#include "Program.h"
#include "Instruction.h"

namespace Backend::IL {
    inline const Type* GetComponentType(const Type* type) {
        switch (type->kind) {
            default:
                return type;
            case TypeKind::Vector:
                return type->As<VectorType>()->containedType;
            case TypeKind::Matrix:
                return type->As<MatrixType>()->containedType;
            case TypeKind::Array:
                return type->As<ArrayType>()->elementType;
            case TypeKind::Texture:
                return type->As<TextureType>()->sampledType;
            case TypeKind::Buffer:
                return type->As<BufferType>()->elementType;
        }
    }

    inline const Type* Splat(Program& program, const Type* scalarType, uint8_t count) {
        return program.GetTypeMap().FindTypeOrAdd(Backend::IL::VectorType {
            .containedType = scalarType,
            .dimension = count
        });
    }

    inline const Type* SplatToValue(Program& program, const Type* scalarType, IL::ID value) {
        const Type* valueType = program.GetTypeMap().GetType(value);
        if (!valueType) {
            ASSERT(false, "No type on splat value");
            return nullptr;
        }

        // Splat to target type
        switch (valueType->kind) {
            default:
                ASSERT(false, "Invalid splat target");
                return nullptr;
            case TypeKind::Bool:
            case TypeKind::Int:
            case TypeKind::FP:
                return scalarType;
            case TypeKind::Vector:
                return Splat(program, scalarType, valueType->As<VectorType>()->dimension);
        }
    }

    inline const bool IsComponentType(const Type* type, TypeKind kind) {
        switch (type->kind) {
            default:
                return type->kind == kind;
            case TypeKind::Vector:
                return type->As<VectorType>()->containedType->kind == kind;
            case TypeKind::Matrix:
                return type->As<MatrixType>()->containedType->kind == kind;
        }
    }

    template<typename T>
    inline const bool IsComponentType(const Type* type) {
        return IsComponentType(type, T::kKind);
    }
}
