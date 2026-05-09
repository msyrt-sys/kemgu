CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -g -O0
SRCDIR = src
TESTDIR = test
BUILD = build

# Windows'ta GCC ciktiya otomatik .exe ekler — hedef adlarinda da uyumlu olmali
ifeq ($(OS),Windows_NT)
    EXE := .exe
else
    EXE :=
endif

SRCS = $(SRCDIR)/utf8.c $(SRCDIR)/anahtar_kelime.c $(SRCDIR)/hata.c $(SRCDIR)/lexer.c
OBJS = $(patsubst $(SRCDIR)/%.c,$(BUILD)/%.o,$(SRCS))

.PHONY: all clean test calistir_lexer_test test_tumu

all: $(BUILD)/kemgu$(EXE)

$(BUILD)/kemgu$(EXE): $(OBJS) $(BUILD)/ana.o
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD)/test_lexer$(EXE): $(OBJS) $(BUILD)/test_lexer.o
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD)/%.o: $(SRCDIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/test_lexer.o: $(TESTDIR)/test_lexer.c | $(BUILD)
	$(CC) $(CFLAGS) -I$(SRCDIR) -c -o $@ $<

$(BUILD):
	mkdir -p $(BUILD)

test: $(BUILD)/test_lexer$(EXE)
	./$(BUILD)/test_lexer$(EXE)

calistir_lexer_test: test

test_tumu: calistir_lexer_test
	@echo "Tum testler gecti!"

clean:
	rm -rf $(BUILD)
