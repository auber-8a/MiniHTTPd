CC = gcc
CFLAGS = -Wall -Wextra -Iinclude
TARGET = minihttpd
SRCS = src/main.c src/server.c src/http.c src/files.c src/mime.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run