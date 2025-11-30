# ==========================================
# Reliable Transport Protocol Project
# Makefile for g++ compilation
# ==========================================

# 编译器和编译标志
CXX = g++
CPPFLAGS = -I./include
# Link against Winsock2 for MinGW 
# Use -Wl,--whole-archive to ensure all symbols are linked
LDFLAGS = -lpthread -Wl,--whole-archive D:\CPUsoft\mingw64\x86_64-w64-mingw32\lib\libws2_32.a -Wl,--no-whole-archive

# 调试模式（DEBUG=1）或发布模式（DEBUG=0）
DEBUG ?= 0

ifeq ($(DEBUG), 1)
    CXXFLAGS = -std=c++11 -Wall -Wextra -g -O0 -DDEBUG
    BUILD_MODE = Debug
else
    CXXFLAGS = -std=c++11 -Wall -Wextra -g -O2
    BUILD_MODE = Release
endif

# 目录设置
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
INCLUDE_DIR = include
TEST_DIR = tests
DATA_DIR = data

# 源文件和目标文件
COMMON_SOURCES = $(SRC_DIR)/connection.cpp \
                 $(SRC_DIR)/packet.cpp \
                 $(SRC_DIR)/checksum.cpp \
                 $(SRC_DIR)/window.cpp \
                 $(SRC_DIR)/congestion.cpp \
                 $(SRC_DIR)/utils.cpp

MAIN_SOURCE = $(SRC_DIR)/main.cpp

ALL_SOURCES = $(COMMON_SOURCES) $(MAIN_SOURCE)

COMMON_OBJECTS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(COMMON_SOURCES))
MAIN_OBJECT = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(MAIN_SOURCE))
ALL_OBJECTS = $(MAIN_OBJECT) $(COMMON_OBJECTS)

# 目标可执行文件
PROG = $(BIN_DIR)/reliable_transport
SERVER = $(BIN_DIR)/rtp_server
CLIENT = $(BIN_DIR)/rtp_client

# ==================== 编译规则 ====================

# 默认目标
all: $(PROG) | $(BIN_DIR) $(DATA_DIR)
	@echo "[Build Complete] Mode: $(BUILD_MODE), Executable: $(PROG)"

# 同时构建独立的server和client目标
both: $(SERVER) $(CLIENT) | $(BIN_DIR) $(DATA_DIR)
	@echo "[Build Complete] Mode: $(BUILD_MODE)"
	@echo "  Server: $(SERVER)"
	@echo "  Client: $(CLIENT)"

# 链接主程序
$(PROG): $(ALL_OBJECTS) | $(BIN_DIR)
	@echo "[Link] $@"
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $^

# 独立的server目标（使用相同的main程序）
$(SERVER): $(ALL_OBJECTS) | $(BIN_DIR)
	@echo "[Link] $@"
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $^

# 独立的client目标（使用相同的main程序）
$(CLIENT): $(ALL_OBJECTS) | $(BIN_DIR)
	@echo "[Link] $@"
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $^

# 编译源文件生成目标文件
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@echo "[Compile] $<"
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

# 创建必要的目录
$(OBJ_DIR):
	@if not exist $(OBJ_DIR) mkdir $(OBJ_DIR)

$(BIN_DIR):
	@if not exist $(BIN_DIR) mkdir $(BIN_DIR)

$(DATA_DIR):
	@if not exist $(DATA_DIR) mkdir $(DATA_DIR)

$(TEST_DIR):
	@if not exist $(TEST_DIR) mkdir $(TEST_DIR)

# ==================== 依赖关系 ====================

# 生成依赖文件
$(OBJ_DIR)/%.d: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@$(CXX) $(CPPFLAGS) -MM $< > $@

# 包含依赖文件
-include $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.d, $(ALL_SOURCES))

# ==================== 清理规则 ====================

# 清理所有编译生成的文件和测试数据
clean: clean_obj
	@if exist $(BIN_DIR) rmdir /s /q $(BIN_DIR)
	@if exist $(TEST_DIR) rmdir /s /q $(TEST_DIR)
	@if exist $(DATA_DIR) for /d %I in ($(DATA_DIR)\test_*) do rmdir /s /q %I
	@if exist *.log del /q *.log
	@echo [Clean] All generated files removed

# 清理目标文件和依赖文件
clean_obj:
	@if exist $(OBJ_DIR) rmdir /s /q $(OBJ_DIR)
	@echo [Clean] Object files removed

# 完全清理（包括测试数据）
distclean: clean
	@if exist $(DATA_DIR) rmdir /s /q $(DATA_DIR)
	@echo [DistClean] All files cleaned including test data

# ==================== 构建模式 ====================

# 调试构建
debug: DEBUG=1
debug: clean all
	@echo "[Debug Build] Enabled"

# 发布构建
release: DEBUG=0
release: clean all
	@echo "[Release Build] Enabled"

# ==================== 运行和测试 ====================

# 运行程序（显示帮助）
run: $(PROG)
	@./$(PROG) -h

# 运行服务器
run_server: $(PROG) | $(DATA_DIR)
	@echo "[Run] Server mode on port 8888"
	@mkdir -p $(DATA_DIR)
	./$(PROG) -s -p 8888 -out $(DATA_DIR)/received.dat

# 运行客户端
run_client: $(PROG) | $(DATA_DIR)
	@echo "[Run] Client mode connecting to localhost"
	@mkdir -p $(DATA_DIR)
	@if [ ! -f $(DATA_DIR)/test_input.dat ]; then \
		dd if=/dev/zero of=$(DATA_DIR)/test_input.dat bs=1K count=100 2>/dev/null; \
		echo "[Create] Test file: $(DATA_DIR)/test_input.dat (100KB)"; \
	fi
	./$(PROG) -c -i 127.0.0.1 -p 8888 -in $(DATA_DIR)/test_input.dat

# 运行基本测试
test: $(PROG) | $(DATA_DIR) $(TEST_DIR)
	@echo "[Test] Running basic functionality tests..."
	@bash ./tests/test.sh

# 运行性能测试
perf_test: $(PROG) | $(DATA_DIR) $(TEST_DIR)
	@echo "[Test] Running performance tests..."
	@bash ./tests/performance_test.sh

# ==================== 显示信息 ====================

info:
	@echo "========== Build Configuration =========="
	@echo "Compiler:       $(CXX)"
	@echo "C++ Standard:   C++11"
	@echo "Build Mode:     $(BUILD_MODE)"
	@echo "CXXFLAGS:       $(CXXFLAGS)"
	@echo "CPPFLAGS:       $(CPPFLAGS)"
	@echo ""
	@echo "Sources:        $(ALL_SOURCES)"
	@echo "Objects:        $(ALL_OBJECTS)"
	@echo "Main Target:    $(PROG)"
	@echo "Server Target:  $(SERVER)"
	@echo "Client Target:  $(CLIENT)"
	@echo "=========================================="

# 显示帮助信息
help:
	@echo "========== Reliable Transport Protocol Makefile =========="
	@echo ""
	@echo "Build Targets:"
	@echo "  all          - Build the main program (default)"
	@echo "  both         - Build server and client aliases"
	@echo "  debug        - Build with debug symbols (-g -O0)"
	@echo "  release      - Build optimized release (-O2)"
	@echo ""
	@echo "Clean Targets:"
	@echo "  clean        - Remove all compiled files and test data"
	@echo "  clean_obj    - Remove only object files"
	@echo "  distclean    - Complete cleanup including data directory"
	@echo ""
	@echo "Run Targets:"
	@echo "  run          - Show program help"
	@echo "  run_server   - Run as server (port 8888)"
	@echo "  run_client   - Run as client (requires test file)"
	@echo ""
	@echo "Test Targets:"
	@echo "  test         - Run basic functionality tests"
	@echo "  perf_test    - Run performance benchmark tests"
	@echo ""
	@echo "Info Targets:"
	@echo "  info         - Show build configuration"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Example Usage:"
	@echo "  make                  # Build in release mode"
	@echo "  make DEBUG=1 all      # Build with debug symbols"
	@echo "  make clean all        # Clean and rebuild"
	@echo "  make test             # Run tests"
	@echo "  make perf_test        # Run performance tests"
	@echo ""
	@echo "=========================================================="

# ==================== 伪目标 ====================

.PHONY: all both debug release clean clean_obj distclean
.PHONY: run run_server run_client test perf_test info help

# ==================== 编译选项说明 ====================

# -std=c++11    : 使用C++11标准
# -Wall         : 显示所有警告信息
# -Wextra       : 显示额外的警告信息
# -g            : 生成调试符号
# -O0           : 无优化（调试模式）
# -O2           : 优化级别2（发布模式）
# -DDEBUG       : 定义DEBUG宏（调试模式）
# -I./include   : 包含目录路径
# -lpthread     : 链接pthread库
