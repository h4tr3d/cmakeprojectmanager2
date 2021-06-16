#pragma once

#include "projectexplorer/projectnodes.h"
#include "fileapireader.h"

#include <QObject>

namespace CMakeProjectManager {
namespace Internal {

class SimpleFileApiReader : public FileApiReader
{
    Q_OBJECT

public:
    std::unique_ptr<CMakeProjectNode> generateProjectTree(
        const QList<const ProjectExplorer::FileNode *> &allFiles,
        QString &errorMessage,
        bool includeHeaderNodes) final;
    ProjectExplorer::RawProjectParts createRawProjectParts(QString &errorMessage) final;

protected:
    void endState(const Utils::FilePath &replyFilePath) final;

private:
    QList<std::tuple<Utils::FilePath, ProjectExplorer::FileType, bool>> m_filesCache;
    QString m_topLevelNameCache;
};

} // namespace Internal
} // namespace CMakeProjectManager
