CMakeProjectManager2
====================

Alternative CMake support for Qt Creator.


Main differents from original CMakeProject plugin:

  * Project file list readed from file system instead of parsing .cbp (CodeBlocks) project file.
    It can be slow on big projects, but reloading initiating only during open project, run CMake,
    or user-initiated tree changes (add new files, erase files, rename file)

  * You can add new files from Qt Creator. Note, you can create file outside project tree,
    but Qt Creator will display only files that present in project tree. Also, you should
    manualy describe this files in CMakeLists.txt.

  * You can rename files in file system from Qt Creator now.

  * You can erase files from files system from Qt Creator now.

Also, I think, that a lot of bugs also introduced :-)


TODO
----

Currently functional is good for me, but I have some ideas to future:

  * Big idea: remove .cbp parsing and parse CMakeLists.txt directly (far far far future :-)

  * Display sources and other files per targets (like KDevelop4)

  * On adding new files add its to specified targets. Similar behaviour on deleting or renaming
    files.

So, If anybody implements this feathures it will be very good :-)


Build plugin
------------

This plugin is oriented to latest GIT version of Qt Creator, sorry I use it and I have no time
to support other stable versions.

1. Prepare Qt Creator (without Full Build)
   For example, all actions runs in directory /tmp/qt-creator

   - Take full Qt Creator source tree from GIT:
       git clone git://gitorious.org/qt-creator/qt-creator.git qt-creator

   - Create Qt Crator build tree:
       mkdir qt-creator-build
       cd qt-creator-build

   - Create shadow build (I use GIT-version of Qt for building):
       /opt/qt-git/bin/qmake /tmp/qt-creator/qt-creator/qtcreator.pro

   - Don't start full build, but generating some files is requred (like ide_version.h):
     - For Qt5 based builds (this call fails but needed files it generate):
         make sub-src-clean
     - For Qt4 based builds:
         make src/Makefile
         make -C src plugins/Makefile
         make -C src/plugins coreplugin/Makefile

   Ok, Qt Creator source and build tree is prepared. Note, I think, that GIT version of Qt Creator
   already installed, for example to /opt/qtcreator-git

2. Build plugin

   - Change directory to /tmp/qt-creator

   - Take sources of CMakeProjectManager2 from Gitorious.org

   - Export two variables
     QTC_SOURCE - point to Qt Creator source tree (in out case: /tmp/qt-creator/qt-cretor)
     QTC_BUILD  - point to Qt Creator build tree (or installed files, like /opt/qtcreator-git)
       export QTC_SOURCE=/tmp/qt-creator/qt-creator
       export QTC_BUILD=/opt/qtcreator-git

   - Create directory for shadow build:
       mkdir cmakeprojectmanager2-build
       cd cmakeprojectmanager2-build

   - Configure plugin:
       /opt/qt-git/bin/qmake QTC_PREFIX=/opt/qtcreator-git
                             LIBS+=-L${QTC_BUILD}/lib/qtcreator \
                             LIBS+=-L${QTC_BUILD}/lib/qtcreator/plugins \
                             ../cmakeprojectmanager2-git/cmakeprojectmanager.pro

      LIBS need for linker, additional INCLUDEPATH need for ide_version.h view.
      QTC_PREFIX by default is '/usr/local'

   - Build:
       make

   - Install
     - to QTC_PREFIX:
         sudo make install
     - for packagers:
         sudo make INSTALL_ROOT=/tmp/plugin install

   Restart Qt Creator, go to Help -> About plugins and turn off default CMakeProjectManager plugin.

3. Build plugin from Qt Creator

   - Open cmakeprojectmanager.pro, go to Projects layout (see left panel)

   - Look to Build Steps section and add next params for qmake:
      LIBS+=-L${QTC_BUILD}/lib/qtcreator
      LIBS+=-L${QTC_BUILD}/lib/qtcreator/plugins

   - Look to Build Environment and define next variables:
      QTC_SOURCE=/tmp/qt-creator/qt-creator
      QTC_BUILD=/opt/qtcreator-git

   Now you can build plugin from Qt Creator.

