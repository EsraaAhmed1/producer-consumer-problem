# Compiler and flags
CC = g++
CFLAGS = -g -o

# Targets
TARGETS = producer consumer

# Default target
all: $(TARGETS)

producer: producer.cpp
	$(CC) $(CFLAGS) producer producer.cpp  

consumer: consumer.cpp
	$(CC) $(CFLAGS) consumer consumer.cpp  

# Clean up executables
clean:
	rm -f $(TARGETS)

