/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "markerswidget.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QListWidget>
#include <KLocale>
#include <KToolBar>
#include <KAction>
#include <QStyle>

MarkersWidget::MarkersWidget(KToolBar *toolbar, QWidget* parent) :
    ToolPanel(parent)
    , m_toolbar(toolbar)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    m_list = new QListWidget(this);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setStyleSheet(QString("QListWidget { background-color: transparent;}"));
    layout->addWidget(m_list);
    m_addAction = new KAction(KIcon("list-add"), i18n("Add Marker"), this);
    m_removeAction = new KAction(KIcon("list-remove"), i18n("Delete Marker"), this);
    m_editAction = new KAction(KIcon("document-edit"), i18n("Edit Marker"), this);
    connect(m_addAction, SIGNAL(triggered()), this, SIGNAL(addMarker()));
    connect(m_removeAction, SIGNAL(triggered()), this, SLOT(slotRemoveMarker()));
    m_removeAction->setEnabled(false);
    m_editAction->setEnabled(false);
    
    connect(m_list, SIGNAL(currentRowChanged(int)), this, SLOT(slotActivateMarker(int)));
}

MarkersWidget::~MarkersWidget()
{
}

void MarkersWidget::fillToolBar()
{
    m_toolbar->clear();
    m_toolbar->addAction(m_addAction);
    m_toolbar->addAction(m_removeAction);
    m_toolbar->addAction(m_editAction);
}

void MarkersWidget::setProject()
{
    
}

void MarkersWidget::setMarkers(const QList <int> &markers)
{
    m_list->clear();
    for (int i = 0; i < markers.count(); ++i) {
        QListWidgetItem *m = new QListWidgetItem(i18n("Marker %1", i), m_list);
        m->setData(0, markers.at(i));
    }
}

void MarkersWidget::slotActivateMarker(int ix)
{
    QListWidgetItem *it = m_list->item(ix);
    if (it) {
        emit seek(it->data(0).toInt());
        m_removeAction->setEnabled(true);
        m_editAction->setEnabled(true);
    }
    else {
        m_removeAction->setEnabled(false);
        m_editAction->setEnabled(false);
    }
}

void MarkersWidget::slotRemoveMarker()
{
    QListWidgetItem *it = m_list->currentItem();
    if (it) {
        emit removeMarker(it->data(0).toInt());
    }
}

#include "markerswidget.moc"
