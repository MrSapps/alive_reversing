cmake_minimum_required(VERSION 3.10)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt uses the dynamic CRT, so every target in this directory does too and must only link
# *_dynamic_crt libs - see the MSVC CRT comment in the top-level CMakeLists.txt.
set(CMAKE_MSVC_RUNTIME_LIBRARY ${RELIVE_DYNAMIC_CRT})

# Qt 5.15's QList uses stdext::checked_array_iterator, which current MSVC STLs deprecate (STL4043).
if (MSVC)
    add_compile_definitions(_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
endif()

# QtCreator supports the following variables for Android, which are identical to qmake Android variables.
# Check https://doc.qt.io/qt/deployment-android.html for more information.
# They need to be set before the find_package( ...) calls below.

#if(ANDROID)
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
#    if (ANDROID_ABI STREQUAL "armeabi-v7a")
#        set(ANDROID_EXTRA_LIBS
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libcrypto.so
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libssl.so)
#    endif()
#endif()

find_package(QT NAMES Qt5 COMPONENTS Widgets Network LinguistTools REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets Network LinguistTools REQUIRED)

set(TS_FILES
  Source/rsc/translations/qt-editor_en_GB.ts
  Source/rsc/translations/qt-editor_de_DE.ts)

include_directories(../../../3rdParty/libmodplug/src)
include_directories(../../../3rdParty/EasyLogging++/EasyLogging/src)
include_directories(../../../3rdParty/json/single_include)

include_directories(Source)

add_library(editor_glue_dynamic_crt
    # The relive_lib bits the editor uses, compiled against the dynamic CRT - relive_lib
    # itself uses the static CRT so it can't be linked directly.
    ../../relive_lib/Grid.cpp
    ../../AliveLibAE/PathData.cpp
    ../../AliveLibAO/PathData.cpp
    ../../relive_lib/CompressionType_4Or5.cpp
    ../../relive_lib/CamDecompressor.cpp
    ../../relive_lib/AnimResources.cpp
    ../../relive_lib/MapWrapper.cpp
    ../../relive_lib/data_conversion/PNGFile.cpp
    ../../relive_lib/data_conversion/file_system.cpp
    ../../relive_lib/data_conversion/AutoFile.cpp
    ../../relive_lib/data_conversion/guid.cpp
    ../../relive_lib/data_conversion/relive_tlvs_serialization.cpp
    ../../relive_lib/data_conversion/CommonTypesSerialization.cpp
    ../../relive_lib/data_conversion/EditorMapObjects.cpp
)
target_include_directories(editor_glue_dynamic_crt PRIVATE
    ../../relive_lib
)
target_link_libraries(editor_glue_dynamic_crt PUBLIC spng_dynamic_crt miniz_dynamic_crt SDL3::SDL3)

add_library(relive_ipc_dynamic_crt
    ../../relive_lib/Ipc/Ipc.cpp
    ../../relive_lib/Ipc/Win32Ipc.cpp
    ../../relive_lib/Ipc/UnixIpc.cpp
)
target_include_directories(relive_ipc_dynamic_crt PRIVATE
    ../../relive_lib
)

set(PROJECT_SOURCES
        Source/GridSnapSettings.hpp
        Source/IGridPointSnapper.hpp
        Source/ClipBoard.cpp
        Source/ClipBoard.hpp
        Source/PasteItemsCommand.cpp
        Source/PasteItemsCommand.hpp
        Source/DeleteItemsCommand.cpp
        Source/DeleteItemsCommand.hpp
        Source/SetSelectionCommand.cpp
        Source/SetSelectionCommand.hpp
        Source/MoveItemsCommand.cpp
        Source/MoveItemsCommand.hpp
        Source/AddCollisionCommand.cpp
        Source/AddCollisionCommand.hpp
        Source/ProgressDialog.hpp
        Source/ProgressDialog.cpp
        Source/ProgressDialog.ui
        Source/TransparencyDialog.hpp
        Source/TransparencyDialog.cpp
        Source/TransparencyDialog.ui
        Source/SelectionSaver.hpp
        Source/AddObjectDialog.hpp
        Source/AddObjectDialog.cpp
        Source/AddObjectDialog.ui
        Source/ChangeMapSizeDialog.hpp
        Source/ChangeMapSizeDialog.cpp
        Source/ChangeMapSizeDialog.ui
        Source/PathDataEditorDialog.hpp
        Source/PathDataEditorDialog.cpp
        Source/PathDataEditorDialog.ui
        Source/MessageEditorDialog.cpp
        Source/MessageEditorDialog.hpp
        Source/MessageEditorDialog.ui
        Source/CameraManager.cpp
        Source/CameraManager.hpp
        Source/CameraManager.ui
        Source/EditorMod.hpp
        Source/EditorMod.cpp
        Source/ModTreeItem.hpp
        Source/ModTreeItem.cpp
        Source/ModTreeWidget.hpp
        Source/ModTreeWidget.cpp
        Source/NewModDialog.hpp
        Source/NewModDialog.cpp
        Source/NewModDialog.ui
        Source/CollisionConnect.cpp
        Source/CollisionConnect.hpp
        Source/IGraphicsItem.hpp
        Source/main.cpp
        Source/AutomationProtocol.hpp
        Source/AutomationServer.hpp
        Source/AutomationServer.cpp
        Source/AutomationCommands.hpp
        Source/AutomationCommands.cpp
        Source/AutomationSceneCommands.hpp
        Source/AutomationSceneCommands.cpp
        Source/AboutDialog.ui
        Source/EditorMainWindow.ui
        Source/EditorTab.ui
        Source/EditorMainWindow.cpp
        Source/EditorMainWindow.hpp
        Source/EditorTab.cpp
        Source/EditorTab.hpp
        Source/EditorGraphicsView.cpp
        Source/EditorGraphicsView.hpp
        Source/FireWidget.cpp
        Source/FireWidget.hpp
        Source/AboutDialog.cpp
        Source/AboutDialog.hpp
        Source/Model.cpp
        Source/Model.hpp
        Source/EditorCamera.hpp
        Source/CollisionObject.cpp
        Source/CollisionObject.hpp
        Source/ResizeableArrowItem.cpp
        Source/ResizeableArrowItem.hpp
        Source/ResizeableRectItem.cpp
        Source/ResizeableRectItem.hpp
        Source/CameraGraphicsItem.cpp
        Source/CameraGraphicsItem.hpp
        Source/EditorGraphicsScene.cpp
        Source/EditorGraphicsScene.hpp
        Source/ItemPositionData.cpp
        Source/ItemPositionData.hpp
        Source/TransparencySettings.hpp
        Source/BigSpinBox.cpp
        Source/BigSpinBox.hpp
        Source/PropertyTreeItemBase.hpp
        Source/ISyncPropertiesToTree.hpp
        Source/StringProperty.hpp
        Source/StringProperty.cpp
        Source/ChangeStringPropertyCommand.hpp
        Source/ChangeStringPropertyCommand.cpp
        Source/ReadOnlyStringProperty.hpp
        Source/ReadOnlyStringProperty.cpp
        Source/PropertyTreeWidget.cpp
        Source/PropertyTreeWidget.hpp
        Source/BasicTypeProperty.hpp
        Source/BasicTypeProperty.cpp
        Source/ChangeBasicTypePropertyCommand.hpp
        Source/ChangeBasicTypePropertyCommand.cpp
        Source/IntegerType.hpp
        Source/BasicTypePropertyChangeData.hpp
        Source/LinkedBasicTypeProperty.hpp
        Source/BoolProperty.hpp
        Source/BoolProperty.cpp
        Source/ChangeBoolPropertyCommand.hpp
        Source/ChangeBoolPropertyCommand.cpp
        Source/BoolPropertyChangeData.hpp
        Source/LinkedBoolProperty.hpp
        Source/EnumProperty.cpp
        Source/EnumProperty.hpp
        Source/EnumPropertyBase.hpp
        Source/EnumPropertyChangeData.hpp
        Source/LinkedEnumProperty.hpp
        Source/ChangeEnumPropertyCommand.hpp
        Source/ReflectedEnumProperties.hpp
        Source/ColourPickerProperty.cpp
        Source/ColourPickerProperty.hpp
        Source/ChangeColourPickerPropertyCommand.hpp
        Source/ChangeColourPickerPropertyCommand.cpp
        ${TS_FILES}
)

#if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
#    qt_create_translation(QM_FILES "${CMAKE_CURRENT_SOURCE_DIR}/Source" ${TS_FILES} OPTIONS -no-obsolete -no-ui-lines -locations none)
#    qt_add_executable(relive-editor ${PROJECT_SOURCES} ${QM_FILES})
#else()
    # Only compile the .ts files to .qm in the build. qt5_create_translation also ran lupdate over
    # the whole source dir whenever a source changed, which slowed builds down and rewrote the
    # checked-in .ts files (dropping existing translations). Refresh them explicitly with:
    #   cmake --build build --target update_translations
    qt5_add_translation(QM_FILES ${TS_FILES})
    add_custom_target(update_translations
        COMMAND Qt5::lupdate "${CMAKE_CURRENT_SOURCE_DIR}/Source" -no-obsolete -no-ui-lines -locations none -ts ${TS_FILES}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM)
    # The editor's ~6 MB of resources: qt5_add_big_resources embeds them into an object file
    # directly instead of generating (and compiling) a ~27 MB C++ array, which took ~20 s.
    qt5_add_big_resources(EDITOR_RESOURCES Source/resources.qrc)
    if(WIN32)
        set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/Source/rsc/icons/relive-editor.rc")
        add_executable(relive-editor ${PROJECT_SOURCES} ${QM_FILES} ${EDITOR_RESOURCES} ${APP_ICON_RESOURCE_WINDOWS})
    else()
        add_executable(relive-editor ${PROJECT_SOURCES} ${QM_FILES} ${EDITOR_RESOURCES})
    endif()
#endif()

if (UNIX)
    SET(INSTAL_BINPATH "${CMAKE_BINARY_DIR}/CommonAppDir")
    install(TARGETS relive-editor DESTINATION "${INSTAL_BINPATH}/ReliveEditor/usr/bin")
endif()

if (MSVC)
    set(MY_PATH "PATH=%PATH%;${Qt5_DIR}/../../../bin")
    set_target_properties(relive-editor PROPERTIES VS_DEBUGGER_ENVIRONMENT "${MY_PATH}")
endif()

target_link_libraries(relive-editor PUBLIC SDL3::SDL3 vorbisfile_dynamic_crt Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network editor_glue_dynamic_crt relive_ipc_dynamic_crt)

add_executable(relive-editor-automation-cli tools/AutomationCli.cpp Source/AutomationProtocol.hpp)
target_include_directories(relive-editor-automation-cli PRIVATE Source)
target_link_libraries(relive-editor-automation-cli PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Network)

add_executable(EditorAutomationTests
    tests/EditorAutomationTestHelpers.hpp
    tests/EditorAutomationTestHelpers.cpp
    tests/EditorAutomationBasicTests.cpp
    tests/EditorAutomationMultiSelectTests.cpp
    tests/EditorAutomationEditTests.cpp
    tests/EditorAutomationPropertyAndModTests.cpp
    tests/EditorAutomationTreeTests.cpp
    Source/AutomationProtocol.hpp)
target_include_directories(EditorAutomationTests PRIVATE Source)
target_link_libraries(EditorAutomationTests PUBLIC googletest_dynamic_crt Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui)
target_compile_definitions(EditorAutomationTests PRIVATE RELIVE_EDITOR_PATH="$<TARGET_FILE:relive-editor>")
# The test files are split up so they build in parallel; the PCH stops each one re-parsing
# gtest, Qt and json.
target_precompile_headers(EditorAutomationTests PRIVATE
    <gtest/gtest.h>
    <nlohmann/json.hpp>
    <QCoreApplication>
    <QImage>
    <QLocalSocket>
    <QProcess>
    <QString>)
add_test(NAME EditorAutomationTests COMMAND EditorAutomationTests)

# Plain unit tests for pure placement math (GridPlacement.hpp) - no Qt/GUI dependency at all,
# unlike EditorAutomationTests above which drives the real editor process end-to-end.
add_executable(GridPlacementTests tests/GridPlacementTests.cpp Source/GridPlacement.hpp)
target_include_directories(GridPlacementTests PRIVATE Source)
target_link_libraries(GridPlacementTests PUBLIC googletest_dynamic_crt)
add_test(NAME GridPlacementTests COMMAND GridPlacementTests)