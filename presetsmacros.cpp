// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "presetsmacros.h"
#include "presetsparser.h"

#include <utils/environment.h>
#include <utils/filepath.h>
#include <utils/hostosinfo.h>

#include <QRegularExpression>

namespace CMakeProjectManager::Internal::CMakePresets::Macros {

static void expandAllButEnv(const PresetsDetails::ConfigurePreset &configurePreset,
                            const Utils::FilePath &sourceDirectory,
                            QString &value)
{
    value.replace("${dollar}", "$");

    value.replace("${sourceDir}", sourceDirectory.toString());
    value.replace("${sourceParentDir}", sourceDirectory.parentDir().toString());
    value.replace("${sourceDirName}", sourceDirectory.fileName());

    value.replace("${presetName}", configurePreset.name);
    if (configurePreset.generator)
        value.replace("${generator}", configurePreset.generator.value());
}

void expand(const PresetsDetails::ConfigurePreset &configurePreset,
            Utils::Environment &env,
            const Utils::FilePath &sourceDirectory)
{
    const QHash<QString, QString> presetEnv = configurePreset.environment
                                                  ? configurePreset.environment.value()
                                                  : QHash<QString, QString>();
    for (auto it = presetEnv.constKeyValueBegin(); it != presetEnv.constKeyValueEnd(); ++it) {
        const QString key = it->first;
        QString value = it->second;

        expandAllButEnv(configurePreset, sourceDirectory, value);

        QRegularExpression envRegex(R"((\$env\{(\w+)\}))");
        for (const QRegularExpressionMatch &match : envRegex.globalMatch(value)) {
            if (match.captured(2) != key)
                value.replace(match.captured(1), presetEnv.value(match.captured(2)));
            else
                value.replace(match.captured(1), "");
        }

        QString sep;
        bool append = true;
        if (key.compare("PATH", Qt::CaseInsensitive) == 0) {
            sep = Utils::OsSpecificAspects::pathListSeparator(env.osType());
            const int index = value.indexOf("$penv{PATH}", 0, Qt::CaseInsensitive);
            if (index != 0)
                append = false;
            value.replace("$penv{PATH}", "", Qt::CaseInsensitive);
        }

        QRegularExpression penvRegex(R"((\$penv\{(\w+)\}))");
        for (const QRegularExpressionMatch &match : penvRegex.globalMatch(value))
            value.replace(match.captured(1), env.value(match.captured(2)));

        if (append)
            env.appendOrSet(key, value, sep);
        else
            env.prependOrSet(key, value, sep);
    }
}

void expand(const PresetsDetails::ConfigurePreset &configurePreset,
            Utils::EnvironmentItems &envItems,
            const Utils::FilePath &sourceDirectory)
{
    const QHash<QString, QString> presetEnv = configurePreset.environment
                                                  ? configurePreset.environment.value()
                                                  : QHash<QString, QString>();

    for (auto it = presetEnv.constKeyValueBegin(); it != presetEnv.constKeyValueEnd(); ++it) {
        const QString key = it->first;
        QString value = it->second;

        expandAllButEnv(configurePreset, sourceDirectory, value);

        QRegularExpression envRegex(R"((\$env\{(\w+)\}))");
        for (const QRegularExpressionMatch &match : envRegex.globalMatch(value)) {
            if (match.captured(2) != key)
                value.replace(match.captured(1), presetEnv.value(match.captured(2)));
            else
                value.replace(match.captured(1), "");
        }

        auto operation = Utils::EnvironmentItem::Operation::SetEnabled;
        if (key.compare("PATH", Qt::CaseInsensitive) == 0) {
            operation = Utils::EnvironmentItem::Operation::Append;
            const int index = value.indexOf("$penv{PATH}", 0, Qt::CaseInsensitive);
            if (index != 0)
                operation = Utils::EnvironmentItem::Operation::Prepend;
            value.replace("$penv{PATH}", "", Qt::CaseInsensitive);
        }

        QRegularExpression penvRegex(R"((\$penv\{(\w+)\}))");
        for (const QRegularExpressionMatch &match : penvRegex.globalMatch(value))
            value.replace(match.captured(1), QString("${%1}").arg(match.captured(2)));

        envItems.emplace_back(Utils::EnvironmentItem(key, value, operation));
    }
}

void expand(const PresetsDetails::ConfigurePreset &configurePreset,
            const Utils::Environment &env,
            const Utils::FilePath &sourceDirectory,
            QString &value)
{
    expandAllButEnv(configurePreset, sourceDirectory, value);

    const QHash<QString, QString> presetEnv = configurePreset.environment
                                                  ? configurePreset.environment.value()
                                                  : QHash<QString, QString>();

    QRegularExpression envRegex(R"((\$env\{(\w+)\}))");
    for (const QRegularExpressionMatch &match : envRegex.globalMatch(value))
        value.replace(match.captured(1), presetEnv.value(match.captured(2)));

    QRegularExpression penvRegex(R"((\$penv\{(\w+)\}))");
    for (const QRegularExpressionMatch &match : penvRegex.globalMatch(value))
        value.replace(match.captured(1), env.value(match.captured(2)));
}

} // namespace CMakeProjectManager::Internal::CMakePresets::Macros
