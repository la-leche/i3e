CXX := g++
CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow
LDLIBS := -lncurses
TARGET := dist/i3trade
SOURCES := main.cpp engine.cpp WindowTree.cpp

$(TARGET): $(SOURCES) engine.hpp WindowTree.hpp
	$(CXX) $(CXXFLAGS) $(SOURCES) $(LDLIBS) -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: run clea
