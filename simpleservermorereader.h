#pragma once

#include "servermodereader.h"

#include <QObject>

namespace CMakeProjectManager {
namespace Internal {

class SimpleServerMoreReader : public ServerModeReader
{
    Q_OBJECT

public:

    void generateProjectTree(CMakeListsNode *root,
                             const QList<const ProjectExplorer::FileNode *> &allFiles) final;

};

} // namespace Internal
} // namespace CMakeProjectManager


