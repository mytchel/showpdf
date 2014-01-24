LDLIBS = `pkg-config --libs gtk+-2.0 poppler-glib`
CFLAGS = -Wall -std=c99 -g -O2 -pipe `pkg-config --cflags gtk+-2.0 poppler-glib`
BIN = showpdf
DIR = /usr/local/bin

all: $(BIN)

install: $(BIN)
	cp $(BIN) $(DIR)

deinstall:
	rm $(DIR)/$(BIN)

clean:
	rm $(BIN)
