// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <extensionsystem/iplugin.h>

namespace ProjectExplorer { class Node; }

namespace CMakeProjectManager::Internal {

class CMakeProjectPlugin final : public ExtensionSystem::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "CMakeProjectManager.json")

public:
    ~CMakeProjectPlugin();

#ifdef WITH_TESTS
private slots:
    void testCMakeParser_data();
    void testCMakeParser();

    void testCMakeSplitValue_data();
    void testCMakeSplitValue();

    void testCMakeProjectImporterQt_data();
    void testCMakeProjectImporterQt();

    void testCMakeProjectImporterToolChain_data();
    void testCMakeProjectImporterToolChain();
#endif

private:
    void initialize() final;
    void extensionsInitialized() final;

    void updateContextActions(ProjectExplorer::Node *node);

    class CMakeProjectPluginPrivate *d = nullptr;
};

} // CMakeProjectManager::Internal
