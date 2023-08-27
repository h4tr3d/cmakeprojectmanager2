cmake_minimum_required(VERSION 3.10)

project(CMakeProjectManager2)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 17)

find_package(QtCreator REQUIRED COMPONENTS Core CppEditor ProjectExplorer TextEditor QtSupport)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
#find_package(QT NAMES Qt5 REQUIRED COMPONENTS Widgets)
set(QtX Qt${QT_VERSION_MAJOR})


add_qtc_plugin(CMakeProjectManager2
  PLUGIN_CLASS CMakeProjectPlugin
  DEPENDS 
    QtCreator::QmlJS
    QtCreator::app_version
  PLUGIN_DEPENDS 
    QtCreator::CppEditor
    QtCreator::ProjectExplorer
    QtCreator::TextEditor
    QtCreator::QtSupport
  SOURCES
    builddirparameters.cpp builddirparameters.h
    cmake_global.h
    cmakeabstractprocessstep.cpp cmakeabstractprocessstep.h
    cmakeautocompleter.cpp cmakeautocompleter.h
    cmakebuildconfiguration.cpp cmakebuildconfiguration.h
    cmakebuildsystem.cpp cmakebuildsystem.h
    cmakebuildstep.cpp cmakebuildstep.h
    cmakebuildtarget.h
    cmakeconfigitem.cpp cmakeconfigitem.h
    cmakeeditor.cpp cmakeeditor.h
    cmakefilecompletionassist.cpp cmakefilecompletionassist.h
    cmakeformatter.cpp cmakeformatter.h
    cmakeindenter.cpp cmakeindenter.h
    cmakeinstallstep.cpp cmakeinstallstep.h
    cmakekitaspect.cpp cmakekitaspect.h
    cmakelocatorfilter.cpp cmakelocatorfilter.h
    cmakeparser.cpp cmakeparser.h
    cmakeprocess.cpp cmakeprocess.h
    cmakeproject.cpp cmakeproject.h
    cmakeproject.qrc
    cmakeprojectconstants.h
    cmakeprojectimporter.cpp cmakeprojectimporter.h
    cmakeprojectmanager.cpp cmakeprojectmanager.h
    cmakeprojectmanagertr.h
    cmakeprojectnodes.cpp cmakeprojectnodes.h
    cmakeprojectplugin.cpp cmakeprojectplugin.h
    cmakesettingspage.cpp cmakesettingspage.h
    cmakespecificsettings.cpp cmakespecificsettings.h
    cmaketool.cpp cmaketool.h
    cmaketoolmanager.cpp cmaketoolmanager.h
    cmaketoolsettingsaccessor.cpp cmaketoolsettingsaccessor.h
    configmodel.cpp configmodel.h
    configmodelitemdelegate.cpp configmodelitemdelegate.h
    fileapidataextractor.cpp fileapidataextractor.h
    fileapiparser.cpp fileapiparser.h
    fileapireader.cpp fileapireader.h
    presetsparser.cpp presetsparser.h
    presetsmacros.cpp presetsmacros.h
    projecttreehelper.cpp projecttreehelper.h
    simplefileapireader.cpp simplefileapireader.h
    3rdparty/cmake/cmListFileCache.cxx
    3rdparty/cmake/cmListFileLexer.cxx
    3rdparty/cmake/cmListFileCache.h
)


# create install rule for resource directories
install(
  DIRECTORY wizard/cmake2
  DESTINATION "${IDE_DATA_PATH}/templates/wizards/projects/"
  USE_SOURCE_PERMISSIONS
)

