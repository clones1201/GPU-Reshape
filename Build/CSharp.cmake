# Quick library setup
function(Project_AddDotNet NAME)
    add_library(
        ${NAME} SHARED
        ${ARGN}
    )

    # DotNet
    set_target_properties(
        ${NAME} PROPERTIES
        VS_DOTNET_TARGET_FRAMEWORK_VERSION "v4.8"
        VS_GLOBAL_ROOTNAMESPACE ${NAME}
    )

    # IDE source discovery
    SetSourceDiscovery(${NAME} CS Include Source Schema)
endfunction()

function(Project_AddDotNetEx)
    cmake_parse_arguments(
        ARGS
        "UNSAFE" # Options
        "NAME;LANG;PROPS" # One Value
        "SOURCE;GENERATED;ASSEMBLIES;LIBS;FLAGS" # Multi Value
        ${ARGN}
    )

    if (NOT "${ARGS_GENERATED}" STREQUAL "")
        # Generate sham target
        #   ! WORKAROUND, Visual Studio generators do not support C# sources from add_custom_command
        #                 Check introduced by 3.24
        add_library(${ARGS_NAME}.Sham INTERFACE ${${ARGS_GENERATED}_Sham})
    endif()

    # Create library
    add_library(
        ${ARGS_NAME} SHARED
        ${ARGS_SOURCE}
        ${${ARGS_GENERATED}}
    )

    # CLI Includes
    if ("${ARGS_LANG}" STREQUAL "CXX")
        target_include_directories(${ARGS_NAME} PUBLIC Include)

        # Enable CLR
        set_target_properties(${ARGS_NAME} PROPERTIES COMMON_LANGUAGE_RUNTIME "")
    endif()

    # Set .NET, link to assemblies and libs
    set_target_properties(
        ${ARGS_NAME} PROPERTIES
        VS_DOTNET_TARGET_FRAMEWORK_VERSION "v4.8"
        VS_GLOBAL_ROOTNAMESPACE "${ARGS_NAME}"
        VS_DOTNET_REFERENCES "${ARGS_ASSEMBLIES};${ARGS_LIBS}"
    )

    # Add flags
    if (NOT "${ARGS_FLAGS}" STREQUAL "")
        target_compile_options(${ARGS_NAME} PUBLIC "${ARGS_FLAGS}")
    endif()

    # Unsafe compilation?
    if ("${ARGS_UNSAFE}")
        set_target_properties(${ARGS_NAME} PROPERTIES VS_GLOBAL_AllowUnsafeBlocks "true")
    endif()

    # Set properties
    if (NOT "${ARGS_PROPS}" STREQUAL "")
        set_target_properties(
            ${ARGS_NAME} PROPERTIES
            VS_USER_PROPS "${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_PROPS}"
        )
    endif()

    # Add explicit links and dependencies
    if (NOT "${ARGS_LIBS}" STREQUAL "")
        target_link_libraries(${ARGS_NAME} PUBLIC ${ARGS_LIBS})

        if (NOT "${ARGS_LANG}" STREQUAL "CXX")
            add_dependencies(${ARGS_NAME} ${ARGS_LIBS})
        endif()
    endif()

    # Reference sham target to let dependencies generate before use
    if (NOT "${ARGS_GENERATED}" STREQUAL "")
        add_dependencies(${ARGS_NAME} ${ARGS_NAME}.Sham)
    endif()

    # IDE source discovery
    SetSourceDiscovery(${ARGS_NAME} ${ARGS_LANG} Include Source)
endfunction()
