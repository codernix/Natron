//  Natron
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
 * contact: immarespond at gmail dot com
 *
 */

// from <https://docs.python.org/3/c-api/intro.html#include-files>:
// "Since Python may define some pre-processor definitions which affect the standard headers on some systems, you must include Python.h before any standard headers are included."
#include <Python.h>

#include "ProjectGui.h"

#include <fstream>

CLANG_DIAG_OFF(deprecated)
CLANG_DIAG_OFF(uninitialized)
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSplitter>
#include <QTimer>
#include <QDebug>
#include <QTextDocument> // for Qt::convertFromPlainText
CLANG_DIAG_ON(deprecated)
CLANG_DIAG_ON(uninitialized)


#include "Engine/Project.h"
#include "Engine/ViewerInstance.h"
#include "Engine/KnobTypes.h"
#include "Engine/EffectInstance.h"
#include "Engine/Node.h"
#include "Engine/Settings.h"
#include "Engine/BackDrop.h"

#include "Gui/GuiApplicationManager.h"
#include "Gui/Gui.h"
#include "Gui/ComboBox.h"
#include "Gui/Button.h"
#include "Gui/LineEdit.h"
#include "Gui/SpinBox.h"
#include "Gui/ViewerTab.h"
#include "Gui/ViewerGL.h"
#include "Gui/DockablePanel.h"
#include "Gui/NodeGui.h"
#include "Gui/TabWidget.h"
#include "Gui/ProjectGuiSerialization.h"
#include "Gui/GuiAppInstance.h"
#include "Gui/NodeGraph.h"
#include "Gui/Splitter.h"
#include "Gui/Histogram.h"
#include "Gui/MultiInstancePanel.h"
#include "Gui/CurveEditor.h"
#include "Gui/BackDropGui.h"
#include "Gui/ScriptEditor.h"
#include "Gui/PythonPanels.h"

ProjectGui::ProjectGui(Gui* gui)
    : _gui(gui)
      , _project()
      , _panel(NULL)
      , _created(false)
      , _colorPickersEnabled()
{
}

ProjectGui::~ProjectGui()
{
    
}

void
ProjectGui::initializeKnobsGui()
{
    assert(_panel);
    _panel->initializeKnobs();
}

void
ProjectGui::create(boost::shared_ptr<Natron::Project> projectInternal,
                   QVBoxLayout* container,
                   QWidget* parent)

{
    _project = projectInternal;

    QObject::connect( projectInternal.get(),SIGNAL( mustCreateFormat() ),this,SLOT( createNewFormat() ) );
    QObject::connect( projectInternal.get(),SIGNAL( knobsInitialized() ),this,SLOT( initializeKnobsGui() ) );

    _panel = new DockablePanel(_gui,
                               projectInternal.get(),
                               container,
                               DockablePanel::eHeaderModeReadOnlyName,
                               false,
                               tr("Project Settings"),
                               tr("The settings of the current project."),
                               false,
                               tr("Settings"),
                               parent);


    _created = true;
}

bool
ProjectGui::isVisible() const
{
    return _panel->isVisible();
}

void
ProjectGui::setVisible(bool visible)
{
    _panel->setVisible(visible);
}

void
ProjectGui::createNewFormat()
{
    boost::shared_ptr<Natron::Project> project = _project.lock();
    AddFormatDialog dialog( project.get(),_gui->getApp()->getGui() );

    if ( dialog.exec() ) {
        project->setOrAddProjectFormat( dialog.getFormat() );
    }
}

AddFormatDialog::AddFormatDialog(Natron::Project *project,
                                 Gui* gui)
    : QDialog(gui),
      _gui(gui),
      _project(project)
{
    _mainLayout = new QVBoxLayout(this);
    _mainLayout->setSpacing(0);
    _mainLayout->setContentsMargins(5, 5, 0, 0);
    setLayout(_mainLayout);
    setWindowTitle( tr("New Format") );

    _fromViewerLine = new QWidget(this);
    _fromViewerLineLayout = new QHBoxLayout(_fromViewerLine);
    _fromViewerLine->setLayout(_fromViewerLineLayout);

    _copyFromViewerCombo = new ComboBox(_fromViewerLine);
    
    project->getViewers(&_viewers);
    

    for (std::list<ViewerInstance*>::iterator it = _viewers.begin(); it != _viewers.end(); ++it) {
        _copyFromViewerCombo->addItem( (*it)->getNode()->getLabel().c_str() );
    }
    _fromViewerLineLayout->addWidget(_copyFromViewerCombo);

    _copyFromViewerButton = new Button(tr("Copy from"),_fromViewerLine);
    _copyFromViewerButton->setToolTip( Qt::convertFromPlainText(
                                           tr("Fill the new format with the currently"
                                              " displayed region of definition of the viewer"
                                              " indicated on the left."), Qt::WhiteSpaceNormal) );
    QObject::connect( _copyFromViewerButton,SIGNAL( clicked() ),this,SLOT( onCopyFromViewer() ) );
    _mainLayout->addWidget(_fromViewerLine);

    _fromViewerLineLayout->addWidget(_copyFromViewerButton);
    _parametersLine = new QWidget(this);
    _parametersLineLayout = new QHBoxLayout(_parametersLine);
    _mainLayout->addWidget(_parametersLine);

    _widthLabel = new QLabel("w:",_parametersLine);
    _parametersLineLayout->addWidget(_widthLabel);
    _widthSpinBox = new SpinBox(this,SpinBox::eSpinBoxTypeInt);
    _widthSpinBox->setMaximum(99999);
    _widthSpinBox->setMinimum(1);
    _widthSpinBox->setValue(1);
    _parametersLineLayout->addWidget(_widthSpinBox);


    _heightLabel = new QLabel("h:",_parametersLine);
    _parametersLineLayout->addWidget(_heightLabel);
    _heightSpinBox = new SpinBox(this,SpinBox::eSpinBoxTypeInt);
    _heightSpinBox->setMaximum(99999);
    _heightSpinBox->setMinimum(1);
    _heightSpinBox->setValue(1);
    _parametersLineLayout->addWidget(_heightSpinBox);


    _pixelAspectLabel = new QLabel(tr("pixel aspect:"),_parametersLine);
    _parametersLineLayout->addWidget(_pixelAspectLabel);
    _pixelAspectSpinBox = new SpinBox(this,SpinBox::eSpinBoxTypeDouble);
    _pixelAspectSpinBox->setMinimum(0.);
    _pixelAspectSpinBox->setValue(1.);
    _parametersLineLayout->addWidget(_pixelAspectSpinBox);


    _formatNameLine = new QWidget(this);
    _formatNameLayout = new QHBoxLayout(_formatNameLine);
    _formatNameLine->setLayout(_formatNameLayout);
    _mainLayout->addWidget(_formatNameLine);


    _nameLabel = new QLabel(tr("Name:"),_formatNameLine);
    _formatNameLayout->addWidget(_nameLabel);
    _nameLineEdit = new LineEdit(_formatNameLine);
    _formatNameLayout->addWidget(_nameLineEdit);

    _buttonsLine = new QWidget(this);
    _buttonsLineLayout = new QHBoxLayout(_buttonsLine);
    _buttonsLine->setLayout(_buttonsLineLayout);
    _mainLayout->addWidget(_buttonsLine);


    _cancelButton = new Button(tr("Cancel"),_buttonsLine);
    QObject::connect( _cancelButton, SIGNAL( clicked() ), this, SLOT( reject() ) );
    _buttonsLineLayout->addWidget(_cancelButton);

    _okButton = new Button(tr("Ok"),_buttonsLine);
    QObject::connect( _okButton, SIGNAL( clicked() ), this, SLOT( accept() ) );
    _buttonsLineLayout->addWidget(_okButton);
}

void
AddFormatDialog::onCopyFromViewer()
{
    QString activeText = _copyFromViewerCombo->itemText( _copyFromViewerCombo->activeIndex() );

    for (std::list<ViewerInstance*>::iterator it = _viewers.begin(); it != _viewers.end(); ++it) {
        if ( (*it)->getNode()->getLabel() == activeText.toStdString() ) {
            ViewerTab* tab = _gui->getViewerTabForInstance(*it);
            RectD f = tab->getViewer()->getRoD(0);
            Format format = tab->getViewer()->getDisplayWindow();
            _widthSpinBox->setValue( f.width() );
            _heightSpinBox->setValue( f.height() );
            _pixelAspectSpinBox->setValue(format.getPixelAspectRatio());
        }
    }
}

Format
AddFormatDialog::getFormat() const
{
    int w = (int)_widthSpinBox->value();
    int h = (int)_heightSpinBox->value();
    double pa = _pixelAspectSpinBox->value();
    QString name = _nameLineEdit->text();

    return Format(0,0,w,h,name.toStdString(),pa);
}

void
ProjectGui::save(boost::archive::xml_oarchive & archive) const
{
    ProjectGuiSerialization projectGuiSerializationObj;

    projectGuiSerializationObj.initialize(this);
    archive << boost::serialization::make_nvp("ProjectGui",projectGuiSerializationObj);
}

void
ProjectGui::load(boost::archive::xml_iarchive & archive)
{
    ProjectGuiSerialization obj;

    archive >> boost::serialization::make_nvp("ProjectGui",obj);

    const std::map<std::string, ViewerData > & viewersProjections = obj.getViewersProjections();

    int leftBound,rightBound;
    _project.lock()->getFrameRange(&leftBound, &rightBound);

    
    ///default color for nodes
    float defR,defG,defB;
    boost::shared_ptr<Settings> settings = appPTR->getCurrentSettings();
    const std::list<NodeGuiSerialization> & nodesGuiSerialization = obj.getSerializedNodesGui();
    for (std::list<NodeGuiSerialization>::const_iterator it = nodesGuiSerialization.begin(); it != nodesGuiSerialization.end(); ++it) {
        const std::string & name = it->getFullySpecifiedName();
        boost::shared_ptr<Natron::Node> internalNode = _gui->getApp()->getProject()->getNodeByFullySpecifiedName(name);
        if (!internalNode) {
            continue;
        }
        
        boost::shared_ptr<NodeGuiI> nGui_i = internalNode->getNodeGui();
        assert(nGui_i);
        boost::shared_ptr<NodeGui> nGui = boost::dynamic_pointer_cast<NodeGui>(nGui_i);
        
        nGui->refreshPosition( it->getX(),it->getY(), true );

        if ( ( it->isPreviewEnabled() && !nGui->getNode()->isPreviewEnabled() ) ||
             ( !it->isPreviewEnabled() && nGui->getNode()->isPreviewEnabled() ) ) {
            nGui->togglePreview();
        }

        Natron::EffectInstance* iseffect = nGui->getNode()->getLiveInstance();

        if ( it->colorWasFound() ) {
            std::list<std::string> grouping;
            iseffect->getPluginGrouping(&grouping);
            std::string majGroup = grouping.empty() ? "" : grouping.front();

            BackDropGui* isBd = dynamic_cast<BackDropGui*>(nGui.get());

            if ( iseffect->isReader() ) {
                settings->getReaderColor(&defR, &defG, &defB);
            } else if ( iseffect->isWriter() ) {
                settings->getWriterColor(&defR, &defG, &defB);
            } else if ( iseffect->isGenerator() ) {
                settings->getGeneratorColor(&defR, &defG, &defB);
            } else if (majGroup == PLUGIN_GROUP_COLOR) {
                settings->getColorGroupColor(&defR, &defG, &defB);
            } else if (majGroup == PLUGIN_GROUP_FILTER) {
                settings->getFilterGroupColor(&defR, &defG, &defB);
            } else if (majGroup == PLUGIN_GROUP_CHANNEL) {
                settings->getChannelGroupColor(&defR, &defG, &defB);
            } else if (majGroup == PLUGIN_GROUP_KEYER) {
                settings->getKeyerGroupColor(&defR, &defG, &defB);
            } else if (majGroup == PLUGIN_GROUP_MERGE) {
                settings->getMergeGroupColor(&defR, &defG, &defB);
            } else if (majGroup == PLUGIN_GROUP_PAINT) {
                settings->getDrawGroupColor(&defR, &defG, &defB);
            } else if (majGroup == PLUGIN_GROUP_TIME) {
                settings->getTimeGroupColor(&defR, &defG, &defB);
            } else if (majGroup == PLUGIN_GROUP_TRANSFORM) {
                settings->getTransformGroupColor(&defR, &defG, &defB);
            } else if (majGroup == PLUGIN_GROUP_MULTIVIEW) {
                settings->getViewsGroupColor(&defR, &defG, &defB);
            } else if (majGroup == PLUGIN_GROUP_DEEP) {
                settings->getDeepGroupColor(&defR, &defG, &defB);
            } else if (isBd) {
                settings->getDefaultBackDropColor(&defR, &defG, &defB);
            } else {
                settings->getDefaultNodeColor(&defR, &defG, &defB);
            }


            float r,g,b;
            it->getColor(&r, &g, &b);
            ///restore color only if different from default.
            if ( (std::abs(r - defR) > 0.05) || (std::abs(g - defG) > 0.05) || (std::abs(b - defB) > 0.05) ) {
                QColor color;
                color.setRgbF(r, g, b);
                nGui->setCurrentColor(color);
            }
            
            double ovR,ovG,ovB;
            bool hasOverlayColor = it->getOverlayColor(&ovR,&ovG,&ovB);
            if (hasOverlayColor) {
                QColor c;
                c.setRgbF(ovR, ovG, ovB);
                nGui->setOverlayColor(c);
            }
            
            if (isBd) {
                double w,h;
                it->getSize(&w, &h);
                isBd->resize(w, h, true);
            }
        }
        
        ViewerInstance* viewer = dynamic_cast<ViewerInstance*>( nGui->getNode()->getLiveInstance() );
        if (viewer) {
            std::map<std::string, ViewerData >::const_iterator found = viewersProjections.find(name);
            if ( found != viewersProjections.end() ) {
                ViewerTab* tab = _gui->getApp()->getGui()->getViewerTabForInstance(viewer);
                tab->getViewer()->setProjection(found->second.zoomLeft, found->second.zoomBottom, found->second.zoomFactor, 1.);
                tab->setChannels(found->second.channels);
                tab->setColorSpace(found->second.colorSpace);
                tab->setGain(found->second.gain);
                tab->setUserRoIEnabled(found->second.userRoIenabled);
                tab->setAutoContrastEnabled(found->second.autoContrastEnabled);
                tab->setUserRoI(found->second.userRoI);
                tab->setClipToProject(found->second.isClippedToProject);
                tab->setRenderScaleActivated(found->second.renderScaleActivated);
                tab->setMipMapLevel(found->second.mipMapLevel);
                tab->setCompositingOperator( (Natron::ViewerCompositingOperatorEnum)found->second.wipeCompositingOp );
                tab->setZoomOrPannedSinceLastFit(found->second.zoomOrPanSinceLastFit);
                tab->setTopToolbarVisible(found->second.topToolbarVisible);
                tab->setLeftToolbarVisible(found->second.leftToolbarVisible);
                tab->setRightToolbarVisible(found->second.rightToolbarVisible);
                tab->setPlayerVisible(found->second.playerVisible);
                tab->setInfobarVisible(found->second.infobarVisible);
                tab->setTimelineVisible(found->second.timelineVisible);
                tab->setCheckerboardEnabled(found->second.checkerboardEnabled);
                tab->setTimelineBounds(found->second.leftBound, found->second.rightBound);
                if (found->second.version >= VIEWER_DATA_REMOVES_FRAME_RANGE_LOCK) {
                    tab->setFrameRangeEdited(leftBound != found->second.leftBound || rightBound != found->second.rightBound);
                } else {
                    tab->setTimelineBounds(leftBound, rightBound);
                    tab->setFrameRangeEdited(false);
                }
                if (!found->second.fpsLocked) {
                    tab->setDesiredFps(found->second.fps);
                }
                tab->setFPSLocked(found->second.fpsLocked);
            }
        }

        if ( it->isSelected() ) {
            _gui->getNodeGraph()->selectNode(nGui, true);
        }
    }


    const std::list<boost::shared_ptr<NodeGui> > nodesGui = getVisibleNodes();
    for (std::list<boost::shared_ptr<NodeGui> >::const_iterator it = nodesGui.begin(); it != nodesGui.end(); ++it) {
        (*it)->refreshEdges();
        (*it)->refreshKnobLinks();
    }
    
    ///now restore the backdrops from old version prior to Natron 1.1
    const std::list<NodeBackDropSerialization> & backdrops = obj.getBackdrops();
    for (std::list<NodeBackDropSerialization>::const_iterator it = backdrops.begin(); it != backdrops.end(); ++it) {
        
        double x,y;
        it->getPos(x, y);
        int w,h;
        it->getSize(w, h);
        
        boost::shared_ptr<KnobI> labelSerialization = it->getLabelSerialization();
        
        CreateNodeArgs args(PLUGINID_NATRON_BACKDROP,
                            "",
                            -1,
                            -1,
                            false,
                            x,
                            y,
                            false,
                            true,
                            QString(),
                            CreateNodeArgs::DefaultValuesList(),
                            _project.lock());
        boost::shared_ptr<Natron::Node> node = getGui()->getApp()->createNode(args);
        boost::shared_ptr<NodeGuiI> gui_i = node->getNodeGui();
        assert(gui_i);
        BackDropGui* bd = dynamic_cast<BackDropGui*>(gui_i.get());
        assert(bd);
        bd->resize(w,h);
        String_Knob* iStr = dynamic_cast<String_Knob*>(labelSerialization.get());
        assert(iStr);
        if (iStr) {
            bd->onLabelChanged(iStr->getValue().c_str());
        }
        float r,g,b;
        it->getColor(r, g, b);
        QColor c;
        c.setRgbF(r,g,b);
        bd->setCurrentColor(c);
        node->setLabel(it->getFullySpecifiedName());
    }

     _gui->getApp()->updateProjectLoadStatus(QObject::tr("Restoring settings panels"));
    
    ///now restore opened settings panels
    const std::list<std::string> & openedPanels = obj.getOpenedPanels();
    //reverse the iterator to fill the layout bottom up
    for (std::list<std::string>::const_reverse_iterator it = openedPanels.rbegin(); it != openedPanels.rend(); ++it) {
        if (*it == kNatronProjectSettingsPanelSerializationName) {
            _gui->setVisibleProjectSettingsPanel();
        } else {
            
            NodePtr node = getInternalProject()->getNodeByFullySpecifiedName(*it);
            if (node) {
                boost::shared_ptr<NodeGuiI> nodeGui_i = node->getNodeGui();
                assert(nodeGui_i);
                NodeGui* nodeGui = dynamic_cast<NodeGui*>(nodeGui_i.get());
                assert(nodeGui);
                NodeSettingsPanel* panel = nodeGui->getSettingPanel();
                if (panel) {
                    nodeGui->setVisibleSettingsPanel(true);
                }
            }
        }
    }
    
    
    
    ///restore user python panels
    const std::list<boost::shared_ptr<PythonPanelSerialization> >& pythonPanels = obj.getPythonPanels();
    if (!pythonPanels.empty()) {
        _gui->getApp()->updateProjectLoadStatus(QObject::tr("Restoring user panels"));
    }

    if (!pythonPanels.empty()) {
        std::string appID = _gui->getApp()->getAppIDString();
        std::string err;
        bool ok = Natron::interpretPythonScript("app = " + appID + "\n", &err, 0);
        assert(ok);
    }
    for (std::list<boost::shared_ptr<PythonPanelSerialization> >::const_iterator it = pythonPanels.begin(); it != pythonPanels.end(); ++it) {
        std::string script = (*it)->pythonFunction + "()\n";
        std::string err,output;
        if (!Natron::interpretPythonScript(script, &err, &output)) {
            _gui->getApp()->appendToScriptEditor(err);
        } else {
            if (!output.empty()) {
                _gui->getApp()->appendToScriptEditor(output);
            }
        }
        const RegisteredTabs& registeredTabs = _gui->getRegisteredTabs();
        RegisteredTabs::const_iterator found = registeredTabs.find((*it)->name);
        if (found != registeredTabs.end()) {
            PyPanel* panel = dynamic_cast<PyPanel*>(found->second.first);
            if (panel) {
                panel->restore((*it)->userData);
                for (std::list<boost::shared_ptr<KnobSerialization> >::iterator it2 = (*it)->knobs.begin(); it2!=(*it)->knobs.end(); ++it2) {
                    Param* param = panel->getParam((*it2)->getName());
                    if (param) {
                        param->getInternalKnob()->clone((*it2)->getKnob());
                        delete param;
                    }
                }
            }
        }
        
    }
    
    _gui->getApp()->updateProjectLoadStatus(QObject::tr("Restoring layout"));
    
    _gui->restoreLayout( true,obj.getVersion() < PROJECT_GUI_SERIALIZATION_MAJOR_OVERHAUL,obj.getGuiLayout() );

    ///restore the histograms
    const std::list<std::string> & histograms = obj.getHistograms();
    for (std::list<std::string>::const_iterator it = histograms.begin(); it != histograms.end(); ++it) {
        Histogram* h = _gui->addNewHistogram();
        h->setObjectName( (*it).c_str() );
        //move it by default to the viewer pane, before restoring the layout anyway which
        ///will relocate it correctly
        _gui->appendTabToDefaultViewerPane(h,h);
    }
    
    if (obj.getVersion() < PROJECT_GUI_SERIALIZATION_NODEGRAPH_ZOOM_TO_POINT) {
        _gui->getNodeGraph()->clearSelection();
    }
    
    _gui->getScriptEditor()->setInputScript(obj.getInputScript().c_str());
    _gui->centerAllNodeGraphsWithTimer();
} // load

std::list<boost::shared_ptr<NodeGui> > ProjectGui::getVisibleNodes() const
{
    return _gui->getVisibleNodes_mt_safe();
}

void
ProjectGui::registerNewColorPicker(boost::shared_ptr<Color_Knob> knob)
{
    for (std::vector<boost::shared_ptr<Color_Knob> >::iterator it = _colorPickersEnabled.begin(); it != _colorPickersEnabled.end(); ++it) {
        (*it)->setPickingEnabled(false);
    }
    _colorPickersEnabled.clear();
    _colorPickersEnabled.push_back(knob);
}

void
ProjectGui::removeColorPicker(boost::shared_ptr<Color_Knob> knob)
{
    std::vector<boost::shared_ptr<Color_Knob> >::iterator found = std::find(_colorPickersEnabled.begin(), _colorPickersEnabled.end(), knob);

    if ( found != _colorPickersEnabled.end() ) {
        _colorPickersEnabled.erase(found);
    }
}

void
ProjectGui::setPickersColor(const QColor & color)
{
    if ( _colorPickersEnabled.empty() ) {
        return;
    }
    boost::shared_ptr<Color_Knob> first = _colorPickersEnabled.front();

    for (U32 i = 0; i < _colorPickersEnabled.size(); ++i) {
        double r,g,b,a;
        r = color.redF();
        g = color.greenF();
        b = color.blueF();
        a = color.alphaF();
        if ( !_colorPickersEnabled[i]->areAllDimensionsEnabled() ) {
            _colorPickersEnabled[i]->activateAllDimensions();
        }
        if (_colorPickersEnabled[i]->getDimension() == 3) {
            _colorPickersEnabled[i]->setValues(r, g, b);
        } else {
            _colorPickersEnabled[i]->setValues(r, g, b,a);
        }
    }
}

