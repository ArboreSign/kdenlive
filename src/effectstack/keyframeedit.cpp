/***************************************************************************
                          keyframedit.cpp  -  description
                             -------------------
    begin                : 03 Aug 2008
    copyright            : (C) 2008 by Marco Gittler
    email                : g.marco@freenet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "keyframeedit.h"
#include "positionedit.h"
#include "widgets/doubleparameterwidget.h"
#include "kdenlivesettings.h"
#include "utils/KoIconUtils.h"

#include "klocalizedstring.h"
#include <QDebug>
#include <QFontDatabase>
#include <QHeaderView>

KeyframeEdit::KeyframeEdit(const QDomElement &e, int minFrame, int maxFrame, const Timecode &tc, int activeKeyframe, QWidget* parent) :
    QWidget(parent),
    m_min(minFrame),
    m_max(maxFrame),
    m_timecode(tc),
    m_keyframesTag(false)
{
    setupUi(this);
    if (m_max == -1) {
        // special case: keyframe for tracks, do not allow keyframes
        widgetTable->setHidden(true);
    }
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    keyframe_list->setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    buttonSeek->setChecked(KdenliveSettings::keyframeseek());
    connect(buttonSeek, SIGNAL(toggled(bool)), this, SLOT(slotSetSeeking(bool)));

    buttonKeyframes->setIcon(KoIconUtils::themedIcon(QStringLiteral("chronometer")));
    button_add->setIcon(KoIconUtils::themedIcon(QStringLiteral("list-add")));
    button_add->setToolTip(i18n("Add keyframe"));
    button_delete->setIcon(KoIconUtils::themedIcon(QStringLiteral("list-remove")));
    button_delete->setToolTip(i18n("Delete keyframe"));
    buttonResetKeyframe->setIcon(KoIconUtils::themedIcon(QStringLiteral("edit-undo")));
    buttonSeek->setIcon(KoIconUtils::themedIcon(QStringLiteral("edit-link")));
    connect(keyframe_list, &QTableWidget::currentCellChanged, this, &KeyframeEdit::rowClicked);
    connect(keyframe_list, SIGNAL(cellChanged(int,int)), this, SLOT(slotGenerateParams(int,int)));

    m_position = new PositionEdit(i18n("Position"), 0, 0, 1, tc, widgetTable);
    ((QGridLayout*)widgetTable->layout())->addWidget(m_position, 3, 0, 1, -1);

    m_slidersLayout = new QGridLayout(param_sliders);
    //m_slidersLayout->setSpacing(0);
    
    m_slidersLayout->setContentsMargins(0, 0, 0, 0);
    m_slidersLayout->setVerticalSpacing(2);
    keyframe_list->setSelectionBehavior(QAbstractItemView::SelectRows);
    keyframe_list->setSelectionMode(QAbstractItemView::SingleSelection);
    addParameter(e, activeKeyframe);
    keyframe_list->resizeRowsToContents();

    //keyframe_list->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
    connect(button_delete, SIGNAL(clicked()), this, SLOT(slotDeleteKeyframe()));
    connect(button_add, SIGNAL(clicked()), this, SLOT(slotAddKeyframe()));
    connect(buttonKeyframes, SIGNAL(clicked()), this, SLOT(slotKeyframeMode()));
    connect(buttonResetKeyframe, SIGNAL(clicked()), this, SLOT(slotResetKeyframe()));
    connect(m_position, SIGNAL(parameterChanged(int)), this, SLOT(slotAdjustKeyframePos(int)));

    //connect(keyframe_list, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), this, SLOT(slotSaveCurrentParam(QTreeWidgetItem*,int)));

    if (!keyframe_list->currentItem()) {
        keyframe_list->setCurrentCell(0, 0);
        keyframe_list->selectRow(0);
    }
    // ensure the keyframe list shows at least 3 lines
    keyframe_list->setMinimumHeight(QFontInfo(keyframe_list->font()).pixelSize() * 9);

    // Do not show keyframe table if only one keyframe exists at the beginning
    if (keyframe_list->rowCount() < 2 && getPos(0) == m_min && m_max != -1)
        widgetTable->setHidden(true);
    else
        buttonKeyframes->setHidden(true);
}

KeyframeEdit::~KeyframeEdit()
{
    setEnabled(false);
    keyframe_list->blockSignals(true);
    keyframe_list->clear();
    QLayoutItem *child;
    while ((child = m_slidersLayout->takeAt(0)) != 0) {
        QWidget *wid = child->widget();
        delete child;
        if (wid)
            delete wid;
    }
}

void KeyframeEdit::addParameter(const QDomElement &e, int activeKeyframe)
{
    keyframe_list->blockSignals(true);
    m_params.append(e.cloneNode().toElement());

    QDomElement na = e.firstChildElement(QStringLiteral("name"));
    QString paramName = i18n(na.text().toUtf8().data());
    QDomElement commentElem = e.firstChildElement(QStringLiteral("comment"));
    QString comment;
    if (!commentElem.isNull())
        comment = i18n(commentElem.text().toUtf8().data());

    int columnId = keyframe_list->columnCount();
    keyframe_list->insertColumn(columnId);
    keyframe_list->setHorizontalHeaderItem(columnId, new QTableWidgetItem(paramName));

    DoubleParameterWidget *doubleparam = new DoubleParameterWidget(paramName, 0,
                                                                   m_params.at(columnId).attribute(QStringLiteral("min")).toDouble(), m_params.at(columnId).attribute(QStringLiteral("max")).toDouble(),
                                                                   m_params.at(columnId).attribute(QStringLiteral("default")).toDouble(), comment, columnId, m_params.at(columnId).attribute(QStringLiteral("suffix")), m_params.at(columnId).attribute(QStringLiteral("decimals")).toInt(), false, this);
    connect(doubleparam, SIGNAL(valueChanged(double)), this, SLOT(slotAdjustKeyframeValue(double)));
    connect(this, SIGNAL(showComments(bool)), doubleparam, SLOT(slotShowComment(bool)));
    connect(doubleparam, SIGNAL(setInTimeline(int)), this, SLOT(slotUpdateVisibleParameter(int)));
    m_slidersLayout->addWidget(doubleparam, columnId, 0);
    if (e.attribute(QStringLiteral("intimeline")) == QLatin1String("1")) {
        doubleparam->setInTimelineProperty(true);
    }

    QStringList frames;
    if (e.hasAttribute(QStringLiteral("keyframes"))) {
        // Effects have keyframes in a "keyframe" attribute, not sure why
        frames = e.attribute(QStringLiteral("keyframes")).split(';', QString::SkipEmptyParts);
        m_keyframesTag = true;
    } else {
        // Transitions take keyframes from the value param
        QString framesValue = e.attribute(QStringLiteral("value"));
        if (!framesValue.contains(QStringLiteral("="))) {
            framesValue.prepend(QStringLiteral("0="));
        }
        frames = framesValue.split(';', QString::SkipEmptyParts);
        m_keyframesTag = false;
    }
    for (int i = 0; i < frames.count(); ++i) {
        int frame = frames.at(i).section('=', 0, 0).toInt();
        bool found = false;
        int j;
        for (j = 0; j < keyframe_list->rowCount(); ++j) {
            int currentPos = getPos(j);
            if (frame == currentPos) {
                keyframe_list->setItem(j, columnId, new QTableWidgetItem(frames.at(i).section('=', 1, 1)));
                found = true;
                break;
            } else if (currentPos > frame) {
                break;
            }
        }
        if (!found) {
            keyframe_list->insertRow(j);
            keyframe_list->setVerticalHeaderItem(j, new QTableWidgetItem(getPosString(frame)));
            keyframe_list->setItem(j, columnId, new QTableWidgetItem(frames.at(i).section('=', 1, 1)));
            keyframe_list->resizeRowToContents(j);
        }
        if ((activeKeyframe > -1) && (activeKeyframe == frame)) {
            keyframe_list->setCurrentCell(i, columnId);
            keyframe_list->selectRow(i);
        }
    }
    if (!keyframe_list->currentItem()) {
        keyframe_list->setCurrentCell(0, columnId);
        keyframe_list->selectRow(0);
    }
    keyframe_list->resizeColumnsToContents();
    keyframe_list->blockSignals(false);
    keyframe_list->horizontalHeader()->setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    keyframe_list->verticalHeader()->setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    slotAdjustKeyframeInfo(false);
    button_delete->setEnabled(keyframe_list->rowCount() > 1);
}

void KeyframeEdit::slotDeleteKeyframe()
{
    if (keyframe_list->rowCount() < 2)
        return;
    int col = keyframe_list->currentColumn();
    int row = keyframe_list->currentRow();
    keyframe_list->removeRow(keyframe_list->currentRow());
    row = qMin(row, keyframe_list->rowCount() - 1);
    keyframe_list->setCurrentCell(row, col);
    keyframe_list->selectRow(row);
    generateAllParams();

    bool disable = keyframe_list->rowCount() < 2;
    button_delete->setEnabled(!disable);
    disable &= getPos(0) == m_min;
    widgetTable->setHidden(disable);
    buttonKeyframes->setHidden(!disable);
}

void KeyframeEdit::slotAddKeyframe(int pos)
{
    keyframe_list->blockSignals(true);
    int row = 0;
    int col = keyframe_list->currentColumn();
    int newrow = row;
    int result = pos;
    if (result > 0) {
        // A position was provided
        for (; newrow < keyframe_list->rowCount(); newrow++) {
            int rowPos = getPos(newrow);
            if (rowPos > result) {
                break;
            }
        }
    } else {
        row = keyframe_list->currentRow();
        newrow = row;
        int pos1 = getPos(row);
        if (row < (keyframe_list->rowCount() - 1)) {
            result = pos1 + (getPos(row + 1) - pos1) / 2;
            newrow++;
        } else if (row == 0) {
            if (pos1 == m_min) {
                result = m_max;
                newrow++;
            } else {
                result = m_min;
            }
        } else {
            if (pos1 < m_max) {
                // last keyframe selected and it is not at end of clip -> add keyframe at the end
                result = m_max;
                newrow++;
            } else {
                int pos2 = getPos(row - 1);
                result = pos2 + (pos1 - pos2) / 2;
            }
        }
    }
    // Calculate new values
    QList <double> previousValues;
    QList <double> nextValues;
    int rowCount = keyframe_list->rowCount();
    // Insert new row
    keyframe_list->insertRow(newrow);
    keyframe_list->setVerticalHeaderItem(newrow, new QTableWidgetItem(getPosString(result)));

    // If keyframe is inserted at start
    if (newrow == 0) {
        for (int i = 0; i < keyframe_list->columnCount(); ++i) {
            int newValue = keyframe_list->item(1, i)->text().toDouble();
            keyframe_list->setItem(newrow, i, new QTableWidgetItem(QString::number(newValue)));
        }
    } else if (newrow == rowCount){
        // Keyframe inserted at end
        for (int i = 0; i < keyframe_list->columnCount(); ++i) {
            int newValue = keyframe_list->item(rowCount - 1, i)->text().toDouble();
            keyframe_list->setItem(newrow, i, new QTableWidgetItem(QString::number(newValue)));
        }
    } else {
        // Interpolate
        int previousPos = getPos(newrow - 1);
        int nextPos = getPos(newrow + 1);
        if (nextPos > previousPos) {
            double factor = ((double) result - previousPos) / (nextPos - previousPos);

            for (int i = 0; i < keyframe_list->columnCount(); ++i) {
                double previousValues = keyframe_list->item(newrow - 1, i)->text().toDouble();
                double nextValues = keyframe_list->item(newrow + 1, i)->text().toDouble();
                int newValue = (int) (previousValues + (nextValues- previousValues) * factor);
                keyframe_list->setItem(newrow, i, new QTableWidgetItem(QString::number(newValue)));
            }
        }
    }
    keyframe_list->resizeRowsToContents();
    keyframe_list->setCurrentCell(newrow, col);
    slotAdjustKeyframeInfo();
    keyframe_list->blockSignals(false);
    generateAllParams();
    if (rowCount == 1) {
        // there was only one keyframe before, so now enable keyframe mode
        slotKeyframeMode();
    }
    button_delete->setEnabled(keyframe_list->rowCount() > 1);
}

void KeyframeEdit::slotGenerateParams(int row, int column)
{
    if (column == -1) {
        // position of keyframe changed
        QTableWidgetItem *item = keyframe_list->item(row, 0);
        if (item == NULL)
            return;

        int pos = getPos(row);
        if (pos <= m_min)
            pos = m_min;
        if (m_max != -1 && pos > m_max)
            pos = m_max;
        QString val = getPosString(pos);
        if (val != keyframe_list->verticalHeaderItem(row)->text())
            keyframe_list->verticalHeaderItem(row)->setText(val);

        for (int col = 0; col < keyframe_list->horizontalHeader()->count(); ++col) {
            item = keyframe_list->item(row, col);
            if (!item) continue;
            int v = item->text().toInt();
            if (v >= m_params.at(col).attribute(QStringLiteral("max")).toInt())
                item->setText(m_params.at(col).attribute(QStringLiteral("max")));
            if (v <= m_params.at(col).attribute(QStringLiteral("min")).toInt())
                item->setText(m_params.at(col).attribute(QStringLiteral("min")));
            QString keyframes;
            for (int i = 0; i < keyframe_list->rowCount(); ++i) {
                if (keyframe_list->item(i, col))
                    keyframes.append(QString::number(getPos(i)) + '=' + keyframe_list->item(i, col)->text() + ';');
            }
            m_params[col].setAttribute(getTag(), keyframes);
        }

        emit parameterChanged();
        return;

    }
    QTableWidgetItem *item = keyframe_list->item(row, column);
    if (item == NULL)
        return;

    int pos = getPos(row);
    if (pos <= m_min)
        pos = m_min;
    if (m_max != -1 && pos > m_max)
        pos = m_max;
    /*QList<QTreeWidgetItem *> duplicates = keyframe_list->findItems(val, Qt::MatchExactly, 0);
    duplicates.removeAll(item);
    if (!duplicates.isEmpty()) {
        // Trying to insert a keyframe at existing value, revert it
        val = m_timecode.getTimecodeFromFrames(m_previousPos);
    }*/
    QString val = getPosString(pos);
    if (val != keyframe_list->verticalHeaderItem(row)->text())
        keyframe_list->verticalHeaderItem(row)->setText(val);

    int v = item->text().toInt();
    if (v >= m_params.at(column).attribute(QStringLiteral("max")).toInt())
        item->setText(m_params.at(column).attribute(QStringLiteral("max")));
    if (v <= m_params.at(column).attribute(QStringLiteral("min")).toInt())
        item->setText(m_params.at(column).attribute(QStringLiteral("min")));
    slotAdjustKeyframeInfo(false);

    QString keyframes;
    for (int i = 0; i < keyframe_list->rowCount(); ++i) {
        if (keyframe_list->item(i, column))
            keyframes.append(QString::number(getPos(i)) + '=' + keyframe_list->item(i, column)->text() + ';');
    }
    m_params[column].setAttribute(getTag(), keyframes);
    emit parameterChanged();
}

const QString KeyframeEdit::getTag() const
{
    QString tag = m_keyframesTag == true ? QStringLiteral("keyframes") : QStringLiteral("value");
    return tag;
}

void KeyframeEdit::generateAllParams()
{
    for (int col = 0; col < keyframe_list->columnCount(); ++col) {
        QString keyframes;
        for (int i = 0; i < keyframe_list->rowCount(); ++i) {
            if (keyframe_list->item(i, col))
                keyframes.append(QString::number(getPos(i)) + '=' + keyframe_list->item(i, col)->text() + ';');
        }
        m_params[col].setAttribute(getTag(), keyframes);
    }
    emit parameterChanged();
}

const QString KeyframeEdit::getValue(const QString &name)
{
    for (int col = 0; col < keyframe_list->columnCount(); ++col) {
        QDomNode na = m_params.at(col).firstChildElement(QStringLiteral("name"));
        QString paramName = i18n(na.toElement().text().toUtf8().data());
        if (paramName == name)
            return m_params.at(col).attribute(getTag());
    }
    return QString();
}

void KeyframeEdit::slotAdjustKeyframeInfo(bool seek)
{
    QTableWidgetItem *item = keyframe_list->currentItem();
    if (!item)
        return;
    int min = m_min;
    int max = m_max;
    QTableWidgetItem *above = keyframe_list->item(item->row() - 1, item->column());
    QTableWidgetItem *below = keyframe_list->item(item->row() + 1, item->column());
    if (above)
        min = getPos(above->row()) + 1;
    if (below)
        max = getPos(below->row()) - 1;

    m_position->blockSignals(true);
    m_position->setRange(min, max, true);
    m_position->setPosition(getPos(item->row()));
    m_position->blockSignals(false);
    QLocale locale;

    for (int col = 0; col < keyframe_list->columnCount(); ++col) {
        DoubleParameterWidget *doubleparam = static_cast <DoubleParameterWidget*>(m_slidersLayout->itemAtPosition(col, 0)->widget());
        if (!doubleparam)
            continue;
        doubleparam->blockSignals(true);
        if (keyframe_list->item(item->row(), col)) {
            doubleparam->setValue(locale.toDouble(keyframe_list->item(item->row(), col)->text()));
        } else {
            //qDebug() << "Null pointer exception caught: http://www.kdenlive.org/mantis/view.php?id=1771";
        }
        doubleparam->blockSignals(false);
    }
    if (KdenliveSettings::keyframeseek() && seek)
        emit seekToPos(m_position->getPosition() - m_min);
}

void KeyframeEdit::slotAdjustKeyframePos(int value)
{
    QTableWidgetItem *item = keyframe_list->currentItem();
    keyframe_list->verticalHeaderItem(item->row())->setText(getPosString(value));
    slotGenerateParams(item->row(), -1);
    if (KdenliveSettings::keyframeseek())
        emit seekToPos(value - m_min);
}

void KeyframeEdit::slotAdjustKeyframeValue(double value)
{
    Q_UNUSED(value)

    QTableWidgetItem *item = keyframe_list->currentItem();
    for (int col = 0; col < keyframe_list->columnCount(); ++col) {
        DoubleParameterWidget *doubleparam = static_cast <DoubleParameterWidget*>(m_slidersLayout->itemAtPosition(col, 0)->widget());
        if (!doubleparam)
            continue;
        double val = doubleparam->getValue();
        QTableWidgetItem *nitem = keyframe_list->item(item->row(), col);
        if (nitem && nitem->text().toDouble() != val)
            nitem->setText(QString::number(val));
    }
    //keyframe_list->item(item->row() - 1, item->column());

}

int KeyframeEdit::getPos(int row)
{
    if (!keyframe_list->verticalHeaderItem(row))
        return 0;
    if (KdenliveSettings::frametimecode())
        return keyframe_list->verticalHeaderItem(row)->text().toInt();
    else
        return m_timecode.getFrameCount(keyframe_list->verticalHeaderItem(row)->text());
}

QString KeyframeEdit::getPosString(int pos)
{
    if (KdenliveSettings::frametimecode())
        return QString::number(pos);
    else
        return m_timecode.getTimecodeFromFrames(pos);
}

void KeyframeEdit::slotSetSeeking(bool seek)
{
    KdenliveSettings::setKeyframeseek(seek);
}

void KeyframeEdit::updateTimecodeFormat()
{
    for (int row = 0; row < keyframe_list->rowCount(); ++row) {
        QString pos = keyframe_list->verticalHeaderItem(row)->text();
        if (KdenliveSettings::frametimecode())
            keyframe_list->verticalHeaderItem(row)->setText(QString::number(m_timecode.getFrameCount(pos)));
        else
            keyframe_list->verticalHeaderItem(row)->setText(m_timecode.getTimecodeFromFrames(pos.toInt()));
    }

    m_position->updateTimecodeFormat();
}

void KeyframeEdit::slotKeyframeMode()
{
    widgetTable->setHidden(false);
    buttonKeyframes->setHidden(true);
    if (keyframe_list->rowCount() == 1) {
        slotAddKeyframe();
    }
}

void KeyframeEdit::slotResetKeyframe()
{
    for (int col = 0; col < keyframe_list->columnCount(); ++col) {
        DoubleParameterWidget *doubleparam = static_cast<DoubleParameterWidget*>(m_slidersLayout->itemAtPosition(col, 0)->widget());
        if (doubleparam)
            doubleparam->slotReset();
    }
}

void KeyframeEdit::slotUpdateVisibleParameter(int id, bool update)
{
    for (int i = 0; i < m_params.count(); ++i) {
        m_params[i].setAttribute(QStringLiteral("intimeline"), (i == id ? "1" : "0"));
    }
    for (int col = 0; col < keyframe_list->columnCount(); ++col) {
        DoubleParameterWidget *doubleparam = static_cast <DoubleParameterWidget*>(m_slidersLayout->itemAtPosition(col, 0)->widget());
        if (!doubleparam)
            continue;
        doubleparam->setInTimelineProperty(col == id);
        ////qDebug()<<"// PARAM: "<<col<<" Set TO: "<<(bool) (col == id);
        
    }
    if (update) emit parameterChanged();
}

bool KeyframeEdit::isVisibleParam(const QString& name)
{
    for (int col = 0; col < keyframe_list->columnCount(); ++col) {
        QDomNode na = m_params.at(col).firstChildElement(QStringLiteral("name"));
        QString paramName = i18n(na.toElement().text().toUtf8().data());
        if (paramName == name)
            return m_params.at(col).attribute(QStringLiteral("intimeline")) == QLatin1String("1");
    }
    return false;
}

void KeyframeEdit::checkVisibleParam()
{
    if (m_params.count() == 0)
        return;
    foreach(const QDomElement &elem, m_params) {
        if (elem.attribute(QStringLiteral("intimeline")) == QLatin1String("1"))
            return;
    }

    slotUpdateVisibleParameter(0);
}

void KeyframeEdit::slotUpdateRange(int inPoint, int outPoint)
{
    m_min = inPoint;
    m_max = outPoint - 1;
}

void KeyframeEdit::rowClicked(int newRow, int, int oldRow, int)
{
    if (oldRow != newRow)
        slotAdjustKeyframeInfo(true);
}
