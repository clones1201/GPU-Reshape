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

#include "GenTypes.h"

#include <iostream>
#include <array>

bool Generators::CommandBufferDispatchTable(const GeneratorInfo& info, TemplateEngine& templateEngine) {
    // Get the commands
    tinyxml2::XMLElement *commands = info.registry->FirstChildElement("commands");
    if (!commands) {
        std::cerr << "Failed to find commands in registry" << std::endl;
        return false;
    }

    // Process all commands
    std::stringstream callbacks;
    for (tinyxml2::XMLNode *commandNode = commands->FirstChild(); commandNode; commandNode = commandNode->NextSibling()) {
        tinyxml2::XMLElement *command = commandNode->ToElement();

        // Is this a hook candidate?
        bool isHookCandidate = false;

        // Skip aliases
        if (command->Attribute("alias", nullptr)) {
            continue;
        }

        // Fiond the prototype definition
        tinyxml2::XMLElement *prototype = command->FirstChildElement("proto");
        if (!prototype) {
            std::cerr << "Malformed command in line: " << command->GetLineNum() << ", prototype not found" << std::endl;
            continue;
        }

        // Get the result
        tinyxml2::XMLElement *prototypeResult = prototype->FirstChildElement("type");
        if (!prototypeResult) {
            std::cerr << "Malformed command in line: " << command->GetLineNum() << ", prototype result not found" << std::endl;
            continue;
        }

        // Check return type
        isHookCandidate |= !std::strcmp(prototypeResult->GetText(), "VkCommandBuffer");

        // Get the name
        tinyxml2::XMLElement *prototypeName = prototype->FirstChildElement("name");
        if (!prototypeName) {
            std::cerr << "Malformed command in line: " << command->GetLineNum() << ", prototype name not found" << std::endl;
            continue;
        }

        // First parameter
        tinyxml2::XMLElement *firstParam = command->FirstChildElement("param");

        // Iterate all parameters
        uint32_t parameterIndex = 0;
        for (tinyxml2::XMLElement *paramNode = firstParam; paramNode; paramNode = paramNode->NextSiblingElement("param"), parameterIndex++) {
            // Get the type
            tinyxml2::XMLElement *paramType = paramNode->FirstChildElement("type");
            if (!paramType) {
                std::cerr << "Malformed parameter in line: " << command->GetLineNum() << ", type not found" << std::endl;
                continue;
            }

            // Note: This is obviously not correct, it may be inside compound types and such, but it's sufficient for prototyping
            isHookCandidate |= !std::strcmp(paramType->GetText(), "VkCommandBuffer");
        }

        // Skip if not interesting
        if (!isHookCandidate) {
            continue;
        }

        // Generate callback
        callbacks << "\n\t// Callback " << prototypeName->GetText() << "\n";
        callbacks << "\t" << "PFN_" << prototypeName->GetText() << " next_" << prototypeName->GetText() << ";\n";

        // Hooked?
        if (info.hooks.count(prototypeName->GetText())) {
            // Generate feature bit set
            callbacks << "\tuint64_t featureBitSet_" << prototypeName->GetText() << "{0};\n";

            // Generate feature bit set mask
            callbacks << "\tuint64_t featureBitSetMask_" << prototypeName->GetText() << "{0};\n";

            // Generate feature callbacks
            callbacks << "\tFeatureHook_" << prototypeName->GetText() << "::Hook featureHooks_" << prototypeName->GetText() << "[64];\n";
        }
    }

    // Instantiate template
    if (!templateEngine.Substitute("$COMMANDBUFFER_CALLBACKS", callbacks.str().c_str())) {
        std::cerr << "Bad template, failed to substitute $HOOKS" << std::endl;
        return false;
    }

    // OK
    return true;
}
