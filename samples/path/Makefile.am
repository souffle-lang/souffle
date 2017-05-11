SOUFFLE_INC=/home/david/souffle/include
CXX=g++
CXXFLAGS= -I $(SOUFFLE_INC) -D __EMBEDDED_SOUFFLE__ -Wall -std=c++11   -O3 -DUSE_LIBZ  -DUSE_SQLITE  -fopenmp -g
LIBS= -lpthread -lsqlite3 -lz 
FILES=driver1.cpp path.cpp

p1:
	$(CXX) $(CXXFLAGS) $(FILES) $(LIBS)	
