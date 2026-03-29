cmake_minimum_required(VERSION 3.16)

project(pycdc-studio VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)

set(PROJECT_SOURCES
    main.cpp
    src/app/app_context.cpp
    src/app/app_context.h
    src/ai/ai_provider_config.cpp
    src/ai/ai_provider_config.h
    src/ai/openai_compatible_client.cpp
    src/ai/openai_compatible_client.h
    src/ai/prompt_builder.cpp
    src/ai/prompt_builder.h
    src/model/code_object_node.h
    src/model/decompile_result.h
    src/model/ai_reconstruction_result.h
    src/model/project_session.cpp
    src/model/project_session.h
    src/service/pycdc_process_runner.cpp
    src/service/pycdc_process_runner.h
    src/service/pycdas_process_runner.cpp
    src/service/pycdas_process_runner.h
    src/service/pycdas_tree_parser.cpp
    src/service/pycdas_tree_parser.h
    src/service/decompiler_service.cpp
    src/service/decompiler_service.h
    src/service/fallback_service.cpp
    src/service/fallback_service.h
    src/ui/lucide_icon_factory.cpp
    src/ui/lucide_icon_factory.h
    src/ui/main_window.cpp
    src/ui/main_window.h
    src/ui/settings_dialog.cpp
    src/ui/settings_dialog.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(pycdc-studio
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(pycdc-studio SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(pycdc-studio
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_include_directories(pycdc-studio PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(pycdc-studio PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network)

if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.pycdc-studio)
endif()
set_target_properties(pycdc-studio PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS pycdc-studio
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(pycdc-studio)
endif()
