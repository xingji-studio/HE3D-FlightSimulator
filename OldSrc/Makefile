CXX = "g++"
CXXFLAGS = -fdiagnostics-color=always -g -std=c++11
LIBS = -lSDL2

# 设置编码的环境变量在 Makefile 命令行中需要加 && 连接
ENV = set LANG=zh_CN.GBK && set LC_ALL=zh_CN.GBK

all: main.exe

main.exe: Engine.o main.o
	$(ENV) && $(CXX) Engine.o main.o $(CXXFLAGS) -o main.exe $(LIBS)

# main.o 应该依赖它自己的源文件和相关的头文件
main.o: main.cpp Engine.h Math.h
	$(ENV) && $(CXX) $(CXXFLAGS) -c main.cpp -o main.o

Engine.o: Engine.cpp Engine.h Math.h
	$(ENV) && $(CXX) $(CXXFLAGS) -c Engine.cpp -o Engine.o

clean:
	rm -f *.o *.exe

rebuild: clean main.exe