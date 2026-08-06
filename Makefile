.RECIPEPREFIX := >

CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

SOURCES = main.cpp engine.cpp WindowTree.cpp
TARGET = i3trade
LDLIBS = -lncurses

ifeq ($(OS),Windows_NT)
TARGET = i3trade.exe
CXXFLAGS += -DNCURSES_STATIC -I/mingw64/include/ncursesw
LDLIBS = -lncursesw
endif

all:
>$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET) $(LDLIBS)

clean:
>rm -f i3trade i3trade.exe
