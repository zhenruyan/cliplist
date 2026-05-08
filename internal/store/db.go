package store

import (
	"database/sql"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"time"

	_ "github.com/mattn/go-sqlite3"
)

type Clip struct {
	ID        int64
	Content   string
	ImagePath string
	MimeType  string
	IsImage   bool
	IsFav     bool
	SourceApp string
	Tags      string
	CreatedAt time.Time
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
		tags       TEXT DEFAULT '',
		created_at DATETIME DEFAULT CURRENT_TIMESTAMP
	);
	CREATE INDEX IF NOT EXISTS idx_clips_created ON clips(created_at DESC);
	`
	_, err := s.db.Exec(schema)
	if err != nil {
		return err
	}
	// add tags column for existing databases
	s.db.Exec(`ALTER TABLE clips ADD COLUMN tags TEXT DEFAULT ''`)
	return nil
}

const selectCols = `id, content, image_path, mime_type, is_image, is_fav, source_app, tags, created_at`

func scanClip(sc interface{ Scan(...interface{}) error }) (Clip, error) {
	var c Clip
	err := sc.Scan(&c.ID, &c.Content, &c.ImagePath, &c.MimeType,
		&c.IsImage, &c.IsFav, &c.SourceApp, &c.Tags, &c.CreatedAt)
	return c, err
}

// Add inserts a new clip (skipping duplicates of the most recent entry).
func (s *Store) Add(c *Clip) error {
	var lastContent sql.NullString
	err := s.db.QueryRow(
		"SELECT content FROM clips ORDER BY created_at DESC LIMIT 1",
	).Scan(&lastContent)
	if err == nil && lastContent.Valid && lastContent.String == c.Content {
		return nil
	}

	_, err = s.db.Exec(
		`INSERT INTO clips (content, image_path, mime_type, is_image, source_app)
		 VALUES (?, ?, ?, ?, ?)`,
		c.Content, c.ImagePath, c.MimeType, c.IsImage, c.SourceApp,
	)
	return err
}

// List returns recent clips deduplicated by content (keeps latest).
func (s *Store) List(limit int) ([]Clip, error) {
	if limit <= 0 {
		limit = 50
	}
	rows, err := s.db.Query(
		`SELECT `+selectCols+` FROM clips o
		 INNER JOIN (SELECT MAX(id) id FROM clips GROUP BY content) m ON o.id = m.id
		 ORDER BY o.created_at DESC LIMIT ?`, limit,
	)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var clips []Clip
	for rows.Next() {
		c, err := scanClip(rows)
		if err != nil {
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
		`SELECT `+selectCols+` FROM clips o
		 INNER JOIN (SELECT MAX(id) id FROM clips GROUP BY content) m ON o.id = m.id
		 WHERE o.content LIKE ?
		 ORDER BY
		   CASE
		     WHEN o.content = ?          THEN 0
		     WHEN o.content LIKE ? || '%' THEN 1
		     WHEN o.content LIKE '%' || ? THEN 2
		     ELSE 3
		   END,
		   length(o.content),
		   o.created_at DESC
		 LIMIT ?`,
		kw, keyword, keyword, keyword, limit,
	)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var clips []Clip
	for rows.Next() {
		c, err := scanClip(rows)
		if err != nil {
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

// ListFiltered returns clips with optional filters.
func (s *Store) ListFiltered(limit int, favOnly bool, isImage *bool) ([]Clip, error) {
	q := `SELECT ` + selectCols + ` FROM clips o
	      INNER JOIN (SELECT MAX(id) id FROM clips GROUP BY content) m ON o.id = m.id
	      WHERE 1=1`
	var args []interface{}
	if favOnly {
		q += ` AND o.is_fav = 1`
	}
	if isImage != nil {
		if *isImage {
			q += ` AND o.is_image = 1`
		} else {
			q += ` AND o.is_image = 0`
		}
	}
	q += ` ORDER BY o.created_at DESC`
	if limit > 0 {
		q += ` LIMIT ?`
		args = append(args, limit)
	}
	rows, err := s.db.Query(q, args...)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var clips []Clip
	for rows.Next() {
		c, err := scanClip(rows)
		if err != nil {
			return nil, err
		}
		clips = append(clips, c)
	}
	return clips, rows.Err()
}

// SetTags sets the tags for a clip.
func (s *Store) SetTags(id int64, tags string) error {
	_, err := s.db.Exec(`UPDATE clips SET tags = ? WHERE id = ?`, tags, id)
	return err
}

// AddTag appends a tag to a clip if not already present.
func (s *Store) AddTag(id int64, tag string) error {
	clip, err := s.GetByID(id)
	if err != nil {
		return err
	}
	tags := clip.Tags
	if tags == "" {
		tags = tag
	} else {
		parts := strings.Split(tags, ",")
		for _, p := range parts {
			if p == tag {
				return nil // already has it
			}
		}
		tags = tags + "," + tag
	}
	return s.SetTags(id, tags)
}

// RemoveTag removes a tag from a clip.
func (s *Store) RemoveTag(id int64, tag string) error {
	clip, err := s.GetByID(id)
	if err != nil {
		return err
	}
	parts := strings.Split(clip.Tags, ",")
	var filtered []string
	for _, p := range parts {
		if p != tag && p != "" {
			filtered = append(filtered, p)
		}
	}
	return s.SetTags(id, strings.Join(filtered, ","))
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
	row := s.db.QueryRow(`SELECT `+selectCols+` FROM clips WHERE id = ?`, id)
	c, err := scanClip(row)
	if err != nil {
		return nil, err
	}
	return &c, nil
}
