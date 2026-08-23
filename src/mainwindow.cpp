#include "mainwindow.hpp"

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
#include <algorithm>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_manager(new NoteManager(
          QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/data"))
    , m_autoSaveTimer(new QTimer(this))
    , m_reminderTimer(new QTimer(this))
    , m_settings("Allex", "AllexNotes")
{
    setupUi();
    setupMenu();
    setupShortcuts();
    setupTray();

    connect(m_autoSaveTimer, &QTimer::timeout, this, &MainWindow::autoSave);
    m_autoSaveTimer->setSingleShot(true);

    connect(m_reminderTimer, &QTimer::timeout, this, &MainWindow::checkReminders);
    m_reminderTimer->start(30000); // check every 30s

    if (isDarkMode())
        toggleDarkMode();

    restoreSession();

    setWindowTitle("Allex Notes");
}

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
    m_darkModeBtn->setToolTip("Toggle dark mode");
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

    // --- Notes page (folder tree + note list) ---
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

    // --- Trash page ---
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

    // Trash toggle button
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

    // Editor toolbar row
    QHBoxLayout *editorToolbar = new QHBoxLayout;
    m_previewBtn = new QPushButton("Preview");
    m_previewBtn->setCheckable(true);
    m_previewBtn->setToolTip("Toggle Markdown preview (Ctrl+P)");
    m_previewBtn->setFixedWidth(80);
    m_reminderLabel = new QLabel("");
    m_reminderLabel->setStyleSheet("color: #e67e22; font-size: 11px;");
    m_reminderLabel->setToolTip("Reminder set");
    editorToolbar->addStretch();
    editorToolbar->addWidget(m_reminderLabel);
    editorToolbar->addWidget(m_previewBtn);
    editorLayout->addLayout(editorToolbar);

    // Editor + Preview stacked
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
    connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onSortChanged);
    connect(m_noteList, &QListWidget::customContextMenuRequested,
            this, &MainWindow::showContextMenu);
    connect(m_folderTree, &QTreeWidget::customContextMenuRequested,
            this, &MainWindow::showFolderContextMenu);
    connect(m_folderTree, &QTreeWidget::itemClicked,
            this, &MainWindow::onFolderClicked);
    connect(trashBtn, &QPushButton::clicked, this, &MainWindow::toggleTrash);
    connect(trashBackBtn, &QPushButton::clicked, this, &MainWindow::toggleTrash);
    connect(emptyTrashBtn, &QPushButton::clicked, this, [this]() {
        auto reply = QMessageBox::question(this, "Empty Trash",
            "Permanently delete ALL notes in trash?",
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            m_manager->emptyTrash();
            loadNotes();
        }
    });
    connect(m_previewBtn, &QPushButton::clicked, this, &MainWindow::togglePreview);

    // --- Stylesheet ---
    setStyleSheet(R"(
        #noteList {
            border: 1px solid #ccc;
            border-radius: 4px;
            padding: 4px;
        }
        #noteList::item {
            padding: 6px 4px;
            border-bottom: 1px solid #eee;
        }
        #noteList::item:selected {
            background: #0078d4;
            color: white;
        }
        QLineEdit, QPlainTextEdit {
            border: 1px solid #ccc;
            border-radius: 4px;
            padding: 4px 8px;
        }
        QLineEdit:focus, QPlainTextEdit:focus {
            border-color: #0078d4;
        }
        QPushButton {
            padding: 6px 12px;
            border-radius: 4px;
            background: #0078d4;
            color: white;
            border: none;
        }
        QPushButton:hover {
            background: #005fa3;
        }
        #trashBtn {
            background: #555;
        }
        QStatusBar {
            font-size: 11px;
            color: #666;
        }
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
    editMenu->addAction("Delete", QKeySequence("Delete"), this, &MainWindow::deleteNote);

    QMenu *viewMenu = menuBar()->addMenu("View");
    viewMenu->addAction("Toggle Dark Mode", QKeySequence("Ctrl+D"), this, &MainWindow::toggleDarkMode);
    viewMenu->addAction("Toggle Preview", QKeySequence("Ctrl+P"), this, &MainWindow::togglePreview);
    viewMenu->addAction("New Folder", this, &MainWindow::createFolder);
}

void MainWindow::setupShortcuts() {
    QShortcut *searchShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(searchShortcut, &QShortcut::activated, this, [this]() { m_searchBox->setFocus(); });

    QShortcut *previewShortcut = new QShortcut(QKeySequence("Ctrl+P"), this);
    connect(previewShortcut, &QShortcut::activated, this, &MainWindow::togglePreview);
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
        for (const Note &n : all)
            if (n.folder() == f) count++;
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
    if (m_showingTrash) {
        m_statusCount->setText(QString("Trash: %1 Note%2").arg(count).arg(count == 1 ? "" : "s"));
    } else {
        m_statusCount->setText(QString("%1 Note%2").arg(count).arg(count == 1 ? "" : "s"));
    }
    m_statusSaved->setText(m_isDirty ? "Unsaved" : "Saved \u2713");
}

// --- Word count ---

void MainWindow::updateWordCount() {
    QString text = m_editor->toPlainText();
    int chars = text.length();
    int words = text.isEmpty() ? 0 :
        text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).count();
    m_statusWords->setText(QString("%1 words, %2 chars").arg(words).arg(chars));
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
    QString name = QInputDialog::getText(this, "New Folder", "Folder name:",
                                          QLineEdit::Normal, {}, &ok);
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
    QString newName = QInputDialog::getText(this, "Rename Folder",
        "New name:", QLineEdit::Normal, oldName, &ok);
    if (!ok || newName.trimmed().isEmpty()) return;
    if (newName == oldName) return;

    for (Note &n : m_manager->allNotes()) {
        if (n.folder() == oldName) {
            n.setFolder(newName.trimmed());
            m_manager->saveNote(n);
        }
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
        if (n.folder() == folder)
            m_manager->deleteNote(n.id());
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
    QString folder = QInputDialog::getItem(this, "Move to Folder",
        "Select folder:", folders, 0, true, &ok);
    if (!ok) return;

    Note n = m_manager->loadNote(m_currentNoteId);
    n.setFolder(folder);
    m_manager->saveNote(n);
    loadNotes();
}

// --- Context menu (pin + color + move + delete) ---

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
    if (note.hasReminder()) {
        menu.addAction("Clear Reminder", this, &MainWindow::clearReminder);
    }
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

// --- CRUD slots ---

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
    m_editor->setPlainText(note.content());
    m_editorStack->setCurrentIndex(0);
    m_previewBtn->setText("Preview");
    m_previewBtn->setChecked(false);
    m_isDirty = false;
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
    m_isDirty = false;
    m_reminderLabel->clear();
    updateWordCount();
    updateStatus();
}

void MainWindow::saveNote() {
    if (m_currentNoteId.isNull()) return;
    Note note = m_manager->loadNote(m_currentNoteId);
    if (note.isNull()) return;

    note.setTitle(m_titleEdit->text());
    note.setContent(m_editor->toPlainText());
    m_manager->saveNote(note);
    m_isDirty = false;

    for (int i = 0; i < m_noteList->count(); ++i) {
        QListWidgetItem *item = m_noteList->item(i);
        if (item->data(Qt::UserRole).toUuid() == m_currentNoteId) {
            QString prefix = note.isPinned() ? "\u2691 " : "";
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
    auto reply = QMessageBox::question(this, "Delete Note",
        "Move this note to trash?",
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
    if (m_showingTrash) {
        m_leftStack->setCurrentIndex(1);
        m_newButton->setEnabled(false);
    } else {
        m_leftStack->setCurrentIndex(0);
        m_newButton->setEnabled(true);
        loadNotes();
    }
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
        "This cannot be undone. Delete permanently?",
        QMessageBox::Yes | QMessageBox::No);
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
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(m_editor->toPlainText().toUtf8());
    }
}

void MainWindow::exportMd() {
    if (m_currentNoteId.isNull()) return;
    QString path = QFileDialog::getSaveFileName(this, "Export as Markdown", {}, "Markdown (*.md)");
    if (path.isEmpty()) return;
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(m_editor->toPlainText().toUtf8());
    }
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

void MainWindow::autoSave() { saveNote(); }

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
            QLineEdit, QPlainTextEdit {
                background-color: #2d2d2d; border: 1px solid #555; color: #e0e0e0;
            }
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
            QComboBox {
                background-color: #2d2d2d; border: 1px solid #555;
                color: #e0e0e0; padding: 4px 8px; border-radius: 4px;
            }
            QTextBrowser {
                background-color: #2d2d2d; color: #e0e0e0; border: 1px solid #555;
                padding: 8px;
            }
        )");
    } else {
        qApp->setStyleSheet("");
    }
}

// --- Reminder ---

void MainWindow::updateReminderLabel() {
    if (m_currentNoteId.isNull()) { m_reminderLabel->clear(); return; }
    Note note = m_manager->loadNote(m_currentNoteId);
    if (note.hasReminder()) {
        m_reminderLabel->setText("\u23F0 " + note.reminder().toString("MMM d, h:mm AP"));
        m_reminderLabel->setToolTip("Reminder set — click Edit > Clear Reminder to remove");
    } else {
        m_reminderLabel->clear();
    }
}

void MainWindow::setReminder() {
    if (m_currentNoteId.isNull()) return;

    Note note = m_manager->loadNote(m_currentNoteId);

    QDateTimeEdit *dateEdit = new QDateTimeEdit(note.hasReminder() ? note.reminder() : QDateTime::currentDateTime().addSecs(3600));
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

            // Desktop notification via notify-send
            QProcess::startDetached("notify-send", {
                "-a", "Allex Notes",
                "-i", "text-editor",
                "Reminder: " + note.title(),
                note.content().left(100)
            });

            // System tray notification
            m_trayIcon->showMessage("Reminder: " + note.title(),
                note.content().left(200),
                QSystemTrayIcon::Information, 5000);

            // Flash window if minimized
            if (!isVisible()) {
                show();
                raise();
                activateWindow();
            }
        }
    }
}

// --- Tray ---

void MainWindow::setupTray() {
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon::fromTheme("text-editor",
        QApplication::style()->standardIcon(QStyle::SP_FileIcon)));
    m_trayIcon->setToolTip("Allex Notes");

    QMenu *trayMenu = new QMenu;
    trayMenu->addAction("Show", this, [this]() { show(); raise(); activateWindow(); });
    trayMenu->addAction("New Note", this, &MainWindow::newNote);
    trayMenu->addSeparator();
    trayMenu->addAction("Quit", qApp, &QApplication::quit);
    m_trayIcon->setContextMenu(trayMenu);

    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this, &MainWindow::onTrayActivated);

    m_trayIcon->show();
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger) {
        if (isVisible()) {
            hide();
        } else {
            show();
            raise();
            activateWindow();
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    saveSession();
    if (m_trayIcon->isVisible()) {
        hide();
        event->ignore();
    } else {
        QMainWindow::closeEvent(event);
    }
}
