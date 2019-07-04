#pragma once

#include "projectexplorer/projectnodes.h"

#include "servermodereader.h"

#include <QObject>

namespace CMakeProjectManager {
namespace Internal {

class SimpleServerModeReader : public ServerModeReader
{
    Q_OBJECT

public:

    std::unique_ptr<CMakeProjectNode> generateProjectTree(
        const QList<const ProjectExplorer::FileNode *> &allFiles, QString &errorMessage) final;

protected:
    QList<std::tuple<Utils::FileName, ProjectExplorer::FileType, bool>> m_filesCache;
    QString m_topLevelNameCache;
};

} // namespace Internal
} // namespace CMakeProjectManager


