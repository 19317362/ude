#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct _yw_emmc_rw_cmp{
	unsigned long long rw_count;
	unsigned long long start_addr;
	unsigned int rw_blks;
	unsigned int max_wcnt;
	unsigned long long sum_wblks;
}yw_emmc_rw_cmp_t;

#if defined(ON_MTK_MT6735_ANDROID) && defined(WIN32)
	#define _IOWR(a,b,c)	0
#endif /* WIN32 */

#define DEV_EMMC "/dev/ywemmc"
#define YWEMMC_TYPE_IOC          'E'
#define YWEMMC_GET_RW_CMP      _IOWR(YWEMMC_TYPE_IOC,3,int)

#define MAX_DEV_NAME_SIZE  32

	//typedef unsigned int x_tick_t;
	//����к���, ��ʵ���� stdarm.cpp��
XPLATFORM_API u32 X_GetTickCount();			// 10�������
XPLATFORM_API u64 X_GetMsTickCount();		// �������
XPLATFORM_API u64 X_GetUsTickCount();		// ΢�����


#define MC_X_GET_TICK()  X_GetTickCount()
#define MC_X_GET_MS_TICK()  X_GetMsTickCount()


#define SOCKET_NOT_GET_TMP_RES 11
#define SOCKET_EINPROGRESS 115
#define TRYNUM 1

#define X_ASSERT	assert

#ifdef WIN32
#include <direct.h>
#define X_ACCESS _access
#define MKDIR(a) _mkdir(( a ))
#else
#define X_ACCESS access
#define MKDIR(a) mkdir(( a ), 0755)
#endif

#ifdef WIN32
#define ACCESS   _access
#else
#define ACCESS   access
#endif


#ifndef WIN32
#include <stdarg.h>
#endif

XPLATFORM_API int x_vsnprintf(char*pp_DestBuffer, int vp_BufferLen, const char* pp_Fmt, va_list  vp);
XPLATFORM_API int x_snprintf(char*pp_DestBuffer, int vp_BufferLen, const char* pp_Fmt, ...);

//��Ҫֱ���ú���. �������������
//us�� SLEEP
#define MC_X_SYS_US_SLEEP(n_US)	x_sys_usleep(n_US)
//ms�� SLEEP
#define MC_X_SYS_MS_SLEEP(n_MS)	x_sys_msleep(n_MS)

XPLATFORM_API void x_sys_usleep(u32 vp_us);
XPLATFORM_API void x_sys_msleep(u32 vp_ms);

// �����ڴ�й¶
//#define DEBUG_MEMORY_LEAKS
#ifdef DEBUG_MEMORY_LEAKS
//#define DUMP_MEM_REPORT_FREQUENCY (30*60) //ÿ��30�������һ���ڴ�������  
#define DUMP_MEM_REPORT_FREQUENCY 60 //ÿ��һ�������һ���ڴ�������  
//#define DUMP_MEM_REPORT_FREQUENCY 10 //����ʱ��10�����һ���ڴ�������
#endif


//#define FUNC_DEBUG_MEM
#ifdef FUNC_DEBUG_MEM
	#define x_malloc(size) xdmalloc(__FILE__, __LINE__, (size))
	#define x_free(memblock) xdfree(__FILE__, __LINE__, (memblock))
#ifdef DEBUG_MEMORY_LEAKS
	#define x_memcpy(dstmemblock, srcmemblock, size)  ymemcpy((dstmemblock), (srcmemblock), (size))
	#define x_memmove(dstmemblock, srcmemblock, size) memmove((dstmemblock), (srcmemblock), (size))
	#define x_mem_null_assert(memblock)
	#define x_mem_check()
	#define x_realloc(memblock, size) realloc((memblock), (size))
	#define x_mem_check_ex(bgn, end)
	#define x_memset(dstmemblock, srcmemblock, size) memset((dstmemblock), (srcmemblock), (size))
#else
	#define x_memcpy(dstmemblock, srcmemblock, size) xdmemcpy(__FILE__, __LINE__, (dstmemblock), (srcmemblock), (size))
	#define x_memmove(dstmemblock, srcmemblock, size) xdmemmove(__FILE__, __LINE__, (dstmemblock), (srcmemblock), (size))
	#define x_mem_null_assert(memblock)  x_dmem_null_assert(__FILE__, __LINE__, (memblock))
	#define x_mem_check()  x_dmem_check(__FILE__, __LINE__)
	#define x_realloc(memblock, size)   xdrealloc(__FILE__, __LINE__, (memblock), (size))
	#define x_mem_check_ex(bgn, end)  x_dmem_check_ex(__FILE__, __LINE__, (bgn), (end))
	#define x_memset(dstmemblock, data, size) xdmemset(__FILE__, __LINE__, (dstmemblock), (data), (size))
#endif
#else
	#define x_malloc(size)  malloc( (size))
	#define x_free(memblock) free((memblock))
	#define x_memcpy(dstmemblock, srcmemblock, size)  ymemcpy((dstmemblock), (srcmemblock), (size))
	#define x_memmove(dstmemblock, srcmemblock, size) memmove((dstmemblock), (srcmemblock), (size))
	#define x_mem_null_assert(memblock)
	#define x_mem_check()
	#define x_realloc(memblock, size) realloc((memblock), (size))
	#define x_mem_check_ex(bgn, end)
	#define x_memset(dstmemblock, srcmemblock, size) memset((dstmemblock), (srcmemblock), (size))
#endif


//#define FUNC_DEBUG_SOCKET
#ifdef FUNC_DEBUG_SOCKET
	#define x_closesocket(_Socket)		xdclosesocket(__FILE__, __LINE__, (_Socket))
#else
	#define x_closesocket(_Socket)		closesocket((_Socket))
#endif

#define FUNC_SECURET_MEM
#ifdef FUNC_SECURET_MEM
#define x_smemcpy(dstmemblock, srcmemblock, dstsize, srcsize)   xsmemcpy(__FILE__, __LINE__, (dstmemblock), (srcmemblock), (dstsize), (srcsize))
#else
#define x_smemcpy(dstmemblock, srcmemblock, dstsize, srcsize)  x_memcpy((dstmemblock), (srcmemblock), (srcsize))
#endif



//epoll��д�¼������Բ���
//#define FUNC_EPOLL_EVENTVALID_CHECK
#ifdef FUNC_EPOLL_EVENTVALID_CHECK
#define	x_epoll_event_valid_check(value, threshold) {\
													if ((value) <= (threshold))\
													{\
														printf("epollevent:%u <= %u %s,%d\n", (value), (threshold), __FILE__, __LINE__);\
													}\
												}\

#else
	#define	x_epoll_event_valid_check(value, threshold)
#endif

//epoll�¼���������ʱ�����Բ���
#ifndef WIN32
//#define FUNC_EPOLL_EVENTTIME_CHECK
#endif

#define EVENTTIME_THRESHOLD 1000
#ifdef FUNC_EPOLL_EVENTTIME_CHECK
	#define x_epoll_event_time_begin(bgntv)   {\
									gettimeofday(&bgntv, 0); \
							      }\

	#define x_epoll_event_time_end(bgntv) {\
	    struct timeval tv; \
		gettimeofday(&tv, 0); \
		int vl_diff = (tv.tv_sec * 1000 + tv.tv_usec / 1000) - (bgntv.tv_sec * 1000 + bgntv.tv_usec / 1000);\
		if (vl_diff > (EVENTTIME_THRESHOLD))\
		{\
			printf("epolltime:%u > %u %s, %s, %d\n", vl_diff, EVENTTIME_THRESHOLD, __FILE__, __func__, __LINE__); \
		}\
	}\


	#define x_epoll_event_time_threshold_end(bgntv, threshold) {\
		struct timeval tv; \
		gettimeofday(&tv, 0); \
		int vl_diff = (tv.tv_sec * 1000 + tv.tv_usec / 1000) - (bgntv.tv_sec * 1000 + bgntv.tv_usec / 1000);\
		if (vl_diff > (threshold))\
		{\
		printf("epolltime:%u > %u %s, %s, %d\n", vl_diff, (threshold), __FILE__, __func__, __LINE__); \
		}\
	}\

#else
	#define x_epoll_event_time_begin(bgntv)
	#define x_epoll_event_time_end(bgntv)
	#define x_epoll_event_time_threshold_end(bgntv, threshold)
#endif

 //��ȡemmc�Ĳ���ͳ��
 XPLATFORM_API  int x_get_emmc_opt_statics(yw_emmc_rw_cmp_t *vpp_emmc_cmp);
#ifdef ON_MTK_MT6735_ANDROID
 //��ȡandroid�ն�����
 int property_get(const char *key, char *value, const char *default_value);
 XPLATFORM_API  int x_property_get(const char *key, char *value, const char *default_value);
 //����android�ն�����
 int property_set(const char *key, const char *value);
 XPLATFORM_API  int x_property_set(const char *key, const char *value);
#endif

 typedef char* LPASCII;
#define SPLIT_MAX_SEGMENT   20

 XPLATFORM_API int SplitString(LPASCII pp_String, LPASCII pp_Out[SPLIT_MAX_SEGMENT]);
 XPLATFORM_API int fmt_uboot_ver_string(const char* pp_verStr, x_tm_t* pp_tm);
 XPLATFORM_API x_time_t x_get_build_time_t(const char* pp_Date, const char* pp_Time);
 XPLATFORM_API int x_get_build_time(const char* pp_Date, const char* pp_Time, u8* pp_BuildDate);
 // ����ϵͳ����λreset��
 XPLATFORM_API void x_sys_reboot(const char* notoTitle);

 // ִ��ϵͳ����
 XPLATFORM_API int x_system(const char* notoTitle);

 // ɾ��·�����һ�ֽڵ�'/'��'\\'����.(���򲻴���)
 //		��: "/mnt/usb/" -> "/mnt/usb", ��"./"�����,�������"./"
 // ���þ���:
 //		char strSafePath[128] = {0};
 //		x_getsafepath("/mnt/usb/", sizeof(strSafePath), strSafePath);
 XPLATFORM_API void x_getsafepath(const char* pInputPath, u32 nMaxOutputLen, char* pOutputPath);

// CheckSum����
XPLATFORM_API u32 x_getAddCheckSum(const u8* pp_Begin, u32 uSize, u32 vp_Org);

// �����żУ���.���ֽڡ�
XPLATFORM_API u8 x_getXorChecksumU8(const u8 * pBuf, u32 ulen, u8 vp_InitXor);

// ��������ʾ�Ļ�ȡ��Сֵ
//		nInputLen: �û���Ҫ����
//		nMaxLen: ������󳤶�
// ˵��1: ���������memcpy,memset��,������󿽱�����,�����볤�ȴ������ֵ,�������־��ӡ����
// ˵��2: ��Щ����,ǰ������ֵ���������(���簴�ȳ��и�,ʣ�೤�Ȼ���ڵ��ڳ���,������������),�Ͳ�Ӧ���ñ���,��Ӧ����YW_MIN
#define X_MIN(nInputLen, nMaxLen) x_getMin(nInputLen, nMaxLen, __FILE__, __LINE__, __func__)
XPLATFORM_API u32 x_getMin(u32 nInputLen, u32 nMaxLen, const char* strFile, int nLine, const char* strFunc);

//��ȡָ���������Ľ���ID(PID)
XPLATFORM_API int x_getPidByProcessName(const char* processName, int *Processid);

#ifdef __cplusplus
};
#endif

#ifdef __cplusplus
class XPLATFORM_API x_mmlib
{
public:
	///--- ��Ҫ�Ƴ����� IMX6 �� T4�Ż�ʹ�� CheckStreamType get_video_stream_frame_type
	// ���֡�������У��������֧����������֡��
	static BOOL x_GetPkgFrameType(const u8 * OnePkgBuf_H264, u32 len, u8& frameType1, const u8 *& frameBuff1, u32& frameLen1, u32& startcodeprefixlen1, u8& frameType2, const u8 *& frameBuff2, u32& frameLen2, u32& startcodeprefixlen2, u8& frameType3, const u8 *& frameBuff3, u32& frameLen3, u32& startcodeprefixlen3, u8& frameType4, const u8 *& frameBuff4, u32& frameLen4, u32& startcodeprefixlen4);

	// ��ȡ֡����
	// u32 startcodeprefix_len; !ǰ׺�ֽ���
	// u8 * OneFrameBuf_H264	!֡����ָ��
	// u8 nal_unit_type         ! NALU_TYPE_xxxx
	// u32 len;                 ! ����nal ͷ��nal ���ȣ��ӵ�һ��00000001����һ��000000001�ĳ���
	// u8 Frametype;            ! ֡����  ��Ӧ ATTR_H264E_NALU_XXXX
	static BOOL x_GetFrameType(u32 startcodeprefix_len, const u8 * OneFrameBuf_H264, u8 nal_unit_type, u32 len, u8& Frametype);
	// ��ÿ�ʼ��ǰ׺���ȣ�00 00 01�� 00 00 00 01��
	static BOOL x_GetStartCodePrefixLen(const u8 * pp_MediaData, u32& startcodeprefix_len);
	static bool x_mmst_is_I_frame(const u8 *pp_h264frame, int vp_length);
	static bool x_is_pframe(const u8 *pp_frame, int vp_length);
};
#endif
