#include<string.h>
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<fcntl.h>

#define MAX_PROCESS_NUM 20
#define MAX_ARG_NUM 100
#define MAX_PATH_LEN 100
#define MAX_CMD_LEN 1000

char   cmd[MAX_CMD_LEN];				//输入的命令行
char   *argv[MAX_PROCESS_NUM][MAX_ARG_NUM];	//存放多个进程的argv参数（解析之后）
char 	*process[MAX_PROCESS_NUM];			//存放多个进程的argv（解析之前）
char	dir[MAX_PATH_LEN];				//调用getcwd的缓冲区

int getCommand()
{
	int pro_count = 0;					//计算命令直接开启的进程数量
	fgets(cmd, 1000, stdin);				//读入命令
	if (cmd[0] == '\n')					//如果这个命令只有一个“\n”，则直接返回-1 ，重新输入
	{
		return -1;
	}
	char delims[] = { '|','\n' };			//先以管道符号|，分隔开多个进程
	process[pro_count] = strtok(cmd, delims);      //把第一个进程的整条命令先分裂出来
	while (process[pro_count] != NULL && pro_count<MAX_PROCESS_NUM)		//把所有进程的命令分离，并计算进程数量
	{
		process[++pro_count] = strtok(NULL, delims);
	}
	if (pro_count >= MAX_PROCESS_NUM)			//解析出的进程数大于最大值
	{
		printf("error : too much process! pls try again!\n");
		return -1;
	}
	int i;
	for (i = 0; i<pro_count; i++)			//对每一个进程的命令进行分解，把它的参数解析出来并存放在argv中
	{
		int argv_count = 0;
		char delims[] = { ' ' };			//以空格' ' 作为分隔符
		argv[i][argv_count] = strtok(process[i], delims);	//类似于分割进程命令
		while (argv[i][argv_count] != NULL & argv_count < MAX_ARG_NUM)
		{
			argv[i][++argv_count] = strtok(NULL, delims);
		}
		if (argv_count >= MAX_ARG_NUM)	//解析出的字符数大于最大值
		{
			printf("error : too much arguement!pls try again!\n");
			return -1;
		}
	}
	return pro_count;					//返回进程数量
}

/*
在isInterCommand()函数中处理内部命令：这里暂时实现了exit和cd两个命令
*/
int isInterCommand()
{
	if (strcmp("exit", argv[0][0]) == 0)	//退出shell	
	{
		exit(0);
	}
	if (strcmp("cd", argv[0][0]) == 0)	//cd命令
	{
		//printf("in cd:%s\n", argv[1]);	
		if (chdir(argv[0][1])<0)		
		{
			perror("chdir");
		}
		return 0;
	}
	return -1;
}

/*
在exterCommand函数中处理外部命令，这里实现了管道和重定向功能，它的参数是之前解析得到的进程数量 
*/
void exterCommand(int pro_count)			
{
	pid_t pid[MAX_PROCESS_NUM];							
	if (pro_count>1)											//多个进程:存在管道
	{
		int i, j;									
		int fd[MAX_PROCESS_NUM][2];								//创建管道的fd
		for (i = 0; i<pro_count - 1; i++)
		{
			pipe(fd[i]);										//有pro_count个进程，则有pro_count-1个管道，先在主进程(shell进程)创建管道
		}
		for (i = 0; i<pro_count; i++)							//分别fork每个子进程
		{
			if ((pid[i] = fork())<0)							//fork	
			{
				perror("fork");
				exit(1);
			}
			else if (pid[i] == 0)								//fork出来的子进程
			{
				if (i == 0)//第一个进程							//第一个子进程：与之关联的只有第一个管道：
				{
					if (dup2(fd[i][1], 1)<0)					//将第一个管道的写端复制到该进程的写端
					{
						perror("dup2");
					}
					close(fd[i][0]);							//关闭继承而来、但不需要使用的fd
					close(fd[i][1]);
					/*
					以下代码与重定向有关:在存在管道的命令中，只有第一个进程可以有输入重定向<(其它进程都以从管道得到它的标准输入)
					*/
					int k = 0;
					while (argv[i][k] != NULL)							//寻找解析出来的argv[i]中，是否有输入重定向符号<
					{
						if (strcmp("<", argv[i][k]) == 0)				//找到一个<
						{
							int fd;
							if ((fd = open(argv[0][k + 1], O_RDONLY))<0)//下一个参数为输入重定向的文件，以只读方式打开它
							{
								perror("open");
								exit(1);
							}	
							dup2(fd, 0);								//把这个fd复制到标准输入
							close(fd);								
							argv[0][k] = NULL;							//
							break;
						}
						k++;
					}
				}
				else if (i == pro_count - 1)//最后一个进程
				{
					if (dup2(fd[i - 1][0], 0)<0)						//最后一个进程只有它前面的第一个管道与之相关联，
					{
						perror("dup2");
					}
					for (j = 0; j<i; j++)								//关闭其它不用的fd
					{
						close(fd[j][0]);
						close(fd[j][1]);
						
					}
					/*
					以下代码与重定向有关:在存在管道的命令中，只有最后一个进程可以有输出重定向(>和>>)(其它进程都将输出写到管道中了)
					*/
					int k = 0;
					while (argv[i][k] != NULL)
					{
						
						if (strcmp(argv[i][k], ">") == 0)										//找到>
						{
							int fd = open(argv[i][k + 1], O_WRONLY | O_CREAT|O_TRUNC, 0666);		//以只写文件打开它，如果该文件不存在，则创建它
							
							dup2(fd, 1);										//将该fd复制到标准输出					
							close(fd);
							argv[i][k] = NULL;
							break;
						}	
						else if (strcmp( argv[i][k],">>") == 0)								//找到>>
						{
							int fd = open(argv[i][k + 1], O_WRONLY | O_CREAT | O_APPEND, 0666);	//以只写、追加模式打开，如果该文件不存在，则创建它
													
							dup2(fd, 1);													//将该fd复制到标准输出
							close(fd);
							argv[i][k] = NULL;
							break;
						}
						k++;
					}
				}
				else
				{
					//中间的进程，有前后两个管道与它相关联
					dup2(fd[i - 1][0], 0);					//将进程的读端连接到前一个管道的读端 							
					dup2(fd[i][1], 1);						//将进程的写端连接到后一个管道的写端
					for (j = 0; j <= i; j++)				//关闭其它fd
					{
						close(fd[j][0]);
						close(fd[j][1]);
					}
				}
				if (execvp(argv[i][0], argv[i])<0)			//使用execvp运行程序
				{
					perror("execvp");
					exit(1);
				}
			}
		}
		for (i = 0; i<pro_count - 1; i++)					//主进程：关闭此前打开的fd，即：创建的管道相关的fd
		{
			close(fd[i][0]);
			close(fd[i][1]);
		}
		for (i = 0; i<pro_count; i++)						//主进程，等待所有的子进程完成
		{
			waitpid(pid[i], NULL, 0);
			printf("%d done!\n", pid[i]);
		}
	}
	else//没有管道!											//命令只直接创建一个进程：这时候，需要考虑重写向符号的“组合问题”
	{
		pid_t pid = fork();
		if (pid<0)
		{
			printf("fork failure!\n");
			exit(1);
		}
		else if (pid == 0)								
		{
			int redire_index[3] = { -1,-1,-1 };				//重写向的索引:这个3个数字的数组分别代表了>、>>、<三个重定向在argv[0]出现的索引 
			
			/*
			该while循环遍历argv[0]，寻找>、>>、<等重定向符号
			对于输出重定向，规则是:> 和>>二者最多只能出现1个
			而输入重定向，可以与一个>或者一个>>组合，当然，也不能出现两个<
			*/
			int j = 0;
			while (argv[0][j] != NULL)
			{
				if (strcmp(argv[0][j], ">") == 0)
				{
					if (redire_index[0] != -1 || redire_index[1] != -1)		//此前已经存在>或者>>
					{
						perror("error!> and >> 不能重复出现");
					}
					redire_index[0] = j;
				}
				else if (strcmp(argv[0][j], ">>") == 0)						
				{
					if (redire_index[0] != -1 || redire_index[1] != -1)		//此前已经存在>或者>>
					{
						perror("error!> and >> 不能重复出现");
						exit(1);
					}
					redire_index[1] = j;
				}
				else if (strcmp(argv[0][j], "<") == 0)						
				{
					if (redire_index[2] != -1)			
					{
						perror("error!< 不能重复出现");
						exit(1);
					}
					redire_index[2] = j;
				}
				j++;
			}
			if ((j = redire_index[0]) != -1)//出现了>:输出重定向(覆盖输出)
			{
				int fd = open(argv[0][j + 1], O_WRONLY | O_CREAT|O_TRUNC, 0666);
				dup2(fd, 1);
				close(fd);
				argv[0][redire_index[0]] = NULL;
			}
			else if ((j = redire_index[1]) != -1)//出现了>>:输出重定向(追加输出)
			{
				int fd = open(argv[0][j + 1], O_WRONLY | O_CREAT | O_APPEND, 0666);		//以O_APPEND方式打开
				dup2(fd, 1);
				close(fd);
				argv[0][redire_index[1]] = NULL;
			}
			if ((j = redire_index[2]) != -1)	//出现了<:输入重定向
			{
				int fd;
				if ((fd = open(argv[0][j + 1], O_RDONLY))<0)
				{
					perror("open");
					exit(1);
				}
				dup2(fd, 0);
				close(fd);
				argv[0][redire_index[2]] = NULL;
			}
			//子进程，开始执行新程序
			if (execvp(argv[0][0], argv[0])<0)
			{
				perror("execvp");
				exit(1);
			}
		}
		else
		{
			wait(NULL);							//主进程:等待子进程完成
		}
	}
}
int main()
{
	while (1)
	{
		int pro_count;
		getcwd(dir, MAX_PATH_LEN);
		printf("[%s csy_shell]$ ", dir);
		if ((pro_count = getCommand()) < 0)
			continue;
		if (isInterCommand() < 0)
		{
			exterCommand(pro_count);
		}
	}
	return 0;
}
