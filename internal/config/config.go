package config

import (
	"os"
	"path/filepath"
	"time"

	"github.com/BurntSushi/toml"
)

type Config struct {
	MaxHistory      int           `toml:"max_history"`
	PollInterval    time.Duration `toml:"-"`
	PollIntervalStr string        `toml:"poll_interval"`
	DBPath          string        `toml:"db_path"`
	IconPath        string        `toml:"icon_path"`
	ImageDir        string        `toml:"image_dir"`
	PasteDelay      string        `toml:"paste_delay"`
	PasteDelayDur   time.Duration `toml:"-"`
	Hotkey          HotkeyConfig  `toml:"hotkey"`
	AutoStart       bool          `toml:"auto_start"`
	IgnoreWS        bool          `toml:"ignore_whitespace"`
	MaxLength       int           `toml:"max_content_length"`
}

type HotkeyConfig struct {
	Modifiers []string `toml:"modifiers"`
	Key       string   `toml:"key"`
}

func DefaultConfigDir() string {
	home, _ := os.UserHomeDir()
	return filepath.Join(home, ".config", "cliplist")
}

func DefaultConfig() *Config {
	dataHome := os.Getenv("XDG_DATA_HOME")
	if dataHome == "" {
		home, _ := os.UserHomeDir()
		dataHome = filepath.Join(home, ".local", "share")
	}
	return &Config{
		MaxHistory:      100,
		PollIntervalStr: "500ms",
		PollInterval:    500 * time.Millisecond,
		DBPath:          filepath.Join(DefaultConfigDir(), "cliplist.db"),
		IconPath:        "",
		ImageDir:        filepath.Join(dataHome, "cliplist", "images"),
		PasteDelay:      "80ms",
		PasteDelayDur:   80 * time.Millisecond,
		Hotkey: HotkeyConfig{
			Modifiers: []string{},
			Key:       "",
		},
		AutoStart: true,
		IgnoreWS:  true,
		MaxLength: 1024 * 1024, // 1MB
	}
}

func Load(path string) (*Config, error) {
	cfg := DefaultConfig()
	if path == "" {
		path = filepath.Join(DefaultConfigDir(), "config.toml")
	}
	if _, err := os.Stat(path); os.IsNotExist(err) {
		return cfg, nil
	}
	if _, err := toml.DecodeFile(path, cfg); err != nil {
		return nil, err
	}
	// parse durations
	if cfg.PollIntervalStr != "" {
		d, err := time.ParseDuration(cfg.PollIntervalStr)
		if err == nil {
			cfg.PollInterval = d
		}
	}
	if cfg.PasteDelay != "" {
		d, err := time.ParseDuration(cfg.PasteDelay)
		if err == nil {
			cfg.PasteDelayDur = d
		}
	}
	return cfg, nil
}

func (c *Config) Save(path string) error {
	if path == "" {
		path = filepath.Join(DefaultConfigDir(), "config.toml")
	}
	dir := filepath.Dir(path)
	if err := os.MkdirAll(dir, 0755); err != nil {
		return err
	}
	f, err := os.Create(path)
	if err != nil {
		return err
	}
	defer f.Close()
	return toml.NewEncoder(f).Encode(c)
}
