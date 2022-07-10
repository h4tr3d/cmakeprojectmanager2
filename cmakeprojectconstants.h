/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#pragma once

namespace CMakeProjectManager {
namespace Constants {

const char CMAKE_MIMETYPE[] = "text/x-cmake";
const char CMAKE_PROJECT_MIMETYPE[] = "text/x-cmake-project";
const char CMAKE_EDITOR_ID[] = "CMakeProject.CMakeEditor";
const char RUN_CMAKE[] = "CMakeProject.RunCMake";
const char CLEAR_CMAKE_CACHE[] = "CMakeProject.ClearCache";
const char RESCAN_PROJECT[] = "CMakeProject.RescanProject";
const char RUN_CMAKE_CONTEXT_MENU[] = "CMakeProject.RunCMakeContextMenu";
const char BUILD_FILE_CONTEXT_MENU[] = "CMakeProject.BuildFileContextMenu";
const char BUILD_FILE[] = "CMakeProject.BuildFile";
const char CMAKE_HOME_DIR[] = "CMakeProject.HomeDirectory";
const char QML_DEBUG_SETTING[] = "CMakeProject.EnableQmlDebugging";

// Project
const char CMAKE_PROJECT_ID[] = "CMakeProjectManager.CMakeProject";

const char CMAKE_BUILDCONFIGURATION_ID[] = "CMakeProjectManager.CMakeBuildConfiguration";

// Menu
const char M_CONTEXT[] = "CMakeEditor.ContextMenu";

namespace Settings {
const char GENERAL_ID[] = "CMakeSpecifcSettings";
const char TOOLS_ID[] = "K.CMake.Tools";
const char CATEGORY[] = "K.CMake";
} // namespace Settings

// Snippets
const char CMAKE_SNIPPETS_GROUP_ID[] = "CMake";

namespace Icons {
const char FILE_OVERLAY[] = ":/cmakeproject/images/fileoverlay_cmake.png";
const char LOGO_BW[] = ":/cmakeproject/images/logo_cmake_bw.png";
const char LOGO_BW_2X[] = ":/cmakeproject/images/logo_cmake_bw@2x.png";
} // namespace Icons

// Actions
const char BUILD_TARGET_CONTEXT_MENU[] = "CMake.BuildTargetContextMenu";

// Build Step
const char CMAKE_BUILD_STEP_ID[] = "CMakeProjectManager.MakeStep";

// Features
const char CMAKE_FEATURE_ID[] = "CMakeProjectManager.Wizard.FeatureCMake";

// Tool
const char TOOL_ID[] = "CMakeProjectManager.CMakeKitInformation";


} // namespace Constants
} // namespace CMakeProjectManager
