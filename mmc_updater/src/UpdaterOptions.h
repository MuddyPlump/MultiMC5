#pragma once

#include "UpdateInstaller.h"

/** Parses the command-line options to the updater binary. */
class UpdaterOptions
{
public:
	UpdaterOptions();

	void parse(int argc, char** argv);

	QString installDir;
	QString packageDir;
	QString scriptPath;
	QString finishCmd;
	QString finishDir;
	unsigned long long waitPid;
	QString logFile;
	bool dryRun;
};
