#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>           
#include <fcntl.h>            
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <asm/types.h>        
#include <linux/videodev2.h>



typedef struct     //定义一个结构体来记录每个缓冲帧映射后的地址和长度。
{
	void *start;
	int length;
}BUFTYPE;

BUFTYPE *user_buf;
int n_buffer = 0;

//打开摄像头设备
int open_camer_device()
{
	int fd;

	if((fd = open("/dev/video0",O_RDWR )) < 0)  //以非阻塞的方式打开摄像头，返回摄像头的fd
	{
		perror("Fail to open");
		// exit(EXIT_FAILURE);
		return -1;
	} 
	printf("open video success\n");
	return fd;
}

int init_mmap(int fd)
{
	int i = 0;
	struct v4l2_requestbuffers reqbuf;          //向驱动申请帧缓冲的请求，里面包含申请的个数

	bzero(&reqbuf,sizeof(reqbuf));               //查看Man手册，我们知道，这个是初始化reqbuf这个变量里边的值为0
	reqbuf.count = 4;                            //缓冲区内缓冲帧的数目
	reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;   //缓冲帧数据格式
	reqbuf.memory = V4L2_MEMORY_MMAP;            //区别是内存映射还是用户指针方式,在这里是内存映射

	//申请视频缓冲区(这个缓冲区位于内核空间，需要通过mmap映射)
	//这一步操作可能会修改reqbuf.count的值，修改为实际成功申请缓冲区个数
	if(-1 == ioctl(fd,VIDIOC_REQBUFS,&reqbuf))     //向设备申请缓冲区
	{
		perror("Fail to ioctl 'VIDIOC_REQBUFS'");
		exit(EXIT_FAILURE);
	}

	n_buffer = reqbuf.count;  //把实际成功申请缓冲区个数的值赋给n_buffer这个变量，因为在申请的时候可能会修改reqbuf.count的值

	printf("n_buffer = %d\n",n_buffer);

	user_buf = calloc(reqbuf.count,sizeof(*user_buf));   //为这个结构体变量分配内存，这个结构体主要的目的保存的是
	//每一个缓冲帧的地址和大小。
	if(user_buf == NULL)
	{
		fprintf(stderr,"Out of memory\n");
		exit(EXIT_FAILURE);
	}

	/*******************************函数注释*********************************/
	//将这些帧缓冲区从内核空间映射到用户空间，便于应用程序读取/处理视频数据;
	//void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
	//addr 映射起始地址，一般为NULL ，让内核自动选择
	//length 被映射内存块的长度
	//prot 标志映射后能否被读写，其值为PROT_EXEC,PROT_READ,PROT_WRITE, PROT_NONE
	//flags 确定此内存映射能否被其他进程共享，MAP_SHARED,MAP_PRIVATE
	//fd,offset, 确定被映射的内存地址
	//返回成功映射后的地址，不成功返回MAP_FAILED ((void*)-1);
	//int munmap(void *addr, size_t length);// 断开映射
	//addr 为映射后的地址，length 为映射后的内存长度
	/*******************************函数注释*********************************/

	for(i = 0; i < reqbuf.count; i ++)
	{
		struct v4l2_buffer buf;

		bzero(&buf,sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		//查询申请到内核缓冲区的信息
		if(-1 == ioctl(fd,VIDIOC_QUERYBUF,&buf))  //通过fd就可以找到内核的缓冲区么？
		{
			perror("Fail to ioctl : VIDIOC_QUERYBUF");
			exit(EXIT_FAILURE);
		}

		user_buf[i].length = buf.length;
		user_buf[i].start =mmap(NULL,buf.length,PROT_READ | PROT_WRITE,MAP_SHARED,fd,buf.m.offset);
		if(MAP_FAILED == user_buf[i].start)  //MAP_FAILED表示mmap没有成功映射，其返回的值
		{
			perror("Fail to mmap");
			exit(EXIT_FAILURE);
		}
	} 

	return 0;
}


/**********************注释*******************************************
//VIDIOC_ENUM_FMT// 显示所有支持的格式  
//int ioctl(intfd, int request, struct v4l2_fmtdesc *argp);  
//structv4l2_fmtdesc  
//{  
//__u32 index;   // 要查询的格式序号，应用程序设置  
//enumv4l2_buf_type type;     // 帧类型，应用程序设置  
//__u32 flags;    // 是否为压缩格式  
//__u8       description[32];      // 格式名称  
//__u32pixelformat; // 格式  
//__u32reserved[4]; // 保留  
//};  
 *****************************注释*************************************/
/****************************注释*********************************************
//structv4l2_capability  
//{  
//__u8 driver[16];     // 驱动名字  
//__u8 card[32];       // 设备名字  
//__u8bus_info[32]; // 设备在系统中的位置  
//__u32 version;       // 驱动版本号  
//__u32capabilities;  // 设备支持的操作  
//__u32reserved[4]; // 保留字段  
//};  
//capabilities 常用值:  
//V4L2_CAP_VIDEO_CAPTURE    // 是否支持图像获取
 *******************************注释*******************************************/
/******************************注释********************************************
//structv4l2_format  
//{  
//enumv4l2_buf_type type;// 帧类型，应用程序设置  
//union fmt  
//{  
//structv4l2_pix_format pix;// 视频设备使用  
//structv4l2_window win;  
//structv4l2_vbi_format vbi;  
//structv4l2_sliced_vbi_format sliced;  
//__u8raw_data[200];  
//};  
//};  
 ********************************注释*******************************************/

//初始化视频设备
int init_camer_device(int fd)
{
	struct v4l2_fmtdesc fmt;              
	struct v4l2_capability cap;         
	struct v4l2_format stream_fmt;       
	int ret;

	//当前视频设备支持的视频格式
	memset(&fmt,0,sizeof(fmt));
	fmt.index = 0;
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;


	while((ret = ioctl(fd,VIDIOC_ENUM_FMT,&fmt)) == 0)  
	{
		fmt.index ++ ;
		printf("{pixelformat = %c%c%c%c},description = '%s'\n",fmt.pixelformat & 0xff,(fmt.pixelformat >> 8)&0xff,(fmt.pixelformat >> 16) & 0xff,(fmt.pixelformat >> 24)&0xff,fmt.description);
	}

	//查询设备属性
	ret = ioctl(fd,VIDIOC_QUERYCAP,&cap);
	if(ret < 0){
		perror("FAIL to ioctl VIDIOC_QUERYCAP");
		exit(EXIT_FAILURE);
	}


	//判断是否是一个视频捕捉设备
	if(!(cap.capabilities & V4L2_BUF_TYPE_VIDEO_CAPTURE))
	{
		printf("The Current device is not a video capture device\n");
		exit(EXIT_FAILURE);

	}
	printf("Successfully captured a video\n");

	//判断是否支持视频流形式
	if(!(cap.capabilities & V4L2_CAP_STREAMING))
	{
		printf("The Current device does not support streaming i/o\n");
		exit(EXIT_FAILURE);
	}
	printf("The Current device support streaming i/o\n");

	//设置摄像头采集数据格式，如设置采集数据的
	//长,宽，图像格式(JPEG,YUYV,MJPEG等格式)
	stream_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	stream_fmt.fmt.pix.width = 680;
	stream_fmt.fmt.pix.height = 480;
	//stream_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
	stream_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	stream_fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

	if(-1 == ioctl(fd,VIDIOC_S_FMT,&stream_fmt))
	{
		perror("Fail to ioctl");
		exit(EXIT_FAILURE);
	}

	//初始化视频采集方式(mmap)
	init_mmap(fd);

	return 0;
}


/************************注释**************************************************
//structv4l2_buffer  
//{  
//__u32 index;   //buffer 序号  
//enumv4l2_buf_type type;     //buffer 类型  
//__u32 byteused;     //buffer 中已使用的字节数  
//__u32 flags;    // 区分是MMAP 还是USERPTR  
//enum v4l2_fieldfield;  
//struct timevaltimestamp;// 获取第一个字节时的系统时间  
//structv4l2_timecode timecode;  
//__u32 sequence;// 队列中的序号  
//enum v4l2_memorymemory;//IO 方式，被应用程序设置  
//union m  
//{  
//__u32 offset;// 缓冲帧地址，只对MMAP 有效  
//unsigned longuserptr;  
//};  
//__u32 length;// 缓冲帧长度  
//__u32 input;  
//__u32 reserved;  
//};  
 *****************************注释************************************************/

//开始采集数据
int start_capturing(int fd)          //启动视频采集后，驱动程序开始采集一帧数据，把采集的数据放入视频采集输入队列的第一个帧缓冲区，
{                                   //一帧数据采集完成，也就是第一个帧缓冲区存满一帧数据后，驱动程序将该帧缓冲区移至视频采集
	unsigned int i;              //输出队列，等待应用程序 从输出队列取出。驱动程序接下来采集下一帧数据，放入第二个帧缓冲区，
	enum v4l2_buf_type type;   //同样帧缓冲区存满下一帧数据后，被放入视频采集输出队列。所以在开始采集视频数据之前，
	//我们需要将申请的缓冲区放入视频采集输入队列中排队,这样视频采集输入队列中才有帧缓冲                                                                 //区，这样也才能保存我们才采集的数据

	//将申请的内核缓冲区放入视频采集输入队列中排队
	for(i = 0;i < n_buffer;i ++)
	{
		struct v4l2_buffer buf;

		bzero(&buf,sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if(-1==ioctl(fd,VIDIOC_QBUF,&buf))
		{                                       
			perror("Fail to ioctl 'VIDIOC_QBUF'");
			exit(EXIT_FAILURE);
		}

	}


	//开始采集数据
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;                                                                                                      //参数中都没有给出，如何联系？
	if(-1 == ioctl(fd,VIDIOC_STREAMON,&type))    
	{
		printf("i = %d.\n",i);
		perror("Fail to ioctl 'VIDIOC_STREAMON'");
		exit(EXIT_FAILURE);
	}

	return 0;
}


//将采集好的数据放到文件中
int process_image(void *addr,int length)   
{                                         
	FILE *fp;
	static int num = 0;
	char picture_name[20];

	// sprintf(picture_name,"test.avi");             //将一帧帧的数据存入test.avi文件中
	sprintf(picture_name,"picture%d.jpg",num ++);  //格式化picture_name这个变量

	if((fp = fopen(picture_name,"w")) == NULL)   //这个方式和上述的sprintf结合起来就实现了文件的自动命名，此方法可以借鉴一下
	{
		perror("Fail to fopen");
		exit(EXIT_FAILURE);
	}
//	printf("sizeof(picture_name)=%d\n",sizeof(picture_name));
//	printf("length=%d\n",length);
//	printf("picture_name=%s\n",picture_name);
	fwrite(addr,length,1,fp);   //从addr写到fp中
	printf("addr=%p\n",addr);
	usleep(500);

	fclose(fp);

	return 0;
}

/***************函数注释****************/
//函数名：bzero
//函数功能：将指定内存块的前n个字节全部设置为零
//函数参数：
//			s为内存指针所指定内存块的首地址，n为需要清零的字节数
/*****************************************/
int read_frame(int fd)
{
	struct v4l2_buffer buf;       

	bzero(&buf,sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  // 指定buf的类型为capture，用于视频捕获设备
	buf.memory = V4L2_MEMORY_MMAP; //映射
	//从输出队列中去取缓冲区
	if(-1 == ioctl(fd,VIDIOC_DQBUF,&buf))                                                                                        //是如何把这两者联 系起来的
	{                                           
		perror("Fail to ioctl 'VIDIOC_DQBUF'");
		exit(EXIT_FAILURE);
	}
	
	assert(buf.index < n_buffer);      //assert函数的作用是判断计算表达式 expression ，如果其值为假（即为0），                                                                                    //那么它先向stderr打印一条出错信息,然后通过调用 abort 来终止程序运行。

	//读取进程空间的数据到一个文件中
	process_image(user_buf[buf.index].start,user_buf[buf.index].length);

	if(-1 == ioctl(fd,VIDIOC_QBUF,&buf))      //这个的作用是：将视频输出的缓冲帧放回到视频输入的缓冲区中去  
	{
		perror("Fail to ioctl 'VIDIOC_QBUF'");
		exit(EXIT_FAILURE);
	}

	return 1;
}


int mainloop(int fd)         //这个mainloop()函数中主要涉及的知识就是select()函数
{ 
	int count = 4;

	while(count -- > 0)     
	{
		for(;;)
		{
			fd_set fds;
			struct timeval tv;
			int r;

			FD_ZERO(&fds);
			FD_SET(fd,&fds);


			/*Timeout*/
			tv.tv_sec = 2;
			tv.tv_usec = 0;

			r = select(fd + 1,&fds,NULL,NULL,&tv);   
			if(-1 == r)
			{
				if(EINTR == errno)
					continue;

				perror("Fail to select");
				exit(EXIT_FAILURE);
			}
			if(0 == r)
			{
				fprintf(stderr,"select Timeout\n");
				exit(EXIT_FAILURE);
			}
			if(read_frame(fd))
			{
				break;
			}
		}
	}
	return 0;
}


void stop_capturing(int fd)
{
	enum v4l2_buf_type type;
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(-1 == ioctl(fd,VIDIOC_STREAMOFF,&type))
	{
		perror("Fail to ioctl 'VIDIOC_STREAMOFF'");
		exit(EXIT_FAILURE);
	}
	return;
}


void uninit_camer_device()
{
	unsigned int i;
	for(i = 0;i < n_buffer;i ++)
	{
		if(-1 == munmap(user_buf[i].start,user_buf[i].length))
		{
			exit(EXIT_FAILURE);
		}
	}
	free(user_buf);
	return;
}


void close_camer_device(int fd)
{
	if(-1 == close(fd))
	{
		perror("Fail to close fd");
		exit(EXIT_FAILURE);
	}
	return;
}


int main()
{
 	int fd; 
    fd = open_camer_device();
	init_camer_device(fd);
	start_capturing(fd);
    mainloop(fd);
	stop_capturing(fd);
	uninit_camer_device(fd);
	close_camer_device(fd);
	return 0;


#if 0
	int fd; 
	int i;
    unsigned int width;
	unsigned int height;
	unsigned int size;
	unsigned int index;
	unsigned int ismjpeg;

	width=320;
	height=240;
	int serfd = socket(AF_INET, SOCK_STREAM, 0);
	if(serfd < 0)
	{
		puts("socket error.");
		return -1;
	}
	puts("socket success.");


	struct sockaddr_in myser;
	memset(&myser, 0, sizeof(myser));
	myser.sin_family = AF_INET;
	myser.sin_port = htons(8888);
	myser.sin_addr.s_addr = htonl(INADDR_ANY);
	int ret = bind(serfd, (struct sockaddr *)&myser, sizeof(myser));
	if(ret != 0)
	{
		puts("bind error.");
		close(serfd);
		return -1;
	}
	puts("bind success.");
	ret = listen(serfd, 5);
	if(ret != 0)
	{
		puts("listen error.");
		close(serfd);
		return -1;
	}
	puts("listen success.");



	fd = open_camer_device();
	init_camer_device(fd);
	start_capturing(fd);

	while(1)
	{
		 printf("wating!!!\n");
		 char buf[1024];
		 int connfd = -1;
		 struct sockaddr_in mycli;
		 int len = sizeof(mycli);

		 connfd = accept(serfd, (struct sockaddr *)&mycli, &len);
		 if(connfd < 0)
		 {
			 puts("accept error.");
			 close(serfd);
			 return -1;
		 }
	     puts("accept success.");

		 while(1)
		 {
			mainloop(fd,(void **)&picture_name,&size,&index);
			char sizes[10]={0};
			sprintf(sizes,"%d",size);
			ret = write(connfd,sizes,10);
			if(ret<0)
			{
				close(connfd);
				return -1;
			}
			ret = write(connfd,picture_name,size);
			if(ret<0)
			{
				close(connfd);
				return -1;
			}
		 }
	}
	stop_capturing(fd);
	uninit_camer_device(fd);
	close_camer_device(fd);
	return 0;
#endif
}
