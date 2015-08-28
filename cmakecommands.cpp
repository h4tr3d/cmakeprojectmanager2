#include "cmakecommands.h"

using namespace CMakeProjectManager;
using namespace CMakeProjectManager::Internal;

CommandAbstract *CMakeCommandsFactory::create(const QByteArray &name)
{
    if (name == QByteArrayLiteral("set"))
        return new Set(name);
    if (name == QByteArrayLiteral("if"))
        return new IfThenElseEndIf(name);
    if (name == QByteArrayLiteral("endif"))
        return new IfThenElseEndIf(name);
    if (name == QByteArrayLiteral("elseif"))
        return new IfThenElseEndIf(name);
    if (name == QByteArrayLiteral("else"))
        return new IfThenElseEndIf(name);
    if (name == QByteArrayLiteral("add_subdirectory"))
        return new AddSubdirectory(name);
    if (name == QByteArrayLiteral("include_directories"))
        return new IncludeDirectories(name);
    if (name == QByteArrayLiteral("add_library"))
        return new AddLibrary(name);
    if (name == QByteArrayLiteral("add_custom_target"))
        return new AddCustomTarget(name);
    if (name == QByteArrayLiteral("add_executable"))
        return new AddExecutable(name);
    if (name == QByteArrayLiteral("project"))
        return new Project(name);
    if (name == QByteArrayLiteral("cmake_minimum_required"))
        return new CmakeMinimumRequired(name);
    if (name == QByteArrayLiteral("add_definitions"))
        return new AddDefinitions(name);

    return 0;
}

Set::Set(const QByteArray &name) :
    CommandAbstract(name)
{
    CommandSyntax cs;
    cs.items.append(CommandItem("var"));
    cs.items.append(CommandItem("CACHE", CommandItem::FLAG, false));
    cs.items.append(CommandItem("TYPE", CommandItem::FLAG, false));
    cs.items.append(CommandItem("list", CommandItem::LIST));
    cs.items.append(CommandItem("CACHE", CommandItem::FLAG, false));
    cs.items.append(CommandItem("FILEPATH", CommandItem::FLAG, false));
    cs.items.append(CommandItem("PATH", CommandItem::FLAG, false));
    cs.items.append(CommandItem("STRING", CommandItem::FLAG, false));
    cs.items.append(CommandItem("FILEPATH", CommandItem::FLAG, false));
    cs.items.append(CommandItem("BOOL", CommandItem::FLAG, false));
    cs.items.append(CommandItem("INTERNAL", CommandItem::FLAG, false));
    cs.items.append(CommandItem("docString", CommandItem::VALUE, false ));
    cs.items.append(CommandItem("FORCE", CommandItem::FLAG, false));
    cs.items.append(CommandItem("PARENT_SCOPE", CommandItem::FLAG, false));

    appendSyntax(cs);
}

bool Set::postProcess()
{
    insertToCache(var(), list());
    return true;
}

IncludeDirectories::IncludeDirectories(const QByteArray &name) :
    CommandAbstract(name)
{
    CommandSyntax cs;
    cs.items.append(CommandItem("AFTER", CommandItem::FLAG, false));
    cs.items.append(CommandItem("BEFORE", CommandItem::FLAG, false));
    cs.items.append(CommandItem("SYSTEM", CommandItem::FLAG, false));
    cs.items.append(CommandItem("list", CommandItem::LIST));
    appendSyntax(cs);
}

bool IncludeDirectories::postProcess()
{
    addIncludeDirectories(list());
    return true;
}

AddExecutable::AddExecutable(const QByteArray &name) :
    CommandAbstract(name)
{
    CommandSyntax cs;
    cs.items.append(CommandItem("name"));
    cs.items.append(CommandItem("WIN32", CommandItem::FLAG, false));
    cs.items.append(CommandItem("MACOSX_BUNDLE", CommandItem::FLAG, false));
    cs.items.append(CommandItem("EXCLUDE_FROM_ALL", CommandItem::FLAG, false));
    cs.items.append(CommandItem("list", CommandItem::LIST, false));
    cs.items.append(CommandItem("IMPORTED", CommandItem::FLAG, false));

    appendSyntax(cs);
}

bool AddExecutable::postProcess()
{
    if (!excludeFromAll())
        appendTarget(name(), list());
    return true;
}

AddSubdirectory::AddSubdirectory(const QByteArray &name) :
    CommandAbstract(name)
{
    CommandSyntax cs;
    cs.items.append(CommandItem("sourceDir"));
    cs.items.append(CommandItem("binaryDir", CommandItem::VALUE, false));
    cs.items.append(CommandItem("EXCLUDE_FROM_ALL", CommandItem::FLAG, false));
    appendSyntax(cs);
}

bool AddSubdirectory::postProcess()
{
    if (!excludeFromAll())
        addSubdirectory(sourceDir());
    return true;
}

Project::Project(const QByteArray &name) :
    CommandAbstract(name)
{
    CommandSyntax cs;
    cs.items.append(CommandItem("projectName"));
    cs.items.append(CommandItem("params", CommandItem::LIST, false));
    appendSyntax(cs);
}

bool Project::postProcess()
{
    setProjectName(projectName());
    return true;
}

CmakeMinimumRequired::CmakeMinimumRequired(const QByteArray &name) :
    CommandAbstract(name)
{
    CommandSyntax cs;
    cs.items.append(CommandItem("VERSION", CommandItem::FLAG));
    cs.items.append(CommandItem("value", CommandItem::VALUE));
    cs.items.append(CommandItem("FATAL_ERROR", CommandItem::FLAG, false));
    appendSyntax(cs);
}

bool CmakeMinimumRequired::postProcess()
{
    QString current = QStringLiteral("3.0.0");

    QStringList l = version().split(QLatin1Char('.'), QString::SkipEmptyParts);
    QStringList r = current.split(QLatin1Char('.'), QString::SkipEmptyParts);

    for (int i = 0; i < qMax(l.count(), r.count()); ++i) {
        int ll = l.value(i).toInt();
        int rr = r.value(i).toInt();
        if (ll != rr) {
            return ll < rr;
        }
    }
    return true;
}

AddLibrary::AddLibrary(const QByteArray &name) :
    CommandAbstract(name)
{
    CommandSyntax cs;
    cs.items.append(CommandItem("name", CommandItem::VALUE));
    cs.items.append(CommandItem("STATIC", CommandItem::FLAG, false));
    cs.items.append(CommandItem("SHARED", CommandItem::FLAG, false));
    cs.items.append(CommandItem("MODULE", CommandItem::FLAG, false));
    cs.items.append(CommandItem("EXCLUDE_FROM_ALL", CommandItem::FLAG, false));
    cs.items.append(CommandItem("list", CommandItem::LIST, false));
    appendSyntax(cs);
}

bool AddLibrary::postProcess()
{
    if (!excludeFromAll())
        appendTarget(name(), list());
    return true;
}


AddCustomTarget::AddCustomTarget(const QByteArray &name) :
    CommandAbstract(name)
{
    CommandSyntax cs;
    cs.items.append(CommandItem("name", CommandItem::VALUE));
    cs.items.append(CommandItem("ALL", CommandItem::FLAG, false));
    cs.items.append(CommandItem("COMMAND", CommandItem::FLAG, false));
    cs.items.append(CommandItem("DEPENDS", CommandItem::FLAG, false));
    cs.items.append(CommandItem("WORKING_DIRECTORY", CommandItem::FLAG, false));
    cs.items.append(CommandItem("COMMENT", CommandItem::FLAG, false));
    cs.items.append(CommandItem("VERBATIM", CommandItem::FLAG, false));
    cs.items.append(CommandItem("SOURCES", CommandItem::FLAG, false));
    cs.items.append(CommandItem("list", CommandItem::LIST, false));
    appendSyntax(cs);
}

bool AddCustomTarget::postProcess()
{
    QStringList l = value("list").toStringList();
    const int count = l.count();
    QStringList res;
    res.reserve(count);
    for (int i = 0; i < count; ++i) {
        if (!l.at(i).endsWith(QLatin1String("CMakeLists.txt")))
            res.append(l.at(i));
    }
    appendTarget(name(), res);
    return true;
}

IfThenElseEndIf::IfThenElseEndIf(const QByteArray &name) :
    CommandAbstract(name)
{
    CommandSyntax cs;
    cs.items.append(CommandItem("expression", CommandItem::LIST, false));
    appendSyntax(cs);
}

bool IfThenElseEndIf::skip()
{
    return false;
}

bool IfThenElseEndIf::postProcess()
{
    const QString expr = expression().join(QLatin1Char(' '));
    if (commandName() != QByteArrayLiteral("if")) {
        removeSkipCondition(expr);
    }
    if (commandName() != QByteArrayLiteral("endif")) {
        //! Always skip as it is not evaluated
        if (commandName() == QByteArrayLiteral("else"))
            insertSkipCondition(QLatin1String("NOT ") + expr, true);
        else
            insertSkipCondition(expr, true);
    }

    return true;
}


AddDefinitions::AddDefinitions(const QByteArray &name) :
    CommandAbstract(name)
{
    CommandSyntax cs;
    cs.items.append(CommandItem("definitions", CommandItem::LIST));
    appendSyntax(cs);
}

bool AddDefinitions::postProcess()
{
    // /D -D ..?
    addDefinitions(definitions());
    return true;
}
