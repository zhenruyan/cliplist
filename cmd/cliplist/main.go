package main

import (
	"encoding/json"
	"fmt"
	"os"
	"strconv"
	"strings"
	"text/tabwriter"

	"github.com/user/cliplist/internal/ipc"
)

func main() {
	if len(os.Args) < 2 {
		printUsage()
		os.Exit(1)
	}

	cmd := os.Args[1]
	args := os.Args[2:]

	var req ipc.Request

	switch cmd {
	case "list", "ls":
		req = ipc.Request{Action: "list", Limit: 20}
		if len(args) > 0 {
			if n, err := strconv.Atoi(args[0]); err == nil {
				req.Limit = n
			}
		}

	case "search", "s":
		if len(args) == 0 {
			fatal("usage: cliplist search <keyword>")
		}
		req = ipc.Request{Action: "search", Query: strings.Join(args, " "), Limit: 20}

	case "copy", "c":
		if len(args) == 0 {
			fatal("usage: cliplist copy <id>")
		}
		id, err := strconv.ParseInt(args[0], 10, 64)
		if err != nil {
			fatal("invalid id: %s", args[0])
		}
		req = ipc.Request{Action: "copy", ID: id}

	case "fav", "f":
		if len(args) == 0 {
			fatal("usage: cliplist fav <id>")
		}
		id, err := strconv.ParseInt(args[0], 10, 64)
		if err != nil {
			fatal("invalid id: %s", args[0])
		}
		req = ipc.Request{Action: "fav", ID: id}

	case "delete", "del", "rm":
		if len(args) == 0 {
			fatal("usage: cliplist delete <id>")
		}
		id, err := strconv.ParseInt(args[0], 10, 64)
		if err != nil {
			fatal("invalid id: %s", args[0])
		}
		req = ipc.Request{Action: "delete", ID: id}

	case "pop", "p":
		req = ipc.Request{Action: "pop"}

	case "settings":
		req = ipc.Request{Action: "settings"}

	case "manager", "m":
		req = ipc.Request{Action: "manager"}

	case "clear":
		req = ipc.Request{Action: "clear"}

	case "help", "-h", "--help":
		printUsage()
		return

	default:
		fmt.Fprintf(os.Stderr, "unknown command: %s\n", cmd)
		printUsage()
		os.Exit(1)
	}

	resp, err := ipc.SendRequest(req)
	if err != nil {
		fatal("error: %v", err)
	}

	if !resp.OK {
		fatal("error: %s", resp.Error)
	}

	if resp.Data != nil {
		printClips(resp.Data)
	} else {
		fmt.Println("OK")
	}
}

func printClips(data interface{}) {
	// Data comes as []map[string]interface{} from JSON
	b, err := json.Marshal(data)
	if err != nil {
		fmt.Println(data)
		return
	}

	var clips []struct {
		ID        int64  `json:"ID"`
		Content   string `json:"Content"`
		MimeType  string `json:"MimeType"`
		IsFav     bool   `json:"IsFav"`
		SourceApp string `json:"SourceApp"`
		CreatedAt string `json:"CreatedAt"`
	}
	if err := json.Unmarshal(b, &clips); err != nil {
		fmt.Println(string(b))
		return
	}

	if len(clips) == 0 {
		fmt.Println("(no clips)")
		return
	}

	w := tabwriter.NewWriter(os.Stdout, 0, 0, 2, ' ', 0)
	fmt.Fprintf(w, "ID\tFAV\tCONTENT\tSOURCE\n")
	for _, c := range clips {
		content := strings.ReplaceAll(c.Content, "\n", " ")
		if len(content) > 60 {
			content = content[:57] + "..."
		}
		fav := " "
		if c.IsFav {
			fav = "★"
		}
		fmt.Fprintf(w, "%d\t%s\t%s\t%s\n", c.ID, fav, content, c.SourceApp)
	}
	w.Flush()
}

func printUsage() {
	fmt.Println(`cliplist - clipboard history manager

Usage:
  cliplist pop            Toggle popup window
  cliplist manager        Open clipboard manager window
  cliplist settings       Open settings panel
  cliplist list [n]       Show last n clips (default 20)
  cliplist search <q>    Search clips by keyword
  cliplist copy <id>     Copy clip to clipboard
  cliplist fav <id>      Toggle favorite
  cliplist delete <id>   Delete a clip
  cliplist clear         Clear all non-favorite clips
  cliplist help          Show this help

Aliases: p, ls, s, c, f, rm`)
}

func fatal(format string, args ...interface{}) {
	fmt.Fprintf(os.Stderr, format+"\n", args...)
	os.Exit(1)
}
