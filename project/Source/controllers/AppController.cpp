
#include "controllers/AppController.h"
#include "controllers/EngineController.h"
#include "controllers/GuiController.h"
#include "controllers/SessionController.h"
#include "engine/GraphProcessor.h"
#include "gui/UnlockForm.h"
#include "session/UnlockStatus.h"
#include "Commands.h"
#include "Globals.h"
#include "Messages.h"
#include "Settings.h"
#include "Version.h"

namespace Element {

AppController::AppController (Globals& g)
    : world (g)
{
    addChild (new GuiController (g, *this));
    addChild (new EngineController ());
    addChild (new SessionController ());
    g.getCommandManager().registerAllCommandsForTarget (this);
    g.getCommandManager().setFirstCommandTarget (this);
}

AppController::~AppController() { }

void AppController::activate()
{
    Controller::activate();
}

void AppController::deactivate()
{
    Controller::deactivate();
}

void AppController::run()
{
    activate();
    
    if (auto* gui = findChild<GuiController>())
        gui->run();
    
    if (auto* sc = findChild<SessionController>())
    {
        const auto lastSession = getWorld().getSettings().getUserSettings()->getValue ("lastSession");
        if (File::isAbsolutePath (lastSession))
            sc->openFile (File (lastSession));
    }
}

void AppController::handleMessage (const Message& msg)
{
	auto* ec = findChild<EngineController>();
	jassert(ec);

    bool handled = true;

    if (const auto* lpm = dynamic_cast<const LoadPluginMessage*> (&msg))
    {
        ec->addPlugin (lpm->description);
    }
    else if (const auto* rnm = dynamic_cast<const RemoveNodeMessage*> (&msg))
    {
        ec->removeNode (rnm->nodeId);
    }
    else if (const auto* acm = dynamic_cast<const AddConnectionMessage*> (&msg))
    {
        if (acm->useChannels())
            ec->connectChannels (acm->sourceNode, acm->sourceChannel, acm->destNode, acm->destChannel);
        else
            ec->addConnection (acm->sourceNode, acm->sourcePort, acm->destNode, acm->destPort);
    }
    else if (const auto* rcm = dynamic_cast<const RemoveConnectionMessage*> (&msg))
    {
        if (rcm->useChannels())
		{
            jassertfalse; // channels not yet supported
        }
        else
        {
            ec->removeConnection (rcm->sourceNode, rcm->sourcePort, rcm->destNode, rcm->destPort);
        }
    }
    else
    {
        handled = false;
    }
    
    if (! handled)
    {
        DBG("[EL] AppController: unhandled Message received");
    }
}


ApplicationCommandTarget* AppController::getNextCommandTarget()
{
    return findChild<GuiController>();
}

void AppController::getAllCommands (Array<CommandID>& cids)
{
    cids.addArray ({
        Commands::mediaNew,
        Commands::mediaOpen,
        Commands::mediaSave,
        Commands::mediaSaveAs,
        
        Commands::signIn,
        Commands::signOut,
        
        Commands::sessionNew,
        Commands::sessionSave,
        Commands::sessionOpen,
        Commands::sessionAddGraph,
        Commands::sessionDuplicateGraph,
        Commands::sessionDeleteGraph,
        Commands::sessionInsertPlugin,
        
        Commands::importGraph,
        Commands::exportGraph,
        
        Commands::checkNewerVersion
    });
    cids.addArray({ Commands::copy, Commands::paste });
}

void AppController::getCommandInfo (CommandID commandID, ApplicationCommandInfo& result)
{
    Commands::getCommandInfo (commandID, result);
}

bool AppController::perform (const InvocationInfo& info)
{
    auto& status (world.getUnlockStatus());
	auto& settings(getGlobals().getSettings());
    bool res = true;
    switch (info.commandID)
    {
        case Commands::sessionOpen:
        {
            FileChooser chooser ("Open Session", lastSavedFile, "*.els", true, false);
            if (chooser.browseForFileToOpen())
                findChild<SessionController>()->openFile (chooser.getResult());
        } break;
        case Commands::sessionNew:
            findChild<SessionController>()->newSession();
            break;
        case Commands::sessionSave:
            findChild<SessionController>()->saveSession (false);
            break;
        case Commands::sessionSaveAs:
            findChild<SessionController>()->saveSession (true);
            break;
        case Commands::sessionClose:
            findChild<SessionController>()->closeSession();
            break;
        case Commands::sessionAddGraph:
            findChild<EngineController>()->addGraph();
            break;
        case Commands::sessionDuplicateGraph:
            findChild<EngineController>()->duplicateGraph();
            break;
        case Commands::sessionDeleteGraph:
            findChild<EngineController>()->removeGraph();
            break;
            
        case Commands::mediaNew:
		{
            if (AlertWindow::showOkCancelBox (AlertWindow::InfoIcon, "New Graph", 
											  "This will clear the current graph, are you sure?"))
            {
                lastSavedFile = File();
                findChild<EngineController>()->clear();
            }
        } break;

        case Commands::mediaSave:
        {
            if (! status.isFullVersion())
            {
                AlertWindow::showMessageBox (AlertWindow::InfoIcon,
                    "Unauthorized", "Saving individual graphs is not available in Element Lite.\nVisit https://kushview.net/products/element to purchase a copy");
            }
            else if (status.isFullVersion())
            {
                if (lastSavedFile.existsAsFile() && lastSavedFile.hasFileExtension ("elg"))
                {
                    jassertfalse; // handle this
                    const ValueTree model ("node");
                    FileOutputStream stream (lastSavedFile);
                    model.writeToStream (stream);
                }
                
                else
                {
                    FileChooser chooser ("Save current graph", File(), "*.elg");
                    if (chooser.browseForFileToSave (true))
                    {
                        lastSavedFile = chooser.getResult();
                        const ValueTree model ("node");
                        jassertfalse; // handle this
                        FileOutputStream stream (lastSavedFile);
                        model.writeToStream (stream);
                    }
                }
            }
        } break;
        
        case Commands::mediaSaveAs:
        {
            if (! status.isFullVersion())
            {
                String msg = "Saving is available in Element Pro.\n";
                msg << "Visit https://kushview.net/products/element to purchase a copy";
                AlertWindow::showMessageBox (AlertWindow::InfoIcon, "Unauthorized", msg);
            }
            else
            {
                FileChooser chooser ("Save current graph", File(), "*.elg");
                if (chooser.browseForFileToSave (true))
                {
                    lastSavedFile = chooser.getResult();
                    const ValueTree model ("node");
                    jassertfalse; //handle
                    FileOutputStream stream (lastSavedFile);
                    model.writeToStream (stream);
                }
            }
        } break;
        
        case Commands::signIn:
        {
            auto* form = new UnlockForm (status, "Enter your license key.",
                                         false, false, true, true);
            DialogWindow::LaunchOptions opts;
            opts.content.setOwned (form);
            opts.resizable = false;
            opts.dialogTitle = "License Manager";
            opts.runModal();
        } break;
        
        case Commands::signOut:
        {
            if (status.isUnlocked())
            {
                auto* props = settings.getUserSettings();
                props->removeValue("L");
                props->save();
                status.load();
            }
        } break;
        
        case Commands::checkNewerVersion:
            CurrentVersion::checkAfterDelay (20, true);
            break;
        
        default: res = false; break;
    }
    return res;
}

}
