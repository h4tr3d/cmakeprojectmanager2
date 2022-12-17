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
* Classic tree view for CMake Server mode (uses automatically with CMake >= 3.7)

This functionality is not compatible with QtC Project View idea: **Project View == Build System view**. So, upstream accepts only changes that can be done via build system tools and API.
CMake does not provide way to modify CMakeLists.txt and related files, add/remove/rename file to targets and so on. So such changes in plugin (different kinds of work arounds) will be
dropped. But in my opinion: usability of IDE must be putted to first place. If some kind of WA that simplify work with CMake cab be implemented - it must be implemented.



Build plugin
------------

This plugin is oriented to latest Git version of Qt Creator, sorry I use it and I have no time
to support other stable versions.



### Prepare Qt Creator

Commit 9d8a419d107ae8219c84bc9178bfed76b94fa930 of the Qt Creator completely remove build using `qmake`. So, instruction updated to build plugin with CMake.

If you have QtC binary installation that contains `lib/cmake/QtCreator` and headers you should not do anything.

Otherwise, you must get full copy of the Qt Creator from the Git, build it and install to some prefix.

For example, all actions runs in directory `/tmp/qt-creator`

- Take full Qt Creator source tree from Git:

    ```bash
    git clone https://github.com/qtproject/qt-creator.git qt-creator
    ```

- Create Qt Crator build tree:

   ```bash
    mkdir qt-creator-build
    cd qt-creator-build
   ```

- Create shadow build:

    ```bash
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/opt/qtcreator-git \
          -DCMAKE_PREFIX_PATH=/usr \
          ..
    # CMAKE_PREFIX_PATH needed to point right location of the LLVM
    ```

- Build and install it

    ```bash
    cmake --build . -j 8 && \
    sudo cmake --install . && \
    sudo cmake --install . --component Devel
    # Devel strongly are needed
    ```
  - Latest QtC missed `tl_expected` packages, copy it manually
    ```bash
    cp -a "$qtc_source/src/libs/3rdparty/tl_expected" "$PREFIX/include/qtcreator/src/libs/3rdparty/"
    ```

Also, refer to the next page for more details:

* https://wiki.qt.io/Building-Qt-Creator-Packages



### Build plugin

- Change directory to `/tmp/qt-creator`

- Take sources of CMakeProjectManager2 from repository

- Create directory for shadow build:
  ```bash
  mkdir cmakeprojectmanager2-build
  cd cmakeprojectmanager2-build
  ```
  
- Configure plugin:
  ```bash
  cmake -DCMAKE_PREFIX_PATH="/opt/qtcreator-git" \
        -DCMAKE_INSTALL_PREFIX="/opt/qtcreator-git" \
        ../cmakeprojectmanager2-git/
  # CMAKE_PREFIX_PATH strogly needed if QtC installed to the non-default CMake prefix (/usr in most cases)
  ```
  Build:
  
  ```bash
  cmake --build . -j 8
  ```
  
- Install
  
  ```bash
  sudo cmake --install .
  ```

Restart Qt Creator, go to Help -> About plugins and turn off default CMakeProjectManager plugin.



### Build plugin from Qt Creator

- Open `cmakeprojectmanager2-git/CMakeLists.txt`, go to Projects layout (see left panel)
- Look to Build Steps section and add next params for CMake:
  ```bash
  -DCMAKE_PREFIX_PATH="/opt/qtcreator-git"
  ```

Now you can build plugin from Qt Creator.

Also, refer to the Qt Creator WiKi about plugins development:

* https://doc.qt.io/qtcreator-extending/first-plugin.html
* https://bugreports.qt.io/browse/QTCREATORBUG-22514



Prebuilt binaries
-----------------

Unsupported anymore. Sorry, just have no a time :-(



TODO & Roadmap
--------------

Actual tasks and todo can be looks at the Issue page: https://github.com/h4tr3d/cmakeprojectmanager2/issues



Sync with Qt Creator upstream plugin
------------------------------------

1. Update Qt Creator git repository, moves to the master branch
2. Create patches for new changes
```
git format-patch <REVISION_SINCE> -- src/plugins/cmakeprojectmanager
```
REVISION_SINCE can be found via 'git log' by comments or Change-Id.
3. Go to CMakeProjectManager2 source tree and change branch to the `qtc-master`
4. Copy patches from the step 2 to the CMakeProjectManager2 source tree root
5. Apply patches:
```
git am -p4 *.patch
```
6. Change branch to the `master`, merge new changes and resolve conflicts
