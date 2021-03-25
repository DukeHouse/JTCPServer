# CC:gcc
CC = g++
OPTION_1 = -std=c++11
OPTION_2 = -lpthread

# CC = clang++
# OPTION_1 = -std=c++11
# OPTION_2 = -stdlib=libstdc++
# OPTION_3 = -stdlib=libc++
TARGET = server
all:
	$(CC) $(OPTION_1) selectTCPServer.hpp INetEvent.hpp CellServer.hpp goodserver.cpp Client.hpp $(OPTION_2) 
	mv a.out $(TARGET)
	rm *.gch 
clean:
	#rm *.gch *.out
	#rm -rf $(BUILD_DIR)
	rm $(TARGET)
run:
	./$(TARGET)
.PHONY:clean
