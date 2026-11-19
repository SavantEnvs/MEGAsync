# Wrapper project: builds upstream's desktop app tree unchanged (add_subdirectory of the repo
# root) and adds the app-based fuzz targets, linked the way src/MEGAAutoTests/UnitTests is.
cmake_minimum_required(VERSION 3.19)
project(megasync_mayhem LANGUAGES C CXX)
get_filename_component(MEGASYNC_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
add_subdirectory("${MEGASYNC_ROOT}" upstream)

find_package(Qt5 REQUIRED COMPONENTS Widgets Core Gui Network Qml Quick QuickWidgets Svg)
set(MegaSyncDir "${MEGASYNC_ROOT}/src/MEGASync")
# The desktop app's sources, compiled once into an OBJECT library and linked into every
# app-based fuzz target and its -standalone reproducer (upstream's cmake fragments add their
# sources, include dirs and link libraries to ${ExecutableTarget}).
add_library(megasync_app OBJECT)
set_target_properties(megasync_app PROPERTIES AUTOUIC ON AUTOMOC ON)
target_compile_definitions(megasync_app PUBLIC $<$<BOOL:${ENABLE_ISOLATED_GFX}>:ENABLE_SDK_ISOLATED_GFX>)
target_platform_compile_options(TARGET megasync_app UNIX -D__STDC_FORMAT_MACROS)
set_property(TARGET megasync_app PROPERTY AUTOUIC_SEARCH_PATHS
    ${MegaSyncDir}/gui/linux ${MegaSyncDir}/gui/node_selector/gui/linux ${MegaSyncDir}/gui/ui ${MegaSyncDir}/gui)
target_sources(megasync_app PRIVATE
    ${MegaSyncDir}/BlockingStageProgressController.cpp
    ${MegaSyncDir}/CommonMessages.cpp
    ${MegaSyncDir}/EventUpdater.cpp
    ${MegaSyncDir}/FolderTransferListener.cpp
    ${MegaSyncDir}/MegaApplication.cpp
    ${MegaSyncDir}/ScaleFactorManager.cpp
    ${MegaSyncDir}/ScanStageController.cpp
    ${MegaSyncDir}/TransferQuota.cpp
    ${MegaSyncDir}/UserAlertTimedClustering.cpp
    ${MegaSyncDir}/drivedata.cpp)
set(ExecutableTarget megasync_app)
set(DontUseResources ON)
include(${MegaSyncDir}/control/control.cmake)
include(${MegaSyncDir}/gui/gui.cmake)
include(${MegaSyncDir}/syncs/syncs.cmake)
include(${MegaSyncDir}/platform/platform.cmake)
include(${MegaSyncDir}/transfers/transfers.cmake)
include(${MegaSyncDir}/stalled_issues/stalledissues.cmake)
include(${MegaSyncDir}/node_selector/nodeselector.cmake)
include(${MegaSyncDir}/notifications/notifications.cmake)
include(${MegaSyncDir}/UserAttributesRequests/userattributesrequests.cmake)
target_include_directories(megasync_app PRIVATE ${MegaSyncDir})
target_link_libraries(megasync_app PUBLIC MEGA::SDKlib MEGA::SDKQtBindings
    Qt5::Svg Qt5::Widgets Qt5::Core Qt5::Gui Qt5::Network Qt5::Qml Qt5::Quick Qt5::QuickWidgets)

# The base image's run-once driver for the -standalone reproducers (C, compiled without SanCov).
add_library(standalone_main OBJECT "$ENV{STANDALONE_FUZZ_MAIN}")
set_source_files_properties("$ENV{STANDALONE_FUZZ_MAIN}" PROPERTIES LANGUAGE C)

# One libFuzzer binary + one -standalone reproducer per harness, both over the same app objects.
function(megasync_fuzz_target name)
  foreach(variant fuzz standalone)
    if(variant STREQUAL "fuzz")
      set(exe ${name})
    else()
      set(exe ${name}-standalone)
    endif()
    add_executable(${exe} "${MEGASYNC_ROOT}/mayhem/harnesses/${name}.cpp" "${MEGASYNC_ROOT}/mayhem/lsan_off.cc")
    target_include_directories(${exe} PRIVATE $<TARGET_PROPERTY:megasync_app,INCLUDE_DIRECTORIES>)
    target_compile_definitions(${exe} PRIVATE $<TARGET_PROPERTY:megasync_app,COMPILE_DEFINITIONS>)
    target_compile_options(${exe} PRIVATE $<TARGET_PROPERTY:megasync_app,COMPILE_OPTIONS>)
    target_link_libraries(${exe} PRIVATE megasync_app)
    if(variant STREQUAL "fuzz")
      target_link_options(${exe} PRIVATE -fsanitize=fuzzer)
    else()
      target_link_libraries(${exe} PRIVATE standalone_main)
    endif()
  endforeach()
endfunction()
megasync_fuzz_target(fuzz_httpserver)
megasync_fuzz_target(fuzz_megaignore)

# HTTPServer::discardClient() C-casts sender() (a QTcpSocket) to QSslSocket*; UBSan's vptr check
# aborts on every disconnect, so only that check is off, only for this file.
set_source_files_properties(${MegaSyncDir}/control/HTTPServer.cpp PROPERTIES COMPILE_OPTIONS "-fno-sanitize=vptr")
