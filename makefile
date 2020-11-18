CXX = g++
CXXFLAGS = -std=c++17 -g

main: main.o httpmessage.o view.o 
	$(CXX) $(CXXFLAGS) -o main main.o view.o httpmessage.o

main.o: httpmessage.h view.h

httpmessage.o: httpmessage.h

view.o: view.h
