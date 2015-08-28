#ifndef COMMANDABSTRACT_H
#define COMMANDABSTRACT_H

#include <QtCore/QMap>
#include <QtCore/QVariant>

namespace CMakeProjectManager {
namespace Internal {

//! CommandSyntaxItem?
class CommandItem {
public:
    enum Type {
        VALUE = 0,
        FLAG,
        LIST
    };

    explicit CommandItem(const QByteArray &cmdName, Type cmdType = VALUE, bool isRequired = true) :
        name(cmdName),
        type(cmdType),
        required(isRequired)
    {}

    QByteArray name;
    Type type;
    bool required;

};

class CommandSyntax {
public:
    QList<CommandItem> items;
    int minArgs;

};

class CMakeListsNode;
class CMakeListsFunction;
class CommandAbstract {
public:
    explicit CommandAbstract(const QByteArray &name);
    virtual ~CommandAbstract();

    inline QByteArray commandName() const
    { return m_commandName; }

    inline QVariant value(const QByteArray &key, const QVariant &defaultValue = QVariant()) const
    { return m_result.value(key, defaultValue); }
    inline bool contains(const QByteArray &key) const
    { return m_result.contains(key); }

    void setFunction(const CMakeListsFunction &fn);
    bool checkSyntax();
    virtual bool skip();

protected:
    virtual bool postProcess() = 0;

    bool checkSyntaxItem(const CommandSyntax &cs);
    void appendSyntax(CommandSyntax cs);
    void insertResult(const QByteArray &key, const QVariant &value);

    QVariant resolveVars(const QVariant &var);
    void insertToCache(const QString &var, const QVariant &value);
    void appendTarget(const QString &target, const QStringList &value);
    void addSubdirectory(const QString &sourceDir);
    void setProjectName(const QString &projectName);
    void insertSkipCondition(const QString &expression, bool evaluatedResult);
    void removeSkipCondition(const QString &expression);
    void addIncludeDirectories(const QStringList &l);
    void addDefinitions(const QByteArray &def);

protected:
    CMakeListsNode *m_data;
    QStringList m_args;
    QByteArray m_commandName;
    QMap<QByteArray, QVariant> m_result;
    QList<CommandSyntax> m_syntax;

};

} //namespace Internal
} //namespace CMakeProjectManager

#endif // COMMANDABSTRACT_H
