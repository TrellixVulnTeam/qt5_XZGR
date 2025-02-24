/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Copyright (C) 2012 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Marc Mutz <marc.mutz@kdab.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Linguist of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "lupdate.h"

#include <translator.h>
#include <qmakeparser.h>
#include <profileevaluator.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTranslator>
#include <QtCore/QLibraryInfo>

#include <iostream>

// Can't have an array of QStaticByteArrayData<N> for different N, so
// use QByteArray, which requires constructor calls. Doesn't matter
// much, since this is in an app, not a lib:
static const QByteArray defaultTrFunctionNames[] = {
// MSVC can't handle the lambda in this array if QByteArrayLiteral expands
// to a lambda. In that case, use a QByteArray instead.
#if defined(Q_CC_MSVC) && defined(Q_COMPILER_LAMBDA)
#define BYTEARRAYLITERAL(F) QByteArray(#F),
#else
#define BYTEARRAYLITERAL(F) QByteArrayLiteral(#F),
#endif
    LUPDATE_FOR_EACH_TR_FUNCTION(BYTEARRAYLITERAL)
#undef BYTEARRAYLITERAL
};
Q_STATIC_ASSERT((TrFunctionAliasManager::NumTrFunctions == sizeof defaultTrFunctionNames / sizeof *defaultTrFunctionNames));

static int trFunctionByDefaultName(const QByteArray &trFunctionName)
{
    for (int i = 0; i < TrFunctionAliasManager::NumTrFunctions; ++i)
        if (trFunctionName == defaultTrFunctionNames[i])
            return i;
    return -1;
}

static int trFunctionByDefaultName(const QString &trFunctionName)
{
    return trFunctionByDefaultName(trFunctionName.toLatin1());
}

TrFunctionAliasManager::TrFunctionAliasManager()
    : m_trFunctionAliases()
{
    for (int i = 0; i < NumTrFunctions; ++i)
        m_trFunctionAliases[i].push_back(defaultTrFunctionNames[i]);
}

TrFunctionAliasManager::~TrFunctionAliasManager() {}

int TrFunctionAliasManager::trFunctionByName(const QByteArray &trFunctionName) const
{
    ensureTrFunctionHashUpdated();
    // this function needs to be fast
    const QHash<QByteArray, TrFunction>::const_iterator it
        = m_nameToTrFunctionMap.find(trFunctionName);
    return it == m_nameToTrFunctionMap.end() ? -1 : *it;
}

void TrFunctionAliasManager::modifyAlias(int trFunction, const QByteArray &alias, Operation op)
{
    QList<QByteArray> &list = m_trFunctionAliases[trFunction];
    if (op == SetAlias)
        list.clear();
    list.push_back(alias);
    m_nameToTrFunctionMap.clear();
}

void TrFunctionAliasManager::ensureTrFunctionHashUpdated() const
{
    if (!m_nameToTrFunctionMap.empty())
        return;

    QHash<QByteArray, TrFunction> nameToTrFunctionMap;
    for (int i = 0; i < NumTrFunctions; ++i)
        foreach (const QByteArray &alias, m_trFunctionAliases[i])
            nameToTrFunctionMap[alias] = TrFunction(i);
    // commit:
    m_nameToTrFunctionMap.swap(nameToTrFunctionMap);
}

static QStringList availableFunctions()
{
    QStringList result;
    result.reserve(TrFunctionAliasManager::NumTrFunctions);
    for (int i = 0; i < TrFunctionAliasManager::NumTrFunctions; ++i)
        result.push_back(QString::fromLatin1(defaultTrFunctionNames[i]));
    return result;
}

static QStringList byteArrayToStringList(const QList<QByteArray> &in)
{
    QStringList result;
    result.reserve(in.size());
    foreach (const QByteArray &function, in)
        result.push_back(QString::fromLatin1(function));
    return result;
}

QStringList TrFunctionAliasManager::availableFunctionsWithAliases() const
{
    QStringList result;
    result.reserve(NumTrFunctions);
    for (int i = 0; i < NumTrFunctions; ++i)
        result.push_back(QString::fromLatin1(defaultTrFunctionNames[i]) +
                         QLatin1String(" (=") +
                         byteArrayToStringList(m_trFunctionAliases[i]).join(QLatin1Char('=')) +
                         QLatin1Char(')'));
    return result;
}

TrFunctionAliasManager trFunctionAliasManager;

static QString m_defaultExtensions;

static void printOut(const QString & out)
{
    std::cout << qPrintable(out);
}

static void printErr(const QString & out)
{
    std::cerr << qPrintable(out);
}

class LU {
    Q_DECLARE_TR_FUNCTIONS(LUpdate)
};

static void recursiveFileInfoList(const QDir &dir,
    const QSet<QString> &nameFilters, QDir::Filters filter,
    QFileInfoList *fileinfolist)
{
    foreach (const QFileInfo &fi, dir.entryInfoList(filter))
        if (fi.isDir())
            recursiveFileInfoList(QDir(fi.absoluteFilePath()), nameFilters, filter, fileinfolist);
        else if (nameFilters.contains(fi.suffix()))
            fileinfolist->append(fi);
}

static void printUsage()
{
    printOut(LU::tr(
        "Usage:\n"
        "    lupdate [options] [project-file]...\n"
        "    lupdate [options] [source-file|path|@lst-file]... -ts ts-files|@lst-file\n\n"
        "lupdate is part of Qt's Linguist tool chain. It extracts translatable\n"
        "messages from Qt UI files, C++, Java and JavaScript/QtScript source code.\n"
        "Extracted messages are stored in textual translation source files (typically\n"
        "Qt TS XML). New and modified messages can be merged into existing TS files.\n\n"
        "Options:\n"
        "    -help  Display this information and exit.\n"
        "    -no-obsolete\n"
        "           Drop all obsolete strings.\n"
        "    -extensions <ext>[,<ext>]...\n"
        "           Process files with the given extensions only.\n"
        "           The extension list must be separated with commas, not with whitespace.\n"
        "           Default: '%1'.\n"
        "    -pluralonly\n"
        "           Only include plural form messages.\n"
        "    -silent\n"
        "           Do not explain what is being done.\n"
        "    -no-sort\n"
        "           Do not sort contexts in TS files.\n"
        "    -no-recursive\n"
        "           Do not recursively scan the following directories.\n"
        "    -recursive\n"
        "           Recursively scan the following directories (default).\n"
        "    -I <includepath> or -I<includepath>\n"
        "           Additional location to look for include files.\n"
        "           May be specified multiple times.\n"
        "    -locations {absolute|relative|none}\n"
        "           Specify/override how source code references are saved in TS files.\n"
        "           Default is absolute.\n"
        "    -no-ui-lines\n"
        "           Do not record line numbers in references to UI files.\n"
        "    -disable-heuristic {sametext|similartext|number}\n"
        "           Disable the named merge heuristic. Can be specified multiple times.\n"
        "    -pro <filename>\n"
        "           Name of a .pro file. Useful for files with .pro file syntax but\n"
        "           different file suffix. Projects are recursed into and merged.\n"
        "    -pro-out <directory>\n"
        "           Virtual output directory for processing subsequent .pro files.\n"
        "    -source-language <language>[_<region>]\n"
        "           Specify the language of the source strings for new files.\n"
        "           Defaults to POSIX if not specified.\n"
        "    -target-language <language>[_<region>]\n"
        "           Specify the language of the translations for new files.\n"
        "           Guessed from the file name if not specified.\n"
        "    -tr-function-alias <function>{+=,=}<alias>[,<function>{+=,=}<alias>]...\n"
        "           With +=, recognize <alias> as an alternative spelling of <function>.\n"
        "           With  =, recognize <alias> as the only spelling of <function>.\n"
        "           Available <function>s (with their currently defined aliases) are:\n"
        "             %2\n"
        "    -ts <ts-file>...\n"
        "           Specify the output file(s). This will override the TRANSLATIONS.\n"
        "    -version\n"
        "           Display the version of lupdate and exit.\n"
        "    @lst-file\n"
        "           Read additional file names (one per line) or includepaths (one per\n"
        "           line, and prefixed with -I) from lst-file.\n"
    ).arg(m_defaultExtensions,
          trFunctionAliasManager.availableFunctionsWithAliases()
                                .join(QStringLiteral("\n             "))));
}

static bool handleTrFunctionAliases(const QString &arg)
{
    foreach (const QString &pair, arg.split(QLatin1Char(','), QString::SkipEmptyParts)) {
        const int equalSign = pair.indexOf(QLatin1Char('='));
        if (equalSign < 0) {
            printErr(LU::tr("tr-function mapping '%1' in -tr-function-alias is missing the '='.\n").arg(pair));
            return false;
        }
        const bool plusEqual = equalSign > 0 && pair[equalSign-1] == QLatin1Char('+');
        const int trFunctionEnd = plusEqual ? equalSign-1 : equalSign;
        const QString trFunctionName = pair.left(trFunctionEnd).trimmed();
        const QString alias = pair.mid(equalSign+1).trimmed();
        const int trFunction = trFunctionByDefaultName(trFunctionName);
        if (trFunction < 0) {
            printErr(LU::tr("Unknown tr-function '%1' in -tr-function-alias option.\n"
                            "Available tr-functions are: %2")
                     .arg(trFunctionName, availableFunctions().join(QLatin1Char(','))));
            return false;
        }
        if (alias.isEmpty()) {
            printErr(LU::tr("Empty alias for tr-function '%1' in -tr-function-alias option.\n")
                     .arg(trFunctionName));
            return false;
        }
        trFunctionAliasManager.modifyAlias(trFunction, alias.toLatin1(),
                                           plusEqual ? TrFunctionAliasManager::AddAlias : TrFunctionAliasManager::SetAlias);
    }
    return true;
}

static void updateTsFiles(const Translator &fetchedTor, const QStringList &tsFileNames,
    const QStringList &alienFiles,
    const QString &sourceLanguage, const QString &targetLanguage,
    UpdateOptions options, bool *fail)
{
    QList<Translator> aliens;
    foreach (const QString &fileName, alienFiles) {
        ConversionData cd;
        Translator tor;
        if (!tor.load(fileName, cd, QLatin1String("auto"))) {
            printErr(cd.error());
            *fail = true;
            continue;
        }
        tor.resolveDuplicates();
        aliens << tor;
    }

    QDir dir;
    QString err;
    foreach (const QString &fileName, tsFileNames) {
        QString fn = dir.relativeFilePath(fileName);
        ConversionData cd;
        Translator tor;
        cd.m_sortContexts = !(options & NoSort);
        if (QFile(fileName).exists()) {
            if (!tor.load(fileName, cd, QLatin1String("auto"))) {
                printErr(cd.error());
                *fail = true;
                continue;
            }
            tor.resolveDuplicates();
            cd.clearErrors();
            if (!targetLanguage.isEmpty() && targetLanguage != tor.languageCode())
                printErr(LU::tr("lupdate warning: Specified target language '%1' disagrees with"
                                " existing file's language '%2'. Ignoring.\n")
                         .arg(targetLanguage, tor.languageCode()));
            if (!sourceLanguage.isEmpty() && sourceLanguage != tor.sourceLanguageCode())
                printErr(LU::tr("lupdate warning: Specified source language '%1' disagrees with"
                                " existing file's language '%2'. Ignoring.\n")
                         .arg(sourceLanguage, tor.sourceLanguageCode()));
        } else {
            if (!targetLanguage.isEmpty())
                tor.setLanguageCode(targetLanguage);
            else
                tor.setLanguageCode(Translator::guessLanguageCodeFromFileName(fileName));
            if (!sourceLanguage.isEmpty())
                tor.setSourceLanguageCode(sourceLanguage);
        }
        tor.makeFileNamesAbsolute(QFileInfo(fileName).absoluteDir());
        if (options & NoLocations)
            tor.setLocationsType(Translator::NoLocations);
        else if (options & RelativeLocations)
            tor.setLocationsType(Translator::RelativeLocations);
        else if (options & AbsoluteLocations)
            tor.setLocationsType(Translator::AbsoluteLocations);
        if (options & Verbose)
            printOut(LU::tr("Updating '%1'...\n").arg(fn));

        UpdateOptions theseOptions = options;
        if (tor.locationsType() == Translator::NoLocations) // Could be set from file
            theseOptions |= NoLocations;
        Translator out = merge(tor, fetchedTor, aliens, theseOptions, err);

        if ((options & Verbose) && !err.isEmpty()) {
            printOut(err);
            err.clear();
        }
        if (options & PluralOnly) {
            if (options & Verbose)
                printOut(LU::tr("Stripping non plural forms in '%1'...\n").arg(fn));
            out.stripNonPluralForms();
        }
        if (options & NoObsolete)
            out.stripObsoleteMessages();
        out.stripEmptyContexts();

        out.normalizeTranslations(cd);
        if (!cd.errors().isEmpty()) {
            printErr(cd.error());
            cd.clearErrors();
        }
        if (!out.save(fileName, cd, QLatin1String("auto"))) {
            printErr(cd.error());
            *fail = true;
        }
    }
}

static void print(const QString &fileName, int lineNo, int type, const QString &msg)
{
    QString pfx = ((type & QMakeHandler::CategoryMask) == QMakeHandler::WarningMessage)
                  ? QString::fromLatin1("WARNING: ") : QString();
    if (lineNo > 0)
        printErr(QString::fromLatin1("%1%2:%3: %4\n").arg(pfx, fileName, QString::number(lineNo), msg));
    else if (lineNo)
        printErr(QString::fromLatin1("%1%2: %3\n").arg(pfx, fileName, msg));
    else
        printErr(QString::fromLatin1("%1%2\n").arg(pfx, msg));
}

class EvalHandler : public QMakeHandler {
public:
    virtual void message(int type, const QString &msg, const QString &fileName, int lineNo)
        { if (verbose) print(fileName, lineNo, type, msg); }

    virtual void fileMessage(const QString &msg)
        { printErr(msg + QLatin1Char('\n')); }

    virtual void aboutToEval(ProFile *, ProFile *, EvalFileType) {}
    virtual void doneWithEval(ProFile *) {}

    bool verbose;
};

static EvalHandler evalHandler;

static QStringList getSources(const char *var, const char *vvar, const QStringList &baseVPaths,
                              const QString &projectDir, const ProFileEvaluator &visitor)
{
    QStringList vPaths = visitor.absolutePathValues(QLatin1String(vvar), projectDir);
    vPaths += baseVPaths;
    vPaths.removeDuplicates();
    return visitor.absoluteFileValues(QLatin1String(var), projectDir, vPaths, 0);
}

static QStringList getSources(const ProFileEvaluator &visitor, const QString &projectDir)
{
    QStringList baseVPaths;
    baseVPaths += visitor.absolutePathValues(QLatin1String("VPATH"), projectDir);
    baseVPaths << projectDir; // QMAKE_ABSOLUTE_SOURCE_PATH
    baseVPaths.removeDuplicates();

    QStringList sourceFiles;

    // app/lib template
    sourceFiles += getSources("SOURCES", "VPATH_SOURCES", baseVPaths, projectDir, visitor);

    sourceFiles += getSources("FORMS", "VPATH_FORMS", baseVPaths, projectDir, visitor);
    sourceFiles += getSources("FORMS3", "VPATH_FORMS3", baseVPaths, projectDir, visitor);

    QStringList vPathsInc = baseVPaths;
    vPathsInc += visitor.absolutePathValues(QLatin1String("INCLUDEPATH"), projectDir);
    vPathsInc.removeDuplicates();
    sourceFiles += visitor.absoluteFileValues(QLatin1String("HEADERS"), projectDir, vPathsInc, 0);

    sourceFiles.removeDuplicates();
    sourceFiles.sort();

    QStringList excludes;
    foreach (QString ex, visitor.values(QLatin1String("TR_EXCLUDE"))) {
        if (!QFileInfo(ex).isAbsolute())
            ex = QDir(projectDir).absoluteFilePath(ex);
        excludes << QDir::cleanPath(ex);
    }
    foreach (const QString &ex, excludes) {
        // TODO: take advantage of the file list being sorted
        QRegExp rx(ex, Qt::CaseSensitive, QRegExp::Wildcard);
        for (QStringList::Iterator it = sourceFiles.begin(); it != sourceFiles.end(); ) {
            if (rx.exactMatch(*it))
                it = sourceFiles.erase(it);
            else
                ++it;
        }
    }

    return sourceFiles;
}

static void processSources(Translator &fetchedTor,
                           const QStringList &sourceFiles, ConversionData &cd)
{
#ifdef QT_NO_QML
    bool requireQmlSupport = false;
#endif
    QStringList sourceFilesCpp;
    for (QStringList::const_iterator it = sourceFiles.begin(); it != sourceFiles.end(); ++it) {
        if (it->endsWith(QLatin1String(".java"), Qt::CaseInsensitive))
            loadJava(fetchedTor, *it, cd);
        else if (it->endsWith(QLatin1String(".ui"), Qt::CaseInsensitive)
                 || it->endsWith(QLatin1String(".jui"), Qt::CaseInsensitive))
            loadUI(fetchedTor, *it, cd);
#ifndef QT_NO_QML
        else if (it->endsWith(QLatin1String(".js"), Qt::CaseInsensitive)
                 || it->endsWith(QLatin1String(".qs"), Qt::CaseInsensitive))
            loadQScript(fetchedTor, *it, cd);
        else if (it->endsWith(QLatin1String(".qml"), Qt::CaseInsensitive))
            loadQml(fetchedTor, *it, cd);
#else
        else if (it->endsWith(QLatin1String(".qml"), Qt::CaseInsensitive)
                 || it->endsWith(QLatin1String(".js"), Qt::CaseInsensitive)
                 || it->endsWith(QLatin1String(".qs"), Qt::CaseInsensitive))
            requireQmlSupport = true;
#endif // QT_NO_QML
        else
            sourceFilesCpp << *it;
    }

#ifdef QT_NO_QML
    if (requireQmlSupport)
        printErr(LU::tr("lupdate warning: Some files have been ignored due to missing qml/javascript support\n"));
#endif

    loadCPP(fetchedTor, sourceFilesCpp, cd);
    if (!cd.error().isEmpty())
        printErr(cd.error());
}

static void processProjects(bool topLevel, bool nestComplain, const QStringList &proFiles,
        const QHash<QString, QString> &outDirMap, ProFileGlobals *option, QMakeParser *parser,
        UpdateOptions options,
        const QString &targetLanguage, const QString &sourceLanguage,
        Translator *parentTor, bool *fail);

static void processProject(
        bool nestComplain, const QString &proFile,
        ProFileGlobals *option, QMakeParser *parser, ProFileEvaluator &visitor,
        UpdateOptions options,
        const QString &targetLanguage, const QString &sourceLanguage,
        Translator *fetchedTor, bool *fail)
{
    QStringList tmp = visitor.values(QLatin1String("CODECFORSRC"));
    if (!tmp.isEmpty()) {
        QByteArray codecForSource = tmp.last().toLatin1().toUpper();
        if (codecForSource == "UTF16" || codecForSource == "UTF-16") {
            options |= SourceIsUtf16;
        } else if (codecForSource == "UTF8" || codecForSource == "UTF-8") {
            options &= ~SourceIsUtf16;
        } else {
            printErr(LU::tr("lupdate warning: Codec for source '%1' is invalid."
                            " Falling back to UTF-8.\n")
                     .arg(QString::fromLatin1(codecForSource)));
            options &= ~SourceIsUtf16;
        }
    }
    QString proPath = QFileInfo(proFile).path();
    if (visitor.templateType() == ProFileEvaluator::TT_Subdirs) {
        QStringList subProFiles;
        QDir proDir(proPath);
        foreach (const QString &subdir, visitor.values(QLatin1String("SUBDIRS"))) {
            QString realdir = visitor.value(subdir + QLatin1String(".subdir"));
            if (realdir.isEmpty())
                realdir = visitor.value(subdir + QLatin1String(".file"));
            if (realdir.isEmpty())
                realdir = subdir;
            QString subPro = QDir::cleanPath(proDir.absoluteFilePath(realdir));
            QFileInfo subInfo(subPro);
            if (subInfo.isDir())
                subProFiles << (subPro + QLatin1Char('/')
                                + subInfo.fileName() + QLatin1String(".pro"));
            else
                subProFiles << subPro;
        }
        processProjects(false, nestComplain, subProFiles, QHash<QString, QString>(),
                        option, parser, options,
                        targetLanguage, sourceLanguage, fetchedTor, fail);
    } else {
        ConversionData cd;
        cd.m_noUiLines = options & NoUiLines;
        cd.m_sourceIsUtf16 = options & SourceIsUtf16;
        cd.m_includePath = visitor.values(QLatin1String("INCLUDEPATH"));
        QStringList sourceFiles = getSources(visitor, proPath);
        QSet<QString> sourceDirs;
        sourceDirs.insert(proPath + QLatin1Char('/'));
        foreach (const QString &sf, sourceFiles)
            sourceDirs.insert(sf.left(sf.lastIndexOf(QLatin1Char('/')) + 1));
        QStringList rootList = sourceDirs.toList();
        rootList.sort();
        for (int prev = 0, curr = 1; curr < rootList.length(); )
            if (rootList.at(curr).startsWith(rootList.at(prev)))
                rootList.removeAt(curr);
            else
                prev = curr++;
        cd.m_projectRoots = QSet<QString>::fromList(rootList);
        processSources(*fetchedTor, sourceFiles, cd);
    }
}

static void processProjects(bool topLevel, bool nestComplain, const QStringList &proFiles,
        const QHash<QString, QString> &outDirMap, ProFileGlobals *option, QMakeParser *parser,
        UpdateOptions options,
        const QString &targetLanguage, const QString &sourceLanguage,
        Translator *parentTor, bool *fail)
{
    foreach (const QString &proFile, proFiles) {

        if (!outDirMap.isEmpty())
            option->setDirectories(QFileInfo(proFile).path(), outDirMap[proFile]);

        ProFileEvaluator visitor(option, parser, &evalHandler);
        visitor.setCumulative(true);
        visitor.setOutputDir(option->shadowedPath(proFile));
        ProFile *pro;
        if (!(pro = parser->parsedProFile(proFile))) {
            if (topLevel)
                *fail = true;
            continue;
        }
        if (!visitor.accept(pro)) {
            if (topLevel)
                *fail = true;
            pro->deref();
            continue;
        }

        if (visitor.contains(QLatin1String("TRANSLATIONS"))) {
            if (parentTor) {
                if (topLevel) {
                    printErr(LU::tr("lupdate warning: TS files from command line "
                                    "will override TRANSLATIONS in %1.\n").arg(proFile));
                    goto noTrans;
                } else if (nestComplain) {
                    printErr(LU::tr("lupdate warning: TS files from command line "
                                    "prevent recursing into %1.\n").arg(proFile));
                    pro->deref();
                    continue;
                }
            }
            QStringList tsFiles;
            QDir proDir(QFileInfo(proFile).path());
            foreach (const QString &tsFile, visitor.values(QLatin1String("TRANSLATIONS")))
                tsFiles << QFileInfo(proDir, tsFile).filePath();
            if (tsFiles.isEmpty()) {
                // This might mean either a buggy PRO file or an intentional detach -
                // we can't know without seeing the actual RHS of the assignment ...
                // Just assume correctness and be silent.
                pro->deref();
                continue;
            }
            Translator tor;
            processProject(false, proFile, option, parser, visitor, options,
                           targetLanguage, sourceLanguage, &tor, fail);
            updateTsFiles(tor, tsFiles, QStringList(),
                          sourceLanguage, targetLanguage, options, fail);
            pro->deref();
            continue;
        }
      noTrans:
        if (!parentTor) {
            if (topLevel)
                printErr(LU::tr("lupdate warning: no TS files specified. Only diagnostics "
                                "will be produced for '%1'.\n").arg(proFile));
            Translator tor;
            processProject(nestComplain, proFile, option, parser, visitor, options,
                           targetLanguage, sourceLanguage, &tor, fail);
        } else {
            processProject(nestComplain, proFile, option, parser, visitor, options,
                           targetLanguage, sourceLanguage, parentTor, fail);
        }
        pro->deref();
    }
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
#ifndef QT_BOOTSTRAPPED
#ifndef Q_OS_WIN32
    QTranslator translator;
    QTranslator qtTranslator;
    QString sysLocale = QLocale::system().name();
    QString resourceDir = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
    if (translator.load(QLatin1String("linguist_") + sysLocale, resourceDir)
        && qtTranslator.load(QLatin1String("qt_") + sysLocale, resourceDir)) {
        app.installTranslator(&translator);
        app.installTranslator(&qtTranslator);
    }
#endif // Q_OS_WIN32
#endif

    m_defaultExtensions = QLatin1String("java,jui,ui,c,c++,cc,cpp,cxx,ch,h,h++,hh,hpp,hxx,js,qs,qml");

    QStringList args = app.arguments();
    QStringList tsFileNames;
    QStringList proFiles;
    QString outDir = QDir::currentPath();
    QHash<QString, QString> outDirMap;
    QMultiHash<QString, QString> allCSources;
    QSet<QString> projectRoots;
    QStringList sourceFiles;
    QStringList includePath;
    QStringList alienFiles;
    QString targetLanguage;
    QString sourceLanguage;

    UpdateOptions options =
        Verbose | // verbose is on by default starting with Qt 4.2
        HeuristicSameText | HeuristicSimilarText | HeuristicNumber;
    int numFiles = 0;
    bool metTsFlag = false;
    bool recursiveScan = true;

    QString extensions = m_defaultExtensions;
    QSet<QString> extensionsNameFilters;

    for (int i = 1; i < args.size(); ++i) {
        QString arg = args.at(i);
        if (arg == QLatin1String("-help")
                || arg == QLatin1String("--help")
                || arg == QLatin1String("-h")) {
            printUsage();
            return 0;
        } else if (arg == QLatin1String("-list-languages")) {
            printOut(getNumerusInfoString());
            return 0;
        } else if (arg == QLatin1String("-pluralonly")) {
            options |= PluralOnly;
            continue;
        } else if (arg == QLatin1String("-noobsolete")
                || arg == QLatin1String("-no-obsolete")) {
            options |= NoObsolete;
            continue;
        } else if (arg == QLatin1String("-silent")) {
            options &= ~Verbose;
            continue;
        } else if (arg == QLatin1String("-target-language")) {
            ++i;
            if (i == argc) {
                printErr(LU::tr("The option -target-language requires a parameter.\n"));
                return 1;
            }
            targetLanguage = args[i];
            continue;
        } else if (arg == QLatin1String("-source-language")) {
            ++i;
            if (i == argc) {
                printErr(LU::tr("The option -source-language requires a parameter.\n"));
                return 1;
            }
            sourceLanguage = args[i];
            continue;
        } else if (arg == QLatin1String("-disable-heuristic")) {
            ++i;
            if (i == argc) {
                printErr(LU::tr("The option -disable-heuristic requires a parameter.\n"));
                return 1;
            }
            arg = args[i];
            if (arg == QLatin1String("sametext")) {
                options &= ~HeuristicSameText;
            } else if (arg == QLatin1String("similartext")) {
                options &= ~HeuristicSimilarText;
            } else if (arg == QLatin1String("number")) {
                options &= ~HeuristicNumber;
            } else {
                printErr(LU::tr("Invalid heuristic name passed to -disable-heuristic.\n"));
                return 1;
            }
            continue;
        } else if (arg == QLatin1String("-locations")) {
            ++i;
            if (i == argc) {
                printErr(LU::tr("The option -locations requires a parameter.\n"));
                return 1;
            }
            if (args[i] == QLatin1String("none")) {
                options |= NoLocations;
            } else if (args[i] == QLatin1String("relative")) {
                options |= RelativeLocations;
            } else if (args[i] == QLatin1String("absolute")) {
                options |= AbsoluteLocations;
            } else {
                printErr(LU::tr("Invalid parameter passed to -locations.\n"));
                return 1;
            }
            continue;
        } else if (arg == QLatin1String("-no-ui-lines")) {
            options |= NoUiLines;
            continue;
        } else if (arg == QLatin1String("-verbose")) {
            options |= Verbose;
            continue;
        } else if (arg == QLatin1String("-no-recursive")) {
            recursiveScan = false;
            continue;
        } else if (arg == QLatin1String("-recursive")) {
            recursiveScan = true;
            continue;
        } else if (arg == QLatin1String("-no-sort")
                   || arg == QLatin1String("-nosort")) {
            options |= NoSort;
            continue;
        } else if (arg == QLatin1String("-version")) {
            printOut(LU::tr("lupdate version %1\n").arg(QLatin1String(QT_VERSION_STR)));
            return 0;
        } else if (arg == QLatin1String("-ts")) {
            metTsFlag = true;
            continue;
        } else if (arg == QLatin1String("-extensions")) {
            ++i;
            if (i == argc) {
                printErr(LU::tr("The -extensions option should be followed by an extension list.\n"));
                return 1;
            }
            extensions = args[i];
            continue;
        } else if (arg == QLatin1String("-tr-function-alias")) {
            ++i;
            if (i == argc) {
                printErr(LU::tr("The -tr-function-alias option should be followed by a list of function=alias mappings.\n"));
                return 1;
            }
            if (!handleTrFunctionAliases(args[i]))
                return 1;
            continue;
        } else if (arg == QLatin1String("-pro")) {
            ++i;
            if (i == argc) {
                printErr(LU::tr("The -pro option should be followed by a filename of .pro file.\n"));
                return 1;
            }
            QString file = QDir::cleanPath(QFileInfo(args[i]).absoluteFilePath());
            proFiles += file;
            outDirMap[file] = outDir;
            numFiles++;
            continue;
        } else if (arg == QLatin1String("-pro-out")) {
            ++i;
            if (i == argc) {
                printErr(LU::tr("The -pro-out option should be followed by a directory name.\n"));
                return 1;
            }
            outDir = QDir::cleanPath(QFileInfo(args[i]).absoluteFilePath());
            continue;
        } else if (arg.startsWith(QLatin1String("-I"))) {
            if (arg.length() == 2) {
                ++i;
                if (i == argc) {
                    printErr(LU::tr("The -I option should be followed by a path.\n"));
                    return 1;
                }
                includePath += args[i];
            } else {
                includePath += args[i].mid(2);
            }
            continue;
        } else if (arg.startsWith(QLatin1String("-")) && arg != QLatin1String("-")) {
            printErr(LU::tr("Unrecognized option '%1'.\n").arg(arg));
            return 1;
        }

        QStringList files;
        if (arg.startsWith(QLatin1String("@"))) {
            QFile lstFile(arg.mid(1));
            if (!lstFile.open(QIODevice::ReadOnly)) {
                printErr(LU::tr("lupdate error: List file '%1' is not readable.\n")
                         .arg(lstFile.fileName()));
                return 1;
            }
            while (!lstFile.atEnd()) {
                QString lineContent = QString::fromLocal8Bit(lstFile.readLine().trimmed());

                if (lineContent.startsWith(QLatin1Literal("-I"))) {
                    if (lineContent.length() == 2) {
                        printErr(LU::tr("The -I option should be followed by a path.\n"));
                        return 1;
                    }
                    includePath += lineContent.mid(2);
                } else {
                    files << lineContent;
                }
            }
        } else {
            files << arg;
        }
        if (metTsFlag) {
            foreach (const QString &file, files) {
                bool found = false;
                foreach (const Translator::FileFormat &fmt, Translator::registeredFileFormats()) {
                    if (file.endsWith(QLatin1Char('.') + fmt.extension, Qt::CaseInsensitive)) {
                        QFileInfo fi(file);
                        if (!fi.exists() || fi.isWritable()) {
                            tsFileNames.append(QFileInfo(file).absoluteFilePath());
                        } else {
                            printErr(LU::tr("lupdate warning: For some reason, '%1' is not writable.\n")
                                     .arg(file));
                        }
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    printErr(LU::tr("lupdate error: File '%1' has no recognized extension.\n")
                             .arg(file));
                    return 1;
                }
            }
            numFiles++;
        } else {
            foreach (const QString &file, files) {
                QFileInfo fi(file);
                if (!fi.exists()) {
                    printErr(LU::tr("lupdate error: File '%1' does not exist.\n").arg(file));
                    return 1;
                }
                if (file.endsWith(QLatin1String(".pro"), Qt::CaseInsensitive)
                    || file.endsWith(QLatin1String(".pri"), Qt::CaseInsensitive)) {
                    QString cleanFile = QDir::cleanPath(fi.absoluteFilePath());
                    proFiles << cleanFile;
                    outDirMap[cleanFile] = outDir;
                } else if (fi.isDir()) {
                    if (options & Verbose)
                        printOut(LU::tr("Scanning directory '%1'...\n").arg(file));
                    QDir dir = QDir(fi.filePath());
                    projectRoots.insert(dir.absolutePath() + QLatin1Char('/'));
                    if (extensionsNameFilters.isEmpty()) {
                        foreach (QString ext, extensions.split(QLatin1Char(','))) {
                            ext = ext.trimmed();
                            if (ext.startsWith(QLatin1Char('.')))
                                ext.remove(0, 1);
                            extensionsNameFilters.insert(ext);
                        }
                    }
                    QDir::Filters filters = QDir::Files | QDir::NoSymLinks;
                    if (recursiveScan)
                        filters |= QDir::AllDirs | QDir::NoDotAndDotDot;
                    QFileInfoList fileinfolist;
                    recursiveFileInfoList(dir, extensionsNameFilters, filters, &fileinfolist);
                    int scanRootLen = dir.absolutePath().length();
                    foreach (const QFileInfo &fi, fileinfolist) {
                        QString fn = QDir::cleanPath(fi.absoluteFilePath());
                        sourceFiles << fn;

                        if (!fn.endsWith(QLatin1String(".java"))
                            && !fn.endsWith(QLatin1String(".jui"))
                            && !fn.endsWith(QLatin1String(".ui"))
                            && !fn.endsWith(QLatin1String(".js"))
                            && !fn.endsWith(QLatin1String(".qs"))
                            && !fn.endsWith(QLatin1String(".qml"))) {
                            int offset = 0;
                            int depth = 0;
                            do {
                                offset = fn.lastIndexOf(QLatin1Char('/'), offset - 1);
                                QString ffn = fn.mid(offset + 1);
                                allCSources.insert(ffn, fn);
                            } while (++depth < 3 && offset > scanRootLen);
                        }
                    }
                } else {
                    foreach (const Translator::FileFormat &fmt, Translator::registeredFileFormats()) {
                        if (file.endsWith(QLatin1Char('.') + fmt.extension, Qt::CaseInsensitive)) {
                            alienFiles << file;
                            goto gotfile;
                        }
                    }
                    sourceFiles << QDir::cleanPath(fi.absoluteFilePath());;
                    projectRoots.insert(fi.absolutePath() + QLatin1Char('/'));
                }
            }
          gotfile:
            numFiles++;
        }
    } // for args

    if (numFiles == 0) {
        printUsage();
        return 1;
    }

    if (!targetLanguage.isEmpty() && tsFileNames.count() != 1)
        printErr(LU::tr("lupdate warning: -target-language usually only"
                        " makes sense with exactly one TS file.\n"));

    bool fail = false;
    if (proFiles.isEmpty()) {
        if (tsFileNames.isEmpty())
            printErr(LU::tr("lupdate warning:"
                            " no TS files specified. Only diagnostics will be produced.\n"));

        Translator fetchedTor;
        ConversionData cd;
        cd.m_noUiLines = options & NoUiLines;
        cd.m_sourceIsUtf16 = options & SourceIsUtf16;
        cd.m_projectRoots = projectRoots;
        cd.m_includePath = includePath;
        cd.m_allCSources = allCSources;
        processSources(fetchedTor, sourceFiles, cd);
        updateTsFiles(fetchedTor, tsFileNames, alienFiles,
                      sourceLanguage, targetLanguage, options, &fail);
    } else {
        if (!sourceFiles.isEmpty() || !includePath.isEmpty()) {
            printErr(LU::tr("lupdate error:"
                            " Both project and source files / include paths specified.\n"));
            return 1;
        }

        evalHandler.verbose = !!(options & Verbose);
        ProFileGlobals option;
        option.qmake_abslocation = QString::fromLocal8Bit(qgetenv("QMAKE"));
        if (option.qmake_abslocation.isEmpty())
            option.qmake_abslocation = app.applicationDirPath() + QLatin1String("/qmake");
        option.initProperties();
        option.setCommandLineArguments(QDir::currentPath(),
                                       QStringList() << QLatin1String("CONFIG+=lupdate_run"));
        QMakeParser parser(0, &evalHandler);

        if (!tsFileNames.isEmpty()) {
            Translator fetchedTor;
            processProjects(true, true, proFiles, outDirMap, &option, &parser, options,
                            targetLanguage, sourceLanguage, &fetchedTor, &fail);
            updateTsFiles(fetchedTor, tsFileNames, alienFiles,
                          sourceLanguage, targetLanguage, options, &fail);
        } else {
            processProjects(true, false, proFiles, outDirMap, &option, &parser, options,
                            targetLanguage, sourceLanguage, 0, &fail);
        }
    }
    return fail ? 1 : 0;
}
