#include <QtCore/QList>
#include <QtCore/QString>

#include "cmakelistsnode.h"
#include "commandabstract.h"

using namespace CMakeProjectManager;
using namespace CMakeProjectManager::Internal;

CommandAbstract::CommandAbstract(const QByteArray &name) :
    m_data(0),
    m_commandName(name)
{
}

CommandAbstract::~CommandAbstract()
{
}

void CommandAbstract::setFunction(const CMakeListsFunction &fn)
{
    m_data = fn.data;
    m_args = fn.getArguments();
}

bool CommandAbstract::checkSyntax()
{
    Q_ASSERT(m_data);
    const int count = m_syntax.count();
    for (int i = 0; i < count; ++i) {
        if (checkSyntaxItem(m_syntax.at(i)))
            return postProcess();
        m_result.clear();
    }
    return false;
}

bool CommandAbstract::checkSyntaxItem(const CommandSyntax &cs)
{
    const int argsCount = m_args.count();
    if (argsCount < cs.minArgs)
        return false;
    if (argsCount == 0)
        return true;

    QByteArray listName;

    const int count = cs.items.count();
    int argsStartPosition = 0;
    int argsEndPosition = argsCount - 1;

    for (int i = count - 1 ; i >= 0 ; --i) {
        if (argsEndPosition < 0)
            return false;
        QString argumentValue = m_args.at(argsEndPosition);

        const CommandItem &item = cs.items.at(i);

        if (item.type == CommandItem::LIST) {
            listName = item.name;
            break;
        }

        if (item.required) {
            if (item.type == CommandItem::FLAG) {
                if (argumentValue.toLatin1() != item.name)
                    return false;

                insertResult(item.name, true);
            } else if (item.type == CommandItem::VALUE) {
                insertResult(item.name, argumentValue);
            }

            --argsEndPosition;
            continue;
        }

        if (item.type == CommandItem::FLAG && argumentValue.toLatin1() == item.name) {
            insertResult(item.name, true);
            --argsEndPosition;
            continue;
        }
    }

    if (argsEndPosition < 0)
        return true;

    for (int i = 0; i < count; ++i) {
        if (argsCount <= argsStartPosition)
            return true;
        QString argumentValue = m_args.at(argsStartPosition);

        const CommandItem &item = cs.items.at(i);

        if (item.type == CommandItem::LIST) {
            break;
        }

        if (item.required) {
            if (item.type == CommandItem::FLAG) {
                if (argumentValue.toLatin1() != item.name)
                    return false;

                insertResult(item.name, true);
            } else if (item.type == CommandItem::VALUE) {
                insertResult(item.name, argumentValue);
            }

            ++argsStartPosition;
            continue;
        }

        if (item.type == CommandItem::FLAG && argumentValue.toLatin1() == item.name) {
            insertResult(item.name, true);
            ++argsStartPosition;
            continue;
        }
    }

    if (!listName.isEmpty()) {
        QStringList list;
        for (int i = argsStartPosition; i <= argsEndPosition; ++i) {
            list.append(m_args.at(i));
        }
        insertResult(listName, list);
        return true;
    }

    return false;
}

void CommandAbstract::appendSyntax(CommandSyntax cs)
{
    cs.minArgs = 0;
    bool alreadyHasList = false;
    QList<CommandItem>::ConstIterator it, end = cs.items.cend();
    for (it = cs.items.cbegin(); it != end; ++it) {
        if (it->required)
            ++cs.minArgs;

        if (it->type == CommandItem::LIST) {
            Q_ASSERT(!alreadyHasList);
            alreadyHasList = true;
        }
    }
    m_syntax.append(cs);
}

void CommandAbstract::insertResult(const QByteArray &key, const QVariant &value)
{
    m_result.insert(key, resolveVars(value));
}

QVariant CommandAbstract::resolveVars(const QVariant &var)
{
    return m_data->resolveVars(var);
}

void CommandAbstract::insertToCache(const QString &var, const QVariant &value)
{
    m_data->insertToCache(var, value);
}

void CommandAbstract::appendTarget(const QString &target, const QStringList &value)
{
    m_data->addTarget(target, value);
}

void CommandAbstract::addSubdirectory(const QString &sourceDir)
{
    m_data->addSubdirectory(sourceDir);
}

void CommandAbstract::setProjectName(const QString &projectName)
{
    m_data->setProjectName(projectName);
}

void CommandAbstract::insertSkipCondition(const QString &expression, bool evaluatedResult)
{
    m_data->insertSkipCondition(expression, evaluatedResult);
}

void CommandAbstract::removeSkipCondition(const QString &expression)
{
    m_data->removeSkipCondition(expression);
}

void CommandAbstract::addIncludeDirectories(const QStringList &l)
{
    m_data->addIncludeDirectories(l);
}

void CommandAbstract::addDefinitions(const QByteArray &def)
{
    m_data->addDefines(def);
}

bool CommandAbstract::skip()
{
    return m_data->skip();
}
