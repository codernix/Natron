//  Natron
//
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
*Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
*contact: immarespond at gmail dot com
*
*/

#include "RotoGui.h"

#include <QString>
#include <QToolBar>
#include <QWidget>
#include <QAction>
#include <QRectF>
#include <QLineF>
#include <QKeyEvent>
#include <QHBoxLayout>

#include "Engine/Node.h"
#include "Engine/RotoContext.h"
#include "Engine/TimeLine.h"

#include "Gui/FromQtEnums.h"
#include "Gui/NodeGui.h"
#include "Gui/DockablePanel.h"
#include "Gui/Button.h"
#include "Gui/ViewerTab.h"
#include "Gui/ViewerGL.h"
#include "Gui/GuiAppInstance.h"
#include "Gui/RotoUndoCommand.h"

#include "Global/GLIncludes.h"

#define kControlPointMidSize 3
#define kBezierSelectionTolerance 8
#define kControlPointSelectionTolerance 8
#define kXHairSelectedCpsTolerance 8
#define kXHairSelectedCpsBox 8
#define kTangentHandleSelectionTolerance 8

using namespace Natron;

namespace {
    
///A list of points and their counter-part, that is: either a control point and its feather point, or
///the feather point and its associated control point
typedef std::pair<boost::shared_ptr<BezierCP> ,boost::shared_ptr<BezierCP> > SelectedCP;
typedef std::list< SelectedCP > SelectedCPs;
    
typedef std::list< boost::shared_ptr<Bezier> > SelectedBeziers;
    
enum EventState
{
    NONE = 0,
    DRAGGING_CPS,
    SELECTING,
    BUILDING_BEZIER_CP_TANGENT,
    BUILDING_ELLIPSE,
    BULDING_ELLIPSE_CENTER,
    BUILDING_RECTANGLE,
    DRAGGING_LEFT_TANGENT,
    DRAGGING_RIGHT_TANGENT,
    DRAGGING_FEATHER_BAR
};
    
}

///A small structure of all the data shared by all the viewers watching the same Roto
struct RotoGuiSharedData
{
    SelectedBeziers selectedBeziers;
    
    SelectedCPs selectedCps;
    
    QRectF selectedCpsBbox;
    bool showCpsBbox;
    
    
    QRectF selectionRectangle;
    
    boost::shared_ptr<Bezier> builtBezier; //< the bezier currently being built
    
    boost::shared_ptr<BezierCP> tangentBeingDragged; //< the control point whose tangent is being dragged.
                                                     //only relevant when the state is DRAGGING_X_TANGENT
    SelectedCP featherBarBeingDragged;
    
    RotoGuiSharedData()
    : selectedBeziers()
    , selectedCps()
    , selectedCpsBbox()
    , showCpsBbox(false)
    , selectionRectangle()
    , builtBezier()
    , tangentBeingDragged()
    , featherBarBeingDragged()
    {
        
    }
    
};

struct RotoGui::RotoGuiPrivate
{
    
    RotoGui* publicInterface;
    
    NodeGui* node;
    ViewerGL* viewer;
    ViewerTab* viewerTab;
    
    boost::shared_ptr<RotoContext> context;
    
    Roto_Type type;
    
    QToolBar* toolbar;
    
    QWidget* selectionButtonsBar;
    QHBoxLayout* selectionButtonsBarLayout;
    Button* autoKeyingEnabled;
    Button* featherLinkEnabled;
    Button* stickySelectionEnabled;
    Button* rippleEditEnabled;
    Button* addKeyframeButton;
    Button* removeKeyframeButton;
    
    
    RotoToolButton* selectTool;
    RotoToolButton* pointsEditionTool;
    RotoToolButton* bezierEditionTool;

    QAction* selectAllAction;
    
    Roto_Tool selectedTool;
    QToolButton* selectedRole;
    
    Natron::KeyboardModifiers modifiers;
    
    EventState state;
    
    QPointF lastClickPos;
    QPointF lastMousePos;

    boost::shared_ptr< RotoGuiSharedData > rotoData;
    
    bool evaluateOnPenUp; //< if true the next pen up will call context->evaluateChange()
    bool evaluateOnKeyUp ; //< if true the next key up will call context->evaluateChange()
    
    RotoGuiPrivate(RotoGui* pub,NodeGui* n,ViewerTab* tab,const boost::shared_ptr<RotoGuiSharedData>& sharedData)
    : publicInterface(pub)
    , node(n)
    , viewer(tab->getViewer())
    , viewerTab(tab)
    , context()
    , type(ROTOSCOPING)
    , toolbar(0)
    , selectionButtonsBar(0)
    , selectTool(0)
    , pointsEditionTool(0)
    , bezierEditionTool(0)
    , selectAllAction(0)
    , selectedTool(SELECT_ALL)
    , selectedRole(0)
    , modifiers(Natron::NoModifier)
    , state(NONE)
    , lastClickPos()
    , lastMousePos()
    , rotoData(sharedData)
    , evaluateOnPenUp(false)
    , evaluateOnKeyUp(false)
    {
        if (n->getNode()->isRotoPaintingNode()) {
            type = ROTOPAINTING;
        }
        context = node->getNode()->getRotoContext();
        assert(context);
        if (!rotoData) {
            rotoData.reset(new RotoGuiSharedData);
        }
        
    }
    
    void clearSelection();
    
    void clearCPSSelection();
    
    void clearBeziersSelection();
    
    void onCurveLockedChangedRecursive(const boost::shared_ptr<RotoItem>& item,bool* ret);
    
    bool removeBezierFromSelection(const Bezier* b);
    
    void refreshSelectionRectangle(const QPointF& pos);
    
    void updateSelectionFromSelectionRectangle();
    
    void drawSelectionRectangle();
    
    void computeSelectedCpsBBOX();
    
    void drawSelectedCpsBBOX();
    
    bool isNearbySelectedCpsCrossHair(const QPointF& pos) const;
    
    void handleBezierSelection(const boost::shared_ptr<Bezier>& curve);
    
    void handleControlPointSelection(const std::pair<boost::shared_ptr<BezierCP>,boost::shared_ptr<BezierCP> >& p);
    
    void drawSelectedCp(int time,const boost::shared_ptr<BezierCP>& cp,double x,double y);
    
    std::pair<boost::shared_ptr<BezierCP>,boost::shared_ptr<BezierCP> >
    isNearbyFeatherBar(int time,const std::pair<double,double>& pixelScale,const QPointF& pos) const;
    
};

RotoToolButton::RotoToolButton(QWidget* parent)
: QToolButton(parent)
{
    
}

void RotoToolButton::mousePressEvent(QMouseEvent* /*event*/)
{
    
}

void RotoToolButton::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        handleSelection();
    } else if (event->button() == Qt::RightButton) {
        showMenu();
    } else {
        QToolButton::mousePressEvent(event);
    }
}

void RotoToolButton::handleSelection()
{
    QAction* curAction = defaultAction();
    if (!isDown()) {
        emit triggered(curAction);
    } else {
        QList<QAction*> allAction = actions();
        for (int i = 0; i < allAction.size(); ++i) {
            if (allAction[i] == curAction) {
                int next = (i == (allAction.size() - 1)) ? 0 : i+1;
                setDefaultAction(allAction[next]);
                emit triggered(allAction[next]);
                break;
            }
        }
    }
}

QAction* RotoGui::createToolAction(QToolButton* toolGroup,
                                   const QIcon& icon,
                                   const QString& text,
                                   const QString& tooltip,
                                   const QKeySequence& shortcut,
                                   RotoGui::Roto_Tool tool)
{
    
#pragma message WARN("Change constructor when icons will be added")
    QAction *action = new QAction(icon,text,toolGroup);
    action->setToolTip(text + ": " + tooltip + "<p><b>Keyboard shortcut: " + shortcut.toString(QKeySequence::NativeText) + "</b></p>");
    
    QPoint data;
    data.setX((int)tool);
    if (toolGroup == _imp->selectTool) {
        data.setY((int)SELECTION_ROLE);
    } else if (toolGroup == _imp->pointsEditionTool) {
        data.setY((int)POINTS_EDITION_ROLE);
    } else if (toolGroup == _imp->bezierEditionTool) {
        data.setY(BEZIER_EDITION_ROLE);
    }
    action->setData(QVariant(data));
    QObject::connect(action, SIGNAL(triggered()), this, SLOT(onToolActionTriggered()));
    toolGroup->addAction(action);
    return action;
}

RotoGui::RotoGui(NodeGui* node,ViewerTab* parent,const boost::shared_ptr<RotoGuiSharedData>& sharedData)
: _imp(new RotoGuiPrivate(this,node,parent,sharedData))
{
    assert(parent);
    
    _imp->toolbar = new QToolBar(parent);
    _imp->toolbar->setOrientation(Qt::Vertical);
    _imp->selectionButtonsBar = new QWidget(parent);
    _imp->selectionButtonsBarLayout = new QHBoxLayout(_imp->selectionButtonsBar);
    
    _imp->autoKeyingEnabled = new Button(QIcon(),"Auto-key",_imp->selectionButtonsBar);
    _imp->autoKeyingEnabled->setCheckable(true);
    _imp->autoKeyingEnabled->setChecked(_imp->context->isAutoKeyingEnabled());
    _imp->autoKeyingEnabled->setDown(_imp->context->isAutoKeyingEnabled());
    _imp->autoKeyingEnabled->setToolTip("When activated any movement to a control point will set a keyframe at the current time.");
    QObject::connect(_imp->autoKeyingEnabled, SIGNAL(clicked(bool)), this, SLOT(onAutoKeyingButtonClicked(bool)));
    _imp->selectionButtonsBarLayout->addWidget(_imp->autoKeyingEnabled);
    
    _imp->featherLinkEnabled = new Button(QIcon(),"Feather-link",_imp->selectionButtonsBar);
    _imp->featherLinkEnabled->setCheckable(true);
    _imp->featherLinkEnabled->setChecked(_imp->context->isFeatherLinkEnabled());
    _imp->featherLinkEnabled->setDown(_imp->context->isFeatherLinkEnabled());
    _imp->featherLinkEnabled->setToolTip("When activated the feather points will follow the same movement as their counter-part does.");
    QObject::connect(_imp->featherLinkEnabled, SIGNAL(clicked(bool)), this, SLOT(onFeatherLinkButtonClicked(bool)));
    _imp->selectionButtonsBarLayout->addWidget(_imp->featherLinkEnabled);
    
    _imp->stickySelectionEnabled = new Button(QIcon(),"Sticky-selection",_imp->selectionButtonsBar);
    _imp->stickySelectionEnabled->setCheckable(true);
    _imp->stickySelectionEnabled->setChecked(false);
    _imp->stickySelectionEnabled->setDown(false);
    _imp->stickySelectionEnabled->setToolTip("When activated, clicking outside of any shape will not clear the current selection.");
    QObject::connect(_imp->stickySelectionEnabled, SIGNAL(clicked(bool)), this, SLOT(onStickySelectionButtonClicked(bool)));
    _imp->selectionButtonsBarLayout->addWidget(_imp->stickySelectionEnabled);
    
    _imp->rippleEditEnabled = new Button(QIcon(),"Ripple-edit",_imp->selectionButtonsBar);
    _imp->rippleEditEnabled->setCheckable(true);
    _imp->rippleEditEnabled->setChecked(_imp->context->isRippleEditEnabled());
    _imp->rippleEditEnabled->setDown(_imp->context->isRippleEditEnabled());
    _imp->rippleEditEnabled->setToolTip("When activated, moving a control point will set it as the same position for all the keyframes "
                                        "it has.");
    QObject::connect(_imp->rippleEditEnabled, SIGNAL(clicked(bool)), this, SLOT(onRippleEditButtonClicked(bool)));
    _imp->selectionButtonsBarLayout->addWidget(_imp->rippleEditEnabled);
    
    _imp->addKeyframeButton = new Button(QIcon(),"+ keyframe",_imp->selectionButtonsBar);
    QObject::connect(_imp->addKeyframeButton, SIGNAL(clicked(bool)), this, SLOT(onAddKeyFrameClicked()));
    _imp->addKeyframeButton->setToolTip("Set a keyframe at the current time for the selected shapes, if any.");
    _imp->selectionButtonsBarLayout->addWidget(_imp->addKeyframeButton);
    
    _imp->removeKeyframeButton = new Button(QIcon(),"- keyframe",_imp->selectionButtonsBar);
    QObject::connect(_imp->removeKeyframeButton, SIGNAL(clicked(bool)), this, SLOT(onRemoveKeyFrameClicked()));
    _imp->removeKeyframeButton->setToolTip("Remove a keyframe at the current time for the selected shape(s), if any.");
    _imp->selectionButtonsBarLayout->addWidget(_imp->removeKeyframeButton);
        
    _imp->selectTool = new RotoToolButton(_imp->toolbar);
    _imp->selectTool->setPopupMode(QToolButton::InstantPopup);
    QObject::connect(_imp->selectTool, SIGNAL(triggered(QAction*)), this, SLOT(onToolActionTriggered(QAction*)));
    QKeySequence selectShortCut(Qt::Key_Q);
    _imp->selectAllAction = createToolAction(_imp->selectTool, QIcon(), "Select all",
                                             "everything can be selected and moved.",
                                             selectShortCut, SELECT_ALL);
    createToolAction(_imp->selectTool, QIcon(), "Select points",
                     "works only for the points of the inner shape,"
                     " feather points will not be taken into account.",
                     selectShortCut, SELECT_POINTS);
    createToolAction(_imp->selectTool, QIcon(), "Select curves",
                     "only the curves can be selected."
                     ,selectShortCut,SELECT_CURVES);
    createToolAction(_imp->selectTool, QIcon(), "Select feather points", "only the feather points can be selected.",selectShortCut,SELECT_FEATHER_POINTS);
    _imp->selectTool->setDown(false);
    _imp->selectTool->setDefaultAction(_imp->selectAllAction);
    _imp->toolbar->addWidget(_imp->selectTool);
    
    _imp->pointsEditionTool = new RotoToolButton(_imp->toolbar);
    _imp->pointsEditionTool->setPopupMode(QToolButton::InstantPopup);
    QObject::connect(_imp->pointsEditionTool, SIGNAL(triggered(QAction*)), this, SLOT(onToolActionTriggered(QAction*)));
    _imp->pointsEditionTool->setText("Add points");
    QKeySequence pointsEditionShortcut(Qt::Key_D);
    QAction* addPtsAct = createToolAction(_imp->pointsEditionTool, QIcon(), "Add points","add a new control point to the shape"
                                          ,pointsEditionShortcut, ADD_POINTS);
    createToolAction(_imp->pointsEditionTool, QIcon(), "Remove points","",pointsEditionShortcut,REMOVE_POINTS);
    createToolAction(_imp->pointsEditionTool, QIcon(), "Cusp points","", pointsEditionShortcut,CUSP_POINTS);
    createToolAction(_imp->pointsEditionTool, QIcon(), "Smooth points","", pointsEditionShortcut,SMOOTH_POINTS);
    createToolAction(_imp->pointsEditionTool, QIcon(), "Open/Close curve","", pointsEditionShortcut,OPEN_CLOSE_CURVE);
    createToolAction(_imp->pointsEditionTool, QIcon(), "Remove feather","set the feather point to be equal to the control point", pointsEditionShortcut,REMOVE_FEATHER_POINTS);
    _imp->pointsEditionTool->setDown(false);
    _imp->pointsEditionTool->setDefaultAction(addPtsAct);
    _imp->toolbar->addWidget(_imp->pointsEditionTool);
    
    _imp->bezierEditionTool = new RotoToolButton(_imp->toolbar);
    _imp->bezierEditionTool->setPopupMode(QToolButton::InstantPopup);
    QObject::connect(_imp->bezierEditionTool, SIGNAL(triggered(QAction*)), this, SLOT(onToolActionTriggered(QAction*)));
    _imp->bezierEditionTool->setText("Bezier");
    QKeySequence editBezierShortcut(Qt::Key_V);
    QAction* drawBezierAct = createToolAction(_imp->bezierEditionTool, QIcon(), "Bezier",
                                              "Edit bezier paths. Click and drag the mouse to adjust tangents. Press enter to close the shape. "
                                              ,editBezierShortcut, DRAW_BEZIER);
    
    ////B-splines are not implemented yet
    //createToolAction(_imp->bezierEditionTool, QIcon(), "B-Spline", DRAW_B_SPLINE);
    
    createToolAction(_imp->bezierEditionTool, QIcon(), "Ellipse","Hold control to draw the ellipse from its center",editBezierShortcut, DRAW_ELLIPSE);
    createToolAction(_imp->bezierEditionTool, QIcon(), "Rectangle","", editBezierShortcut,DRAW_RECTANGLE);
    _imp->toolbar->addWidget(_imp->bezierEditionTool);
    
    ////////////Default action is to make a new bezier
    _imp->selectedRole = _imp->selectTool;
    onToolActionTriggered(drawBezierAct);

    QObject::connect(_imp->node->getNode()->getApp()->getTimeLine().get(), SIGNAL(frameChanged(SequenceTime,int)),
                     this, SLOT(onCurrentFrameChanged(SequenceTime,int)));
    QObject::connect(_imp->context.get(), SIGNAL(refreshViewerOverlays()), this, SLOT(onRefreshAsked()));
    QObject::connect(_imp->context.get(), SIGNAL(selectionChanged(int)), this, SLOT(onSelectionChanged(int)));
    QObject::connect(_imp->context.get(), SIGNAL(itemLockedChanged()), this, SLOT(onCurveLockedChanged()));
    restoreSelectionFromContext();
}

RotoGui::~RotoGui()
{
    
}

boost::shared_ptr<RotoGuiSharedData> RotoGui::getRotoGuiSharedData() const
{
    return _imp->rotoData;
}

QWidget* RotoGui::getButtonsBar(RotoGui::Roto_Role role) const
{
    switch (role) {
        case SELECTION_ROLE:
            return _imp->selectionButtonsBar;
            break;
        case POINTS_EDITION_ROLE:
            return _imp->selectionButtonsBar;
            break;
        case BEZIER_EDITION_ROLE:
            return _imp->selectionButtonsBar;
            break;
        default:
            assert(false);
            break;
    }
}

QWidget* RotoGui::getCurrentButtonsBar() const
{
    return getButtonsBar(getCurrentRole());
}

RotoGui::Roto_Tool RotoGui::getSelectedTool() const
{
    return _imp->selectedTool;
}

void RotoGui::setCurrentTool(RotoGui::Roto_Tool tool,bool emitSignal)
{
    QList<QAction*> actions = _imp->selectTool->actions();
    actions.append(_imp->pointsEditionTool->actions());
    actions.append(_imp->bezierEditionTool->actions());
    for (int i = 0; i < actions.size(); ++i) {
        QPoint data = actions[i]->data().toPoint();
        if ((RotoGui::Roto_Tool)data.x() == tool) {
            onToolActionTriggeredInternal(actions[i],emitSignal);
            return;
        }
    }
    assert(false);
}

QToolBar* RotoGui::getToolBar() const
{
    return _imp->toolbar;
}

void RotoGui::onToolActionTriggered()
{
    QAction* act = qobject_cast<QAction*>(sender());
    if (act) {
        onToolActionTriggered(act);
    }
}

void RotoGui::onToolActionTriggeredInternal(QAction* action,bool emitSignal)
{
    QPoint data = action->data().toPoint();
    Roto_Role actionRole = (Roto_Role)data.y();
    QToolButton* toolButton = 0;
    
    Roto_Role previousRole = getCurrentRole();
    
    switch (actionRole) {
        case SELECTION_ROLE:
            toolButton = _imp->selectTool;
            emit roleChanged((int)previousRole,(int)SELECTION_ROLE);
            break;
        case POINTS_EDITION_ROLE:
            toolButton = _imp->pointsEditionTool;
            emit roleChanged((int)previousRole,(int)POINTS_EDITION_ROLE);
            break;
        case BEZIER_EDITION_ROLE:
            toolButton = _imp->bezierEditionTool;
            emit roleChanged((int)previousRole,(int)BEZIER_EDITION_ROLE);
            break;
        default:
            assert(false);
            break;
    }
    
    
    assert(_imp->selectedRole);
    if (_imp->selectedRole != toolButton) {
        _imp->selectedRole->setDown(false);
    }
    
    ///reset the selected control points
    _imp->rotoData->selectedCps.clear();
    _imp->rotoData->showCpsBbox = false;
    _imp->rotoData->selectedCpsBbox.setTopLeft(QPointF(0,0));
    _imp->rotoData->selectedCpsBbox.setTopRight(QPointF(0,0));
    
    ///clear all selection if we were building a new bezier
    if (previousRole == BEZIER_EDITION_ROLE && _imp->selectedTool == DRAW_BEZIER && _imp->rotoData->builtBezier &&
        (Roto_Tool)data.x() != _imp->selectedTool) {
        _imp->rotoData->builtBezier->setCurveFinished(true);
        _imp->clearSelection();
    }
    
    
    assert(toolButton);
    toolButton->setDown(true);
    toolButton->setDefaultAction(action);
    _imp->selectedRole = toolButton;
    _imp->selectedTool = (Roto_Tool)data.x();
    if(emitSignal) {
        emit selectedToolChanged((int)_imp->selectedTool);
    }

}

void RotoGui::onToolActionTriggered(QAction* act)
{
    onToolActionTriggeredInternal(act, true);
}

RotoGui::Roto_Role RotoGui::getCurrentRole() const
{
    if (_imp->selectedRole == _imp->selectTool) {
        return SELECTION_ROLE;
    } else if (_imp->selectedRole == _imp->pointsEditionTool) {
        return POINTS_EDITION_ROLE;
    } else if (_imp->selectedRole == _imp->bezierEditionTool) {
        return BEZIER_EDITION_ROLE;
    }
    assert(false);
}

void RotoGui::RotoGuiPrivate::drawSelectedCp(int time,const boost::shared_ptr<BezierCP>& cp,double x,double y)
{
    ///if the tangent is being dragged, color it
    bool colorLeftTangent = false;
    bool colorRightTangent = false;
    if (cp == rotoData->tangentBeingDragged &&
        (state == DRAGGING_LEFT_TANGENT  || state == DRAGGING_RIGHT_TANGENT)) {
        colorLeftTangent = state == DRAGGING_LEFT_TANGENT ? true : false;
        colorRightTangent = !colorLeftTangent;
    }
    
    
    double leftDerivX,leftDerivY,rightDerivX,rightDerivY;
    cp->getLeftBezierPointAtTime(time, &leftDerivX, &leftDerivY);
    cp->getRightBezierPointAtTime(time, &rightDerivX, &rightDerivY);
    
    bool drawLeftHandle = leftDerivX != x || leftDerivY != y;
    bool drawRightHandle = rightDerivX != x || rightDerivY != y;
    glBegin(GL_POINTS);
    if (drawLeftHandle) {
        if (colorLeftTangent) {
            glColor3f(0.2, 1., 0.);
        }
        glVertex2d(leftDerivX,leftDerivY);
        if (colorLeftTangent) {
            glColor3d(0.85, 0.67, 0.);
        }
    }
    if (drawRightHandle) {
        if (colorRightTangent) {
            glColor3f(0.2, 1., 0.);
        }
        glVertex2d(rightDerivX,rightDerivY);
        if (colorRightTangent) {
            glColor3d(0.85, 0.67, 0.);
        }
    }
    glEnd();
    
    glBegin(GL_LINE_STRIP);
    if (drawLeftHandle) {
        glVertex2d(leftDerivX,leftDerivY);
    }
    glVertex2d(x, y);
    if (drawRightHandle) {
        glVertex2d(rightDerivX,rightDerivY);
    }
    glEnd();

}

void RotoGui::drawOverlays(double /*scaleX*/,double /*scaleY*/) const
{
    std::list< boost::shared_ptr<Bezier> > beziers = _imp->context->getCurvesByRenderOrder();
    int time = _imp->context->getTimelineCurrentTime();
    
    std::pair<double,double> pixelScale;
    std::pair<double,double> viewportSize;
    
    _imp->viewer->getPixelScale(pixelScale.first, pixelScale.second);
    _imp->viewer->getViewportSize(viewportSize.first, viewportSize.second);
    
    glPushAttrib(GL_HINT_BIT | GL_ENABLE_BIT | GL_LINE_BIT | GL_COLOR_BUFFER_BIT | GL_POINT_BIT);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_BLEND);
    glHint(GL_LINE_SMOOTH_HINT,GL_DONT_CARE);
    glLineWidth(1.5);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glPointSize(7.);
    glEnable(GL_POINT_SMOOTH);
    for (std::list< boost::shared_ptr<Bezier> >::const_iterator it = beziers.begin(); it!=beziers.end(); ++it) {
        
        if ((*it)->isActivated(time)) {
            
            ///draw the bezier
            std::list< Point > points;
            (*it)->evaluateAtTime_DeCasteljau(time,0, 100, &points);
            
            double curveColor[4];
            if (!(*it)->isLockedRecursive()) {
                (*it)->getOverlayColor(curveColor);
            } else {
                curveColor[0] = 0.8;curveColor[1] = 0.8;curveColor[2] = 0.8;curveColor[3] = 1.;
            }
            glColor4dv(curveColor);
            
            glBegin(GL_LINE_STRIP);
            for (std::list<Point >::const_iterator it2 = points.begin(); it2!=points.end(); ++it2) {
                glVertex2f(it2->x, it2->y);
            }
            glEnd();
            
            ///draw the feather points
            std::list< Point > featherPoints;
            RectD featherBBox(INT_MAX,INT_MAX,INT_MIN,INT_MIN);
            (*it)->evaluateFeatherPointsAtTime_DeCasteljau(time,0, 100, &featherPoints,true,&featherBBox);
            std::vector<double> constants(featherPoints.size()),multiples(featherPoints.size());
            Bezier::precomputePointInPolygonTables(featherPoints, &constants, &multiples);
            
            if (!featherPoints.empty()) {
                glLineStipple(2, 0xAAAA);
                glEnable(GL_LINE_STIPPLE);
                glBegin(GL_LINE_STRIP);
                for (std::list<Point >::const_iterator it2 = featherPoints.begin(); it2!=featherPoints.end(); ++it2) {
                    glVertex2f(it2->x, it2->y);
                }
                glEnd();
                glDisable(GL_LINE_STIPPLE);
            }
            
            ///draw the control points if the bezier is selected
            std::list< boost::shared_ptr<Bezier> >::const_iterator selected =
            std::find(_imp->rotoData->selectedBeziers.begin(),_imp->rotoData->selectedBeziers.end(),*it);
            
            if (selected != _imp->rotoData->selectedBeziers.end()) {
                const std::list< boost::shared_ptr<BezierCP> >& cps = (*selected)->getControlPoints();
                const std::list< boost::shared_ptr<BezierCP> >& featherPts = (*selected)->getFeatherPoints();
                assert(cps.size() == featherPts.size());
                
                double cpHalfWidth = kControlPointMidSize * pixelScale.first;
                double cpHalfHeight = kControlPointMidSize * pixelScale.second;
                
                glColor3d(0.85, 0.67, 0.);
                
                std::list< boost::shared_ptr<BezierCP> >::const_iterator itF = featherPts.begin();
                int index = 0;
                
                std::list< boost::shared_ptr<BezierCP> >::const_iterator prevCp = cps.end();
                --prevCp;
                std::list< boost::shared_ptr<BezierCP> >::const_iterator nextCp = cps.begin();
                ++nextCp;
                for (std::list< boost::shared_ptr<BezierCP> >::const_iterator it2 = cps.begin(); it2!=cps.end();
                     ++it2,++itF,++index,++nextCp,++prevCp) {
                    
                    if (nextCp == cps.end()) {
                        nextCp = cps.begin();
                    }
                    if (prevCp == cps.end()) {
                        prevCp = cps.begin();
                    }
                    
                    double x,y;
                    (*it2)->getPositionAtTime(time, &x, &y);
                    
                    ///if the control point is the only control point being dragged, color it to identify it to the user
                    bool colorChanged = false;
                    SelectedCPs::const_iterator firstSelectedCP = _imp->rotoData->selectedCps.begin();
                    if ((firstSelectedCP->first == *it2)
                        && _imp->rotoData->selectedCps.size() == 1 && _imp->state == DRAGGING_CPS) {
                        glColor3f(0.2, 1., 0.);
                        colorChanged = true;
                    }
                    
                    glBegin(GL_POLYGON);
                    glVertex2f(x - cpHalfWidth, y - cpHalfHeight);
                    glVertex2f(x + cpHalfWidth, y - cpHalfHeight);
                    glVertex2f(x + cpHalfWidth, y + cpHalfHeight);
                    glVertex2f(x - cpHalfWidth, y + cpHalfHeight);
                    glEnd();
                    
                    if (colorChanged) {
                        glColor3d(0.85, 0.67, 0.);
                    }
                    
                    if ((firstSelectedCP->first == *itF)
                        && _imp->rotoData->selectedCps.size() == 1 && _imp->state == DRAGGING_CPS && !colorChanged) {
                        glColor3f(0.2, 1., 0.);
                        colorChanged = true;
                    }
                    
                    double xF,yF;
                    (*itF)->getPositionAtTime(time, &xF, &yF);
                    ///draw the feather point only if it is distinct from the associated point
                    bool drawFeather = !(*it2)->equalsAtTime(time, **itF);
                    double distFeatherX = 20. * pixelScale.first;
                    double distFeatherY = 20. * pixelScale.second;
                    if (drawFeather) {
                        glBegin(GL_POLYGON);
                        glVertex2f(xF - cpHalfWidth, yF - cpHalfHeight);
                        glVertex2f(xF + cpHalfWidth, yF - cpHalfHeight);
                        glVertex2f(xF + cpHalfWidth, yF + cpHalfHeight);
                        glVertex2f(xF - cpHalfWidth, yF + cpHalfHeight);
                        glEnd();
                        
                        if (_imp->state == DRAGGING_FEATHER_BAR &&
                            (*itF == _imp->rotoData->featherBarBeingDragged.first || *itF == _imp->rotoData->featherBarBeingDragged.second)) {
                            glColor3f(0.2, 1., 0.);
                            colorChanged = true;
                        } else {
                           
                            glColor4dv(curveColor);
                        }
                        
                        double beyondX,beyondY;
                        double dx = (xF - x);
                        double dy = (yF - y);
                        double dist = sqrt(dx * dx + dy * dy);
                        beyondX = (dx * (dist + distFeatherX)) / dist + x;
                        beyondY = (dy * (dist + distFeatherY)) / dist + y;
                        
                        ///draw a link between the feather point and the control point.
                        ///Also extend that link of 20 pixels beyond the feather point.
                        
                        glBegin(GL_LINE_STRIP);
                        glVertex2f(x, y);
                        glVertex2f(xF, yF);
                        glVertex2f(beyondX, beyondY);
                        glEnd();
                        
                        glColor3d(0.85, 0.67, 0.);
                        
                    } else {
                        ///if the feather point is identical to the control point
                        ///draw a small hint line that the user can drag to move the feather point
                        if (_imp->selectedTool == SELECT_ALL || _imp->selectedTool == SELECT_FEATHER_POINTS) {
                            int cpCount = (*it2)->getCurve()->getControlPointsCount();
                            if (cpCount > 1) {
                                
                                Natron::Point controlPoint;
                                controlPoint.x = x;
                                controlPoint.y = y;
                                Natron::Point featherPoint;
                                featherPoint.x = xF;
                                featherPoint.y = yF;
                                
                                Bezier::expandToFeatherDistance(controlPoint, &featherPoint, distFeatherX, featherPoints, constants, multiples, featherBBox, time, prevCp, it2, nextCp);
                                
                                if (_imp->state == DRAGGING_FEATHER_BAR &&
                                    (*itF == _imp->rotoData->featherBarBeingDragged.first ||
                                     *itF == _imp->rotoData->featherBarBeingDragged.second)) {
                                    glColor3f(0.2, 1., 0.);
                                    colorChanged = true;
                                } else {
                                    glColor4dv(curveColor);
                                }
                              
                                glBegin(GL_LINES);
                                glVertex2f(x, y);
                                glVertex2f(featherPoint.x, featherPoint.y);
                                glEnd();
                                
                                glColor3d(0.85, 0.67, 0.);
                      
                            }
                        }
                    }
                    
                    
                    if (colorChanged) {
                        glColor3d(0.85, 0.67, 0.);
                        colorChanged = false;
                    }

                    
                    for (SelectedCPs::const_iterator cpIt = _imp->rotoData->selectedCps.begin();
                         cpIt != _imp->rotoData->selectedCps.end();++cpIt) {
                        
                        ///if the control point is selected, draw its tangent handles
                        if (cpIt->first == *it2) {
                            _imp->drawSelectedCp(time, cpIt->first, x, y);
                            if (drawFeather) {
                                _imp->drawSelectedCp(time, cpIt->second, xF, yF);
                            }
                        } else if (cpIt->second == *it2) {
                            _imp->drawSelectedCp(time, cpIt->second, x, y);
                            if (drawFeather) {
                                _imp->drawSelectedCp(time, cpIt->first, xF, yF);
                            }
 
                        }
                    }
                   
  
                }
            }
        }
    }
    
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_POINT_SMOOTH);
    glLineWidth(1.);
    glPointSize(1.);
    glDisable(GL_BLEND);
    glPopAttrib();
    

    if (_imp->state == SELECTING) {
        _imp->drawSelectionRectangle();
    }
    
    if (_imp->rotoData->showCpsBbox && _imp->state != SELECTING) {
        _imp->drawSelectedCpsBBOX();
    }
}

void RotoGui::RotoGuiPrivate::drawSelectedCpsBBOX()
{
    std::pair<double,double> pixelScale;
    viewer->getPixelScale(pixelScale.first, pixelScale.second);
    
    glPushAttrib(GL_HINT_BIT | GL_ENABLE_BIT | GL_LINE_BIT | GL_COLOR_BUFFER_BIT);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glHint(GL_LINE_SMOOTH_HINT,GL_DONT_CARE);
    
    
    QPointF topLeft = rotoData->selectedCpsBbox.topLeft();
    QPointF btmRight = rotoData->selectedCpsBbox.bottomRight();
    
    glLineWidth(1.5);
    
    glColor4f(0.8,0.8,0.8,1.);
    glBegin(GL_LINE_STRIP);
    glVertex2f(topLeft.x(),btmRight.y());
    glVertex2f(topLeft.x(),topLeft.y());
    glVertex2f(btmRight.x(),topLeft.y());
    glVertex2f(btmRight.x(),btmRight.y());
    glVertex2f(topLeft.x(),btmRight.y());
    glEnd();
    
    double midX = (topLeft.x() + btmRight.x()) / 2.;
    double midY = (btmRight.y() + topLeft.y()) / 2.;
    
    double xHairMidSizeX = kXHairSelectedCpsBox * pixelScale.first;
    double xHairMidSizeY = kXHairSelectedCpsBox * pixelScale.second;

    
    QLineF selectedCpsCrossHorizLine;
    selectedCpsCrossHorizLine.setLine(midX - xHairMidSizeX, midY, midX + xHairMidSizeX, midY);
    QLineF selectedCpsCrossVertLine;
    selectedCpsCrossVertLine.setLine(midX, midY - xHairMidSizeY, midX, midY + xHairMidSizeY);
    
    glBegin(GL_LINES);
    glVertex2f(std::max(selectedCpsCrossHorizLine.p1().x(),topLeft.x()),selectedCpsCrossHorizLine.p1().y());
    glVertex2f(std::min(selectedCpsCrossHorizLine.p2().x(),btmRight.x()),selectedCpsCrossHorizLine.p2().y());
    glVertex2f(selectedCpsCrossVertLine.p1().x(),std::max(selectedCpsCrossVertLine.p1().y(),btmRight.y()));
    glVertex2f(selectedCpsCrossVertLine.p2().x(),std::min(selectedCpsCrossVertLine.p2().y(),topLeft.y()));
    glEnd();
    
    glDisable(GL_LINE_SMOOTH);
    glCheckError();
    
    glLineWidth(1.);
    glPopAttrib();
    glColor4f(1., 1., 1., 1.);
}

void RotoGui::RotoGuiPrivate::drawSelectionRectangle()
{
    
    glPushAttrib(GL_HINT_BIT | GL_ENABLE_BIT | GL_LINE_BIT | GL_COLOR_BUFFER_BIT);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glHint(GL_LINE_SMOOTH_HINT,GL_DONT_CARE);
    
    glColor4f(0.5,0.8,1.,0.2);
    QPointF btmRight = rotoData->selectionRectangle.bottomRight();
    QPointF topLeft = rotoData->selectionRectangle.topLeft();
    
    glBegin(GL_POLYGON);
    glVertex2f(topLeft.x(),btmRight.y());
    glVertex2f(topLeft.x(),topLeft.y());
    glVertex2f(btmRight.x(),topLeft.y());
    glVertex2f(btmRight.x(),btmRight.y());
    glEnd();
    
    
    glLineWidth(1.5);
    
    glBegin(GL_LINE_STRIP);
    glVertex2f(topLeft.x(),btmRight.y());
    glVertex2f(topLeft.x(),topLeft.y());
    glVertex2f(btmRight.x(),topLeft.y());
    glVertex2f(btmRight.x(),btmRight.y());
    glVertex2f(topLeft.x(),btmRight.y());
    glEnd();
    
    
    glDisable(GL_LINE_SMOOTH);
    glCheckError();
    
    glLineWidth(1.);
    glPopAttrib();
    glColor4f(1., 1., 1., 1.);

}

void RotoGui::RotoGuiPrivate::refreshSelectionRectangle(const QPointF& pos)
{
    RectD selection(std::min(lastClickPos.x(),pos.x()),
                    std::min(lastClickPos.y(),pos.y()),
                    std::max(lastClickPos.x(),pos.x()),
                    std::max(lastClickPos.y(),pos.y()));

    rotoData->selectionRectangle.setBottomRight(QPointF(selection.x2,selection.y1));
    rotoData->selectionRectangle.setTopLeft(QPointF(selection.x1,selection.y2));
}

void RotoGui::RotoGuiPrivate::updateSelectionFromSelectionRectangle()
{
    
    if (!publicInterface->isStickySelectionEnabled()) {
        clearSelection();
    }
    
    int selectionMode = -1;
    if (selectedTool == SELECT_ALL) {
        selectionMode = 0;
    } else if (selectedTool == SELECT_POINTS) {
        selectionMode = 1;
    } else if (selectedTool == SELECT_FEATHER_POINTS || selectedTool == SELECT_CURVES) {
        selectionMode = 2;
    }
    
    QPointF topLeft = rotoData->selectionRectangle.topLeft();
    QPointF btmRight = rotoData->selectionRectangle.bottomRight();
    int l = std::min(topLeft.x(), btmRight.x());
    int r = std::max(topLeft.x(), btmRight.x());
    int b = std::min(topLeft.y(), btmRight.y());
    int t = std::max(topLeft.y(), btmRight.y());
    std::list<boost::shared_ptr<Bezier> > curves = context->getCurvesByRenderOrder();
    for (std::list<boost::shared_ptr<Bezier> >::const_iterator it = curves.begin(); it!=curves.end(); ++it) {
        
        if (!(*it)->isLockedRecursive()) {
            SelectedCPs points  = (*it)->controlPointsWithinRect(l, r, b, t, 0,selectionMode);
            if (selectedTool != SELECT_CURVES) {
                rotoData->selectedCps.insert(rotoData->selectedCps.end(), points.begin(), points.end());
            }
            if (!points.empty()) {
                rotoData->selectedBeziers.push_back(*it);
            }
        }
    }
    
    
    
    context->select(rotoData->selectedBeziers, RotoContext::OVERLAY_INTERACT);
    
    computeSelectedCpsBBOX();

}

void RotoGui::RotoGuiPrivate::clearSelection()
{
    clearBeziersSelection();
    clearCPSSelection();
    
}
void RotoGui::RotoGuiPrivate::clearCPSSelection()
{
    rotoData->selectedCps.clear();
    rotoData->showCpsBbox = false;
    rotoData->selectedCpsBbox.setTopLeft(QPointF(0,0));
    rotoData->selectedCpsBbox.setTopRight(QPointF(0,0));
}

void RotoGui::RotoGuiPrivate::clearBeziersSelection()
{
    context->clearSelection(RotoContext::OVERLAY_INTERACT);
    rotoData->selectedBeziers.clear();
}

bool RotoGui::RotoGuiPrivate::removeBezierFromSelection(const Bezier* b)
{
    for (SelectedBeziers::iterator fb = rotoData->selectedBeziers.begin(); fb != rotoData->selectedBeziers.end(); ++fb) {
        if (fb->get() == b) {
            context->deselect(*fb,RotoContext::OVERLAY_INTERACT);
            rotoData->selectedBeziers.erase(fb);
            return true;
        }
    }
    return false;
}

static void handleControlPointMaximum(int time,const BezierCP& p,double* l,double *b,double *r,double *t)
{
    double x,y,xLeft,yLeft,xRight,yRight;
    p.getPositionAtTime(time, &x, &y);
    p.getLeftBezierPointAtTime(time, &xLeft, &yLeft);
    p.getRightBezierPointAtTime(time, &xRight, &yRight);
    
    *r = std::max(x, *r);
    *l = std::min(x, *l);
    
    *r = std::max(xLeft, *r);
    *l = std::min(xLeft, *l);
  
    *r = std::max(xRight, *r);
    *l = std::min(xRight, *l);
    
    *t = std::max(y, *t);
    *b = std::min(y, *b);
    
    *t = std::max(yLeft, *t);
    *b = std::min(yLeft, *b);

    
    *t = std::max(yRight, *t);
    *b = std::min(yRight, *b);
}

void RotoGui::RotoGuiPrivate::computeSelectedCpsBBOX()
{
    int time = context->getTimelineCurrentTime();
    std::pair<double, double> pixelScale;
    viewer->getPixelScale(pixelScale.first,pixelScale.second);
    
    
    double l = INT_MAX,r = INT_MIN,b = INT_MAX,t = INT_MIN;
    for (SelectedCPs::iterator it = rotoData->selectedCps.begin(); it!=rotoData->selectedCps.end(); ++it) {
        handleControlPointMaximum(time,*(it->first),&l,&b,&r,&t);
        handleControlPointMaximum(time,*(it->second),&l,&b,&r,&t);
    }
    rotoData->selectedCpsBbox.setCoords(l, t, r, b);
    if (rotoData->selectedCps.size() > 1) {
        rotoData->showCpsBbox = true;
    } else {
        rotoData->showCpsBbox = false;
    }
}

void RotoGui::RotoGuiPrivate::handleBezierSelection(const boost::shared_ptr<Bezier>& curve)
{
    ///find out if the bezier is already selected.
    SelectedBeziers::const_iterator found =
    std::find(rotoData->selectedBeziers.begin(),rotoData->selectedBeziers.end(),curve);
    
    if (found == rotoData->selectedBeziers.end()) {
        
        ///clear previous selection if the SHIFT modifier isn't held
        if (!modifiers.testFlag(Natron::ShiftModifier)) {
            clearBeziersSelection();
        }
        rotoData->selectedBeziers.push_back(curve);
        context->select(curve,RotoContext::OVERLAY_INTERACT);
    }

}

void RotoGui::RotoGuiPrivate::handleControlPointSelection(const std::pair<boost::shared_ptr<BezierCP>,boost::shared_ptr<BezierCP> >& p)
{
    ///find out if the cp is already selected.
    SelectedCPs::const_iterator foundCP = rotoData->selectedCps.end();
    for (SelectedCPs::const_iterator it = rotoData->selectedCps.begin(); it!=rotoData->selectedCps.end(); ++it) {
        if (p.first == it->first) {
            foundCP = it;
            break;
        }
    }
    
    if (foundCP == rotoData->selectedCps.end()) {
        ///clear previous selection if the SHIFT modifier isn't held
        if (!modifiers.testFlag(Natron::ShiftModifier)) {
            rotoData->selectedCps.clear();
        }
        rotoData->selectedCps.push_back(p);
        computeSelectedCpsBBOX();
  
    }
    
    state = DRAGGING_CPS;

}



bool RotoGui::penDown(double /*scaleX*/,double /*scaleY*/,const QPointF& /*viewportPos*/,const QPointF& pos)
{
    std::pair<double, double> pixelScale;
    _imp->viewer->getPixelScale(pixelScale.first, pixelScale.second);
    
    bool didSomething = false;

    int time = _imp->context->getTimelineCurrentTime();
    
    ////////////////// TANGENT SELECTION
    ///in all cases except cusp/smooth if a control point is selected, check if the user clicked on a tangent handle
    ///in which case we go into DRAGGING_TANGENT mode
    int tangentSelectionTol = kTangentHandleSelectionTolerance * pixelScale.first;
    if (_imp->selectedTool != CUSP_POINTS && _imp->selectedTool != SMOOTH_POINTS && _imp->selectedTool != SELECT_CURVES) {
        for (SelectedCPs::iterator it = _imp->rotoData->selectedCps.begin(); it!=_imp->rotoData->selectedCps.end(); ++it) {
            if (_imp->selectedTool == SELECT_ALL ||
                _imp->selectedTool == DRAW_BEZIER) {
                int ret = it->first->isNearbyTangent(time, pos.x(), pos.y(), tangentSelectionTol);
                if (ret >= 0) {
                    _imp->rotoData->tangentBeingDragged = it->first;
                    _imp->state = ret == 0 ? DRAGGING_LEFT_TANGENT : DRAGGING_RIGHT_TANGENT;
                    didSomething = true;
                } else {
                    ///try with the counter part point
                    ret = it->second->isNearbyTangent(time, pos.x(), pos.y(), tangentSelectionTol);
                    if (ret >= 0) {
                        _imp->rotoData->tangentBeingDragged = it->second;
                        _imp->state = ret == 0 ? DRAGGING_LEFT_TANGENT : DRAGGING_RIGHT_TANGENT;
                        didSomething = true;
                    }
                }
            } else if (_imp->selectedTool == SELECT_FEATHER_POINTS) {
                const boost::shared_ptr<BezierCP>& fp = it->first->isFeatherPoint() ? it->first : it->second;
                int ret = fp->isNearbyTangent(time, pos.x(), pos.y(), tangentSelectionTol);
                if (ret >= 0) {
                    _imp->rotoData->tangentBeingDragged = fp;
                    _imp->state = ret == 0 ? DRAGGING_LEFT_TANGENT : DRAGGING_RIGHT_TANGENT;
                    didSomething = true;
                }
            } else if (_imp->selectedTool == SELECT_POINTS) {
                const boost::shared_ptr<BezierCP>& cp = it->first->isFeatherPoint() ? it->second : it->first;
                int ret = cp->isNearbyTangent(time, pos.x(), pos.y(), tangentSelectionTol);
                if (ret >= 0) {
                    _imp->rotoData->tangentBeingDragged = cp;
                    _imp->state = ret == 0 ? DRAGGING_LEFT_TANGENT : DRAGGING_RIGHT_TANGENT;
                    didSomething = true;
                }
            }
            
            if (didSomething) {
                return didSomething;
            }
        }
    }
    
    //////////////////BEZIER SELECTION
    /////Check if the point is nearby a bezier
    ///tolerance for bezier selection
    double bezierSelectionTolerance = kBezierSelectionTolerance * pixelScale.first;
    double nearbyBezierT;
    int nearbyBezierCPIndex;
    bool isFeather;
    boost::shared_ptr<Bezier> nearbyBezier =
    _imp->context->isNearbyBezier(pos.x(), pos.y(), bezierSelectionTolerance,&nearbyBezierCPIndex,&nearbyBezierT,&isFeather);

    std::pair<boost::shared_ptr<BezierCP>,boost::shared_ptr<BezierCP> > nearbyCP;
    int nearbyCpIndex = -1;
    double cpSelectionTolerance = kControlPointSelectionTolerance * pixelScale.first;
    if (nearbyBezier) {
        /////////////////CONTROL POINT SELECTION
        //////Check if the point is nearby a control point of a selected bezier
        ///Find out if the user selected a control point
        if (nearbyBezier->isLockedRecursive()) {
            nearbyBezier.reset();
        } else {
            Bezier::ControlPointSelectionPref pref = Bezier::WHATEVER_FIRST;
            if (_imp->selectedTool == SELECT_FEATHER_POINTS) {
                pref = Bezier::FEATHER_FIRST;
            }
            
            nearbyCP = nearbyBezier->isNearbyControlPoint(pos.x(), pos.y(), cpSelectionTolerance,pref,&nearbyCpIndex);
        }

    }
    switch (_imp->selectedTool) {
        case SELECT_ALL:
        case SELECT_POINTS:
        case SELECT_FEATHER_POINTS:
        {
            
            std::pair<boost::shared_ptr<BezierCP>,boost::shared_ptr<BezierCP> > featherBarSel;
            if (_imp->selectedTool == SELECT_ALL || _imp->selectedTool == SELECT_FEATHER_POINTS) {
                featherBarSel = _imp->isNearbyFeatherBar(time, pixelScale, pos);
            }
            if (nearbyBezier) {
                _imp->handleBezierSelection(nearbyBezier);
                ///check if the user clicked nearby the cross hair of the selection rectangle in which case
                ///we drag all the control points selected
                if (_imp->isNearbySelectedCpsCrossHair(pos)) {
                    _imp->state = DRAGGING_CPS;
                } else if (nearbyCP.first) {
                    _imp->handleControlPointSelection(nearbyCP);
                }  else if (featherBarSel.first) {
                    _imp->clearCPSSelection();
                    _imp->rotoData->featherBarBeingDragged = featherBarSel;
                    _imp->handleControlPointSelection(_imp->rotoData->featherBarBeingDragged);
                    _imp->state = DRAGGING_FEATHER_BAR;
                }
                
            } else {
                
                if (featherBarSel.first) {
                    _imp->clearCPSSelection();
                    _imp->rotoData->featherBarBeingDragged = featherBarSel;
                    _imp->handleControlPointSelection(_imp->rotoData->featherBarBeingDragged);
                    _imp->state = DRAGGING_FEATHER_BAR;
                } else if (_imp->isNearbySelectedCpsCrossHair(pos)) {
                    ///check if the user clicked nearby the cross hair of the selection rectangle in which case
                    ///we drag all the control points selected
                    _imp->state = DRAGGING_CPS;
                } else {
                    if (!_imp->modifiers.testFlag(Natron::ShiftModifier)) {
                        if (!isStickySelectionEnabled()) {
                            _imp->clearSelection();
                        }
                        _imp->rotoData->selectionRectangle.setTopLeft(pos);
                        _imp->rotoData->selectionRectangle.setBottomRight(pos);
                        _imp->state = SELECTING;
                        
                    }
                }
            }
            didSomething = true;
            
        }   break;
        case SELECT_CURVES:
            if (nearbyBezier) {
                _imp->handleBezierSelection(nearbyBezier);
            } else {
                if (!isStickySelectionEnabled() && !_imp->modifiers.testFlag(Natron::ShiftModifier)) {
                    _imp->clearSelection();
                    _imp->rotoData->selectionRectangle.setTopLeft(pos);
                    _imp->rotoData->selectionRectangle.setBottomRight(pos);
                    _imp->state = SELECTING;
                    
                }
            }
            break;
        case ADD_POINTS:
            ///If the user clicked on a bezier and this bezier is selected add a control point by
            ///splitting up the targeted segment
            if (nearbyBezier) {
                SelectedBeziers::const_iterator foundBezier =
                std::find(_imp->rotoData->selectedBeziers.begin(), _imp->rotoData->selectedBeziers.end(), nearbyBezier);
                if (foundBezier != _imp->rotoData->selectedBeziers.end()) {
                    ///check that the point is not too close to an existing point
                    if (nearbyCP.first) {
                        _imp->handleControlPointSelection(nearbyCP);
                    } else {
                        pushUndoCommand(new AddPointUndoCommand(this,nearbyBezier,nearbyBezierCPIndex,nearbyBezierT));
                        _imp->evaluateOnPenUp = true;
                    }
                    didSomething = true;
                }
            }
            break;
        case REMOVE_POINTS:
            if (nearbyCP.first) {
                Bezier* curve = nearbyCP.first->getCurve();
                assert(nearbyBezier.get() == curve);
                if (nearbyCP.first->isFeatherPoint()) {
                    pushUndoCommand(new RemovePointUndoCommand(this,nearbyBezier,nearbyCP.second));
                } else {
                    pushUndoCommand(new RemovePointUndoCommand(this,nearbyBezier,nearbyCP.first));
                }
                didSomething = true;
            }
            break;
        case REMOVE_FEATHER_POINTS:
            if (nearbyCP.first) {
                assert(nearbyBezier);
                pushUndoCommand(new RemoveFeatherUndoCommand(this,nearbyBezier,
                                                             nearbyCP.first->isFeatherPoint() ? nearbyCP.first : nearbyCP.second));
                didSomething = true;
            }
            break;
        case OPEN_CLOSE_CURVE:
            if (nearbyBezier) {
                pushUndoCommand(new OpenCloseUndoCommand(this,nearbyBezier));
                didSomething = true;
            }
            break;
        case SMOOTH_POINTS:

            if (nearbyCP.first) {
                pushUndoCommand(new SmoothCuspUndoCommand(this,nearbyBezier,nearbyCP,time,false));
                didSomething = true;
            }
            break;
        case CUSP_POINTS:
            if (nearbyCP.first && _imp->context->isAutoKeyingEnabled()) {
                pushUndoCommand(new SmoothCuspUndoCommand(this,nearbyBezier,nearbyCP,time,true));
                didSomething = true;
            }
            break;
        case DRAW_BEZIER:
        {
            if (_imp->rotoData->builtBezier && _imp->rotoData->builtBezier->isCurveFinished()) {
                _imp->rotoData->builtBezier.reset();
                _imp->clearSelection();
                onToolActionTriggered(_imp->selectAllAction);
                return true;
            }
            if (_imp->rotoData->builtBezier) {
                ///if the user clicked on a control point of the bezier, select the point instead.
                ///if that point is the starting point of the curve, close the curve
                const std::list<boost::shared_ptr<BezierCP> >& cps = _imp->rotoData->builtBezier->getControlPoints();
                int i = 0;
                for (std::list<boost::shared_ptr<BezierCP> >::const_iterator it = cps.begin(); it!=cps.end(); ++it,++i) {
                    double x,y;
                    (*it)->getPositionAtTime(time, &x, &y);
                    if (x >= (pos.x() - cpSelectionTolerance) && x <= (pos.x() + cpSelectionTolerance) &&
                        y >= (pos.y() - cpSelectionTolerance) && y <= (pos.y() + cpSelectionTolerance)) {
                        if (it == cps.begin()) {
                            pushUndoCommand(new OpenCloseUndoCommand(this,_imp->rotoData->builtBezier));
                            
                            _imp->rotoData->builtBezier.reset();
                            
                            _imp->rotoData->selectedCps.clear();
                            onToolActionTriggered(_imp->selectAllAction);
                            
                            
                        } else {
                            boost::shared_ptr<BezierCP> fp = _imp->rotoData->builtBezier->getFeatherPointAtIndex(i);
                            assert(fp);
                            _imp->handleControlPointSelection(std::make_pair(*it, fp));
                            _imp->state = DRAGGING_CPS;
                        }
                        
                        return true;
                    }
                }

            }
            MakeBezierUndoCommand* cmd = new MakeBezierUndoCommand(this,_imp->rotoData->builtBezier,true,pos.x(),pos.y(),time);
            pushUndoCommand(cmd);
            _imp->rotoData->builtBezier = cmd->getCurve();
            assert(_imp->rotoData->builtBezier);
            _imp->state = BUILDING_BEZIER_CP_TANGENT;
            didSomething = true;
        }   break;
        case DRAW_B_SPLINE:
            
            break;
        case DRAW_ELLIPSE:
        {
            bool fromCenter = _imp->modifiers.testFlag(Natron::ControlModifier);
            pushUndoCommand(new MakeEllipseUndoCommand(this,true,fromCenter,pos.x(),pos.y(),time));
            if (fromCenter) {
                _imp->state = BULDING_ELLIPSE_CENTER;
            } else {
                _imp->state = BUILDING_ELLIPSE;
            }
            didSomething = true;
            
        }   break;
        case DRAW_RECTANGLE:
        {
            pushUndoCommand(new MakeRectangleUndoCommand(this,true,pos.x(),pos.y(),time));
            _imp->evaluateOnPenUp = true;
            _imp->state = BUILDING_RECTANGLE;
            didSomething = true;
        }   break;
        default:
            assert(false);
            break;
    }
    
    _imp->lastClickPos = pos;
    _imp->lastMousePos = pos;
    return didSomething;
}

bool RotoGui::penMotion(double /*scaleX*/,double /*scaleY*/,const QPointF& /*viewportPos*/,const QPointF& pos)
{
    std::pair<double, double> pixelScale;
    _imp->viewer->getPixelScale(pixelScale.first, pixelScale.second);
    
    int time = _imp->context->getTimelineCurrentTime();
    ///Set the cursor to the appropriate case
    bool cursorSet = false;
    if (_imp->rotoData->selectedCps.size() > 1 && _imp->isNearbySelectedCpsCrossHair(pos)) {
        _imp->viewer->setCursor(QCursor(Qt::SizeAllCursor));
        cursorSet = true;
    } else {
        double cpTol = kControlPointSelectionTolerance * pixelScale.first;
        
        if (_imp->state != DRAGGING_CPS) {
            for (SelectedBeziers::const_iterator it = _imp->rotoData->selectedBeziers.begin(); it!=_imp->rotoData->selectedBeziers.end(); ++it) {
                int index = -1;
                std::pair<boost::shared_ptr<BezierCP>,boost::shared_ptr<BezierCP> > nb =
                (*it)->isNearbyControlPoint(pos.x(), pos.y(), cpTol,Bezier::WHATEVER_FIRST,&index);
                if (index != -1) {
                    _imp->viewer->setCursor(QCursor(Qt::CrossCursor));
                    cursorSet = true;
                    break;
                }
            }
        }
        if (!cursorSet && _imp->state != DRAGGING_LEFT_TANGENT && _imp->state != DRAGGING_RIGHT_TANGENT) {
            ///find a nearby tangent
            for (SelectedCPs::const_iterator it = _imp->rotoData->selectedCps.begin(); it!=_imp->rotoData->selectedCps.end(); ++it) {
                if (it->first->isNearbyTangent(time, pos.x(), pos.y(), cpTol) != -1) {
                    _imp->viewer->setCursor(QCursor(Qt::CrossCursor));
                    cursorSet = true;
                    break;
                }
            }
        }
    }
    if (!cursorSet) {
        _imp->viewer->setCursor(QCursor(Qt::ArrowCursor));
    }
    
    
    double dx = pos.x() - _imp->lastMousePos.x();
    double dy = pos.y() - _imp->lastMousePos.y();
    bool didSomething = false;
    switch (_imp->state) {
        case DRAGGING_CPS:
        {
            pushUndoCommand(new MoveControlPointsUndoCommand(this,dx,dy,time));
            _imp->evaluateOnPenUp = true;
            _imp->computeSelectedCpsBBOX();
            didSomething = true;
        }   break;
        case SELECTING:
        {
            _imp->refreshSelectionRectangle(pos);
            didSomething = true;
        }   break;
        case BUILDING_BEZIER_CP_TANGENT:
        {
            assert(_imp->rotoData->builtBezier);
            pushUndoCommand(new MakeBezierUndoCommand(this,_imp->rotoData->builtBezier,false,dx,dy,time));
            didSomething = true;
        }   break;
        case BUILDING_ELLIPSE:
        {
            pushUndoCommand(new MakeEllipseUndoCommand(this,false,false,dx,dy,time));
            
            didSomething = true;
            _imp->evaluateOnPenUp = true;
        }   break;
        case BULDING_ELLIPSE_CENTER:
        {
            pushUndoCommand(new MakeEllipseUndoCommand(this,false,true,dx,dy,time));
            _imp->evaluateOnPenUp = true;
            didSomething = true;
        }   break;
        case BUILDING_RECTANGLE:
        {
            pushUndoCommand(new MakeRectangleUndoCommand(this,false,dx,dy,time));
            didSomething = true;
            _imp->evaluateOnPenUp = true;
        }   break;
        case DRAGGING_LEFT_TANGENT:
        {
            assert(_imp->rotoData->tangentBeingDragged);
            pushUndoCommand(new MoveTangentUndoCommand(this,dx,dy,time,_imp->rotoData->tangentBeingDragged,true));
            _imp->evaluateOnPenUp = true;
            didSomething = true;
        }   break;
        case DRAGGING_RIGHT_TANGENT:
        {
            assert(_imp->rotoData->tangentBeingDragged);
            pushUndoCommand(new MoveTangentUndoCommand(this,dx,dy,time,_imp->rotoData->tangentBeingDragged,false));
            _imp->evaluateOnPenUp = true;
            didSomething = true;
        }   break;
        case DRAGGING_FEATHER_BAR:
        {
            pushUndoCommand(new MoveFeatherBarUndoCommand(this,dx,dy,_imp->rotoData->featherBarBeingDragged,time));
            _imp->evaluateOnPenUp = true;
            didSomething = true;
        }   break;
        case NONE:
        default:
            break;
    }
    _imp->lastMousePos = pos;
    return didSomething;
}

void RotoGui::evaluate(bool redraw)
{
    if (redraw) {
        _imp->viewer->redraw();
    }
    _imp->context->evaluateChange();
    _imp->node->getNode()->getApp()->triggerAutoSave();
    _imp->viewerTab->onRotoEvaluatedForThisViewer();
}

void RotoGui::autoSaveAndRedraw()
{
    _imp->viewer->redraw();
    _imp->node->getNode()->getApp()->triggerAutoSave();
}

bool RotoGui::penUp(double /*scaleX*/,double /*scaleY*/,const QPointF& /*viewportPos*/,const QPointF& /*pos*/)
{
    if (_imp->state == SELECTING) {
        _imp->updateSelectionFromSelectionRectangle();
    }
    
    if (_imp->evaluateOnPenUp) {
        _imp->context->evaluateChange();
        _imp->node->getNode()->getApp()->triggerAutoSave();
        _imp->viewerTab->onRotoEvaluatedForThisViewer();
        _imp->evaluateOnPenUp = false;
    }
    _imp->rotoData->tangentBeingDragged.reset();
    _imp->rotoData->featherBarBeingDragged.first.reset();
    _imp->rotoData->featherBarBeingDragged.second.reset();
    _imp->state = NONE;
    
    if (_imp->selectedTool == DRAW_ELLIPSE || _imp->selectedTool == DRAW_RECTANGLE) {
        _imp->rotoData->selectedCps.clear();
        onToolActionTriggered(_imp->selectAllAction);
    }
    
    return true;
}

void RotoGui::removeCurve(Bezier* curve)
{
    if (curve == _imp->rotoData->builtBezier.get()) {
        _imp->rotoData->builtBezier.reset();
    }
    _imp->context->removeItem(curve);
}

bool RotoGui::keyDown(double /*scaleX*/,double /*scaleY*/,QKeyEvent* e)
{
    bool didSomething = false;
    _imp->modifiers = QtEnumConvert::fromQtModifiers(e->modifiers());
    if (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) {
        ///if control points are selected, delete them, otherwise delete the selected beziers
        if (!_imp->rotoData->selectedCps.empty()) {
            pushUndoCommand(new RemovePointUndoCommand(this,_imp->rotoData->selectedCps));
            didSomething = true;
        } else if (!_imp->rotoData->selectedBeziers.empty()) {
            pushUndoCommand(new RemoveCurveUndoCommand(this,_imp->rotoData->selectedBeziers));
            didSomething = true;
        }
        
    } else if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        if (_imp->selectedTool == DRAW_BEZIER && _imp->rotoData->builtBezier && !_imp->rotoData->builtBezier->isCurveFinished()) {
            pushUndoCommand(new OpenCloseUndoCommand(this,_imp->rotoData->builtBezier));
            _imp->rotoData->builtBezier.reset();
            _imp->rotoData->selectedCps.clear();
             onToolActionTriggered(_imp->selectAllAction);
            _imp->context->evaluateChange();
            didSomething = true;
        }
    } else if (e->key() == Qt::Key_A && e->modifiers().testFlag(Qt::ControlModifier)) {
        ///if no bezier are selected, select all beziers
        if (_imp->rotoData->selectedBeziers.empty()) {
            std::list<boost::shared_ptr<Bezier> > bez = _imp->context->getCurvesByRenderOrder();
            for (std::list<boost::shared_ptr<Bezier> >::const_iterator it = bez.begin(); it!=bez.end(); ++it) {
                _imp->context->select(*it,RotoContext::OVERLAY_INTERACT);
                _imp->rotoData->selectedBeziers.push_back(*it);
            }
        } else {
            ///select all the control points of all selected beziers
            _imp->rotoData->selectedCps.clear();
            for (SelectedBeziers::iterator it = _imp->rotoData->selectedBeziers.begin(); it!=_imp->rotoData->selectedBeziers.end(); ++it) {
                const std::list<boost::shared_ptr<BezierCP> >& cps = (*it)->getControlPoints();
                const std::list<boost::shared_ptr<BezierCP> >& fps = (*it)->getFeatherPoints();
                assert(cps.size() == fps.size());
                
                std::list<boost::shared_ptr<BezierCP> >::const_iterator cpIT = cps.begin();
                for (std::list<boost::shared_ptr<BezierCP> >::const_iterator fpIT = fps.begin(); fpIT != fps.end(); ++fpIT, ++ cpIT) {
                    _imp->rotoData->selectedCps.push_back(std::make_pair(*cpIT, *fpIT));
                }
            }
            _imp->computeSelectedCpsBBOX();
        }
        didSomething = true;
    } else if (e->key() == Qt::Key_Q) {
        _imp->selectTool->handleSelection();
    } else if (e->key() == Qt::Key_V) {
        _imp->bezierEditionTool->handleSelection();
    } else if (e->key() == Qt::Key_D) {
        _imp->pointsEditionTool->handleSelection();
    }
    
    return didSomething;
}

bool RotoGui::keyUp(double /*scaleX*/,double /*scaleY*/,QKeyEvent* e)
{
    _imp->modifiers = QtEnumConvert::fromQtModifiers(e->modifiers());
    if (_imp->evaluateOnKeyUp) {
        _imp->context->evaluateChange();
        _imp->node->getNode()->getApp()->triggerAutoSave();
        _imp->viewerTab->onRotoEvaluatedForThisViewer();
        _imp->evaluateOnKeyUp = false;
    }
    return false;
}

bool RotoGui::RotoGuiPrivate::isNearbySelectedCpsCrossHair(const QPointF& pos) const
{
    
    std::pair<double, double> pixelScale;
    viewer->getPixelScale(pixelScale.first,pixelScale.second);

    double xHairMidSizeX = kXHairSelectedCpsBox * pixelScale.first;
    double xHairMidSizeY = kXHairSelectedCpsBox * pixelScale.second;
    
    double l = rotoData->selectedCpsBbox.topLeft().x();
    double r = rotoData->selectedCpsBbox.bottomRight().x();
    double b = rotoData->selectedCpsBbox.bottomRight().y();
    double t = rotoData->selectedCpsBbox.topLeft().y();
    
    double toleranceX = kXHairSelectedCpsTolerance * pixelScale.first;
    double toleranceY = kXHairSelectedCpsTolerance * pixelScale.second;

    double midX = (l + r) / 2.;
    double midY = (b + t) / 2.;
    
    double lCross = midX - xHairMidSizeX;
    double rCross = midX + xHairMidSizeX;
    double bCross = midY - xHairMidSizeY;
    double tCross = midY + xHairMidSizeY;
    
    if (pos.x() >= (lCross - toleranceX) &&
        pos.x() <= (rCross + toleranceX) &&
        pos.y() <= (tCross + toleranceY) &&
        pos.y() >= (bCross - toleranceY)) {
        return true;
    } else {
        return false;
    }
}

std::pair<boost::shared_ptr<BezierCP>,boost::shared_ptr<BezierCP> >
RotoGui::RotoGuiPrivate::isNearbyFeatherBar(int time,const std::pair<double,double>& pixelScale,const QPointF& pos) const
{
    double distFeatherX = 20. * pixelScale.first;

    double acceptance = 10 * pixelScale.second;
    
    for (SelectedBeziers::const_iterator it = rotoData->selectedBeziers.begin(); it!=rotoData->selectedBeziers.end(); ++it) {
        const std::list<boost::shared_ptr<BezierCP> >& fps = (*it)->getFeatherPoints();
        const std::list<boost::shared_ptr<BezierCP> >& cps = (*it)->getControlPoints();
        int cpCount = (int)cps.size();
        if (cpCount <= 1) {
            continue;
        }
        
        std::list<Point> polygon;
        RectD polygonBBox(INT_MAX,INT_MAX,INT_MIN,INT_MIN);
        (*it)->evaluateFeatherPointsAtTime_DeCasteljau(time, 0, 50, &polygon, true, &polygonBBox);
        std::vector<double> constants(polygon.size()),multipliers(polygon.size());
        Bezier::precomputePointInPolygonTables(polygon, &constants, &multipliers);
    
        std::list<boost::shared_ptr<BezierCP> >::const_iterator itF = fps.begin();
        std::list<boost::shared_ptr<BezierCP> >::const_iterator nextF = itF;
        ++nextF;
        std::list<boost::shared_ptr<BezierCP> >::const_iterator prevF = fps.end();
        --prevF;
        std::list<boost::shared_ptr<BezierCP> >::const_iterator itCp = cps.begin();

        for (;itCp != cps.end(); ++itF,++nextF,++prevF,++itCp) {
            
            if (prevF == fps.end()) {
                prevF = fps.begin();
            }
            if (nextF == fps.end()) {
                nextF = fps.begin();
            }
            
            Point controlPoint,featherPoint;
            (*itCp)->getPositionAtTime(time, &controlPoint.x, &controlPoint.y);
            (*itF)->getPositionAtTime(time, &featherPoint.x, &featherPoint.y);
            
            Bezier::expandToFeatherDistance(controlPoint, &featherPoint, distFeatherX, polygon, constants,
                                            multipliers, polygonBBox, time, prevF, itF, nextF);
            assert(featherPoint.x != controlPoint.x || featherPoint.y != controlPoint.y);
            
            if (((pos.y() >= (controlPoint.y - acceptance) && pos.y() <= (featherPoint.y + acceptance)) ||
                 (pos.y() >= (featherPoint.y - acceptance) && pos.y() <= (controlPoint.y + acceptance))) &&
                ((pos.x() >= (controlPoint.x - acceptance) && pos.x() <= (featherPoint.x + acceptance)) ||
                 (pos.x() >= (featherPoint.x - acceptance) && pos.x() <= (controlPoint.x + acceptance)))) {
                    Point a;
                    a.x = (featherPoint.x - controlPoint.x);
                    a.y = (featherPoint.y - controlPoint.y);
                    double norm = sqrt(a.x * a.x + a.y * a.y);
                    
                    ///The point is in the bounding box of the segment, if it is vertical it must be on the segment anyway
                    if (norm == 0) {
                        return std::make_pair(*itCp, *itF);
                    }
                    
                    a.x /= norm;
                    a.y /= norm;
                    Point b;
                    b.x = (pos.x() - controlPoint.x);
                    b.y = (pos.y() - controlPoint.y);
                    norm = sqrt(b.x * b.x + b.y * b.y);
                    
                    ///This vector is not vertical
                    if (norm != 0) {
                        
                        b.x /= norm;
                        b.y /= norm;
                        
                        double crossProduct = b.y * a.x - b.x * a.y;
                        if (std::abs(crossProduct) <  0.1) {
                            return std::make_pair(*itCp, *itF);
                        }
                    }
                   
                }
            
        }
        
    }

    return std::make_pair(boost::shared_ptr<BezierCP>(), boost::shared_ptr<BezierCP>());
}

void RotoGui::onAutoKeyingButtonClicked(bool e)
{
    _imp->autoKeyingEnabled->setDown(e);
    _imp->context->onAutoKeyingChanged(e);
}

void RotoGui::onFeatherLinkButtonClicked(bool e)
{
    _imp->featherLinkEnabled->setDown(e);
    _imp->context->onFeatherLinkChanged(e);
}

void RotoGui::onRippleEditButtonClicked(bool e)
{
    _imp->rippleEditEnabled->setDown(e);
    _imp->context->onRippleEditChanged(e);
}

void RotoGui::onStickySelectionButtonClicked(bool e)
{
    _imp->stickySelectionEnabled->setDown(e);
}

bool RotoGui::isStickySelectionEnabled() const
{
    return _imp->stickySelectionEnabled->isChecked();
}

void RotoGui::onAddKeyFrameClicked()
{
    int time = _imp->context->getTimelineCurrentTime();
    for (SelectedBeziers::iterator it = _imp->rotoData->selectedBeziers.begin(); it!=_imp->rotoData->selectedBeziers.end(); ++it) {
        (*it)->setKeyframe(time);
    }
}

void RotoGui::onRemoveKeyFrameClicked()
{
    int time = _imp->context->getTimelineCurrentTime();
    for (SelectedBeziers::iterator it = _imp->rotoData->selectedBeziers.begin(); it!=_imp->rotoData->selectedBeziers.end(); ++it) {
        (*it)->removeKeyframe(time);
    }
}

void RotoGui::onCurrentFrameChanged(SequenceTime /*time*/,int)
{
    _imp->computeSelectedCpsBBOX();
}

void RotoGui::restoreSelectionFromContext()
{
    _imp->rotoData->selectedBeziers = _imp->context->getSelectedCurves();
}

void RotoGui::onRefreshAsked()
{
    _imp->viewer->redraw();
}

void RotoGui::RotoGuiPrivate::onCurveLockedChangedRecursive(const boost::shared_ptr<RotoItem>& item,bool* ret)
{
    boost::shared_ptr<Bezier> b = boost::dynamic_pointer_cast<Bezier>(item);
    boost::shared_ptr<RotoLayer> layer = boost::dynamic_pointer_cast<RotoLayer>(item);
    if (b) {
        if (item->getLocked()) {
            for (SelectedBeziers::iterator fb = rotoData->selectedBeziers.begin(); fb != rotoData->selectedBeziers.end(); ++fb) {
                if (fb->get() == b.get()) {
                    rotoData->selectedBeziers.erase(fb);
                    *ret = true;
                    break;
                }
            }
        } else {
            ///Explanation: This change has been made in result to a user click on the settings panel.
            ///We have to reselect the bezier overlay hence put a reason different of OVERLAY_INTERACT
            SelectedBeziers::iterator found = std::find(rotoData->selectedBeziers.begin(),rotoData->selectedBeziers.end(),b);
            if (found == rotoData->selectedBeziers.end()) {
                rotoData->selectedBeziers.push_back(b);
                context->select(b, RotoContext::SETTINGS_PANEL);
                *ret  = true;
            }
        }
        

    } else if (layer) {
        const std::list<boost::shared_ptr<RotoItem> >& items = layer->getItems();
        for (std::list<boost::shared_ptr<RotoItem> >::const_iterator it = items.begin(); it != items.end(); ++it) {
            onCurveLockedChangedRecursive(*it, ret);
        }
    }
}

void RotoGui::onCurveLockedChanged()
{
    boost::shared_ptr<RotoItem> item = _imp->context->getLastItemLocked();
    assert(item);
    bool changed = false;
    if (item) {
        _imp->onCurveLockedChangedRecursive(item, &changed);
    }
    if (changed) {
        _imp->viewer->redraw();
    }
    
}

void RotoGui::onSelectionChanged(int reason)
{
    if ((RotoContext::SelectionReason)reason != RotoContext::OVERLAY_INTERACT) {
 
        _imp->rotoData->selectedBeziers = _imp->context->getSelectedCurves();
        _imp->viewer->redraw();
    }
}

void RotoGui::setSelection(const std::list<boost::shared_ptr<Bezier> >& selectedBeziers,
                  const std::list<std::pair<boost::shared_ptr<BezierCP> ,boost::shared_ptr<BezierCP> > >& selectedCps)
{
    _imp->rotoData->selectedBeziers.clear();
    for (SelectedBeziers::const_iterator it = selectedBeziers.begin(); it!= selectedBeziers.end(); ++it) {
        if (*it) {
            _imp->rotoData->selectedBeziers.push_back(*it);
        }
    }
    _imp->rotoData->selectedCps.clear();
    for (SelectedCPs::const_iterator it = selectedCps.begin(); it!=selectedCps.end(); ++it) {
        if (it->first && it->second) {
            _imp->rotoData->selectedCps.push_back(*it);
        }
    }
    _imp->context->select(_imp->rotoData->selectedBeziers,RotoContext::OVERLAY_INTERACT);
    _imp->computeSelectedCpsBBOX();
}

void RotoGui::setSelection(const boost::shared_ptr<Bezier>& curve,
                  const std::pair<boost::shared_ptr<BezierCP> ,boost::shared_ptr<BezierCP> >& point)
{
    _imp->rotoData->selectedBeziers.clear();
    if (curve) {
        _imp->rotoData->selectedBeziers.push_back(curve);
    }
    _imp->rotoData->selectedCps.clear();
    if (point.first && point.second) {
        _imp->rotoData->selectedCps.push_back(point);
    }
    if (curve) {
        _imp->context->select(curve, RotoContext::OVERLAY_INTERACT);
    }
    _imp->computeSelectedCpsBBOX();
}

void RotoGui::getSelection(std::list<boost::shared_ptr<Bezier> >* selectedBeziers,
                  std::list<std::pair<boost::shared_ptr<BezierCP> ,boost::shared_ptr<BezierCP> > >* selectedCps)
{
    *selectedBeziers = _imp->rotoData->selectedBeziers;
    *selectedCps = _imp->rotoData->selectedCps;
}

void RotoGui::setBuiltBezier(const boost::shared_ptr<Bezier>& curve)
{
    assert(curve);
    _imp->rotoData->builtBezier = curve;
}

boost::shared_ptr<Bezier> RotoGui::getBezierBeingBuild() const
{
    return  _imp->rotoData->builtBezier;
}

void RotoGui::pushUndoCommand(QUndoCommand* cmd)
{
    NodeSettingsPanel* panel = _imp->node->getSettingPanel();
    assert(panel);
    panel->pushUndoCommand(cmd);
}

QString RotoGui::getNodeName() const
{
    return _imp->node->getNode()->getName().c_str();
}

RotoContext* RotoGui::getContext()
{
    return _imp->context.get();
}