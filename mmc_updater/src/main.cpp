#include "AppInfo.h"
#include "FileUtils.h"
#include "Log.h"
#include "Platform.h"
#include "ProcessUtils.h"
#include "StringUtils.h"
#include "UpdateScript.h"
#include "UpdaterOptions.h"

#include <thread>

#if defined(PLATFORM_LINUX)
  #include "UpdateDialogGtkFactory.h"
  #include "UpdateDialogAscii.h"
#endif

#if defined(PLATFORM_MAC)
  #include "MacBundle.h"
  #include "UpdateDialogCocoa.h"
#endif

#if defined(PLATFORM_WINDOWS)
  #include "UpdateDialogWin32.h"
#endif

#include <iostream>
#include <memory>

#define UPDATER_VERSION "0.16"

UpdateDialog* createUpdateDialog();

void runUpdaterThread(void* arg)
{
#ifdef PLATFORM_MAC
	// create an autorelease pool to free any temporary objects
	// created by Cocoa whilst handling notifications from the UpdateInstaller
	void* pool = UpdateDialogCocoa::createAutoreleasePool();
#endif

	try
	{
		UpdateInstaller* installer = static_cast<UpdateInstaller*>(arg);
		installer->run();
	}
	catch (const std::exception& ex)
	{
		LOG(Error,"Unexpected exception " + std::string(ex.what()));
	}

#ifdef PLATFORM_MAC
	UpdateDialogCocoa::releaseAutoreleasePool(pool);
#endif
}

#ifdef PLATFORM_MAC
extern unsigned char Info_plist[];
extern unsigned int Info_plist_len;

extern unsigned char mac_icns[];
extern unsigned int mac_icns_len;

bool unpackBundle(int argc, char** argv)
{
	MacBundle bundle(FileUtils::tempPath(),AppInfo::name());
	std::string currentExePath = ProcessUtils::currentProcessPath();

	if (currentExePath.find(bundle.bundlePath()) != std::string::npos)
	{
		// already running from a bundle
		return false;
	}
	LOG(Info,"Creating bundle " + bundle.bundlePath());

	// create a Mac app bundle
	std::string plistContent(reinterpret_cast<const char*>(Info_plist),Info_plist_len);
	std::string iconContent(reinterpret_cast<const char*>(mac_icns),mac_icns_len);
	bundle.create(plistContent,iconContent,ProcessUtils::currentProcessPath());

	std::list<std::string> args;
	for (int i = 1; i < argc; i++)
	{
		args.push_back(argv[i]);
	}
	ProcessUtils::runSync(bundle.executablePath(),args);
	return true;
}
#endif

void setupConsole()
{
#ifdef PLATFORM_WINDOWS
	// see http://stackoverflow.com/questions/587767/how-to-output-to-console-in-c-windows
	// and http://www.libsdl.org/cgi/docwiki.cgi/FAQ_Console
	AttachConsole(ATTACH_PARENT_PROCESS);
	freopen( "CON", "w", stdout );
	freopen( "CON", "w", stderr );
#endif
}

int main(int argc, char** argv)
{
#ifdef PLATFORM_MAC
	void* pool = UpdateDialogCocoa::createAutoreleasePool();
#endif
	
	Log::instance()->open(AppInfo::logFilePath());

#ifdef PLATFORM_MAC
	// when the updater is run for the first time, create a Mac app bundle
	// and re-launch the application from the bundle.  This permits
	// setting up bundle properties (such as application icon)
	if (unpackBundle(argc,argv))
	{
		return 0;
	}
#endif

	UpdaterOptions options;
	options.parse(argc,argv);

	UpdateInstaller installer;
	UpdateScript script;

	if (!options.scriptPath.isEmpty())
	{
		script.parse(options.scriptPath.toStdString());
	}

	LOG(Info,"started updater. install-dir: " + options.installDir
	         + ", package-dir: " + options.packageDir
			 + ", wait-pid: " + QString::number(options.waitPid)
			 + ", script-path: " + options.scriptPath
			 + ", finish-cmd: " + options.finishCmd
			 + ", finish-dir: " + options.finishDir);

	installer.setInstallDir(options.installDir.toStdString());
	installer.setPackageDir(options.packageDir.toStdString());
	installer.setScript(&script);
	installer.setWaitPid(options.waitPid);
	installer.setFinishCmd(options.finishCmd.toStdString());
	installer.setFinishDir(options.finishDir.toStdString());
	installer.setDryRun(options.dryRun);

	installer.run();

#ifdef PLATFORM_MAC
	UpdateDialogCocoa::releaseAutoreleasePool(pool);
#endif

	return 0;
}

UpdateDialog* createUpdateDialog()
{
#if defined(PLATFORM_WINDOWS)
	return new UpdateDialogWin32();
#elif defined(PLATFORM_MAC)
	return new UpdateDialogCocoa();
#elif defined(PLATFORM_LINUX)
	UpdateDialog* dialog = UpdateDialogGtkFactory::createDialog();
	if (!dialog)
	{
		dialog = new UpdateDialogAscii();
	}
	return dialog;
#endif
}

#ifdef PLATFORM_WINDOWS
// application entry point under Windows
int CALLBACK WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR lpCmdLine,
                     int nCmdShow)
{
	int argc = 0;
	char** argv;
	ProcessUtils::convertWindowsCommandLine(GetCommandLineW(),argc,argv);
	return main(argc,argv);
}
#endif
