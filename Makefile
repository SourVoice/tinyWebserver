CXX = g++
CXXFLAGS = -std=c++11 -Wall -O2
LDFLAGS = -lpthread -lmysqlclient

SRCS = main.cpp config/config.cpp utils/log.cpp utils/utils.cpp http/http_conn.cpp CGImysql/sql_connection_pool.cpp timer/timer.cpp webserver.cpp
OBJS = $(SRCS:.cpp=.o)

TARGET = server

.PHONY: all clean

webserver : $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp 
	$(CXX) $(CXXFLAGS) -c -g -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
