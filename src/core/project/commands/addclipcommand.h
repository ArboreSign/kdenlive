/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef ADDCLIPCOMMAND_H
#define ADDCLIPCOMMAND_H

#include <QUndoCommand>
#include <KUrl>
#include <mlt++/Mlt.h>

class AbstractClipPlugin;
class ProjectFolder;
class AbstractProjectClip;



/**
 * @class AddClipCommand
 * @brief Handles creating and adding a clip to the project.
 * 
 * WARNING: when the parentItem is recreated between redo and undo calls we will crash
 * -> instead of a pointer store a "index"-Tree which allows us to get the current pointer to the parentItem
 */

class AddClipCommand : public QUndoCommand
{
public:
    /**
     * @brief Constructor.
     * @param url url to the file which should be added
     * @param plugin plugin handling the clip type of the file
     * @param parentItem folder to which the clip should be added to
     */
    explicit AddClipCommand(const KUrl &url, const QString &id, const AbstractClipPlugin *plugin, ProjectFolder *parentItem, bool addClip, QUndoCommand* parent = 0);
    explicit AddClipCommand(const QString &displayName, const QString &service, Mlt::Properties &properties, const QString &id, const AbstractClipPlugin *plugin, ProjectFolder *parentItem, bool addClip, QUndoCommand* parent = 0);

    void undo();
    void redo();
    void addClip();
    void deleteClip();

private:
    KUrl m_url;
    QString m_displayName;
    QString m_service;
    Mlt::Properties m_properties;
    QString m_id;
    const AbstractClipPlugin *m_plugin;
    AbstractProjectClip *m_clip;
    ProjectFolder *m_parentItem;
    bool m_addClip;
};

#endif