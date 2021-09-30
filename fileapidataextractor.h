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

#include "cmakebuildtarget.h"
#include "cmakeprojectnodes.h"

#include <projectexplorer/rawprojectpart.h>

#include <utils/fileutils.h>
#include <utils/optional.h>

#include <QList>
#include <QSet>
#include <QString>

#include <memory>

namespace CMakeProjectManager {
namespace Internal {

class FileApiData;

class CMakeFileInfo
{
public:
    bool operator==(const CMakeFileInfo& other) const { return path == other.path; }

    Utils::FilePath path;
    bool isCMake = false;
    bool isCMakeListsDotTxt = false;
    bool isExternal = false;
    bool isGenerated = false;
};

inline uint qHash(const CMakeFileInfo &info, uint seed = 0) { return info.path.hash(seed); }

class FileApiQtcData
{
public:
    QString errorMessage;
    CMakeConfig cache;
    QSet<CMakeFileInfo> cmakeFiles;
    QList<CMakeBuildTarget> buildTargets;
    ProjectExplorer::RawProjectParts projectParts;
    std::unique_ptr<CMakeProjectNode> rootProjectNode;
    QSet<Utils::FilePath> knownHeaders;
    QString ctestPath;
    bool isMultiConfig = false;
    bool usesAllCapsTargets = false;
};

FileApiQtcData extractData(FileApiData &data,
                           const Utils::FilePath &sourceDirectory,
                           const Utils::FilePath &buildDirectory);
FileApiQtcData generateFallbackData(const Utils::FilePath &topCmakeFile,
                                    const Utils::FilePath &sourceDirectory,
                                    const Utils::FilePath &buildDirectory,
                                    QString errorMessage);

} // namespace Internal
} // namespace CMakeProjectManager
