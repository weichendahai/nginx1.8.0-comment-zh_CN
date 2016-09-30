一、函数指针
首先它是一个指针，只是这个指针指向的是一个函数。指针变量可以指向变量的地址、数组、字符串、动态分配地址，同时也可指向一个函数，每个函数在编译的时候，系统会分配给该函数一个入口地址，函数名表示这个入口地址，那么指向函数的指针变量称为函数指针变量。
表示：
struct file_operations {
int (*seek) (struct inode * ，struct file *， off_t ，int);
int (*read) (struct inode * ，struct file *， char ，int);
}
上面这个结构体file_operations里面的组件都是函数指针：int (*read) (struct inode * ，struct file *， char ，int);注意int (*read)是加上括号的。

二、指针函数
首先它是一个函数，只不过这个函数的返回值是一个地址值。函数返回值必须用同类型的指针变量来接受，
也就是说，指针函数一定有函数返回值，而且，在主调函数中，函数返回值必须赋给同类型的指针变量。
格式：
	类型说明符 * 函数名(参数)
表示：
	float *fun();
	float *p;
	p = fun(a);
注意指针函数与函数指针表示方法的不同，千万不要混淆。
最简单的辨别方式就是看函数名前面的指针*号有没有被括号（）包含，
如果被包含就是函数指针，反之则是指针函数。

三，宏定义
#define MAX( a, b) ( (a) > (b) (a) : (b) )
四,声明新的类型名,类型别名
typedef 声明新的类型名
typedef int INTEGER；
INTEGER x，y；
等价于
int x , y;
typedef只是进行类型重定义，只是为该类型命名一个别名
四，sizeof
sizeof是C/C++中的一个操作符（operator）
简单的说其作用就是返回一个对象或者类型所占的内存字节数。
默认32位系统，括号()内为64位系统；
char ：				1个字节
char*（即指针变量）: 4个字节(8个字节) 
short int : 		2个字节
int：  				4个字节
unsigned int : 		4个字节
float:  			4个字节
double:   			8个字节
long:   			4个字节 (8个字节)
long long:  		8个字节 (8个字节)
unsigned long:  	4个字节 (8个字节)

五,malloc
malloc 向系统申请分配指定size个字节的内存空间。
返回类型是 void* 类型。void* 表示未确定类型的指针。
C,C++规定，void* 类型可以强制转换为任何其它类型的指针。



