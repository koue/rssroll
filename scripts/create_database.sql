CREATE TABLE categories (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	title VARCHAR(100),
	description VARCHAR(100)
);

CREATE TABLE channels (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	catid INTEGER,
	modified TIMESTAMP,
	link VARCHAR(100),
	language VARCHAR(20),
	title VARCHAR(100),
	description VARCHAR(100)
);

CREATE TABLE feeds (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	chanid INTEGER,
	modified TIMESTAMP,
	link VARCHAR(100),
	title VARCHAR(100),
	description TEXT,
	pubdate VARCHAR(50)
);

CREATE INDEX feeds_pubdate_idx on feeds(pubdate);
