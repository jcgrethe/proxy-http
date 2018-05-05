CC = gcc
CFLAGS = -c -Wall -g -Os
LD = $(CC)
LDFLAGS =
BINDIR = bin

TARGET = http-response

OBJECTS = $(patsubst %.c, %.o, $(shell find . -name "*.c"))

all: $(BINDIR)/$(TARGET)

$(BINDIR)/$(TARGET): $(OBJECTS)
	@$(LD) -o $@ $^ $(LDFLAGS)
	@echo "Linking complete!"

%.o: %.c
	@$(CC) $(CFLAGS) $^ -o $@
	@echo "Compiled "$<" successfully!"

.PHONY: clean
clean:
	@rm $(BINDIR)/$(TARGET) $(OBJECTS)
	@echo "Executable and objects removed!"