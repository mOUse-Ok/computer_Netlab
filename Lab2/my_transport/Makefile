# 可靠传输协议项目 Makefile

# 编译器设置
CXX = g++
CXXFLAGS = -std=c++11 -Wall -g
LDFLAGS = -lpthread

# 项目名称
TARGET = reliable_transport

# 源文件和对象文件
SRCS = main.cpp connection.cpp packet.cpp checksum.cpp window.cpp congestion.cpp utils.cpp
OBJS = $(SRCS:.cpp=.o)

# 头文件列表
HEADERS = reliable_transport.h connection.h packet.h checksum.h window.h congestion.h utils.h

# 默认目标
all: $(TARGET)

# 生成可执行文件
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# 编译规则
%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 清理目标
clean:
	rm -f $(OBJS) $(TARGET)

# 重新构建
rebuild: clean all

.PHONY: all clean rebuild