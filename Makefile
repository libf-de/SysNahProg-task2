BINARY=main
OBJ=${BINARY}.o
CXXFLAGS=-std=c++20 -Wall -Wextra -fPIC -lexplain -g

all: $(BINARY)

run: $(BINARY)
	./$(BINARY)

$(BINARY): $(OBJ)
	$(CXX) $(CXXFLAGS) $< -o $(BINARY)

clean:
	$(RM) $(OBJ) $(BINARY)
