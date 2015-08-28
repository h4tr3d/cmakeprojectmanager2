#ifndef CMAKELISTSNODE_H
#define CMAKELISTSNODE_H

#include <QtCore/QVariantMap>

namespace CMakeProjectManager {
namespace Internal {

class CMakeListsNode;
class CMakeListsArgument {
public:
    enum Delimiter {
        UNQUOTED,
        QUOTED,
        BRACKET
    };

    explicit CMakeListsArgument():
        data(0), Delim(UNQUOTED), Line(0) {}

    explicit CMakeListsArgument(const QString &v, Delimiter d, CMakeListsNode *parent, long line) :
        data(parent), Value(v), Delim(d), Line(line) {}

    CMakeListsNode *data;
    QString Value;
    Delimiter Delim;
    long Line;

};

class CMakeListsFunction {
public:
    explicit CMakeListsFunction() :
        data(0), line(0) {}

    explicit CMakeListsFunction(CMakeListsNode *parent, const QByteArray &tokenName, long lineNumber) :
        data(parent), name(tokenName), line(lineNumber) {}

    QStringList getArguments() const;

    CMakeListsNode *data;
    QByteArray name;
    long line;
    QList<CMakeListsArgument> Arguments;

};

class CMakeListsParserError {
public:
    explicit CMakeListsParserError() :
        m_line(-1), m_warning(false) {}

    inline bool hasError()
    { return !(m_msg.isEmpty() || m_warning); }

    QString m_msg;
    long m_line;
    bool m_warning;

};

typedef struct cmListFileLexer_s cmListFileLexer;
typedef struct cmListFileLexer_Token_s cmListFileLexer_Token;
class CMakeListsNode {
public:
    enum Separation {
        SeparationOkay,
        SeparationWarning,
        SeparationError
    };

    explicit CMakeListsNode(const QString &nodeProjectDir, CMakeListsNode *nodeParent = 0);
    virtual ~CMakeListsNode();

    bool parseFile();
    bool parseFunction(const QByteArray &name, long line);
    bool addArgument(cmListFileLexer_Token *token, CMakeListsArgument::Delimiter delim);

    inline void setError(const QString &msg)
    { setError(-1, msg); }
    inline void setError(long line, const QString &msg)
    { error.m_line = line; error.m_msg = msg; }

    QVariant resolveVar(const QString &var);
    QVariant resolveVars(const QVariant &var);
    void insertToCache(const QString &var, const QVariant &value);
    void addTarget(const QString &target, const QStringList &value);
    void addSubdirectory(const QString &sourceDir);
    void addIncludeDirectories(const QStringList &l);
    void addDefines(const QByteArray &defs);
    void setProjectName(const QString &name);


    bool skip() const;
    void insertSkipCondition(const QString &expression, bool evaluatedResult);
    void removeSkipCondition(const QString &expression);

    cmListFileLexer *lexer;
    QList<CMakeListsFunction> functions;
    CMakeListsFunction function;
    CMakeListsParserError error;
    QVariantMap properties;
    QMap<QString, QStringList> targets;
    CMakeListsNode *parent;
    QList<QPair<QString, bool> > skipConditions;
    QString projectDir;
    QString projectName;
    QStringList includeDirs;
    QByteArray defines;
    QList<CMakeListsNode *> childNodes;
    Separation separation;

};

} //namespace Internal
} //namespace CMakeProjectManager

#endif // CMAKELISTSNODE_H
