#include "GenTypes.h"
#include "Types.h"
#include "Name.h"

// Common
#include <Common/String.h>

// Std
#include <sstream>
#include <regex>
#include <string_view>

namespace {
    struct ParameterInfo {
        std::string name;
        std::string info;
    };
}

/// Translate a given RST type into spec type
/// \param type RST type
/// \return empty if failed
static std::string TranslateType(const std::string_view& type) {
    if (type == "void") {
        return "DXILIntrinsicTypeSpec::Void";
    } else if (type == "i64") {
        return "DXILIntrinsicTypeSpec::I64";
    } else if (type == "i32") {
        return "DXILIntrinsicTypeSpec::I32";
    } else if (type == "f64") {
        return "DXILIntrinsicTypeSpec::F64";
    } else if (type == "float" || type == "f32") {
        return "DXILIntrinsicTypeSpec::F32";
    } else if (type == "f16") {
        return "DXILIntrinsicTypeSpec::F16";
    } else if (type == "i8") {
        return "DXILIntrinsicTypeSpec::I8";
    } else if (type == "i1") {
        return "DXILIntrinsicTypeSpec::I1";
    } else if (type == "%dx.types.Handle") {
        return "DXILIntrinsicTypeSpec::Handle";
    } else if (type == "%dx.types.Dimensions") {
        return "DXILIntrinsicTypeSpec::Dimensions";
    }

    // Unknown
    return "";
}

bool Generators::DXILIntrinsics(const GeneratorInfo &info, TemplateEngine &templateEngine) {
    std::stringstream intrinsics;

    // Current UID
    uint32_t uid = 0;

    // Regex patterns
    std::regex declarePattern("(\\:\\:)((\\s|(;.*$))*)declare (%?[A-Za-z.0-9]+) @([A-Za-z.0-9]+)\\(");
    std::regex parameterPattern("\\s*(%?[A-Za-z0-9\\.]+)(,|\\))(\\s+; (.*))?$");
    std::regex overloadPattern("(f16|f32|f64|i1|i8|i16|i32|i64)");

    // For all declarations
    for(std::sregex_iterator m = std::sregex_iterator(info.dxilRST.begin(), info.dxilRST.end(), declarePattern); m != std::sregex_iterator(); m++) {
        // Any error?
        bool wasError = false;

        // Overload status
        auto overloadStr = (*m)[2].str();

        // All overloads
        std::vector<std::string> overloads;

        // For all overloads
        for(std::sregex_iterator p = std::sregex_iterator(overloadStr.begin(), overloadStr.end(), overloadPattern); p != std::sregex_iterator(); p++) {
            std::string overload = (*p)[1].str();

            // Ignore duplicates
            if (std::find(overloads.begin(), overloads.end(), overload) != overloads.end()) {
                continue;
            }

            // Add overload
            overloads.push_back(overload);
        }

        // Translate return type
        std::string returnType = TranslateType((*m)[5].str());
        if (returnType.empty()) {
            continue;
        }

        // Dummy overload
        if (overloads.empty()) {
            overloads.push_back("");
        }

        // Push all overloads
        for (const std::string& overload : overloads) {
            // Get name
            std::string name;
            if (overload == "") {
                name = (*m)[6].str();
            } else {
                name = (*m)[6].str();

                // Remove default overload
                name.erase(name.begin() + name.find_last_of('.') + 1, name.end());

                // Add overload
                name += overload;
            }

            // Convert key wise name
            std::string keyName;
            for (size_t i = 0; i < name.length(); i++) {
                if (name[i] == '.') {
                    continue;
                }

                // Capitalize first character and after '.'
                keyName.push_back((i == 0 || name[i - 1] == '.') ? std::toupper(name[i]) : name[i]);
            }

            // Optional overload type
            std::string overloadType;

            // Translate overload
            if (!overload.empty()) {
                // Translate overload type
                overloadType = TranslateType(overload);
                if (overloadType.empty()) {
                    continue;
                }
            }

            // Override return type if need be
            if (!overload.empty() && returnType != "DXILIntrinsicTypeSpec::Void") {
                returnType = overloadType;
            }

            // All parameters
            std::vector<ParameterInfo> parameters;

            // For all parameters
            for(std::sregex_iterator p = std::sregex_iterator(info.dxilRST.begin() + m->position() + m->length(), info.dxilRST.end(), parameterPattern); p != std::sregex_iterator(); p++) {
                // Translate parameter type
                std::string paramType = TranslateType((*p)[1].str());
                if (paramType.empty()) {
                    wasError = true;
                    break;
                }

                // Get info
                std::string paramInfo = (*p)[4].str();

                // Extremely crude overload deduction
                if (paramInfo.find("value") != std::string::npos) {
                    paramType = overloadType;
                }

                // Add parameter
                parameters.push_back(ParameterInfo {
                    .name = paramType,
                    .info = paramInfo
                });

                // End of intrinsic?
                if ((*p)[2].str() == ")") {
                    break;
                }
            }

            // Failed parsing?
            if (wasError) {
                continue;
            }

            // Emit intrinsic spec
            intrinsics << "\tstatic DXILIntrinsicSpec " << keyName << " {\n";
            intrinsics << "\t\t.uid = " << (uid++) << ",\n";
            intrinsics << "\t\t.name = \"" << name << "\",\n";
            intrinsics << "\t\t.returnType = " << returnType << ",\n";
            intrinsics << "\t\t.parameterTypes = {\n";

            // Emit parameters
            for (const ParameterInfo& param : parameters) {
                intrinsics << "\t\t\t" << param.name << ", // " << param.info << "\n";
            }

            // Close
            intrinsics << "\t\t}\n";
            intrinsics << "\t};\n\n";
        }
    }

    // Substitute keys
    templateEngine.Substitute("$INTRINSICS", intrinsics.str().c_str());

    // OK
    return true;
}
