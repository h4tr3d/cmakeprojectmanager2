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
        const QList<const ProjectExplorer::FileNode *> &allFiles, QString &errorMessage) final;

private:
    
};

} // namespace Internal
} // namespace CMakeProjectManager
