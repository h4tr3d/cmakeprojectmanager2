/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#pragma once

#include "cmaketoolchaininfo.h"

#include <projectexplorer/namedwidget.h>

#include <utils/progressindicator.h>
#include <utils/fancylineedit.h>

#include <QTimer>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QLabel;
class QPushButton;
class QTreeView;
class QSortFilterProxyModel;
class QGroupBox;
class QRadioButton;
QT_END_NAMESPACE

namespace CMakeProjectManager {

class ConfigModel;

namespace Internal {

class CMakeBuildConfiguration;

class CMakeBuildSettingsWidget : public ProjectExplorer::NamedWidget
{
    Q_OBJECT
public:
    CMakeBuildSettingsWidget(CMakeBuildConfiguration *bc);

    void setError(const QString &message);

private:
    void updateButtonState();
    void updateAdvancedCheckBox();

    CMakeToolchainInfo currentToolchainInfo() const;
    void toolchainFileSelect();
    void toolchainEdit();
    void toolchainRadio(bool toggled);

    CMakeBuildConfiguration *m_buildConfiguration;
    QTreeView *m_configView;
    ConfigModel *m_configModel;
    QSortFilterProxyModel *m_configFilterModel;
    Utils::ProgressIndicator *m_progressIndicator;
    QPushButton *m_editButton;
    QPushButton *m_resetButton;
    QCheckBox *m_showAdvancedCheckBox;
    QPushButton *m_reconfigureButton;
    QGroupBox *m_toolchainGroupBox;
    Utils::FancyLineEdit *m_toolchainLineEdit;
    QPushButton *m_toolchainFileSelectPushButton;
    QPushButton *m_toolchainEditPushButton;
    QRadioButton *m_fileToolchainRadioButton;
    QRadioButton *m_inlineToolchainRadioButton;
    QCheckBox    *m_forceClearCache;
    QTimer m_showProgressTimer;
    QLabel *m_errorLabel;
    QLabel *m_errorMessageLabel;

    QString m_toolchainInlineCurrent;
};

} // namespace Internal
} // namespace CMakeProjectManager
