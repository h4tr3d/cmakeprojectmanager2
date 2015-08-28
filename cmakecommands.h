#ifndef CMAKECOMMANDS_H
#define CMAKECOMMANDS_H

#include "commandabstract.h"

namespace CMakeProjectManager {
namespace Internal {

class CMakeCommandsFactory {
public:
    static CommandAbstract *create(const QByteArray &name);
};

class Set : public CommandAbstract {
public:
    explicit Set(const QByteArray &name);

    QString var() const { return value("var").toString(); }
    QVariant list() const { return value("list"); }

protected:
    bool postProcess();

};

class IncludeDirectories : public CommandAbstract {
public:
    explicit IncludeDirectories(const QByteArray &name);

    QStringList list() const { return value("list").toStringList(); }

protected:
    bool postProcess();

};

class AddExecutable : public CommandAbstract {
public:
    explicit AddExecutable(const QByteArray &name);

    QString name() const { return value("name").toString(); }
    QStringList list() const { return value("list").toStringList(); }
    bool win32() const { return contains("WIN32"); }
    bool macOsXBundle() const { return contains("MACOSX_BUNDLE"); }
    bool excludeFromAll() const { return contains("EXCLUDE_FROM_ALL"); }

protected:
    bool postProcess();

};

class AddSubdirectory : public CommandAbstract {
public:
    explicit AddSubdirectory(const QByteArray &name);

    QString sourceDir() const { return value("sourceDir").toString(); }
    QString binaryDir() const { return value("binaryDir").toString(); }
    bool excludeFromAll() const { return contains("EXCLUDE_FROM_ALL"); }

protected:
    bool postProcess();

};

class Project : public CommandAbstract {
public:
    explicit Project(const QByteArray &name);

    QString projectName() const { return value("projectName").toString(); }
    QStringList params() const { return value("params").toStringList(); }

protected:
    bool postProcess();

};

class CmakeMinimumRequired : public CommandAbstract {
public:
    explicit CmakeMinimumRequired(const QByteArray &name);

    QString version() const { return value("value").toString(); }

protected:
    bool postProcess();

};

class AddLibrary : public CommandAbstract {
public:
    explicit AddLibrary(const QByteArray &name);

    QString name() const { return value("name").toString(); }
    QStringList list() const { return value("list").toStringList(); }
    bool isStatic() const { return contains("STATIC"); }
    bool isShared() const { return contains("SHARED"); }
    bool excludeFromAll() const { return contains("EXCLUDE_FROM_ALL"); }

protected:
    bool postProcess();

};

class AddCustomTarget : public CommandAbstract {
public:
    explicit AddCustomTarget(const QByteArray &name);

    QString name() const { return value("name").toString(); }

protected:
    bool postProcess();

};

// Spring, Summer, Fall, Winter... and Spring
class IfThenElseEndIf : public CommandAbstract {
public:
    explicit IfThenElseEndIf(const QByteArray &name);
    bool skip();

    QStringList expression() const { return value("expression").toStringList(); }

protected:
    bool postProcess();

};

class AddDefinitions : public CommandAbstract {
public:
    explicit AddDefinitions(const QByteArray &name);

    QByteArray definitions() const { return value("definitions").toByteArray(); }

protected:
    bool postProcess();

};

} //namespace Internal
} //namespace CMakeProjectManager

#endif // CMAKECOMMANDS_H
