#include "stdafx.h"
#ifdef _SUPPORT_HTTP_DOWNLOAD
#include <stdlib.h>
#include <stdio.h>
#include "x_http_client.h"
#include <string.h>//字符串处理
#include <sys/socket.h>//套接字
#include <arpa/inet.h>//ip地址处理
#include <fcntl.h>//open系统调用
#include <unistd.h>//write系统调用
#include <netdb.h>//查询DNS
#include <sys/stat.h>//stat系统调用获取文件大小
#include <sys/time.h>//获取下载时间
#include <time.h>
#define FILE_NO   FILE_P_X_NET_INF
#define HTTP_HEAD_SIZE 2048
yw_http_download_client::yw_http_download_client()
{
	X_PRINT("init http client!!!\n");
	m_store_pos = IN_NATIVE;
	m_download_status = PREPER_DOWNLOAD;
	m_load_mode=SIG_THR_MODE;
	memcpy(m_url,"http://127.0.0.1",strlen("http://127.0.0.1"));		
	m_thr_num = 1;
	m_waittime = 30;
	m_redowncount = m_waittime;
	m_isEnableRandomAccessFile = false;
	m_save_in_memory=NULL;
	m_result_notify =NULL;
}
yw_http_download_client::~yw_http_download_client()
{
}
	
u32 yw_http_download_client::setSerAddr(const char *url)
{
	memset(m_url,0,sizeof(m_url));
	memcpy(m_url,url,strlen(url));
	X_PRINT("m_url=%s\n",url);
	return 0;
}
u32 yw_http_download_client::setDownloadPath(HttpDlStorePosition storepostion,char *fileAddr,char* file_name)
{
	m_store_pos = storepostion;
	if(fileAddr != NULL && file_name !=NULL)
	{
		strncpy(m_file_path,fileAddr,strlen(fileAddr));
		strncpy(m_file_name,file_name,strlen(file_name));
	}
	return 0;
}
u32 yw_http_download_client::setRandomAccessFile(bool isEnableRandomAccessFile,i32 wait_time)
{
	m_isEnableRandomAccessFile = isEnableRandomAccessFile;
	if(isEnableRandomAccessFile == true)
	{
		m_waittime=wait_time;
		m_redowncount = wait_time;
	}	
	else	
        {
		m_waittime= 0;
		m_redowncount = 0;
	}
	return 0;
}
u32 yw_http_download_client::setDownloadMode(HttpDownloadMode loadmode,u32 thr_num)
{
	m_load_mode = loadmode;
	m_thr_num = thr_num;
	return 0;
}
u32 yw_http_download_client::parse_url(char* host,u32 *port,char* file_name)
{
    /*通过url解析出域名, 端口, 以及文件名**/
    i32 j = 0;
    i32 start = 0;
    *port = 80;
    const char *patterns[] = {"http://", "https://", NULL};
    for (i32 i = 0; patterns[i]; i++)//分离下载地址中的http协议
        if (strncmp(m_url, patterns[i], strlen(patterns[i])) == 0)
            start = strlen(patterns[i]);
    //解析域名, 这里处理时域名后面的端口号会保留
    for (i32 i = start; m_url[i] != '/' && m_url[i] != '\0'; i++, j++)
        host[j] = m_url[i];
    host[j] = '\0';
    //解析端口号, 如果没有, 那么设置端口为80
    char *pos = strstr(host, ":");
    if (pos)
        sscanf(pos, ":%d", port);
    //删除域名端口号
    for (i32 i = 0; i < (i32)strlen(host); i++)
    {
        if (host[i] == ':')
        {
            host[i] = '\0';
            break;
        }
    }
    //获取下载文件名
    j = 0;
    for (i32 i = start; m_url[i] != '\0'; i++)
    {
        if (m_url[i] == '/')
        {
            if (i !=  (i32)strlen(m_url) - 1)
                j = 0;
            continue;
        }
        else
            file_name[j++] = m_url[i];
    }
    file_name[j] = '\0';	
	return 0;
}
struct HTTP_RES_HEADER yw_http_download_client::parse_header(char *response)
{
    /*获取响应头的信息*/
    struct HTTP_RES_HEADER resp;
    char *pos = strstr(response, "HTTP/");
    if (pos)//获取返回代码
        sscanf(pos, "%*s %d", &resp.status_code);
    pos = strstr(response, "Content-Type:");
    if (pos)//获取返回文档类型
        sscanf(pos, "%*s %s", resp.content_type);
    pos = strstr(response, "Content-Length:");
    if (pos)//获取返回文档长度
        sscanf(pos, "%*s %lld", &resp.content_length);
    return resp;
}
void yw_http_download_client::get_ip_addr(char *host_name, char *ip_addr)
{
    /*通过域名得到相应的ip地址*/
    struct hostent *host = gethostbyname(host_name);//此函数将会访问DNS服务器
    if (!host)
    {
        ip_addr = NULL;
        return;
    }
    for (i32 i = 0; host->h_addr_list[i]; i++)
    {
        strcpy(ip_addr, inet_ntoa( * (struct in_addr*) host->h_addr_list[i]));
        break;
    }
}
void yw_http_download_client::progress_bar(s64 cur_size, s64 total_size, f64 speed)
{
    /*用于显示下载进度条*/
    float percent = (float) cur_size / total_size;
    const i32 numTotal = 50;
    i32 numShow = (i32)(numTotal * percent);
    if (numShow == 0)
        numShow = 1;
    if (numShow > numTotal)
        numShow = numTotal;
    char sign[51] = {0};
    memset(sign, '=', numTotal);
    printf("\r%.2f%%[%-*.*s] %.2f/%.2fMB %4.0fkb/s", percent * 100, numTotal, numShow, sign, cur_size / 1024.0 / 1024.0, total_size / 1024.0 / 1024.0, speed);
    fflush(stdout);
    m_download_per = percent * 100;
    if (numShow == numTotal)
        printf("\n");
}
s64 yw_http_download_client::get_file_size(const char *filename)
{
    //通过系统调用直接得到文件的大小
    struct stat buf;
    if (stat(filename, &buf) < 0)
        return 0;
    return (s64) buf.st_size;
}
void yw_http_download_client::checkStausChanged(HttpDownLoadStatus oldStatus,HttpDownLoadStatus newStatus)
{
	printf("m_old_download_status=%d newStatus=%d\n",m_old_download_status,newStatus);
	if(oldStatus !=newStatus)
	{
		if(m_result_notify !=NULL)
			 m_result_notify(newStatus);
		m_old_download_status = newStatus;
	}
}
void yw_http_download_client::download(i32 client_socket, char *file_name, s64 content_length)
{
    /*下载文件函数*/
    s64 hasrecieve = 0;//记录已经下载的长度
    struct timeval t_start, t_end;//记录一次读取的时间起点和终点, 计算速度
    i32 mem_size = 8192;//缓冲区大小8K
    i32 buf_len = mem_size;//理想状态每次读取8K大小的字节流
    i32 len=0;
	i32 fd=0;
    m_download_status = ON_DOWNLOAD;
	checkStausChanged(m_old_download_status,m_download_status);
    //创建文件描述符
    if(m_store_pos !=IN_MEM)
    {
	    fd= open(file_name, O_CREAT | O_WRONLY , S_IRWXG | S_IRWXO | S_IRWXU);
	    if (fd < 0)
	    {
	    	printf("create file faile!\n");	
	        //printf("文件创建失败!\n");
			m_download_status = DOWNLOAD_FAIL;
			checkStausChanged(m_old_download_status,m_download_status);
			return ;
		}
    }
    char *buf = (char *) x_malloc(mem_size * sizeof(char));
	memset(buf,0,sizeof(buf));
    //从套接字流中读取文件流
    s64 diff = 0;
    i32 prelen = 0;
    f64 speed =0;
    i32 fail_count=0;
	struct timeval outTime;
	outTime.tv_sec = 5;   //设置等待时间为1s
	outTime.tv_usec = 0; //毫秒
	fd_set fdread;
    while (hasrecieve < content_length)
    {
        gettimeofday(&t_start, NULL ); //获取开始时间
	   FD_ZERO(&fdread);
	   FD_SET(client_socket, &fdread); //sessionSock为之前创建的会话套接字
	 
	   switch(select(client_socket + 1, &fdread, NULL, NULL, &outTime))
	  {
	    case -1:  
	    case  0:
		len = 0;
		break;
	   default:
		if(FD_ISSET(client_socket, &fdread))
			len=recv(client_socket,buf,buf_len,0);
		break;
	}
        //len = read(client_socket, buf, buf_len);
	if(len <= 0)
	{
		fail_count ++;
		if(fail_count == 100)
		{
			break;
		}
	}
	else
	{
		fail_count = 0;
		if(m_store_pos == IN_NATIVE)
		{
        		write(fd, buf, len);
		}
		else if(m_store_pos == IN_MEM)
		{
			int res=m_save_in_memory(buf,len);
			if(res >= 0)
				m_total_size +=len;
		}
		else
		{
			write(fd, buf, len);
		}
	}         
	gettimeofday(&t_end, NULL ); //获取结束时间
	
        hasrecieve += len;//更新已经下载的长度
	
        //计算速度
        if (t_end.tv_usec - t_start.tv_usec >= 0 &&  t_end.tv_sec - t_start.tv_sec >= 0)
            diff += 1000000 * ( t_end.tv_sec - t_start.tv_sec ) + (t_end.tv_usec - t_start.tv_usec);//us
        if (diff >= 1000000)//当一个时间段大于1s=1000000us时, 计算一次速度
        {
            speed = (f64)(hasrecieve - prelen) / (f64)diff * (1000000.0 / 1024.0);
            prelen = hasrecieve;//清零下载量
            diff = 0;//清零时间段长度
        }
        progress_bar(hasrecieve, content_length, speed);
        if (hasrecieve == content_length)
            break;
    }
	if(m_store_pos!= IN_MEM && fd > 0)
    	close(fd);
	if(buf)
	  x_free(buf);
}
	
u32 yw_http_download_client::startDownload()
{
		char host[128] = {0};
		u32 port = 0;
		char file_name[128] = {0};
		char  ip_addr[16] = {0};
		char file_path_name[128] = {0};
		char * temp = NULL;
		m_download_status = PREPER_DOWNLOAD;
		m_old_download_status = PREPER_DOWNLOAD;
		X_PRINT("1.analysis url...\n");
		//printf("1: 正在解析下载地址..."); 
		parse_url(host, &port, file_name);
		X_PRINT("host:[%s],port=%d file_name=%s\n",host,port,file_name);
		char*pos=strstr(m_url,host);
		if(pos !=NULL)
		{
			if(strlen(pos) > 0)
			{
				char *pos1=strstr(pos, "/");
				if(pos1 !=NULL)
				{
					if(strlen(pos1) >0)
					{
						memcpy(m_resq_file_name,pos1,strlen(pos1));
						printf("aaaaaam_resq_file_name=%s\n",m_resq_file_name);
					}
					else
					{
						memcpy(m_resq_file_name,file_name,strlen(m_resq_file_name));
					}
				}
				else
				{
					memcpy(m_resq_file_name,m_url,strlen(m_url));
				}
			}
			else
			{
				memcpy(m_resq_file_name,m_url,strlen(m_url));
			}
		}
		else
		{
			memcpy(m_resq_file_name,m_url,strlen(m_url));
		}
			X_PRINT("2.get server ip addr...\n");
			//printf("2: 正在获取远程服务器IP地址...");
			get_ip_addr(host, ip_addr);//调用函数同访问DNS服务器获取远程主机的IPs
			if (strlen(ip_addr) == 0)
				{
					printf("err: can not get server ip addr ,please check url...\n");
				 // printf("错误: 无法获取到远程服务器的IP地址, 请检查下载地址的有效性\n");
					m_download_status = DOWNLOAD_FAIL;
					checkStausChanged(m_old_download_status,m_download_status);
				  return -1;
				}
			strncpy(m_host,host,strlen(host));
			strncpy(m_ip_addr,ip_addr,strlen(ip_addr));
			m_port=port;
	#if 0
			puts("\n>>>>下载地址解析成功<<<<");
				printf("\t下载地址: %s\n", m_url);
				printf("\tԶ程主机: %s\n", host);
				printf("\tIP 地 址: %s\n", ip_addr);
				printf("\t主机PORT: %d\n", port);
			printf("\t 文件名 : %s\n\n", file_name);
	#endif 
			X_PRINT("\n download ip addr analysis success \n");
			X_PRINT("\t url=%s\n",m_url);
			X_PRINT("\t host=%s \n",host);
			X_PRINT("\t server ip=%s\n",ip_addr);
			X_PRINT("\t server port=%d\n",port);
			X_PRINT("\t download file name =%s\n",file_name);
			
			
			//设置http请求头信息
				char header[2048] = {0};
				sprintf(header, \
					"GET %s HTTP/1.1\r\n"\
					"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"\
					"User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537(KHTML, like Gecko) Chrome/47.0.2526Safari/537.36\r\n"\
					"Host: %s\r\n"\
					"Connection: keep-alive\r\n"\
					"\r\n"\
				,m_resq_file_name, host);
				printf("3. create socket...\n");
				//puts("3: 创建网络套接字...");
				i32 client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
				if (client_socket < 0)
				{
					printf("create socket fail...\n");
					//printf("套接字创建失败: %d\n", client_socket);
					m_download_status = DOWNLOAD_FAIL;
					checkStausChanged(m_old_download_status,m_download_status);
					return -1;
				}
				m_client_socket = client_socket;
				//创建IP地址结构体
				struct sockaddr_in addr;
				memset(&addr, 0, sizeof(addr));
				addr.sin_family = AF_INET;
				addr.sin_addr.s_addr = inet_addr(ip_addr);
				addr.sin_port = htons(port);
		
				//连接远程主机
				printf("4.on connect server...\n");
			   // puts("4: 正在连接远程主机...");
				i32 res = connect(client_socket, (struct sockaddr *) &addr, sizeof(addr));
				if (res == -1)
				{
				printf("connect server fail , error:%d\n",res);
			//	printf("连接远程主机失败, error: %d\n", res);
				m_download_status = DOWNLOAD_FAIL;
				checkStausChanged(m_old_download_status,m_download_status);
				return -1;
				}
				printf("5.send http request...\n");
			//puts("5: 正在发送http下载请求...");
				write(client_socket, header, strlen(header));//writeϵͳ调用, 将请求header发送给服务器
				i32 mem_size = 4096;
				 i32 length = 0;
				 i32 len;
				 char *buf = (char *) x_malloc(mem_size * sizeof(char));
				 char *response = (char *) x_malloc(mem_size * sizeof(char));
		
			//ÿ次单个字符读取响应头信息
			//puts("6: 正在解析http响应头...");
			printf("analysis http head....\n");
			while ((len = read(client_socket, buf, 1)) != 0)
			{
				if (length + len > mem_size)
				{
					//动态内存申请, 因为无法确定响应头内容长度
					mem_size *= 2;
					temp= (char *) realloc(response, sizeof(char) * mem_size);
					if (temp == NULL)
					{
						X_ERROR("realloc memory fail\n");
						//printf("动态内存申请失败\n");
						m_download_status = DOWNLOAD_FAIL;
						checkStausChanged(m_old_download_status,m_download_status);
						return -1;
					}
					response = temp;
				}
		
				buf[len] = '\0';
				strcat(response, buf);
		
				//找到响应头的头部信息
				i32 flag = 0;
				for (i32 i = strlen(response) - 1; response[i] == '\n' || response[i] == '\r'; i--, flag++);
				if (flag == 4)//连续两个换行和回车表示已经到达响应头的头尾, 即将出现的就是需要下载的内容
					break;
		
				length += len;
			}
		
			struct HTTP_RES_HEADER resp = parse_header(response);
			X_PRINT("\t >>>>http answer success...\n");
			X_PRINT("\t >>>>answer code :%d\n",resp.status_code);
		   // printf("\n>>>>http响应头解析成功:<<<<\n");
		   // printf("\tHTTP响应代码: %d\n", resp.status_code);
			if (resp.status_code != 200)
			{
				X_WARN("file can not download!!! code =%d\n",resp.status_code);
				//printf("文件无法下载, 远程主机返回: %d\n", resp.status_code);
				m_download_status = DOWNLOAD_FAIL;
				checkStausChanged(m_old_download_status,m_download_status);
				return 0;
			}
			X_PRINT("start download.....\n");
			//printf();
		   // printf("\tHTTP文档类型: %s\n", resp.content_type);
		   // printf("\tHTTP主体长度: %ld字节\n\n", resp.content_length);
		   // printf("7: 开始文件下载...\n");
		   m_content_length=resp.content_length;
			//printf("1.m_content_len=%d resp.content_length=%d\n",m_content_length,resp.content_length);
			if(m_store_pos == IN_NATIVE)
				snprintf(file_path_name,sizeof(file_path_name),"%s/%s",m_file_path,m_file_name);
			X_PRINT("file_path_name=%s\n",file_path_name);
			if(m_store_pos == IN_NATIVE)
				download(client_socket, file_path_name, resp.content_length);
			else if(m_store_pos == IN_MEM)
				download(client_socket, m_fileAddr, resp.content_length);
			else
				download(client_socket, file_path_name, resp.content_length);
			if(m_store_pos == IN_NATIVE)
			{
				if (resp.content_length == get_file_size(file_path_name))
				{
				   // printf("\n文件%s下载成功! ^_^\n\n", file_name);
					m_download_status = DOWNLOAD_SUCCESS;
					checkStausChanged(m_old_download_status,m_download_status);
				}
				else
				{
					m_download_status = DOWNLOAD_PAUSE;
					checkStausChanged(m_old_download_status,m_download_status);
	
					//remove(file_name);
				//	  printf("\n文件下载中有字节缺失, 下载失败, 请重试!\n\n");
				}
			}
			else
			{
				 if (resp.content_length == m_total_size)
				{
				   // printf("\n文件%s下载成功! ^_^\n\n", file_name);
					m_download_status = DOWNLOAD_SUCCESS;
					checkStausChanged(m_old_download_status,m_download_status);
	
				}
				else
				{
					if(m_isEnableRandomAccessFile == true)
					{
						m_download_status = DOWNLOAD_PAUSE;
						checkStausChanged(m_old_download_status,m_download_status);
					}
					else
					{
						m_download_status = DOWNLOAD_FAIL;
						checkStausChanged(m_old_download_status,m_download_status);
					}
					//remove(file_name);
					//printf("\n文件下载中有字节缺失, 下载失败, 请重试!\n\n");
				}
		
			}
			if(m_isEnableRandomAccessFile == true)
			{
			
				while(m_download_status == DOWNLOAD_PAUSE && m_redowncount > 0)
				{
					printf("aa");
					goondownload(file_path_name);
					m_download_status = DOWNLOAD_PAUSE;
					checkStausChanged(m_old_download_status,m_download_status);
					X_PRINT("redowncount=%d",m_redowncount);
					m_redowncount --;
					if(m_store_pos != IN_MEM)
					{
						if (resp.content_length == get_file_size(file_path_name))
						{
							X_TRACE("download success......\n");
							//printf("\n文件%s下载成功! ^_^\n\n", file_name);
							m_download_status = DOWNLOAD_SUCCESS;
							checkStausChanged(m_old_download_status,m_download_status);
							break;			
						}
					}
					else
					{
						if (resp.content_length == m_total_size)
						{
							m_download_status = DOWNLOAD_SUCCESS;
							checkStausChanged(m_old_download_status,m_download_status);
							break;
						}
						else
						{
							m_download_status = DOWNLOAD_PAUSE;
							checkStausChanged(m_old_download_status,m_download_status);
						   // printf("\n文件下载中有字节缺失, 下载失败, 请重试!\n\n");
						}
					}
					sleep(1);
				}
			}
			if(m_store_pos == IN_NATIVE)
			{
				if (resp.content_length != get_file_size(file_path_name))
				{
					X_WARN("down load fail...\n");
					//printf("\n文件下载中有字节缺失, 下载失败, 请重试!\n\n");
					m_download_status = DOWNLOAD_FAIL;
					checkStausChanged(m_old_download_status,m_download_status);
					remove(m_file_name);			
				}
			}
			else
			{
				if (resp.content_length == m_total_size)
				{
					printf("download success!!!");
					m_download_status = DOWNLOAD_SUCCESS;
					checkStausChanged(m_old_download_status,m_download_status);
				}
				else
				{
					
					m_download_status = DOWNLOAD_FAIL;
					X_WARN("down load fail...\n");
					checkStausChanged(m_old_download_status,m_download_status);
					//printf("\n文件下载中有字节缺失, 下载失败, 请重试!\n\n");
				}
		
			}
			shutdown(client_socket, 2);//关闭套接字的接收和发送
			if(buf)
			  x_free(buf);
			if(response)
			  x_free(response);
			return 0;
}
i32 yw_http_download_client::get_head_info(i32 client_socket,char *buf,char *response,struct HTTP_RES_HEADER *resp)
{
    i32 mem_size = 4096;
    i32 length = 0;
    i32 len;
    puts("6: 正在解析http响应头...");
    while ((len = read(client_socket, buf, 1)) != 0)
    {
        if (length + len > mem_size)
        {
            //动态内存申请, 因为无法确定响应头内容长度
            mem_size *= 2;
            char * temp = (char *) realloc(response, sizeof(char) * mem_size);
            if (temp == NULL)
            {
            	X_WARN("realloc memory fail...\n")	;
//                printf("动态内存申请失败\n");
				m_download_status = DOWNLOAD_FAIL;
				checkStausChanged(m_old_download_status,m_download_status);
               return -1;
            }
            response = temp;
        }
        buf[len] = '\0';
        strcat(response, buf);
        //找到响应头的头部信息
        i32 flag = 0;
        for (i32 i = strlen(response) - 1; response[i] == '\n' || response[i] == '\r'; i--, flag++);
        if (flag == 4)//连续两个换行和回车表示已经到达响应头的头尾, 即将出现的就是需要下载的内容
            break;
        length += len;
    }
    struct HTTP_RES_HEADER  tmp_resp;
    tmp_resp = parse_header(response);
    memcpy(resp,&tmp_resp,sizeof(HTTP_RES_HEADER));
  //  printf("\n>>>>http响应头解析成功:<<<<\n");
 //   printf("\tHTTP响应代码: %d\n", resp->status_code);
    if (resp->status_code != 200)
    {
  //      printf("文件无法下载, 远程主机返回: %d\n", resp->status_code);
        return 0;
    }
 //   printf("\tHTTP文档类型: %s\n", resp->content_type);
 //   printf("\tHTTP主体长度: %ld字节\n\n", resp->content_length);
 	return 0;
}
s64 yw_http_download_client::FileLocate(char* path)  
{
	FILE *fp = NULL; s64 last = 0;
	if((fp=fopen(path,"ab+"))==NULL)
	{
		printf("can not open %s\n",path);
		return 0;
	}
	else
	{
		fseek(fp,0L,SEEK_END);
		last=ftell(fp);
		fclose(fp);
		return last;
	}
}
void yw_http_download_client::redownload(i32 client_socket, char *file_name, s64 content_length)
{
    /*下载文件函数*/
    s64 hasrecieve = 0;//记录已经下载的长度
    struct timeval t_start, t_end;//记录一次读取的时间起点和终点, 计算速度
    i32 mem_size = 8192;//缓冲区大小8K
    i32 buf_len = mem_size;//理想状态每次读取8K大小的字节流
    i32 len =0;
	 i32 fd=0;
    m_download_status = ON_DOWNLOAD;
    //创建文件描述符
    if(m_store_pos != IN_MEM)
    {
	   fd= open(file_name, O_CREAT | O_WRONLY | O_APPEND, S_IRWXG | S_IRWXO | S_IRWXU);
	    if (fd < 0)
	    {
	      //  printf("文件创建失败!\n");
			X_WARN("open file =%s faile ...\n",file_name);
			m_download_status = DOWNLOAD_FAIL;
			checkStausChanged(m_old_download_status,m_download_status);
	       return;
	    }
    }
    char *buf = (char *) x_malloc(mem_size * sizeof(char));
    //从套接字流中读取文件流
    s64 diff = 0;
    i32 prelen = 0;
    f64 speed=0;
    u32 fail_count =5;
	struct timeval outTime;
	outTime.tv_sec = 5;   //设置等待时间为1s
	outTime.tv_usec = 0; //毫秒
	fd_set fdread;
    while (hasrecieve < content_length)
    {
        gettimeofday(&t_start, NULL ); //获取开始时间
	   FD_ZERO(&fdread);
	   FD_SET(client_socket, &fdread); //sessionSock为之前创建的会话套接字
	 
	   switch(select(client_socket + 1, &fdread, NULL, NULL, &outTime))
	  {
	    case -1:  
	    case  0:
		len = 0;
		break;
	   default:
		if(FD_ISSET(client_socket, &fdread))
			len=recv(client_socket,buf,buf_len,0);
		break;
	}
        //len = read(client_socket, buf, buf_len);
	X_PRINT("len=%d fail_count=%d",len,fail_count);
	if(len <= 0)
	{
		fail_count ++;
		usleep(100);	
		if(fail_count == 100)
		{
			printf("download fail ....\n");
			break;
		}
	}
	else
	{
		fail_count = 0;
		if(m_store_pos != IN_MEM)
		{
        	write(fd, buf, len);
		}
		else
		{
			u32 res =m_save_in_memory(buf,len);
			if(res >= 0)
				m_total_size += len;
		}
		m_redowncount=m_waittime;
	}         
        gettimeofday(&t_end, NULL ); //获取结束时间
        hasrecieve += len;//更新已经下载的长度
        //计算速度
        if (t_end.tv_usec - t_start.tv_usec >= 0 &&  t_end.tv_sec - t_start.tv_sec >= 0)
            diff += 1000000 * ( t_end.tv_sec - t_start.tv_sec ) + (t_end.tv_usec - t_start.tv_usec);//us
        if (diff >= 1000000)//当一个时间段大于1s=1000000us时, 计算一次速度
        {
            speed = (f64)(hasrecieve - prelen) / (f64)diff * (1000000.0 / 1024.0);
            prelen = hasrecieve;//清零下载量
            diff = 0;//清零时间段长度
        }
        progress_bar(hasrecieve, content_length, speed);
        if (hasrecieve == content_length)
            break;
    }
    
	if(m_store_pos != IN_MEM)
    	close(fd);
	if(buf)
	    x_free(buf);
}
i32 yw_http_download_client::goondownload(char* path)
{
	s64 file_pos = 0;
	if(m_store_pos != IN_MEM)
		file_pos=FileLocate(path);
	char header[HTTP_HEAD_SIZE] = {0};
	i32 mem_size = 4096;
	if(m_store_pos == IN_MEM)
	{
			sprintf(header, \
            "GET %s HTTP/1.1\r\n"\
            "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"\
            "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537(KHTML, like Gecko) Chrome/47.0.2526Safari/537.36\r\n"\
            "Host: %s\r\n"\
	    	"Range: bytes=%d-\r\n" \
            "Connection: keep-alive\r\n"\
            "\r\n"\
        ,m_resq_file_name, m_host,m_total_size);
	}
	else
	{
	sprintf(header, \
            "GET %s HTTP/1.1\r\n"\
            "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"\
            "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537(KHTML, like Gecko) Chrome/47.0.2526Safari/537.36\r\n"\
            "Host: %s\r\n"\
	    "Range: bytes=%lld-\r\n" \
            "Connection: keep-alive\r\n"\
            "\r\n"\
        ,m_resq_file_name, m_host,file_pos);
	}
		m_client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	    if (m_client_socket < 0)
	    {
		X_PRINT("套接字创建失败: %d\n", m_client_socket);
		m_download_status = DOWNLOAD_FAIL;
		checkStausChanged(m_old_download_status,m_download_status);
		return -1;
	    }
	    //创建IP地址结构体
	    struct sockaddr_in addr;
	    memset(&addr, 0, sizeof(addr));
	    addr.sin_family = AF_INET;
	    addr.sin_addr.s_addr = inet_addr(m_ip_addr);
	    addr.sin_port = htons(m_port);
	    X_PRINT("m_ip_addr=%s m_port=%d\n",m_ip_addr,m_port);
	    //连接远程主机
	  //  puts("4: 正在连接远程主机...");
	    i32 res = connect(m_client_socket, (struct sockaddr *) &addr, sizeof(addr));
	    if (res == -1)
	    {
		//printf("连接远程主机失败, error: %d\n", res);
		m_download_status = DOWNLOAD_FAIL;
		checkStausChanged(m_old_download_status,m_download_status);
		return -1;
	    }
	//puts("正在发送http下载请求...");
        write(m_client_socket, header, strlen(header));//writeϵͳ调用, 将请求header发送给服务器
	char *buf = (char *) x_malloc(mem_size * sizeof(char));
	char *response = (char *) x_malloc(mem_size * sizeof(char));
	struct HTTP_RES_HEADER resp;
	get_head_info(m_client_socket,buf,response,&resp);
	if(m_store_pos !=IN_MEM)
		redownload(m_client_socket,path,resp.content_length);
	else
		redownload(m_client_socket,m_fileAddr ,resp.content_length);
	if(buf)
	   x_free(buf);
	if(response)
	   x_free(response);
	return 0;
}
HttpDownLoadStatus yw_http_download_client::getDownloadStatus()
{
	return m_download_status;
}
f32 yw_http_download_client::getDownloadPer()
{
	return m_download_per;
}
i32 yw_http_download_client::setDownloadResultNotify(DownloadResultNotify_Cb p_result_notify)
{	
    if(p_result_notify)
    {
	    m_result_notify=p_result_notify;
		return 0;
    }
	else
		return -1;
}
i32 yw_http_download_client::setSaveInMemory(SaveInMemory_Cb p_save_in_memory)
{
    if(p_save_in_memory)
    {
	    m_save_in_memory = p_save_in_memory;
		return 0;
    }
	else
	{
	    return -1;
	}
}
i32 yw_http_download_client::GetContentLength()
{
	X_PRINT("m_content_length=%d\n",m_content_length);
	return m_content_length;
}
#endif
