#pragma once

#include "servermodereader.h"

#include <QObject>

namespace CMakeProjectManager {
namespace Internal {

class SimpleServerModeReader : public ServerModeReader
{
    Q_OBJECT

public:

    void generateProjectTree(CMakeProjectNode *root,
                             const QList<const ProjectExplorer::FileNode *> &allFiles) final;

};

} // namespace Internal
} // namespace CMakeProjectManager


