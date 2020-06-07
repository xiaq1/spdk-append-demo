main:
	g++ main.cpp layout.cpp -g -o main -std=c++11 -I../common/ -I/home/xiaqichao/Haha/base/common/ -I/home/xiaqichao/Haha/base/ullib/include/ -I/home/xiaqichao/third-64/gflags/include/ -I/home/xiaqichao/Haha/base/bthread -I/home/xiaqichao/Haha/base/bvar -I/home/xiaqichao/Haha/base/iobuf -I/home/xiaqichao/Haha/bce/cds
	#g++ main.cpp layout.cpp -o main -std=c++11 -I../common/ -I/home/xiaqichao/Haha/base/common/ -I/home/xiaqichao/Haha/base/ullib/include/ -I/home/xiaqichao/third-64/gflags/include/ -I/home/xiaqichao/Haha/base/bthread -I/home/xiaqichao/Haha/base/bvar -I/home/xiaqichao/Haha/base/iobuf -I/home/xiaqichao/Haha/bce/cds
clean:
	rm -rf main main.o
