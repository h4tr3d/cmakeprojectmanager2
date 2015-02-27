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

#include "cmakeinlineeditordialog.h"

#include <utils/environment.h>
#include <utils/hostosinfo.h>
#include <texteditor/snippets/snippeteditor.h>
#include <texteditor/texteditorsettings.h>

#include <texteditor/highlighterutils.h>

#include <utils/mimetypes/mimedatabase.h>

#include <QVBoxLayout>
#include <QDialogButtonBox>

namespace CMakeProjectManager {

class CMakeInlineEditorWidgetPrivate
{
public:
    TextEditor::TextEditorWidget *m_editor;
};

CMakeInlineEditorWidget::CMakeInlineEditorWidget(QWidget *parent)
    : QWidget(parent),
      d(new CMakeInlineEditorWidgetPrivate)
{
    d->m_editor = new TextEditor::SnippetEditorWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(d->m_editor);

    d->m_editor->textDocument()->setMimeType(QLatin1String("text/x-cmake"));
    d->m_editor->configureGenericHighlighter();
}

CMakeInlineEditorWidget::~CMakeInlineEditorWidget()
{
    delete d;
}

void CMakeInlineEditorWidget::setContent(const QString &content)
{
    d->m_editor->document()->setPlainText(content);
}

QString CMakeInlineEditorWidget::content() const
{
    return d->m_editor->document()->toPlainText();
}


class CMakeInlineEditorDialogPrivate
{
public:
    CMakeInlineEditorWidget *m_editor;
};

CMakeInlineEditorDialog::CMakeInlineEditorDialog(QWidget *parent)
    : QDialog(parent),
      d(new CMakeInlineEditorDialogPrivate)
{
    resize(640, 480);
    d->m_editor = new CMakeInlineEditorWidget(this);
    QDialogButtonBox *box = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    connect(box, SIGNAL(accepted()), this, SLOT(accept()));
    connect(box, SIGNAL(rejected()), this, SLOT(reject()));
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(d->m_editor);
    layout->addWidget(box);
    setWindowTitle(tr("Edit CMake Toolchain definition"));
}

CMakeInlineEditorDialog::~CMakeInlineEditorDialog()
{
    delete d;
}

void CMakeInlineEditorDialog::setContent(const QString &content)
{
    d->m_editor->setContent(content);
}

QString CMakeInlineEditorDialog::content() const
{
    return d->m_editor->content();
}

QString CMakeInlineEditorDialog::getContent(QWidget *parent, const QString &initial, bool *ok)
{
    CMakeInlineEditorDialog dlg(parent);
    dlg.setContent(initial);
    bool result = dlg.exec() == QDialog::Accepted;
    if (ok)
        *ok = result;
    if (result)
        return dlg.content();
    return QString();

}


} // namespace CMakeProjectManager

