/****************************************************************************
**
** Copyright (C) 2015 Alexander Drozdov.
** Contact: adrozdoff@gmail.com
**
** This file is part of CMakeProjectManager2 plugin.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
****************************************************************************/

#pragma once

#include <QDialog>

namespace CMakeProjectManager {

class CMakeInlineEditorWidgetPrivate;

class CMakeInlineEditorWidget : public QWidget
{
    Q_OBJECT
public:
    CMakeInlineEditorWidget(QWidget *parent = 0);
    ~CMakeInlineEditorWidget();

    void setContent(const QString &content);
    QString content() const;

private:
    CMakeInlineEditorWidgetPrivate *d;
};


class CMakeInlineEditorDialogPrivate;

class CMakeInlineEditorDialog : public QDialog
{
    Q_OBJECT
public:
    CMakeInlineEditorDialog(QWidget *parent = 0);
    ~CMakeInlineEditorDialog();

    void setContent(const QString &content);
    QString content() const;

    static QString getContent(QWidget *parent, const QString &initial, bool *ok = 0);

private:
    CMakeInlineEditorDialogPrivate *d;
};

} // namespace CMakeProjectManager

