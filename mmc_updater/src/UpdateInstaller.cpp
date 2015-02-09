#include "UpdateInstaller.h"

#include "AppInfo.h"
#include "FileUtils.h"
#include "Log.h"
#include "ProcessUtils.h"
#include "UpdateObserver.h"
#include "StringUtils.h"

void UpdateInstaller::setWaitPid(PLATFORM_PID pid)
{
	m_waitPid = pid;
}

void UpdateInstaller::setInstallDir(const std::string& path)
{
	m_installDir = path;
}

void UpdateInstaller::setPackageDir(const std::string& path)
{
	m_packageDir = path;
}

void UpdateInstaller::setBackupDir(const std::string& path)
{
	m_backupDir = path;
}

void UpdateInstaller::setScript(UpdateScript* script)
{
	m_script = script;
}

void UpdateInstaller::setFinishCmd(const std::string& cmd)
{
	m_finishCmd = cmd;
}

void UpdateInstaller::setFinishDir(const std::string &dir)
{
	m_finishDir = dir;
}

std::list<std::string> UpdateInstaller::updaterArgs() const
{
	std::list<std::string> args;
	args.push_back("--install-dir");
	args.push_back(m_installDir);
	args.push_back("--package-dir");
	args.push_back(m_packageDir);
	args.push_back("--script");
	args.push_back(m_script->path());
	if (m_dryRun)
	{
		args.push_back("--dry-run");
	}
	if (m_finishDir.size())
	{
		args.push_back("--dir");
		args.push_back(m_finishDir);
	}
	return args;
}

std::string UpdateInstaller::friendlyErrorForError(const FileUtils::IOException& exception) const
{
	std::string friendlyError;

	switch (exception.type())
	{
		case FileUtils::IOException::ReadOnlyFileSystem:
#ifdef PLATFORM_MAC
			friendlyError = AppInfo::appName() + " was started from a read-only location.  "
			                "Copy it to the Applications folder on your Mac and run "
			                "it from there.";
#else
			friendlyError = AppInfo::appName() + " was started from a read-only location.  "
							"Re-install it to a location that can be updated and run it from there.";
#endif
			break;
		case FileUtils::IOException::DiskFull:
			friendlyError = "The disk is full.  Please free up some space and try again.";
			break;
		default:
			break;
	}

	return friendlyError;
}

void UpdateInstaller::run() throw ()
{
	if (!m_script || !m_script->isValid())
	{
		LOG(Error, "Unable to read update script");
		return;
	}
	if (m_installDir.empty())
	{
		LOG(Error, "No installation directory specified");
		return;
	}

	std::string updaterPath;
	try
	{
		updaterPath = ProcessUtils::currentProcessPath();
	}
	catch (const FileUtils::IOException&)
	{
		LOG(Error,"error reading process path with mode");
		return;
	}

	if (m_waitPid != 0)
	{
		LOG(Info,"Waiting for main app process to finish");
		ProcessUtils::waitForProcess(m_waitPid);
	}

	std::list<std::string> args = updaterArgs();
	args.push_back("--mode");
	args.push_back("main");
	args.push_back("--wait");
	args.push_back(intToStr(ProcessUtils::currentProcessId()));

	int installStatus = 0;
	if (!checkAccess())
	{
		LOG(Info,"Insufficient rights to install app to " + m_installDir + " requesting elevation");

		// start a copy of the updater with admin rights
		installStatus = ProcessUtils::runElevated(updaterPath,args,AppInfo::name());
	}
	else
	{
		LOG(Info,"Sufficient rights to install app - restarting with same permissions");
		installStatus = ProcessUtils::runSync(updaterPath,args);
	}

	if (installStatus == 0)
	{
		LOG(Info,"Update install completed");
	}
	else
	{
		LOG(Error,"Update install failed with status " + intToStr(installStatus));
	}

	// restart the main application - this is currently done
	// regardless of whether the installation succeeds or not
	restartMainApp();

	// clean up files created by the updater
	cleanup();
}

void UpdateInstaller::cleanup()
{
	try
	{
		FileUtils::removeDir(m_packageDir.c_str());
	}
	catch (const FileUtils::IOException& ex)
	{
		LOG(Error,"Error cleaning up updater " + std::string(ex.what()));
	}
	LOG(Info,"Updater files removed");
}

void UpdateInstaller::revert()
{
	LOG(Info,"Reverting installation!");
	std::map<std::string,std::string>::const_iterator iter = m_backups.begin();
	for (;iter != m_backups.end();iter++)
	{
		const std::string& installedFile = iter->first;
		const std::string& backupFile = iter->second;
		LOG(Info,"Restoring " + installedFile);
		if(!m_dryRun)
		{
			if (FileUtils::fileExists(installedFile.c_str()))
			{
				FileUtils::removeFile(installedFile.c_str());
			}
			FileUtils::moveFile(backupFile.c_str(),installedFile.c_str());
		}
	}
}

void UpdateInstaller::installFile(const UpdateScriptFile& file)
{
	std::string sourceFile = file.source;
	std::string destPath = file.dest;
	std::string absDestPath = FileUtils::makeAbsolute(destPath.c_str(), m_installDir.c_str()).toStdString();

	LOG(Info,"Installing file " + sourceFile + " to " + absDestPath);

	// backup the existing file if any
	backupFile(absDestPath);

	// create the target directory if it does not exist
	std::string destDir = FileUtils::dirname(absDestPath.c_str()).toStdString();
	if (!FileUtils::fileExists(destDir.c_str()))
	{
		LOG(Info,"Destination path missing. Creating " + destDir);
		if(!m_dryRun)
		{
			FileUtils::mkpath(destDir.c_str());
		}
	}

	if (!FileUtils::fileExists(sourceFile.c_str()))
	{
		throw "Source file does not exist: " + sourceFile;
	}
	if(!m_dryRun)
	{
		FileUtils::copyFile(sourceFile.c_str(),absDestPath.c_str());

		// set the permissions on the newly extracted file
		FileUtils::chmod(absDestPath.c_str(),file.permissions);
	}
}

void UpdateInstaller::installFiles()
{
	LOG(Info,"Installing files.");
	std::vector<UpdateScriptFile>::const_iterator iter = m_script->filesToInstall().begin();
	int filesInstalled = 0;
	for (;iter != m_script->filesToInstall().end();iter++)
	{
		installFile(*iter);
		++filesInstalled;
	}
}

void UpdateInstaller::uninstallFiles()
{
	LOG(Info,"Uninstalling files.");
	std::vector<std::string>::const_iterator iter = m_script->filesToUninstall().begin();
	for (;iter != m_script->filesToUninstall().end();iter++)
	{
		std::string path = FileUtils::makeAbsolute(iter->c_str(), m_installDir.c_str()).toStdString();
		if (FileUtils::fileExists(path.c_str()))
		{
			LOG(Info,"Uninstalling " + path);
			if(!m_dryRun)
			{
				FileUtils::removeFile(path.c_str());
			}
		}
		else
		{
			LOG(Warn,"Unable to uninstall file " + path + " because it does not exist.");
		}
	}
}

void UpdateInstaller::backupFile(const std::string& path)
{
	if (!FileUtils::fileExists(path.c_str()))
	{
		// no existing file to backup
		return;
	}
	std::string backupPath = path + ".bak";
	LOG(Info,"Backing up file: " + path + " as " + backupPath);
	if(!m_dryRun)
	{
		FileUtils::removeFile(backupPath.c_str());
		FileUtils::moveFile(path.c_str(), backupPath.c_str());
	}
	m_backups[path] = backupPath;
}

void UpdateInstaller::removeBackups()
{
	LOG(Info,"Removing backups.");
	std::map<std::string,std::string>::const_iterator iter = m_backups.begin();
	for (;iter != m_backups.end();iter++)
	{
		const std::string& backupFile = iter->second;
		LOG(Info,"Removing " + backupFile);
		if(!m_dryRun)
		{
			FileUtils::removeFile(backupFile.c_str());
		}
	}
}

bool UpdateInstaller::checkAccess()
{
	std::string testFile = m_installDir + "/update-installer-test-file";
	LOG(Info,"Checking for access: " + testFile);
	try
	{
		if(!m_dryRun)
		{
			FileUtils::removeFile(testFile.c_str());
		}
	}
	catch (const FileUtils::IOException& error)
	{
		LOG(Info,"Removing existing access check file failed " + std::string(error.what()));
	}

	try
	{
		if(!m_dryRun)
		{
			FileUtils::touch(testFile.c_str());
			FileUtils::removeFile(testFile.c_str());
		}
		return true;
	}
	catch (const FileUtils::IOException& error)
	{
		LOG(Info,"checkAccess() failed " + std::string(error.what()));
		return false;
	}
}

void UpdateInstaller::restartMainApp()
{
	try
	{
		std::string command = m_finishCmd;
		std::list<std::string> args;

		if (!command.empty())
		{
			if(!m_finishDir.empty())
			{
				args.push_back("--dir");
				args.push_back(m_finishDir);
			}
			LOG(Info,"Starting main application " + command);
			if(!m_dryRun)
			{
				ProcessUtils::runAsync(command,args);
			}
		}
		else
		{
			LOG(Error,"No main binary specified");
		}
	}
	catch (const std::exception& ex)
	{
		LOG(Error,"Unable to restart main app " + std::string(ex.what()));
	}
}

void UpdateInstaller::postInstallUpdate()
{
	// perform post-install actions

#ifdef PLATFORM_MAC
	// touch the application's bundle directory so that
	// OS X' Launch Services notices any changes in the application's
	// Info.plist file.
	LOG(Info,"Touching " + m_installDir + " to notify OSX of metadata changes.");
	if(!m_dryRun)
	{
		FileUtils::touch(m_installDir.c_str());
	}
#endif
}

void UpdateInstaller::setDryRun(bool dryRun)
{
	m_dryRun = dryRun;
}
