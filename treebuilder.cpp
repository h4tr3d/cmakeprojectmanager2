/****************************************************************************
**
** Copyright (C) 2016 Alexander Drozdov.
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
#include "treebuilder.h"

#include <coreplugin/fileiconprovider.h>
#include <coreplugin/icore.h>
#include <projectexplorer/projectexplorerconstants.h>

#include <utils/runextensions.h>
#include <utils/algorithm.h>
#include <utils/mimetypes/mimedatabase.h>

#include <QDir>
#include <QDebug>

using namespace ProjectExplorer;

namespace CMakeProjectManager {

namespace {

// TODO This code taken from projectnodes.cpp and it marked as HACK. Wait for more clean solution.
FileType getFileType(const QString &file)
{
    using namespace ProjectExplorer;

    Utils::MimeDatabase mdb;
    const Utils::MimeType mt = mdb.mimeTypeForFile(file);
    if (!mt.isValid())
        return UnknownFileType;

    const QString typeName = mt.name();
    if (typeName == QLatin1String(ProjectExplorer::Constants::CPP_SOURCE_MIMETYPE)
        || typeName == QLatin1String(ProjectExplorer::Constants::C_SOURCE_MIMETYPE))
        return SourceType;
    if (typeName == QLatin1String(ProjectExplorer::Constants::CPP_HEADER_MIMETYPE)
        || typeName == QLatin1String(ProjectExplorer::Constants::C_HEADER_MIMETYPE))
        return HeaderType;
    if (typeName == QLatin1String(ProjectExplorer::Constants::RESOURCE_MIMETYPE))
        return ResourceType;
    if (typeName == QLatin1String(ProjectExplorer::Constants::FORM_MIMETYPE))
        return FormType;
    if (typeName == QLatin1String(ProjectExplorer::Constants::QML_MIMETYPE))
        return QMLType;
    return UnknownFileType;
}
} // ::anonymous

TreeBuilder::TreeBuilder(QObject *parent)
    : QObject(parent)
{
    connect(&m_watcher, &QFutureWatcherBase::finished,
            this, &TreeBuilder::buildTreeFinished);
}

void TreeBuilder::startParsing(const Utils::FileName &baseDir)
{
    m_watcher.cancel();
    m_watcher.waitForFinished();

    m_baseDir = baseDir;
    m_filesForFuture.clear();
    m_pathsForFuture.clear();

    m_watcher.setFuture(Utils::runAsync(&TreeBuilder::run, this));
}

void TreeBuilder::run(QFutureInterface<void> &fi)
{
    m_futureCount = 0;
    fi.setProgressRange(0, 10);

    buildTree(m_baseDir, fi, 5);

    fi.setProgressValue(4);

    // Sort and prepare

    // Step 1: sort collections
    Utils::sort(m_filesForFuture);
    if (fi.isCanceled())  {
        fi.setProgressValue(10);
        return;
    }
    fi.setProgressValue(6);

    Utils::sort(m_pathsForFuture);
    if (fi.isCanceled())  {
        fi.setProgressValue(10);
        return;
    }
    fi.setProgressValue(8);

    // Step 2: remove dups
    m_paths = Utils::filteredUnique(m_pathsForFuture);
    fi.setProgressValue(10);
}

void TreeBuilder::buildTreeFinished()
{
    m_files = std::move(m_filesForFuture);
    m_paths = std::move(m_pathsForFuture);

    m_filesForFuture.clear();
    m_pathsForFuture.clear();

    emit parsingFinished();
}

void TreeBuilder::cancel()
{
    m_watcher.cancel();
    m_watcher.waitForFinished();
}

void TreeBuilder::wait()
{
    m_watcher.waitForFinished();
}

bool TreeBuilder::isParsing() const
{
    return m_watcher.isStarted() || m_watcher.isRunning();
}

QFuture<void> TreeBuilder::future() const
{
    return m_watcher.future();
}

void TreeBuilder::buildTree(const Utils::FileName &baseDir,
                            QFutureInterface<void> &fi,
                            int symlinkDepth)
{
    if (symlinkDepth == 0)
        return;

    const QFileInfoList fileInfoList = QDir(baseDir.toString()).entryInfoList(QDir::Files |
                                                                              QDir::Dirs |
                                                                              QDir::NoDotAndDotDot |
                                                                              QDir::NoSymLinks);
    foreach (const QFileInfo &fileInfo, fileInfoList) {
        Utils::FileName fn = Utils::FileName(fileInfo);
        if (m_futureCount % 100) {
            emit parsingProgress(fn);
        }

        if (fi.isCanceled())
            return;

        ++m_futureCount;
        if (fileInfo.isDir() && isValidDir(fileInfo)) {
            m_pathsForFuture.append(fn);
            buildTree(fn, fi, symlinkDepth - fileInfo.isSymLink());
        } else if (isValidFile(fileInfo)) {
            m_filesForFuture.append(fn);
        }
    }
}

TreeBuilder::~TreeBuilder()
{
    cancel();
}

Utils::FileNameList TreeBuilder::files() const
{
    return m_files;
}

Utils::FileNameList TreeBuilder::paths() const
{
    return m_paths;
}

QList<FileNode *> TreeBuilder::fileNodes() const
{
    return Utils::transform(m_files, [](const Utils::FileName &fileName) {
        ProjectExplorer::FileNode *node = 0;
        bool generated = false;
        QString onlyFileName = fileName.fileName();
        if (   (onlyFileName.startsWith(QLatin1String("moc_")) && onlyFileName.endsWith(QLatin1String(".cxx")))
               || (onlyFileName.startsWith(QLatin1String("ui_")) && onlyFileName.endsWith(QLatin1String(".h")))
               || (onlyFileName.startsWith(QLatin1String("qrc_")) && onlyFileName.endsWith(QLatin1String(".cxx"))))
            generated = true;

        if (fileName.endsWith(QLatin1String("CMakeLists.txt")))
            node = new ProjectExplorer::FileNode(fileName, ProjectExplorer::ProjectFileType, false);
        else {
            ProjectExplorer::FileType fileType = getFileType(fileName.fileName());
            node = new ProjectExplorer::FileNode(fileName, fileType, generated);
        }

        return node;
    });
}

bool TreeBuilder::isValidDir(const QFileInfo &fileInfo)
{
    const QString fileName = fileInfo.fileName();
    const QString suffix = fileInfo.suffix();

    if (fileName.startsWith(QLatin1Char('.')))
        return false;

    else if (fileName == QLatin1String("CVS"))
        return false;

    // TODO ### user include/exclude

    return true;
}

bool TreeBuilder::isValidFile(const QFileInfo &fileInfo)
{
    return !fileInfo.fileName().endsWith(QLatin1String("CMakeLists.txt.user"));
}

#if 0
QList<Glob> TreeBuilder::parseFilter(const QString &filter)
{
    QList<Glob> result;
    QStringList list = filter.split(QLatin1Char(';'), QString::SkipEmptyParts);
    foreach (const QString &e, list) {
        QString entry = e.trimmed();
        Glob g;
        if (entry.indexOf(QLatin1Char('*')) == -1 && entry.indexOf(QLatin1Char('?')) == -1) {
            g.mode = Glob::EXACT;
            g.matchString = entry;
        } else if (entry.startsWith(QLatin1Char('*')) && entry.indexOf(QLatin1Char('*'), 1) == -1
                   && entry.indexOf(QLatin1Char('?'), 1) == -1) {
            g.mode = Glob::ENDSWITH;
            g.matchString = entry.mid(1);
        } else {
            g.mode = Glob::REGEXP;
            g.matchRegexp = QRegExp(entry, Qt::CaseInsensitive, QRegExp::Wildcard);
        }
        result.append(g);
    }
    return result;
}


void TreeBuilder::applyFilter(const QString &showFilesfilter, const QString &hideFilesfilter)
{
    QList<Glob> filter = parseFilter(showFilesfilter);
    m_showFilesFilter = filter;

    filter = parseFilter(hideFilesfilter);
    m_hideFilesFilter = filter;
}
#endif

} // namespace CMakeProjectManager
