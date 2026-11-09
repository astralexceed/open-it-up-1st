# Open It UP: 1st Preservation

An open-source reimplementation of the **Pump It Up: 1st Dance Floor** (1999) engine, made to play the game on modern platforms without the original arcade hardware.

> This repository contains no game files. You need your own copy to play.

## About

Open It UP replicates the behavior of the original binary: game rules, timing, scoring, the life bar, screens, the service menu and game modes. The goal is to behave like the machine, not to reinterpret it. That is why the game runs at a fixed 60 frames per second, at any resolution you want.

- **Open source.** All the code in this repository is under the [MIT](LICENSE) license. Anyone can study it, fix it or port it.
- **BYOG: bring your own game.** The engine includes no images, music, charts or code from the original game. The launcher reads a backup of your own copy and extracts the files once, on your own computer. Nothing is uploaded anywhere.
- **100% AI-assisted.** This project was made entirely with the help of artificial intelligence. We aim to make the game available and preserve it, not to produce handcrafted work. Fixes are welcome.
- **Preservation.** We support the preservation and rescue of abandonware. Arcade games depend on hardware that ages and disappears, and this is an attempt to keep this one playable.

## Download and play

Download your version from the **Releases** section of this repository. None of them include game files.

| Version | File | Status |
|---|---|---|
| Windows | `.exe` | New, in testing |
| macOS (Apple Silicon) | `.zip` with the `.app` | Supported |
| Web | `.zip` | Supported |

You will need these files from your own copy:

| File | Purpose | Required? |
|---|---|---|
| `19990930.chd` | BGAs, steps, sprites and music | Yes |
| `piu10.u9` | Extra sound effects (grades, coins, menus and more) | No, but those effects will be missing |

**Windows.** Put the `.exe` in its own folder and open it. The program is not signed, so Windows may block it: choose **More info > Run anyway**.

**macOS.** Unzip the file, put the `.app` in its own folder and open it. It is not signed by Apple, so macOS will block it the first time: allow it in **System Settings > Privacy & Security > Open Anyway**.

**Web.** The page **does not work by double-clicking the file**, because browsers only let it store the game when it is opened from a server. Unzip the `.zip`, open a terminal in that folder and run this ([Python](https://www.python.org/downloads/) required):

```sh
python3 -m http.server 8000 --bind 127.0.0.1
```

Leave it running and open <http://localhost:8000/piu.html>. Press `Ctrl+C` to stop it. You need a recent browser with WebGL 2: Chrome, Edge, Firefox or Safari.

You can also upload the contents of the `.zip` to any static host, as long as it is served over **HTTPS**. Each visitor picks their own copy, and it is extracted in their browser. Do not upload the game files to the server.

To modify the game or compile it yourself, see [BUILDING.md](BUILDING.md).

### Updating and saved data

Download the new version and replace the program. On Windows and macOS, keep the `Data` folder next to it so you don't have to extract again. On the web, reload the page with `Cmd+Shift+R` or `Ctrl+Shift+R`.

| Version | Extracted files | Settings and controls |
|---|---|---|
| Web | That site's browsing data | That site's browsing data |
| macOS and Windows | `Data` folder, next to the game | `Data/settings.json` |

Extracted files take about 700 MB. On the web, clearing the site's data means extracting again. If macOS or Windows cannot write next to the program, they use `~/Library/Application Support/Open It UP` or `%LOCALAPPDATA%\Open It UP`.

## How to play

1. In the launcher, choose your `19990930.chd` and, if you have it, `piu10.u9`. You only do this once.
2. Adjust the aspect ratio, language and display mode, then press **Launch game**. Launching is like turning the machine on: it starts with no credits.
3. Insert coins. By default, 5 coins are 1 credit, and each player pays one credit.
4. Step on the **center** of your pad on the title screen.
5. On the selection screens, step on a corner to choose and on **the same corner again** to confirm. You have 20 seconds.
6. Step on each arrow as it reaches the fixed arrows at the top.

A second player can join during selection by stepping on their center panel, if there is a credit.

### Controls

These are the default keys.

| Panel | Player 1 | Player 2 |
|---|---|---|
| Up-left ↖ | `Q` | `U` |
| Up-right ↗ | `E` | `O` |
| Center ● | `S` | `K` |
| Down-left ↙ | `Z` | `M` |
| Down-right ↘ | `C` | `.` |

| Action | Key |
|---|---|
| Insert coin | `5` or `6` |
| Service menu: open and move | `F2` |
| Service menu: select | `F3` |
| Service menu: CLEAR button | `F4` |
| Back to the launcher (turns the machine off), outside the service menu | `Esc` |
| Close the game | `Cmd+Q` on macOS, `Alt+F4` on Windows |

On some Macs the `F` keys need `Fn`. Game keys do not work in the launcher.

To change them, open **Additional settings > Map controls** in the launcher. Click a panel, coin or service button, press the key you want, and press **Save** when you are done. A key cannot be used for two things. **Reset** restores the defaults, and `Esc` cancels. `Esc` cannot be assigned.

### Life and grades

You clear the song if your life is still up and you missed no more than half of the rows. With the default setting, you cannot be cut off mid-song on Stage 1. From the stage set by STAGE BREAK onward, the game ends if your life stays at the minimum long enough.

At the end you get a grade: **S, A, B, C, D or F**. An S requires no Bad or Miss.

### Service menu

| Option | Values | Default |
|---|---|---|
| GAME MODE | Normal, Event | Normal |
| DIFFICULTY | Easy, Normal, Hard | Easy |
| STAGE BREAK | 1, 2 or 3 Stage | 2 Stage |
| SHOW HELP | All, Easy Only, Easy and Normal | Easy Only |
| DEMO SOUND | Off, On | On |
| COIN SETTING (in COIN OPTION) | 1 to 10 coins per credit | 5 |

Use **SAVE AND EXIT** to save. Settings persist after closing the game. Credits are lost when the machine is turned off, as on the original.

## Game modes

| Corner | Mode | Players | Stages |
|---|---|---|---|
| ↖ | Easy | 1 or 2 | 3 |
| ↗ | Hard | 1 or 2 | 3 |
| ↙ | Double (1 player) or Battle (2 players) | 1 / 2 | 3 / 1 |
| ↘ | Non-Stop | 1 or 2 | 1 |

- **Easy.** The entry mode. Each stage offers 4 songs.
- **Hard.** The same structure with tougher steps.
- **Double.** One player uses both pads, with 10 panels on a centered field.
- **Non-Stop.** Long remixes with a 3D flight in the background. One stage, and nobody is cut off mid-song.
- **Battle.** Two players with alternating attack and defense blocks. In **attack**, white rows of 5 arrows arrive: step on the panels you want, on time. In **defense**, what your rival stepped on during their attack arrives on your side as steps you must hit. Nobody fails mid-song. At the end, scores are compared and **WIN** or **LOSE** appears.

### Event mode

Turn it on from the service menu: GAME OPTION, set GAME MODE to EVENT, then SAVE AND EXIT. It is the free-play mode meant for events.

- Exiting the service menu adds **2 credits**, and in this mode they are **never spent**.
- There is no mode screen: you go straight to choosing among 4 songs. The last two are remixes with the 3D background.
- It is **one song per game**. After the grade, it goes back to the start.
- The EVENT sign replaces the credit counter.

The title screen always requires one credit on the counter, even though Event does not charge it. If you close and reopen the game, there are 0 credits: enter and exit the service menu, or insert one credit's worth of coins. The original machine works the same way.

## Cheats

Enter them by stepping on the panels on the mode or song screen. The keys shown are the defaults. Each player has their own, and they last the whole game. Entering a code again removes it, except Copy and Reverse Copy, which are removed with Clear All.

| Cheat | Sequence | P1 keys | P2 keys | Effect |
|---|---|---|---|---|
| Vanish | ● ● ↗ ↖ ● ↘ ↙ ● | `S S E Q S C Z S` | `K K O U K . M K` | Arrows fade out before reaching the top. |
| Mirror | ● ● ↙ ● ↗ ↘ ● ↖ | `S S Z S E C S Q` | `K K M K O . K U` | Mirrors the arrows. Only applies if no other special modifier is active. |
| Random | ● ↙ ● ↘ ● ↗ ● ↖ | `S Z S C S E S Q` | `K M K . K O K U` | Each arrow falls randomly within its half of the pad. |
| X2 | ● ● ↖ ● ● ● ↗ ● | `S S Q S S S E S` | `K K U K K K O K` | Doubles the speed. |
| X4 | ● ● ↘ ● ● ↖ ● ● | `S S C S S Q S S` | `K K . K K U K K` | Quadruples the speed. |
| X8 | ● ● ↙ ● ↘ ● ● ● | `S S Z S C S S S` | `K K M K . K K K` | Multiplies the speed by 8. |
| Copy | ● ● ↖ ↗ ↖ ↗ ● ● | `S S Q E Q E S S` | `K K U O U O K K` | With two players, P2 gets P1's arrows. |
| Reverse Copy | ● ● ↗ ↖ ↗ ↖ ● ● | `S S E Q E Q S S` | `K K O U O U K K` | With two players, P2 gets P1's arrows inverted. |
| Clear All | ● ● ● ● ● ● ● ● | `S S S S S S S S` | `K K K K K K K K` | Removes all of that player's cheats. |

X2 and X4 together give X3.

## Disclaimer

This is a fan project with no affiliation with Andamiro or the Pump It Up rights holders. Trademarks and game content belong to their respective owners. Only use files obtained from a legal backup of your own copy.

## Thanks

To the whole Pump It Up community: to everyone who has kept the machines alive, documented the game and kept dancing for more than 25 years. This is for you. We hope you enjoy it.
