CC = gcc
CFLAGS = -Wall -Wextra -O3
LDFLAGS = -lSDL2 -lm

TARGET = fourier-ocean-physics
SRCS = fourier-ocean-physics.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)