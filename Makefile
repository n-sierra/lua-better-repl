CFLAGS = -gstabs -O0
OBJS = repl.o
TARGET = repl

$(TARGET): $(OBJS)
	gcc -lreadline -llua -o $@ $(OBJS)

c.o:
	gcc -c $<

clean:
	rm $(OBJS) $(TARGET)
