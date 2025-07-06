# Proposed SQLite Database Schema

This document outlines the proposed SQLite database schema for storing player data, migrating from the existing JSON-based storage.

## Schema Overview

The proposed schema consists of two tables: `players` and `playlist_records`. This design normalizes the data, making it more scalable and efficient to query compared to a single JSON file.

### `players` Table

This table stores general information about each player.

| Column          | Data Type | Constraints      | Description                  |
| --------------- | --------- | ---------------- | ---------------------------- |
| `id`            | `TEXT`    | `PRIMARY KEY`    | The player's unique SteamID. |
| `name`          | `TEXT`    | `NOT NULL`       | The player's last known name.  |
| `met_count`     | `INTEGER` | `DEFAULT 0`      | Number of times met.         |
| `offense_ratio` | `REAL`    | `DEFAULT 0.5`    | Calculated offense playstyle ratio. |
| `defense_ratio` | `REAL`    | `DEFAULT 0.5`    | Calculated defense playstyle ratio. |
| `note`          | `TEXT`    |                  | A user-defined note about the player. |

### `playlist_records` Table

This table stores player statistics and win/loss records for each specific game playlist. It has a many-to-one relationship with the `players` table.

| Column           | Data Type | Constraints      | Description                               |
| ---------------- | --------- | ---------------- | ----------------------------------------- |
| `id`             | `INTEGER` | `PRIMARY KEY AUTOINCREMENT` | A unique identifier for the record.         |
| `player_id`      | `TEXT`    | `FOREIGN KEY(players.id)` | The ID of the player this record belongs to. |
| `playlist_id`    | `INTEGER` | `NOT NULL`       | The ID of the game playlist.              |
| `with_wins`      | `INTEGER` | `DEFAULT 0`      | Wins with this player on your team.       |
| `with_losses`    | `INTEGER` | `DEFAULT 0`      | Losses with this player on your team.     |
| `against_wins`   | `INTEGER` | `DEFAULT 0`      | Wins against this player.                 |
| `against_losses` | `INTEGER` | `DEFAULT 0`      | Losses against this player.               |
| `goals`          | `INTEGER` | `DEFAULT 0`      | Total goals scored.                       |
| `assists`        | `INTEGER` | `DEFAULT 0`      | Total assists made.                       |
| `shots`          | `INTEGER` | `DEFAULT 0`      | Total shots taken.                        |
| `saves`          | `INTEGER` | `DEFAULT 0`      | Total saves made.                         |
| `epic_saves`     | `INTEGER` | `DEFAULT 0`      | Total epic saves made.                    |
| `clears`         | `INTEGER` | `DEFAULT 0`      | Total clears.                             |
| `demos`          | `INTEGER` | `DEFAULT 0`      | Total demolitions.                        |
| `center_balls`   | `INTEGER` | `DEFAULT 0`      | Total center balls.                       |
| `hat_tricks`     | `INTEGER` | `DEFAULT 0`      | Total hat tricks.                         |
| `playmakers`     | `INTEGER` | `DEFAULT 0`      | Total playmakers (3 assists).             |
| `saviors`        | `INTEGER` | `DEFAULT 0`      | Total saviors (3 saves).                  |

### Relationship Diagram

```mermaid
erDiagram
    players {
        TEXT id PK
        TEXT name
        INTEGER met_count
        REAL offense_ratio
        REAL defense_ratio
        TEXT note
    }
    playlist_records {
        INTEGER id PK
        TEXT player_id FK
        INTEGER playlist_id
        INTEGER with_wins
        INTEGER with_losses
        INTEGER against_wins
        INTEGER against_losses
        INTEGER goals
        INTEGER assists
        INTEGER shots
        INTEGER saves
        INTEGER epic_saves
        INTEGER clears
        INTEGER demos
        INTEGER center_balls
        INTEGER hat_tricks
        INTEGER playmakers
        INTEGER saviors
    }
    players ||--o{ playlist_records : "has"
