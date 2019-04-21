# Online Judge 的判题程序

## 简介

OJ的判题程序，由c++编写，程序流程图：

![oj-judge](https://ws1.sinaimg.cn/large/006tNc79ly1g2aktcvl39j30sn1cdth9.jpg)

### C++的编译命令：

```bash
g++ main.cpp -o main -lm -DONLINE_JUDGE -w -O2 -fmax-errors=3 -std=c++14 -static
```

其中-fmax-errors=3是为了防止编译阶段产生过多的错误信息导致程序输出过多

-O2是优化选项，可以使程序在运行阶段运行更快，并且去掉一些未使用的代码，比如一个初始化过的数组变量，但是从未使用过，使用此编译选项，运行时这个数组就像不存在一样。

-DONLINE_JUDGE登陆`#define ONLINE_JUDGE`用户可以根据这个宏判断环境

-w会忽略编译过程中所有的Warning信息，只输出Error 信息

### C的编译命令：

````bash
gcc main.c -o main -lm -DONLINE_JUDGE -w -O2 -fmax-errors=3 -std=c11 -static
````

跟c++的几乎是一样的

### 编译过程

编译过程中使用`setrlimit`限制编译使用的内存和CPU时间，并且使用`alarm`来限制实际时间，防止用户提交包含

```c++
#include "/dev/random"
```

这种头文件的代码导致编译卡死

### 运行过程

运行时通过`freopen`将标准输入重定向到输入文件中，将标准输出重定向到输出文件中。

运行过程同样适用setrlimit限制内存占用、CPU时间、堆栈大小、输出大小、进程数量。但是`setrlimit`只是做一个粗限制，也就是说它的设定值会比题目设定值稍大一些，因为如果设定为跟题目设定值一样，那么一旦超过了限制，程序会被KILL掉，不好判断出是什么超出了限制，所以这里只是做一个粗限制，保证程序不会出错。

同时使用`ptrace`监听运行进程的每一个系统调用，并且在每次捕捉到系统调用后都会获取进程当前使用的各种资源并比对题目的限制，在这里可以判断出是什么东西超出了限制，同时判断系统调用是否合法（预先定义好了可以使用的系统调用）

## 待完善

1. 目前不支持special judge的题目

