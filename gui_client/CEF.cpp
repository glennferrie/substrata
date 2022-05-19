/*=====================================================================
CEF.cpp
-------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "CEF.h"


#include "CEFInternal.h"
#include <PlatformUtils.h>
#if CEF_SUPPORT  // CEF_SUPPORT will be defined in CMake (or not).
#include <cef_app.h>
#include <cef_client.h>
#include <wrapper/cef_helpers.h>
#ifdef OSX
#include <wrapper/cef_library_loader.h>
#endif
#endif


class GlareCEFApp : public CefApp, public RefCounted
{
public:
	GlareCEFApp()
	{
		lifespan_handler = new LifeSpanHandler();
	}

	~GlareCEFApp()
	{
	}

	virtual void OnBeforeCommandLineProcessing(const CefString& process_type, CefRefPtr<CefCommandLine> command_line) override
	{
		// To allow autoplaying videos etc. without having to click:
		// See https://www.magpcss.org/ceforum/viewtopic.php?f=6&t=16517
		command_line->AppendSwitchWithValue("autoplay-policy", "no-user-gesture-required");


		//TEMP:
		//command_line->AppendSwitch("enable-logging");
		//command_line->AppendSwitchWithValue("v", "2");

		/*if(process_type.empty())
		{
		command_line->AppendSwitch("disable-gpu");
		command_line->AppendSwitch("disable-gpu-compositing");
		}*/
	}

	CefRefPtr<LifeSpanHandler> lifespan_handler;
	
	IMPLEMENT_REFCOUNTING(GlareCEFApp);
};



static bool CEF_initialised = false;
CefRefPtr<GlareCEFApp> glare_cef_app;


bool CEF::isInitialised()
{
	return CEF_initialised;
}


void CEF::initialiseCEF(const std::string& base_dir_path)
{
	assert(!CEF_initialised);
	if(CEF_initialised)
		return;

#ifdef OSX
	// Load the CEF framework library at runtime instead of linking directly
	// as required by the macOS sandbox implementation.
	CefScopedLibraryLoader library_loader;
	if(!library_loader.LoadInMain())
	{
		conPrint("CefScopedLibraryLoader LoadInMain failed.");
		throw glare::Exception("CefScopedLibraryLoader LoadInMain failed.");
	}
#endif

	CefMainArgs args;

	CefSettings settings;

#ifdef OSX
	//const std::string browser_process_path = base_dir_path + "/../Frameworks/gui_client Helper.app"; // On mac, base_dir_path is the path to Resources.
#else
	settings.no_sandbox = true;
	const std::string browser_process_path = base_dir_path + "/browser_process.exe";
	conPrint("Using browser_process_path '" + browser_process_path + "'...");
	CefString(&settings.browser_subprocess_path).FromString(browser_process_path);
#endif

	glare_cef_app = new GlareCEFApp();

	bool result = CefInitialize(args, settings, glare_cef_app, /*windows sandbox info=*/NULL);
	if(result)
		CEF_initialised = true;
	else
	{
		conPrint("CefInitialize failed.");
	}
}


void CEF::shutdownCEF()
{
	assert(CEF_initialised);

	// Wait until browser processes are shut down
	while(!glare_cef_app->lifespan_handler->mBrowserList.empty())
	{
		PlatformUtils::Sleep(1);
		CefDoMessageLoopWork();
	}

	glare_cef_app->lifespan_handler = CefRefPtr<LifeSpanHandler>();

	CEF_initialised = false;
	CefShutdown();

	glare_cef_app = CefRefPtr<GlareCEFApp>();
}


LifeSpanHandler* CEF::getLifespanHandler()
{
	return glare_cef_app->lifespan_handler.get();
}


void CEF::doMessageLoopWork()
{
#if CEF_SUPPORT
	if(CEF_initialised)
		CefDoMessageLoopWork();
#endif
}
