#pragma once

#include <utils/fileutils.h>

namespace ProjectExplorer {
class FileNode;
class FolderNode;
class ProjectNode;
}

namespace CMakeProjectManager {
namespace Compat {
namespace ProjectExplorer {

::ProjectExplorer::FileNode *recursiveFileNode(::ProjectExplorer::ProjectNode *projectNode,
                                               const Utils::FileName &file,
                                               const Utils::FileName &overrideBaseDir = Utils::FileName());

} // ::ProjectExplorer
} // ::Compat
} // ::CMakeProjectManager
