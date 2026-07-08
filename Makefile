CXX = g++
CXXFLAGS = -O3 -std=c++17
LIBS = -lncursesw

TARGET = i3trade

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) main.cpp -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET) ticks.csv