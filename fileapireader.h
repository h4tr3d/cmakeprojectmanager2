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

#include "builddirreader.h"
#include "fileapiparser.h"
#include "cmakeprocess.h"

#include <utils/optional.h>

#include <QFuture>

#include <memory>

namespace ProjectExplorer {
class ProjectNode;
}

namespace CMakeProjectManager {
namespace Internal {

class FileApiQtcData;

class FileApiReader final : public BuildDirReader
{
    Q_OBJECT

public:
    FileApiReader();
    ~FileApiReader() final;

    void setParameters(const BuildDirParameters &p) final;

    bool isCompatible(const BuildDirParameters &p) final;
    void resetData() final;
    void parse(bool forceCMakeRun, bool forceConfiguration) final;
    void stop() final;

    bool isParsing() const final;

    QSet<Utils::FilePath> projectFilesToWatch() const final;
    QList<CMakeBuildTarget> takeBuildTargets(QString &errorMessage) final;
    CMakeConfig takeParsedConfiguration(QString &errorMessage) final;
    std::unique_ptr<CMakeProjectNode> generateProjectTree(
        const QList<const ProjectExplorer::FileNode *> &allFiles, QString &errorMessage) final;
    ProjectExplorer::RawProjectParts createRawProjectParts(QString &errorMessage) final;

private:
    void startState();
    void endState(const QFileInfo &replyFi);
    void startCMakeState(const QStringList &configurationArguments);
    void cmakeFinishedState(int code, QProcess::ExitStatus status);

    std::unique_ptr<CMakeProcess> m_cmakeProcess;

    // cmake data:
    CMakeConfig m_cache;
    QSet<Utils::FilePath> m_cmakeFiles;
    QList<CMakeBuildTarget> m_buildTargets;
    ProjectExplorer::RawProjectParts m_projectParts;
    std::unique_ptr<CMakeProjectNode> m_rootProjectNode;
    QSet<Utils::FilePath> m_knownHeaders;

    Utils::optional<QFuture<FileApiQtcData *>> m_future;

    // Update related:
    bool m_isParsing = false;

    std::unique_ptr<FileApiParser> m_fileApi;
};

} // namespace Internal
} // namespace CMakeProjectManager
