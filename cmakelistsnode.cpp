#include <QtCore/QDebug>
#include <QtCore/QFile>

#include "cmListFileLexer.h"
#include "cmakelistsnode.h"

using namespace CMakeProjectManager;
using namespace CMakeProjectManager::Internal;

CMakeListsNode::CMakeListsNode(const QString &nodeProjectDir, CMakeListsNode *nodeParent) :
    lexer(cmListFileLexer_New()),
    parent(nodeParent),
    projectDir(nodeProjectDir)
{
}

CMakeListsNode::~CMakeListsNode()
{
    cmListFileLexer_Delete(lexer);
    qDeleteAll(childNodes);
    childNodes.clear();
}

bool CMakeListsNode::parseFile()
{
    QFile f(projectDir + QLatin1String("/CMakeLists.txt"));
    if (!f.open(QIODevice::ReadOnly)) {
        setError(QLatin1String("Error can not open file "));
        return false;
    }

    QByteArray contents = f.readAll();
    f.close();
    if (!cmListFileLexer_SetString(lexer, contents.constData())) {
        setError(QLatin1String("Error can not open file "));
        return false;
    }

    // Use a simple recursive-descent parser to process the token stream.
    bool haveNewline = true;
    while (cmListFileLexer_Token *token = cmListFileLexer_Scan(lexer))
    {
        if (token->type == cmListFileLexer_Token_Space) {
        } else if (token->type == cmListFileLexer_Token_Newline) {
            haveNewline = true;
        } else if (token->type == cmListFileLexer_Token_CommentBracket) {
            haveNewline = false;
        } else if (token->type == cmListFileLexer_Token_Identifier) {
            if (haveNewline) {
                haveNewline = false;
                if (parseFunction(token->text, token->line)) {
                    functions.append(function);
                } else {
                    return false;
                }
            } else {
                setError(token->line, QLatin1String("Expected a newline"));

                return false;
            }
        } else {
            setError(token->line, QLatin1String("Expected a command name"));

            return false;
        }
    }
    return true;
}

bool CMakeListsNode::parseFunction(const QByteArray &name, long line)
{
    function = CMakeListsFunction(this, name, line);

    // Command name has already been parsed. Read the left paren.
    cmListFileLexer_Token *token;
    while ((token = cmListFileLexer_Scan(lexer)) && token->type == cmListFileLexer_Token_Space)
    {}

    if (!token) {
        //! sync code with cmCache* in order to provide right line with error instead of token
        setError(cmListFileLexer_GetCurrentLine(lexer),
                 QLatin1String("Parse error. Function missing opening \"(\"."));
        return false;
    }

    if (token->type != cmListFileLexer_Token_ParenLeft) {
        setError(token->line, QString::fromLatin1("Parse error.  Expected \"(\""));
        return false;
    }

    // Arguments.
    unsigned long lastLine;
    Q_UNUSED(lastLine)
    unsigned long parentDepth = 0;
    separation = SeparationOkay;
    while ((lastLine = cmListFileLexer_GetCurrentLine(lexer), token = cmListFileLexer_Scan(lexer))) {
        if (token->type == cmListFileLexer_Token_Space || token->type == cmListFileLexer_Token_Newline) {
            separation = SeparationOkay;
            continue;
        }

        switch (token->type) {
        case cmListFileLexer_Token_ParenLeft:
            parentDepth++; // ++parentDepth
            separation = SeparationOkay;
            if (!addArgument(token, CMakeListsArgument::UNQUOTED))
                return false;
            break;
        case cmListFileLexer_Token_ParenRight:
            if (parentDepth == 0)
                return true;

            parentDepth--;
            separation = SeparationOkay;
            if (!addArgument(token, CMakeListsArgument::UNQUOTED))
                return false;
            separation = SeparationWarning; //! Not clear for me..
            break;
        case cmListFileLexer_Token_Identifier:
        case cmListFileLexer_Token_ArgumentUnquoted:
            if (!addArgument(token, CMakeListsArgument::UNQUOTED))
                return false;
            separation = SeparationWarning;
            break;
        case cmListFileLexer_Token_ArgumentQuoted:
            if (!addArgument(token, CMakeListsArgument::QUOTED))
                return false;

            separation = SeparationWarning;
            break;
        case cmListFileLexer_Token_ArgumentBracket:
            if (!addArgument(token, CMakeListsArgument::BRACKET))
                return false;
            separation = SeparationError;
            break;
        case cmListFileLexer_Token_CommentBracket:
            separation = SeparationError;
            break;
        default:
            setError(token->line, QLatin1String("Function missing ending \")\""));
            return false;
        }
    }

    setError(token->line, QLatin1String("Function missing ending \")\". End of file reached"));

    return false;
}

bool CMakeListsNode::addArgument(cmListFileLexer_Token *token, CMakeListsArgument::Delimiter delim)
{
    CMakeListsArgument a(QString::fromLatin1(token->text), delim, this, token->line);
    function.Arguments.append(a);
    if (separation == SeparationOkay)
        return true;

    bool isError = (separation == SeparationError || delim == CMakeListsArgument::BRACKET);

    setError(token->line, QLatin1String("Argument not separated from preceding token by whitespace"));
    //! Warnings will be eaten by errors later
    error.m_warning = !isError;
    return !isError;
}

QStringList CMakeListsFunction::getArguments() const
{
    QStringList args;
    const int count = Arguments.count();
    for (int i = 0; i < count; ++i) {
        QVariant resolved = data->resolveVars(Arguments.at(i).Value);
        if (resolved.canConvert<QStringList>())
            args.append(resolved.toStringList());
        else
            args.append(resolved.toString());
    }
    return args;
}

QVariant CMakeListsNode::resolveVar(const QString &var)
{
    QString result = var;
    int pos = 0;
    while ((pos = result.indexOf(QLatin1String("${"), pos)) != -1) {
        if (pos > 0 && result.at(pos - 1) == QLatin1Char('\\')) {
            ++pos;
            continue;
        }

        int varEnd = result.indexOf(QLatin1Char('}'), pos);
        if (varEnd == -1 || varEnd - pos == 2)
            break; // error..

        varEnd -= pos;
        QString key = result.mid(pos + 2, varEnd - 2);
        QVariant value = properties.value(key);
        QString strValue;
        if (value.canConvert<QStringList>()) {
            QStringList l = value.toStringList();
            if (l.count() > 1)
                return l;
            if (l.count() == 1)
                strValue = l.at(0);
        } else
            strValue = value.toString();

        result.replace(pos, varEnd + 1, strValue);
        pos = 0;
    }

    return result;
}

//! process variable by type
QVariant CMakeListsNode::resolveVars(const QVariant &var)
{
    if (var.type() == QVariant::Bool)
        return var;

    if (var.type() == QVariant::String) {
        QString strValue = var.toString();
        if (strValue.contains(QLatin1Char(';')))
            return resolveVars(strValue.split(QLatin1Char(';'), QString::SkipEmptyParts));
        return resolveVar(strValue);
    }

    if (var.canConvert<QVariantList>()) {
        QVariantList args;
        QVariantList vList = var.toList();
        for (int i = 0; i < vList.count(); ++i) {
            QVariant v = resolveVars(vList.at(i));
            if (v.canConvert<QVariantList>()) {
                args.append(v.toList());
            } else {
                args.append(v);
            }
        }
        return args;
    }

    return var;
}

void CMakeListsNode::insertToCache(const QString &var, const QVariant &value)
{
    QString resolvedVar = resolveVar(var).toString();
    if (resolvedVar.isEmpty())
        return;

    properties.insert(resolvedVar, resolveVars(value));
}

void CMakeListsNode::addTarget(const QString &target, const QStringList &value)
{
    targets.insert(target, value);
}

void CMakeListsNode::addSubdirectory(const QString &sourceDir)
{
    childNodes.append(new CMakeListsNode(projectDir + QLatin1Char('/') + sourceDir, this));
}

void CMakeListsNode::addIncludeDirectories(const QStringList &l)
{
    includeDirs.append(l);
}

void CMakeListsNode::addDefines(const QByteArray &newDefines)
{
    if (!defines.isEmpty())
        defines += ' ';
    defines += newDefines;
}

void CMakeListsNode::setProjectName(const QString &name)
{
    projectName = name;
    if (parent != 0)
        setError(QStringLiteral("Project set not in top-level CMakeLists.txt"));
}

bool CMakeListsNode::skip() const
{
    const int count = skipConditions.count();
    for (int i = 0; i < count; ++i) {
        if (skipConditions.at(i).second)
            return true;
    }
    return false;
}

void CMakeListsNode::insertSkipCondition(const QString &expression, bool evaluatedResult)
{
    skipConditions.append(qMakePair(expression, evaluatedResult));
}

void CMakeListsNode::removeSkipCondition(const QString &expression)
{
    if (skipConditions.isEmpty()) {
        qDebug() << Q_FUNC_INFO << expression << "expression not found";
        return;
    }

    skipConditions.removeLast();
}
