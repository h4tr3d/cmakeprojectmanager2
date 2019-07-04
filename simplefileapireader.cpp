#include "simplefileapireader.h"

#include "cmakebuildconfiguration.h"
#include "cmakeprojectconstants.h"
#include "cmakeprojectmanager.h"
#include "fileapidataextractor.h"
#include "projecttreehelper.h"

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/fileiconprovider.h>
#include <coreplugin/messagemanager.h>
#include <coreplugin/progressmanager/progressmanager.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/task.h>
#include <projectexplorer/taskhub.h>
#include <projectexplorer/toolchain.h>

#include <utils/algorithm.h>
#include <utils/optional.h>
#include <utils/qtcassert.h>
#include <utils/runextensions.h>

#include <QDateTime>
#include <QLoggingCategory>

using namespace ProjectExplorer;
using namespace Utils;

namespace CMakeProjectManager {
namespace Internal {

// --------------------------------------------------------------------
// SimpleFileApiReader:
// --------------------------------------------------------------------


std::unique_ptr<CMakeProjectNode> SimpleFileApiReader::generateProjectTree(
    const QList<const FileNode *> &allFiles, QString &errorMessage)
{
    Q_UNUSED(errorMessage)

    addHeaderNodes(m_rootProjectNode.get(), m_knownHeaders, allFiles);
    return std::move(m_rootProjectNode);
}

} // namespace Internal
} // namespace CMakeProjectManager
