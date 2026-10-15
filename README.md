# Item Level Scaling for Turtle WoW (vMaNGOS)

![mod-twow-item-level-scaling](mod-twow-item-level-scaling.jpg)

Dynamic dungeon and raid equipment scaling module for **Turtle WoW** custom vMaNGOS (`tortoise-wow` client 1.18.1.7272 / Interface 11200) and **TortoiseBots**.

The module dynamically adapts instanced equipment drops to the current party and encounter hierarchy, permanently publishing persistent synthetic variants into the native `ItemPrototype` store without modifying engine sources or requiring client patches.

---

## 1. Key Features

- **Dynamic Encounter Scaling**: Scales item stats, armor, weapon damage, and required levels upward or downward based on creature levels or highest real player levels.
- **Native Loot Preservation**: Skips scaling automatically when original Blizzard loot already matches target progression.
- **Controlled Variance**: Optional floor/ceiling variance rolls for lucky drop upgrades.
- **Zero Core Modifications**: Fully adheres to **The Unediting Doctrine**; zero edits or patches to `tortoise-wow` or `TortoiseBots`.
- **Pre-Allocated Template Pool**: Generates and manages runtime synthetic item variants safely without memory rehashing or pointer invalidation.
- **Durable InnoDB Persistence**: All generated variants are saved transactionally to the world database and restored seamlessly on server restarts.
- **TortoiseBots Interoperability**: Excludes headless playerbots from determining scaling levels when `ItemScaling.RealPlayersOnly = 1`.
- **Cross-Platform**: Fully compatible with Linux (Ubuntu 22.04 / 24.04 LTS) and Windows.

---

## 2. Installation

### 2.1 Clone or Link the Module

Place the module inside the `modules/` directory of your TortoiseWoW tree:

```bash
cd /path/to/tortoise-wow/modules
git clone https://github.com/Ildourol/mod-twow-item-level-scaling.git mod-twow-item-level-scaling
```

Module structure:

```text
tortoise-wow/
├── modules/
│   └── mod-twow-item-level-scaling/
│       ├── conf/
│       │   └── mod-twow-item-level-scaling.conf.dist
│       ├── data/
│       │   └── sql/world/
│       ├── src/
│       ├── mod-twow-item-level-scaling.cmake
│       ├── mod-twow-item-level-scaling.jpg
│       └── README.md
```

### 2.2 Configuration

Copy the distributed configuration template:

```bash
cp modules/mod-twow-item-level-scaling/conf/mod-twow-item-level-scaling.conf.dist \
   modules/mod-twow-item-level-scaling/conf/mod-twow-item-level-scaling.conf
```

At minimum, ensure the module is enabled in `mod-twow-item-level-scaling.conf`:

```ini
ItemScaling.Enable = 1
```

### 2.3 Configure and Build TortoiseWoW

**Linux / Ubuntu (22.04 & 24.04 LTS):**

```bash
# Install dependencies
sudo apt update && sudo apt install -y build-essential cmake git libace-dev libmysqlclient-dev libssl-dev libbz2-dev zlib1g-dev

cd /path/to/tortoise-wow
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DMODULE_MOD_TWOW_ITEM_LEVEL_SCALING=static
cmake --build . --target mangosd -j$(nproc)
```

**Windows (Visual Studio 2022):**

```powershell
cd C:\path\to\tortoise-wow
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DMODULE_MOD_TWOW_ITEM_LEVEL_SCALING=static
cmake --build build --config Release --target mangosd --parallel
```

### 2.4 Database Migrations

The SQL migrations under `data/sql/world/` are automatically applied by the TortoiseWoW database auto-updater at server startup.

---

## 3. Chat Commands

Gamemaster commands (available in-game and via server console):

- `.itemscaling status` or `.mils status` — Displays operational status, active scaling method, diagnostics, and template cache metrics.
- `.itemscaling preview <itemId> <targetLevel>` — Simulates item scaling for a specific item and target level, outputting projected stats, armor, weapon damage, and item level.
- `.itemscaling reload` — Reloads module configuration without restarting the worldserver.

---

## 4. License & Credits

- Developed for TortoiseWoW / Turtle WoW 1.18.1 by Antigravity (Google DeepMind).
- Licensed under the GNU General Public License v2 (GPLv2).
