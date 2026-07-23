cmake_minimum_required(VERSION 3.16)

project(180
    VERSION 1.0.0
    DESCRIPTION "AGV teach pendant HMI"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Out-of-source builds recommended:
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build -j

option(BUILD_MODBUS_BACKEND "Build libmodbus_backend shared library" ON)

find_package(Qt5 5.15 REQUIRED COMPONENTS
    Core
    Gui
    Network
    Widgets
    Qml
    Quick
    QuickWidgets
)

set(APP_SOURCES
    agvmodbusmanager.cpp
    animationmanager.cpp
    batterywidget.cpp
    enablebuttonworker.cpp
    featureswitchmanager.cpp
    featureswitchwidget.cpp
    main.cpp
    maindevicemodbusapi.cpp
    mainmodbuslabelmapper.cpp
    mainmodbuspoller.cpp
    mainwindow.cpp
    mainwindow_lifecycle.cpp
    mainmodbusconnector.cpp
    mainmodbusstatus.cpp
    mappingconfig.cpp
    modebuttonstyler.cpp
    navigationicon.cpp
    matrixkeymonitor.cpp
    matrixkeythreadmanager.cpp
    modbustcpclient.cpp
    modbusthreadmanager.cpp
    modbusstringregisters.cpp
    modbuswritegate.cpp
    modbusvariables.cpp
    operationrecorder.cpp
    poseprovider.cpp
    speedmodeselector.cpp
    steeringmodeselector.cpp
    techarcgauge.cpp
    techchamfertoolbutton.cpp
    techpushbutton.cpp
    techshapes.cpp
    techslideredit.cpp
    techsliderlabel.cpp
    techspeedgauge.cpp
    techvirtualkeyboard.cpp
)

# Keep in sync with 180.pro HEADERS (orphan Q_OBJECT headers without .cpp
# must not be listed, or AUTOMOC will emit undefined references).
set(APP_HEADERS
    agvmodbusmanager.h
    enablebuttonworker.h
    featureswitchmanager.h
    featureswitchwidget.h
    maindevicemodbusapi.h
    mainmodbuslabelmapper.h
    mainmodbuspoller.h
    mainwindow.h
    mainmodbusconnector.h
    mainmodbusstatus.h
    mappingconfig.h
    modebuttonstyler.h
    navigationicon.h
    matrixkeymonitor.h
    matrixkeythreadmanager.h
    modbustcpclient.h
    modbusthreadmanager.h
    modbusstringregisters.h
    modbuswritegate.h
    modbusvariables.h
    operationrecorder.h
    poseprovider.h
    speedmodeselector.h
    steeringmodeselector.h
    techarcgauge.h
    techchamfertoolbutton.h
    techpushbutton.h
    techshapes.h
    techslideredit.h
    techsliderlabel.h
    techspeedgauge.h
    techvirtualkeyboard.h
    batterywidget.h
)

set(APP_FORMS
    mainwindow.ui
)

set(APP_RESOURCES
    res.qrc
)

add_executable(180
    ${APP_SOURCES}
    ${APP_HEADERS}
    ${APP_FORMS}
    ${APP_RESOURCES}
)

target_link_libraries(180 PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Network
    Qt5::Widgets
    Qt5::Qml
    Qt5::Quick
    Qt5::QuickWidgets
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(180 PRIVATE -Wall -Wextra)
endif()

# Optional Doxygen docs: cmake --build build --target docs
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/Doxyfile")
    add_custom_target(docs
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/docs/doxygen"
        COMMAND ${DOXYGEN_EXECUTABLE} "${CMAKE_SOURCE_DIR}/Doxyfile"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Generating API docs with Doxygen"
        VERBATIM
    )
endif()

# ---------------------------------------------------------------------------
# libmodbus_backend.so (same role as build_modbus_backend_local.sh)
# ---------------------------------------------------------------------------
if(BUILD_MODBUS_BACKEND)
    set(LIBMODBUS_ROOT "${CMAKE_SOURCE_DIR}/third_party/libmodbus-local"
        CACHE PATH "Prefix of locally built static libmodbus (include/ + lib/)")

    set(_libmodbus_include "${LIBMODBUS_ROOT}/include")
    set(_libmodbus_a "${LIBMODBUS_ROOT}/lib/libmodbus.a")

    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(PKG_LIBMODBUS QUIET libmodbus)
    endif()

    if(EXISTS "${_libmodbus_a}" AND EXISTS "${_libmodbus_include}/modbus/modbus.h")
        set(MODBUS_BACKEND_INCLUDES "${_libmodbus_include}")
        set(MODBUS_BACKEND_LIBS "${_libmodbus_a}")
        message(STATUS "modbus backend: using static libmodbus at ${LIBMODBUS_ROOT}")
    elseif(PKG_LIBMODBUS_FOUND)
        set(MODBUS_BACKEND_INCLUDES ${PKG_LIBMODBUS_INCLUDE_DIRS})
        set(MODBUS_BACKEND_LIBS ${PKG_LIBMODBUS_LIBRARIES})
        message(STATUS "modbus backend: using system libmodbus via pkg-config")
    else()
        message(WARNING
            "BUILD_MODBUS_BACKEND=ON but libmodbus not found.\n"
            "  Expected: ${_libmodbus_a}\n"
            "  Or install libmodbus and pkg-config.\n"
            "  You can still run: ./build_modbus_backend_local.sh\n"
            "  Or reconfigure with -DBUILD_MODBUS_BACKEND=OFF")
        set(BUILD_MODBUS_BACKEND OFF)
    endif()
endif()

if(BUILD_MODBUS_BACKEND)
    add_library(modbus_backend SHARED
        modbus_backend_c.cpp
    )
    set_target_properties(modbus_backend PROPERTIES
        OUTPUT_NAME "modbus_backend"
        PREFIX "lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
    target_include_directories(modbus_backend PRIVATE ${MODBUS_BACKEND_INCLUDES})
    target_link_libraries(modbus_backend PRIVATE ${MODBUS_BACKEND_LIBS})
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(modbus_backend PRIVATE -Wall -Wextra -fPIC)
    endif()
    # Keep ABI symbols visible for QLibrary::resolve
    set_target_properties(modbus_backend PROPERTIES
        CXX_VISIBILITY_PRESET default
        VISIBILITY_INLINES_HIDDEN OFF
    )
endif()

install(TARGETS 180 RUNTIME DESTINATION bin)
if(TARGET modbus_backend)
    install(TARGETS modbus_backend LIBRARY DESTINATION lib)
endif()
