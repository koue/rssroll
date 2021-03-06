CREATE TABLE tags (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	title VARCHAR(100),
	description VARCHAR(100),
	UNIQUE(title)
);

CREATE TABLE channels (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	tagid INTEGER,
	modified TIMESTAMP,
	link VARCHAR(100),
	language VARCHAR(20),
	title VARCHAR(100),
	description VARCHAR(100),
	UNIQUE(link)
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
