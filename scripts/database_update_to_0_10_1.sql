CREATE TABLE tags (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	title VARCHAR(100),
	description VARCHAR(100),
	UNIQUE(title)
);

INSERT INTO tags SELECT * FROM categories;
ALTER TABLE channels RENAME COLUMN catid TO tagid;
DROP TABLE categories;
