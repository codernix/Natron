//  Natron
//
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
//
//  Created by Frédéric Devernay on 03/09/13.
//
//
#include "OfxHost.h"

#include <cassert>
#include <fstream>
#include <new> // std::bad_alloc
#include <stdexcept> // std::exception
#include <cctype> // tolower
#include <algorithm> // transform
#include <string>
#include <QtCore/QDir>
#include <QtCore/QMutex>
#include <QtCore/QThreadPool>
#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>

#ifdef OFX_SUPPORTS_MULTITHREAD
#include <QtCore/QThread>
#include <QtCore/QThreadStorage>
#include <QtConcurrentMap>
#include <boost/bind.hpp>
#endif

//ofx
#include <ofxParametricParam.h>

//ofx host support
#include <ofxhPluginAPICache.h>
#include <ofxhImageEffect.h>
#include <ofxhImageEffectAPI.h>
#include <ofxhHost.h>
#include <ofxhParam.h>

#include <tuttle/ofxReadWrite.h>

//our version of parametric param suite support
#include "ofxhParametricParam.h"

#include "Engine/AppManager.h"
#include "Engine/LibraryBinary.h"

#include "Engine/OfxEffectInstance.h"
#include "Engine/OfxImageEffectInstance.h"
#include "Engine/KnobTypes.h"
#include "Engine/Plugin.h"
#include "Engine/StandardPaths.h"
#include "Engine/Settings.h"

using namespace Natron;

Natron::OfxHost::OfxHost()
:_imageEffectPluginCache(new OFX::Host::ImageEffect::PluginCache(*this))
{
    _properties.setStringProperty(kOfxPropName,NATRON_ORGANIZATION_DOMAIN_TOPLEVEL "." NATRON_ORGANIZATION_DOMAIN_SUB "." NATRON_APPLICATION_NAME); //"uk.co.thefoundry.nuke" //< use this to pass for nuke
    _properties.setStringProperty(kOfxPropLabel, NATRON_APPLICATION_NAME); // "nuke" //< use this to pass for nuke
    _properties.setIntProperty(kOfxPropAPIVersion, 1 , 0); //API v1.0
    _properties.setIntProperty(kOfxPropAPIVersion, 0 , 1);
    _properties.setIntProperty(kOfxPropVersion, NATRON_VERSION_MAJOR , 0);
    _properties.setIntProperty(kOfxPropVersion, NATRON_VERSION_MINOR , 1);
	_properties.setIntProperty(kOfxPropVersion, NATRON_VERSION_REVISION , 0);
    _properties.setStringProperty(kOfxPropVersionLabel, NATRON_VERSION_STRING);
    _properties.setIntProperty(kOfxImageEffectHostPropIsBackground, 0);
    _properties.setIntProperty(kOfxImageEffectPropSupportsOverlays, 1);
    _properties.setIntProperty(kOfxImageEffectPropSupportsMultiResolution, 1);
    _properties.setIntProperty(kOfxImageEffectPropSupportsTiles, 1);
    _properties.setIntProperty(kOfxImageEffectPropTemporalClipAccess, 1);
    _properties.setStringProperty(kOfxImageEffectPropSupportedComponents,  kOfxImageComponentRGBA, 0);
    _properties.setStringProperty(kOfxImageEffectPropSupportedContexts, kOfxImageEffectContextGenerator, 0 );
    _properties.setStringProperty(kOfxImageEffectPropSupportedContexts, kOfxImageEffectContextFilter, 1);
    _properties.setStringProperty(kOfxImageEffectPropSupportedContexts, kOfxImageEffectContextGeneral, 2 );
    _properties.setStringProperty(kOfxImageEffectPropSupportedContexts, kOfxImageEffectContextTransition, 3 );
    
    _properties.setStringProperty(kOfxImageEffectPropSupportedPixelDepths,kOfxBitDepthFloat,0);
    _properties.setIntProperty(kOfxImageEffectPropSupportsMultipleClipDepths, 0); 
    _properties.setIntProperty(kOfxImageEffectPropSupportsMultipleClipPARs, 0);
    _properties.setIntProperty(kOfxImageEffectPropSetableFrameRate, 0);
    _properties.setIntProperty(kOfxImageEffectPropSetableFielding, 0);
    _properties.setIntProperty(kOfxParamHostPropSupportsCustomInteract, 1 );
    _properties.setIntProperty(kOfxParamHostPropSupportsStringAnimation, String_Knob::canAnimateStatic());
    _properties.setIntProperty(kOfxParamHostPropSupportsChoiceAnimation, Choice_Knob::canAnimateStatic());
    _properties.setIntProperty(kOfxParamHostPropSupportsBooleanAnimation, Bool_Knob::canAnimateStatic());
    _properties.setIntProperty(kOfxParamHostPropSupportsCustomAnimation, String_Knob::canAnimateStatic());
    _properties.setIntProperty(kOfxParamHostPropMaxParameters, -1);
    _properties.setIntProperty(kOfxParamHostPropMaxPages, 0);
    _properties.setIntProperty(kOfxParamHostPropPageRowColumnCount, 0, 0 );
    _properties.setIntProperty(kOfxParamHostPropPageRowColumnCount, 0, 1 );
    _properties.setIntProperty(kOfxImageEffectInstancePropSequentialRender, 0);
    _properties.setIntProperty(kOfxParamHostPropSupportsParametricAnimation, 0);

}

Natron::OfxHost::~OfxHost()
{
    //Clean up, to be polite.
    OFX::Host::PluginCache::clearPluginCache();
    delete _imageEffectPluginCache;
}

OFX::Host::ImageEffect::Instance* Natron::OfxHost::newInstance(void* ,
                                                                OFX::Host::ImageEffect::ImageEffectPlugin* plugin,
                                                                OFX::Host::ImageEffect::Descriptor& desc,
                                                                const std::string& context)
{
    assert(plugin);
    
    
    
    return new Natron::OfxImageEffectInstance(plugin,desc,context,false);
}

/// Override this to create a descriptor, this makes the 'root' descriptor
OFX::Host::ImageEffect::Descriptor *Natron::OfxHost::makeDescriptor(OFX::Host::ImageEffect::ImageEffectPlugin* plugin)
{
    assert(plugin);
    OFX::Host::ImageEffect::Descriptor *desc = new OFX::Host::ImageEffect::Descriptor(plugin);
    return desc;
}

/// used to construct a context description, rootContext is the main context
OFX::Host::ImageEffect::Descriptor *Natron::OfxHost::makeDescriptor(const OFX::Host::ImageEffect::Descriptor &rootContext,
                                                                     OFX::Host::ImageEffect::ImageEffectPlugin *plugin)
{
    assert(plugin);
    OFX::Host::ImageEffect::Descriptor *desc = new OFX::Host::ImageEffect::Descriptor(rootContext, plugin);
    return desc;
}

/// used to construct populate the cache
OFX::Host::ImageEffect::Descriptor *Natron::OfxHost::makeDescriptor(const std::string &bundlePath,
                                                                     OFX::Host::ImageEffect::ImageEffectPlugin *plugin)
{
    assert(plugin);
    OFX::Host::ImageEffect::Descriptor *desc = new OFX::Host::ImageEffect::Descriptor(bundlePath, plugin);
    return desc;
}



/// message
OfxStatus Natron::OfxHost::vmessage(const char* msgtype,
                                     const char* /*id*/,
                                     const char* format,
                                     va_list args)
{
    assert(msgtype);
    assert(format);
    char buf[10000];
    sprintf(buf, format,args);
    std::string message(buf);
    
    std::string type(msgtype);

    if (type == kOfxMessageLog) {
#pragma message WARN("Log in a log buffer, not on stdout!")
        std::cout << message << std::endl;
    } else if (type == kOfxMessageFatal || type == kOfxMessageError) {
        Natron::errorDialog(NATRON_APPLICATION_NAME, message);
    } else if (type == kOfxMessageWarning) {
        Natron::warningDialog(NATRON_APPLICATION_NAME, message);
    } else if (type == kOfxMessageMessage) {
        Natron::informationDialog(NATRON_APPLICATION_NAME, message);
    } else if (type == kOfxMessageQuestion) {
        if (Natron::questionDialog(NATRON_APPLICATION_NAME, message) == Natron::Yes) {
            return kOfxStatReplyYes;
        } else {
            return kOfxStatReplyNo;
        }
    }
    return kOfxStatReplyDefault;
}

OfxStatus Natron::OfxHost::setPersistentMessage(const char* type,
                                                 const char* id,
                                                 const char* format,
                                                 va_list args){
    vmessage(type,id,format,args);
    return kOfxStatOK;
}

/// clearPersistentMessage
OfxStatus Natron::OfxHost::clearPersistentMessage(){
    return kOfxStatOK;
}

void Natron::OfxHost::getPluginAndContextByID(const std::string& pluginID,  OFX::Host::ImageEffect::ImageEffectPlugin** plugin,std::string& context) {
    // throws out_of_range if the plugin does not exist
    const OFXPluginEntry& ofxPlugin = _ofxPlugins.at(pluginID);

    *plugin = _imageEffectPluginCache->getPluginById(ofxPlugin.openfxId);
    if (!(*plugin)) {
        throw std::runtime_error(std::string("Error: Could not get plugin ") + ofxPlugin.openfxId);
    }


    OFX::Host::PluginHandle *pluginHandle;
    // getPluginHandle() must be called before getContexts():
    // it calls kOfxActionLoad on the plugin, which may set properties (including supported contexts)
    try {
        pluginHandle = (*plugin)->getPluginHandle();
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Error: Could not get plugin handle for plugin ") + pluginID + ": " + e.what());
    } catch (...) {
        throw std::runtime_error(std::string("Error: Could not get plugin handle for plugin ") + pluginID);
    }
    if(!pluginHandle) {
        throw std::runtime_error(std::string("Error: Could not get plugin handle for plugin ") + pluginID);
    }
    assert(pluginHandle->getOfxPlugin() && pluginHandle->getOfxPlugin()->mainEntry);

    const std::set<std::string>& contexts = (*plugin)->getContexts();

    if (contexts.size() == 0) {
        throw std::runtime_error(std::string("Error: Plugins supports no context"));
        //context = kOfxImageEffectContextGeneral;
        //plugin->addContext(kOfxImageEffectContextGeneral);
    } else if (contexts.size() == 1) {
        context = (*contexts.begin());
    } else {

        std::set<std::string>::iterator found = contexts.find(kOfxImageEffectContextReader);
        if (found != contexts.end()) {
            context = *found;
        } else {
            found = contexts.find(kOfxImageEffectContextWriter);
            if (found != contexts.end()) {
                context = *found;
            } else {
                found = contexts.find(kOfxImageEffectContextGeneral);
                if(found != contexts.end()){
                    context = *found;
                }else{
                    found = contexts.find(kOfxImageEffectContextFilter);
                    if(found != contexts.end()){
                        context = *found;
                    }else{
                        found = contexts.find(kOfxImageEffectContextGenerator);
                        if(found != contexts.end()){
                            context = *found;
                        }else{
                            found = contexts.find(kOfxImageEffectContextTransition);
                            if(found != contexts.end()){
                                context = *found;
                            }else{
                                context = kOfxImageEffectContextPaint;
                            }
                        }
                    }
                }
            }
        }
    }
}

AbstractOfxEffectInstance* Natron::OfxHost::createOfxEffect(const std::string& name,Natron::Node* node,
                                                            const NodeSerialization* serialization ) {

    assert(node);
    OFX::Host::ImageEffect::ImageEffectPlugin *plugin;
    std::string context;

    getPluginAndContextByID(name,&plugin,context);

    AbstractOfxEffectInstance* hostSideEffect = new OfxEffectInstance(node);
    hostSideEffect->createOfxImageEffectInstance(plugin, context,serialization);
    return hostSideEffect;
}

void Natron::OfxHost::addPathToLoadOFXPlugins(const std::string path) {
    OFX::Host::PluginCache::getPluginCache()->addFileToPath(path);
}


void Natron::OfxHost::loadOFXPlugins(std::vector<Natron::Plugin*>* plugins,
                                     std::map<std::string,std::vector<std::string> >* readersMap,
                                     std::map<std::string,std::vector<std::string> >* writersMap) {
    
    assert(OFX::Host::PluginCache::getPluginCache());
    /// set the version label in the global cache
    OFX::Host::PluginCache::getPluginCache()->setCacheVersion(NATRON_APPLICATION_NAME "OFXCachev1");
        
    /// register the image effect cache with the global plugin cache
    _imageEffectPluginCache->registerInCache(*OFX::Host::PluginCache::getPluginCache());
    
    
#if defined(WINDOWS)
    OFX::Host::PluginCache::getPluginCache()->addFileToPath("C:\\Program Files\\Common Files\\OFX\\Nuke");
    OFX::Host::PluginCache::getPluginCache()->addFileToPath("C:\\Program Files (x86)\\Common Files\\OFX");
#endif
#if defined(__linux__)
    OFX::Host::PluginCache::getPluginCache()->addFileToPath("/usr/OFX/Nuke");
#endif
#if defined(__APPLE__)
    OFX::Host::PluginCache::getPluginCache()->addFileToPath("/Library/OFX/Nuke");
#endif
    
    QStringList extraPluginsSearchPaths = appPTR->getCurrentSettings()->getPluginsExtraSearchPaths();
    for (int i = 0; i < extraPluginsSearchPaths.size(); ++i) {
		std::string path = extraPluginsSearchPaths.at(i).toStdString();
		if (!path.empty()) {
			OFX::Host::PluginCache::getPluginCache()->addFileToPath(path);
		}
    }
    
    QDir dir(QCoreApplication::applicationDirPath());
    dir.cdUp();
    OFX::Host::PluginCache::getPluginCache()->addFileToPath(QString(dir.absolutePath()+  "/Plugins").toStdString());
    
    /// now read an old cache
    // The cache location depends on the OS.
    // On OSX, it will be ~/Library/Caches/<organization>/<application>/OFXCache.xml
    //on Linux ~/.cache/<organization>/<application>/OFXCache.xml
    //on windows:
    QString ofxcachename = Natron::StandardPaths::writableLocation(Natron::StandardPaths::CacheLocation) + QDir::separator() + "OFXCache.xml";

    std::ifstream ifs(ofxcachename.toStdString().c_str());
    if (ifs.is_open()) {
        OFX::Host::PluginCache::getPluginCache()->readCache(ifs);
        ifs.close();
    }
    OFX::Host::PluginCache::getPluginCache()->scanPluginFiles();

    // write the cache NOW (it won't change anyway)
    /// flush out the current cache
    writeOFXCache();

    /*Filling node name list and plugin grouping*/
    const std::vector<OFX::Host::ImageEffect::ImageEffectPlugin *>& ofxPlugins = _imageEffectPluginCache->getPlugins();
    for (unsigned int i = 0 ; i < ofxPlugins.size(); ++i) {
        OFX::Host::ImageEffect::ImageEffectPlugin* p = ofxPlugins[i];
        assert(p);
        if(p->getContexts().size() == 0)
            continue;
       
        QString openfxId = p->getIdentifier().c_str();
        const std::string& grouping = p->getDescriptor().getPluginGrouping();
        
        const std::string& bundlePath = p->getBinary()->getBundlePath();
        const std::string& pluginLabel = OfxEffectInstance::getPluginLabel(p->getDescriptor().getShortLabel(),
                                                                    p->getDescriptor().getLabel(),
                                                                    p->getDescriptor().getLongLabel());
     
        
        std::string pluginId = OfxEffectInstance::generateImageEffectClassName(p->getDescriptor().getShortLabel(),
                                                                               p->getDescriptor().getLabel(),
                                                                               p->getDescriptor().getLongLabel(),
                                                                           grouping);

       
        
        QStringList groups = OfxEffectInstance::getPluginGrouping(pluginLabel, grouping);
        
        assert(p->getBinary());
        QString iconFilename = QString(bundlePath.c_str()) + "/Contents/Resources/";
        iconFilename.append(p->getDescriptor().getProps().getStringProperty(kOfxPropIcon,1).c_str());
        iconFilename.append(openfxId);
        iconFilename.append(".png");
        QString groupIconFilename;
        if (groups.size() > 0) {
            groupIconFilename = QString(p->getBinary()->getBundlePath().c_str()) + "/Contents/Resources/";
            groupIconFilename.append(p->getDescriptor().getProps().getStringProperty(kOfxPropIcon,1).c_str());
            groupIconFilename.append(groups[0]);
            groupIconFilename.append(".png");
        }
        
        _ofxPlugins[pluginId] = OFXPluginEntry(openfxId.toStdString(), grouping);

        emit toolButtonAdded(groups,pluginId.c_str(), pluginLabel.c_str(), iconFilename, groupIconFilename);
        QMutex* pluginMutex = NULL;
        if(p->getDescriptor().getRenderThreadSafety() == kOfxImageEffectRenderUnsafe){
            pluginMutex = new QMutex(QMutex::Recursive);
        }
        Natron::Plugin* plugin = new Natron::Plugin(new Natron::LibraryBinary(Natron::LibraryBinary::BUILTIN),
                                                    pluginId.c_str(),pluginLabel.c_str(),pluginMutex,p->getVersionMajor(),
                                                    p->getVersionMinor());
        plugins->push_back(plugin);
        
        
        ///if this plugin's descriptor has the kTuttleOfxImageEffectPropSupportedExtensions property,
        ///use it to fill the readersMap and writersMap
        int formatsCount = p->getDescriptor().getProps().getDimension(kTuttleOfxImageEffectPropSupportedExtensions);
        std::vector<std::string> formats(formatsCount);
        for (int k = 0; k < formatsCount; ++k) {
            formats[k] = p->getDescriptor().getProps().getStringProperty(kTuttleOfxImageEffectPropSupportedExtensions,k);
            std::transform(formats[k].begin(), formats[k].end(), formats[k].begin(), ::tolower);
        }
        

        const std::set<std::string>& contexts = p->getContexts();
        std::set<std::string>::const_iterator foundReader = contexts.find(kOfxImageEffectContextReader);
        std::set<std::string>::const_iterator foundWriter = contexts.find(kOfxImageEffectContextWriter);

        
        if(foundReader != contexts.end() && formatsCount > 0 && readersMap){
            ///we're safe to assume that this plugin is a reader
            for(U32 k = 0; k < formats.size();++k){
                std::map<std::string,std::vector<std::string> >::iterator it;
                it = readersMap->find(formats[k]);
                
                if(it != readersMap->end()){
                    it->second.push_back(pluginId);
                }else{
                    std::vector<std::string> newVec(1);
                    newVec[0] = pluginId;
                    readersMap->insert(std::make_pair(formats[k], newVec));
                }
            }
        }else if(foundWriter != contexts.end() && formatsCount > 0 && writersMap){
            ///we're safe to assume that this plugin is a writer.
            for(U32 k = 0; k < formats.size();++k){
                std::map<std::string,std::vector<std::string> >::iterator it;
                it = writersMap->find(formats[k]);
                
                if(it != writersMap->end()){
                    it->second.push_back(pluginId);
                }else{
                    std::vector<std::string> newVec(1);
                    newVec[0] = pluginId;
                    writersMap->insert(std::make_pair(formats[k], newVec));
                }
            }

        }

    }
}



void Natron::OfxHost::writeOFXCache(){
    /// and write a new cache, long version with everything in there
    QString ofxcachename = Natron::StandardPaths::writableLocation(Natron::StandardPaths::CacheLocation);
    QDir().mkpath(ofxcachename);
    ofxcachename +=  QDir::separator();
    ofxcachename += "OFXCache.xml";
    std::ofstream of(ofxcachename.toStdString().c_str());
    assert(of.is_open());
    assert(OFX::Host::PluginCache::getPluginCache());
    OFX::Host::PluginCache::getPluginCache()->writePluginCache(of);
    of.close();
}

void Natron::OfxHost::clearPluginsLoadedCache() {

    QString ofxcachename = Natron::StandardPaths::writableLocation(Natron::StandardPaths::CacheLocation);

    QDir().mkpath(ofxcachename);
    ofxcachename +=  QDir::separator();
    ofxcachename += "OFXCache.xml";
    
    if (QFile::exists(ofxcachename)) {
        QFile::remove(ofxcachename);
    }
}

void Natron::OfxHost::loadingStatus(const std::string & pluginId) {
    if (appPTR) {
        appPTR->setLoadingStatus("OpenFX: " + QString(pluginId.c_str()));
    }
}

bool Natron::OfxHost::pluginSupported(OFX::Host::ImageEffect::ImageEffectPlugin *plugin, std::string &reason) const
{
    // check that the plugin supports kOfxBitDepthFloat
    if (plugin->getDescriptor().getParamSetProps().findStringPropValueIndex(kOfxImageEffectPropSupportedPixelDepths, kOfxBitDepthFloat) == -1) {
        reason = "32-bits floating-point bit depth not supported by plugin";
        return false;
    }

    return true;
}

void* Natron::OfxHost::fetchSuite(const char *suiteName, int suiteVersion) {
    if (strcmp(suiteName, kOfxParametricParameterSuite)==0  && suiteVersion == 1) {
        return OFX::Host::ParametricParam::GetSuite(suiteVersion);
    }else{
        return OFX::Host::ImageEffect::Host::fetchSuite(suiteName, suiteVersion);
    }
}

/////////////////
/////////////////////////////////////////////////// MULTI_THREAD SUITE ///////////////////////////////////////////////////
/////////////////


#ifdef OFX_SUPPORTS_MULTITHREAD

///Stored as int, because we need -1
static QThreadStorage<int> gThreadIndex;

namespace {
    
    static OfxStatus threadFunctionWrapper(OfxThreadFunctionV1 func,
                                           unsigned int threadIndex,
                                           unsigned int threadMax,
                                           void *customArg) {
        assert(threadIndex < threadMax);
        gThreadIndex.localData() = (int)threadIndex;
        OfxStatus ret = kOfxStatOK;
        try {
            func(threadIndex, threadMax, customArg);
        } catch (const std::bad_alloc& ba) {
            ret =  kOfxStatErrMemory;
        } catch (...) {
            ret =  kOfxStatFailed;
        }
        ///reset back the index otherwise it could mess up the indexes if the same thread is re-used
        gThreadIndex.localData() = -1;
        return ret;

    }
}


// Function to spawn SMP threads
//  This function will spawn nThreads separate threads of computation (typically one per CPU) to allow something to perform symmetric multi processing. Each thread will call 'func' passing in the index of the thread and the number of threads actually launched.
// multiThread will not return until all the spawned threads have returned. It is up to the host how it waits for all the threads to return (busy wait, blocking, whatever).
// nThreads can be more than the value returned by multiThreadNumCPUs, however the threads will be limitted to the number of CPUs returned by multiThreadNumCPUs.
// This function cannot be called recursively.
// Note that the thread indexes are from 0 to nThreads-1.
// http://openfx.sourceforge.net/Documentation/1.3/ofxProgrammingReference.html#OfxMultiThreadSuiteV1_multiThread

OfxStatus Natron::OfxHost::multiThread(OfxThreadFunctionV1 func,unsigned int nThreads, void *customArg)
{
   
    
    if (!func) {
        return kOfxStatFailed;
    }
    
    unsigned int maxConcurrentThread;
    OfxStatus st = multiThreadNumCPUS(&maxConcurrentThread);
    if (st != kOfxStatOK) {
        return st;
    }

    // from the documentation:
    // "nThreads can be more than the value returned by multiThreadNumCPUs, however
    // the threads will be limitted to the number of CPUs returned by multiThreadNumCPUs."

    if (nThreads == 1 || appPTR->getCurrentSettings()->getNumberOfThreads() == -1) {
        try {
            for (unsigned int i = 0; i < nThreads; ++i) {
                func(i, nThreads, customArg);
            }
            return kOfxStatOK;
        } catch (...) {
            return kOfxStatFailed;
        }
    }
    
    // check that this thread does not already have an ID
    if (gThreadIndex.hasLocalData() && (gThreadIndex.localData() != -1)) {
        return kOfxStatErrExists;
    }
    
    std::vector<unsigned int> threadIndexes(nThreads);
    for (unsigned int i = 0; i < nThreads; ++i) {
        threadIndexes[i] = i;
    }
    
    /// DON'T set the maximum thread count, this is a global application setting, and see the documentation excerpt above
    //QThreadPool::globalInstance()->setMaxThreadCount(nThreads);
    QFuture<OfxStatus> future = QtConcurrent::mapped(threadIndexes, boost::bind(::threadFunctionWrapper,func, _1, nThreads, customArg));
    future.waitForFinished();
    ///DON'T reset back to the original value the maximum thread count
    //QThreadPool::globalInstance()->setMaxThreadCount(QThread::idealThreadCount());
    
    for (QFuture<OfxStatus>::const_iterator it = future.begin() ; it!= future.end(); ++it) {
        OfxStatus stat = *it;
        if (stat != kOfxStatOK) {
            return stat;
        }
    }
    return kOfxStatOK;
}

// Function which indicates the number of CPUs available for SMP processing
//  This value may be less than the actual number of CPUs on a machine, as the host may reserve other CPUs for itself.
// http://openfx.sourceforge.net/Documentation/1.3/ofxProgrammingReference.html#OfxMultiThreadSuiteV1_multiThreadNumCPUs
OfxStatus Natron::OfxHost::multiThreadNumCPUS(unsigned int *nCPUs) const
{
    if (!nCPUs) {
        return kOfxStatFailed;
    }
    if (appPTR->getCurrentSettings()->getNumberOfThreads() == -1) {
        *nCPUs = 1;
    } else {
        int activeThreadsCount = QThreadPool::globalInstance()->activeThreadCount();
        // better than QThread::idealThreadCount();, because it can be set by a global preference:
        int maxThreadsCount = QThreadPool::globalInstance()->maxThreadCount();
        *nCPUs = std::max(1, maxThreadsCount - activeThreadsCount);
    }

    return kOfxStatOK;
}

// Function which indicates the index of the current thread
//  This function returns the thread index, which is the same as the threadIndex argument passed to the OfxThreadFunctionV1.
// If there are no threads currently spawned, then this function will set threadIndex to 0
// http://openfx.sourceforge.net/Documentation/1.3/ofxProgrammingReference.html#OfxMultiThreadSuiteV1_multiThreadIndex
// Note that the thread indexes are from 0 to nThreads-1, so a return value of 0 does not mean that it's not a spawned thread
// (use multiThreadIsSpawnedThread() to check if it's a spawned thread)
OfxStatus Natron::OfxHost::multiThreadIndex(unsigned int *threadIndex) const
{
    if (!threadIndex)
        return kOfxStatFailed;

    if (gThreadIndex.hasLocalData()) {
        if (gThreadIndex.localData() == -1) {
            *threadIndex = 0;
        } else {
            *threadIndex = gThreadIndex.localData();
        }
    } else {
        *threadIndex = 0;
    }
    return kOfxStatOK;
}

// Function to enquire if the calling thread was spawned by multiThread
// http://openfx.sourceforge.net/Documentation/1.3/ofxProgrammingReference.html#OfxMultiThreadSuiteV1_multiThreadIsSpawnedThread
int Natron::OfxHost::multiThreadIsSpawnedThread() const
{
    return gThreadIndex.hasLocalData() && gThreadIndex.localData() != -1;
}

// Create a mutex
//  Creates a new mutex with lockCount locks on the mutex initially set.
// http://openfx.sourceforge.net/Documentation/1.3/ofxProgrammingReference.html#OfxMultiThreadSuiteV1_mutexCreate
OfxStatus Natron::OfxHost::mutexCreate(OfxMutexHandle *mutex, int lockCount)
{
    if (!mutex) {
        return kOfxStatFailed;
    }
    
    // suite functions should not throw
    try {
        QMutex* m = new QMutex(QMutex::Recursive);
        for (int i = 0; i < lockCount; ++i) {
            m->lock();
        }
        *mutex = (OfxMutexHandle)(m);
        return kOfxStatOK;
    } catch (std::bad_alloc) {
        qDebug() << "mutexCreate(): memory error.";
        return kOfxStatErrMemory;
    } catch ( const std::exception& e ) {
        qDebug() << "mutexCreate(): " << e.what();
        return kOfxStatErrUnknown;
    } catch ( ... ) {
        qDebug() << "mutexCreate(): unknown error.";
        return kOfxStatErrUnknown;
    }
}

// Destroy a mutex
//  Destroys a mutex intially created by mutexCreate.
// http://openfx.sourceforge.net/Documentation/1.3/ofxProgrammingReference.html#OfxMultiThreadSuiteV1_mutexDestroy
OfxStatus Natron::OfxHost::mutexDestroy(const OfxMutexHandle mutex)
{
    if (mutex == 0) {
        return kOfxStatErrBadHandle;
    }
    // suite functions should not throw
    try {
        delete reinterpret_cast<const QMutex*>(mutex);
        return kOfxStatOK;
    } catch (std::bad_alloc) {
        qDebug() << "mutexDestroy(): memory error.";
        return kOfxStatErrMemory;
    } catch ( const std::exception& e ) {
        qDebug() << "mutexDestroy(): " << e.what();
        return kOfxStatErrUnknown;
    } catch ( ... ) {
        qDebug() << "mutexDestroy(): unknown error.";
        return kOfxStatErrUnknown;
    }
}

// Blocking lock on the mutex
//  This trys to lock a mutex and blocks the thread it is in until the lock suceeds.
// A sucessful lock causes the mutex's lock count to be increased by one and to block any other calls to lock the mutex until it is unlocked.
// http://openfx.sourceforge.net/Documentation/1.3/ofxProgrammingReference.html#OfxMultiThreadSuiteV1_mutexLock
OfxStatus Natron::OfxHost::mutexLock(const OfxMutexHandle mutex)
{
    if (mutex == 0) {
        return kOfxStatErrBadHandle;
    }
    // suite functions should not throw
    try {
        reinterpret_cast<QMutex*>(mutex)->lock();
        return kOfxStatOK;
    } catch (std::bad_alloc) {
        qDebug() << "mutexLock(): memory error.";
        return kOfxStatErrMemory;
    } catch ( const std::exception& e ) {
        qDebug() << "mutexLock(): " << e.what();
        return kOfxStatErrUnknown;
    } catch ( ... ) {
        qDebug() << "mutexLock(): unknown error.";
        return kOfxStatErrUnknown;
    }
}

// Unlock the mutex
//  This unlocks a mutex. Unlocking a mutex decreases its lock count by one.
// http://openfx.sourceforge.net/Documentation/1.3/ofxProgrammingReference.html#OfxMultiThreadSuiteV1_mutexUnLock
OfxStatus Natron::OfxHost::mutexUnLock(const OfxMutexHandle mutex)
{
    if (mutex == 0) {
        return kOfxStatErrBadHandle;
    }
    // suite functions should not throw
    try {
        reinterpret_cast<QMutex*>(mutex)->unlock();
        return kOfxStatOK;
    } catch (std::bad_alloc) {
        qDebug() << "mutexUnLock(): memory error.";
        return kOfxStatErrMemory;
    } catch ( const std::exception& e ) {
        qDebug() << "mutexUnLock(): " << e.what();
        return kOfxStatErrUnknown;
    } catch ( ... ) {
        qDebug() << "mutexUnLock(): unknown error.";
        return kOfxStatErrUnknown;
    }
}

// Non blocking attempt to lock the mutex
//  This attempts to lock a mutex, if it cannot, it returns and says so, rather than blocking.
// A sucessful lock causes the mutex's lock count to be increased by one, if the lock did not suceed, the call returns immediately and the lock count remains unchanged.
// http://openfx.sourceforge.net/Documentation/1.3/ofxProgrammingReference.html#OfxMultiThreadSuiteV1_mutexTryLock
OfxStatus Natron::OfxHost::mutexTryLock(const OfxMutexHandle mutex)
{
    if (mutex == 0) {
        return kOfxStatErrBadHandle;
    }
    // suite functions should not throw
    try {
        if (reinterpret_cast<QMutex*>(mutex)->tryLock()) {
            return kOfxStatOK;
        } else {
            return kOfxStatFailed;
        }
    } catch (std::bad_alloc) {
        qDebug() << "mutexTryLock(): memory error.";
        return kOfxStatErrMemory;
    } catch ( const std::exception& e ) {
        qDebug() << "mutexTryLock(): " << e.what();
        return kOfxStatErrUnknown;
    } catch ( ... ) {
        qDebug() << "mutexTryLock(): unknown error.";
        return kOfxStatErrUnknown;
    }
}
#endif

