CC      = mpicc
CFLAGS  = -O2 -Wall
SRCDIR  = src
TARGET  = stats

SOURCES = $(SRCDIR)/main.c \
          $(SRCDIR)/utils.c \
          $(SRCDIR)/parse_args.c \
          $(SRCDIR)/frequency.c

HEADERS = $(SRCDIR)/config.h \
          $(SRCDIR)/utils.h \
          $(SRCDIR)/parse_args.h \
          $(SRCDIR)/frequency.h

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -I$(SRCDIR) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET)

.PHONY: all clean