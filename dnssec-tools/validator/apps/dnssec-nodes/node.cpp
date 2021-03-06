/****************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Nokia Corporation and its Subsidiary(-ies) nor
**     the names of its contributors may be used to endorse or promote
**     products derived from this software without specific prior written
**     permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QMessageBox>
#include <QStyleOption>
#include <qdebug.h>

#include "edge.h"
#include "node.h"
#include "graphwidget.h"
#include "DetailsViewer.h"

Node::Node(GraphWidget *graphWidget, const QString &nodeName, const QString &fqdn, int depth)
    : m_parent(0), graph(graphWidget), m_nodeName(nodeName.toLower()), m_fqdn(fqdn.toLower()), m_depth(depth),
      m_subData(), m_accessCount(0), m_accessTime(0), m_resultCache(0), m_colorAlpha(255)
{
    setFlag(ItemIsMovable);
    setFlag(ItemSendsGeometryChanges);
    setCacheMode(DeviceCoordinateCache);
    setZValue(-1);
    setToolTip(fqdn);
}

void Node::addEdge(Edge *edge)
{
    edgeList << edge;
    edge->adjust();
}

QSet<Edge *> Node::edges() const
{
    return edgeList;
}

void Node::removeEdge(Edge *edge)
{
    edgeList.remove(edge);
}

void Node::calculateForces()
{
    if (!scene() || scene()->mouseGrabberItem() == this) {
        newPos = pos();
        return;
    }

    // Sum up all forces pushing this item away
    qreal xvel = 0;
    qreal yvel = 0;
    foreach (QGraphicsItem *item, scene()->items()) {
        Node *node = qgraphicsitem_cast<Node *>(item);
        if (!node)
            continue;

        QPointF vec = mapToItem(node, 0, 0);
        qreal dx = vec.x();
        qreal dy = vec.y();
        double l = 2.0 * (dx * dx + dy * dy);
        if (l > 0) {
            xvel += (dx * 150.0) / l;
            yvel += (dy * 150.0) / l;
        }
    }

    // Now subtract all forces pulling items together
    double weight = (edgeList.size() + 1) * graph->nodeScale();
    foreach (Edge *edge, edgeList) {
        QPointF vec;
        if (edge->sourceNode() == this)
            vec = mapToItem(edge->destNode(), 0, 0);
        else
            vec = mapToItem(edge->sourceNode(), 0, 0);
        xvel -= vec.x() / weight;
        yvel -= vec.y() / weight;
    }

    if (qAbs(xvel) < 0.1 && qAbs(yvel) < 0.1)
        xvel = yvel = 0;

    QRectF sceneRect = scene()->sceneRect();
    newPos = pos() + QPointF(xvel, yvel);
    newPos.setX(qMin(qMax(newPos.x(), sceneRect.left() + 10), sceneRect.right() - 10));
    newPos.setY(qMin(qMax(newPos.y(), sceneRect.top() + 10), sceneRect.bottom() - 10));
}

void Node::setNewPos(QPointF pos) {
    if (graph->isLocked())
        setPos(pos);
    else
        newPos = pos;
}

bool Node::advance()
{
    if (newPos == pos())
        return false;

    if (scene() && scene()->mouseGrabberItem() == this)
        return false;

    if (graph->layoutType() != GraphWidget::springyLayout) {
        // migrate half way to the new position
        QPointF distance = (newPos - pos()) / 2;
        QPointF moveTo = pos() + distance;
        if ((newPos - moveTo).manhattanLength() < 5)
            moveTo = newPos;
        setPos(moveTo);
    } else {
        setPos(newPos);
    }
    return true;
}

QRectF Node::boundingRect() const
{
#if defined(Q_OS_SYMBIAN) || defined(Q_WS_MAEMO_5)
    // Add some extra space around the circle for easier touching with finger
    qreal adjust = 30;
    return QRectF( -10 - adjust, -10 - adjust,
                  20 + adjust * 2, 20 + adjust * 2);
#else
    qreal adjust = 2;
    return QRectF( -10 - adjust, -10 - adjust,
                  23 + adjust, 23 + adjust);
#endif
}

QPainterPath Node::shape() const
{
    QPainterPath path;
#if defined(Q_OS_SYMBIAN) || defined(Q_WS_MAEMO_5)
    // Add some extra space around the circle for easier touching with finger
    path.addEllipse( -40, -40, 80, 80);
#else
    path.addEllipse(-10, -10, 20, 20);
#endif
    return path;
}

QColor Node::getColorForStatus(int status) {
    QColor color;

    if (status & DNSData::FAILED)
        color = Qt::red;
    else if ((status & (DNSData::DNE | DNSData::VALIDATED)) == (DNSData::DNE | DNSData::VALIDATED))
        color = Qt::blue;
    else if (status & DNSData::DNE)
        color = Qt::cyan;
    // note: TRUSTED trumps VALIDATED because some log messages mistakenly
    // indicate validated when the entire chain is not.  Thus if any status
    // indicates it was TRUSTED, then something somewhere wasn't complete.
    else if (status & DNSData::TRUSTED)
        color = Qt::yellow;
    else if (status & DNSData::VALIDATED)
        color = Qt::green;
    else if (status & DNSData::IGNORE)
        color = Qt::magenta;
    else
        color = QColor(128,128,128);

    color.setAlpha(m_colorAlpha);

    return color;
}

void Node::setupPainting(int status, const QStyleOptionGraphicsItem *option, QPainter *painter) {
    QColor color = getColorForStatus(status);

    QRadialGradient gradient(-3, -3, 10);
    if (option && option->state & QStyle::State_Sunken) {
        gradient.setCenter(3, 3);
        gradient.setFocalPoint(3, 3);
        gradient.setColorAt(1, QColor(color).light(120));
        gradient.setColorAt(0, QColor(Qt::white).light(120));
    } else {
        gradient.setColorAt(0, QColor(Qt::white));
        gradient.setColorAt(1, QColor(color));
    }

    painter->setBrush(gradient);
    painter->setPen(QPen(Qt::black, 0));
}

void Node::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    painter->setPen(Qt::NoPen);
    painter->setBrush(Qt::darkGray);
    painter->drawEllipse(-7, -7, 20, 20);

    if (m_subData.count() > 1) {
        int angleSegment = 360 * 16 / m_subData.count();
        int count = 0;

        foreach (DNSData data, m_subData) {
            setupPainting(data.DNSSECStatus(), option, painter);

            painter->drawPie(QRectF(-7, -7, 20, 20), count * angleSegment, angleSegment);

            count++;
        }
    } else {
        setupPainting(m_resultCache, option, painter);

        painter->drawEllipse(-10, -10, 20, 20);
    }


    if (m_nodeName.length() > 0) {
        QFont font = painter->font();
        font.setPointSize(6);
        painter->setFont(font);
        painter->drawText(QRectF(-10, -5, 20, 20), m_nodeName);
    }
}

QVariant Node::itemChange(GraphicsItemChange change, const QVariant &value)
{
    switch (change) {
    case ItemPositionHasChanged:
        foreach (Edge *edge, edgeList)
            edge->adjust();
        graph->itemMoved();
        break;
    default:
        break;
    };

    return QGraphicsItem::itemChange(change, value);
}

void Node::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    update();
    if (event->button() == Qt::MidButton) {
        // middle button moves
        event->setButton(Qt::LeftButton);
        QGraphicsItem::mousePressEvent(event);
    } else if (event->button() == Qt::RightButton) {
        DetailsViewer lv(this);
        lv.exec();
    } else {
        // everything else selects
        graph->setInfo(this);
    }
}

void Node::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    update();
    QGraphicsItem::mouseReleaseEvent(event);
}

void Node::addChild(Node *child)
{
    m_children.insert(child);
}

QSet<Node *> Node::children() {
    return m_children;
}

bool Node::hasChildren() {
    return ! m_children.isEmpty();
}

void Node::removeChild(Node *child) {
    m_children.remove(child);
}

void Node::addParent(Node *parent)
{
    m_parent = parent;
}

Node *Node::parent() {
    return m_parent;
}

void Node::addLogMessage(const QString logMessage)
{
    m_logMessages.push_back(logMessage);
}

QStringList Node::logMessages()
{
    return m_logMessages;
}

void Node::addSubData(const DNSData &data)
{
    if (!m_subData.contains(data.recordType())) {
        m_subData.insert(data.recordType(), data);
    } else {
        // merge in the other data with ours (internally this drops UNKNOWNS
        m_subData[data.recordType()].addDNSSECStatus(data.DNSSECStatus());
    }
    cacheDNSDataValidity();
}

QString Node::getSubData()
{
    QString description;
    foreach(DNSData data, m_subData) {
        description += data.recordType() + "(" + QString::number(int(data.DNSSECStatus())) + "), ";
    }
    return description;
}

void Node::cacheDNSDataValidity()
{
    m_resultCache = 0;
    foreach(DNSData data, m_subData) {
        m_resultCache |= data.DNSSECStatus();
    }
}

int Node::DNSSECValidity()
{
    return m_resultCache;
}

void Node::setAlpha(int alpha) {
    m_colorAlpha = alpha;
}

QMap<QString, DNSData>Node::getAllSubData()
{
    return m_subData;
}

bool Node::subDataExistsFor(QString type)
{
    if (m_subData.contains(type))
        return true;

    return false;
}
