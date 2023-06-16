CXX = g++
CXXFLAGS = -std=c++11 -Wall
NANAPATH = /home/rockstar/workspace/webserver/nana
NANAINC = $(NANAPATH)/include
NANALIB = $(NANAPATH)/build/bin

INCS = -I$(NANAINC)
LIBS = -L$(NANALIB) -lnana -lX11 -lrt -lXft -lfontconfig

LIBS += -lstdc++fs -lpthread -lmysqlclient

SRCS = main.cpp config/config.cpp utils/log.cpp utils/utils.cpp http/http_conn.cpp CGImysql/sql_connection_pool.cpp timer/timer.cpp webserver.cpp
OBJS = $(SRCS:.cpp=.o)

TARGET = server

.PHONY: all clean

webserver : $(TARGET)

$(TARGET): $(OBJS) $(NANALIB)/libnana.a
	$(CXX) $(CXXFLAGS) $(OBJS) $(INCS) $(LIBS) -o $@

%.o: %.cpp 
	$(CXX) $(CXXFLAGS) -g -c $< -o $@ $(INCS) 

clean:
	rm -f $(OBJS) $(TARGET)
