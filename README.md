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

If you have QtC binary installation that contains .pro/.pri and headers you should not do anythind. Just setup `QTC_BUILD` to the QtC Prefix (like `/opt/qtcreator-git`) and `QTC_SOURCE` to the directory, contains `qtcreator.pri` file (like `/usr/src/qtcreator-git`).

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

- Create shadow build (if you use another Qt installation path, just provide correct path to the `qmake`):

    ```bash
    /opt/qt56/bin/qmake \
	            IDE_PACKAGE_MODE=1 \
              PREFIX=/opt/qtcreator-git \
	            LLVM_INSTALL_DIR=/usr/lib/llvm-3.9 \ 
              CONFIG+=qbs_enable_project_file_updates \
              /tmp/qt-creator/qt-creator/qtcreator.pro
    ```

- Build and install it

    ```bash
    make -j8 && sudo make install
    ```

If you want to re-use binary QtC installation without full rebuild, you should do steps like previous but with minor differences:

1. After repository clone you must checkout correct version of QtC.
2. Qt version must be same to the Qt uses to build binary QtC.
3. Options for the qmake must be same to the binary QtC one.
4. Build step can be omited. (TODO: possible you should run `make sub-src-clean` in build dir to prepare some files).


### Build plugin
- Change directory to `/tmp/qt-creator`
- Take sources of CMakeProjectManager2 from repository
- Export two variables
  * **QTC_SOURCE** - point to Qt Creator source tree (in out case: `/tmp/qt-creator/qt-cretor`)
  * **QTC_BUILD**  - point to Qt Creator build tree (or installed files, like `/opt/qtcreator-git`). Use this variable only in case, when you want
                     to put compiled files directly to the QtC tree. It fails, if QtC owned by other user but run build with sudo is a very very
                     bad idea.
  * **QTC_BUILD_INPLACE** - undefined by default. In most cases must be same to the **QTC_BUILD**. If defined, this location will be used as output
                            location for compiled plugin files (.so). As a result, you can omit install step. Useful only for individual users QtC 
                            installation at the home directory. If QtC installation does not owned by user who invoke compilation linking will be
                            fails. Strongly recommends do not use this variable, but use QTC_PREFIX for qmake and INSTALL_ROOT for make install.
  ```bash
  export QTC_SOURCE=/tmp/qt-creator/qt-creator
  export QTC_BUILD=/opt/qtcreator-git
  ```
- Create directory for shadow build:
```bash
mkdir cmakeprojectmanager2-build
cd cmakeprojectmanager2-build
```
- Configure plugin:
```bash
/opt/qt-git/bin/qmake QTC_PREFIX=/opt/qtcreator-git
                      LIBS+=-L${QTC_BUILD}/lib/qtcreator \
                      LIBS+=-L${QTC_BUILD}/lib/qtcreator/plugins \
                      ../cmakeprojectmanager2-git/cmakeprojectmanager.pro
```
  LIBS need for linker. QTC_PREFIX by default is `/usr/local`.
- Build:
```bash
 make -j8
 ```
- Install
  - to QTC_PREFIX:
  ```bash
  sudo make install
  ```
  - for packagers:
  ```bash
  make INSTALL_ROOT=/tmp/plugin install
  ```

Restart Qt Creator, go to Help -> About plugins and turn off default CMakeProjectManager plugin.

### Build plugin from Qt Creator
- Open `cmakeprojectmanager.pro`, go to Projects layout (see left panel)
- Look to Build Steps section and add next params for qmake:
```bash
LIBS+=-L${QTC_BUILD}/lib/qtcreator
LIBS+=-L${QTC_BUILD}/lib/qtcreator/plugins
```
- Look to Build Environment and define next variables:
```bash
QTC_SOURCE=/tmp/qt-creator/qt-creator
QTC_BUILD=/opt/qtcreator-git
```

Now you can build plugin from Qt Creator.

Prebuilt binaries
-----------------

Now Ubuntu 14.04/16.04 / Mint 17.x/18.x PPA with master-git Qt Creator and CMakePrjectManager2 plugin:<br />
https://launchpad.net/~adrozdoff/+archive/ubuntu/qtcreator-git

This repo depdens on next Qt repository: **ppa:beineri/opt-qt561-trusty**

To install next steps is required (Trusty):
```bash
sudo apt-add-repository ppa:adrozdoff/llvm-backport (i386)
sudo apt-add-repository ppa:adrozdoff/llvm-backport-x64 (x86_64)
sudo apt-add-repository ppa:beineri/opt-qt561-trusty
sudo apt-add-repository ppa:adrozdoff/qtcreator-git
sudo apt-get update
sudo apt-get install qtcreator-git qtcreator-git-plugin-cmake2
```

Xenial:
```bash
sudo apt-add-repository ppa:beineri/opt-qt561-xenial
sudo apt-add-repository ppa:adrozdoff/qtcreator-git
sudo apt-get update
sudo apt-get install qtcreator-git qtcreator-git-plugin-cmake2
```

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
