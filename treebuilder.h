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
#pragma once

#include <projectexplorer/projectnodes.h>

#include <QObject>
#include <QList>
#include <QFutureWatcher>

namespace CMakeProjectManager {

class TreeBuilder : public QObject
{
    Q_OBJECT
public:
    explicit TreeBuilder(QObject *parent = 0);
    ~TreeBuilder() override;

    Utils::FileNameList files() const;
    Utils::FileNameList paths() const;

    QList<ProjectExplorer::FileNode*> fileNodes() const;

    void startScanning(const Utils::FileName &baseDir);
    void cancel();
    void wait();

    bool isScanning() const;

    QFuture<void> future() const;

#if 0
    void applyFilter(const QString &selectFilesfilter, const QString &hideFilesfilter);
#endif

signals:
    void scanningFinished();
    void scanningProgress(const Utils::FileName &fileName);

private:
    void buildTreeFinished();
#if 0
    QList<ProjectExplorer::Glob> parseFilter(const QString &filter);
#endif

    void run(QFutureInterface<void> &fi);
    void buildTree(const Utils::FileName &baseDir, QFutureInterface<void> &fi, int symlinkDepth);

    bool isValidDir(const QFileInfo &fileInfo);
    bool isValidFile(const QFileInfo &fileInfo);

private:
    // Used in the future thread need to all not used after calling startParsing
    Utils::FileName m_baseDir;

    Utils::FileNameList m_files;
    Utils::FileNameList m_filesForFuture;

    Utils::FileNameList m_paths;
    Utils::FileNameList m_pathsForFuture;

    QFutureWatcher<void> m_watcher;
    int m_futureCount = 0;

    bool m_parsing = false;

#if 0
    QList<ProjectExplorer::Glob> m_hideFilesFilter;
    QList<ProjectExplorer::Glob> m_showFilesFilter;
#endif
};

} // namespace CMakeProjectManager

