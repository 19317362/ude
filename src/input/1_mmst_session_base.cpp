#include "stdarm.h"

#ifdef FUNC_USE_TRANSFER_LIB

#include "mmst_session_base.h"
#ifndef FUNC_MMST_EPOLL_20				// 媒体传输网络版本2.0 使用EPOLL边缘触发 模块化
	#include "tcp_channel.h"
#endif
#include "local_tcp_channel.h"
#ifndef FUNC_MMST_EPOLL_20				// 媒体传输网络版本2.0 使用EPOLL边缘触发 模块化
	#include "udp_channel.h"
#endif
#include "mmst_timer.h"
#include "MediaChCtx.h"
#ifndef WIN32
	#include <netinet/tcp.h>
#endif

#ifdef PROP_PRO_TYPE_IPCSELF_ON				// IPC协议
	#include "mmst_IPCClientSession.h"
	#include "mmst_IPCClient_history_upload.h"
#endif

// 文件号
#define FILE_NO   FILE_D_MMST_SESSION_BASE

#define PACKET_TICK_INTERVAL  1
#define DETECT_READ_EVENT_INTERVAL  (1000*5*60)

#define MMST_CONVERT_TO_HISTORY_CH(CH) ((CH)+MAX_REAL_TRANS_CH)
#define MMST_CONVERT_TO_TRANS_CH(CH)   ((CH)-MAX_REAL_TRANS_CH)
#define MMST_CONVERT_TO_GPS_CH

#define MAX_STORE_CH     (MAX_REAL_TRANS_CH)
#define HIST_GPS_CH_IDX  (MAX_STORE_CH)
#define MC_GET_STORE_CH(V)  (((V)>>4)&0x0F)
#define MC_GET_STORE_TYPE(V)  ((V)&0x0F)

#define  MC_SLEEP(MS) x_sys_msleep(MS)

#if VM_MMST_FLOWCTL_PROP		// 媒体传输流量控制属性
	u32 Session_Base::m_CurrentDayFlow = 0x000;
	u32 Session_Base::m_CurrentMonthFlow = 0x0000;
	u32 Session_Base::m_nTcpPushNum = 0x0000;
	u32 Session_Base::m_nSendNum = 0x0000;
#endif

	Mmst_QueryResultList m_arrQueryList;
const char *format_time(time_t vl_NowTm)
{
	static char sa_data[64];
	static char sa_data2[64];
	static bool sl_redirect_flag;
	char* pp_str;

	if (sl_redirect_flag)
	{
		pp_str = sa_data;
	}
	else
	{
		pp_str = sa_data2;
	}
	sl_redirect_flag = !sl_redirect_flag;

	vl_NowTm += 946656000 + 28800;

	vl_NowTm = getLocalTime(vl_NowTm);
	strftime(pp_str, 30,
		//"It is %M minutes after %I o'clock (%Z)  %A, %B %d 19%y",
		"%Y-%m-%d %H:%M:%S",
		//localtime(&vl_NowTm)
		gmtime(&vl_NowTm)
	);
	return pp_str;
}


#ifdef FUNC_MMST_EPOLL_20				// 媒体传输网络版本2.0 使用EPOLL边缘触发 模块化

// Epoll加事件
void mmstipnet_epool_add(x_epoll_base* instance, int socket, u32 vp_events, int ctx)
{
	GetDvrrThread()->GetEpoll().add(socket, vp_events, ctx, instance);
}

// Epoll修改事件
void mmstipnet_epool_mod(x_epoll_base* instance, int socket, u32 vp_events, int ctx)
{
	GetDvrrThread()->GetEpoll().mod(socket, vp_events, ctx, instance);
}

// Epoll删除事件
void mmstipnet_epool_del(x_epoll_base* instance, int socket, u32 vp_events, int ctx)
{
	GetDvrrThread()->GetEpoll().del(socket, vp_events, ctx, instance);
}

// 获得ip缓冲区
char * mmstipnet_getiprevbuff_opt(int & size)
{
	return LDK_GetCurTmpIpRevBuff(&size);
}

#endif

bool Session_Base::m_bServerMode = false;
extern dvrr::async_io_server *dvrr_get_service(void);
extern int dvrr_get_mq_id(void);

Session_Base::Session_Base(const t_TransferParam* pp_Init)
	: m_bForbidden(false)
	, m_timer_deteck_socket_read(dvrr_get_service())
	, m_timer_send_gps(dvrr_get_service())
	, m_pCurPkg(NULL)
#ifndef FUNC_MMST_EPOLL_20				// 媒体传输网络版本2.0 使用EPOLL边缘触发 模块化
	, m_pMainSocket(NULL)
#endif
{
#ifdef VICIOUS_CIRCLE
	m_VcMaxSize = 0;
	m_Vcp = 0;
	m_Ru = 0.3f;
	m_pVCMap = Session_Base::GetVcMap();
#endif
	m_TransChFlag = 0;
	if (NULL != pp_Init)
	{//Client模式
		m_Param = *pp_Init;
	}
	else
	{//Server模式
		memset(&m_Param, 0, sizeof(m_Param));//
		SetServerMode();
	}
	X_TRACE("m_bServerMode:%d", m_bServerMode);

	for (u8 i = 0; i < MAX_TRANS_CH; i++)
	{
		m_wait_I_frame[i] = true;
#ifndef FUNC_MMST_EPOLL_20				// 媒体传输网络版本2.0 使用EPOLL边缘触发 模块化
		m_clientSock[i] = NULL;
#endif
	}

	for (int i = 0; i < RTP_STREAM_TYPE_CNT; i++)
	{
		for (int j = 0; j < MAX_RCV_CH; j++)
		{
			g_Stm[i][j] = NULL;
		}
	}

	if (pp_Init)
	{
//#ifdef FUNC_IS_IPCV3
//		CMmstMedia::SetMissFrameBufferSizeRange(MMST_ENABLE_MISS_FRAME_BUFFERED_BYTES * 2/MAX_TRANS_CH, MMST_DISABLE_MISS_FRAME_BUFFERED_BYTES * 2/MAX_TRANS_CH, pp_Init->v_AppProtoType);
//#else
		CMmstMedia::SetMissFrameBufferSizeRange(MMST_ENABLE_MISS_FRAME_BUFFERED_BYTES, MMST_DISABLE_MISS_FRAME_BUFFERED_BYTES, pp_Init->v_AppProtoType);
//#endif
	}
#if VM_MMST_FLOWCTL_PROP		// 媒体传输流量控制属性
	m_BandwidthInfo.v_RxBytesPerSecondQos = 100 << 10;
	m_BandwidthInfo.v_TxBytesPerSecondQos = 100 << 10;
	m_flow.Reset();
	m_flow.SetTxLimit(m_BandwidthInfo.v_TxBytesPerSecondQos);
	m_LastFlowTick = 0;
#endif

	if (MMST_RETRY_MAX_CNT == 0)
	{
		MMST_RETRY_MAX_CNT = 32;
	}
	if (MMST_REG_INTERVAL == 0)
	{
		MMST_REG_INTERVAL = 4;
	}
	if (MMST_RESET_MAX_CNT == 0)
	{
		MMST_RESET_MAX_CNT = 4;
	}
	if (MMST_HAND_INTERVAL < 10)
	{
		MMST_HAND_INTERVAL = 10;
	}
	if (MMST_HAND_FAIL_CNT == 0)
	{
		MMST_HAND_FAIL_CNT = 6;
	}
	if (MMST_RESET_INTERVAL == 0)
	{
		MMST_RESET_INTERVAL = 2;
	}
	if (MMST_MAX_SECONDS_WAIT_FOR_ACK == 0)
	{
		MMST_MAX_SECONDS_WAIT_FOR_ACK = 30;
	}
	if (MMST_FLOW_REPORT_INTERVAL < 15)
	{
		MMST_FLOW_REPORT_INTERVAL = 15;
	}
#ifndef WIN32
	//		x_free((void*)pp_Init);
#endif

#ifdef FUNC_IS_IPCV3
	m_State = TS_STATE_CONNECTING;
#else
	if (MC_MMST_IN_TCP())
	{
		m_State = TS_STATE_CONNECTING;
	}
	else
	{
		m_State = TS_STATE_REG;
	}
#endif

	m_LastHBAckTick = 0;
	m_LastHBTxTick = 0;
	//printf("INIT APPID:%08X\n",m_Param.v_APPID);
	m_PackSN = 0;
	m_LastSendTick = 0;
	m_bIntraNet = false;
	m_TxSeconds = 0;
	m_TxBytesTotal = 0;
	m_HeartbeatInterval = MMST_HAND_INTERVAL;

#ifdef MMST_MEM_ANA_FTR
	g_stmCnt.Reset();
#endif
	m_I = 0;
#ifdef FUMC_MMST_BALANCE_SEND
	memset(m_J, 0, sizeof(m_J));
#else
	m_J = 0;
#endif
	m_IsRedirect = false;
	m_IsHeartAddiGps = false;
	SetRegFlag(MMST_CCF_NORMAL);
	m_can_stopchannel = false;
	if (MC_MMST_IN_TCP())
	{
		CMmstMedia::m_bTcpMode = true;
	}
	else
	{
		CMmstMedia::m_bTcpMode = false;
	}
	memset(&g_MmstQueryStatics, 0, sizeof(g_MmstQueryStatics));

	m_timer_deteck_socket_read.bind(this, &Session_Base::OnDectecSocketReadEvent);

	m_pDownMediaDataBuff = NULL;
	m_DownMediaDataBuffSize = 0;
	m_DownMediaDataOffset = 0;
	m_DownMediaDataRestlen = 0;
	m_audioFormat = 0;
	m_RealTxBytesWaiting = 0;
	m_HisTxBytesWaiting = 0;

#ifndef FUNC_MMST_EPOLL_20				// 媒体传输网络版本2.0 使用EPOLL边缘触发 模块化
	m_clientsockwait = false;
#endif
}

Session_Base::~Session_Base()
{
	//要释放内存

#ifndef FUNC_MMST_EPOLL_20				// 媒体传输网络版本2.0 使用EPOLL边缘触发 模块化
	while (!m_otherSocket.empty())
	{
		t_clientSocket::iterator it = m_otherSocket.begin();
		CloseChannel((*it), RSC_NORMAL);//关闭相关的传输.
	}

	//平台错误下发导致数据传输走主通道，异常断开需要处理
	CloseChannel(m_pMainSocket, RSC_NORMAL);
	m_pMainSocket = NULL;
#endif

#ifndef FUNC_MMST_EPOLL_20				// 媒体传输网络版本2.0 使用EPOLL边缘触发 模块化
	for (u8 i = 0; i < MAX_TRANS_CH; i++)
	{
		m_clientSock[i] = NULL;
	}
#endif

	if (m_pDownMediaDataBuff)
	{
		x_free(m_pDownMediaDataBuff);
		m_pDownMediaDataBuff = NULL;
	}
}


u32 Session_Base::GetBufferedStreamCnt()
{
	u8 i;
	int j;
	u32 vl_Cnt = 0;

	for (i = 0; i < MAX_TRANS_CH; ++i)
	{
		if (m_aCh[i].HasAnySending())
		{
			for (j = 0; j < RTP_STREAM_TYPE_CNT; ++j)
			{
				vl_Cnt += m_aCh[i].m_stm[j].GetBufferedStreamCnt();
			}
		}
	}
	return vl_Cnt;
}


void Session_Base::OnSysError(const char* pp_Err, int ec)
{
#define MAX_DESC_LEN 256
	char pl_Desc[MAX_DESC_LEN];
	memset(pl_Desc, 0, sizeof(pl_Desc));
	if (ec == 0)
	{
		ec = errno;
		if (NULL != pp_Err)
		{
			sprintf(pl_Desc, "%s:%s", pp_Err, strerror(ec));
		}
		else
		{
			sprintf(pl_Desc, "%s", strerror(ec));
		}
		X_WARN("OnSysError errno=%d, %s", ec, pl_Desc);
	}

	if (TestFlag(m_Param.v_ExtraFlag, TRANSFER_FTP_RESUME))
	{
		m_State = TS_STATE_WAIT_RECONNECT;// 其他流程进入将状态修改导致后续不能重连
#ifndef FUNC_MMST_EPOLL_20				// 媒体传输网络版本2.0 使用EPOLL边缘触发 模块化
		OnSockDisconnected(m_pMainSocket);
#endif
		return;
	}

	MC_CALL_CB(TEV_ERROR, TEE_SYSTEM_ERRNO, ec, pl_Desc);
}

void Session_Base::OnError(t_TrasferErrorCode vp_Ec, const char* pp_Err)
{
	if (m_State == TS_STATE_ERROR)
	{
		return;
	}

	if (pp_Err)
	{
		X_ERROR("Mmst: OnError:%d Errstr=%s", vp_Ec, pp_Err);
	}
	else
	{
		X_ERROR("Mmst: OnError:%d", vp_Ec);
	}
	StopAllCh();//STOP DVRR CH
//	m_histroy_upload.stop();
	m_timer_send_gps.stop();
	Reset();
	MC_CALL_CB(TEV_ERROR, 0, vp_Ec, pp_Err);
	SetState(TS_STATE_ERROR, __FILE__, __LINE__, __func__, (int)vp_Ec);

}

u32 Session_Base::GetDataSizeInOutBuffer(void)
{
	u32 vl_ret = 0;
	if (!MC_MMST_IN_TCP())
	{
		return CMmstMedia::m_TxBytesWaiting;
	}
#ifndef FUNC_MMST_EPOLL_20				// 媒体传输网络版本2.0 使用EPOLL边缘触发 模块化
	if (!m_pMainSocket)
	{
		// 部标协议的下载功能是自己另有一套tcp连接,所以m_pMainSocket为NULL是正常的,这里不需要输出
		//X_ERROR("NULL == m_pMainSocket");
		return 0;
	}
	vl_ret = m_pMainSocket->GetDataSizeInOutBuffer();
	t_clientSocket::iterator it = m_otherSocket.begin();
	for (; it != m_otherSocket.end(); it++)
	{
		vl_ret += (*it)->GetDataSizeInOutBuffer();
	}
	vl_ret += CMmstMedia::m_TxBytesWaiting;
	if (vl_ret > 0)
	{
		//MMST_TRACE("InOutBufferSize:%d\n", vl_ret);
	}
#endif

	return vl_ret;
}

//打包发送
int Session_Base::SendPacket(t_MediaPackHdr* p_Pack, bool vp_ReTrans, u32 *pp_PackLen)
{
	int vl_ret = 0;
	static u32 vl_Cnt = 0;
#if VM_MMST_FLOWCTL_PROP		// 媒体传输流量控制属性
	static u32 vl_Tick;
#endif
	u8 vl_Ch;
	u8 vl_type;
	if (NULL == m_pCurPkg)
	{
		X_ERROR("NULL == m_pCurPkg");
		return vl_ret;
	}
	DVRR_WRITEMONITORLOG(0x90000017, 1000, NULL, false);
	vl_Ch = p_Pack->p_Frame->v_ChID;
	vl_type = p_Pack->p_Frame->v_PayloadType;
	DVRR_CONSTVARCHECK(&m_pCurPkg->m_bufftestpre);
	//DVRR_CONSTVARCHECK(&m_pCurPkg->m_bufftestback, false);
	m_pCurPkg->ResetBuffer();
	//X_TRACE("SendPacket----->Ch:%u Type:%u", vl_Ch, vl_type);
	if (!vp_ReTrans) //if is not resend package,use a new SN;
	{
		p_Pack->v_SendSN = GetPSN(p_Pack->p_Frame->v_PayloadType - 1);
	}
	else
	{
		m_pCurPkg->SetResendFlag();
	}
	//MMST_TRACE("sn:%2x,%d\n", p_Pack->v_SendSN, p_Pack->v_SN);
	m_pCurPkg->MakeMediaPack(m_Param.v_TermNo, (u16)p_Pack->v_SendSN, 0xFF, p_Pack, 0, (char *)m_Param.a_MobileNum, m_Param.v_LogicChNum, m_Param.v_transfertype);

#if VM_MMST_FLOWCTL_PROP		// 媒体传输流量控制属性
	if (vp_ReTrans)
	{
		m_ana.CountRetransLen(p_Pack->v_Len);
	}
	m_ana.CountLen(p_Pack->v_Len);
#endif
	if (NULL != pp_PackLen)
	{
		*pp_PackLen = m_pCurPkg->GetPackSize();
	}
	//TxHook(m_pCurPkg->GetParentSocket(), p_Pack->p_Frame, p_Pack->v_Len, 0);

	vl_ret = Send();

	vl_Cnt++;

	m_LastSendTick = MDK_GetTickCount();
	p_Pack->v_SentTick = m_LastSendTick;
	p_Pack->v_SentCount++;

#if VM_MMST_FLOWCTL_PROP		// 媒体传输流量控制属性
	if (vl_ret > 0 && IsValidChType(vl_Ch, vl_type))
	{
#ifdef FUNC_FLOW_AND_MEDIA_11
		// 修改说明:
		//		1: 长度字段,不能强制加上"UDP_PACK_HEAD_LEN",因为使用tcp还是udp,可以动态决定的,这里不能写死
		//		2: 目前只有有为协议使用了流控,所以有为协议维持代码原样,其他协议要改
		//if (FlowLimitProc(m_LastSendTick, (u32)(vl_ret + UDP_PACK_HEAD_LEN), vl_Ch, vl_type) == 0)
		int nFlowResult = -1;
		if (g_conf_media_protocol == COMM_PROTOCOL_SUB_TYPE_YWRTMP) // 有为协议沿用原代码
			nFlowResult = FlowLimitProc(m_LastSendTick, (u32)(vl_ret + UDP_PACK_HEAD_LEN), vl_Ch, vl_type);
		else // 其他协议,长度不加UDP_PACK_HEAD_LEN
			nFlowResult = FlowLimitProc(m_LastSendTick, (u32)vl_ret, vl_Ch, vl_type);

		if (nFlowResult == 0) // 流量超标
#else
		if (FlowLimitProc(m_LastSendTick, (u32)(vl_ret + UDP_PACK_HEAD_LEN), vl_Ch, vl_type) == 0)
#endif // FUNC_FLOW_AND_MEDIA_11
		{
			p_Pack->p_Frame = NULL;
		}
	}

	if (((vl_Cnt % 400) == 1))
	{
		float vl_Speed;
		vl_Cnt = 0;
		u32 vl_KK;
		vl_KK = MC_X_TICK_TO_SECOND(MDK_GetTickCount() - m_TxSeconds);
		if (m_TxBytesTotal > 0 && m_ana.m_TxPkg > 0 && vl_KK > 0)
		{
			vl_Speed = (float)(m_TxBytesTotal / 1024.0 / vl_KK);
			MMST_PRINT(MMST_DISP_FMT, 0xFF, 0, CMmstMedia::m_TxBytesWaiting, 0, (int)(m_ana.m_TxBytes*1.0 / m_ana.m_TxPkg), vl_Speed, (float)(m_ana.m_TxBytes*1.0 / m_TxBytesTotal), m_ana.m_TxPkg, m_ana.m_ReTxPkg, m_ana.m_ReTxPkg*1.0 / m_ana.m_TxPkg, m_ana.m_TxPkg*1.0 / vl_KK, vl_KK, m_TxBytesTotal, m_flow.v_StreamLen, m_flow.m_LatestBPS / 1024.0, m_flow.m_PER);

			//DumpSockInfo();
		}
		if (((u32)(m_LastSendTick - vl_Tick)) > 500)
		{
			//vl_Tick = MDK_GetTickCount();
			//DumpSockInfo();
		}
	}
#endif

	return vl_ret;
}
//////////////////////////////////////////////////////////////////////////
void mask_signal(void)
{
#ifndef WIN32
	sigset_t add;
	int vl_ret;
	sigemptyset(&add);
	sigaddset(&add, SIGINT);
	sigaddset(&add, SIGTERM);
	vl_ret = pthread_sigmask(SIG_SETMASK, &add, NULL);
	if (0 != vl_ret)
	{
		perror("pthread_sigmask");
	}
#endif
}

void Session_Base::FreeRTPStream(t_RTPStream* p_rtpStream)
{
	if (!p_rtpStream)
	{
		return;
	}

#ifdef MMST_MEM_ANA_FTR
	g_stmCnt.a_Freeed++;
#endif

	//DVRR里分配的.
	x_free((void*)p_rtpStream);
}

void Session_Base::CloseChMedia(u8 vl_Ch, u8 vl_Type, t_TrasferSessionState vp_NextState)
{
	//MMST_PRINT("%s %d CH:%d Type:%d Next:%d\n",__func__,__LINE__, vl_Ch, vl_Type, vp_NextState);
	DVRR_WRITEMONITORLOG(0x90006012, 1000, NULL, false);
	if (m_aCh[vl_Ch].GetStreamByType(vl_Type))
	{
		m_aCh[vl_Ch].GetStreamByType(vl_Type)->SetState(MMST_MEDIA_STATE_CLOSED);
		m_aCh[vl_Ch].GetStreamByType(vl_Type)->Clear();
		ClearBit(m_aCh[vl_Ch].m_Flag, vl_Type);
	}
	else
	{
	    X_ERROR("CH:%d Type:%d Next:%d\n", vl_Ch, vl_Type, vp_NextState);
	}
	//m_aCh[vl_Ch].Clear();
	if (m_aCh[vl_Ch].HasAnySending())
	{
	}
	else
	{//本通道所有传输已经结束
		ClearBit(m_TransChFlag, vl_Ch);	//清通道标记
#if VM_MMST_FLOWCTL_PROP		// 媒体传输流量控制属性
		m_aTransCtrl[vl_Ch].set(0, 0);//重置流控
#endif
		if (HasAnyTransfering())
		{
		}
		else
		{//所有通道传输都已经结束. 进入STOP状态
			SetState(vp_NextState, __FILE__, __LINE__, __func__);
		}
	}
}

bool Session_Base::AllReseted()
{
	u8 i;
	int j;
	bool vl_ret = true;

	for (i = 0; i < MAX_TRANS_CH; ++i)
	{
		//MMST_TRACE("No.%d\n",i);
		if (m_aCh[i].HasAnySending())
		{
			for (j = 0; j < RTP_STREAM_TYPE_CNT; ++j)
			{
				//MMST_TRACE("Noooo.%d\n",j);
				if (m_aCh[i].m_stm[j].GetState() == MMST_MEDIA_STATE_RESETING)
				{
					vl_ret = false;
					break;
				}
			}
		}
	}

	return vl_ret;
}


void Session_Base::Reset()
{
	m_LastACK = MDK_GetTickCount();
	if (NULL == m_pCurPkg)
		return;

	m_pCurPkg->ResetCmd();
}

bool Session_Base::audio_session_is_open(void)
{
	return (m_aCh[AUDIO_SESSION_CHANNEL].GetStreamByType(RTP_STREAM_TYPE_AUDIO)->GetState() == MMST_MEDIA_STATE_SENDING);
}


u8 Session_Base::GetAppProType()
{
	return m_Param.v_AppProtoType;
}

t_TransferParam* Session_Base::GetTransParm()
{
	return &m_Param;
}

#ifndef FUNC_MMST_EPOLL_20				// 媒体传输网络版本2.0 使用EPOLL边缘触发 模块化
void Session_Base::DestroyChannel(x_asio_socket* pp_Socket)
{
	if (pp_Socket != m_pMainSocket)
	{
		m_otherSocket.erase(pp_Socket);
		if (pp_Socket)
			x_delete pp_Socket;
		pp_Socket = NULL;
	}
	else
	{
		if (m_pMainSocket)
		{
			m_pMainSocket->Close();
		}
	}
}
#endif

#define INIT_MSG_MAP(msgid, mask,callback)\
	{msgid, 0, (mask),callback}

#define MMST_MSG_MAP_STATE_MASK_DEFAULT 0XFFFFFFFF
#define MMST_MSG_MAP_STATE_MASK_IDLE_SENDING ((u32)1<<TS_STATE_IDLE) |((u32)1<<TS_STATE_SENDING)


int Session_Base::SetForbiden(bool bOp)
{
	m_bForbidden = bOp;
	return 0;
}

int Session_Base::GetInfo(t_TransferSessionInfo* pp_Info)
{
	if (NULL != pp_Info)
	{
		//memcpy( &pp_Info->v_Ctrl, &m_Param.v_ctrl, sizeof(t_TransferCtrl) );
		//pp_Info->v_Mode = m_Param.v_Mode;
		pp_Info->v_State = m_State;
	}
	return 0;
}

bool Session_Base::CanSendNext()
{
#if VM_MMST_FLOWCTL_PROP		// 媒体传输流量控制属性
	return m_flow.CanSend();
#else
	return true;
#endif
}

//改为批量发送. 尽量把缓存的内容优先发送
int Session_Base::PostStream(t_RTPStream* pp_Stream, bool b_Hist /*= false*/)
{
	X_TRACE_FL();
	return 0;
}

bool isInnerIP(u32 a_ip)
{
	a_ip = htonl(a_ip);
	//	MMST_TRACE("%08X\n",a_ip);
	return ((a_ip == 0xa) || (a_ip >> 16 == 0xc0a8) || (a_ip >> 22 == 0x2b0));
}

int Convert_NetIntf_To_ConnectStrage(u8 vp_netInfStratage)
{
	E_CONNECT_STRATAGE vl_connect_stratage;

	switch (vp_netInfStratage)
	{
	case 0: //any
		vl_connect_stratage = ECS_NORMAL;
		break;
	case 1: //ppp
		vl_connect_stratage = ECS_PPP_ONLY;
		break;
	case 2: //wifi优先
		vl_connect_stratage = ECS_WIFI_PRIOR;
		break;
	case 3: //wifi
		vl_connect_stratage = ECS_WIFI_ONLY;
		break;
	case 4: //eth0
		vl_connect_stratage = ECS_ETH_ONLY;
		break;
	default:
		vl_connect_stratage = ECS_NORMAL;
		break;
	}

	return (int)vl_connect_stratage;
}

int Session_Base::ThreadInit(u8 pm_type)
{
	struct bsd_in_addr vl_ia;
	char al_IP[32];
#ifndef s_addr
	#define s_addr  S_un.S_addr
#endif
	vl_ia.s_addr = m_Param.v_RemoteAddr;
#if 1
	#undef s_addr
#endif

	strcpy(al_IP, inet_ntoa(*(in_addr *)(&vl_ia)));
	if (strcmp(al_IP, "0.0.0.0") == 0)
	{
		X_TRACE("MMST ip error %s", al_IP);
		return ENETUNREACH;
	}
	m_bIntraNet = isInnerIP(m_Param.v_RemoteAddr);
	X_TRACE("---MMST %s %s Server %s %d: IntraNet:%d Tcp:%d", __DATE__, __TIME__, al_IP, m_Param.v_RemotePort, m_bIntraNet, m_Param.v_ProtoType);
#if VM_MMST_FLOWCTL_PROP		// 媒体传输流量控制属性
	if (m_bIntraNet)
	{
		m_flow.SetTxLimit(10 << 20);
	}
#endif

	//open socket
#ifndef WIN32
	X_ASSERT(NULL == m_pMainSocket);
#endif

	m_pMainSocket = CreateChannel(m_Param.v_RemoteAddr, m_Param.v_RemotePort, pm_type, MC_MMST_IN_TCP());
	if (NULL == m_pMainSocket)
	{
		m_State = TS_STATE_STOPED;
		OnSysError("CreateChannel");
		return -1;
	}

#if VM_MMST_FLOWCTL_PROP		// 媒体传输流量控制属性
#ifdef DATA_PACK_ANA_FTR
	m_ana.reset();
#endif
#endif

	//SNDBUF: 206848/262144

#ifndef FUNC_MMST_EPOLL_20				// 媒体传输网络版本2.0 使用EPOLL边缘触发 模块化
	SetCurrentPkg(m_pMainSocket);
#endif
	if (MC_MMST_IN_TCP())
	{
		SetState(TS_STATE_CONNECTING, __FILE__, __LINE__, __func__);
	}
	else
	{
		SetState(TS_STATE_REG, __FILE__, __LINE__, __func__);
	}
	return 0;
}

int Session_Base::Start()
{
	X_TRACE_FL();
	return 0;
}

int Session_Base::SendStopMsg(u32 vp_Type)
{
	X_TRACE_FL();
	return 0;
}

int Session_Base::Stop()
{
	X_TRACE_FL();
	return 0;
}

int Session_Base::On1Hz()
{
	X_TRACE_FL();
	return 0;
}

void Session_Base::StartChMediaEx(u8 vl_Ch, u8 vl_Type)
{
	if (!IsValidChType(vl_Ch, vl_Type, true))
	{
		MMST_TRACE_WARNING();
		return;
	}

	if (m_aCh[vl_Ch].GetStreamByType(vl_Type)->GetState() != MMST_MEDIA_STATE_SENDING)
	{
		X_TRACE("StartChMediaEx:%u,%u", vl_Ch, vl_Type);

#ifdef FUNC_IS_IPCV3
		int vl_ret = -1;
		//启动实时码流
		if (vl_Ch < HISTROY_TRANS_CH_BASE && vl_Type != 0)
		{
			X_TRACE("start transfer for ch%d flag:%2x", vl_Ch, vl_Type);
			vl_ret = IPC_StartTransfer(vl_Ch, vl_Type, &(m_aCh[vl_Ch].m_cfg));
			if (0 != vl_ret)
			{
				X_ERROR("StartTransfer");
				return;
			}
		}
		else
		{
			vl_ret = 0;
		}
#endif

		StartChMedia(vl_Ch, vl_Type);
	}
}
BOOL Session_Base::IsChMediaStart(u8 vl_Ch, u8 vl_Type)
{
	if (vl_Ch >= YW_MAX(MAX_TRANS_CH, MAX_REAL_TRANS_CH))
		return FALSE;
	if (vl_Type > RTP_STREAM_TYPE_CNT)
		return FALSE;

	E_MMST_MEDIA_STATE state = m_aCh[vl_Ch].GetStreamByType(vl_Type)->GetState();
	if (state == MMST_MEDIA_STATE_NA || state == MMST_MEDIA_STATE_CLOSED)
		return FALSE;
	else
		return TRUE;
}

void Session_Base::StartChMedia(u8 vl_Ch, u8 vl_Type)
{
	X_TRACE("StartChMedia vl_Ch: %d, vl_Type: %d", vl_Ch, vl_Type);

	m_aCh[vl_Ch].GetStreamByType(vl_Type)->SetState(MMST_MEDIA_STATE_SENDING);
	SetBit(m_aCh[vl_Ch].m_Flag, vl_Type);
	SetBit(m_TransChFlag, vl_Ch);	// 设置通道标记
	SetState(TS_STATE_SENDING, __FILE__, __LINE__, __func__);
}

void Session_Base::StopAllCh()//调用DVRR -- STOP
{
	u8 i;
	for (i = 0; i < MAX_TRANS_CH; ++i)
	{
		StopCh(i);
	}
}


void Session_Base::AsyncStop()
{
	X_TRACE_FL();
}

bool Session_Base::HasAnySending()
{
	u8 i;
	bool b_Has = false;
	for (i = 0; i < MAX_TRANS_CH; ++i)
	{
		if (m_aCh[i].HasAnySending())
		{
			b_Has = true;
			break;
		}
	}

	return b_Has;
}

bool Session_Base::TimeoutCheck()
{
	X_TRACE_FL();
	return true;
}

void Session_Base::StopChMedia(u8 vl_Ch, u8 vl_Type, t_TrasferSessionState vp_NextState)
{
	int vl_ret = 0;
	u8 vl_Flag;

	X_TRACE("StopChMedia: %d,%d", vl_Ch, vl_Type);
	if (vl_Type == 1)
	{
		vl_Flag = 0x01;
	}
	else  if (vl_Type == 2)
	{
		vl_Flag = 0x02;
	}
	else if (vl_Type == RTP_STREAM_TYPE_GPS)
	{
		vl_Flag = RTP_STREAM_TYPE_GPS;
	}
	else
	{
		vl_Flag = 0x0;
	}

	if (vl_Flag != 0)
	{
		if (m_aCh[vl_Ch].IsSendingType(vl_Type))
		{
			//stop
			if (vl_Ch < HISTROY_TRANS_CH_BASE)
			{

#ifdef FUNC_IS_IPCV3
				vl_ret = IPC_StopTransfer(vl_Ch, vl_Flag);
#else
#if defined(SUPPORT_IPC_FUN) || defined(SUPPORT_EXTERNAL_IPC_FUN)
				if ((MC_IS_YWIPC_CFG()) && (vl_Ch >= dvrr_get_max_cam_num() && vl_Ch < (dvrr_get_max_cam_num() + MAX_IPC_REAL_TRANS_CH)))
				{
					//停止IPC传?
#ifdef DXR_VIDEO_ONLY_LOCAL_TRANSFER						// T4本地视频传输
        		    {
        		        X_TRACE("rtmp stop ipc transfer");
#ifdef DXR_IPC_TRANSMODE                       //IPC传输模式
                        g_dvrr_State.v_ipcRemoteState[vl_Ch - 2] = 0;
#endif
					vl_ret = ipc_StopTransfer(vl_Ch, vl_Flag);
        		    }
                    //else
                     //   vl_ret = 0;
#else
				    vl_ret = ipc_StopTransfer(vl_Ch,vl_Flag);
#endif
				}
				else if ((MC_IS_EXTERNL_IPC_CFG()) && (vl_Ch >= dvrr_get_max_cam_num() && vl_Ch < (dvrr_get_max_cam_num() + MAX_NMA_CH_NUM)))
				{
					//停止扩展IPC传?
					vl_ret = extIpc_StopTransfer(vl_Ch, vl_Flag);
				}
				else
#endif
				{
					vl_ret = mmst_StopTransfer(vl_Ch, vl_Flag);
				}
#endif
			}
			else
			{
				X_TRACE("Stop Channel:%d,%d", vl_Ch, vl_Type);
				vl_ret = 0;
			}

			if (0 != vl_ret)
			{
				OnSysError("StopTransfer");
			}
		}
		else
		{
			vl_ret = 0;
		}
	}
	else
	{
		MMST_TRACE_ERROR();
		vl_ret = 2;
	}

	CloseChMedia(vl_Ch, vl_Type, vp_NextState);
}

int Session_Base::GetSocketID()
{
#ifndef FUNC_MMST_EPOLL_20				// 媒体传输网络版本2.0 使用EPOLL边缘触发 模块化
	if (m_pMainSocket)
	{
		return m_pMainSocket->GetSocket();
	}
	else
#endif
	{
#ifndef FUNC_MMST_EPOLL_20				// 媒体传输网络版本2.0 使用EPOLL边缘触发 模块化
		X_ERROR("m_pMainSocket = %p", m_pMainSocket);
#endif
		return 0;
	}
}

void Session_Base::SetGPSInterval(u32 vp_Interval)
{
	//throw std::exception("The method or operation is not implemented.");
	m_nGpsInterval = vp_Interval;
}

bool Session_Base::IsConnected()
{
	//throw std::exception("The method or operation is not implemented.");
	return (TS_STATE_IDLE == m_State) || (TS_STATE_SENDING == m_State) || (TS_STATE_RESETING == m_State);
}

void Session_Base::GpsTrans(u32 vp_Start, u32 vp_Interval)
{
	//throw std::exception("The method or operation is not implemented.");
	if (vp_Start)
	{
		SetGPSInterval(vp_Interval);
		m_bGpsTransfering = true;
	}
	else
	{
		m_bGpsTransfering = false;
	}
}

void Session_Base::UpdateTxBytesWaiting(u8 vp_ChId, u32 vp_StreamLen, bool vp_Add)
{
	u32 *pl_TxBytesWaiting = &m_RealTxBytesWaiting;
	if (MC_IS_HIST_CH(vp_ChId))
	{
		pl_TxBytesWaiting = &m_HisTxBytesWaiting;
	}

	if (vp_Add)
	{
		(*pl_TxBytesWaiting) += vp_StreamLen;
	}
	else
	{
		(*pl_TxBytesWaiting) -= vp_StreamLen;
	}
}

Mmst_QueryResult* Session_Base::FindQuaryList(u32 nLogicChnl, u32 nBeginTime, u32 nEndTime)
{
	for (u32 i = 0; i < m_arrQueryList.size(); ++i)
	{
		if ((m_arrQueryList[i].nChnl == (nLogicChnl))
			&& (m_arrQueryList[i].nBeginTime == nBeginTime))
		{
			return &m_arrQueryList[i];
		}
	}
	return NULL;
}

void Session_Base::ClearQueryList()
{
	m_arrQueryList.clear();
}

void Session_Base::InsertQueryItem(Mmst_QueryResult item)
{
	m_arrQueryList.push_back(item);
}

int Session_Base::SendNextCh()
{
	int vl_Sent = 0;
#ifndef FUNC_MMST_EPOLL_20				// 媒体传输网络版本2.0 使用EPOLL边缘触发 模块化
	t_MediaPackHdr* pl_Pack;
	bool vl_ReTrans;
	u8 i;
	u8 j;
#ifdef FUNC_IS_IPCV3
	x_asio_socket *pl_hist_socket = NULL;
#else
	x_asio_socket *pl_hist_socket;
#endif
	if (m_otherSocket.empty())
	{
		pl_hist_socket = m_pMainSocket;
	}
	else
	{
		pl_hist_socket = *m_otherSocket.begin();
#ifdef TCP_SEND_CONGESTION
		u32 sockbufsize = 0;
		if (NULL != pl_hist_socket)
		{
			sockbufsize = pl_hist_socket->GetDataSizeInOutBuffer();
		}
		if (sockbufsize > 1 * 1024 || 0 == sockbufsize)
		{
			printf("m_otherSocket(%p) data buffer is 0x%x.%s%d\n", pl_hist_socket, sockbufsize, __func__, __LINE__);
		}
#endif
	}

#ifdef FUMC_MMST_BALANCE_SEND


#else
	//printf("- No.[%d] [%d]\n",m_I,m_J);
	if (m_J >= RTP_STREAM_TYPE_CNT)
	{//一个通道完了, 下一个通道
		m_J = 0;
		m_I++;
	}

	if (m_I >= MAX_TRANS_CH)
	{//到头了, 回到0通道
		m_I = 0;
		m_J = 0;
	}
#endif // FUMC_MMST_BALANCE_SEND

	for (i = m_I; i < MAX_TRANS_CH; i++)
	{
#ifndef FUNC_IS_IPCV3
		if (i == AUDIO_SESSION_CHANNEL)
		{
			if (!IsSockExist(m_clientSock[i]))
			{
				m_clientSock[i] = pl_hist_socket;
				X_TRACE("assign channel[%d] with sock:%p", i, pl_hist_socket);
			}
		}

		if (MC_IS_HIST_CH(i) && (NULL != m_hist_channel) && (m_hist_channel->is_pause()))
		{
#ifdef TCP_SEND_CONGESTION
			printf("MC_IS_HIST_CH(%d):0x%x, channel_is_pasue:%d.%s%d\n",
				i, MC_IS_HIST_CH(i), m_hist_channel->is_pause(), __func__, __LINE__);
#endif
			i = MAX_HISTROY_TRANS_CH;
			continue;
		}
#endif
		if (NULL == m_clientSock[i])
		{
			continue;
		}

		BOOL bIsFrameLastPkg = FALSE; // 是否为一帧的最后一个分包

#ifdef FUMC_MMST_BALANCE_SEND
		for (j = m_J[m_I]; j < RTP_STREAM_TYPE_CNT; ++j)
#else
		for (j = m_J; j < RTP_STREAM_TYPE_CNT; ++j)
#endif
		{
			if (m_aCh[i].IsSendingType(j + 1)
				&& m_aCh[i].m_stm[j].IsInSendingState())
			{
				if (NULL != m_clientSock[i])
				{
					if (m_clientSock[i]->IsTcp() && !m_clientSock[i]->IsConnected())
					{
						x_sys_usleep(1);
						break;
					}

					/*有超过10K数据没有发出去或者本地缓存超过128K*/
					if (m_clientSock[i]->GetDataSizeInOutBuffer() > 10 * 1024 || CMmstMedia::m_TxBytesWaiting > 128 * 1024)
					{
						m_clientsockwait = true;
					}
					else
					{
						m_clientsockwait = false;
					}
					/*调大缓存的临界值*/
					if (m_clientSock[i]->IsTcp() && m_clientSock[i]->GetDataSizeInOutBuffer() > 10 * 1024)
					{
						static u32 vl_retrynum = 0;
						if (((vl_retrynum) % 4) == 0)
						{
							//((x_asio_tcp*)m_clientSock[i])->WriteInnerBufferData();
							OnSockSend(m_clientSock[i]);
						}
						vl_retrynum++;
						break;
					}
				}

				// pl_Pack不为NULL,表示有数据,准备发送,若为NULL,则表示当前这种媒体已全部发完
				pl_Pack = m_aCh[i].m_stm[j].Pop(vl_ReTrans);
				if (NULL != pl_Pack)
				{
					//要取得本CH绑定的SOCKET
					if (NULL == m_clientSock[i])
					{
						printf("i:%u\n", i);
					}
					ASSERT(NULL != m_clientSock[i]);
					//X_PRINT("m_clientSock[%d]=%d", i, m_clientSock[i]->GetSocket());

					SetCurrentPkg(m_clientSock[i]);//绑定SOCKET/PACKET
					int vl_ret = SendPacket(pl_Pack, vl_ReTrans);//发送
					if (0 == vl_ret)
					{
						//send返回0已执行过onsockdisconnected
						return 0;
					}

					if (((pl_Pack->v_SplitFlag & 0x11) == 0x11) // 单包
						|| ((pl_Pack->v_SplitFlag & 0x10) == 0x10) // 尾包
						)
					{
						bIsFrameLastPkg = TRUE; // 表示这是一个帧的最后一个分包
					}


					if (NULL == pl_Pack->p_Frame)
					{
#ifdef TCP_SEND_CONGESTION
						printf("pl_Pack->p_Frame NULL. i=%d, j=%d,%s%d\n", i, j, __func__, __LINE__);
#endif
						MMST_TRACE_WARNING();
						x_delete pl_Pack;
						pl_Pack = NULL;
						break;
					}

					vl_Sent += pl_Pack->v_Len;
					if (MC_MMST_IN_TCP())
					{//TCP 不重发
#ifdef TCP_SEND_CONGESTION
						printf("vl_Sent:%d, pl_Pack->v_Len:%d. i=%d, j=%d,%s%d\n", vl_Sent, pl_Pack->v_Len, i, j, __func__, __LINE__);
#endif
						m_aCh[i].m_stm[j].FreeIfDone(pl_Pack->p_Frame);
						x_delete pl_Pack;
						pl_Pack = NULL;
					}
					break; // for (j)
				}
				else
				{
#ifdef TCP_SEND_CONGESTION
					printf("pl_Pack NULL. i=%d, j=%d,%s%d\n", i, j, __func__, __LINE__);
#endif
#if VM_MMST_FLOWCTL_PROP		// 媒体传输流量控制属性
					FlowLimitProc(MDK_GetTickCount(), 0, i, j + 1);
#endif
				}
			}
		} // for (j)

#ifdef FUMC_MMST_BALANCE_SEND
		if (vl_Sent > 0) //有发的
		{
			// 如果是当前帧的最后一个分包
			if (bIsFrameLastPkg)
			{
				// 当前通道准备发下一种媒体
				m_J[m_I] = j + 1;
				if (m_J[m_I] >= RTP_STREAM_TYPE_CNT)
				{
					m_J[m_I] = 0;
				}

				// 当前通道已发完了一个媒体帧,就准备发下一个通道的数据,以使得各通道均匀发送
				m_I++;
				if (m_I >= MAX_TRANS_CH)
				{
					m_I = 0;
				}
			}
			else // 不是最后一个分包,也就是说当前帧还没完
			{
				// m_J保持不变,m_I记住当前通道,以便下一次发送还发这里
				m_I = i;
			}

			break;
		}
		else // 没发的
		{
			// 若该媒体已发完,则转向下一个媒体
			if ((j < RTP_STREAM_TYPE_CNT) && (m_aCh[i].m_stm[j].IsEmpty()))
			{
				m_J[m_I]++;
				if (m_J[m_I] >= RTP_STREAM_TYPE_CNT)
				{
					m_J[m_I] = 0;
				}
			}
		}

#else
		if (vl_Sent > 0)
		{//有发的
			m_I = i;
			m_J = j + 1;//下次进来发下一个
			break;
		}
		else
		{//没发的 --- 继续下一个通道, 从0 开始
			m_J = 0;
		}
#endif // FUMC_MMST_BALANCE_SEND
	} // for (i)

	if (vl_Sent == 0)
	{//本轮未发. 回到0通道
#ifdef TCP_SEND_CONGESTION
		printf("vl_Sent is 0.m_I=%d, m_J=%d, i=%d, j=%d, %s%d\n", m_I, m_J, i, j, __func__, __LINE__);
#endif
		m_I = 0;

#ifdef FUMC_MMST_BALANCE_SEND
		m_J[m_I] = 0;
#else
		m_J = 0;
#endif // FUMC_MMST_BALANCE_SEND

	}
#endif

	return vl_Sent;
}

#if VM_MMST_FLOWCTL_PROP		// 媒体传输流量控制属性
void Session_Base::SetBandWidth(const t_BandwidthInfo* param1)
{//limit is for 3G only
	if (m_bIntraNet)
	{
		return;
	}
	if (param1->v_TxBytesPerSecondQos != m_BandwidthInfo.v_TxBytesPerSecondQos)
	{
		m_flow.SetTxLimit(param1->v_TxBytesPerSecondQos);
		MMST_TRACE("--- QosTX %08X => %08X\n", m_BandwidthInfo.v_TxBytesPerSecondQos, param1->v_TxBytesPerSecondQos);

	}
	memcpy(&m_BandwidthInfo, param1, sizeof(t_BandwidthInfo));
	if (m_BandwidthInfo.v_TxBytesPerSecondQos < 16)
	{
		m_BandwidthInfo.v_TxBytesPerSecondQos = 16;
	}
}
#endif

bool Session_Base::IsValidChType(u8 vl_Ch, u8 vl_Type, bool vp_IsInnerChannel)
{
	return ((vl_Ch < (vp_IsInnerChannel ? MAX_TRANS_CH : MAX_REAL_TRANS_CH))
		&& (vl_Type > 0)
		&& (vl_Type <= RTP_STREAM_TYPE_CNT));
}

void Session_Base::SendTEV(t_TransferEventType vp_TEV, size_t vp_Size, void* pp_Info)
{
	MC_CALL_CB(vp_TEV, vp_Size, 0, (const char*)pp_Info);
}



void Session_Base::SetFLCtrl(const t_FLControl* param1)
{
	//throw std::exception("The method or operation is not implemented.");
#if VM_MMST_FLOWCTL_PROP		// 媒体传输流量控制属性
	memcpy(&m_flc, param1, sizeof(t_FLControl));
	if (m_flc.v_flResetCurrent)
	{
		m_flow.ResetACM();
	}

	MMST_TRACE("--- MMST SET MB:%u/%u  Seconds:%u/%u \n", m_flc.v_AlarmMB, m_flc.v_StopMB, m_flc.v_AlarmSecond, m_flc.v_StopSecond);
#endif
}

void Session_Base::SendResetPacket(u8 vp_ChInfo, E_RESET_CAUSE vl_envent)//发送重置包
{
	X_TRACE_FL();
}

void Session_Base::SendEvent(u32 vp_GpsSize, const void* pp_GpsData, u32 vp_DataSize, const void* pp_Data)
{
	X_TRACE_FL();
}

bool Session_Base::IsTransingHistory(void)
{
	X_TRACE_FL();
	return false;
}

bool Session_Base::IsNeedNewSession(const t_TransferParam* pp_Init)
{
	return false;
}

int Session_Base::ProcIntfDown(u8 vp_IntfType)
{
	return 0;
}

void Session_Base::SetGetFLAck(const t_FLGetInfo* param1)
{
	X_TRACE_FL();
}


u16 Session_Base::GetPSN(u8 vp_Type)//取流水号
{
	//printf("before Session_Base::GetPSN:%d\n", m_PackSN);
	if (0 == vp_Type)
		return m_PackSN++;

	return m_PackSNExt++;
}

void Session_Base::SetPSN(u16 vp_PackSn, u8 vp_Type)//设置流水号
{
	//printf("before Session_Base::GetPSN:%d\n", m_PackSN);
	if (0 == vp_Type)
	{
		m_PackSN = vp_PackSn ? (vp_PackSn + 1) : 0;
		return;
	}

	m_PackSNExt = vp_PackSn ? (vp_PackSn + 1) : 0;
}

//////////////////////////////////////////////////////////////////////////


void Session_Base::SetRegFlag(t_ConnCommFlag vp_Flag)
{
	if (m_IsRedirect)
	{
		switch (vp_Flag)
		{
		case MMST_CCF_NORMAL:
			m_RegFlag = MMST_REG_FLAG_REDIRCET;
			break;
		case MMST_CCF_TIMEOUT:
			m_RegFlag = MMST_REG_FLAG_REDIRECT_RECONN_TIMEOUNT;
			break;
		case MMST_CCF_FAILED:
			m_RegFlag = MMST_REG_FLAG_REDIRCET_FAILED;
			break;
		case MMST_CCF_PT_REQ:
			m_RegFlag = MMST_REG_FLAG_NORMAL;
			break;
		case MMST_CCF_EXCEPTION:
			m_RegFlag = MMST_REG_FLAG_REDIRECT_RECONN_EXCEPTION;
			break;
		default:
			MMST_TRACE_ERROR();
			break;
		}
	}
	else
	{
		switch (vp_Flag)
		{
		case MMST_CCF_NORMAL:
			m_RegFlag = MMST_REG_FLAG_NORMAL;
			break;
		case MMST_CCF_TIMEOUT:
			m_RegFlag = MMST_REG_FLAG_RECONN_TIMOUT;
			break;
		case MMST_CCF_FAILED:
			m_RegFlag = MMST_REG_FLAG_ERROR_RSP;
			break;
		case MMST_CCF_PT_REQ:
			m_RegFlag = MMST_REG_FLAG_NORMAL;
			break;
		case MMST_CCF_EXCEPTION:
			m_RegFlag = MMST_REG_FLAG_ERROR_RSP;
			break;
		default:
			MMST_TRACE_ERROR();
			break;
		}
	}
}



void Session_Base::SetState(t_TrasferSessionState st, const char* pp_File, int vp_Line, const char* pp_Func, int vp_ErrCode)
{
	static const char* c_Static[] =
	{
		"REG",//正在注册
		"IDLE",//已经注册, IDLE状态
		"SENDING",//发送状态 -- 有任何一个通道的任何一种数据在发送, 则进入这个状态
		"RESETING",//正在重置
		"STOPING",//正在停止
		"ERROR",//错误状态. 需要由APP来KILL后再CREATE
		"STOPED",//停止状态. 需要由APP来KILL后再CREATE -- 这是整个SESSION的最后一个状态变化, 之后SESSION已经结束.
		"CONNECTING",
#ifdef PROP_PRO_TYPE_IPCSELF_ON				// IPCЭ��
		"CONNECTED",
#endif
		"PRESTOP",
		"FTP_CMD_USER",
		"FTP_CMD_PASS",
		"FTP_CMD_TYPEI",
		"FTP_DATA_LISTEN",
		"FTP_CMD_FILE_SIZE",
		"FTP_DATA_ACCPTED",
		"FTP_DATA_PORT",
		"FTP_DATA_PASV",
		"FTP_DATA_CONNECT",
		"FTP_DATA_STORE",
		"WAIT_RECONNECT",
		"TRANS_PAUSE",
		"IPC_DIABLE", //关电IPC, 置ipC为非使用状态
		"IPC_ENABLE", //开IPC,置IPC为使用状态

		"IPC_WAIT_CONNECT",
		"IPC_REG",
		"IPC_IDLE",
		"IPC_DATA",
		"IPC_STOPPED",
		"IPC_BGN_RESET",
		"IPC_END_RESET",
		"MAX"
	};

#ifdef WIN32
	if (st == TS_STATE_ERROR)
	{
		printf("ERROR\n");//debug: VC break point
	}
#endif

	if ((st != m_State || st == TS_STATE_REG))
	{
		X_TRACE("Mmst: %p State %d %s =>%d %s [%s:%d <%s()>]", this, m_State, c_Static[m_State], st, c_Static[st], pp_File, vp_Line, pp_Func);
		MC_CALL_CB(TEV_STATE_CHANGED, m_State, st, (const char*)vp_ErrCode);
#ifdef FUNC_IS_IPCV3
		if (m_Param.v_ProtoType != ECT_UART)
		{
			m_pCurPkg = GetCmdPacket();//状态变化只需要修改main socket相关pkg的sameCnt(on1hz使用)
		}
#else
		m_pCurPkg = GetCmdPacket();//状态变化只需要修改main socket相关pkg的sameCnt(on1hz使用)
#endif

		if (NULL != m_pCurPkg)
		{
			m_pCurPkg->m_SameCnt = 0;
		}

		if (m_State == TS_STATE_REG)
		{
			m_timer_send_gps.stop();
		}
	}
	else
	{
		return;
	}
	if (st == TS_STATE_SENDING)
	{
		m_I = 0;
#ifdef FUMC_MMST_BALANCE_SEND
		m_J[m_I] = 0;
#else
		m_J = 0;
#endif
		//CMmstMedia::m_TxBytesWaiting =0;
		//CMmstMedia::UpdateCurrentSendBufferBytes(GetDataSizeInOutBuffer(), m_Param.v_AppProtoType);
		m_TxSeconds = MDK_GetTickCount();
		m_TxBytesTotal = 0;
#if VM_MMST_FLOWCTL_PROP		// 媒体传输流量控制属性
		//m_flow.Reset();
		m_ana.reset();
		m_LastFlowTick = 0;
#endif
		m_LastACK = MDK_GetTickCount();
		if (MC_MMST_IN_TCP())
		{

		}
		else
		{
			// 				if (NULL == m_pUdpEngine)
			// 				{
			// 					m_pUdpEngine = new udp_engine(10);
			//
			// 				}
			// 				if (NULL != m_pUdpEngine)
			// 				{
			// 					m_pUdpEngine->PollNext();//启动发动机
			// 				}
		}

	}
	else if (st == TS_STATE_IDLE)
	{
		m_idleState_tick = 0;
	}
	m_State = st;
}

CMmstMedia *Session_Base::GetMmstMediaByChInfo(u8 vp_ch, u8 vp_type)
{
	if (vp_ch >= YW_ARRAY_SIZE(m_aCh))
	{
		return NULL;
	}
	return IsValidChType(vp_ch, vp_type) ? m_aCh[vp_ch].GetStreamByType(vp_type) : NULL;
}


CMmstMedia *Session_Base::GetMmstMediaByChInfo(u8 vp_ChInfo)
{
	u8 vl_MediaType = MC_GET_MYTYPE_ID(vp_ChInfo);
	u8 vl_Ch = MC_GET_MYCH_ID(vp_ChInfo);
	return GetMmstMediaByChInfo(vl_Ch, vl_MediaType);
}

#ifdef VICIOUS_CIRCLE
enum
{
	MMST_VC_REQUST,
	MMST_VC_RESPONE
};

enum
{
	VC_RUN_RESPONE_OK = 0,
	VC_RUN_GO_ON,
	VC_RUN_STOP_ALL
};

#define MAX_G		(float)(0.2)
#define MIN_G		(float)(0.1)


int Session_Base::VC_MediaReseting(u32 ch, u32 *p_sw, u32 *p_tick)
{
	MMST_PRINT("---%s---\n", __func__);
	int vl_ret = 0;
	u32 vl_nowtime = 0;
	if (m_Ru > MAX_G)
	{
		if (MMST_VC_REQUST == *p_sw)
		{
			//SendResetPacket( MC_MAKE_CHINFO(ch,0+1), 2 );
			*p_sw = MMST_VC_RESPONE;
			*p_tick = MDK_GetTickCount();
			vl_ret = VC_RUN_GO_ON;
		}
		else if (MMST_VC_RESPONE == *p_sw) {
			vl_nowtime = MDK_GetTickCount();
			if (vl_nowtime - *p_tick > 100 * 2)//2s  按照协议
			{
				*p_sw = MMST_VC_REQUST;
				vl_ret = VC_RUN_GO_ON;
			}
			else {
				//if (m_Ru > MIN_G) {//还是较高的Ru
				if (1) {//rec rsp msg
					*p_sw = MMST_VC_REQUST;
					vl_ret = VC_RUN_RESPONE_OK;
				}
				else {
					vl_ret = VC_RUN_GO_ON;
				}
				//} else {
				//	vl_ret = VC_RUN_STOP_ALL;
				//}
			}
		}
	}
	else
	{
		vl_ret = VC_RUN_GO_ON;
	}

	return vl_ret;
}

int Session_Base::VC_MediaSending(u32 ch, u32 *p_sw, u32 *p_tick)
{
	MMST_PRINT("---%s---\n", __func__);

	int vl_ret = 0;

	CMmstMedia* pl_stm;
	pl_stm = m_aCh[ch].GetStreamByType(0);
	if (pl_stm->GetState() == MMST_MEDIA_STATE_RESETING) //--重置中
	{
		pl_stm->SetState(MMST_MEDIA_STATE_SENDING);
	}
	//set open chx media
	vl_ret = VC_RUN_RESPONE_OK;

	return vl_ret;
}

int Session_Base::VC_MediaCloseing(u32 ch, u32 *p_sw, u32 *p_tick)
{
	MMST_PRINT("---%s---\n", __func__);
	int vl_ret = 0;
	//set close chx media
	vl_ret = VC_RUN_RESPONE_OK;
	return vl_ret;
}

int Session_Base::VC_Wait(u32 tick10ms, u32 *p_sw, u32 *p_tick)
{
	MMST_PRINT("---%s---\n", __func__);
	int vl_ret = 0;
	u32 vl_nowtime = 0;
	if (m_Ru > MAX_G) {
		if (MMST_VC_REQUST == *p_sw)
		{
			*p_sw = MMST_VC_RESPONE;
			*p_tick = MDK_GetTickCount();
			vl_ret = VC_RUN_GO_ON;
		}
		else if (MMST_VC_RESPONE == *p_sw) {
			vl_nowtime = MDK_GetTickCount();
			if (vl_nowtime - *p_tick > tick10ms)
			{
				vl_ret = VC_RUN_RESPONE_OK;
			}
			else {
				//if (m_Ru < MIN_G) {
				//	vl_ret = VC_RUN_STOP_ALL;
				//}
			}
		}
	}
	else {
		vl_ret = VC_RUN_STOP_ALL;
	}

	return vl_ret;
}

int Session_Base::VC_Ru(u32 ru, u32 *p_sw, u32 *p_tick)
{
	MMST_PRINT("---%s---\n", __func__);
	int vl_ret;
	if (m_Ru > MAX_G) {
		vl_ret = VC_RUN_RESPONE_OK;
	}
	else {
		vl_ret = VC_RUN_GO_ON;
	}
	return vl_ret;
}

#define INIT_VC_MAP(func, parm, flag, tick)\
{func, parm, flag, tick}

t_MmstVcMap * Session_Base::GetVcMap(void)
{
	static t_MmstVcMap sa_VcMap[] =
	{
		//加个空状态
		INIT_VC_MAP(&Session_Base::VC_Ru, 	0, 		MMST_VC_REQUST, 0),
		INIT_VC_MAP(&Session_Base::VC_MediaReseting, 	4, 		MMST_VC_REQUST, 0),//ch4
		INIT_VC_MAP(&Session_Base::VC_MediaSending, 	0, 		MMST_VC_REQUST, 0),
		INIT_VC_MAP(&Session_Base::VC_MediaCloseing, 	0, 		MMST_VC_REQUST, 0),
		INIT_VC_MAP(&Session_Base::VC_Wait, 	100 * 10, MMST_VC_REQUST, 0),//10s
		INIT_VC_MAP(&Session_Base::VC_Ru, 	0, 		MMST_VC_REQUST, 0)
	};
	m_VcMaxSize = YW_ARRAY_SIZE(sa_VcMap);
	return sa_VcMap;
}


#if defined(ON_3520)
#ifdef EPOLLRDHUP
#undef  EPOLLRDHUP
#define EPOLLRDHUP 0x200 //since Linux 2.6.17
#endif
#elif defined(ON_FSL_IMX6) || defined(ON_FSL_IMX6_ANDROID) || defined(ON_MTK_MT6735_ANDROID)
#endif

void Session_Base::OnSocketEvent(int events, void * arg)
{
#ifndef FUNC_MMST_EPOLL_20				// 媒体传输网络版本2.0 使用EPOLL边缘触发 模块化
	x_asio_socket* pl_Socket = (x_asio_socket*)arg;
	if (!IsSockExist(pl_Socket))
	{
		X_TRACE("socket:%p alread deleted.", pl_Socket);
		return;
	}

	x_asio_tcp* pl_tcp = dynamic_cast<x_asio_tcp*>(pl_Socket);
	//X_TRACE("-- event %08X",events);
	//X_TRACE("CMmstMedia::m_TxBytesWaiting:%d", CMmstMedia::m_TxBytesWaiting);
	m_timer_deteck_socket_read.start_timer(DETECT_READ_EVENT_INTERVAL, 1);
	if ((events&(EPOLLIN | EPOLLOUT)) != 0)
	{
		if (NULL != pl_tcp && pl_tcp->IsConnecting())// pl_Socket->IsTcp() && !pl_Socket->IsConnected())
		{
			//X_TRACE("-- event %08X", events);
			//X_TRACE("-- socket %08X", pl_tcp->GetFd());

			pl_tcp->SetConnected();

			//WPS -- 所有平台通用 非 V3专用  20180424
			pl_tcp->mod();
			OnSockConnected(pl_Socket);
			//return;//WPS 不用 return 20180424
		}
	}

	if ((events&EPOLLIN))//|| (events&EPOLLHUP) )
	{
		OnSockRead(pl_Socket);
		//printf("-- %s %d %08X ID:%d\n",__func__,__LINE__,events,pl_Socket->GetSocket());
		//pl_Socket->OnRead();
	}
	if (MC_MMST_IN_TCP()
		&& NULL != pl_tcp
		&& (events&EPOLLOUT) != 0
		&& pl_tcp->IsReady()
		)
	{
		if (!pl_Socket->IsReady())
		{
			DestroyChannel(pl_Socket);
		}
		else
		{
			OnSockSend(pl_Socket);
		}
		//if (events&EPOLLRDHUP)
		//{//reomote closed
		//	//OnRemoteClosed();
		//}

		//if (events&EPOLLHUP)
		//{//closed
		//	printf("--- Connect faild 1 @ %d seconds\n",MC_X_TICK_TO_SECOND(MDK_GetTickCount() - m_pCurPkg->m_LastTm) );
		//	//OnLocalClosed();
		//	pl_Socket->OnDisconnect(TCP_DISCONNECT_ERROR, Errno);
		//	return;
		//}
		// 		if (events&EPOLLERR)
		// 		{//error
		// 			printf("--- Connect faild 2 @ %d seconds\n",MC_X_TICK_TO_SECOND(MDK_GetTickCount() - m_pCurPkg->m_LastTm) );
		// 			pl_Socket->OnDisconnect();
		// 			if (!pl_Socket->Ready())
		// 			{
		// 				DestroyChannel(pl_Socket);
		// 			}
		// 			return;
		// 		}
	}
	//	printf("-- %s %d %08X\n",__func__,__LINE__,events);
#endif
}

void Session_Base::DumpSockInfo()
{
#ifndef WIN32
#if 1
	struct tcp_info vl_info;
	if (!m_pMainSocket)
	{
		return;
	}

	socklen_t vl_Len = sizeof(struct tcp_info);
	x_memset(&vl_info, 0, sizeof(vl_info));
	//TCP_INFO
	if (getsockopt(m_pMainSocket->GetSocket(), SOL_TCP, TCP_INFO, (char *)&vl_info, &vl_Len) != 0)
	{
		perror("TCP_NODELAY");
	}
	else
	{
		printf("--- TCP[%d] INFO %d/%d: lost %u Retr:%u iRetr:%u MTU:%u SND MSS:%u RCV MSS:%u CWnd:%u RTT:%u TRetr:%u\n"
			, m_pMainSocket->GetSocket()
			, vl_Len, sizeof(struct tcp_info)
			, vl_info.tcpi_lost
			, vl_info.tcpi_retransmits
			, vl_info.tcpi_retrans
			, vl_info.tcpi_pmtu
			, vl_info.tcpi_snd_mss
			, vl_info.tcpi_rcv_mss
			, vl_info.tcpi_snd_cwnd
			, vl_info.tcpi_rtt
			, vl_info.tcpi_total_retrans
		);
	}

	t_clientSocket::iterator it;
	for (it = m_otherSocket.begin(); it != m_otherSocket.end(); ++it)
	{
		if (getsockopt((*it)->GetSocket(), SOL_TCP, TCP_INFO, (char *)&vl_info, &vl_Len) != 0)
		{
			perror("TCP_NODELAY");
		}
		else
		{
			printf("--- TCP[%d] INFO %d/%d: lost %u Retr:%u iRetr:%u MTU:%u SND MSS:%u RCV MSS:%u CWnd:%u RTT:%u TRetr:%u\n"
				, (*it)->GetSocket()
				, vl_Len, sizeof(struct tcp_info)
				, vl_info.tcpi_lost
				, vl_info.tcpi_retransmits
				, vl_info.tcpi_retrans
				, vl_info.tcpi_pmtu
				, vl_info.tcpi_snd_mss
				, vl_info.tcpi_rcv_mss
				, vl_info.tcpi_snd_cwnd
				, vl_info.tcpi_rtt
				, vl_info.tcpi_total_retrans
			);
		}
	}
#endif
#endif

}
#endif



int Session_Base::Send(bool vp_Txtype, bool vp_BufferedRetrans)
{
	x_asio_socket* pl_Sock = NULL;
	int vl_tx = 0;
	DVRR_WRITEMONITORLOG(0x90000021, 1000, NULL, false);
#if VM_MMST_FLOWCTL_PROP		// 媒体传输流量控制属性
#ifdef NETWORK_FLOW_STATICS
	m_nSendNum++;
#endif
#endif

	if ((NULL == m_pCurPkg) || (m_pCurPkg->GetPackSize() == 0))
	{
		return vl_tx;
	}
	pl_Sock = m_pCurPkg->GetParentSocket();
	//X_TRACE("Session_Base::Send pl_Sock= %p", pl_Sock);
	vl_tx = m_pCurPkg->SendPkg();
	// lyw 2014-03-17 3.0 version is not used
	//if (pl_Sock->IsTcp())
	//{
	//	if (pl_Sock->GetDataSizeInOutBuffer()>0)
	//	{//if any data waiting
	//		dvrr_upd_mmst_socket_handle( pl_Sock, DEFAULT_TCP_INIT_EVENT);
	//	}
	//	else
	//	{//减少没必要的调用. 在OnWrite中清掉 EPOLLOUT
	//		//dvrr_upd_mmst_socket_handle( GetSocketIndex(pl_Sock), DEFAULT_TCP_IO_EVENT);
	//	}
	//}
	if (vl_tx == 0)
	{
#ifndef FUNC_MMST_EPOLL_20				// 媒体传输网络版本2.0 使用EPOLL边缘触发 模块化
		OnSockDisconnected(pl_Sock);
#endif
		X_WARN("SockDisconnected when sending");
	}
	else
	{
		m_timer_deteck_socket_read.start_timer(DETECT_READ_EVENT_INTERVAL, 1);
	}
	return vl_tx;
}

#ifdef FUNC_MMST_EPOLL_20				// 媒体传输网络版本2.0 使用EPOLL边缘触发 模块化
// 连接完成
void Session_Base::onconnect(NetSpace::CIPClient* client, BOOL connected)
{
	X_TRACE_FL();
}

// 发送完成
void Session_Base::onsend(NetSpace::CIPClient* client)
{
	X_TRACE_FL();
}

// 接收到数据
void Session_Base::onrecv(NetSpace::CIPClient* client, char* buf, int len)
{
	X_TRACE_FL();
}

// 服务端断开
void Session_Base::ondisconnect(NetSpace::CIPClient* client)
{
	X_TRACE_FL();
}

#else
void Session_Base::OnSockRead(x_asio_socket* pp_Socket)
{
	X_TRACE_FL();
}

void Session_Base::OnSockConnected(x_asio_socket* pp_Socket)
{
	X_TRACE_FL();
}

void Session_Base::OnSockConnectFailed(x_asio_socket* pp_Socket)
{
	X_TRACE_FL();
}

void Session_Base::OnSockAccepted(x_asio_socket* pp_Socket)
{
	X_TRACE_FL();
}
void Session_Base::OnSockDisconnected(x_asio_socket* pp_Socket)
{
	X_TRACE_FL();
}
#endif


bool Session_Base::InStage(int vp_State)
{
	return m_State == vp_State;
}

void Session_Base::SetServerMode()
{
	m_bServerMode = true;
}


CMediaChCtx* Session_Base::GetCtx(u8 vl_Ch, u8 vl_Type, bool bAlloc)
{
	CMediaChCtx* pl_Ctx = NULL;
	vl_Type--;//SHIT
	if ((vl_Type < RTP_STREAM_TYPE_CNT) && (vl_Ch < MAX_RCV_CH))
	{
		pl_Ctx = g_Stm[vl_Type][vl_Ch];
		if (NULL == pl_Ctx && bAlloc)
		{
			g_Stm[vl_Type][vl_Ch] = x_new CMediaChCtx(vl_Ch, vl_Type);
			pl_Ctx = g_Stm[vl_Type][vl_Ch];
		}
	}
	return pl_Ctx;
}

CMmstPktMgr* Session_Base::GetCmdPacket()
{
#ifndef FUNC_MMST_EPOLL_20				// 媒体传输网络版本2.0 使用EPOLL边缘触发 模块化
	if (NULL != m_pMainSocket)
	{
		return (CMmstPktMgr*)m_pMainSocket->GetPacket();
	}
	else
#endif
	{
		return NULL;
	}
}

#ifdef FUNC_MMST_EPOLL_20				// 媒体传输网络版本2.0 使用EPOLL边缘触发 模块化
NetSpace::CIPClient * Session_Base::GetMainSocket()
{
	return m_pMainSocket;
}

NetSpace::CIPClient * Session_Base::CreateChannel(ipaddr_t vp_RemoteAddr, port_t vp_RemotePort, u8 pm_type, bool vp_bTcpMode, int vp_InsteadFd)
{
	NetSpace::CIPClient *pl_Socket = NULL;
	//int vl_connect_stratage = Convert_NetIntf_To_ConnectStrage(m_Param.v_netInfStratage);

	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
#ifdef WIN32
	sin.sin_addr.S_un.S_addr = vp_RemoteAddr;
#else
	#ifdef s_addr
		#undef s_addr
	#endif
	sin.sin_addr.s_addr = vp_RemoteAddr;
	#if 1
		#define s_addr  S_un.S_addr
	#endif
#endif

	X_TRACE("socket mmst remoteip=%s port=%d  %s", inet_ntoa(sin.sin_addr), vp_RemotePort, strerror(errno));
	if (vp_bTcpMode)
		pl_Socket = x_new NetSpace::CTCPClient(this, "MMST-TCP", inet_ntoa(sin.sin_addr), vp_RemotePort, mmstipnet_epool_add, mmstipnet_epool_mod, mmstipnet_epool_del, mmstipnet_getiprevbuff_opt);
	else
		pl_Socket = x_new NetSpace::CUDPClient(this, "MMST-UDP", inet_ntoa(sin.sin_addr), vp_RemotePort, mmstipnet_epool_add, mmstipnet_epool_mod, mmstipnet_epool_del, mmstipnet_getiprevbuff_opt);
	///+++ 需要增加 vl_connect_stratage类型
	//	pl_Socket = new tcp_channel(vp_RemoteAddr, vp_RemotePort, pm_type, vl_connect_stratage);
	//}
	//else
	//{
	//	pl_Socket = new udp_channel(vp_RemoteAddr, vp_RemotePort, pm_type, vl_connect_stratage);
	//}

	if (NULL == pl_Socket)
	{
		XERROR_FUNC();
		return NULL;
	}

	X_TRACE("CreateChannel succeess.");

	return pl_Socket;
}
#else
x_asio_socket *Session_Base::GetMainSocket()
{
	return m_pMainSocket;
}

void Session_Base::SetCurrentPkg(x_asio_socket* pp_MainSocket)
{
#ifdef FUNC_IS_IPCV3
	if (m_Param.v_ProtoType != ECT_UART)
	{
#endif
		if (NULL == pp_MainSocket)
		{
			m_pCurPkg = NULL;
			X_WARN("m_pCurPkg=%p", m_pCurPkg);
			return;
		}

		m_pCurPkg = (CMmstPktMgr*)pp_MainSocket->GetPacket();
		/*if (NULL != m_pCurPkg)
		{
		m_pCurPkg->m_LastTm = MDK_GetTickCount();
		}*/
#ifdef FUNC_IS_IPCV3
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
//发送算法
void Session_Base::OnSockSend(x_asio_socket* pp_Socket)
{
	//printf("-- %s %d\n",__func__,__LINE__);
	if ((NULL != pp_Socket) && pp_Socket->IsTcp() && pp_Socket->IsReady())
	{
		x_asio_tcp *pl_tcp = (x_asio_tcp *)pp_Socket;
		pl_tcp->WriteInnerBufferData();
	}
	else
	{
		x_sys_usleep(1);
	}
}


void Session_Base::CloseChannel(x_asio_socket* pp_Socket, E_RESET_CAUSE vp_Cause)
{
	//从通道要关闭. 通过主通道做通知.
	//关闭SOCKET
	if (!pp_Socket)
	{
		X_ERROR("CloseChannel: NULL == pp_Socket");
		return;
	}
	X_TRACE("Session_Base::CloseChannel ppsocket=%p, vp_cause=%d", pp_Socket, vp_Cause);
	t_mapSock2Ch::iterator its = m_mapSock2Ch.lower_bound(pp_Socket);
	t_mapSock2Ch::iterator ite = m_mapSock2Ch.upper_bound(pp_Socket);
	t_mapSock2Ch::iterator it;
	SetCurrentPkg(m_pMainSocket);//通过主通道来发
	for (it = its; it != ite; it++)
	{//可能关联到多个CH
		if (m_clientSock[it->second] == pp_Socket)
		{
			//关闭连接再打开连接然后才收到sock err关闭m_clientSock[it->second]已发生变化
			m_clientSock[it->second] = NULL;
		}
		//通过事件通知SERVER.
		if (!MC_IS_HIST_CH(it->second))
		{
			SendResetPacket((u8)it->second, vp_Cause);
		}
#ifdef FUNC_IS_IPCV3
		u8 vl_Flag;
		if (it->second < HISTROY_TRANS_CH_BASE)
		{
			if (m_aCh[it->second].HasAnySending())
			{
				vl_Flag = RTP_STREAM_TYPE_VIDEO | RTP_STREAM_TYPE_AUDIO;
				IPC_StopTransfer((u8)it->second, vl_Flag);
			}
		}
		else if (NULL != m_hist_channel)
		{
			DeleteHistChannel();
		}
#else
		StopCh((u8)it->second);
#endif
		//关闭通道上的所有媒体传输标记
		CloseChMedia((u8)it->second, 0x01, TS_STATE_IDLE);
		CloseChMedia((u8)it->second, 0x02, TS_STATE_IDLE);

		if ((it->second == AUDIO_SESSION_CHANNEL)/* && TestFlag(g_dvrr_State.v_TalkUploading, BIT1)*/)
		{
			StopRealTalk();
		}
	}

	dvrr_del_mmst_socket_handle(pp_Socket);
	pp_Socket->Close();
	X_WARN("m_otherSocket::erase=%d size=%d", pp_Socket, m_otherSocket.size());
	if (m_otherSocket.find(pp_Socket) != m_otherSocket.end())
		m_otherSocket.erase(pp_Socket);
	X_WARN("m_otherSocket::erase=%d size=%d", pp_Socket, m_otherSocket.size());

	m_mapSock2Ch.erase(its, ite);
	//MMST_TRACE_WARNING();
	if (m_otherSocket.size() == 0)
	{
		MMST_TRACE_WARNING();
		ResetAllCh(TS_STATE_IDLE, __FILE__, __LINE__, __func__);
	}

	//需要在最后释放socket，否则session析构删除main socket, ResetAllCh访问main socket/m_pCurPkg会越界
	x_delete pp_Socket;
	pp_Socket = NULL;
}

void Session_Base::SetTransferFlag(x_asio_socket* pp_Socket, int vp_Ch, int vp_Flag)
{
	if (pp_Socket->IsTcp())
	{
		tcp_channel* tcp = dynamic_cast<tcp_channel *>(pp_Socket);
		tcp->SetTransferFlag(vp_Ch, vp_Flag);
	}
	else
	{
		udp_channel* udp = dynamic_cast<udp_channel *>(pp_Socket);
		udp->SetTransferFlag(vp_Ch, vp_Flag);
	}
}

void Session_Base::ResetTransferFlag(x_asio_socket* pp_Socket, int vp_Ch, int vp_Flag)
{
	if (pp_Socket->IsTcp())
	{
		tcp_channel* tcp = dynamic_cast<tcp_channel *>(pp_Socket);
		tcp->ResetTransferFlag(vp_Ch, vp_Flag);
	}
	else
	{
		udp_channel* udp = dynamic_cast<udp_channel *>(pp_Socket);
		udp->ResetTransferFlag(vp_Ch, vp_Flag);
	}
}

int Session_Base::GetTransferFlag(x_asio_socket* pp_Socket, int vp_Ch)
{
	if (pp_Socket->IsTcp())
	{
		tcp_channel* tcp = dynamic_cast<tcp_channel *>(pp_Socket);
		return tcp->GetTrasferFlag(vp_Ch);
	}
	else
	{
		udp_channel* udp = dynamic_cast<udp_channel *>(pp_Socket);
		return udp->GetTrasferFlag(vp_Ch);
	}
}


//将SOCKET与CH关联起来
void Session_Base::RegisterChanel(x_asio_socket* pp_Socket, int vp_Ch)
{
	m_mapSock2Ch.insert(pair < x_asio_socket*, int >(pp_Socket, vp_Ch));
}

void Session_Base::UnRegisterChanel(x_asio_socket* pp_Socket, int vp_Ch)
{
	t_mapSock2Ch::iterator its = m_mapSock2Ch.lower_bound(pp_Socket);
	t_mapSock2Ch::iterator ite = m_mapSock2Ch.upper_bound(pp_Socket);
	t_mapSock2Ch::iterator it;
	for (it = its; it != ite; it++)
	{
		if (it->second == vp_Ch)
		{
			//m_clientSock[it->second] = NULL;
			m_mapSock2Ch.erase(it);
			break;
		}
	}
}

bool Session_Base::IsSockAttachedHisCh(x_asio_socket* pp_Socket)
{
	t_mapSock2Ch::iterator its = m_mapSock2Ch.lower_bound(pp_Socket);
	t_mapSock2Ch::iterator ite = m_mapSock2Ch.upper_bound(pp_Socket);
	t_mapSock2Ch::iterator it;
	for (it = its; it != ite; it++)
	{
		if (MC_IS_HIST_CH(it->second))
		{
			return true;
		}
	}
	return false;
}

bool Session_Base::IsSockExist(x_asio_socket* pp_Socket)
{
	if ((NULL == pp_Socket) || (NULL == m_pMainSocket))//川标存在mainSocket为NULL情况(连续告警)
	{
		return false;
	}

	if (m_pMainSocket == pp_Socket)
	{
		return true;
	}

	if (m_otherSocket.find(pp_Socket) != m_otherSocket.end())
	{
		return true;
	}

	return false;
}

x_asio_socket * Session_Base::CreateChannel(ipaddr_t vp_RemoteAddr, port_t vp_RemotePort, u8 pm_type, bool vp_bTcpMode, int vp_InsteadFd)
{
	x_asio_socket *pl_Socket = NULL;
#ifdef FUNC_MMST_EPOLL_20				// 媒体传输网络版本2.0 使用EPOLL边缘触发 模块化
#else
	int vl_connect_stratage = Convert_NetIntf_To_ConnectStrage(m_Param.v_netInfStratage);

	if (vp_bTcpMode)
	{
		pl_Socket = x_new tcp_channel(vp_RemoteAddr, vp_RemotePort, pm_type, vl_connect_stratage);
	}
	else
	{
		pl_Socket = x_new udp_channel(vp_RemoteAddr, vp_RemotePort, pm_type, vl_connect_stratage);
	}

	if (NULL == pl_Socket)
	{
		XERROR_FUNC();
		return NULL;
	}

	if (-1 != vp_InsteadFd)
	{
		pl_Socket->Attach(vp_InsteadFd);
		pl_Socket->SetNonblocking();
	}
	else
	{
		if (!pl_Socket->Open())
		{
			X_ERROR("Mmst: pl_Socket->Open() err");
			XERROR_FUNC();
			x_delete pl_Socket;
			pl_Socket = NULL;
			return NULL;
		}
	}

	int vl_ret = dvrr_add_mmst_socket_handle(pl_Socket, (vp_bTcpMode ? DEFAULT_TCP_INIT_EVENT : DEFAULT_UDP_INIT_EVENT), this);
	if (0 != vl_ret)
	{
		X_ERROR("Mmst: dvrr_add_mmst_socket_handle return err (vl_ret=%d)", vl_ret);
		XERROR_FUNC();
		x_delete pl_Socket;
		pl_Socket = NULL;
		return NULL;
	}
#endif
	X_TRACE("CreateChannel succeess.");
	return pl_Socket;
}

void Session_Base::StartTransferForSocket(x_asio_socket* pp_Socket)
{//启动SOCKET相关的传输
	int vl_flag = 0;
	t_mapSock2Ch::iterator its = m_mapSock2Ch.lower_bound(pp_Socket);
	t_mapSock2Ch::iterator ite = m_mapSock2Ch.upper_bound(pp_Socket);
	t_mapSock2Ch::iterator it;
	X_TRACE_FL();
	tcp_channel* tcp = dynamic_cast<tcp_channel *>(pp_Socket);
	ASSERT(NULL != tcp);
	if (NULL == tcp)
		return;

#ifdef FUNC_IS_IPCV3
	#ifndef SUPPORT_BMIPC_EPOLL_20		// IPC使用Epoll 边缘触发
		for (it = its; it != ite; ++it)
		{
			vl_flag = tcp->GetTrasferFlag(it->second);

#ifdef PROP_STOR_DATA_FETCH			// 提取磁盘数据属性
			if (MC_IS_HIST_CH(it->second) && (NULL != m_hist_channel))
			{
#ifdef PROP_PRO_TYPE_IPCSELF_ON				// IPC协议
				if (!((ipcclient_history_upload_channel *)m_hist_channel)->start(m_cur_hist_type))
				{
					TryCloseChanel(it->second, vl_flag);
				}
#endif
				continue;
			}
#endif

			if (vl_flag != 0)
			{
				X_TRACE("StartTransferForSocket, vl_flag:%d", vl_flag);

				if (vl_flag & RTP_STREAM_TYPE_VIDEO)
					StartChMediaEx((u8)it->second, RTP_STREAM_TYPE_VIDEO);

				if (vl_flag & RTP_STREAM_TYPE_AUDIO)
					StartChMediaEx((u8)it->second, RTP_STREAM_TYPE_AUDIO);
			}
		}
	#endif
#else

	for (it = its; it != ite; ++it)
	{//可能关联到多个CH
		if (AUDIO_SESSION_CHANNEL == it->second)
		{
			if (g_dvrr_State.v_TalkMode & BIT1)
			{
				X_TRACE("StartTransferForSocket, start mmst audio for ipcall,ch:%d", g_dvrr_State.v_TalkUploadCh);
				int vl_ret = dvrr_start_mmst_audio(g_dvrr_State.v_TalkUploadCh, DVRR_MEDIA_USER_IPCALL);
				if (HI_SUCCESS != vl_ret)
				{
					X_TRACE_FL();
					ClearFlag(g_dvrr_State.v_TalkUploading, BIT1);
					g_dvrr_State.v_TalkMode = 0;
				}
			}
			continue;
		}
		else if (MC_IS_HIST_CH(it->second))
		{
			continue;
		}
		vl_flag = tcp->GetTrasferFlag(it->second);
		//ASSERT(vl_flag !=0);
		if (vl_flag != 0)
		{
			int vl_ret = 0;
			X_TRACE("Mmst: start pending transfer for ch%d socket%d flag:%2x", it->second, it->first->GetSocket(), vl_flag);
#if defined(SUPPORT_IPC_FUN) || defined(SUPPORT_EXTERNAL_IPC_FUN)
			if ((MC_IS_YWIPC_CFG()) && (it->second >= dvrr_get_max_cam_num() && it->second < (dvrr_get_max_cam_num() + MAX_IPC_REAL_TRANS_CH)))
			{
				// 有为IPC摄像头
				X_TRACE("Mmst: is ipc (Chnl=%u, v_Cbr=%u, v_VideoType=%u, v_Frame=%u, v_BitRate=%u, v_GOP=%u)", it->second, m_aCh[it->second].m_cfg.v_Cbr, m_aCh[it->second].m_cfg.v_VideoType, m_aCh[it->second].m_cfg.v_Frame, m_aCh[it->second].m_cfg.v_BitRate, m_aCh[it->second].m_cfg.v_GOP);
#ifdef DXR_VIDEO_ONLY_LOCAL_TRANSFER						// T4本地视频传输
#ifdef DXR_IPC_TRANSMODE                       //IPC传输模式
                if (g_dvrr_State.v_ipcTransState == 1)
                {

                    X_TRACE("ipc trans from local to remote");
                    ipc_StopTransfer(2, (u8)vl_flag);
                    ipc_StopTransfer(3, (u8)vl_flag);
                    //g_dvrr_State.v_mmstLocalHandle = NULL;
                    g_dvrr_State.v_ipcTransState = 0;
                }
#endif
    		    {
    		        X_TRACE("rtmp start ipc transfer");
#ifdef DXR_IPC_TRANSMODE                       //IPC传输模式
                    g_dvrr_State.v_ipcRemoteState[it->second - 2] = 1;
#endif
    		        vl_ret = ipc_StartTransfer((u8)it->second, (u8)vl_flag, &(m_aCh[it->second].m_cfg), this);
                    if (0 == vl_ret)
                        g_dvrr_State.v_ipcTransState = 2;
    		    }
                //else
                  //  vl_ret = 0;
#else
				vl_ret = ipc_StartTransfer(it->second, vl_flag, &(m_aCh[it->second].m_cfg), this);
#endif
			}
			else if ((MC_IS_EXTERNL_IPC_CFG()) && (it->second >= dvrr_get_max_cam_num() && it->second < (dvrr_get_max_cam_num() + MAX_NMA_CH_NUM)))
			{
				// 标准IPC摄像头
				X_TRACE("Mmst: is externl ipc (Chnl=%u, v_Cbr=%u, v_VideoType=%u, v_Frame=%u, v_BitRate=%u, v_GOP=%u)", it->second, m_aCh[it->second].m_cfg.v_Cbr, m_aCh[it->second].m_cfg.v_VideoType, m_aCh[it->second].m_cfg.v_Frame, m_aCh[it->second].m_cfg.v_BitRate, m_aCh[it->second].m_cfg.v_GOP);
				vl_ret = extIpc_StartTransfer((u8)it->second, (u8)vl_flag, &(m_aCh[it->second].m_cfg), this);
			}
			else
#endif
			{
				// 普通摄像头
				X_TRACE("Mmst: is local trans (Chnl=%u, v_Cbr=%u, v_VideoType=%u, v_Frame=%u, v_BitRate=%u, v_GOP=%u)", it->second, m_aCh[it->second].m_cfg.v_Cbr, m_aCh[it->second].m_cfg.v_VideoType, m_aCh[it->second].m_cfg.v_Frame, m_aCh[it->second].m_cfg.v_BitRate, m_aCh[it->second].m_cfg.v_GOP);
				vl_ret = mmst_StartTransfer((u8)it->second, (u8)vl_flag, &(m_aCh[it->second].m_cfg), this);

			}

			if (0 != vl_ret)
			{
				X_TRACE("Mmst: start transfer err (vl_ret=%u)", vl_ret);
				OnSysError("StartTransfer");
			}

			if (0 == vl_ret)
			{
#define _DVRR_REC_CHN_VIDEO 0x01
#define _DVRR_REC_CHN_AUDIO 0x02
				if (vl_flag & _DVRR_REC_CHN_VIDEO)
				{
					StartChMedia((u8)it->second, _DVRR_REC_CHN_VIDEO);
				}

				if (vl_flag & _DVRR_REC_CHN_AUDIO)
				{
					StartChMedia((u8)it->second, _DVRR_REC_CHN_AUDIO);
				}

			}
		}
		//vl_flag = ((~vl_flag)&vl_flag);
		//tcp->ResetTransferFlag(it->second, vl_flag);
	}

#endif
	//SOCKET NB??
	dvrr_upd_mmst_socket_handle(pp_Socket, DEFAULT_TCP_IO_EVENT);
}
#endif

void Session_Base::ResetAllCh(t_TrasferSessionState st, const char* pp_File, int vp_Line, const char* pp_Func)
{
	u8 i;
	int j;
	SetState(st, pp_File, vp_Line, pp_Func);
	for (i = 0; i < MAX_TRANS_CH; ++i)
	{
		if (m_aCh[i].HasAnySending())
		{
			for (j = 0; j < RTP_STREAM_TYPE_CNT; ++j)
			{
				//MMST_TRACE("Noooo.%d\n",j);
				if (m_aCh[i].m_stm[j].CanReseting())
				{
					m_aCh[i].m_stm[j].Clear();
					if (i >= HISTROY_TRANS_CH_BASE)
					{
						m_aCh[i].m_stm[j].SetState(MMST_MEDIA_STATE_CLOSED);
					}
					else if (st == TS_STATE_RESETING)
					{
						m_aCh[i].m_stm[j].SetState(MMST_MEDIA_STATE_RESETING);
					}
					else
					{
						m_aCh[i].m_stm[j].SetState(MMST_MEDIA_STATE_CLOSING);
					}
				}
			}
		}
	}
}

// 删除历史通道实例
void Session_Base::DeleteHistChannel(bool bCloseForce)
{
	if (NULL != m_hist_channel)
	{
		if (bCloseForce && !m_hist_channel->is_closed())
			m_hist_channel->close(true);

		x_delete m_hist_channel;
		m_hist_channel = NULL;
	}
}

void Session_Base::StopRealTalk()
{
	StopChMedia(AUDIO_SESSION_CHANNEL, RTP_STREAM_TYPE_AUDIO, TS_STATE_IDLE);
	if (m_DownMediaDataBuffSize > m_DownMediaDataOffset)
	{
		u32 vl_len = m_DownMediaDataBuffSize - m_DownMediaDataOffset;
		if (2 == m_audioFormat && vl_len > m_DownMediaDataRestlen)//g726
		{
			vl_len -= m_DownMediaDataRestlen;
		}
		//发送到APP
		X_TRACE("vl_MediaLen = %d, m_audioFormat=0x%x", vl_len, m_audioFormat);
		MC_CALL_CB(TEV_TALK_DATA, m_audioFormat, vl_len, (const char*)m_pDownMediaDataBuff + m_DownMediaDataOffset);
	}
	m_DownMediaDataBuffSize = 0;
	m_DownMediaDataOffset = 0;
	m_DownMediaDataRestlen = 0;

	if (g_dvrr_State.v_TalkMode & BIT1)//启动过传输，需停止
	{
		dvrr_stop_mmst_audio(g_dvrr_State.v_TalkUploadCh, DVRR_MEDIA_USER_IPCALL);
	}

	ClearFlag(g_dvrr_State.v_TalkUploading, BIT1);
	g_dvrr_State.v_TalkMode = 0;
}

void Session_Base::TryCloseChanel(int vp_Ch, int vp_Flag)
{
#ifndef FUNC_MMST_EPOLL_20				// 媒体传输网络版本2.0 使用EPOLL边缘触发 模块化
	x_asio_socket* pl_Socket = m_clientSock[vp_Ch];

	if (NULL != pl_Socket)
	{
		ResetTransferFlag(pl_Socket, vp_Ch, vp_Flag);
		if (!GetTransferFlag(pl_Socket, vp_Ch))
		{
			X_TRACE("TryCloseChanel vp_Ch:%d, pl_Socket:%p, sock2ch size:%d", vp_Ch, pl_Socket, m_mapSock2Ch.size());
			UnRegisterChanel(pl_Socket, vp_Ch);
			m_clientSock[vp_Ch] = NULL;

			if ((m_pMainSocket != pl_Socket) && (m_mapSock2Ch.find(pl_Socket) == m_mapSock2Ch.end()))
			{
				CloseChannel(pl_Socket, RSC_USERBREAK);
			}
		}
	}
#endif
}

void Session_Base::StopCh(u8 vp_Ch)//停止一个通道的所有传输
{
	int vl_Flag;
	if (vp_Ch < HISTROY_TRANS_CH_BASE)
	{
		if (m_aCh[vp_Ch].HasAnySending())
		{
			vl_Flag = RTP_STREAM_TYPE_VIDEO | RTP_STREAM_TYPE_AUDIO;
#if defined(SUPPORT_IPC_FUN) || defined(SUPPORT_EXTERNAL_IPC_FUN)
			if ((MC_IS_YWIPC_CFG()) && (vp_Ch >= dvrr_get_max_cam_num() && vp_Ch < (dvrr_get_max_cam_num() + MAX_IPC_REAL_TRANS_CH)))
			{
#ifdef DXR_VIDEO_ONLY_LOCAL_TRANSFER						// T4本地视频传输
		        X_TRACE("rtmp ipc transfer");
#ifdef DXR_IPC_TRANSMODE                       //IPC传输模式
				if ((vp_Ch - 2) >= 0
					&& (vp_Ch - 2) < YW_ARRAY_SIZE(g_dvrr_State.v_ipcRemoteState))
				{
					g_dvrr_State.v_ipcRemoteState[vp_Ch - 2] = 0;
				}
#endif
				ipc_StopTransfer(vp_Ch, (u8)vl_Flag);
#else
				ipc_StopTransfer(vp_Ch, (u8)vl_Flag);
#endif
			}

			if ((MC_IS_EXTERNL_IPC_CFG()) && (vp_Ch >= dvrr_get_max_cam_num() && vp_Ch < (dvrr_get_max_cam_num() + MAX_NMA_CH_NUM)))
			{
				extIpc_StopTransfer(vp_Ch, (u8)vl_Flag);
			}
			else
#endif
			{
				mmst_StopTransfer(vp_Ch, (u8)vl_Flag);
			}
		}
	}
}

void Session_Base::StopHistCh(u8 vp_Ch)//停止一个通道的所有传输
{
#if VM_MMST_FLOWCTL_PROP		// 媒体传输流量控制属性
	m_aTransCtrl[vp_Ch].set(0, 0);
#endif
	if (vp_Ch >= HISTROY_TRANS_CH_BASE)
	{
		if (m_aCh[vp_Ch].HasAnySending())
		{
			CloseChMedia(vp_Ch, RTP_STREAM_TYPE_VIDEO, TS_STATE_IDLE);
			CloseChMedia(vp_Ch, RTP_STREAM_TYPE_AUDIO, TS_STATE_IDLE);
		}
	}
}

//PUSH PACKET 到 SOCKET
//注意:
//按新实现, TCP SOCKET内部有动态的TX BUFFER, 可以通过 GetOutputLength 取得BUFFER 的字节数
//相当于双缓冲
void Session_Base::TcpPushPacket(int sendnum)
{
	int i;
	int vl_sent = 0;
#define MAX_PUSH_PACKET		512
	int u32maxsendnum = MAX_PUSH_PACKET;
	DVRR_WRITEMONITORLOG(0x80000f00, 1000, NULL, false);
	if (sendnum > 0)
	{
		u32maxsendnum = sendnum;
	}

#if VM_MMST_FLOWCTL_PROP		// 媒体传输流量控制属性
#ifdef NETWORK_FLOW_STATICS
	Session_Base::m_nTcpPushNum++;
#endif
#endif

	if (CanSendNext())
	{
		for (i = 0; i < u32maxsendnum; ++i)
		{//
			vl_sent = SendNextCh();
			if (vl_sent == 0)//Send 本轮未发
			{
				vl_sent = SendNextCh();
				if (vl_sent == 0)
				{//无数据了
#ifdef TCP_SEND_CONGESTION
					printf("don't send any data this time.%s%d\n", __func__, __LINE__);
#endif
					break;
				}
			}
		}
	}
	else
	{
		X_ASSERT(false);
		MMST_TRACE("TcpPushPacket:CanSendNext false,m_TxBytesWaiting:%u\n", CMmstMedia::m_TxBytesWaiting);
	}
}

bool Session_Base::IsServerMode()
{
	return m_bServerMode;
}

#if VM_MMST_FLOWCTL_PROP		// 媒体传输流量控制属性
u32 Session_Base::GetTcpPushNum()
{
	return Session_Base::m_nTcpPushNum;
}

u32 Session_Base::GetSendNum()
{
	return Session_Base::m_nSendNum;
}

int Session_Base::FlowLimitProc(u32 vp_CurTick, u32 vp_Len, u8 vp_Ch, u8 vp_type)
{
	//X_TRACE_FL();
	return -1;
}

void Session_Base::Update(u32 vp_CurrentDayFlow, u32 vp_CurrentMonthFlow)
{
	Session_Base::m_CurrentDayFlow = vp_CurrentDayFlow;
	Session_Base::m_CurrentMonthFlow = vp_CurrentMonthFlow;
}
#endif

void Session_Base::StaticsNetworkFlow()
{
#ifdef NETWORK_FLOW_STATICS
	static u32 vl_nPreTotalFlow = 0;
	static u32 vl_nPreTryWriteNum = 0;
	static u32 vl_nPreReTryWriteNum = 0;
	static u32 vl_nPreWriteBuffNum = 0;
#if VM_MMST_FLOWCTL_PROP
	static u32 vl_nTcpPushNum = 0;
	static u32 vl_nSendNum = 0;
#endif

	u32 vl_TF = x_asio_tcp::GetCurTotalFlow();
	u32 vl_TW = x_asio_tcp::GetTryWriteNum();
	u32 vl_TRW = x_asio_tcp::GetReTryWriteNum();
	u32 vl_WB = x_asio_tcp::GetWriteBuffNum();
#if VM_MMST_FLOWCTL_PROP
	u32 vl_TPN = GetTcpPushNum();
	u32 vl_TSN = GetSendNum();
#endif

#if 1
    static u32 sl_tick = MC_X_GET_TICK();
    if ((MC_X_GET_TICK() - sl_tick) < 100)
    {
        return;
    }
	else
	{
	    sl_tick =  MC_X_GET_TICK();
	}
#endif

#if VM_MMST_FLOWCTL_PROP
	X_PRINT("=== TF=%u AF=%u AW=%u ARW=%u AWB=%u TPN=%u TSN=%u CB=%u TBW=%u", vl_TF, (vl_TF - vl_nPreTotalFlow), (vl_TW - vl_nPreTryWriteNum), (vl_TRW - vl_nPreReTryWriteNum), (vl_WB - vl_nPreWriteBuffNum), (vl_TPN - vl_nTcpPushNum), (vl_TSN - vl_nSendNum), GetDataSizeInOutBuffer(), CMmstMedia::m_TxBytesWaiting);
#else
	X_PRINT("=== TF=%u AF=%u AW=%u ARW=%u AWB=%u CB=%u TBW=%u===", vl_TF, (vl_TF - vl_nPreTotalFlow), (vl_TW - vl_nPreTryWriteNum), (vl_TRW - vl_nPreReTryWriteNum), (vl_WB - vl_nPreWriteBuffNum), GetDataSizeInOutBuffer(), CMmstMedia::m_TxBytesWaiting);
#endif
	x_asio_tcp::PrintWArrayTick();
	vl_nPreTotalFlow = vl_TF;
	vl_nPreTryWriteNum = vl_TW;
	vl_nPreReTryWriteNum = vl_TRW;
	vl_nPreWriteBuffNum = vl_WB;
#if VM_MMST_FLOWCTL_PROP
	vl_nTcpPushNum = vl_TPN;
	vl_nSendNum = vl_TSN;
#endif

#endif
}


void Session_Base::SetSessionID(t_TransferSessionID vp_ID)
{
	m_SessionID = vp_ID;
}

t_TransferSessionID Session_Base::GetSessionID()
{
	return m_SessionID;
}

#if 0
void Session_Base::DeleteQueue(t_stremReq* pp_StmQueued)
{
	while (!pp_StmQueued->empty())
	{
		FreeRTPStream(pp_StmQueued->front());
		pp_StmQueued->pop();
	}
	x_delete pp_StmQueued;
	pp_StmQueued = NULL;
}
#endif

bool Session_Base::HasAnyTransfering()
{
	return m_TransChFlag != 0;
}

bool Session_Base::IsInTcp()
{
	return m_Param.v_ProtoType == TPT_TCP;
}

void Session_Base::TraceMMSTQueryStatics(u16 msgId)
{
	X_TRACE("QS:[0x%04x] Qn=%u Qns=%u Ln=%u Li=%u Lis=%u diff=%u", (msgId), g_MmstQueryStatics.v_QueryNum, g_MmstQueryStatics.v_QuerySuccessNum, g_MmstQueryStatics.v_QueryCurListNum, g_MmstQueryStatics.v_GetCurIdx, g_MmstQueryStatics.v_getCurSuccessIdx, (g_MmstQueryStatics.v_GetCurIdx - g_MmstQueryStatics.v_getCurSuccessIdx));
}

void Session_Base::OnDectecSocketReadEvent(unsigned long vp_count, void *pp_ctx)
{
	MMST_TRACE("!!!!33!!!!!!!!!!!!Wait Socket read event timerout!\n");
	//(void)fflush(stdout);
	SetState(TS_STATE_ERROR, __FILE__, __LINE__, __func__, TEE_SERVER_NO_RESPONSE);
}
//////////////////////////////////////////////////////////////////////////

#ifdef FUNC_FLOW_AND_MEDIA_11

FlowSumMedia::FlowSumMedia(void)
{
	m_nFlow = 0;
}
FlowSumMedia::~FlowSumMedia(void)
{
}
void FlowSumMedia::Add(u64 nDataLen)
{
	m_nFlow += nDataLen;
}
void FlowSumMedia::Clear(void)
{
	m_nFlow = 0;
}
u64 FlowSumMedia::GetFlow(void)
{
	return m_nFlow;
}
//////////////////////////////////////////////////////////////////////////
FlowSumChnl::FlowSumChnl(void)
{
	// 注意创建的顺序,必须跟"DataTypeToPos函数"内的转换顺序相同
	m_arrMediaFlow.push_back(x_new FlowSumMedia()); // 视频
	m_arrMediaFlow.push_back(x_new FlowSumMedia()); // 音频
	m_arrMediaFlow.push_back(x_new FlowSumMedia()); // 图片
	m_arrMediaFlow.push_back(x_new FlowSumMedia()); // 通信命令
}
FlowSumChnl::~FlowSumChnl(void)
{
	for (u32 i = 0; i < m_arrMediaFlow.size(); ++i)
	{
		if (NULL != m_arrMediaFlow[i])
		{
			x_delete m_arrMediaFlow[i];
			m_arrMediaFlow[i] = NULL;
		}
	}
	m_arrMediaFlow.clear();
}
void FlowSumChnl::Add(u32 nDataType, u64 nDataLen)
{
	s32 nPos = DataTypeToPos(nDataType);
	if (nPos < 0)
		return;
	m_arrMediaFlow[nPos]->Add(nDataLen);
}
u64 FlowSumChnl::GetFlow(u32 nDataType)
{
	s32 nPos = DataTypeToPos(nDataType);
	if (nPos < 0)
		return 0;
	return m_arrMediaFlow[nPos]->GetFlow();
}
void FlowSumChnl::Clear(void)
{
	for (u32 i = 0; i < m_arrMediaFlow.size(); ++i)
	{
		if (NULL != m_arrMediaFlow[i])
		{
			m_arrMediaFlow[i]->Clear();
		}
	}
}
s32 FlowSumChnl::DataTypeToPos(u32 nType)
{
	// 注意:若此处对应关系有改动,那FlowSumChnl::FlowSumChnl(void)中的顺序也要跟着调整
	switch (nType)
	{
	case FST_VIDEO:	return 0;	// 视频
	case FST_AUDIO:	return 1;	// 音频
	case FST_PIC: return 2;		// 图片频
	case FST_CMD: return 3;		// 通信命令帧
	}
	return -1;
}
//////////////////////////////////////////////////////////////////////////

CFlowSumMng g_cFlowSumMng;
CFlowSumMng::CFlowSumMng(void)
{
	m_nVideoFlowMonth = 0;
	m_nVideoFlowDay = 0;
	m_nCmdFlowMonth = 0;
	m_nCmdFlowDay = 0;
	m_nNetCardFlowTag = 0;

	m_nState = 0;
	m_nLastSaveTick = 0;

	// 默认5分钟保存一次
	// 说明1: 本来设计为30分钟保存一次的,但因为我们无法保证掉电时一定保存成功,而若保存失败会丢失数据,所以把保存频率改小
	// 说明2: 掉电保存成功率不高的问题有待解决,目前先在软件控制上绕过此问题,先保证宇通过检通过
	// 说明3: 已跟刘工(刘迎午)核实,5分钟保存一次不会对flash造成损坏
	m_nSaveIntvl = 5 * 60;

	m_nDay = 0;
	m_nMonth = 0;

	// 构造时并不知道实际摄像头个数,所以这里预留两倍的最大摄像头数,足够满足各种需求
	u32 nMaxChnlCnt = CH_MAX * 2;
	for (u32 i = 0; i < nMaxChnlCnt; ++i)
	{
		m_arrChnlSum.push_back(x_new FlowSumChnl());
	}
}
CFlowSumMng::~CFlowSumMng(void)
{
	for (u32 i = 0; i < m_arrChnlSum.size(); ++i)
	{
		if (NULL != m_arrChnlSum[i])
		{
			x_delete m_arrChnlSum[i];
			m_arrChnlSum[i] = NULL;
		}
	}
	m_arrChnlSum.clear();
}
void CFlowSumMng::Start()
{
	SetState(S_INIT);
	Scan(); // 立即执行
}
BOOL CFlowSumMng::IsDoing()
{
	if (m_nState == S_END)
		return FALSE;
	else
		return TRUE;
}
void CFlowSumMng::Stop()
{
	SetState(S_END);
}
void CFlowSumMng::Add(u32 nChnl, u32 nDataType, u64 nDataLen)
{
	// 找到流量统计器
	FlowSumChnl* pFlowSum = Find(nChnl);
	if (NULL == pFlowSum)
	{
		// 历史回放时,这里会频繁的打印,应该是逻辑没调好,所以暂时屏蔽打印,以后有时间要查逻辑上的问题
		X_ERR("FlowSum: Add: err, chnl no found (Chnl=%u, DataType=%u, DataLen=%llu, m_arrChnlSum.size()=%u)", nChnl, nDataType, nDataLen, m_arrChnlSum.size());
		return;
	}

	pFlowSum->Add(nDataType, nDataLen);
}
void CFlowSumMng::Scan(void)
{
	if (!IsDoing())
		return;

	switch (GetState())
	{
	case S_INIT: // 初始化
		{
			m_nLastSaveTick = CObjectA::GetNowTick();

			// 各通道流量清0
			AllChnlFlowClear();

			// 现在不需要给月/日赋值,而在后面的"FlowLoad()"函数会载入上次保存的月/日
			m_nMonth = 0;
			m_nDay = 0;

			SetState(S_LOAD);
		}
		break;
	case S_LOAD: // 从存储介质中载入之前保存的数据
		{
			FlowLoad();
			SetState(S_READY);
		}
		break;
	case S_READY: // 就绪
		{
			// 每10秒输出一次基本信息
			static u32 nLastPrintTick = 0;
			if (CObjectA::GetNowTick() - nLastPrintTick > 10000)
			{
				nLastPrintTick = CObjectA::GetNowTick();
				u32 nRx = 0; u32 nTx = 0;
				MDK_Get_GPRSBytesCount(0, &nRx, &nTx);
				X_DEBUG("FlowSum: Net: CardTag=%llu,Rx=%u(B),Tx=%u(B), Video[DayBase=%llu(B),MonBase=%llu(B),Chg=%llu(B)], Cmd[DayBase=%llu(B),MonBase=%llu(B),Chg=%llu(B)]"
					, m_nNetCardFlowTag, nRx, nTx
					, m_nVideoFlowDay, m_nVideoFlowMonth, GetAllChnlMediaFlow(FST_VIDEO)
					, m_nCmdFlowDay, m_nCmdFlowMonth, GetAllChnlMediaFlow(FST_CMD));
			}

			// 是否到时间保存
			if (CObjectA::CalcEscapeMsec(m_nLastSaveTick) >= m_nSaveIntvl * 1000)
			{
				m_nLastSaveTick = CObjectA::GetNowTick();

				// 保存流量
				FlowSave();
			}

			// 获取当前的月份和日期
			u32 nYear = 0; u32 nMonth = 0; u32 nDay = 0; u32 nTemp = 0;
			u32 nLocTime = getLocalTime(AppFormsSpace::Apt_GetUtcFrom2000());
			x_utc_to_tms(nLocTime, nYear, nMonth, nDay, nTemp, nTemp, nTemp);

			// 必须是有效时间才判断是否跨天(防止终端启动时时间未校准的情况)
			if (nYear > 2010)
			{
				// 若过了一月,则日/月流量都清0
				if (m_nMonth != nMonth)
				{
					OnPassDay(nDay);
					OnPassMonth(nMonth);
				}
				// 若过了一日,则日流量清0
				else if (m_nDay != nDay)
				{
					OnPassDay(nDay);
				}
			}
		}
		break;
	}
}
u32 CFlowSumMng::GetVideoDayFlow(void)
{
	// 累加当前流量(字节)
	u64 nCurFlow = GetAllChnlMediaFlow(FST_VIDEO);

	// 总日流量 = 之前记录的日流量 + 当前流量
	return (u32)((m_nVideoFlowDay + nCurFlow) / 1024);
}
u32 CFlowSumMng::GetVideoMonthFlow(void)
{
	// 累加当前流量(字节)
	u64 nCurFlow = GetAllChnlMediaFlow(FST_VIDEO);

	// 总月流量 = 之前记录的月流量 + 当前流量
	return (u32)((m_nVideoFlowMonth + nCurFlow) / 1024);
}
u32 CFlowSumMng::GetPicDayFlow(void)
{
	return 0; // 图片流量由MA来做
}
u32 CFlowSumMng::GetPicMonthFlow(void)
{
	return 0; // 图片流量由MA来做
}
u32 CFlowSumMng::GetCmdDayFlow(void)
{
	// 累加当前流量(字节)
	u64 nCurFlow = GetAllChnlMediaFlow(FST_CMD);
	
	// 总日流量 = 之前记录的日流量 + 当前流量
	return (u32)((m_nCmdFlowDay + nCurFlow) / 1024);
}
u32 CFlowSumMng::GetCmdMonthFlow(void)
{
	// 累加当前流量(字节)
	u64 nCurFlow = GetAllChnlMediaFlow(FST_CMD);

	// 总月流量 = 之前记录的月流量 + 当前流量
	return (u32)((m_nCmdFlowMonth + nCurFlow) / 1024);
}
u64 CFlowSumMng::GetNetCardFlowSum(void)
{
	u32 nRx = 0;
	u32 nTx = 0;
	MDK_Get_GPRSBytesCount(0, &nRx, &nTx);
	return (u64)(nRx + nTx);
}
void CFlowSumMng::SetSaveIntvl(u32 nSeconds)
{
	// 保存间隔不允许少于10秒
	if (nSeconds < 10)
		nSeconds = 10;

	m_nSaveIntvl = nSeconds;
	X_INFO("FlowSum: SaveIntvl: Set: (Intvl=%u)", m_nSaveIntvl);
}
u32 CFlowSumMng::GetSaveIntvl(void)
{
	return m_nSaveIntvl;
}
void CFlowSumMng::OnPassDay(u32 nNewDay)
{
	X_INFO("FlowSum: PassDay (DayOld=%u,DayNew=%u,Time=%u,FlowMonth=%u(KB),FlowDay=%u(KB))", m_nDay, nNewDay, AppFormsSpace::Apt_GetUtcFrom2000(), GetVideoMonthFlow(), GetVideoDayFlow());

	m_nDay = nNewDay;

	// 历史记录的日流量清0
	m_nVideoFlowDay = 0;
	m_nCmdFlowDay = 0; 
	
	// 各通道的当前流量全部清0
	AllChnlFlowClear();

	//更新网卡流量标签
	m_nNetCardFlowTag = GetNetCardFlowSum();

	// 保存日流量(目的是将存储介质上的日流量清0)
	FlowSaveDay();
}
void CFlowSumMng::OnPassMonth(u32 nNewMonth)
{
	// 清空月流量
	X_INFO("FlowSum: PassMonth (MonthOld=%u,MonthNew=%u,Time=%u,FlowMonth=%u(KB),FlowDay=%u(KB))", m_nMonth, nNewMonth, AppFormsSpace::Apt_GetUtcFrom2000(), GetVideoMonthFlow(), GetVideoDayFlow());

	m_nMonth = nNewMonth;

	// 历史记录的月流量清0
	m_nVideoFlowMonth = 0;
	m_nCmdFlowMonth = 0;

	// 各通道的当前流量全部清0
	AllChnlFlowClear();

	//更新网卡流量标签
	m_nNetCardFlowTag = GetNetCardFlowSum();

	// 保存月流量(目的是将存储介质上的月流量清0)
	FlowSaveMonth();
}
void CFlowSumMng::PrintMyInfo(void)
{
	X_INFO("FlowSum: Print: State=%u, Day:[Day=%u,Flow=%llu(B)], Month:[Month=%u,Flow=%llu(B)], SaveIntvl=%u(s)", m_nState, m_nDay, m_nVideoFlowDay, m_nMonth, m_nVideoFlowMonth, m_nSaveIntvl);

	for (u32 i = 0; i < m_arrChnlSum.size(); ++i)
	{
		if (NULL != m_arrChnlSum[i])
		{
			X_INFO("FlowSum: Print: Chnl[%u]: AV=%llu(B), Pic=%llu(B), Cmd=%llu(B))", i + 1, m_arrChnlSum[i]->GetFlow(FST_VIDEO), m_arrChnlSum[i]->GetFlow(FST_PIC), m_arrChnlSum[i]->GetFlow(FST_CMD));
		}
	}
}
void CFlowSumMng::SetState(u32 nState)
{
	m_nState = nState;
}
u32 CFlowSumMng::GetState()
{
	return m_nState;
}
FlowSumChnl* CFlowSumMng::Find(u32 nChnl)
{
	if ((nChnl < 1) || (nChnl > m_arrChnlSum.size()))
		return NULL;
	return m_arrChnlSum[nChnl - 1];
}
void CFlowSumMng::AllChnlFlowClear()
{
	// 各通道流量清0
	for (u32 i = 0; i < m_arrChnlSum.size(); ++i)
	{
		if (NULL != m_arrChnlSum[i])
			m_arrChnlSum[i]->Clear();
	}
}
u64 CFlowSumMng::GetAllChnlMediaFlow(u32 nMedia)
{
	// 从上次保存到现在,网卡产生的流量(B)
	u64 nNetCardFlowChg = GetNetCardFlowSum() - m_nNetCardFlowTag;

	// 获取信令流量要特殊处理
	if (nMedia == FST_CMD)
	{
		// 从上次保存到现在,视频产生的流量(B)
		u64 nVedioFlowChg = GetAllChnlMediaFlow(FST_VIDEO);
		// 信令流量(B) = 网卡流量 - 视频流量
		u64 nCmdFlowChg = nNetCardFlowChg - nVedioFlowChg;
		return nCmdFlowChg;
	}
	else // "非信令"流量则执行通用处理
	{
		u64 nCurFlowChg = 0; // 从上次保存到现在,该媒体的流量变化
		for (u32 i = 0; i < m_arrChnlSum.size(); ++i)
		{
			if (NULL != m_arrChnlSum[i])
				nCurFlowChg += m_arrChnlSum[i]->GetFlow(nMedia);
		}

		// 任何一种媒体的流量都不得超过网卡流量,超过了就要纠正
		if (nCurFlowChg > nNetCardFlowChg)
		{
			X_WARN("FlowSum: media flow > netcard flow, now adjust (Media=%u,MediaFlow=%llu,NetFlow=%llu)", nMedia, nCurFlowChg, nNetCardFlowChg);
			nCurFlowChg = nNetCardFlowChg;
		}

		return nCurFlowChg;
	}
}
void CFlowSumMng::FlowLoad()
{
	u32 nTemp = 0;
	u32 nTime = 0;

	// 载入日流量保存日期 (此为本地时间)
	nTime = get_feature_dayflowtime();
	x_utc_to_tms(nTime, nTemp, nTemp, m_nDay, nTemp, nTemp, nTemp);

	// 载入月流量保存日期 (此为本地时间)
	nTime = get_feature_monthflowtime();
	x_utc_to_tms(nTime, nTemp, m_nMonth, nTemp, nTemp, nTemp, nTemp);

	// 视频
	{
		// 载入视频日流量
		m_nVideoFlowDay = get_feature_dayflownum() * 1024;
		// 载入视频月流量
		m_nVideoFlowMonth = get_feature_mouthflownum() * 1024;
	}

	// 信令
	{
		// 载入信令日流量
		m_nCmdFlowDay = get_feature_daybaseflownum() * 1024;
		// 载入信令月流量
		m_nCmdFlowMonth = get_feature_monthbaseflownum() * 1024;
	}

	// 初始化网卡流量标签
	m_nNetCardFlowTag = GetNetCardFlowSum();

	// 提示:载入的日/月可能与当前的日/月不一致(比如终端关闭了好几天才重新点火),但没关系,这里不用处理,之后在"case S_READY"状态中会处理

	X_INFO("FlowSum: Load: (SaveTime=%u,Day=%u,Month=%u,Vidoe[DayBase=%llu(B),MonBase=%llu(B)],Cmd[DayBase=%llu(B),MonBase=%llu(B)],CardFlow=%llu(B))", AppFormsSpace::Apt_GetUtcFrom2000(), m_nDay, m_nMonth, m_nVideoFlowDay, m_nVideoFlowMonth, m_nCmdFlowDay, m_nCmdFlowMonth, m_nNetCardFlowTag);
}
void CFlowSumMng::FlowSave()
{
	// 如果流量未发生变化,则不存储
	if ((GetAllChnlMediaFlow(FST_VIDEO) == 0)
		&& (GetAllChnlMediaFlow(FST_CMD) == 0))
	{
		X_INFO("FlowSum: Save: no need");
		return;
	}

	// 日流量
	FlowSaveDay();

	// 月流量
	FlowSaveMonth();

	// 立即写入flash
	FEATURE_ModifyNotice();

	// 更新流量内存值,此值应该保持与磁盘存储的值一致
	{
		// 各个通道记录的视频流量总和
		u64 nChnlVideoFlow = GetAllChnlMediaFlow(FST_VIDEO);
		// 视频日流量
		m_nVideoFlowDay += nChnlVideoFlow;
		// 视频月流量
		m_nVideoFlowMonth += nChnlVideoFlow;

		// 各个通道记录的信令流量总和
		u64 nChnlCmdFlow = GetAllChnlMediaFlow(FST_CMD);
		// 信令日流量
		m_nCmdFlowDay += nChnlCmdFlow;
		// 信令月流量
		m_nCmdFlowMonth += nChnlCmdFlow;

		// 更新网卡流量标签
		m_nNetCardFlowTag = GetNetCardFlowSum();
	}

	// 使各通道流量开始计算
	AllChnlFlowClear();
}
void CFlowSumMng::FlowSaveDay()
{
	// 存储日期
	u32 nTime = 0;
	{
		nTime = AppFormsSpace::Apt_GetUtcFrom2000();
		nTime = getLocalTime(nTime); // 转本地时间
		set_feature_dayflowtime(nTime);

		u32 nTemp = 0;
		x_utc_to_tms(nTime, nTemp, nTemp, m_nDay, nTemp, nTemp, nTemp);
	}

	// 存储流量值
	{
		// 最新视频日流量
		u32 nVideoFlowDay = GetVideoDayFlow();
		// 最新信令日流量
		u32 nCmdFlowDay = GetCmdDayFlow();

		// 存储
		set_feature_dayflownum(nVideoFlowDay, nCmdFlowDay);
	}

	X_INFO("FlowSum: Save: Day: (SaveTime=%u,Day=%u,VidoeBase=%llu(K),CmdFlow=%llu(K),NetCar=%llu/%llu(B))", nTime, m_nDay, m_nVideoFlowDay / 1024, m_nCmdFlowDay / 1024, m_nNetCardFlowTag, GetNetCardFlowSum());
}
void CFlowSumMng::FlowSaveMonth()
{
	// 存储月份
	u32 nTime = 0;
	{
		nTime = AppFormsSpace::Apt_GetUtcFrom2000();
		nTime = getLocalTime(nTime); // 转本地时间
		set_feature_mouthflowtime(nTime);

		u32 nTemp = 0;
		x_utc_to_tms(nTime, nTemp, m_nMonth, nTemp, nTemp, nTemp, nTemp);
	}

	// 存储流量值
	{
		// 最新视频月流量(KB)
		u32 nVideoFlowMonth = GetVideoMonthFlow();
		// 最新信令月流量(KB)
		u32 nCmdFlowMonth = GetCmdMonthFlow();

		// 存储流量值
		set_feature_mouthflownum(nVideoFlowMonth, nCmdFlowMonth);
	}

	X_INFO("FlowSum: Save: Month: (SaveTime=%u,Month=%u,VidoeFlow=%llu(K),CmdFlow=%llu(K),NetCar=%llu/%llu(B))", nTime, m_nMonth, m_nVideoFlowMonth / 1024, m_nCmdFlowMonth / 1024, m_nNetCardFlowTag, GetNetCardFlowSum());
}

//////////////////////////////////////////////////////////////////////////
u32 RtpStreamType_To_FlowSumType(u32 nRtpStreamType)
{
	switch (nRtpStreamType)
	{
	case RTP_STREAM_TYPE_VIDEO: return FST_VIDEO;
	case RTP_STREAM_TYPE_AUDIO: return FST_AUDIO;
	case RTP_STREAM_TYPE_PIC: return FST_PIC;
	}
	return FST_NULL;
}
#endif // FUNC_FLOW_AND_MEDIA_11
//////////////////////////////////////////////////////////////////////////

std::vector<u32> ChnlMaskToChnlArray(u32 nChnlMask, u32 nMaxBitCount)
{
	std::vector<u32> arrChnl;

	for (u32 i = 0; i < nMaxBitCount; ++i)
	{
		u32 nCurMask = 1 << i;
		if (nChnlMask & nCurMask)
		{
			arrChnl.push_back(i + 1); // 逻辑通道号(1开始)
		}
	}

	return arrChnl;
}


CMmstProcBase::CMmstProcBase(void)
{
	m_nState = 0;
	m_bProcSucc = FALSE;
	m_nErrCode = 0;
	m_bIsDoing = FALSE;
	m_bIsFinish = TRUE;
	m_bAttention = FALSE;

	m_pTmpBuf = NULL;
	m_nTmpBufMaxLen = 0;
	m_nTmpBufCurLen = 0;

	m_pProcExecuter = NULL;

	m_nDelayState = 0;
	m_nDelayStartTick = 0;
	m_nDelayDur = 0;
}
CMmstProcBase::~CMmstProcBase(void)
{
	TmpBuf_Free();
}
BOOL CMmstProcBase::Start(void)
{
	if (IsDoing())
		return TRUE;

	// 绑定流程驱动器
	if (NULL == m_pProcExecuter)
		m_pProcExecuter = &g_cMmstProcExecuter;
	m_pProcExecuter->Add(this);

	m_bIsDoing = TRUE;
	m_bIsFinish = FALSE;
	SetAttention(TRUE);

	SetState(0);
	return TRUE;
}
void CMmstProcBase::Scan(void)
{
	if (m_nState == MMST_S_DELAY)
	{
		if (CObjectA::GetNowTick() - m_nDelayStartTick > m_nDelayDur)
		{
			// 延迟时间到,跳转到指定状态
			SetState(m_nDelayState);
			return;
		}
	}
}
BOOL CMmstProcBase::Stop(void)
{
	SetAttention(FALSE);

	if (!IsDoing())
		return TRUE;

	OnFinish(FALSE, 1);

	return TRUE;
}
BOOL CMmstProcBase::IsDoing(void)
{
	return m_bIsDoing;
}
BOOL CMmstProcBase::IsFinish(void)
{
	return m_bIsFinish;
}
BOOL CMmstProcBase::IsSuccess(void)
{
	return m_bProcSucc;
}
u32 CMmstProcBase::GetErrCode(void)
{
	return m_nErrCode;
}
u32 CMmstProcBase::GetProgress(void)
{
	return 0;
}
BOOL CMmstProcBase::SetState(u32 nState)
{
	m_nState = nState;
	return TRUE;
}
u32 CMmstProcBase::GetState(void)
{
	return m_nState;
}
void CMmstProcBase::SetAttention(BOOL bYes)
{
	m_bAttention = bYes;
}
BOOL CMmstProcBase::IsAttention(void)
{
	return m_bAttention;
}
void CMmstProcBase::CancelAutoScan()
{
	if (m_pProcExecuter == NULL)
		return;
	m_pProcExecuter->Del(this);
}
BOOL CMmstProcBase::OnFinish(BOOL bSucc, u32 nErrCode)
{
	m_bProcSucc = bSucc;
	m_nErrCode = nErrCode;
	m_bIsDoing = FALSE;
	m_bIsFinish = TRUE;

	// 从驱动器中注销,不再执行Scan()函数
	if (NULL != m_pProcExecuter)
		m_pProcExecuter->Del(this);

	return TRUE;
}
void CMmstProcBase::DelayDo(u32 nState, u32 nDuration)
{
	SetState(MMST_S_DELAY);
	m_nDelayState = nState;
	m_nDelayDur = nDuration;
	m_nDelayStartTick = CObjectA::GetNowTick();
}

u8* CMmstProcBase::TmpBuf_Get(void)
{
	return m_pTmpBuf;
}
u32 CMmstProcBase::TmpBuf_GetMaxLen(void)
{
	return m_nTmpBufMaxLen;
}
u32 CMmstProcBase::TmpBuf_GetCurLen(void)
{
	return m_nTmpBufCurLen;
}
BOOL CMmstProcBase::TmpBuf_MakeSaveLen(u32 nNeedLen)
{
	if (nNeedLen == 0)
		return TRUE;
	if (NULL == m_pTmpBuf || m_nTmpBufMaxLen < nNeedLen)
	{
		if (NULL != m_pTmpBuf)
			TmpBuf_Free();
		m_pTmpBuf = new u8[nNeedLen];
		if (NULL == m_pTmpBuf)
		{
			X_ERR("Mmst: new mem err (NeedLen=%u)", nNeedLen);
			return FALSE;
		}
		m_nTmpBufMaxLen = nNeedLen;
		m_nTmpBufCurLen = 0;
	}
	return TRUE;
}
void CMmstProcBase::TmpBuf_Free(void)
{
	if (NULL != m_pTmpBuf)
	{
		delete[] m_pTmpBuf;
		m_pTmpBuf = NULL;
		m_nTmpBufMaxLen = 0;
		m_nTmpBufCurLen = 0;
	}
}

//================================================================================================================
CMmstProcExecuter g_cMmstProcExecuter;
CMmstProcExecuter::CMmstProcExecuter(void)
{
	m_nCurDo = 0;
}
CMmstProcExecuter::~CMmstProcExecuter(void)
{
}
BOOL CMmstProcExecuter::Add(CMmstProcBase* pObject)
{
	if (NULL == pObject)
		return FALSE;

	if (IsExist(pObject))
		return TRUE;

	m_arrProcObj.push_back(pObject);

	return TRUE;
}
BOOL CMmstProcExecuter::Del(CMmstProcBase* pObject)
{
	if (m_arrProcObj.size() == 0)
		return TRUE;

	u32 nDelUnit = 0;
	std::list<CMmstProcBase*>::iterator it = m_arrProcObj.begin();
	while (it != m_arrProcObj.end())
	{
		if (*it == pObject)
		{
			m_arrProcObj.erase(it);

			if (nDelUnit < m_nCurDo)
			{
				// 如果删除m_nCurDo之前的单元,则后面每个元素会向前移一格,所以m_nCurDo也要-1
				--m_nCurDo;
			}

			return TRUE;
		}
		++nDelUnit;
		++it;
	}
	return TRUE;
}
BOOL CMmstProcExecuter::IsExist(CMmstProcBase* pObject)
{
	std::list<CMmstProcBase*>::iterator it = m_arrProcObj.begin();
	while (it != m_arrProcObj.end())
	{
		if (*it == pObject)
			return TRUE;
		++it;
	}

	return FALSE;
}
void CMmstProcExecuter::On10Hz(void)
{
	if (m_arrProcObj.size() == 0)
		return;

	if (m_nCurDo >= m_arrProcObj.size())
	{
		m_nCurDo = 0;
	}

	// 提示: 这里之所以不设计成
	//		std::list<CMmstProcBase*>::iterator m_itCurDo;
	// 而设计成
	//		u32 m_nCurDo;
	// 是因为it->Do()函数中,可能会对列表做删除操作,若恰好把it所指的单元删掉,那么下一句++m_itCurDo会抛异常
	// 而用整型的话,无论怎么加都不会抛异常

	std::list<CMmstProcBase*>::iterator it = m_arrProcObj.begin();
	for (u32 i = 0; i < m_nCurDo; ++i)
		++it;
	(*it)->Scan();

	++m_nCurDo;
}

//============================================================================================
CMmstTimeUp::CMmstTimeUp(void)
{
	m_nDuration = 0;
	m_nStartTick = 0;
}
CMmstTimeUp::~CMmstTimeUp(void)
{
}
BOOL CMmstTimeUp::Start(void)
{
	if (!CMmstProcBase::Start())
		return FALSE;

	m_nStartTick = CObjectA::GetNowTick();
	return TRUE;
}
void CMmstTimeUp::Scan(void)
{
	if (!IsDoing())
		return;

	if (CObjectA::CalcEscapeMsec(m_nStartTick) >= m_nDuration)
	{
		OnFinish(TRUE, 0);
	}
}
void CMmstTimeUp::SetDuration(u32 nDuration)
{
	m_nDuration = nDuration;
}
u32 CMmstTimeUp::GetDuration(void)
{
	return m_nDuration;
}
void CMmstTimeUp::UpdateTime(void)
{
	m_nStartTick = CObjectA::GetNowTick();
}

//============================================================================================
CCmdTimeUp::CCmdTimeUp(void)
{
	m_nCmdID = 0;
}
CCmdTimeUp::~CCmdTimeUp(void)
{
}
BOOL CCmdTimeUp::Start(u32 nCmdID, u32 nTimeLimit)
{
	m_nCmdID = nCmdID;
	SetDuration(nTimeLimit * 1000);
	return CMmstTimeUp::Start();
}
u32 CCmdTimeUp::GetCmdID(void)
{
	return m_nCmdID;
}

//=====================================================================================================================
CNetSender::CNetSender(void)
{
	m_pSender = NULL;

	m_nTotal = 0;
	m_nOnceDo = 0;
	m_nOffset = 0;
}
CNetSender::~CNetSender(void)
{
}
BOOL CNetSender::Send(const u8* pData, u32 nDataLen)
{
	if (NULL == m_pSender)
	{
		X_ERR("NetSender: Send: err, m_pSender=NULL");
		return FALSE;
	}
	if (!m_pSender->IsConnected())
	{
		X_ERR("NetSender: Send: err, is disconn");
		return FALSE;
	}

	// 如果本类还有残留数据未发完,则不允许再次执行发送
	if (IsDoing())
	{
		X_ERR("NetSender: Send: err, proc is stop");
		return FALSE;
	}

	// 首次发送,取最大允许的字节数来发
	u32 nFirstSendLen = YW_MIN(nDataLen, SOCKET_BLOCK_BUFFER_SIZE);

	// 发送
	int nRealSendLen = m_pSender->Send((const char*)pData, nFirstSendLen);

	// 若返回0则表示有严重的系统错误,不必再发
	if (nRealSendLen <= 0)
	{
		X_ERR("NetSender: Send: err, deep err");
		return FALSE;
	}

	// 若首次发送,就将所有数据发完,且成功
	if ((nFirstSendLen == nDataLen) && (nFirstSendLen == (u32)nRealSendLen))
		return TRUE;

	// 若不满足上述条件,就只可能有如下2种情况:
	//		1 首次发送完全成功,但nDataLen较大,需要继续分批发送
	//		2 首次发送未完,尚有残留数据
	// 无论哪种情况,都可以做如下相同的处理

	// 先将数据缓存起来,等待异步发送
	if (!TmpBuf_MakeSaveLen(nDataLen))
		return FALSE;
	m_nTmpBufCurLen = nDataLen - nFirstSendLen;
	memcpy(m_pTmpBuf, pData + nFirstSendLen, m_nTmpBufCurLen);

	// 记录字节信息
	m_nTotal = m_nTmpBufCurLen; // 待发送的总字节数
	m_nOnceDo = SOCKET_BLOCK_BUFFER_SIZE; // 每次发送的最大字节数
	m_nOffset = 0; // 当前发送的偏移字节数

	// 启动定时扫描
	Start();

	// 若还可以发,则立即发送残留数据
	if (!m_pSender->IsBusy())
		Scan();

	return TRUE;
}
void CNetSender::Scan(void)
{
	u32 nOffset = 0;
	u32 nSize = 0;

	if (!IsDoing())
		return;
	if (NULL == m_pSender)
		return;
	if (!m_pSender->IsConnected())
	{
		X_ERR("NetSender: detect disconn when scan");
		OnFinish(FALSE, 1);
		return;
	}

	while (!m_pSender->IsBusy())
	{
		// 获取当前需要发送的偏移地址和发送字节数
		if (!GetOffset(nOffset, nSize))
		{
			// 所有数据已发完
			OnFinish(TRUE, 0);
			return;
		}

		// 发送
		m_pSender->Send((const char*)m_pTmpBuf + nOffset, nSize);
	}
}
BOOL CNetSender::OnFinish(BOOL bSucc, u32 nErrCode)
{
	m_nTotal = 0;
	m_nOffset = 0;
	// m_nOnceDo 不要修改这个值,这是用户设定的,下次流程启动后同样有效
	return CMmstProcBase::OnFinish(bSucc, nErrCode);
}

void CNetSender::SetSender(NetSpace::CIPClient* pSender)
{
	m_pSender = pSender;
}
BOOL CNetSender::IsBusy(void)
{
	return IsDoing();
}
BOOL CNetSender::SetBufSize(u32 nNewSize)
{
	if (nNewSize == 0)
	{
		TmpBuf_Free();
		return TRUE;
	}
	else
	{
		return TmpBuf_MakeSaveLen(nNewSize);
	}
}
u32 CNetSender::GetBufMaxSize(void)
{
	return m_nTmpBufMaxLen;
}
BOOL CNetSender::GetOffset(u32 &nOutOffset, u32 &nOutSize)
{
	nOutOffset = 0;
	nOutSize = 0;

	if (m_nOffset >= m_nTotal)
		return FALSE;

	u64 nRemain = m_nTotal - m_nOffset;

	if (nRemain > m_nOnceDo)
		nOutSize = m_nOnceDo;
	else
		nOutSize = (u32)nRemain;

	nOutOffset = m_nOffset;

	m_nOffset += nOutSize;

	return TRUE;
}

//==================================================================================================================
#ifdef FUNC_EXPORT_TO_MP4
CMp4InfoMaker::CMp4InfoMaker(void)
{
	m_nChnlMask = 0;
	m_nTmBgn = 0;
	m_nTmEnd = 0;
	m_nMedia = 0;

	m_nTakeVideoFrame = 0;
	m_nTakeVideoLen = 0;
	m_nTakeAudioFrame = 0;
	m_nTakeAudioLen = 0;
}
CMp4InfoMaker::~CMp4InfoMaker(void)
{
	FreeMp4Maker();
}
void CMp4InfoMaker::Scan(void)
{
	if (!IsDoing())
		return;

	switch (m_nState)
	{
	case MHM_S_INIT: // 初始化
		{
			m_nTakeVideoFrame = 0;
			m_nTakeVideoLen = 0;
			m_nTakeAudioFrame = 0;
			m_nTakeAudioLen = 0;

			// mp4生成器的一些配置
			if (!CreateMp4Maker())
			{
				OnFinish(FALSE, 1);
				return;
			}

			// 数据提取器的一些配置
			{
				// 创建提取器
				u32 nCovMedia = 0;
				if ((m_nMedia & DXR_MDMASK_VIDEO) && (m_nMedia & DXR_MDMASK_AUDIO))
					nCovMedia = RTP_STREAM_TYPE_MIXED; // 音视频
				else
					nCovMedia = RTP_STREAM_TYPE_VIDEO; // 单视频
				m_pDataTaker.reset(x_new app_history_fetch_control(0, 0, m_nTmBgn, m_nTmEnd, nCovMedia, m_nChnlMask, FETCH_CTRL_TYPE_DOWNLOAD, 1));

				// 设置回调
				m_pDataTaker->set_media_callback(fastdelegate::MakeDelegate(this, &CMp4InfoMaker::OnMediaData));

				// 启动提取器
				int nResult = m_pDataTaker->start(FETCH_CTRL_TYPE_DOWNLOAD);
				if (nResult != MMST_ERR_OK)
				{
					X_INFO("Mp4InfoMaker: media taker start err (result=%u)", nResult);
					OnFinish(FALSE, 1);
					return;
				}

				// 等待I帧 (此句一定要在start后再调用)
				m_pDataTaker->set_wait_I_frame(true);
			}

			SetState(MHM_S_WORKING);
		}
		break;
	case MHM_S_WORKING: // 正在生成mp4头
		{
			// 此状态下,就等待 CMp4InfoMaker::OnMediaData 接收数据并处理
		}
		break;
	}
}
BOOL CMp4InfoMaker::OnFinish(BOOL bSucc, u32 nErrCode)
{
	if (bSucc)
		X_INFO("Mp4InfoMaker: End: Ok");
	else
		X_INFO("Mp4InfoMaker: End: Err");

	// 注意,流程完毕时不要释放MP4生成器,因为外面应用可能要做事后分析.这些生成器会在析构函数中自动释放
	//FreeMp4Maker();

	return CMmstProcBase::OnFinish(bSucc, nErrCode);
}
void CMp4InfoMaker::SetParam(u32 nChnlMask, u32 nTmBgn, u32 nTmEnd, u32 nMedia)
{
	m_nChnlMask = nChnlMask;
	m_nTmBgn = nTmBgn;
	m_nTmEnd = nTmEnd;
	m_nMedia = nMedia;

	X_INFO("Mp4InfoMaker: SetParam: (ChnlMask=%u, TmBgn=%u, TmEnd=%u, Media=%u)", m_nChnlMask, nTmBgn, nTmEnd, nMedia);
}
u8* CMp4InfoMaker::GetMp4Head(u32 nChnl)
{
	CH264AndG711aToMp4Info* pMp4Maker = (CH264AndG711aToMp4Info*)FindMp4Maker(nChnl);
	if (NULL == pMp4Maker)
	{
		X_ERR("MmstJs: mp4 make no find (nChnl=%u)", nChnl);
		return NULL;
	}

	return pMp4Maker->GetMp4Head();
}
u32 CMp4InfoMaker::GetMp4HeadLen(u32 nChnl)
{
	CH264AndG711aToMp4Info* pMp4Maker = (CH264AndG711aToMp4Info*)FindMp4Maker(nChnl);
	if (NULL == pMp4Maker)
		return 0;
	return pMp4Maker->GetMp4HeadLen();
}
u32 CMp4InfoMaker::GetFileSize(u32 nChnl)
{
	CH264AndG711aToMp4Info* pMp4Maker = (CH264AndG711aToMp4Info*)FindMp4Maker(nChnl);
	if (NULL == pMp4Maker)
		return 0;
	return (u32)pMp4Maker->GetFileSize();
}
void CMp4InfoMaker::OnMediaData(t_MediaInfo* pMediaInfo, u32 nSeq, u32 nCrc32)
{
	// 数据提取完毕
	if (NULL == pMediaInfo)
	{
		CloseMp4Maker();
		X_INFO("Mp4InfoMaker: take finish (VFrm=%u,VSize=%u,AFrm=%u,ASiae=%u)", m_nTakeVideoFrame, m_nTakeVideoLen, m_nTakeAudioFrame, m_nTakeAudioLen);

		// 如果一个帧都没有提到,则算是错误
		if ((0 == m_nTakeVideoLen) && (0 == m_nTakeAudioLen))
		{
			OnFinish(FALSE, 1);
		}
		else // 正常结束
		{
			OnFinish(TRUE, 0);
		}

		return;
	}
	else // 收到一帧媒体数据
	{
		// 逻辑通道号 => mp4生成器
		CH264AndG711aToMp4Info* pMp4Maker = NULL;
		{
			u32 nChnl = pMediaInfo->m_chid + 1; // 逻辑通道号(1开始)
			pMp4Maker = (CH264AndG711aToMp4Info*)FindMp4Maker(nChnl);
			if (NULL == pMp4Maker)
				return;
		}

		// 时戳
		u64 nTimeStamp = pMediaInfo->m_timestamp;
		// 媒体类型 (这个算法是参考 vl_stream.set_type(pp_info->m_type, false) 中的算法)
		u32 nMediaType = pMediaInfo->m_type % (RTP_STREAM_TYPE_CNT + 1);

		// 视频
		if (nMediaType == RTP_STREAM_TYPE_VIDEO)
		{
			//X_INFO("Mp4InfoMaker: rcv: video (Len=%u, ts=%llu)", pInfo->m_length, nTimeStamp);
			++m_nTakeVideoFrame;
			m_nTakeVideoLen += pMediaInfo->m_media_length;
			pMp4Maker->WriteVideo((const u8*)pMediaInfo->m_media_data, pMediaInfo->m_media_length, nTimeStamp);
		}
		// 音频
		else if (nMediaType == RTP_STREAM_TYPE_AUDIO)
		{
			//X_INFO("Mp4InfoMaker: rcv: audio (Len=%u, ts=%llu)", pInfo->m_length, nTimeStamp);
			++m_nTakeAudioFrame;
			m_nTakeAudioLen += pMediaInfo->m_media_length;
			pMp4Maker->WriteAudio((const u8*)pMediaInfo->m_media_data, pMediaInfo->m_media_length, nTimeStamp);
		}
	}
}
BOOL CMp4InfoMaker::CreateMp4Maker(void)
{
	// 释放之前的mp4生成器
	FreeMp4Maker();

	// 从掩码中得到逻辑通道号列表
	std::vector<u32> arrChnlNum = ChnlMaskToChnlArray(m_nChnlMask, CH_MAX);
	if (arrChnlNum.size() == 0)
	{
		X_ERR("Mp4InfoMaker: no input chnl");
		return FALSE;
	}

	// 根据通道号,创建mp4生成器
	for (u32 i = 0; i < arrChnlNum.size(); ++i)
	{
		u32 nChnl = arrChnlNum[i]; // 逻辑通道号(1开始)
		CH264AndG711aToMp4Info* pMp4Maker = x_new CH264AndG711aToMp4Info();

		char bufFileName[128] = {0};
		sprintf(bufFileName, "CMp4InfoMaker%u.mp4", i + 1);
		// 虽然是在内存中生成mp4,但文件名还是要填的,底层会根据文件名来找相应的处理对象,相当于关键字的作用
		if (!pMp4Maker->Open(bufFileName))
		{
			X_ERR("Mp4InfoMaker: mp4 open fail (%s)", bufFileName);
			return FALSE;
		}
		m_arrMp4Maker[nChnl] = pMp4Maker;
	}

	return TRUE;
}

void CMp4InfoMaker::FreeMp4Maker(void)
{
	std::map<u32, CH264ToMp4*>::iterator it = m_arrMp4Maker.begin();
	while (it != m_arrMp4Maker.end())
	{
		if (NULL != it->second)
		{
			if (it->second->IsOpened())
				it->second->Close();
			x_delete it->second;
		}
		++it;
	}
	m_arrMp4Maker.clear();
}
void CMp4InfoMaker::CloseMp4Maker(void)
{
	std::map<u32, CH264ToMp4*>::iterator it = m_arrMp4Maker.begin();
	while (it != m_arrMp4Maker.end())
	{
		if (NULL != it->second)
		{
			if (it->second->IsOpened())
				it->second->Close();
		}
		++it;
	}
}
CH264ToMp4* CMp4InfoMaker::FindMp4Maker(u32 nChnl)
{
	std::map<u32, CH264ToMp4*>::iterator it = m_arrMp4Maker.find(nChnl);
	if (it == m_arrMp4Maker.end())
		return NULL;
	return it->second;
}


#endif // FUNC_EXPORT_TO_MP4

#endif // FUNC_USE_TRANSFER_LIB
