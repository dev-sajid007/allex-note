<div align="center">

# Allex Notes

A modern, feature-rich note-taking application built with Qt6 and C++17.

[![Qt6](https://img.shields.io/badge/Qt6-6.11-41CD52?style=flat&logo=qt)](https://qt.io)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat&logo=cplusplus)](https://isocpp.org)
[![CMake](https://img.shields.io/badge/CMake-3.20+-064F82?style=flat&logo=cmake)](https://cmake.org)
[![OpenSSL](https://img.shields.io/badge/OpenSSL-3.x-8B5CF6?style=flat)](https://openssl.org)
[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux-lightgrey?style=flat)]()

</div>

---

## Screenshots

```
┌─────────────────────────────────────────────────────────────────────┐
│  Allex Notes                              🔍 Search    + New        │
├──────────┬──────────────────────────────────────────────────────────┤
│  FILE    │                                                          │
│          │  My first note                                           │
│  VIEW    │  ──────────────────────────────────────────────────────  │
│  SYNC    │                                                          │
│  SETTINGS│  Start writing your note here...                         │
│          │                                                          │
│  FOLDERS │                                                          │
│  📁 C++  │                                                          │
│  📁 Linux│                                                          │
│          │                                                          │
│  NOTES   ├──────────────────────────────────────────────────────────┤
│  📄 C++  │  Words: 12   Characters: 87              ☁ Synced ✓     │
│  📄 Linux│                                                          │
│  📄 Note1└──────────────────────────────────────────────────────────┘
└─────────────────────────────────────────────────────────────────────┘
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
- **App icon** — custom notepad icon, bundled as SVG + PNG

### Security
- **Master password** — PBKDF2-HMAC-SHA256 + AES-256-GCM encryption (OpenSSL)
- **Lock screen** — app locks on startup or manually from tray
- **Per-note lock** — encrypt individual notes, content unreadable without password

### Backup & Search
- **Backup/Restore** — export all notes to `.allex` JSON bundle, import with merge
- **Search highlight** — matches highlighted in yellow in the editor
- **F3 navigation** — jump to next search match

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
| Backup | `Ctrl+Shift+B` |
| Sync now | `Ctrl+Shift+G` |
| Next match | `F3` |
| Quit | `Ctrl+Q` |

---

## Build & Run

### Prerequisites

- CMake 3.20+
- GCC 11+ or Clang 12+
- Qt6 Widgets + PrintSupport + NetworkAuth + Svg (for icon rendering)
- OpenSSL 3.x (for AES-256-GCM encryption)

### Install dependencies (Fedora)

```bash
sudo dnf install qt6-qtbase-devel qt6-qtnetworkauth-devel qt6-qtsvg-devel openssl-devel cmake gcc-c++
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

### Install (system-wide)

```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --install build
```

---

## Project Structure

```
allex-note/
├── CMakeLists.txt              # Build configuration + install targets
├── allex-notes.desktop         # Desktop launcher entry
├── src/
│   ├── main.cpp                # Application entry point
│   ├── mainwindow.cpp/hpp      # Main window UI + all features
│   ├── note.cpp/hpp            # Note data model (JSON serialization)
│   ├── notemanager.cpp/hpp     # CRUD operations + file I/O
│   ├── authmanager.cpp/hpp     # Google OAuth2 authentication
│   ├── syncmanager.cpp/hpp     # Google Drive sync engine
│   └── crypto.cpp/hpp          # AES-256-GCM + PBKDF2 encryption
├── assets/icons/
│   ├── allex-notes.svg         # Source icon
│   └── allex-notes-{16..256}.png  # Rasterized icons
├── resources/
│   └── resources.qrc           # Qt resource file (icons embedded)
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

Each note contains: id, title, content, created/modified timestamps, folder, color, pin status, optional reminder datetime, and optional locked status (encrypted content stored as ciphertext).

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
| **OpenSSL** | AES-256-GCM encryption |
| **QCryptographicHash** | PBKDF2 password hashing |
| **notify-send** | Desktop notifications |

---

## Password Protection

### Setup

1. **Settings → Set Master Password** → enter and confirm password
2. On next startup, lock screen appears automatically

### Features

| Feature | How |
|---|---|
| Lock app | Settings → Lock App, or tray → Lock |
| Lock a note | Right-click note → Lock Note |
| Unlock note | Right-click → Unlock Note → enter password |
| Change password | Settings → Set Master Password (enter old first) |
| Remove password | Set new password → leave blank → confirm |

### Encryption

- Passwords hashed with **PBKDF2-HMAC-SHA256** (100,000 iterations, random salt)
- Note content encrypted with **AES-256-GCM** (authenticated encryption)
- Salt stored in `QSettings`, never in plaintext

---

## Backup & Restore

### Backup

File → Backup → saves a `.allex` file containing all notes as JSON.

### Restore

File → Restore → import a `.allex` backup file. Merge logic:
- Notes missing locally → added
- Notes modified after backup → kept if backup is newer
- Existing notes → kept if local is newer

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

## What's New

| Version | Features |
|---|---|
| **v1** | Basic CRUD, search, auto-save, dark mode, reminders, system tray |
| **v2** | Google Drive sync, folders, pin notes, color tags, trash bin, markdown preview, export |
| **v3** | App icon, desktop launcher, autostart, master password (AES-256-GCM), backup/restore, search highlight |

---

## Troubleshooting

| Issue | Fix |
|---|---|
| `No Icon set` or crash on launch | Rebuild with `cmake --build build` (icon resource must be compiled) |
| Segfault on restore session | Ensure latest code is built (`git pull && cmake --build build`) |
| Tray icon not visible | Some Wayland sessions don't support system tray — app runs normally without it |
| Sync fails | Re-authenticate via **Sync → Sign in to Google** |

---

## License

MIT License

---

<div align="center">

**Built with Qt6 + C++17**

</div>
