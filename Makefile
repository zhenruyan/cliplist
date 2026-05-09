BINARY_NAME=cliplist
DAEMON_NAME=cliplistd
PREFIX=/usr/local
BINDIR=$(PREFIX)/bin

# Go parameters
GO=go
GOFLAGS=-v
CGO_ENABLED=1

.PHONY: all build clean install uninstall run test fmt vet

all: build

build: build-daemon build-cli

build-daemon:
	CGO_ENABLED=$(CGO_ENABLED) $(GO) build $(GOFLAGS) -o $(DAEMON_NAME) ./cmd/cliplistd

build-cli:
	$(GO) build $(GOFLAGS) -o $(BINARY_NAME) ./cmd/cliplist

clean:
	rm -f $(BINARY_NAME) $(DAEMON_NAME)
	$(GO) clean

install: build
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(DAEMON_NAME) $(DESTDIR)$(BINDIR)/$(DAEMON_NAME)
	install -m 755 $(BINARY_NAME) $(DESTDIR)$(BINDIR)/$(BINARY_NAME)
	install -d $(DESTDIR)/usr/share/icons/hicolor/scalable/apps
	install -m 644 assets/icons/cliplist.svg $(DESTDIR)/usr/share/icons/hicolor/scalable/apps/cliplist.svg
	install -d $(DESTDIR)/etc/xdg/autostart
	install -m 644 assets/cliplist.desktop $(DESTDIR)/etc/xdg/autostart/cliplist.desktop

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(DAEMON_NAME)
	rm -f $(DESTDIR)$(BINDIR)/$(BINARY_NAME)
	rm -f $(DESTDIR)/usr/share/icons/hicolor/scalable/apps/cliplist.svg
	rm -f $(DESTDIR)/etc/xdg/autostart/cliplist.desktop

run-daemon: build-daemon
	./$(DAEMON_NAME)

run-cli: build-cli
	./$(BINARY_NAME) list

test:
	$(GO) test ./...

fmt:
	$(GO) fmt ./...

deb:
	./scripts/build-deb.sh

vet:
	$(GO) vet ./...
