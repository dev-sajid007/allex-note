#include "mainwindow.hpp"
#include "crypto.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMessageBox>
#include <QStandardPaths>
#include <QApplication>
#include <QDir>
#include <QStatusBar>
#include <QShortcut>
#include <QFileDialog>
#include <QMenuBar>
#include <QMenu>
#include <QColorDialog>
#include <QInputDialog>
#include <QPrinter>
#include <QTextDocument>
#include <QCloseEvent>
#include <QRegularExpression>
#include <QDateTimeEdit>
#include <QProcess>
#include <QSystemTrayIcon>
#include <QTextCursor>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_manager(new NoteManager(
          QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/data"))
    , m_auth(new AuthManager(this))
    , m_sync(new SyncManager(m_auth,
          QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/data",
          this))
    , m_autoSaveTimer(new QTimer(this))
    , m_reminderTimer(new QTimer(this))
    , m_settings("Allex", "AllexNotes")
{
    m_masterPasswordHash = m_settings.value("master/passwordHash").toString();
    m_masterSalt = m_settings.value("master/salt").toByteArray();

    setupUi();
    setupMenu();
    setupShortcuts();
    setupTray();
    setupSync();
    applyIcon();

    connect(m_autoSaveTimer, &QTimer::timeout, this, &MainWindow::autoSave);
    m_autoSaveTimer->setSingleShot(true);

    connect(m_reminderTimer, &QTimer::timeout, this, &MainWindow::checkReminders);
    m_reminderTimer->start(30000);

    if (isDarkMode()) toggleDarkMode();
    restoreSession();

    if (!m_masterPasswordHash.isEmpty() && !m_appLocked) {
        QTimer::singleShot(100, this, [this]() { lockApp(); });
    }
}

void MainWindow::applyIcon() {
    QIcon icon(":/allex-notes-128.png");
    setWindowIcon(icon);
    if (m_trayIcon) m_trayIcon->setIcon(icon);
}

// --- UI ---

void MainWindow::setupUi() {
    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    m_splitter = new QSplitter(Qt::Horizontal, this);

    // --- Sidebar ---
    QWidget *sidebar = new QWidget;
    QVBoxLayout *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(8, 8, 8, 8);

    QHBoxLayout *topRow = new QHBoxLayout;
    m_searchBox = new QLineEdit;
    m_searchBox->setPlaceholderText("Search...");
    m_darkModeBtn = new QPushButton("D");
    m_darkModeBtn->setFixedWidth(32);
    m_darkModeBtn->setToolTip("Toggle dark mode (Ctrl+D)");
    topRow->addWidget(m_searchBox);
    topRow->addWidget(m_darkModeBtn);
    sideLayout->addLayout(topRow);

    m_newButton = new QPushButton("+ New");
    sideLayout->addWidget(m_newButton);

    QHBoxLayout *sortRow = new QHBoxLayout;
    QLabel *sortLabel = new QLabel("Sort:");
    m_sortCombo = new QComboBox;
    m_sortCombo->addItems({"Modified", "Created", "Title"});
    sortRow->addWidget(sortLabel);
    sortRow->addWidget(m_sortCombo);
    sideLayout->addLayout(sortRow);

    m_leftStack = new QStackedWidget;

    m_notesPage = new QWidget;
    QVBoxLayout *notesLayout = new QVBoxLayout(m_notesPage);
    notesLayout->setContentsMargins(0, 0, 0, 0);

    m_folderTree = new QTreeWidget;
    m_folderTree->setHeaderHidden(true);
    m_folderTree->setRootIsDecorated(false);
    m_folderTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_folderTree->setMaximumHeight(160);
    notesLayout->addWidget(m_folderTree);

    m_noteList = new QListWidget;
    m_noteList->setObjectName("noteList");
    m_noteList->setContextMenuPolicy(Qt::CustomContextMenu);
    notesLayout->addWidget(m_noteList);

    m_leftStack->addWidget(m_notesPage);

    m_trashPage = new QWidget;
    QVBoxLayout *trashLayout = new QVBoxLayout(m_trashPage);
    trashLayout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout *trashHeader = new QHBoxLayout;
    QPushButton *trashBackBtn = new QPushButton("\u2190 Back");
    QPushButton *emptyTrashBtn = new QPushButton("Empty Trash");
    emptyTrashBtn->setStyleSheet("background: #e74c3c; color: white;");
    trashHeader->addWidget(trashBackBtn);
    trashHeader->addStretch();
    trashHeader->addWidget(emptyTrashBtn);
    trashLayout->addLayout(trashHeader);

    m_trashList = new QListWidget;
    m_trashList->setObjectName("noteList");
    trashLayout->addWidget(m_trashList);

    m_leftStack->addWidget(m_trashPage);
    m_leftStack->setCurrentIndex(0);
    sideLayout->addWidget(m_leftStack);

    QPushButton *trashBtn = new QPushButton("Trash");
    trashBtn->setObjectName("trashBtn");
    sideLayout->addWidget(trashBtn);

    m_splitter->addWidget(sidebar);

    // --- Editor ---
    QWidget *editorArea = new QWidget;
    QVBoxLayout *editorLayout = new QVBoxLayout(editorArea);
    editorLayout->setContentsMargins(8, 8, 8, 8);

    m_titleEdit = new QLineEdit;
    m_titleEdit->setPlaceholderText("Note title...");
    QFont titleFont = m_titleEdit->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    m_titleEdit->setFont(titleFont);
    editorLayout->addWidget(m_titleEdit);

    QHBoxLayout *editorToolbar = new QHBoxLayout;
    m_previewBtn = new QPushButton("Preview");
    m_previewBtn->setCheckable(true);
    m_previewBtn->setToolTip("Toggle Markdown preview (Ctrl+P)");
    m_previewBtn->setFixedWidth(80);
    m_lockIndicator = new QLabel("");
    m_lockIndicator->setStyleSheet("color: #e74c3c; font-size: 12px; font-weight: bold;");
    editorToolbar->addStretch();
    editorToolbar->addWidget(m_lockIndicator);
    editorToolbar->addWidget(m_previewBtn);
    editorLayout->addLayout(editorToolbar);

    m_editorStack = new QStackedWidget;

    m_editor = new QPlainTextEdit;
    m_editor->setPlaceholderText("Start writing your note here...\n\nMarkdown supported: **bold**, *italic*, # headings, - lists");
    QFont editorFont = m_editor->font();
    editorFont.setPointSize(12);
    m_editor->setFont(editorFont);
    m_editor->setTabStopDistance(40);
    m_editorStack->addWidget(m_editor);

    m_preview = new QTextBrowser;
    m_preview->setOpenExternalLinks(true);
    m_editorStack->addWidget(m_preview);

    m_editorStack->setCurrentIndex(0);
    editorLayout->addWidget(m_editorStack);

    m_splitter->addWidget(editorArea);
    m_splitter->setSizes({220, 680});

    QHBoxLayout *mainLayout = new QHBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_splitter);

    // --- Status bar ---
    m_statusCount = new QLabel("0 Notes");
    m_statusWords = new QLabel("");
    m_statusSaved = new QLabel("Saved \u2713");
    m_syncStatusLabel = new QLabel("");
    m_syncStatusLabel->setStyleSheet("color: #0078d4; font-size: 11px;");
    statusBar()->addPermanentWidget(m_syncStatusLabel);
    statusBar()->addPermanentWidget(m_statusWords);
    statusBar()->addPermanentWidget(m_statusCount);
    statusBar()->addPermanentWidget(m_statusSaved);

    // --- Connections ---
    connect(m_newButton, &QPushButton::clicked, this, &MainWindow::newNote);
    connect(m_noteList, &QListWidget::itemClicked, this, [this]() { openSelectedNote(); });
    connect(m_trashList, &QListWidget::itemDoubleClicked, this, &MainWindow::restoreFromTrash);
    connect(m_titleEdit, &QLineEdit::textChanged, this, &MainWindow::onContentChanged);
    connect(m_editor, &QPlainTextEdit::textChanged, this, &MainWindow::onContentChanged);
    connect(m_searchBox, &QLineEdit::textChanged, this, &MainWindow::onSearchChanged);
    connect(m_darkModeBtn, &QPushButton::clicked, this, &MainWindow::toggleDarkMode);
    connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onSortChanged);
    connect(m_noteList, &QListWidget::customContextMenuRequested, this, &MainWindow::showContextMenu);
    connect(m_folderTree, &QTreeWidget::customContextMenuRequested, this, &MainWindow::showFolderContextMenu);
    connect(m_folderTree, &QTreeWidget::itemClicked, this, &MainWindow::onFolderClicked);
    connect(trashBtn, &QPushButton::clicked, this, &MainWindow::toggleTrash);
    connect(trashBackBtn, &QPushButton::clicked, this, &MainWindow::toggleTrash);
    connect(emptyTrashBtn, &QPushButton::clicked, this, [this]() {
        auto reply = QMessageBox::question(this, "Empty Trash",
            "Permanently delete ALL notes in trash?", QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) { m_manager->emptyTrash(); loadNotes(); }
    });
    connect(m_previewBtn, &QPushButton::clicked, this, &MainWindow::togglePreview);

    // --- Stylesheet ---
    setStyleSheet(R"(
        #noteList { border: 1px solid #ccc; border-radius: 4px; padding: 4px; }
        #noteList::item { padding: 6px 4px; border-bottom: 1px solid #eee; }
        #noteList::item:selected { background: #0078d4; color: white; }
        QLineEdit, QPlainTextEdit { border: 1px solid #ccc; border-radius: 4px; padding: 4px 8px; }
        QLineEdit:focus, QPlainTextEdit:focus { border-color: #0078d4; }
        QPushButton { padding: 6px 12px; border-radius: 4px; background: #0078d4; color: white; border: none; }
        QPushButton:hover { background: #005fa3; }
        #trashBtn { background: #555; }
        QStatusBar { font-size: 11px; color: #666; }
    )");
}

void MainWindow::setupMenu() {
    QMenu *fileMenu = menuBar()->addMenu("File");
    fileMenu->addAction("New Note", QKeySequence("Ctrl+N"), this, &MainWindow::newNote);
    fileMenu->addSeparator();
    fileMenu->addAction("Export as TXT", this, &MainWindow::exportTxt);
    fileMenu->addAction("Export as Markdown", this, &MainWindow::exportMd);
    fileMenu->addAction("Export as PDF", this, &MainWindow::exportPdf);
    fileMenu->addSeparator();
    fileMenu->addAction("Backup", QKeySequence("Ctrl+Shift+B"), this, &MainWindow::backupNotes);
    fileMenu->addAction("Restore", this, &MainWindow::restoreNotes);
    fileMenu->addSeparator();
    fileMenu->addAction("Quit", QKeySequence("Ctrl+Q"), qApp, &QApplication::quit);

    QMenu *editMenu = menuBar()->addMenu("Edit");
    editMenu->addAction("Save", QKeySequence("Ctrl+S"), this, &MainWindow::saveNote);
    editMenu->addAction("Save As", QKeySequence("Ctrl+Shift+S"), this, &MainWindow::saveNoteAs);
    editMenu->addSeparator();
    editMenu->addAction("Pin/Unpin", this, &MainWindow::togglePin);
    editMenu->addAction("Change Color", this, &MainWindow::changeColor);
    editMenu->addAction("Move to Folder", this, &MainWindow::moveToFolder);
    editMenu->addSeparator();
    editMenu->addAction("Set Reminder", this, &MainWindow::setReminder);
    editMenu->addAction("Clear Reminder", this, &MainWindow::clearReminder);
    editMenu->addSeparator();
    editMenu->addAction("Lock/Unlock Note", this, &MainWindow::toggleNoteLock);
    editMenu->addSeparator();
    editMenu->addAction("Delete", QKeySequence("Delete"), this, &MainWindow::deleteNote);

    QMenu *viewMenu = menuBar()->addMenu("View");
    viewMenu->addAction("Toggle Dark Mode", QKeySequence("Ctrl+D"), this, &MainWindow::toggleDarkMode);
    viewMenu->addAction("Toggle Preview", QKeySequence("Ctrl+P"), this, &MainWindow::togglePreview);
    viewMenu->addAction("New Folder", this, &MainWindow::createFolder);

    QMenu *syncMenu = menuBar()->addMenu("Sync");
    syncMenu->addAction("Sign in to Google", this, &MainWindow::signInToGoogle);
    syncMenu->addAction("Sign out", this, &MainWindow::signOutFromGoogle);
    syncMenu->addSeparator();
    syncMenu->addAction("Sync Now", QKeySequence("Ctrl+Shift+G"), this, &MainWindow::triggerSync);

    QMenu *settingsMenu = menuBar()->addMenu("Settings");
    settingsMenu->addAction("Set Master Password", this, &MainWindow::setupMasterPassword);
    settingsMenu->addAction("Lock App", this, &MainWindow::lockApp);
    settingsMenu->addSeparator();
    settingsMenu->addAction("Enable Autostart", this, &MainWindow::enableAutostart);
    settingsMenu->addAction("Disable Autostart", this, &MainWindow::disableAutostart);
}

void MainWindow::setupShortcuts() {
    QShortcut *searchShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(searchShortcut, &QShortcut::activated, this, [this]() { m_searchBox->setFocus(); });
    QShortcut *previewShortcut = new QShortcut(QKeySequence("Ctrl+P"), this);
    connect(previewShortcut, &QShortcut::activated, this, &MainWindow::togglePreview);
    QShortcut *nextMatch = new QShortcut(QKeySequence("F3"), this);
    connect(nextMatch, &QShortcut::activated, this, &MainWindow::nextSearchMatch);
}

// --- Icon + Tray ---

void MainWindow::setupTray() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) return;

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(":/allex-notes-128.png"));
    m_trayIcon->setToolTip("Allex Notes");

    QMenu *trayMenu = new QMenu;
    trayMenu->addAction("Show", this, [this]() { show(); raise(); activateWindow(); });
    trayMenu->addAction("New Note", this, &MainWindow::newNote);
    trayMenu->addAction("Lock", this, &MainWindow::lockApp);
    trayMenu->addSeparator();
    trayMenu->addAction("Quit", qApp, &QApplication::quit);
    m_trayIcon->setContextMenu(trayMenu);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayActivated);
    m_trayIcon->show();
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger) {
        if (isVisible()) hide();
        else { show(); raise(); activateWindow(); }
    }
}

// --- Note list helpers ---

static QString displayTitle(const Note &n) {
    QString t = n.title().trimmed();
    return t.isEmpty() ? "(Untitled)" : t;
}

void MainWindow::populateList(const QList<Note> &notes) {
    m_noteList->clear();
    QList<Note> sorted = notes;
    int sortMode = m_sortCombo->currentIndex();

    std::stable_sort(sorted.begin(), sorted.end(), [sortMode](const Note &a, const Note &b) {
        if (a.isPinned() != b.isPinned()) return a.isPinned();
        switch (sortMode) {
        case 0: return a.modifiedAt() > b.modifiedAt();
        case 1: return a.createdAt() > b.createdAt();
        case 2: return a.title().toLower() < b.title().toLower();
        default: return a.modifiedAt() > b.modifiedAt();
        }
    });

    for (const Note &n : sorted) {
        QListWidgetItem *item = new QListWidgetItem;
        QString prefix;
        if (n.isPinned()) prefix += "\u2691 ";
        if (n.hasReminder()) prefix += "\u23F0 ";
        if (n.isLocked()) prefix += "\U0001F512 ";
        if (!n.color().isEmpty()) item->setForeground(QColor(n.color()));
        item->setText(prefix + displayTitle(n));
        item->setData(Qt::UserRole, n.id());
        m_noteList->addItem(item);
    }
}

QString MainWindow::currentFolder() const {
    QTreeWidgetItem *item = m_folderTree->currentItem();
    if (!item) return {};
    return item->data(0, Qt::UserRole).toString();
}

void MainWindow::populateTree() {
    m_folderTree->clear();
    QList<Note> all = m_manager->allNotes();

    QTreeWidgetItem *allItem = new QTreeWidgetItem(m_folderTree);
    allItem->setText(0, QString("All Notes (%1)").arg(all.size()));
    allItem->setData(0, Qt::UserRole, "");
    allItem->setSelected(true);

    QStringList folders = m_manager->folders();
    for (const QString &f : folders) {
        int count = 0;
        for (const Note &n : all) if (n.folder() == f) count++;
        QTreeWidgetItem *item = new QTreeWidgetItem(m_folderTree);
        item->setText(0, QString("%1 (%2)").arg(f).arg(count));
        item->setData(0, Qt::UserRole, f);
    }
}

void MainWindow::loadNotes() {
    populateTree();
    QString folder = currentFolder();
    populateList(m_manager->allNotes(folder));
    m_trashList->clear();
    for (const Note &n : m_manager->trashedNotes()) {
        QListWidgetItem *item = new QListWidgetItem(displayTitle(n));
        item->setData(Qt::UserRole, n.id());
        m_trashList->addItem(item);
    }
    updateStatus();
}

void MainWindow::onSearchChanged(const QString &text) {
    QString folder = currentFolder();
    populateList(m_manager->search(text, folder));
    updateSearchHighlights();
    updateStatus();
}

void MainWindow::onSortChanged(int) {
    QString query = m_searchBox->text();
    if (query.isEmpty()) loadNotes();
    else onSearchChanged(query);
}

void MainWindow::updateStatus() {
    QListWidget *activeList = m_showingTrash ? m_trashList : m_noteList;
    int count = activeList->count();
    if (m_showingTrash) m_statusCount->setText(QString("Trash: %1 Note%2").arg(count).arg(count == 1 ? "" : "s"));
    else m_statusCount->setText(QString("%1 Note%2").arg(count).arg(count == 1 ? "" : "s"));
    m_statusSaved->setText(m_isDirty ? "Unsaved" : "Saved \u2713");
}

// --- Word count ---

void MainWindow::updateWordCount() {
    QString text = m_editor->toPlainText();
    int chars = text.length();
    int words = text.isEmpty() ? 0 : text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).count();
    m_statusWords->setText(QString("%1 words, %2 chars").arg(words).arg(chars));
}

// --- Reminder ---

void MainWindow::updateReminderLabel() {
    if (m_currentNoteId.isNull()) { m_reminderLabel->clear(); return; }
    Note note = m_manager->loadNote(m_currentNoteId);
    if (note.hasReminder()) {
        m_reminderLabel->setText("\u23F0 " + note.reminder().toString("MMM d, h:mm AP"));
    } else {
        m_reminderLabel->clear();
    }
}

void MainWindow::setReminder() {
    if (m_currentNoteId.isNull()) return;
    Note note = m_manager->loadNote(m_currentNoteId);

    QDateTimeEdit *dateEdit = new QDateTimeEdit(
        note.hasReminder() ? note.reminder() : QDateTime::currentDateTime().addSecs(3600));
    dateEdit->setCalendarPopup(true);
    dateEdit->setMinimumDateTime(QDateTime::currentDateTime());

    QDialog dialog(this);
    dialog.setWindowTitle("Set Reminder");
    dialog.setMinimumWidth(280);
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel("Remind me at:"));
    layout->addWidget(dateEdit);
    QHBoxLayout *btnLayout = new QHBoxLayout;
    QPushButton *okBtn = new QPushButton("OK");
    QPushButton *cancelBtn = new QPushButton("Cancel");
    btnLayout->addStretch();
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);
    connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) return;
    note.setReminder(dateEdit->dateTime());
    m_manager->saveNote(note);
    updateReminderLabel();
    loadNotes();
    delete dateEdit;
}

void MainWindow::clearReminder() {
    if (m_currentNoteId.isNull()) return;
    Note note = m_manager->loadNote(m_currentNoteId);
    note.clearReminder();
    m_manager->saveNote(note);
    updateReminderLabel();
    loadNotes();
}

void MainWindow::checkReminders() {
    QDateTime now = QDateTime::currentDateTime();
    for (const Note &note : m_manager->allNotes()) {
        if (!note.hasReminder()) continue;
        if (note.reminder() <= now && !m_firedReminders.contains(note.id())) {
            m_firedReminders.insert(note.id());
            QProcess::startDetached("notify-send", {
                "-a", "Allex Notes", "-i", "text-editor",
                "Reminder: " + note.title(), note.content().left(100)
            });
            m_trayIcon->showMessage("Reminder: " + note.title(),
                note.content().left(200), QSystemTrayIcon::Information, 5000);
            if (!isVisible()) { show(); raise(); activateWindow(); }
        }
    }
}

// --- Folder management ---

void MainWindow::onFolderClicked(QTreeWidgetItem *item, int) {
    if (m_isDirty) {
        auto reply = QMessageBox::question(this, "Unsaved Changes",
            "You have unsaved changes. Save before switching?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (reply == QMessageBox::Save) saveNote();
        else if (reply == QMessageBox::Cancel) return;
    }
    clearEditor();
    QString folder = item->data(0, Qt::UserRole).toString();
    populateList(m_manager->allNotes(folder));
    updateStatus();
}

void MainWindow::createFolder() {
    bool ok;
    QString name = QInputDialog::getText(this, "New Folder", "Folder name:", QLineEdit::Normal, {}, &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    Note n = m_manager->createNote("New Note", {}, name.trimmed());
    loadNotes();
    selectNote(n.id());
}

void MainWindow::renameFolder() {
    QTreeWidgetItem *item = m_folderTree->currentItem();
    if (!item) return;
    QString oldName = item->data(0, Qt::UserRole).toString();
    if (oldName.isEmpty()) return;

    bool ok;
    QString newName = QInputDialog::getText(this, "Rename Folder", "New name:", QLineEdit::Normal, oldName, &ok);
    if (!ok || newName.trimmed().isEmpty() || newName == oldName) return;

    for (Note &n : m_manager->allNotes()) {
        if (n.folder() == oldName) { n.setFolder(newName.trimmed()); m_manager->saveNote(n); }
    }
    loadNotes();
}

void MainWindow::deleteFolder() {
    QTreeWidgetItem *item = m_folderTree->currentItem();
    if (!item) return;
    QString folder = item->data(0, Qt::UserRole).toString();
    if (folder.isEmpty()) return;

    auto reply = QMessageBox::question(this, "Delete Folder",
        QString("Delete folder \"%1\" and all its notes?").arg(folder),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    for (Note &n : m_manager->allNotes()) {
        if (n.folder() == folder) m_manager->deleteNote(n.id());
    }
    loadNotes();
}

void MainWindow::showFolderContextMenu(const QPoint &pos) {
    QTreeWidgetItem *item = m_folderTree->itemAt(pos);
    if (!item) return;
    QString folder = item->data(0, Qt::UserRole).toString();

    QMenu menu;
    if (!folder.isEmpty()) {
        menu.addAction("Rename Folder", this, &MainWindow::renameFolder);
        menu.addAction("Delete Folder", this, &MainWindow::deleteFolder);
        menu.addSeparator();
    }
    menu.addAction("New Folder", this, &MainWindow::createFolder);
    menu.exec(m_folderTree->mapToGlobal(pos));
}

// --- Move to folder ---

void MainWindow::moveToFolder() {
    if (m_currentNoteId.isNull()) return;
    QStringList folders = m_manager->folders();
    folders.removeAll({});

    bool ok;
    QString folder = QInputDialog::getItem(this, "Move to Folder", "Select folder:", folders, 0, true, &ok);
    if (!ok) return;

    Note n = m_manager->loadNote(m_currentNoteId);
    n.setFolder(folder);
    m_manager->saveNote(n);
    loadNotes();
}

// --- Context menu ---

void MainWindow::showContextMenu(const QPoint &pos) {
    QListWidgetItem *item = m_noteList->itemAt(pos);
    if (!item) return;

    QUuid id = item->data(Qt::UserRole).toUuid();
    Note note = m_manager->loadNote(id);

    QMenu menu;
    menu.addAction(note.isPinned() ? "Unpin" : "Pin", this, &MainWindow::togglePin);

    QMenu *colorMenu = menu.addMenu("Color");
    colorMenu->addAction("No color", this, [this]() {
        QUuid id = m_noteList->currentItem()->data(Qt::UserRole).toUuid();
        Note n = m_manager->loadNote(id);
        n.setColor("");
        m_manager->saveNote(n);
        loadNotes();
    });
    QStringList colors = {"#e74c3c", "#e67e22", "#f1c40f", "#2ecc71", "#3498db", "#9b59b6"};
    for (const QString &c : colors) {
        colorMenu->addAction("", this, [this, c]() {
            QUuid id = m_noteList->currentItem()->data(Qt::UserRole).toUuid();
            Note n = m_manager->loadNote(id);
            n.setColor(c);
            m_manager->saveNote(n);
            loadNotes();
        });
    }

    QMenu *folderMenu = menu.addMenu("Move to Folder");
    QStringList folders = m_manager->folders();
    folders.removeAll({});
    folders.prepend("No Folder");
    for (const QString &f : folders) {
        folderMenu->addAction(f, this, [this, f]() {
            QUuid id = m_noteList->currentItem()->data(Qt::UserRole).toUuid();
            Note n = m_manager->loadNote(id);
            n.setFolder(f == "No Folder" ? "" : f);
            m_manager->saveNote(n);
            loadNotes();
        });
    }

    menu.addSeparator();
    menu.addAction(note.hasReminder() ? "Edit Reminder" : "Set Reminder", this, &MainWindow::setReminder);
    if (note.hasReminder()) menu.addAction("Clear Reminder", this, &MainWindow::clearReminder);

    menu.addSeparator();
    menu.addAction(note.isLocked() ? "Unlock Note" : "Lock Note", this, &MainWindow::toggleNoteLock);

    menu.addSeparator();
    menu.addAction("Delete", this, &MainWindow::deleteNote);
    menu.exec(m_noteList->mapToGlobal(pos));
}

void MainWindow::togglePin() {
    if (!m_noteList->currentItem()) return;
    QUuid id = m_noteList->currentItem()->data(Qt::UserRole).toUuid();
    Note n = m_manager->loadNote(id);
    n.setPinned(!n.isPinned());
    m_manager->saveNote(n);
    loadNotes();
}

void MainWindow::changeColor() {
    if (!m_noteList->currentItem()) return;
    QColor c = QColorDialog::getColor(Qt::white, this, "Pick note color");
    if (!c.isValid()) return;
    QUuid id = m_noteList->currentItem()->data(Qt::UserRole).toUuid();
    Note n = m_manager->loadNote(id);
    n.setColor(c.name());
    m_manager->saveNote(n);
    loadNotes();
}

void MainWindow::togglePreview() {
    bool showing = m_editorStack->currentIndex() == 1;
    if (showing) {
        m_editorStack->setCurrentIndex(0);
        m_previewBtn->setText("Preview");
        m_previewBtn->setChecked(false);
    } else {
        m_preview->document()->setMarkdown(m_editor->toPlainText());
        m_editorStack->setCurrentIndex(1);
        m_previewBtn->setText("Edit");
        m_previewBtn->setChecked(true);
    }
}

// --- CRUD ---

void MainWindow::newNote() {
    if (m_showingTrash) toggleTrash();
    QString folder = currentFolder();
    Note n = m_manager->createNote("New Note", {}, folder);
    loadNotes();
    selectNote(n.id());
    m_titleEdit->selectAll();
    m_titleEdit->setFocus();
}

void MainWindow::openSelectedNote() {
    if (!m_noteList->currentItem()) return;

    if (m_isDirty) {
        auto reply = QMessageBox::question(this, "Unsaved Changes",
            "You have unsaved changes. Save before switching?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (reply == QMessageBox::Save) saveNote();
        else if (reply == QMessageBox::Cancel) {
            for (int i = 0; i < m_noteList->count(); ++i) {
                if (m_noteList->item(i)->data(Qt::UserRole).toUuid() == m_currentNoteId) {
                    m_noteList->setCurrentRow(i);
                    return;
                }
            }
            return;
        }
    }

    QUuid id = m_noteList->currentItem()->data(Qt::UserRole).toUuid();
    selectNote(id);
}

void MainWindow::selectNote(const QUuid &id) {
    Note note = m_manager->loadNote(id);
    if (note.isNull()) return;
    m_currentNoteId = id;
    m_titleEdit->setText(note.title());
    m_editorStack->setCurrentIndex(0);
    m_previewBtn->setText("Preview");
    m_previewBtn->setChecked(false);
    m_isDirty = false;

    if (note.isLocked()) {
        if (showLockDialog()) {
            QByteArray decrypted = Crypto::decrypt(
                QByteArray::fromBase64(note.content().toUtf8()), m_masterPasswordHash);
            if (!decrypted.isEmpty()) {
                m_editor->setPlainText(QString::fromUtf8(decrypted));
            } else {
                m_editor->setPlainText("[Decryption failed - wrong password]");
            }
            m_lockIndicator->setText("\U0001F512 Locked");
        } else {
            m_editor->setPlainText("[Locked - enter password to view]");
            m_titleEdit->setEnabled(false);
            m_editor->setEnabled(false);
        }
    } else {
        m_editor->setPlainText(note.content());
        m_titleEdit->setEnabled(true);
        m_editor->setEnabled(true);
        m_lockIndicator->clear();
    }

    updateWordCount();
    updateReminderLabel();
    updateStatus();
}

void MainWindow::clearEditor() {
    m_currentNoteId = QUuid();
    m_titleEdit->clear();
    m_editor->clear();
    m_editorStack->setCurrentIndex(0);
    m_previewBtn->setText("Preview");
    m_previewBtn->setChecked(false);
    m_lockIndicator->clear();
    m_titleEdit->setEnabled(true);
    m_editor->setEnabled(true);
    m_isDirty = false;
    m_searchHighlights.clear();
    m_editor->setExtraSelections({});
    updateWordCount();
    updateStatus();
}

void MainWindow::saveNote() {
    if (m_currentNoteId.isNull()) return;
    Note note = m_manager->loadNote(m_currentNoteId);
    if (note.isNull()) return;

    note.setTitle(m_titleEdit->text());

    if (note.isLocked()) {
        QByteArray plaintext = m_editor->toPlainText().toUtf8();
        QByteArray ciphertext = Crypto::encrypt(plaintext, m_masterPasswordHash);
        note.setContent(QString(ciphertext.toBase64()));
    } else {
        note.setContent(m_editor->toPlainText());
    }

    m_manager->saveNote(note);
    m_isDirty = false;

    for (int i = 0; i < m_noteList->count(); ++i) {
        QListWidgetItem *item = m_noteList->item(i);
        if (item->data(Qt::UserRole).toUuid() == m_currentNoteId) {
            QString prefix;
            if (note.isPinned()) prefix += "\u2691 ";
            if (note.hasReminder()) prefix += "\u23F0 ";
            if (note.isLocked()) prefix += "\U0001F512 ";
            item->setText(prefix + displayTitle(note));
            if (!note.color().isEmpty()) item->setForeground(QColor(note.color()));
            break;
        }
    }
    updateStatus();
}

void MainWindow::saveNoteAs() {
    if (m_currentNoteId.isNull()) return;
    Note note = m_manager->loadNote(m_currentNoteId);
    if (note.isNull()) return;
    Note newNote(m_titleEdit->text(), m_editor->toPlainText());
    newNote.setFolder(note.folder());
    m_manager->saveNote(newNote);
    loadNotes();
    selectNote(newNote.id());
}

void MainWindow::deleteNote() {
    if (!m_noteList->currentItem()) return;
    auto reply = QMessageBox::question(this, "Delete Note", "Move this note to trash?",
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;
    QUuid id = m_noteList->currentItem()->data(Qt::UserRole).toUuid();
    m_manager->trashNote(id);
    clearEditor();
    loadNotes();
}

// --- Trash ---

void MainWindow::toggleTrash() {
    m_showingTrash = !m_showingTrash;
    if (m_showingTrash) { m_leftStack->setCurrentIndex(1); m_newButton->setEnabled(false); }
    else { m_leftStack->setCurrentIndex(0); m_newButton->setEnabled(true); loadNotes(); }
    updateStatus();
}

void MainWindow::restoreFromTrash() {
    if (!m_trashList->currentItem()) return;
    QUuid id = m_trashList->currentItem()->data(Qt::UserRole).toUuid();
    m_manager->restoreNote(id);
    loadNotes();
}

void MainWindow::permanentlyDeleteFromTrash() {
    if (!m_trashList->currentItem()) return;
    auto reply = QMessageBox::question(this, "Permanently Delete",
        "This cannot be undone. Delete permanently?", QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;
    QUuid id = m_trashList->currentItem()->data(Qt::UserRole).toUuid();
    m_manager->permanentlyDeleteNote(id);
    loadNotes();
}

// --- Export ---

void MainWindow::exportTxt() {
    if (m_currentNoteId.isNull()) return;
    QString path = QFileDialog::getSaveFileName(this, "Export as TXT", {}, "Text (*.txt)");
    if (path.isEmpty()) return;
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(m_editor->toPlainText().toUtf8());
}

void MainWindow::exportMd() {
    if (m_currentNoteId.isNull()) return;
    QString path = QFileDialog::getSaveFileName(this, "Export as Markdown", {}, "Markdown (*.md)");
    if (path.isEmpty()) return;
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(m_editor->toPlainText().toUtf8());
}

void MainWindow::exportPdf() {
    if (m_currentNoteId.isNull()) return;
    QString path = QFileDialog::getSaveFileName(this, "Export as PDF", {}, "PDF (*.pdf)");
    if (path.isEmpty()) return;
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    QTextDocument doc;
    doc.setPlainText(m_editor->toPlainText());
    doc.print(&printer);
}

// --- Content tracking ---

void MainWindow::onContentChanged() {
    if (!m_isDirty) { m_isDirty = true; updateStatus(); }
    updateWordCount();
    m_autoSaveTimer->start(2000);
}

void MainWindow::autoSave() {
    saveNote();
    if (m_auth->isSignedIn()) triggerSync();
}

// --- Google Sync ---

void MainWindow::setupSync() {
    connect(m_auth, &AuthManager::signedIn, this, [this]() { updateSyncStatus(); triggerSync(); });
    connect(m_auth, &AuthManager::signedOut, this, [this]() { updateSyncStatus(); });
    connect(m_auth, &AuthManager::authError, this, [this](const QString &err) {
        m_syncStatusLabel->setText("Auth error");
        QMessageBox::warning(this, "Sign-in Error", err);
    });
    connect(m_sync, &SyncManager::syncComplete, this, &MainWindow::onSyncComplete);
    connect(m_sync, &SyncManager::syncError, this, &MainWindow::onSyncError);
    connect(m_sync, &SyncManager::syncProgress, this, [this](const QString &s) {
        m_syncStatusLabel->setText("\u2601 " + s);
    });
    updateSyncStatus();
}

void MainWindow::signInToGoogle() {
    QDialog dialog(this);
    dialog.setWindowTitle("Sign in to Google");
    dialog.setMinimumWidth(400);
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(
        "Steps:\n1. Go to console.cloud.google.com\n"
        "2. Enable Google Drive API\n"
        "3. OAuth consent screen → add your Gmail\n"
        "4. Credentials → OAuth Client ID → Desktop app\n"
        "5. Copy Client ID and Secret below"));

    layout->addWidget(new QLabel("Client ID:"));
    QLineEdit *idEdit = new QLineEdit;
    idEdit->setPlaceholderText("xxxx.apps.googleusercontent.com");
    layout->addWidget(idEdit);
    layout->addWidget(new QLabel("Client Secret:"));
    QLineEdit *secretEdit = new QLineEdit;
    secretEdit->setEchoMode(QLineEdit::Password);
    layout->addWidget(secretEdit);

    QHBoxLayout *btnLayout = new QHBoxLayout;
    QPushButton *okBtn = new QPushButton("Sign in");
    QPushButton *cancelBtn = new QPushButton("Cancel");
    btnLayout->addStretch();
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);
    connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) return;
    QString clientId = idEdit->text().trimmed();
    QString secret = secretEdit->text().trimmed();
    if (clientId.isEmpty() || secret.isEmpty()) {
        QMessageBox::warning(this, "Error", "Client ID and Secret are required.");
        return;
    }
    m_auth->signIn(clientId, secret);
}

void MainWindow::signOutFromGoogle() {
    auto reply = QMessageBox::question(this, "Sign Out",
        "Sign out of Google? Sync will stop.", QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;
    m_auth->signOut();
}

void MainWindow::triggerSync() {
    if (!m_auth->isSignedIn()) { m_syncStatusLabel->setText("\u2601 Not signed in"); return; }
    m_sync->syncNow();
}

void MainWindow::onSyncComplete() { updateSyncStatus(); loadNotes(); }
void MainWindow::onSyncError(const QString &error) {
    m_syncStatusLabel->setText("Sync error");
    QMessageBox::warning(this, "Sync Error", error);
}

void MainWindow::updateSyncStatus() {
    if (!m_auth->isSignedIn()) { m_syncStatusLabel->setText("\u2601 Not signed in"); return; }
    if (m_sync->isSyncing()) { m_syncStatusLabel->setText("\u2601 Syncing..."); return; }
    QDateTime last = m_sync->lastSynced();
    if (last.isValid()) {
        qint64 secs = last.secsTo(QDateTime::currentDateTime());
        if (secs < 60) m_syncStatusLabel->setText("\u2601 Synced just now");
        else if (secs < 3600) m_syncStatusLabel->setText(QString("\u2601 Synced %1m ago").arg(secs / 60));
        else m_syncStatusLabel->setText(QString("\u2601 Synced %1h ago").arg(secs / 3600));
    } else {
        m_syncStatusLabel->setText("\u2601 Signed in");
    }
}

// --- Password ---

void MainWindow::setupMasterPassword() {
    QDialog dialog(this);
    dialog.setWindowTitle("Set Master Password");
    dialog.setMinimumWidth(350);
    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    if (!m_masterPasswordHash.isEmpty()) {
        layout->addWidget(new QLabel("Enter current password:"));
        QLineEdit *oldEdit = new QLineEdit;
        oldEdit->setEchoMode(QLineEdit::Password);
        layout->addWidget(oldEdit);

        layout->addWidget(new QLabel("New password (blank = remove):"));
        QLineEdit *newEdit = new QLineEdit;
        newEdit->setEchoMode(QLineEdit::Password);
        layout->addWidget(newEdit);
        QLineEdit *confirmEdit = new QLineEdit;
        confirmEdit->setEchoMode(QLineEdit::Password);
        confirmEdit->setPlaceholderText("Confirm new password");
        layout->addWidget(confirmEdit);

        QHBoxLayout *btnLayout = new QHBoxLayout;
        QPushButton *okBtn = new QPushButton("Set");
        QPushButton *cancelBtn = new QPushButton("Cancel");
        btnLayout->addStretch(); btnLayout->addWidget(okBtn); btnLayout->addWidget(cancelBtn);
        layout->addLayout(btnLayout);
        connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
        connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

        if (dialog.exec() != QDialog::Accepted) return;

        if (!Crypto::verifyPassword(oldEdit->text(), m_masterPasswordHash, m_masterSalt)) {
            QMessageBox::warning(this, "Error", "Current password is incorrect.");
            return;
        }

        if (newEdit->text().isEmpty()) {
            m_masterPasswordHash.clear();
            m_masterSalt.clear();
            m_settings.remove("master/passwordHash");
            m_settings.remove("master/salt");
            QMessageBox::information(this, "Done", "Master password removed.");
            return;
        }

        if (newEdit->text() != confirmEdit->text()) {
            QMessageBox::warning(this, "Error", "Passwords do not match.");
            return;
        }

        m_masterSalt = Crypto::generateSalt();
        m_masterPasswordHash = Crypto::hashPassword(newEdit->text(), m_masterSalt);
    } else {
        layout->addWidget(new QLabel("New password:"));
        QLineEdit *passEdit = new QLineEdit;
        passEdit->setEchoMode(QLineEdit::Password);
        layout->addWidget(passEdit);
        QLineEdit *confirmEdit = new QLineEdit;
        confirmEdit->setEchoMode(QLineEdit::Password);
        confirmEdit->setPlaceholderText("Confirm password");
        layout->addWidget(confirmEdit);

        QHBoxLayout *btnLayout = new QHBoxLayout;
        QPushButton *okBtn = new QPushButton("Set");
        QPushButton *cancelBtn = new QPushButton("Cancel");
        btnLayout->addStretch(); btnLayout->addWidget(okBtn); btnLayout->addWidget(cancelBtn);
        layout->addLayout(btnLayout);
        connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
        connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

        if (dialog.exec() != QDialog::Accepted) return;

        if (passEdit->text() != confirmEdit->text()) {
            QMessageBox::warning(this, "Error", "Passwords do not match.");
            return;
        }

        m_masterSalt = Crypto::generateSalt();
        m_masterPasswordHash = Crypto::hashPassword(passEdit->text(), m_masterSalt);
    }

    m_settings.setValue("master/passwordHash", m_masterPasswordHash);
    m_settings.setValue("master/salt", m_masterSalt);
    QMessageBox::information(this, "Done", "Master password saved.");
}

void MainWindow::lockApp() {
    if (m_masterPasswordHash.isEmpty()) {
        QMessageBox::information(this, "No Password", "Set a master password first (Settings → Set Master Password).");
        return;
    }
    m_appLocked = true;
    clearEditor();
    hide();
    showLockDialog();
}

bool MainWindow::showLockDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle("Allex Notes - Locked");
    dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dialog.setMinimumWidth(300);
    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLabel *icon = new QLabel("\U0001F512");
    icon->setStyleSheet("font-size: 48px; text-align: center;");
    layout->addWidget(icon);

    QLabel *prompt = new QLabel("Enter master password:");
    layout->addWidget(prompt);

    QLineEdit *passEdit = new QLineEdit;
    passEdit->setEchoMode(QLineEdit::Password);
    layout->addWidget(passEdit);

    QHBoxLayout *btnLayout = new QHBoxLayout;
    QPushButton *unlockBtn = new QPushButton("Unlock");
    QPushButton *quitBtn = new QPushButton("Quit");
    btnLayout->addStretch(); btnLayout->addWidget(unlockBtn); btnLayout->addWidget(quitBtn);
    layout->addLayout(btnLayout);

    connect(unlockBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(quitBtn, &QPushButton::clicked, qApp, &QApplication::quit);
    connect(passEdit, &QLineEdit::returnPressed, &dialog, &QDialog::accept);

    bool unlocked = false;
    connect(&dialog, &QDialog::accepted, this, [&]() {
        if (Crypto::verifyPassword(passEdit->text(), m_masterPasswordHash, m_masterSalt)) {
            m_appLocked = false;
            show();
            raise();
            activateWindow();
            unlocked = true;
        } else {
            QMessageBox::warning(this, "Wrong Password", "The password is incorrect.");
        }
    });

    if (!m_appLocked) return true;

    dialog.exec();
    return unlocked;
}

void MainWindow::toggleNoteLock() {
    if (m_currentNoteId.isNull()) return;
    Note note = m_manager->loadNote(m_currentNoteId);

    if (note.isLocked()) {
        auto reply = QMessageBox::question(this, "Unlock Note",
            "Remove lock from this note?", QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
        note.setLocked(false);
        note.setContent(m_editor->toPlainText());
        m_lockIndicator->clear();
    } else {
        if (m_masterPasswordHash.isEmpty()) {
            QMessageBox::warning(this, "No Password",
                "Set a master password first (Settings → Set Master Password).");
            return;
        }
        note.setLocked(true);
        QByteArray plaintext = m_editor->toPlainText().toUtf8();
        QByteArray ciphertext = Crypto::encrypt(plaintext, m_masterPasswordHash);
        note.setContent(QString(ciphertext.toBase64()));
        note.setLockedSalt(m_masterSalt);
        m_lockIndicator->setText("\U0001F512 Locked");
    }

    m_manager->saveNote(note);
    loadNotes();
}

void MainWindow::unlockNote() {
    if (m_currentNoteId.isNull()) return;
    Note note = m_manager->loadNote(m_currentNoteId);
    if (!note.isLocked()) return;

    if (showLockDialog()) {
        QByteArray decrypted = Crypto::decrypt(
            QByteArray::fromBase64(note.content().toUtf8()), m_masterPasswordHash);
        if (!decrypted.isEmpty()) {
            m_editor->setPlainText(QString::fromUtf8(decrypted));
            m_titleEdit->setEnabled(true);
            m_editor->setEnabled(true);
            m_lockIndicator->clear();
        }
    }
}

// --- Backup ---

void MainWindow::backupNotes() {
    QString path = QFileDialog::getSaveFileName(this, "Backup Notes",
        QDir::homePath() + "/allex-notes-backup.allex", "Allex Backup (*.allex)");
    if (path.isEmpty()) return;

    QJsonArray notesArray;
    for (const Note &n : m_manager->allNotes()) {
        notesArray.append(n.toJson());
    }

    // Also include trashed notes
    for (const Note &n : m_manager->trashedNotes()) {
        notesArray.append(n.toJson());
    }

    QJsonObject bundle;
    bundle["version"] = 1;
    bundle["exported"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    bundle["notes"] = notesArray;

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(bundle).toJson());
        QMessageBox::information(this, "Backup Complete",
            QString("Backed up %1 notes to:\n%2").arg(notesArray.size()).arg(path));
    } else {
        QMessageBox::warning(this, "Error", "Could not write backup file.");
    }
}

void MainWindow::restoreNotes() {
    QString path = QFileDialog::getOpenFileName(this, "Restore Notes", {},
        "Allex Backup (*.allex);;All Files (*)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Error", "Could not read backup file.");
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject bundle = doc.object();
    QJsonArray notesArray = bundle["notes"].toArray();

    int imported = 0;
    for (const QJsonValue &v : notesArray) {
        Note n = Note::fromJson(v.toObject());
        Note existing = m_manager->loadNote(n.id());
        if (existing.isNull()) {
            m_manager->saveNote(n);
            imported++;
        } else if (n.modifiedAt() > existing.modifiedAt()) {
            m_manager->saveNote(n);
            imported++;
        }
    }

    loadNotes();
    QMessageBox::information(this, "Restore Complete",
        QString("Imported %1 notes from backup.").arg(imported));
}

// --- Search highlight ---

void MainWindow::updateSearchHighlights() {
    m_searchHighlights.clear();
    m_currentSearchIndex = -1;

    QString query = m_searchBox->text();
    if (query.isEmpty() || m_editor->toPlainText().isEmpty()) {
        m_editor->setExtraSelections({});
        return;
    }

    QTextCursor cursor(m_editor->document());
    QTextCharFormat fmt;
    fmt.setBackground(QColor("#ffff00"));

    QTextCursor highlightCursor(m_editor->document());
    while (!highlightCursor.isNull() && !highlightCursor.atEnd()) {
        highlightCursor = m_editor->document()->find(query, highlightCursor,
            QTextDocument::FindCaseSensitively);
        if (!highlightCursor.isNull()) {
            QTextEdit::ExtraSelection sel;
            sel.format = fmt;
            sel.cursor = highlightCursor;
            m_searchHighlights.append(sel);
        }
    }

    m_editor->setExtraSelections(m_searchHighlights);

    if (!m_searchHighlights.isEmpty()) {
        m_currentSearchIndex = 0;
        QTextCursor first = m_searchHighlights[0].cursor;
        m_editor->setTextCursor(first);
    }
}

void MainWindow::nextSearchMatch() {
    if (m_searchHighlights.isEmpty()) return;
    m_currentSearchIndex = (m_currentSearchIndex + 1) % m_searchHighlights.size();
    m_editor->setTextCursor(m_searchHighlights[m_currentSearchIndex].cursor);
    m_editor->centerCursor();
}

// --- Autostart ---

static QString autostartPath() {
    return QDir::homePath() + "/.config/autostart/allex-notes.desktop";
}

void MainWindow::enableAutostart() {
    QString desktopPath = QCoreApplication::applicationDirPath() + "/../share/applications/allex-notes.desktop";
    if (!QFile::exists(desktopPath)) {
        desktopPath = "/usr/share/applications/allex-notes.desktop";
    }
    if (!QFile::exists(desktopPath)) {
        // Copy from source
        QString srcPath = QCoreApplication::applicationDirPath() + "/../../allex-notes.desktop";
        if (QFile::exists(srcPath)) {
            QFile::copy(srcPath, autostartPath());
        } else {
            // Generate minimal desktop entry
            QFile file(autostartPath());
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                file.write("[Desktop Entry]\nType=Application\nName=Allex Notes\n"
                           "Exec=" + QCoreApplication::applicationFilePath().toUtf8() + "\n"
                           "Icon=allex-notes\nTerminal=false\n");
            }
        }
    } else {
        QFile::copy(desktopPath, autostartPath());
    }

    QMessageBox::information(this, "Autostart Enabled",
        "Allex Notes will start automatically on login.");
}

void MainWindow::disableAutostart() {
    QFile::remove(autostartPath());
    QMessageBox::information(this, "Autostart Disabled",
        "Allex Notes will no longer start automatically on login.");
}

bool MainWindow::isAutostartEnabled() const {
    return QFile::exists(autostartPath());
}

// --- Session ---

void MainWindow::saveSession() {
    m_settings.setValue("windowGeometry", saveGeometry());
    m_settings.setValue("lastNoteId", m_currentNoteId.toString(QUuid::WithoutBraces));
    m_settings.setValue("sortIndex", m_sortCombo->currentIndex());
}

void MainWindow::restoreSession() {
    QByteArray geo = m_settings.value("windowGeometry").toByteArray();
    if (!geo.isEmpty()) restoreGeometry(geo);
    else resize(900, 600);
    m_sortCombo->setCurrentIndex(m_settings.value("sortIndex", 0).toInt());
    loadNotes();

    QString lastId = m_settings.value("lastNoteId").toString();
    QUuid id(lastId);
    if (!id.isNull()) {
        for (int i = 0; i < m_noteList->count(); ++i) {
            if (m_noteList->item(i)->data(Qt::UserRole).toUuid() == id) {
                m_noteList->setCurrentRow(i);
                selectNote(id);
                break;
            }
        }
    }
}

// --- Dark mode ---

bool MainWindow::isDarkMode() const {
    return m_settings.value("darkMode", false).toBool();
}

void MainWindow::toggleDarkMode() {
    bool dark = !isDarkMode();
    m_settings.setValue("darkMode", dark);
    if (dark) {
        qApp->setStyleSheet(R"(
            * { background-color: #1e1e1e; color: #e0e0e0; }
            QLineEdit, QPlainTextEdit { background-color: #2d2d2d; border: 1px solid #555; color: #e0e0e0; }
            QLineEdit:focus, QPlainTextEdit:focus { border-color: #0078d4; }
            QListWidget { background-color: #252525; border: 1px solid #555; }
            QListWidget::item:selected { background: #0078d4; color: white; }
            QTreeWidget { background-color: #252525; border: 1px solid #555; }
            QTreeWidget::item:selected { background: #0078d4; color: white; }
            QPushButton { background: #0078d4; color: white; }
            QPushButton:hover { background: #005fa3; }
            #trashBtn { background: #555; }
            QMenuBar { background: #1e1e1e; color: #e0e0e0; }
            QMenuBar::item:selected { background: #333; }
            QMenu { background: #252525; color: #e0e0e0; border: 1px solid #555; }
            QMenu::item:selected { background: #0078d4; }
            QStatusBar { color: #999; }
            QComboBox { background-color: #2d2d2d; border: 1px solid #555; color: #e0e0e0; padding: 4px 8px; border-radius: 4px; }
            QTextBrowser { background-color: #2d2d2d; color: #e0e0e0; border: 1px solid #555; padding: 8px; }
        )");
    } else {
        qApp->setStyleSheet("");
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    saveSession();
    if (m_trayIcon && m_trayIcon->isVisible()) {
        hide();
        event->ignore();
    } else {
        QMainWindow::closeEvent(event);
    }
}
