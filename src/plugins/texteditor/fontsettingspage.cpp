/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://www.qtsoftware.com/contact.
**
**************************************************************************/

#include "fontsettingspage.h"

#include "editcolorschemedialog.h"
#include "fontsettings.h"
#include "texteditorconstants.h"
#include "ui_fontsettingspage.h"

#include <coreplugin/icore.h>
#include <utils/settingsutils.h>

#include <QtCore/QDebug>
#include <QtCore/QSettings>
#include <QtCore/QTimer>
#include <QtGui/QCheckBox>
#include <QtGui/QComboBox>
#include <QtGui/QFileDialog>
#include <QtGui/QFontDatabase>
#include <QtGui/QListWidget>
#include <QtGui/QMessageBox>
#include <QtGui/QPalette>
#include <QtGui/QPalette>
#include <QtGui/QTextCharFormat>
#include <QtGui/QTextEdit>
#include <QtGui/QToolButton>

namespace TextEditor {
namespace Internal {

class FontSettingsPagePrivate
{
public:
    FontSettingsPagePrivate(const TextEditor::FormatDescriptions &fd,
                            const QString &name,
                            const QString &category,
                            const QString &trCategory);

public:
    const QString m_name;
    const QString m_settingsGroup;
    const QString m_category;
    const QString m_trCategory;

    TextEditor::FormatDescriptions m_descriptions;
    FontSettings m_value;
    FontSettings m_lastValue;
    Ui::FontSettingsPage ui;
};

struct ColorSchemeEntry
{
    QString fileName;
    QString name;
    bool readOnly;
};

} // namespace Internal
} // namespace TextEditor

Q_DECLARE_METATYPE(TextEditor::Internal::ColorSchemeEntry)

using namespace TextEditor;
using namespace TextEditor::Internal;

static QString customStylesPath()
{
    QString path = QFileInfo(Core::ICore::instance()->settings()->fileName()).path();
    path.append(QLatin1String("/qtcreator/styles/"));
    return path;
}

static QString createColorSchemeFileName(const QString &pattern)
{
    const QString stylesPath = customStylesPath();
    QString baseFileName = stylesPath;
    baseFileName += pattern;

    // Find an available file name
    int i = 1;
    QString fileName;
    do {
        fileName = baseFileName.arg((i == 1) ? QString() : QString::number(i));
        ++i;
    } while (QFile::exists(fileName));

    // Create the base directory when it doesn't exist
    if (!QFile::exists(stylesPath) && !QDir().mkpath(stylesPath)) {
        qWarning() << "Failed to create color scheme directory:" << stylesPath;
        return QString();
    }

    return fileName;
}

// ------- FontSettingsPagePrivate
FontSettingsPagePrivate::FontSettingsPagePrivate(const TextEditor::FormatDescriptions &fd,
                                                 const QString &name,
                                                 const QString &category,
                                                 const QString &trCategory) :
    m_name(name),
    m_settingsGroup(Core::Utils::settingsKey(category)),
    m_category(category),
    m_trCategory(trCategory),
    m_descriptions(fd)
{
    bool settingsFound = false;
    QSettings *settings = Core::ICore::instance()->settings();
    if (settings)
        settingsFound = m_value.fromSettings(m_settingsGroup, m_descriptions, settings);

    if (!settingsFound) { // Apply defaults
        foreach (const FormatDescription &f, m_descriptions) {
            const QString name = f.name();

            m_value.formatFor(name).setForeground(f.foreground());
            m_value.formatFor(name).setBackground(f.background());
            m_value.formatFor(name).setBold(f.format().bold());
            m_value.formatFor(name).setItalic(f.format().italic());
        }
    } else if (m_value.colorSchemeFileName().isEmpty()) {
        // No color scheme was loaded, but one might be imported from the ini file
        ColorScheme defaultScheme;
        foreach (const FormatDescription &f, m_descriptions) {
            const QString name = f.name();
            defaultScheme.formatFor(name).setForeground(f.foreground());
            defaultScheme.formatFor(name).setBackground(f.background());
            defaultScheme.formatFor(name).setBold(f.format().bold());
            defaultScheme.formatFor(name).setItalic(f.format().italic());
        }
        if (m_value.colorScheme() != defaultScheme) {
            // Save it as a color scheme file
            QString schemeFileName = createColorSchemeFileName(QLatin1String("customized%1.xml"));
            if (!schemeFileName.isEmpty()) {
                if (m_value.saveColorScheme(schemeFileName) && settings)
                    m_value.toSettings(m_category, settings);
            }
        }
    }

    m_lastValue = m_value;
}


// ------- FormatDescription
FormatDescription::FormatDescription(const QString &name, const QString &trName, const QColor &color) :
    m_name(name),
    m_trName(trName)
{
    m_format.setForeground(color);
}

QColor FormatDescription::foreground() const
{
    if (m_name == QLatin1String(Constants::C_LINE_NUMBER)) {
        const QColor bg = QApplication::palette().background().color();
        if (bg.value() < 128) {
            return QApplication::palette().foreground().color();
        } else {
            return QApplication::palette().dark().color();
        }
    } else if (m_name == QLatin1String(Constants::C_CURRENT_LINE_NUMBER)) {
        const QColor bg = QApplication::palette().background().color();
        if (bg.value() < 128) {
            return QApplication::palette().foreground().color();
        } else {
            return m_format.foreground();
        }
    } else if (m_name == QLatin1String(Constants::C_PARENTHESES)) {
        return QColor(Qt::red);
    }
    return m_format.foreground();
}

QColor FormatDescription::background() const
{
    if (m_name == QLatin1String(Constants::C_TEXT))
        return Qt::white;
    else if (m_name == QLatin1String(Constants::C_LINE_NUMBER))
        return QApplication::palette().background().color();
    else if (m_name == QLatin1String(Constants::C_SEARCH_RESULT))
        return QColor(0xffef0b);
    else if (m_name == QLatin1String(Constants::C_PARENTHESES))
        return QColor(0xb4, 0xee, 0xb4);
    else if (m_name == QLatin1String(Constants::C_CURRENT_LINE)
             || m_name == QLatin1String(Constants::C_SEARCH_SCOPE)) {
        const QPalette palette = QApplication::palette();
        const QColor &fg = palette.color(QPalette::Highlight);
        const QColor &bg = palette.color(QPalette::Base);

        qreal smallRatio;
        qreal largeRatio;
        if (m_name == QLatin1String(Constants::C_CURRENT_LINE)) {
            smallRatio = .15;
            largeRatio = .3;
        } else {
            smallRatio = .05;
            largeRatio = .4;
        }
        const qreal ratio = ((palette.color(QPalette::Text).value() < 128)
                             ^ (palette.color(QPalette::HighlightedText).value() < 128)) ? smallRatio : largeRatio;

        const QColor &col = QColor::fromRgbF(fg.redF() * ratio + bg.redF() * (1 - ratio),
                                             fg.greenF() * ratio + bg.greenF() * (1 - ratio),
                                             fg.blueF() * ratio + bg.blueF() * (1 - ratio));
        return col;
    } else if (m_name == QLatin1String(Constants::C_SELECTION)) {
        const QPalette palette = QApplication::palette();
        return palette.color(QPalette::Highlight);
    } else if (m_name == QLatin1String(Constants::C_OCCURRENCES)) {
        return QColor(220, 220, 220);
    } else if (m_name == QLatin1String(Constants::C_OCCURRENCES_RENAME)) {
        return QColor(255, 200, 200);
    }
    return QColor(); // invalid color
}


//  ------------ FontSettingsPage
FontSettingsPage::FontSettingsPage(const FormatDescriptions &fd,
                                   const QString &category,
                                   const QString &trCategory,
                                   QObject *parent) :
    Core::IOptionsPage(parent),
    d_ptr(new FontSettingsPagePrivate(fd, tr("Font & Colors"), category, trCategory))
{
}

FontSettingsPage::~FontSettingsPage()
{
    delete d_ptr;
}

QString FontSettingsPage::id() const
{
    return d_ptr->m_name;
}

QString FontSettingsPage::trName() const
{
    return d_ptr->m_name;
}

QString FontSettingsPage::category() const
{
    return d_ptr->m_category;
}

QString FontSettingsPage::trCategory() const
{
    return d_ptr->m_trCategory;
}

QWidget *FontSettingsPage::createPage(QWidget *parent)
{
    QWidget *w = new QWidget(parent);
    d_ptr->ui.setupUi(w);

    d_ptr->ui.schemeListWidget->addItem(tr("Default"));
    d_ptr->ui.schemeListWidget->setCurrentIndex(d_ptr->ui.schemeListWidget->model()->index(0, 0));
    d_ptr->ui.editButton->setEnabled(true);

    QFontDatabase db;
    const QStringList families = db.families();
    d_ptr->ui.familyComboBox->addItems(families);
    const int idx = families.indexOf(d_ptr->m_value.family());
    d_ptr->ui.familyComboBox->setCurrentIndex(idx);

    d_ptr->ui.antialias->setChecked(d_ptr->m_value.antialias());

    connect(d_ptr->ui.familyComboBox, SIGNAL(activated(int)), this, SLOT(updatePointSizes()));
    connect(d_ptr->ui.schemeListWidget, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)),
            this, SLOT(colorSchemeSelected(QListWidgetItem*)));
    connect(d_ptr->ui.cloneButton, SIGNAL(clicked()), this, SLOT(cloneColorScheme()));
    connect(d_ptr->ui.editButton, SIGNAL(clicked()), this, SLOT(editColorScheme()));
    connect(d_ptr->ui.deleteButton, SIGNAL(clicked()), this, SLOT(deleteColorScheme()));

    updatePointSizes();
    refreshColorSchemeList();
    d_ptr->m_lastValue = d_ptr->m_value;
    return w;
}

void FontSettingsPage::updatePointSizes()
{
    const int oldSize = d_ptr->m_value.fontSize();
    if (d_ptr->ui.sizeComboBox->count()) {
        const QString curSize = d_ptr->ui.sizeComboBox->currentText();
        bool ok = true;
        int oldSize = curSize.toInt(&ok);
        if (!ok)
            oldSize = d_ptr->m_value.fontSize();
        d_ptr->ui.sizeComboBox->clear();
    }
    QFontDatabase db;
    const QList<int> sizeLst = db.pointSizes(d_ptr->ui.familyComboBox->currentText());
    int idx = 0;
    int i = 0;
    for (; i < sizeLst.count(); ++i) {
        if (idx == 0 && sizeLst.at(i) >= oldSize)
            idx = i;
        d_ptr->ui.sizeComboBox->addItem(QString::number(sizeLst.at(i)));
    }
    if (d_ptr->ui.sizeComboBox->count())
        d_ptr->ui.sizeComboBox->setCurrentIndex(idx);
}

void FontSettingsPage::colorSchemeSelected(QListWidgetItem *item)
{
    bool modifiable = false;
    if (item) {
        const ColorSchemeEntry entry = item->data(Qt::UserRole).value<ColorSchemeEntry>();
        modifiable = !entry.readOnly;
    }
    d_ptr->ui.cloneButton->setEnabled(item != 0);
    d_ptr->ui.deleteButton->setEnabled(modifiable);
    d_ptr->ui.editButton->setEnabled(modifiable);
}

void FontSettingsPage::cloneColorScheme()
{
    QListWidgetItem *item = d_ptr->ui.schemeListWidget->currentItem();
    if (!item)
        return;

    const ColorSchemeEntry entry = item->data(Qt::UserRole).value<ColorSchemeEntry>();

    // Load the currently selected color scheme
    if (!d_ptr->m_value.loadColorScheme(entry.fileName, d_ptr->m_descriptions))
        return;

    QString baseFileName = QFileInfo(entry.fileName).completeBaseName();
    baseFileName += QLatin1String("_copy%1.xml");
    QString fileName = createColorSchemeFileName(baseFileName);

    if (!fileName.isEmpty()) {
        ColorScheme scheme = d_ptr->m_value.colorScheme();
        scheme.setName(tr("%1 (copy)").arg(scheme.name()));
        scheme.save(fileName);
        d_ptr->m_value.setColorSchemeFileName(fileName);

        refreshColorSchemeList();
    }
}

void FontSettingsPage::deleteColorScheme()
{
    QListWidgetItem *item = d_ptr->ui.schemeListWidget->currentItem();
    if (!item)
        return;

    const ColorSchemeEntry entry = item->data(Qt::UserRole).value<ColorSchemeEntry>();
    if (!entry.readOnly) {
        int ret = QMessageBox::warning(d_ptr->ui.deleteButton->window(),
                                       tr("Delete Color Scheme"),
                                       tr("Are you sure you want to delete this color scheme permanently?"),
                                       QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

        if (ret == QMessageBox::Yes && QFile::remove(entry.fileName))
            refreshColorSchemeList();
    }
}

void FontSettingsPage::editColorScheme()
{
    QListWidgetItem *item = d_ptr->ui.schemeListWidget->currentItem();
    if (!item)
        return;

    const ColorSchemeEntry entry = item->data(Qt::UserRole).value<ColorSchemeEntry>();
    if (entry.readOnly)
        return;

    d_ptr->m_value.setFamily(d_ptr->ui.familyComboBox->currentText());
    d_ptr->m_value.setAntialias(d_ptr->ui.antialias->isChecked());

    bool ok = true;
    const int size = d_ptr->ui.sizeComboBox->currentText().toInt(&ok);
    if (ok)
        d_ptr->m_value.setFontSize(size);

    if (!d_ptr->m_value.loadColorScheme(entry.fileName, d_ptr->m_descriptions))
        return;

    EditColorSchemeDialog dialog(d_ptr->m_descriptions,
                                 d_ptr->m_value,
                                 d_ptr->ui.editButton->window());

    if (dialog.exec() == QDialog::Accepted) {
        ColorScheme newColorScheme = dialog.colorScheme();
        newColorScheme.save(entry.fileName);
        d_ptr->m_value.setColorScheme(newColorScheme);
        refreshColorSchemeList();
    }
}

void FontSettingsPage::addColorSchemeEntry(const QString &fileName, bool readOnly)
{
    ColorSchemeEntry entry;
    entry.fileName = fileName;
    entry.name = ColorScheme::readNameOfScheme(fileName);
    entry.readOnly = readOnly;

    QListWidgetItem *item = new QListWidgetItem(entry.name);
    item->setData(Qt::UserRole, QVariant::fromValue<ColorSchemeEntry>(entry));
    d_ptr->ui.schemeListWidget->addItem(item);
}

void FontSettingsPage::refreshColorSchemeList()
{
    d_ptr->ui.schemeListWidget->clear();

    QString resourcePath = Core::ICore::instance()->resourcePath();
    QDir styleDir(resourcePath + QLatin1String("/styles"));
    styleDir.setNameFilters(QStringList() << QLatin1String("*.xml"));
    styleDir.setFilter(QDir::Files);

    int selected = 0;
    int count = 0;

    foreach (const QString &file, styleDir.entryList()) {
        const QString fileName = styleDir.absoluteFilePath(file);
        addColorSchemeEntry(fileName, true);
        if (d_ptr->m_value.colorSchemeFileName() == fileName)
            selected = count;
        ++count;
    }

    styleDir.setPath(customStylesPath());

    foreach (const QString &file, styleDir.entryList()) {
        const QString fileName = styleDir.absoluteFilePath(file);
        addColorSchemeEntry(fileName, false);
        if (d_ptr->m_value.colorSchemeFileName() == fileName)
            selected = count;
        ++count;
    }

    const QModelIndex s = d_ptr->ui.schemeListWidget->model()->index(selected, 0);
    d_ptr->ui.schemeListWidget->setCurrentIndex(s);
}

void FontSettingsPage::delayedChange()
{
    emit changed(d_ptr->m_value);
}

void FontSettingsPage::apply()
{
    d_ptr->m_value.setFamily(d_ptr->ui.familyComboBox->currentText());
    d_ptr->m_value.setAntialias(d_ptr->ui.antialias->isChecked());

    if (QListWidgetItem *item = d_ptr->ui.schemeListWidget->currentItem()) {
        const ColorSchemeEntry entry = item->data(Qt::UserRole).value<ColorSchemeEntry>();
        if (entry.fileName != d_ptr->m_value.colorSchemeFileName())
            d_ptr->m_value.loadColorScheme(entry.fileName, d_ptr->m_descriptions);
    }

    bool ok = true;
    const int size = d_ptr->ui.sizeComboBox->currentText().toInt(&ok);
    if (ok)
        d_ptr->m_value.setFontSize(size);
    saveSettings();
}

void FontSettingsPage::saveSettings()
{
    if (d_ptr->m_value != d_ptr->m_lastValue) {
	d_ptr->m_lastValue = d_ptr->m_value;
	if (QSettings *settings = Core::ICore::instance()->settings())
	    d_ptr->m_value.toSettings(d_ptr->m_settingsGroup, settings);

	QTimer::singleShot(0, this, SLOT(delayedChange()));
    }
}

void FontSettingsPage::finish()
{
    // If changes were applied, these are equal. Otherwise restores last value.
    d_ptr->m_value = d_ptr->m_lastValue;
}

const FontSettings &FontSettingsPage::fontSettings() const
{
    return d_ptr->m_value;
}
