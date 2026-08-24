<div align="center">

# Allex Notes

A modern, feature-rich note-taking application built with Qt6 and C++17.

[![Qt6](https://img.shields.io/badge/Qt6-6.11-41CD52?style=flat&logo=qt)](https://qt.io)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat&logo=cplusplus)](https://isocpp.org)
[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux-lightgrey?style=flat)]()

</div>

---

## Screenshots

```
┌─────────────────────────────────────────────────────────────┐
│  Allex Notes                         🔍 Search       + New  │
├───────────────┬─────────────────────────────────────────────┤
│  All Notes (4)│                                             │
│  📁 C++ (2)   │  My first note                              │
│  📁 Linux (1) │  ───────────────────────────────────────    │
│               │                                             │
│  📄 C++       │  Start writing your note here...            │
│  📄 Linux     │                                             │
│  📄 Fedora    │                                             │
│  📄 Projects  │                                             │
├───────────────┴─────────────────────────────────────────────┤
│  4 Notes                                      Saved ✓       │
└─────────────────────────────────────────────────────────────┘
```

---

## Features

### Core
- **Create / Open / Save / Delete** notes with auto-save
- **Search** notes by title or content in real-time
- **Notes sidebar** with live sorting (Modified / Created / Title)

### Organization
- **Folders** — organize notes into folders, create/rename/delete
- **Pin notes** — pin important notes to the top of the list
- **Color tags** — 6 color options to visually categorize notes
- **Trash bin** — soft delete with restore or permanent delete

### Editing
- **Markdown preview** — toggle edit/preview with `Ctrl+P`
- **Word & character count** — live stats in the status bar
- **Export** — save as `.txt`, `.md`, or `.pdf`

### Experience
- **Google Drive sync** — sign in once, notes auto-sync to your Drive
- **Dark mode** — toggle with `Ctrl+D`, persisted across sessions
- **Reminders** — set datetime reminders with desktop notifications
- **System tray** — minimize to tray, click to show/hide
- **Session restore** — window position and last note restored on startup
- **Keyboard shortcuts** — all major actions accessible via shortcuts

---

## Keyboard Shortcuts

| Action | Shortcut |
|---|---|
| New note | `Ctrl+N` |
| Save | `Ctrl+S` |
| Save as | `Ctrl+Shift+S` |
| Search | `Ctrl+F` |
| Toggle preview | `Ctrl+P` |
| Toggle dark mode | `Ctrl+D` |
| Delete note | `Delete` |
| Sync now | `Ctrl+Shift+G` |
| Quit | `Ctrl+Q` |

---

## Build & Run

### Prerequisites

- CMake 3.20+
- GCC 11+ or Clang 12+
- Qt6 Widgets + PrintSupport + NetworkAuth

### Install dependencies (Fedora)

```bash
sudo dnf install qt6-qtbase-devel qt6-qtnetworkauth-devel cmake gcc-c++
```

### Build

```bash
git clone https://github.com/dev-sajid007/allex-note.git
cd allex-note
cmake -B build
cmake --build build
```

### Run

```bash
./build/allex-notes
```

---

## Project Structure

```
allex-note/
├── CMakeLists.txt              # Build configuration
├── src/
│   ├── main.cpp                # Application entry point
│   ├── mainwindow.cpp/hpp      # Main window UI + logic
│   ├── note.cpp/hpp            # Note data model (JSON serialization)
│   ├── notemanager.cpp/hpp     # CRUD operations + file I/O
│   ├── authmanager.cpp/hpp     # Google OAuth2 authentication
│   └── syncmanager.cpp/hpp     # Google Drive sync engine
├── resources/
│   └── resources.qrc           # Qt resource file
├── assets/
│   └── icons/                  # Application icons
└── data/                       # Note storage (runtime)
```

---

## Storage

Notes are stored as JSON files in:

```
~/.local/share/Allex/AllexNotes/data/
├── <uuid>.json                 # Active notes
└── trash/                      # Trashed notes
    └── <uuid>.json
```

Each note contains: id, title, content, created/modified timestamps, folder, color, pin status, and optional reminder datetime.

---

## Technologies

| Technology | Purpose |
|---|---|
| **Qt6 Widgets** | GUI framework |
| **C++17** | Language standard |
| **CMake** | Build system |
| **QJsonDocument** | Note serialization |
| **QSystemTrayIcon** | System tray integration |
| **QPrinter** | PDF export |
| **QTextDocument** | Markdown rendering |
| **QOAuth2** | Google OAuth2 authentication |
| **QNetworkAccessManager** | Drive REST API calls |
| **notify-send** | Desktop notifications |

---

## Google Drive Sync

### Setup (one-time, ~5 minutes)

1. Go to [console.cloud.google.com](https://console.cloud.google.com)
2. Create a new project (or use existing)
3. Enable **Google Drive API** in the API Library
4. Configure **OAuth consent screen**: External, add your Gmail as test user
5. Go to **Credentials** → Create **OAuth Client ID** → Desktop app
6. Copy the **Client ID** and **Client Secret**
7. In Allex Notes: **Sync → Sign in to Google** → paste credentials
8. Browser opens → sign in with your Google account → grant access
9. Done! Notes auto-sync every 5 minutes and after each save

### How it works

- Notes sync to a hidden `appDataFolder` in your Google Drive (won't appear in your main Drive)
- Each note = one `<uuid>.json` file in the Drive
- **Conflict resolution**: last-modified timestamp wins
- **Merge**: notes on other devices auto-download, local changes auto-upload
- OAuth tokens auto-refresh (valid for ~7 days in test mode)

### Menu

| Action | How |
|---|---|
| Sign in | Sync → Sign in to Google |
| Sign out | Sync → Sign out |
| Manual sync | Sync → Sync Now (`Ctrl+Shift+G`) |
| Status | Status bar shows `☁ Synced 2m ago` |

---

## License

MIT License

---

<div align="center">

**Built with Qt6 + C++17**

</div>
