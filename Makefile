CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall \
            $(shell pkg-config --cflags gtk+-3.0 gtk-layer-shell-0 jsoncpp)
LDFLAGS  := $(shell pkg-config --libs   gtk+-3.0 gtk-layer-shell-0 jsoncpp)
TARGET   := wizer

all: $(TARGET)

$(TARGET): widget.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
