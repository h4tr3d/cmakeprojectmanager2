#include <QtCore/QDebug>
#include <QtCore/QList>
#include <QtCore/QString>

#include <projectexplorer/projectnodes.h>

#include "cmakecommands.h"
#include "cmakelistsnode.h"
#include "cmakelistsparser.h"
#include "commandabstract.h"

using namespace CMakeProjectManager;
using namespace CMakeProjectManager::Internal;
using namespace ProjectExplorer;
using namespace Utils;

namespace CMakeProjectManager {
namespace Internal {

static const char *CMAKE_PROJECT_FILE = "CMakeLists.txt";

class CMakeListsParserPrivate {
public:
    CMakeListsNode *curNode;
    CMakeListsNode *rootNode;

    explicit CMakeListsParserPrivate() :
        curNode(0),
        rootNode(0)
    {}

    ~CMakeListsParserPrivate()
    {
        curNode = 0;
        delete rootNode;
        rootNode = 0;
    }

    bool compileData();

    QList<CMakeListsNode *> nodeList()
    {
        QList<CMakeListsNode *> result;
        QList<CMakeListsNode*> stack = rootNode->childNodes;
        result << rootNode << stack;
        while (!stack.isEmpty())
        {
            result << stack.last()->childNodes;
            stack << stack.takeLast()->childNodes;
        }
        return result;
    }

};

} //namespace Internal
} //namespace CMakeProjectManager

CMakeListsParser::CMakeListsParser() :
    d(new CMakeListsParserPrivate)
{
}

CMakeListsParser::~CMakeListsParser()
{
    delete d;
}

bool CMakeListsParser::parseProject(const QString &projectPath)
{
    d->rootNode = new CMakeListsNode(projectPath, 0);
    d->curNode = d->rootNode;
    if (!d->curNode->parseFile())
        return false;
    return d->compileData();
}

QList<ProjectExplorer::FileNode *> CMakeListsParser::projectFiles() const
{
    QList<CMakeListsNode *> nodes = d->nodeList();
    QList<ProjectExplorer::FileNode *> t;
    for (int i = 0; i < nodes.count(); ++i) {
        QStringList l;
        CMakeListsNode *node = nodes.at(i);
        const QString location = node->projectDir + QLatin1Char('/');

        QMap<QString, QStringList>::ConstIterator it, end = node->targets.cend();
        for (it = node->targets.cbegin(); it != end; ++it) {
            QStringList tmp = it.value();
            if (!tmp.isEmpty())
                l.append(tmp);
        }

        t.append(new ProjectExplorer::FileNode(FileName::fromString(location + QLatin1String(CMAKE_PROJECT_FILE)),
                                               ProjectExplorer::ProjectFileType, false));

        l.sort();
        l.removeDuplicates();
        Q_ASSERT(!l.contains(QLatin1String(CMAKE_PROJECT_FILE)));
        for (int j = 0; j < l.count(); ++j) {
            const QString &it = l.at(j);
            if (!it.isEmpty())
                t.append(new ProjectExplorer::FileNode(FileName::fromString(location + it), ProjectExplorer::SourceType,
                                                       false));
        }
    }

    return t;
}

QStringList CMakeListsParser::targets() const
{
    QList<CMakeListsNode *> nodes = d->nodeList();
    QStringList t;
    for (int i = 0; i < nodes.count(); ++i)
        t.append(nodes.at(i)->targets.keys());
    return t;
}

QString CMakeListsParser::projectName() const
{
    if (d->rootNode)
        return d->rootNode->projectName;
    return QString();
}

QStringList CMakeListsParser::includeDirectories() const
{
    QList<CMakeListsNode *> nodes = d->nodeList();
    QStringList result;
    for (int i = 0; i < nodes.count(); ++i) {
        CMakeListsNode *node = nodes.at(i);
        QStringList l = node->includeDirs;
        const QString location = node->projectDir + QLatin1Char('/');

        for (int j = 0; j < l.count(); ++j) {
            result.append(location + l.at(j));
        }
    }
    return result;
}

QByteArray CMakeListsParser::defines() const
{
    QList<CMakeListsNode *> nodes = d->nodeList();
    QByteArray result;
    for (int i = 0; i < nodes.count(); ++i) {
        if (result.isEmpty())
            result += ' ';
        result += nodes.at(i)->defines;
    }
    return result;
}

bool CMakeListsParser::hasError() const
{
    if (d->curNode)
        return d->curNode->error.hasError();
    return false;
}

QString CMakeListsParser::errorString() const
{
    if (d->curNode)
        return d->curNode->error.m_msg;
    return QString();
}

bool CMakeListsParserPrivate::compileData()
{
    QList<CMakeListsFunction>::ConstIterator it, end = curNode->functions.cend();
    for (it = curNode->functions.cbegin(); it != end; ++it) {
        int subDirsCount = curNode->childNodes.count();

        CommandAbstract *cmd = CMakeCommandsFactory::create(it->name);
        if (!cmd)
            continue;

        cmd->setFunction(*it);

        if (cmd->skip())
            continue;

        if (!cmd->checkSyntax()) {
            curNode->setError(it->line, QObject::tr("Syntax error for %1").
                              arg(QString::fromLatin1(it->name)));
            delete cmd;
            return false;
        }

        delete cmd;
        if (curNode->childNodes.count() > subDirsCount) {
            curNode = curNode->childNodes.last();
            curNode->parseFile();
            compileData();
            curNode = curNode->parent;
        }
    }
    return true;
}
