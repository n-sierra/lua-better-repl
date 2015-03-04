CC = gcc
CFLAGS = -MMD -MP
LDFLAGS = -lreadline -llua5.1

SRCDIR  = .
OBJDIR  = ./obj
BINDIR  = ./bin

TARGET  = $(BINDIR)/lua-better-repl
FILES   = repl.c
SOURCES = $(addprefix $(SRCDIR)/, $(FILES))
OBJECTS = $(addprefix $(OBJDIR)/, $(FILES:.c=.o))
DEPENDS = $(OBJECTS:.o=.d)

LIBS    =
INCLUDE = -I/usr/include/lua5.1

$(TARGET): $(OBJECTS) $(LIBS) $(BINDIR)
	$(CC) -o $@ $(filter %.o, $^) $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ -c $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

all:
	clean $(TARGET)

clean:
	rm -f $(OBJECTS) $(DEPENDS) $(TARGET)
	-rmdir --ignore-fail-on-non-empty $(OBJDIR) $(BINDIR)

-include $(DEPENDS)
