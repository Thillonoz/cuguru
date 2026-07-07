# Cuguru

A [Suguru](https://en.wikipedia.org/wiki/Suguru_(puzzle)) logic puzzle game developed entirely in C. 

## 🧠 How to Play

### The Rules of Suguru
Suguru (sometimes called Tectonic) is a number placement puzzle with three simple rules:
1. **The Blocks:** The grid is divided into irregularly shaped blocks, usually containing between 1 and 5 cells.
2. **The Numbers:** Fill every cell with a number from 1 to *n*, where *n* is the total number of cells in that specific block. (For example, a 3-cell block must contain the numbers 1, 2, and 3).
3. **The Restriction:** The same number **cannot** touch another instance of itself in any direction—horizontally, vertically, or diagonally.

### 🎮 Controls

| Action | Key |
|---|---|
| **Enter Number** | `1` - `9` |
| **Clear Cell** | `Backspace` |
| **Quit Game** | `Esc` |

---

## 🛠️ Build & Run

This project uses `make` to manage the build process. 

| Command | Description |
|---|---|
| `make run` | Compiles the source code and immediately starts the game. |
| `make clean` | Cleans up the directory by removing the compiled main executable and build artifacts. |

---

## ☕ Support

If you enjoy playing Cuguru or found the code helpful, consider supporting the project!

[![Buy Me A Coffee](https://img.shields.io/badge/Buy%20Me%20a%20Coffee-ffdd00?style=for-the-badge&logo=buy-me-a-coffee&logoColor=black)](https://buymeacoffee.com/thillonoz)
