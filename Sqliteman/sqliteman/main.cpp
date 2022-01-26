/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

// #include <iostream>
#include <signal.h>

// required for *BSD
#ifndef WIN32
#include <unistd.h>
#endif

#include <QApplication>
#include <QtCore/QDir>
#include <QIcon>
#include <QtCore/QLocale>
#include <QMessageBox>
#include <QStyleFactory>
#include <QtCore/QtDebug> //qDebug
#include <QtCore/QTextStream>
#include <QtCore/QTranslator>

#include "litemanwindow.h"
#include "preferences.h"
#include "utils.h"

#define ARG_VERSION "--version"
#define ARG_HELP "--help"
#define ARG_LANG "--lang"
#define ARG_AVAILLANG "--langs"
#define ARG_VERSION_SHORT "-v"
#define ARG_HELP_SHORT "-h"
#define ARG_LANG_SHORT "-l"
#define ARG_AVAILLANG_SHORT "-la"
#define ARG_SCRIPT "-s"
#define ARG_EXECUTE "-x"
#define endl QString("\n")


#ifndef WIN32
void initCrashHandler();
static void defaultCrashHandler(int sig);
#endif


/*! \brief Parse the CLI user input.
Based on the Scribus code (a bit).
\author Petr Vanek <petr@scribus.info>
*/
class ArgsParser
{
	public:
		ArgsParser(int c, char ** v);
		~ArgsParser(){};
		bool parseArgs();
		QString localeCode();
		//! \brief No file is opened when the returned value is null
		const QString & fileToOpen();
		const QString & scriptToOpen();
		bool executeScript();
	private:
		int argc;
		char ** argv;
		QString m_locale;
		QMap<int,QString> m_localeList;
		void langsAvailable();
		QString m_lastDB;
		QString m_lastSqlFile;
        bool m_execute;
};

/*! \brief Pre-fill available translations into QMap to cooperate
with PreferencesDialog.
*/
ArgsParser::ArgsParser(int c, char ** v)
	: argc(c), argv(v), m_locale(""),
	m_lastDB(QString()), m_lastSqlFile(QString())
{
    m_execute = false;
	QDir d(TRANSLATION_DIR, "*.qm");
	int i = 1; // 0 is for system default
	QStringList l = d.entryList();
	QStringList::const_iterator j;
    for (j = l.constBegin(); j != l.constEnd(); ++i, ++j) {
        QString s = *j; // non-const copy
		m_localeList[i] = s.remove("sqliteman_").remove(".qm");
	}
}

//! \brief Print available translations
void ArgsParser::langsAvailable()
{
	// HACK: QTextStream simulates std::cout here. It can handle
	// QStrings without any issues. E.g. std::cout << QString::toStdString()
	// does compile problems in some Qt4 configurations.
	QTextStream cout(stdout, QIODevice::WriteOnly);
	cout << QString("Available translation:") << endl;
    QList<QString> values = m_localeList.values();
    QList<QString>::const_iterator i;
    for (i = values.constBegin(); i != values.constEnd(); ++i) {
		cout << QString("  --lang ") << *i << endl;
    }
}

/*! \brief Get the right translations.
Property: 1) specified from CLI - it overrides Prefs or System
2) from preferences
3) system pre-configured
*/
QString ArgsParser::localeCode()
{
	QString ret;
	Preferences * prefs = Preferences::instance();
	if (!m_locale.isEmpty())
		ret = QLocale(m_locale).name();
	else if (prefs->GUItranslator() != 0)
		ret = m_localeList[prefs->GUItranslator()];
	else
		ret = QLocale::system().name();
	return ret.left(2);
}

const QString & ArgsParser::fileToOpen()
{
	if (m_lastDB.isNull())
	{
        m_execute = false; // in case called before executeScript()
		Preferences* p = Preferences::instance();
		if (p->openLastDB() && QFileInfo(p->lastDB()).exists())
			m_lastDB = p->lastDB();
	}
	return m_lastDB;
}

const QString & ArgsParser::scriptToOpen()
{
	if (m_lastSqlFile.isNull())
	{
		Preferences* p = Preferences::instance();
		if (p->openLastSqlFile() && QFileInfo(p->lastSqlFile()).exists())
			m_lastSqlFile = p->lastSqlFile();
	}
	return m_lastSqlFile;
}

bool ArgsParser::executeScript()
{
    // only allow script to be executed if a database name is given
    return m_execute && !(m_lastDB.isNull());
}

bool ArgsParser::parseArgs()
{
	QString arg("");
	QTextStream cout(stdout, QIODevice::WriteOnly);

	for(int i = 1; i < argc; i++)
	{
		arg = argv[i];

		if ((arg == ARG_LANG || arg == ARG_LANG_SHORT) && (++i < argc))
		{
			m_locale = argv[i];
			continue;
		}
		else if (   ((arg == ARG_SCRIPT) && (++i < argc))
                 || ((arg == ARG_EXECUTE) && (++i < argc)))
		{
			m_lastSqlFile = QFile::decodeName(argv[i]);
			if (!QFileInfo(m_lastSqlFile).exists())
			{
				if (m_lastSqlFile.left(1) == "-" || m_lastSqlFile.left(2) == "--")
					cout << QString("Invalid argument: ") << m_lastSqlFile << endl;
				else
					cout << QString("File ") << m_lastSqlFile << QString(" does not exist, aborting.") << endl;
				return false;
			}
			if (arg == ARG_EXECUTE) { m_execute = true; }
			continue;
		}
		else if (arg == ARG_VERSION || arg == ARG_VERSION_SHORT)
		{
			cout << QString("Sqliteman ") << SQLITEMAN_VERSION << endl;
			return false;
		}
		else if (arg == ARG_HELP || arg == ARG_HELP_SHORT)
		{
			cout << endl << QString("sqliteman [options] [databasefile]") << endl;
			cout << QString("options:") << endl;
			cout << QString("  --help    -h  displays small help") << endl;
			cout << QString("  --version -v  prints version") << endl;
			cout << QString("  --lang    -l  set a GUI language. E.g. --lang cs for Czech") << endl;
			cout << QString("  --langs   -la lists available languages") << endl;
			cout << QString("  -s scriptfile loads scriptfile") << endl;
			cout << QString("  -x scriptfile loads and executes scriptfile") << endl;
			cout << QString("  + various Qt options") << endl;
			cout << QString("  for more information use sqliteman's built-in help viewer") << endl << endl;
			return false;
		}
		else if (arg == ARG_AVAILLANG || arg == ARG_AVAILLANG_SHORT)
		{
			langsAvailable();
			return false;
		}
		else
		{
			m_lastDB = QFile::decodeName(argv[i]);
			if (!QFileInfo(m_lastDB).exists())
			{
				if (m_lastDB.left(1) == "-" || m_lastDB.left(2) == "--")
					cout << QString("Invalid argument: ") << m_lastDB << endl;
				else
					cout << QString("File ") << m_lastDB << QString(" does not exist, aborting.") << endl;
				return false;
			}
			return true;
		}
	}
	return true;
}

int main(int argc, char ** argv)
{
	QApplication app(argc, argv);
#ifndef  WIN32
	initCrashHandler();
#endif
	ArgsParser cli(argc, argv);
	if (!cli.parseArgs()) { return 0; }

	int style = Preferences::instance()->GUIstyle();
	if (style != 0)
	{
		bool styleSuccess = false;
		QStringList sl = QStyleFactory::keys();
		sl.sort();
		if (sl.count() > (style-1))
		{
			QStyle * s = QStyleFactory::create(sl.at(style-1));
			if (s)
			{
				QApplication::setStyle(s);
				styleSuccess = true;
			}
		}
		if (!styleSuccess)
		{
			QTextStream cout(stdout, QIODevice::WriteOnly);
			cout << "Cannot setup GUI style. Default is used.";
		}
	}

	app.setWindowIcon(Utils::getIcon("sqliteman.png"));

	if (QApplication::font() != Preferences::instance()->GUIfont())
		app.setFont(Preferences::instance()->GUIfont());

	QTranslator translator;
	translator.load(Utils::getTranslator(cli.localeCode()));

	app.installTranslator(&translator);

	LiteManWindow * wnd = new LiteManWindow(
        cli.fileToOpen(), cli.scriptToOpen(), cli.executeScript());
	wnd->setLanguage(cli.localeCode());
	wnd->show();

	// for some strange reason it sometimes doesn't get shown....
	// wnd->dataViewer->ui.mainToolBar->setVisible(true);

	int r = app.exec();
	delete wnd;
	return r;
}

#ifndef WIN32
void initCrashHandler()
{
	typedef void (*HandlerType)(int);
	HandlerType handler	= 0;
	handler = defaultCrashHandler;
	if (!handler)
		handler = SIG_DFL;
	sigset_t mask;
	sigemptyset(&mask);
#ifdef SIGSEGV
	signal (SIGSEGV, handler);
	sigaddset(&mask, SIGSEGV);
#endif
#ifdef SIGFPE
	signal (SIGFPE, handler);
	sigaddset(&mask, SIGFPE);
#endif
#ifdef SIGILL
	signal (SIGILL, handler);
	sigaddset(&mask, SIGILL);
#endif
#ifdef SIGABRT
	signal (SIGABRT, handler);
	sigaddset(&mask, SIGABRT);
#endif
	sigprocmask(SIG_UNBLOCK, &mask, 0);
}


void defaultCrashHandler(int sig)
{
	QTextStream cout(stdout, QIODevice::WriteOnly);
	static int crashRecursionCounter = 0;
	crashRecursionCounter++;
	signal(SIGALRM, SIG_DFL);
	if (crashRecursionCounter < 2)
	{
		crashRecursionCounter++;
		QString sigMsg(QString("\nSqliteman crashes due to Signal #%1\n\n\
All databases opened will be rolled back and closed.\n\n\
Collect last steps that forced this\n\
situlation and report it as a bug, please.").arg(sig));
		cout << sigMsg << endl;
		QMessageBox::critical(0, "Sqliteman", sigMsg);
		alarm(300);
	}
	exit(255);
}
#endif

/* The following is an opening page for the source documentation. IGNORE */
/*!
\mainpage Sqliteman Source Documentation

The following pages contain an overview of the various classes, types and function that
make the Sqliteman source code. To better understand Sqliteman, this document can provide an
aid.

Maybe you can think that the documentation is weak. OK, should be better but the code
is clear in most cases.

\note This Sw is a fork of Igor Khanin's original LiteMan. But now it's totally rewritten.
You can find original copyright in the unmodified places.

How to create fresh documentation? Download the source code from SVN. Go to the
Sqliteman/devel-doc directory. Then run make. Doxygen is required, flawfinder and dot are
optional.
 */
