#include "stdafx.h"
#ifdef _SUPPORT_HTTP_DOWNLOAD
#include <stdlib.h>
#include <stdio.h>
#include "x_http_client.h"
#include <string.h>//å­—ç¬¦ä¸²å¤„ç†
#include <sys/socket.h>//å¥—æ¥å­—
#include <arpa/inet.h>//ipåœ°å€å¤„ç†
#include <fcntl.h>//openç³»ç»Ÿè°ƒç”¨
#include <unistd.h>//writeç³»ç»Ÿè°ƒç”¨
#include <netdb.h>//æŸ¥è¯¢DNS
#include <sys/stat.h>//statç³»ç»Ÿè°ƒç”¨è·å–æ–‡ä»¶å¤§å°
#include <sys/time.h>//è·å–ä¸‹è½½æ—¶é—´
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
    /*é€šè¿‡urlè§£æå‡ºåŸŸå, ç«¯å£, ä»¥åŠæ–‡ä»¶å**/
    i32 j = 0;
    i32 start = 0;
    *port = 80;
    const char *patterns[] = {"http://", "https://", NULL};

    for (i32 i = 0; patterns[i]; i++)//åˆ†ç¦»ä¸‹è½½åœ°å€ä¸­çš„httpåè®®
        if (strncmp(m_url, patterns[i], strlen(patterns[i])) == 0)
            start = strlen(patterns[i]);

    //è§£æåŸŸå, è¿™é‡Œå¤„ç†æ—¶åŸŸååé¢çš„ç«¯å£å·ä¼šä¿ç•™
    for (i32 i = start; m_url[i] != '/' && m_url[i] != '\0'; i++, j++)
        host[j] = m_url[i];
    host[j] = '\0';

    //è§£æç«¯å£å·, å¦‚æœæ²¡æœ‰, é‚£ä¹ˆè®¾ç½®ç«¯å£ä¸º80
    char *pos = strstr(host, ":");
    if (pos)
        sscanf(pos, ":%d", port);

    //åˆ é™¤åŸŸåç«¯å£å·
    for (i32 i = 0; i < (i32)strlen(host); i++)
    {
        if (host[i] == ':')
        {
            host[i] = '\0';
            break;
        }
    }
    //è·å–ä¸‹è½½æ–‡ä»¶å
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
    /*è·å–å“åº”å¤´çš„ä¿¡æ¯*/
    struct HTTP_RES_HEADER resp;

    char *pos = strstr(response, "HTTP/");
    if (pos)//è·å–è¿”å›ä»£ç 
        sscanf(pos, "%*s %d", &resp.status_code);

    pos = strstr(response, "Content-Type:");
    if (pos)//è·å–è¿”å›æ–‡æ¡£ç±»å‹
        sscanf(pos, "%*s %s", resp.content_type);

    pos = strstr(response, "Content-Length:");
    if (pos)//è·å–è¿”å›æ–‡æ¡£é•¿åº¦
        sscanf(pos, "%*s %lld", &resp.content_length);

    return resp;
}

void yw_http_download_client::get_ip_addr(char *host_name, char *ip_addr)
{
    /*é€šè¿‡åŸŸåå¾—åˆ°ç›¸åº”çš„ipåœ°å€*/
    struct hostent *host = gethostbyname(host_name);//æ­¤å‡½æ•°å°†ä¼šè®¿é—®DNSæœåŠ¡å™¨
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
    /*ç”¨äºæ˜¾ç¤ºä¸‹è½½è¿›åº¦æ¡*/
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
    //é€šè¿‡ç³»ç»Ÿè°ƒç”¨ç›´æ¥å¾—åˆ°æ–‡ä»¶çš„å¤§å°
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
    /*ä¸‹è½½æ–‡ä»¶å‡½æ•°*/
    s64 hasrecieve = 0;//è®°å½•å·²ç»ä¸‹è½½çš„é•¿åº¦
    struct timeval t_start, t_end;//è®°å½•ä¸€æ¬¡è¯»å–çš„æ—¶é—´èµ·ç‚¹å’Œç»ˆç‚¹, è®¡ç®—é€Ÿåº¦
    i32 mem_size = 8192;//ç¼“å†²åŒºå¤§å°8K
    i32 buf_len = mem_size;//ç†æƒ³çŠ¶æ€æ¯æ¬¡è¯»å–8Kå¤§å°çš„å­—èŠ‚æµ
    i32 len=0;
	i32 fd=0;
    m_download_status = ON_DOWNLOAD;
	checkStausChanged(m_old_download_status,m_download_status);
    //åˆ›å»ºæ–‡ä»¶æè¿°ç¬¦
    if(m_store_pos !=IN_MEM)
    {
	    fd= open(file_name, O_CREAT | O_WRONLY , S_IRWXG | S_IRWXO | S_IRWXU);
	    if (fd < 0)
	    {
	    	printf("create file faile!\n");	
	        //printf("æ–‡ä»¶åˆ›å»ºå¤±è´¥!\n");
			m_download_status = DOWNLOAD_FAIL;
			checkStausChanged(m_old_download_status,m_download_status);
			return ;
		}
    }
    char *buf = (char *) x_malloc(mem_size * sizeof(char));
	memset(buf,0,sizeof(buf));
    //ä»å¥—æ¥å­—æµä¸­è¯»å–æ–‡ä»¶æµ
    s64 diff = 0;
    i32 prelen = 0;
    f64 speed =0;
    i32 fail_count=0;
	struct timeval outTime;
	outTime.tv_sec = 5;   //è®¾ç½®ç­‰å¾…æ—¶é—´ä¸º1s
	outTime.tv_usec = 0; //æ¯«ç§’
	fd_set fdread;
    while (hasrecieve < content_length)
    {
        gettimeofday(&t_start, NULL ); //è·å–å¼€å§‹æ—¶é—´

	   FD_ZERO(&fdread);
	   FD_SET(client_socket, &fdread); //sessionSockä¸ºä¹‹å‰åˆ›å»ºçš„ä¼šè¯å¥—æ¥å­—
	 
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
	gettimeofday(&t_end, NULL ); //è·å–ç»“æŸæ—¶é—´
	
        hasrecieve += len;//æ›´æ–°å·²ç»ä¸‹è½½çš„é•¿åº¦
	
        //è®¡ç®—é€Ÿåº¦
        if (t_end.tv_usec - t_start.tv_usec >= 0 &&  t_end.tv_sec - t_start.tv_sec >= 0)
            diff += 1000000 * ( t_end.tv_sec - t_start.tv_sec ) + (t_end.tv_usec - t_start.tv_usec);//us

        if (diff >= 1000000)//å½“ä¸€ä¸ªæ—¶é—´æ®µå¤§äº1s=1000000usæ—¶, è®¡ç®—ä¸€æ¬¡é€Ÿåº¦
        {
            speed = (f64)(hasrecieve - prelen) / (f64)diff * (1000000.0 / 1024.0);
            prelen = hasrecieve;//æ¸…é›¶ä¸‹è½½é‡
            diff = 0;//æ¸…é›¶æ—¶é—´æ®µé•¿åº¦
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
		//printf("1: ÕıÔÚ½âÎöÏÂÔØµØÖ·..."); 
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
			//printf("2: ÕıÔÚ»ñÈ¡Ô¶³Ì·şÎñÆ÷IPµØÖ·...");
			get_ip_addr(host, ip_addr);//µ÷ÓÃº¯ÊıÍ¬·ÃÎÊDNS·şÎñÆ÷»ñÈ¡Ô¶³ÌÖ÷»úµÄIPs
			if (strlen(ip_addr) == 0)
				{
					printf("err: can not get server ip addr ,please check url...\n");
				 // printf("´íÎó: ÎŞ·¨»ñÈ¡µ½Ô¶³Ì·şÎñÆ÷µÄIPµØÖ·, Çë¼ì²éÏÂÔØµØÖ·µÄÓĞĞ§ĞÔ\n");
					m_download_status = DOWNLOAD_FAIL;
					checkStausChanged(m_old_download_status,m_download_status);
				  return -1;
				}
			strncpy(m_host,host,strlen(host));
			strncpy(m_ip_addr,ip_addr,strlen(ip_addr));
			m_port=port;
	#if 0
			puts("\n>>>>ÏÂÔØµØÖ·½âÎö³É¹¦<<<<");
				printf("\tÏÂÔØµØÖ·: %s\n", m_url);
				printf("\tÔ¶³ÌÖ÷»ú: %s\n", host);
				printf("\tIP µØ Ö·: %s\n", ip_addr);
				printf("\tÖ÷»úPORT: %d\n", port);
			printf("\t ÎÄ¼şÃû : %s\n\n", file_name);
	#endif 
			X_PRINT("\n download ip addr analysis success \n");
			X_PRINT("\t url=%s\n",m_url);
			X_PRINT("\t host=%s \n",host);
			X_PRINT("\t server ip=%s\n",ip_addr);
			X_PRINT("\t server port=%d\n",port);
			X_PRINT("\t download file name =%s\n",file_name);
			
			
			//ÉèÖÃhttpÇëÇóÍ·ĞÅÏ¢
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
				//puts("3: ´´½¨ÍøÂçÌ×½Ó×Ö...");
				i32 client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
				if (client_socket < 0)
				{
					printf("create socket fail...\n");
					//printf("Ì×½Ó×Ö´´½¨Ê§°Ü: %d\n", client_socket);
					m_download_status = DOWNLOAD_FAIL;
					checkStausChanged(m_old_download_status,m_download_status);
					return -1;
				}
				m_client_socket = client_socket;
				//´´½¨IPµØÖ·½á¹¹Ìå
				struct sockaddr_in addr;
				memset(&addr, 0, sizeof(addr));
				addr.sin_family = AF_INET;
				addr.sin_addr.s_addr = inet_addr(ip_addr);
				addr.sin_port = htons(port);
		
				//Á¬½ÓÔ¶³ÌÖ÷»ú
				printf("4.on connect server...\n");
			   // puts("4: ÕıÔÚÁ¬½ÓÔ¶³ÌÖ÷»ú...");
				i32 res = connect(client_socket, (struct sockaddr *) &addr, sizeof(addr));
				if (res == -1)
				{
				printf("connect server fail , error:%d\n",res);
			//	printf("Á¬½ÓÔ¶³ÌÖ÷»úÊ§°Ü, error: %d\n", res);
				m_download_status = DOWNLOAD_FAIL;
				checkStausChanged(m_old_download_status,m_download_status);
				return -1;
				}
				printf("5.send http request...\n");
			//puts("5: ÕıÔÚ·¢ËÍhttpÏÂÔØÇëÇó...");
				write(client_socket, header, strlen(header));//writeÏµÍ³µ÷ÓÃ, ½«ÇëÇóheader·¢ËÍ¸ø·şÎñÆ÷
				i32 mem_size = 4096;
				 i32 length = 0;
				 i32 len;
				 char *buf = (char *) x_malloc(mem_size * sizeof(char));
				 char *response = (char *) x_malloc(mem_size * sizeof(char));
		
			//Ã¿´Îµ¥¸ö×Ö·û¶ÁÈ¡ÏìÓ¦Í·ĞÅÏ¢
			//puts("6: ÕıÔÚ½âÎöhttpÏìÓ¦Í·...");
			printf("analysis http head....\n");
			while ((len = read(client_socket, buf, 1)) != 0)
			{
				if (length + len > mem_size)
				{
					//¶¯Ì¬ÄÚ´æÉêÇë, ÒòÎªÎŞ·¨È·¶¨ÏìÓ¦Í·ÄÚÈİ³¤¶È
					mem_size *= 2;
					temp= (char *) realloc(response, sizeof(char) * mem_size);
					if (temp == NULL)
					{
						X_ERROR("realloc memory fail\n");
						//printf("¶¯Ì¬ÄÚ´æÉêÇëÊ§°Ü\n");
						m_download_status = DOWNLOAD_FAIL;
						checkStausChanged(m_old_download_status,m_download_status);
						return -1;
					}
					response = temp;
				}
		
				buf[len] = '\0';
				strcat(response, buf);
		
				//ÕÒµ½ÏìÓ¦Í·µÄÍ·²¿ĞÅÏ¢
				i32 flag = 0;
				for (i32 i = strlen(response) - 1; response[i] == '\n' || response[i] == '\r'; i--, flag++);
				if (flag == 4)//Á¬ĞøÁ½¸ö»»ĞĞºÍ»Ø³µ±íÊ¾ÒÑ¾­µ½´ïÏìÓ¦Í·µÄÍ·Î², ¼´½«³öÏÖµÄ¾ÍÊÇĞèÒªÏÂÔØµÄÄÚÈİ
					break;
		
				length += len;
			}
		
			struct HTTP_RES_HEADER resp = parse_header(response);
			X_PRINT("\t >>>>http answer success...\n");
			X_PRINT("\t >>>>answer code :%d\n",resp.status_code);
		   // printf("\n>>>>httpÏìÓ¦Í·½âÎö³É¹¦:<<<<\n");
		   // printf("\tHTTPÏìÓ¦´úÂë: %d\n", resp.status_code);
			if (resp.status_code != 200)
			{
				X_WARN("file can not download!!! code =%d\n",resp.status_code);
				//printf("ÎÄ¼şÎŞ·¨ÏÂÔØ, Ô¶³ÌÖ÷»ú·µ»Ø: %d\n", resp.status_code);
				m_download_status = DOWNLOAD_FAIL;
				checkStausChanged(m_old_download_status,m_download_status);
				return 0;
			}
			X_PRINT("start download.....\n");
			//printf();
		   // printf("\tHTTPÎÄµµÀàĞÍ: %s\n", resp.content_type);
		   // printf("\tHTTPÖ÷Ìå³¤¶È: %ld×Ö½Ú\n\n", resp.content_length);
		   // printf("7: ¿ªÊ¼ÎÄ¼şÏÂÔØ...\n");
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
				   // printf("\nÎÄ¼ş%sÏÂÔØ³É¹¦! ^_^\n\n", file_name);
					m_download_status = DOWNLOAD_SUCCESS;
					checkStausChanged(m_old_download_status,m_download_status);
				}
				else
				{
					m_download_status = DOWNLOAD_PAUSE;
					checkStausChanged(m_old_download_status,m_download_status);
	
					//remove(file_name);
				//	  printf("\nÎÄ¼şÏÂÔØÖĞÓĞ×Ö½ÚÈ±Ê§, ÏÂÔØÊ§°Ü, ÇëÖØÊÔ!\n\n");
				}
			}
			else
			{
				 if (resp.content_length == m_total_size)
				{
				   // printf("\nÎÄ¼ş%sÏÂÔØ³É¹¦! ^_^\n\n", file_name);
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
					//printf("\nÎÄ¼şÏÂÔØÖĞÓĞ×Ö½ÚÈ±Ê§, ÏÂÔØÊ§°Ü, ÇëÖØÊÔ!\n\n");
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
							//printf("\nÎÄ¼ş%sÏÂÔØ³É¹¦! ^_^\n\n", file_name);
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
						   // printf("\nÎÄ¼şÏÂÔØÖĞÓĞ×Ö½ÚÈ±Ê§, ÏÂÔØÊ§°Ü, ÇëÖØÊÔ!\n\n");
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
					//printf("\nÎÄ¼şÏÂÔØÖĞÓĞ×Ö½ÚÈ±Ê§, ÏÂÔØÊ§°Ü, ÇëÖØÊÔ!\n\n");
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
					//printf("\nÎÄ¼şÏÂÔØÖĞÓĞ×Ö½ÚÈ±Ê§, ÏÂÔØÊ§°Ü, ÇëÖØÊÔ!\n\n");
				}
		
			}
			shutdown(client_socket, 2);//¹Ø±ÕÌ×½Ó×ÖµÄ½ÓÊÕºÍ·¢ËÍ
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
    puts("6: æ­£åœ¨è§£æhttpå“åº”å¤´...");
    while ((len = read(client_socket, buf, 1)) != 0)
    {
        if (length + len > mem_size)
        {
            //åŠ¨æ€å†…å­˜ç”³è¯·, å› ä¸ºæ— æ³•ç¡®å®šå“åº”å¤´å†…å®¹é•¿åº¦
            mem_size *= 2;
            char * temp = (char *) realloc(response, sizeof(char) * mem_size);
            if (temp == NULL)
            {
            	X_WARN("realloc memory fail...\n")	;
//                printf("åŠ¨æ€å†…å­˜ç”³è¯·å¤±è´¥\n");
				m_download_status = DOWNLOAD_FAIL;
				checkStausChanged(m_old_download_status,m_download_status);
               return -1;
            }
            response = temp;
        }

        buf[len] = '\0';
        strcat(response, buf);

        //æ‰¾åˆ°å“åº”å¤´çš„å¤´éƒ¨ä¿¡æ¯
        i32 flag = 0;
        for (i32 i = strlen(response) - 1; response[i] == '\n' || response[i] == '\r'; i--, flag++);
        if (flag == 4)//è¿ç»­ä¸¤ä¸ªæ¢è¡Œå’Œå›è½¦è¡¨ç¤ºå·²ç»åˆ°è¾¾å“åº”å¤´çš„å¤´å°¾, å³å°†å‡ºç°çš„å°±æ˜¯éœ€è¦ä¸‹è½½çš„å†…å®¹
            break;

        length += len;
    }
    struct HTTP_RES_HEADER  tmp_resp;
    tmp_resp = parse_header(response);
    memcpy(resp,&tmp_resp,sizeof(HTTP_RES_HEADER));
  //  printf("\n>>>>httpå“åº”å¤´è§£ææˆåŠŸ:<<<<\n");

 //   printf("\tHTTPå“åº”ä»£ç : %d\n", resp->status_code);
    if (resp->status_code != 200)
    {
  //      printf("æ–‡ä»¶æ— æ³•ä¸‹è½½, è¿œç¨‹ä¸»æœºè¿”å›: %d\n", resp->status_code);
        return 0;
    }
 //   printf("\tHTTPæ–‡æ¡£ç±»å‹: %s\n", resp->content_type);
 //   printf("\tHTTPä¸»ä½“é•¿åº¦: %ldå­—èŠ‚\n\n", resp->content_length);
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
    /*ä¸‹è½½æ–‡ä»¶å‡½æ•°*/
    s64 hasrecieve = 0;//è®°å½•å·²ç»ä¸‹è½½çš„é•¿åº¦
    struct timeval t_start, t_end;//è®°å½•ä¸€æ¬¡è¯»å–çš„æ—¶é—´èµ·ç‚¹å’Œç»ˆç‚¹, è®¡ç®—é€Ÿåº¦
    i32 mem_size = 8192;//ç¼“å†²åŒºå¤§å°8K
    i32 buf_len = mem_size;//ç†æƒ³çŠ¶æ€æ¯æ¬¡è¯»å–8Kå¤§å°çš„å­—èŠ‚æµ
    i32 len =0;
	 i32 fd=0;
    m_download_status = ON_DOWNLOAD;
    //åˆ›å»ºæ–‡ä»¶æè¿°ç¬¦
    if(m_store_pos != IN_MEM)
    {
	   fd= open(file_name, O_CREAT | O_WRONLY | O_APPEND, S_IRWXG | S_IRWXO | S_IRWXU);
	    if (fd < 0)
	    {
	      //  printf("æ–‡ä»¶åˆ›å»ºå¤±è´¥!\n");
			X_WARN("open file =%s faile ...\n",file_name);
			m_download_status = DOWNLOAD_FAIL;
			checkStausChanged(m_old_download_status,m_download_status);
	       return;
	    }
    }
    char *buf = (char *) x_malloc(mem_size * sizeof(char));

    //ä»å¥—æ¥å­—æµä¸­è¯»å–æ–‡ä»¶æµ
    s64 diff = 0;
    i32 prelen = 0;
    f64 speed=0;
    u32 fail_count =5;
	struct timeval outTime;
	outTime.tv_sec = 5;   //è®¾ç½®ç­‰å¾…æ—¶é—´ä¸º1s
	outTime.tv_usec = 0; //æ¯«ç§’
	fd_set fdread;
    while (hasrecieve < content_length)
    {
        gettimeofday(&t_start, NULL ); //è·å–å¼€å§‹æ—¶é—´

	   FD_ZERO(&fdread);
	   FD_SET(client_socket, &fdread); //sessionSockä¸ºä¹‹å‰åˆ›å»ºçš„ä¼šè¯å¥—æ¥å­—
	 
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
        gettimeofday(&t_end, NULL ); //è·å–ç»“æŸæ—¶é—´

        hasrecieve += len;//æ›´æ–°å·²ç»ä¸‹è½½çš„é•¿åº¦

        //è®¡ç®—é€Ÿåº¦
        if (t_end.tv_usec - t_start.tv_usec >= 0 &&  t_end.tv_sec - t_start.tv_sec >= 0)
            diff += 1000000 * ( t_end.tv_sec - t_start.tv_sec ) + (t_end.tv_usec - t_start.tv_usec);//us

        if (diff >= 1000000)//å½“ä¸€ä¸ªæ—¶é—´æ®µå¤§äº1s=1000000usæ—¶, è®¡ç®—ä¸€æ¬¡é€Ÿåº¦
        {
            speed = (f64)(hasrecieve - prelen) / (f64)diff * (1000000.0 / 1024.0);
            prelen = hasrecieve;//æ¸…é›¶ä¸‹è½½é‡
            diff = 0;//æ¸…é›¶æ—¶é—´æ®µé•¿åº¦
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
		X_PRINT("Ì×½Ó×Ö´´½¨Ê§°Ü: %d\n", m_client_socket);
		m_download_status = DOWNLOAD_FAIL;
		checkStausChanged(m_old_download_status,m_download_status);
		return -1;
	    }
	    //´´½¨IPµØÖ·½á¹¹Ìå
	    struct sockaddr_in addr;
	    memset(&addr, 0, sizeof(addr));
	    addr.sin_family = AF_INET;
	    addr.sin_addr.s_addr = inet_addr(m_ip_addr);
	    addr.sin_port = htons(m_port);
	    X_PRINT("m_ip_addr=%s m_port=%d\n",m_ip_addr,m_port);
	    //Á¬½ÓÔ¶³ÌÖ÷»ú
	  //  puts("4: ÕıÔÚÁ¬½ÓÔ¶³ÌÖ÷»ú...");
	    i32 res = connect(m_client_socket, (struct sockaddr *) &addr, sizeof(addr));
	    if (res == -1)
	    {
		//printf("Á¬½ÓÔ¶³ÌÖ÷»úÊ§°Ü, error: %d\n", res);
		m_download_status = DOWNLOAD_FAIL;
		checkStausChanged(m_old_download_status,m_download_status);
		return -1;
	    }
	//puts("ÕıÔÚ·¢ËÍhttpÏÂÔØÇëÇó...");
        write(m_client_socket, header, strlen(header));//writeÏµÍ³µ÷ÓÃ, ½«ÇëÇóheader·¢ËÍ¸ø·şÎñÆ÷
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

