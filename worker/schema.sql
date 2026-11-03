-- Classement VELOCITY (D1 leaderboard) : npx wrangler d1 execute leaderboard --remote --file schema.sql
CREATE TABLE IF NOT EXISTS players (id TEXT PRIMARY KEY, keyhash TEXT NOT NULL, name TEXT NOT NULL, mmr INTEGER NOT NULL, played INTEGER NOT NULL, wins INTEGER NOT NULL, updated INTEGER NOT NULL);
CREATE INDEX IF NOT EXISTS players_mmr ON players (played, mmr);
CREATE TABLE IF NOT EXISTS matches (mid TEXT PRIMARY KEY, created INTEGER NOT NULL, done INTEGER NOT NULL, result TEXT);
CREATE TABLE IF NOT EXISTS reports (mid TEXT NOT NULL, reporter TEXT NOT NULL, body TEXT NOT NULL, at INTEGER NOT NULL, PRIMARY KEY (mid, reporter));
CREATE TABLE IF NOT EXISTS rate (id TEXT PRIMARY KEY, win INTEGER NOT NULL, n INTEGER NOT NULL);
