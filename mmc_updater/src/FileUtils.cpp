#include "FileUtils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "Log.h"

FileUtils::IOException::IOException(const QString &error)
{
	init(errno,error);
}

FileUtils::IOException::IOException(int errorCode, const QString &error)
{
	init(errorCode,error);
}

void FileUtils::IOException::init(int errorCode, const QString &error)
{
	m_error = error;

#ifdef Q_OS_LINUX
	m_errorCode = errorCode;

	if (m_errorCode > 0)
	{
		m_error += " details: " + QString::fromLocal8Bit(strerror(m_errorCode));
	}
#endif

#ifdef Q_OS_WIN
	m_errorCode = 0;
	m_error += " GetLastError returned: " + QString::number(GetLastError());
#endif
}

FileUtils::IOException::~IOException() throw ()
{
}

FileUtils::IOException::Type FileUtils::IOException::type() const
{
#ifdef Q_OS_LINUX
	switch (m_errorCode)
	{
		case 0:
			return NoError;
		case EROFS:
			return ReadOnlyFileSystem;
		case ENOSPC:
			return DiskFull;
		default:
			return Unknown;
	}
#else
	return Unknown;
#endif
}

bool FileUtils::fileExists(const QString &path) throw (IOException)
{
	return QFile::exists(path);
}

int FileUtils::fileMode(const QString &path) throw (IOException)
{
	return QFile::permissions(path);
}

void FileUtils::chmod(const QString &path, int mode) throw (IOException)
{
#ifdef Q_OS_LINUX
	if (!QFile::setPermissions(path, (QFile::Permission)mode))
	{
		throw IOException("Failed to set permissions on " + path + " to " + QString::number(mode));
	}
#endif
}

void FileUtils::moveFile(const QString &src, const QString &dest) throw (IOException)
{
	QFile::rename(src, dest);
}

void FileUtils::mkpath(const QString &dir) throw (IOException)
{
	if (!QDir().mkpath(dir))
	{
		throw IOException("Unable to create directory " + dir);
	}
}

void FileUtils::removeFile(const QString &src) throw (IOException)
{
	if (QFile::remove(src))
	{
		throw IOException("Unable to remove file " + src);
	}
}

QString FileUtils::fileName(const QString &path)
{
	return QFileInfo(path).fileName();
}

QString FileUtils::dirname(const QString &path)
{
	return QDir(path).dirName();
}

void FileUtils::touch(const QString &path) throw (IOException)
{
	QFile file(path);
	if (!file.open(QFile::WriteOnly | QFile::Append))
	{
		throw IOException("Couldn't open a file for writing: " +
						  file.errorString());
	}
	file.close();
}

QString FileUtils::canonicalPath(const QString &path)
{
	return QFileInfo(path).canonicalFilePath();
}

QString FileUtils::tempPath()
{
	return QDir::tempPath();
}

QString FileUtils::readFile(const QString &path) throw (IOException)
{
	QFile file(path);
	if (!file.open(QFile::ReadOnly))
	{
		throw IOException("Failed to read file " + path);
	}
	return QString::fromLatin1(file.readAll());
}

void FileUtils::copyFile(const QString &src, const QString &dest) throw (IOException)
{
	if (!QFile::copy(src, dest))
	{
		throw IOException("Failed to copy " + src + " to " + dest);
	}
	QFile::setPermissions(dest, QFile::permissions(src));
}

QString FileUtils::makeAbsolute(const QString &path, const QString &basePath)
{
	return QDir(path).absoluteFilePath(basePath);
}

void FileUtils::removeDir(const QString &path) throw (IOException)
{
	if (!QDir(path).removeRecursively())
	{
		throw IOException("Couldn't remove directory " + path);
	}
}
