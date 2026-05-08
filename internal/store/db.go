package store

import (
	"database/sql"
	"fmt"
	"os"
	"path/filepath"
	"time"

	_ "github.com/mattn/go-sqlite3"
)

type Clip struct {
	ID         int64
	Content    string
	ImagePath  string
	MimeType   string
	IsImage    bool
	IsFav      bool
	SourceApp  string
	CreatedAt  time.Time
}

type Store struct {
	db *sql.DB
}

func New(dbPath string) (*Store, error) {
	dir := filepath.Dir(dbPath)
	if err := os.MkdirAll(dir, 0755); err != nil {
		return nil, fmt.Errorf("create db dir: %w", err)
	}

	db, err := sql.Open("sqlite3", dbPath+"?_journal_mode=WAL&_busy_timeout=5000")
	if err != nil {
		return nil, fmt.Errorf("open db: %w", err)
	}

	s := &Store{db: db}
	if err := s.migrate(); err != nil {
		db.Close()
		return nil, fmt.Errorf("migrate: %w", err)
	}
	return s, nil
}

func (s *Store) Close() error {
	return s.db.Close()
}

func (s *Store) migrate() error {
	schema := `
	CREATE TABLE IF NOT EXISTS clips (
		id         INTEGER PRIMARY KEY AUTOINCREMENT,
		content    TEXT,
		image_path TEXT,
		mime_type  TEXT NOT NULL DEFAULT 'text/plain',
		is_image   BOOLEAN DEFAULT 0,
		is_fav     BOOLEAN DEFAULT 0,
		source_app TEXT DEFAULT '',
		created_at DATETIME DEFAULT CURRENT_TIMESTAMP
	);
	CREATE INDEX IF NOT EXISTS idx_clips_created ON clips(created_at DESC);
	`
	_, err := s.db.Exec(schema)
	return err
}

// Add inserts a new clip (skipping duplicates of the most recent entry).
func (s *Store) Add(c *Clip) error {
	// check duplicate: if the most recent clip has the same content, skip
	var lastContent sql.NullString
	err := s.db.QueryRow(
		"SELECT content FROM clips ORDER BY created_at DESC LIMIT 1",
	).Scan(&lastContent)
	if err == nil && lastContent.Valid && lastContent.String == c.Content {
		return nil // duplicate, skip
	}

	_, err = s.db.Exec(
		`INSERT INTO clips (content, image_path, mime_type, is_image, source_app)
		 VALUES (?, ?, ?, ?, ?)`,
		c.Content, c.ImagePath, c.MimeType, c.IsImage, c.SourceApp,
	)
	if err != nil {
		return err
	}

	return nil
}

// List returns recent clips (non-deleted).
func (s *Store) List(limit int) ([]Clip, error) {
	if limit <= 0 {
		limit = 50
	}
	rows, err := s.db.Query(
		`SELECT id, content, image_path, mime_type, is_image, is_fav, source_app, created_at
		 FROM clips ORDER BY created_at DESC LIMIT ?`, limit,
	)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var clips []Clip
	for rows.Next() {
		var c Clip
		if err := rows.Scan(&c.ID, &c.Content, &c.ImagePath, &c.MimeType,
			&c.IsImage, &c.IsFav, &c.SourceApp, &c.CreatedAt); err != nil {
			return nil, err
		}
		clips = append(clips, c)
	}
	return clips, rows.Err()
}

// Search finds clips matching the keyword, ordered by match quality.
func (s *Store) Search(keyword string, limit int) ([]Clip, error) {
	if limit <= 0 {
		limit = 50
	}
	kw := "%" + keyword + "%"
	rows, err := s.db.Query(
		`SELECT id, content, image_path, mime_type, is_image, is_fav, source_app, created_at
		 FROM clips WHERE content LIKE ?
		 ORDER BY
		   CASE
		     WHEN content = ?        THEN 0
		     WHEN content LIKE ? || '%' THEN 1
		     WHEN content LIKE '%' || ? THEN 2
		     ELSE 3
		   END,
		   length(content),
		   created_at DESC
		 LIMIT ?`,
		kw, keyword, keyword, keyword, limit,
	)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var clips []Clip
	for rows.Next() {
		var c Clip
		if err := rows.Scan(&c.ID, &c.Content, &c.ImagePath, &c.MimeType,
			&c.IsImage, &c.IsFav, &c.SourceApp, &c.CreatedAt); err != nil {
			return nil, err
		}
		clips = append(clips, c)
	}
	return clips, rows.Err()
}

// ToggleFav toggles the favorite status of a clip.
func (s *Store) ToggleFav(id int64) error {
	_, err := s.db.Exec(`UPDATE clips SET is_fav = NOT is_fav WHERE id = ?`, id)
	return err
}

// Delete removes a clip by ID.
func (s *Store) Delete(id int64) error {
	_, err := s.db.Exec(`DELETE FROM clips WHERE id = ?`, id)
	return err
}

// Clear removes all non-fav clips.
func (s *Store) Clear() error {
	_, err := s.db.Exec(`DELETE FROM clips WHERE is_fav = 0`)
	return err
}

// Count returns total number of clips.
func (s *Store) Count() (int, error) {
	var count int
	err := s.db.QueryRow("SELECT COUNT(*) FROM clips").Scan(&count)
	return count, err
}

// GetByID returns a single clip.
func (s *Store) GetByID(id int64) (*Clip, error) {
	var c Clip
	err := s.db.QueryRow(
		`SELECT id, content, image_path, mime_type, is_image, is_fav, source_app, created_at
		 FROM clips WHERE id = ?`, id,
	).Scan(&c.ID, &c.Content, &c.ImagePath, &c.MimeType,
		&c.IsImage, &c.IsFav, &c.SourceApp, &c.CreatedAt)
	if err != nil {
		return nil, err
	}
	return &c, nil
}
