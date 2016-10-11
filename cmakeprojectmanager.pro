## set the QTC_SOURCE environment variable to override the setting here
QTCREATOR_SOURCES = $$(QTC_SOURCE)

## set the QTC_BUILD environment variable to override the setting here
IDE_BUILD_TREE = $$(QTC_BUILD_INPLACE)

DEFINES += CMAKEPROJECTMANAGER_LIBRARY
include(cmakeprojectmanager_dependencies.pri)
include($$QTCREATOR_SOURCES/src/qtcreatorplugin.pri)

# For the highlighter:
INCLUDEPATH += $$QTCREATOR_SOURCES/src/plugins/texteditor

HEADERS = builddirmanager.h \
    cmakebuildinfo.h \
    cmakebuildstep.h \
    cmakeconfigitem.h \
    cmakeproject.h \
    cmakeprojectplugin.h \
    cmakeprojectmanager.h \
    cmakeprojectconstants.h \
    cmakeprojectnodes.h \
    cmakerunconfiguration.h \
    cmakebuildconfiguration.h \
    cmakeeditor.h \
    cmakelocatorfilter.h \
    cmakefilecompletionassist.h \
    cmaketool.h \
    cmakeparser.h \
    cmakesettingspage.h \
    cmakesnippetprovider.h \
    cmaketoolmanager.h \
    cmake_global.h \
    cmakekitinformation.h \
    cmakekitconfigwidget.h \
    cmakecbpparser.h \
    cmakefile.h \
    cmakebuildsettingswidget.h \
    cmakeindenter.h \
    cmakeautocompleter.h \
    configmodel.h \
    cmaketoolchaininfo.h

SOURCES = builddirmanager.cpp \
    cmakebuildstep.cpp \
    cmakeconfigitem.cpp \
    cmakeproject.cpp \
    cmakeprojectplugin.cpp \
    cmakeprojectmanager.cpp \
    cmakeprojectnodes.cpp \
    cmakerunconfiguration.cpp \
    cmakebuildconfiguration.cpp \
    cmakeeditor.cpp \
    cmakelocatorfilter.cpp \
    cmakefilecompletionassist.cpp \
    cmaketool.cpp \
    cmakeparser.cpp \
    cmakesettingspage.cpp \
    cmakesnippetprovider.cpp \
    cmaketoolmanager.cpp \
    cmakekitinformation.cpp \
    cmakekitconfigwidget.cpp \
    cmakecbpparser.cpp \
    cmakefile.cpp \
    cmakebuildsettingswidget.cpp \
    cmakeindenter.cpp \
    cmakeautocompleter.cpp \
    configmodel.cpp \
    cmaketoolchaininfo.cpp

RESOURCES += cmakeproject.qrc

OTHER_FILES += README.txt

WIZARD_FILES = wizard/cmake2/*

wizardfiles.files = $$WIZARD_FILES
wizardfiles.path = $$QTC_PREFIX/share/qtcreator/templates/wizards/projects/cmake2/
wizardfiles.CONFIG += no_check_exist
INSTALLS += wizardfiles


