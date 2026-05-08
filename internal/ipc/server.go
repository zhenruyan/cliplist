package ipc

import (
	"encoding/json"
	"fmt"
	"log"
	"net"
	"os"
	"path/filepath"
)

const sockName = "cliplist.sock"

// Request is a CLI -> daemon command.
type Request struct {
	Action string `json:"action"` // list, search, clear, fav, delete, quit
	ID     int64  `json:"id,omitempty"`
	Query  string `json:"query,omitempty"`
	Limit  int    `json:"limit,omitempty"`
}

// Response is the daemon -> CLI reply.
type Response struct {
	OK    bool        `json:"ok"`
	Error string      `json:"error,omitempty"`
	Data  interface{} `json:"data,omitempty"`
}

// Handler processes IPC requests.
type Handler func(req Request) Response

// SocketPath returns the unix socket path.
func SocketPath() string {
	runtime := os.Getenv("XDG_RUNTIME_DIR")
	if runtime == "" {
		runtime = filepath.Join(os.TempDir(), fmt.Sprintf("cliplist-%d", os.Getuid()))
		os.MkdirAll(runtime, 0700)
	}
	return filepath.Join(runtime, sockName)
}

// Server listens on a unix socket and dispatches requests to the handler.
type Server struct {
	path    string
	handler Handler
	ln      net.Listener
}

func NewServer(handler Handler) *Server {
	return &Server{
		path:    SocketPath(),
		handler: handler,
	}
}

func (s *Server) Start() error {
	os.Remove(s.path)
	ln, err := net.Listen("unix", s.path)
	if err != nil {
		return fmt.Errorf("listen %s: %w", s.path, err)
	}
	s.ln = ln
	os.Chmod(s.path, 0600)

	go s.serve()
	log.Printf("[ipc] listening on %s", s.path)
	return nil
}

func (s *Server) Stop() {
	if s.ln != nil {
		s.ln.Close()
		os.Remove(s.path)
	}
}

func (s *Server) serve() {
	for {
		conn, err := s.ln.Accept()
		if err != nil {
			// listener closed
			return
		}
		go s.handleConn(conn)
	}
}

func (s *Server) handleConn(conn net.Conn) {
	defer conn.Close()

	var req Request
	decoder := json.NewDecoder(conn)
	if err := decoder.Decode(&req); err != nil {
		writeResponse(conn, Response{OK: false, Error: err.Error()})
		return
	}

	resp := s.handler(req)
	writeResponse(conn, resp)
}

func writeResponse(conn net.Conn, resp Response) {
	encoder := json.NewEncoder(conn)
	encoder.Encode(resp)
}

// Client sends a request to the daemon.
func SendRequest(req Request) (*Response, error) {
	conn, err := net.Dial("unix", SocketPath())
	if err != nil {
		return nil, fmt.Errorf("connect: %w (is cliplistd running?)", err)
	}
	defer conn.Close()

	if err := json.NewEncoder(conn).Encode(req); err != nil {
		return nil, fmt.Errorf("send: %w", err)
	}

	var resp Response
	if err := json.NewDecoder(conn).Decode(&resp); err != nil {
		return nil, fmt.Errorf("recv: %w", err)
	}
	return &resp, nil
}
