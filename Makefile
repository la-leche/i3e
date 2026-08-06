.RECIPEPREFIX := >

CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

SOURCES := main.cpp engine.cpp WindowTree.cpp
TARGET := i3trade

ifeq ($(OS),Windows_NT)
TARGET := i3trade.exe
endif

all:
>$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

clean:
>rm -f i3trade i3trade.exe
