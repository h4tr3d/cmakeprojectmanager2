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
    SimpleFileApiReader();
    
    ProjectExplorer::RawProjectParts createRawProjectParts(QString &errorMessage) final;
    
    // TBD: make virtual?
    std::unique_ptr<CMakeProjectNode> rootProjectNode(const ProjectExplorer::TreeScanner::Result &allFiles, bool failedToParse);

private:
    QList<std::tuple<Utils::FilePath, ProjectExplorer::FileType, bool>> m_filesCache;
    QString m_topLevelNameCache;
};

} // namespace Internal
} // namespace CMakeProjectManager
