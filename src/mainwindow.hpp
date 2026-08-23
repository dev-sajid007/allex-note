#pragma once

#include "notemanager.hpp"

#include <QMainWindow>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QSettings>
#include <QComboBox>
#include <QTreeWidget>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QSystemTrayIcon>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void newNote();
    void openSelectedNote();
    void saveNote();
    void saveNoteAs();
    void deleteNote();
    void onSearchChanged(const QString &text);
    void onContentChanged();
    void autoSave();
    void toggleDarkMode();
    void updateStatus();
    void togglePin();
    void changeColor();
    void onSortChanged(int index);
    void onFolderClicked(QTreeWidgetItem *item, int);
    void createFolder();
    void renameFolder();
    void deleteFolder();
    void toggleTrash();
    void restoreFromTrash();
    void permanentlyDeleteFromTrash();
    void exportTxt();
    void exportMd();
    void exportPdf();
    void moveToFolder();
    void togglePreview();
    void setReminder();
    void clearReminder();
    void checkReminders();
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);

private:
    void setupUi();
    void setupMenu();
    void setupShortcuts();
    void setupTray();
    void loadNotes();
    void populateList(const QList<Note> &notes);
    void populateTree();
    void selectNote(const QUuid &id);
    void clearEditor();
    bool isDarkMode() const;
    void showContextMenu(const QPoint &pos);
    void showFolderContextMenu(const QPoint &pos);
    void updateWordCount();
    void updateReminderLabel();
    void saveSession();
    void restoreSession();
    QString currentFolder() const;

    NoteManager *m_manager;
    QTimer *m_autoSaveTimer;
    QTimer *m_reminderTimer;
    QSettings m_settings;

    QUuid m_currentNoteId;
    bool m_isDirty = false;
    bool m_showingTrash = false;

    QSplitter *m_splitter;
    QLineEdit *m_searchBox;
    QPushButton *m_newButton;
    QComboBox *m_sortCombo;
    QPushButton *m_darkModeBtn;

    QStackedWidget *m_leftStack;
    QWidget *m_notesPage;
    QWidget *m_trashPage;

    QTreeWidget *m_folderTree;
    QListWidget *m_noteList;
    QListWidget *m_trashList;

    QLineEdit *m_titleEdit;
    QPlainTextEdit *m_editor;
    QTextBrowser *m_preview;
    QStackedWidget *m_editorStack;
    QPushButton *m_previewBtn;
    QLabel *m_reminderLabel;
    QLabel *m_statusCount;
    QLabel *m_statusSaved;
    QLabel *m_statusWords;

    QSystemTrayIcon *m_trayIcon;
    QSet<QUuid> m_firedReminders;
};
