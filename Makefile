.RECIPEPREFIX := >

CXX ?= g++
CPPFLAGS ?=
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
LDFLAGS ?=
LDLIBS ?=


SOURCES := \
    main.cpp \
    core/config.cpp \
    core/engine.cpp \
    core/WindowTree.cpp \
    charts/ohlc/ohlc.cpp \
    charts/delta/delta.cpp \
    charts/tpo/tpo.cpp

TARGET := i3trade

ifeq ($(OS),Windows_NT)
TARGET := i3trade.exe
CPPFLAGS += -I/mingw64/include/ncursesw
LDLIBS += -lncursesw
else
LDLIBS += -lncurses
endif

all: $(TARGET)

$(TARGET): $(SOURCES) \
           core/engine.hpp \
           core/config.hpp \
           core/WindowTree.hpp \
           charts/ohlc/ohlc.hpp \
           charts/delta/delta.hpp \
           charts/tpo/tpo.hpp
>$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCES) $(LDFLAGS) -o $@ $(LDLIBS)

clean:
>rm -f i3trade i3trade.exe

.PHONY: all clean