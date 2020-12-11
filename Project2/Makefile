CC = gcc

CFLAGS  = -pthread -g -Wall

TARGET = proxy

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c

clean:
	$(RM) $(TARGET)