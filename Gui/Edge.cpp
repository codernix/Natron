//  Natron
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
*Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012. 
*contact: immarespond at gmail dot com
*
*/

#include "Edge.h"

#include <cmath>
#include <QPainter>
#include <QGraphicsScene>

#include "Gui/NodeGui.h"
#include "Engine/Node.h"
#include "Engine/ViewerInstance.h"

const qreal pi= 3.14159265358979323846264338327950288419717;
static const qreal UNATTACHED_ARROW_LENGTH=60.;
const int graphicalContainerOffset=10; //number of offset pixels from the arrow that determine if a click is contained in the arrow or not

Edge::Edge(int inputNb_, double angle_,const boost::shared_ptr<NodeGui>& dest_, QGraphicsItem *parent)
: QGraphicsLineItem(parent)
, _isOutputEdge(false)
, inputNb(inputNb_)
, angle(angle_)
, label(NULL)
, arrowHead()
, dest(dest_)
, source()
, _defaultColor(Qt::black)
, _renderingColor(243,149,0)
, _useRenderingColor(false)
{
    assert(dest);
    setPen(QPen(Qt::black, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (inputNb != -1) {
        label = new QGraphicsTextItem(QString(dest->getNode()->getInputLabel(inputNb).c_str()),this);
        label->setDefaultTextColor(QColor(200,200,200));
    }
    setAcceptedMouseButtons(Qt::LeftButton);
    initLine();
    setFlag(QGraphicsItem::ItemStacksBehindParent);
    setZValue(0);
}

Edge::Edge(const boost::shared_ptr<NodeGui>& src,QGraphicsItem *parent)
: QGraphicsLineItem(parent)
, _isOutputEdge(true)
, inputNb(-1)
, angle(pi / 2.)
, label(NULL)
, arrowHead()
, dest()
, source(src)
, _defaultColor(Qt::black)
, _renderingColor(243,149,0)
, _useRenderingColor(false)
{
    assert(src);
    setPen(QPen(Qt::black, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    setAcceptedMouseButtons(Qt::LeftButton);
    initLine();
    setFlag(QGraphicsItem::ItemStacksBehindParent);
    setZValue(0);
}

Edge::~Edge()
{
    if (dest) {
        dest->markInputNull(this);
    }
}

void Edge::initLine()
{
    if (!source && !dest) {
        return;
    }
    
    QSize dstNodeSize;
    QSize srcNodeSize;
    if (dest) {
        dstNodeSize = NodeGui::nodeSize(dest->getNode()->isPreviewEnabled());
    }
    if (source) {
        srcNodeSize = NodeGui::nodeSize(source->getNode()->isPreviewEnabled());
    }
    
    QPointF dst;
    
    if (dest) {
        dst = mapFromItem(dest.get(),QPointF(dest->boundingRect().x(),dest->boundingRect().y())
                          + QPointF(dstNodeSize.width() / 2., dstNodeSize.height() / 2.));
    } else if (source && !dest) {
        dst = mapFromItem(source.get(),QPointF(source->boundingRect().x(),source->boundingRect().y())
                          + QPointF(srcNodeSize.width() / 2., srcNodeSize.height() + 10));
    }
    
    QPointF srcpt;
    if (source && dest) {
        /////// This is a connected edge, either input or output
        srcpt = mapFromItem(source.get(),QPointF(source->boundingRect().x(),source->boundingRect().y()))
            + QPointF(srcNodeSize.width() / 2.,srcNodeSize.height() / 2.);
        
        /////// Only input edges have a label
        if (label) {
            /*adjusting src and dst to show label at the middle of the line*/
            QPointF labelSrcpt= mapFromItem(source.get(),QPointF(source->boundingRect().x(),source->boundingRect().y()))
            + QPointF(srcNodeSize.width() / 2.,srcNodeSize.height());
            
            
            QPointF labelDst = mapFromItem(dest.get(),QPointF(dest->boundingRect().x(),dest->boundingRect().y())
                                           + QPointF(dstNodeSize.width() / 2.,0));
            double norm = sqrt(pow(labelDst.x() - labelSrcpt.x(),2) + pow(labelDst.y() - labelSrcpt.y(),2));
            if(norm > 20.){
                label->setPos((labelDst.x()+labelSrcpt.x())/2.-5.,
                              (labelDst.y()+labelSrcpt.y())/2.-10);
                label->show();
            }else{
                label->hide();
            }
        }
    
    } else if (!source && dest) {
        ///// The edge is an input edge which is unconnected
        
        srcpt = QPointF(dst.x() + (cos(angle)*UNATTACHED_ARROW_LENGTH),
          dst.y() - (sin(angle) * UNATTACHED_ARROW_LENGTH));
        
        if (label) {
            double cosinus = cos(angle);
            int yOffset = 0;
            if(cosinus < 0){
                yOffset = -40;
            }else if(cosinus >= -0.01 && cosinus <= 0.01){
                
                yOffset = +5;
            }else{
                
                yOffset = +10;
            }
            
            /*adjusting dst to show label at the middle of the line*/
            
            QPointF labelDst = mapFromItem(dest.get(),QPointF(dest->boundingRect().x(),dest->boundingRect().y())
                                           + QPointF(dstNodeSize.width() / 2.,0));
            
            label->setPos(((labelDst.x()+srcpt.x())/2.)+yOffset,(labelDst.y()+srcpt.y())/2.-20);
        }
    } else if (source && !dest) {
        ///// The edge is an output edge which is unconnected
        srcpt = mapFromItem(source.get(),QPointF(source->boundingRect().x(),source->boundingRect().y()))
        + QPointF(srcNodeSize.width() / 2.,srcNodeSize.height() / 2.);
        
        ///output edges don't have labels
    }
    
    setLine(dst.x(),dst.y(),srcpt.x(),srcpt.y());

    if (dest) {
        QPointF dstPost = mapFromItem(dest.get(),QPointF(dest->boundingRect().x(),dest->boundingRect().y()));
        QLineF edges[] = {
            QLineF(dstPost.x()+dstNodeSize.width(), // right
                   dstPost.y(),
                   dstPost.x()+dstNodeSize.width(),
                   dstPost.y()+dstNodeSize.height()),
            QLineF(dstPost.x()+dstNodeSize.width(), // bottom
                   dstPost.y()+dstNodeSize.height(),
                   dstPost.x(),
                   dstPost.y()+dstNodeSize.height()),
            QLineF(dstPost.x(),  // left
                   dstPost.y()+dstNodeSize.height(),
                   dstPost.x(),
                   dstPost.y()),
            QLineF(dstPost.x(), // top
                   dstPost.y(),
                   dstPost.x()+dstNodeSize.width(),
                   dstPost.y())};
        
        for (int i = 0; i < 4; ++i) {
            QPointF intersection;
            QLineF::IntersectType type = edges[i].intersect(line(), &intersection);
            if(type == QLineF::BoundedIntersection){
                setLine(QLineF(intersection,line().p2()));
                break;
            }
        }
    }
    
    qreal a;
    a = acos(line().dx() / line().length());
    if (line().dy() >= 0)
        a = 2*pi - a;
    qreal arrowSize = 5;
    QPointF arrowP1 = line().p1() + QPointF(sin(a + pi / 3) * arrowSize,
                                            cos(a + pi / 3) * arrowSize);
    QPointF arrowP2 = line().p1() + QPointF(sin(a + pi - pi / 3) * arrowSize,
                                            cos(a + pi - pi / 3) * arrowSize);

    arrowHead.clear();
    arrowHead << dst << arrowP1 << arrowP2;
}


QPainterPath Edge::shape() const
 {
     QPainterPath path = QGraphicsLineItem::shape();
     path.addPolygon(arrowHead);


     return path;
 }
static double dist2(const QPointF& p1,const QPointF& p2){
    return  pow(p2.x() - p1.x(),2) +  pow(p2.y() - p1.y(),2);
}

static double distToSegment(const QLineF& line,const QPointF& p){
    double length = pow(line.length(),2);
    const QPointF& p1 = line.p1();
    const QPointF &p2 = line.p2();
    if(length == 0.)
        dist2(p, p1);
    // Consider the line extending the segment, parameterized as p1 + t (p2 - p1).
    // We find projection of point p onto the line.
    // It falls where t = [(p-p1) . (p2-p1)] / |p2-p1|^2
    double t = ((p.x() - p1.x()) * (p2.x() - p1.x()) + (p.y() - p1.y()) * (p2.y() - p1.y())) / length;
    if (t < 0) return dist2(p, p1);
    if (t > 1) return dist2(p, p2);
    return sqrt(dist2(p, QPointF(p1.x() + t * (p2.x() - p1.x()),
         p1.y() + t * (p2.y() - p1.y()))));
}

bool Edge::contains(const QPointF &point) const{
    double dist = distToSegment(line(), point);
    return  dist <= graphicalContainerOffset;
}
void Edge::dragSource(const QPointF& src)
{
    
    setLine(QLineF(line().p1(),src));

    double a = acos(line().dx() / line().length());
    if (line().dy() >= 0)
        a = 2*pi - a;

    double arrowSize = 5;
    QPointF arrowP1 = line().p1() + QPointF(sin(a + pi / 3) * arrowSize,
                                            cos(a + pi / 3) * arrowSize);
    QPointF arrowP2 = line().p1() + QPointF(sin(a + pi - pi / 3) * arrowSize,
                                            cos(a + pi - pi / 3) * arrowSize);
    arrowHead.clear();
	arrowHead << line().p1() << arrowP1 << arrowP2;

    
    if (label) {
        label->setPos(QPointF(((line().p1().x()+src.x())/2.)-5,((line().p1().y()+src.y())/2.)-5));
    }
}

void Edge::dragDest(const QPointF& dst)
{
    setLine(QLineF(dst,line().p2()));

    double a = acos(line().dx() / line().length());
    if (line().dy() >= 0)
        a = 2*pi - a;
    
    double arrowSize = 5;
    QPointF arrowP1 = line().p1() + QPointF(sin(a + pi / 3) * arrowSize,
                                            cos(a + pi / 3) * arrowSize);
    QPointF arrowP2 = line().p1() + QPointF(sin(a + pi - pi / 3) * arrowSize,
                                            cos(a + pi - pi / 3) * arrowSize);
    arrowHead.clear();
	arrowHead << line().p1() << arrowP1 << arrowP2;
    
}

void Edge::paint(QPainter *painter, const QStyleOptionGraphicsItem * /*options*/,
           QWidget * /*parent*/)
 {

     QPen myPen = pen();
     
     if (_useRenderingColor) {
         myPen.setColor(_renderingColor);
     } else {
         myPen.setColor(_defaultColor);
     }
     if(dest && dest->getNode()->getLiveInstance()->isInputOptional(inputNb)){
         QVector<qreal> dashStyle;
         qreal space = 4;
         dashStyle << 3 << space;
         myPen.setDashPattern(dashStyle);
     }else{
         myPen.setStyle(Qt::SolidLine);
     }
     
     painter->setPen(myPen);
     if (_useRenderingColor) {
         painter->setBrush(_renderingColor);
     } else {
         painter->setBrush(_defaultColor);
     }
     painter->drawLine(line());
     painter->drawPolygon(arrowHead);

  }
