#ifndef CMAKELISTSPARSER_H
#define CMAKELISTSPARSER_H

#include <QtCore/QString>

namespace CMakeProjectManager {
namespace Internal {

class CMakeListsParserPrivate;
class CMakeListsParser {
public:
    explicit CMakeListsParser();
    ~CMakeListsParser();

    bool parseProject(const QString &projectPath);

    QList<ProjectExplorer::FileNode *> projectFiles() const;
    QStringList targets() const;
    QString projectName() const;
    QStringList includeDirectories() const;
    QByteArray defines() const;

    bool hasError() const;
    QString errorString() const;

private:
    CMakeListsParserPrivate *d;

};

} //namespace Internal
} //namespace CMakeProjectManager

#endif // CMAKELISTSPARSER_H
