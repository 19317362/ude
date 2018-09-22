#include "stdafx.h"

#include "x_poll_thread.h"

#define EPOLLWAIT 10


// 文件号
#define FILE_NO   FILE_P_X_POLL_THREAD



x_poll_thread::x_poll_thread(bool vp_CreateSuspended, const char* pp_ThreadName, bool vp_MQopen, u32 vp_thStepMask, X_THREAD_PRIORITY vp_prio, const char* pp_mqName)
: x_thread(vp_CreateSuspended, pp_ThreadName, vp_prio)
, m_epoll(this)
, m_mq(pp_mqName)
, m_epollWait(~0)
, m_OpenMQ(vp_MQopen)
, m_ThreadStepMask(vp_thStepMask)
{
	if (m_OpenMQ)
	{
		X_INFO("thread %s create and open mq", this->m_thName.c_str());
		m_mq.Open();
	}

	m_epollWait = EPOLLWAIT;
	m_periodic_callback = NULL;
	memset(m_commonRxBuffer, 0, sizeof(m_commonRxBuffer));
	m_bRun = false;
}

x_poll_thread::~x_poll_thread(void)
{
#ifndef WIN32
	for (t_timers::iterator it = m_timers.begin(); it != m_timers.end(); ++it)
	{
		if (!(m_free_timers.find(it->first)
			== m_free_timers.end()))
		{
			m_epoll.del(it->second);
		}

		x_delete(it->second);
		it->second = NULL;
	}
	m_timers.clear();
#endif
}

bool x_poll_thread::proc_message(XMSG* pp_msg, bool& vp_Known)
{
	bool vl_Consumed = false;
	bool vl_FreeNeeded = false;

	vp_Known = false;

	if (pp_msg->v_MsgClass == X_MC_INTERNAL)
	{
		pp_msg = ( alias_cast<X_MSG_INTERNAL*>( m_commonRxBuffer ) )->p_RealMSG;

		vl_FreeNeeded = true;
	}

	if (pp_msg->v_MsgClass == X_MC_SYSTEM)
	{
		vp_Known = true;
		t_msgDelegateMap::iterator it = m_sysMsgMap.find(pp_msg->v_MsgID);
		if (it != m_sysMsgMap.end())
		{
			if (pp_msg->v_MsgID == XMSG_SYS_QUIT)
			{
				vl_Consumed = true;
				m_bRun = false;
			}
			else
			{
				vl_Consumed = it->second(pp_msg);
			}
		}

		if (!vl_Consumed)
		{
			OnSysMessage(pp_msg);
		}
	}
	else if (pp_msg->v_MsgClass == X_MC_USER)
	{ 
	    if(pp_msg->v_MsgFlag == 0x6B) //����ywupdate��Ϣ����,��ǰywupdate��Ϣ���ʹ������
	    {
			t_msgDelegateMap::iterator it = m_usrMsgMap.find(pp_msg->v_MsgID);		
			vp_Known = true;
	        XMSG vl_msg;
			vl_msg.v_MsgClass = pp_msg->v_MsgClass;
			vl_msg.v_MsgFlag = pp_msg->v_MsgFlag;
			vl_msg.v_DataLen = pp_msg->v_DataLen;
			vl_msg.v_MsgID = pp_msg->v_MsgID;
			x_memcpy(pp_msg, (void*)&vl_msg, sizeof(XMSG));			
			if (it != m_usrMsgMap.end())
			{
				vl_Consumed = it->second(pp_msg);
			}
			
			if (!vl_Consumed)
			{
				OnUserMessage(pp_msg);
			}
	    }
		else
		{	   
		    t_msgDelegateMap::iterator it = m_usrMsgMap.find(pp_msg->v_MsgID);
			
			vp_Known = true;
			
			if (it != m_usrMsgMap.end())
			{
				vl_Consumed = it->second(pp_msg);
			}
			
			if (!vl_Consumed)
			{
				OnUserMessage(pp_msg);
			}
		}

	}
	else
	{
		vp_Known = false;
	}

	return vl_FreeNeeded;
}

extern "C"
{
	void MDK_DumpMemory(const void* pp_Data, size_t vp_Size, const char* pp_Desc);
}

int x_poll_thread::on_mq_evnent(x_asio_base* pp_base, const void *pp_ctx)
{
	int vl_rxSize;
	XMSG* pl_msg = NULL;
	bool vl_FreeNeeded = false;
	bool vp_Known = true;

#ifdef FUNC_EPOLL_EVENTTIME_CHECK
	struct timeval bgntv;
#endif

	x_epoll_event_time_begin(bgntv);

	vl_rxSize = m_mq.Read(m_commonRxBuffer, COMMON_RX_BUFFER_SIZE);

	if (vl_rxSize > 0)
	{
		if ((u32)vl_rxSize >= sizeof( XMSG ))
		{
			pl_msg = (XMSG*)m_commonRxBuffer;
		}
		else
		{
			OnUnknownMessage(m_commonRxBuffer, vl_rxSize);

			return 0;
		}

#ifdef DEBUG_PLATEFORM_COMMAND
		char szMsg[128] = { 0 };

		x_snprintf(szMsg, sizeof(szMsg), "msgid=0x%04x, thread=%s\n", pl_msg->v_MsgID, this->m_thName.c_str());

		PLATEFORM_COMMANDLOG(0x10000000, 2000, szMsg, false);
#endif

		vl_FreeNeeded = proc_message(pl_msg, vp_Known);
		if (!vp_Known)
		{
			OnUnknownMessage(m_commonRxBuffer, vl_rxSize);
		}

		if (vl_FreeNeeded)
		{
			( alias_cast<X_MSG_INTERNAL*>( m_commonRxBuffer ) )->Release();
		}
	}
	else
	{
		x_epoll_event_valid_check(vl_rxSize, 0);
	}

	x_epoll_event_time_end(bgntv);

#ifdef FUNC_EPOLL_EVENTTIME_CHECK
	struct timeval tmptv;

	gettimeofday(&tmptv, 0);

	int vl_tmpdiff = (tmptv.tv_sec * 1000 + tmptv.tv_usec / 1000) - (bgntv.tv_sec * 1000 + bgntv.tv_usec / 1000);
	if (vl_tmpdiff > (EVENTTIME_THRESHOLD))
	{
		X_TRACE("MsgID=0x%04x, threadname=%s", pl_msg->v_MsgID, this->m_thName.c_str());
	}
#endif

	return 0;
}

int x_poll_thread::on_timer_evnent(x_asio_base* pp_base, const void *pp_ctx)
{
#ifndef WIN32
#ifdef DEBUG_PLATEFORM_COMMAND
	char szMsg[128] = { 0 };
	x_snprintf(szMsg, sizeof(szMsg), "on_timer_evnent=%d thread=%s", pp_base->GetFd(), this->m_thName.c_str());
	PLATEFORM_COMMANDLOG(0x10000020, 2000, szMsg, false);
#endif

	if (!((x_asio_timer *)pp_base)->m_bRepeat)
	{
		m_free_timers.insert(pp_base->GetFd());
		m_epoll.del((m_timers[pp_base->GetFd()]));
	}

	u64 exp;

	((x_asio_timer *)pp_base)->Read(&exp, sizeof(exp));

	OnTimer(pp_base->GetFd(), pp_ctx);
#endif

	return 0;
}

void x_poll_thread::mask_signal(void)
{
#ifdef WIN32
#else
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

void x_poll_thread::ThreadProc()
{
	m_bRun = true;
	int vl_ret = 0;
	int vl_Pid = (int)getpid_ex();

	X_WARN("Threadname: %s, ThreadId: %d, Pid: %u started", GetThreadName(), GetThreadId(), vl_Pid);

	mask_signal();

	if (m_prio != XTP_PRIORITY_NORMAL)
	{
		SetPriority(m_prio);
	}

	// 由于win32 未完整模拟linux文件，此处加入epoll会引起无法接收事件问题
	if (m_OpenMQ)
	{
		m_epoll.add(&m_mq, NULL, MakeDelegate(this, &x_poll_thread::on_mq_evnent), NULL);
	}

	if (OnThreadInit())
	{
		while (m_bRun)
		{
#ifdef WIN32
			MSG msg;
			x_sys_msleep(1);

			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				switch (msg.message)
				{
				case WM_TIMER:
					{
						t_timerList::iterator it(m_timerList.find(msg.wParam));

						if (it != m_timerList.end())
						{
							x_timer vl_tm = it->second;
							//on timer 中可能会start/stop timer, 会引起IT不可用.!!
							OnTimer(msg.wParam, vl_tm.v_Context);
							if (!vl_tm.v_Repeat)
							{
								StopTimer(msg.wParam);
							}
						}
					}
					break;
				}

				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
#endif
			PLATEFORM_STEP(m_ThreadStepMask, GetThreadId());

			PrePoll();

			if (NULL != m_periodic_callback)	// 处理周期性任务
			{
				m_periodic_callback();
			}

#ifdef WIN32
			// 由于win32 未完整模拟linux文件，此处加入epoll会引起无法接收事件问题
			if (m_OpenMQ)
			{
				while (m_mq.getMqMsgNum() > 0)
				{
					on_mq_evnent(&m_mq, NULL);
				}
			}
			vl_ret = m_epoll.wait(1);
#else
			vl_ret = m_epoll.wait(m_epollWait);
#endif

			PostPoll();
		    PLATEFORM_STEP(m_ThreadStepMask, (GetThreadId()|0x81000000));
		}
	}

	X_WARN("Thread %s %d exited", GetThreadName(), GetThreadId());

	OnThreadExit();
}

int x_poll_thread::PostThMsg(const XMSG* pp_msg)
{
	int vl_ret = -1;

	if (m_OpenMQ)
	{
		if ( sizeof(XMSG) + pp_msg->v_DataLen > 240 )
		{
			X_MSG_INTERNAL vl_msg(pp_msg);

			vl_ret = m_mq.Write(&vl_msg, sizeof(X_MSG_INTERNAL) );
		}
		else
		{
		#ifdef FUNC_POST_MSG_FAIL_RETRY
			u32 vl_cnt = 0;		// 延时次数
			while ((0 != vl_ret) && (vl_cnt++ < 5))
			{
				vl_ret = m_mq.Write(pp_msg, sizeof(XMSG)+pp_msg->v_DataLen);

				if (0 != vl_ret)
				{
					if (EAGAIN != errno)
					{
						X_WARN("MsgClass:%d MsgId:%d v_DataLen:%d threadname:%s MqMsgNum=%d vl_ret:%d errno:%s(%d)",
							pp_msg->v_MsgClass, pp_msg->v_MsgID, pp_msg->v_DataLen, this->m_thName.c_str(), m_mq.getMqMsgNum(), vl_ret, strerror(errno), errno);
						break;
					}
					else
					{
						//X_WARN("MsgClass:%d MsgId:%d v_DataLen:%d threadname:%s MqMsgNum=%d vl_ret:%d errno:%s(%d) cnt:%u(%u)",
						//	pp_msg->v_MsgClass, pp_msg->v_MsgID, pp_msg->v_DataLen, this->m_thName.c_str(), m_mq.getMqMsgNum(), vl_ret, strerror(errno), errno, vl_cnt, 50);
						x_sys_msleep(50);		// 因资源暂不可用导致消息发送失败则延时重试
					}
				}
			}
		#else
			vl_ret = m_mq.Write(pp_msg, sizeof(XMSG) + pp_msg->v_DataLen);

			if (0 != vl_ret)
			{
				X_WARN("MsgClass:%d MsgId:%d v_DataLen:%d threadname:%s MqMsgNum=%d vl_ret:%d errno:%s",
					pp_msg->v_MsgClass, pp_msg->v_MsgID, pp_msg->v_DataLen, this->m_thName.c_str(), m_mq.getMqMsgNum(), vl_ret, strerror(errno));
			}
		#endif
		}
	}
	else
	{
		X_WARN("mq not open");
	}

	return vl_ret;
}

void x_poll_thread::OnUserMessage(XMSG* pp_msg)
{
	X_TRACE("unknown user message:%08x", pp_msg->v_MsgID);
}

void x_poll_thread::OnSysMessage(XMSG* pp_msg)
{
	switch (pp_msg->v_MsgID)
	{
	case XMSG_SYS_QUIT:
		{
			X_WARN("Thread %s %d QUIT received", GetThreadName(), GetThreadId());

			m_bRun = false;
		}
		break;
	default:
		{
			X_TRACE("unknown sys message:%08x", pp_msg->v_MsgID);
		}
		break;
	}
}

int x_poll_thread::Quit()
{
	X_INFO("Thread %s %d QUIT received", GetThreadName(), GetThreadId());
	m_bRun = false;

	return 0;
}

int x_poll_thread::PostQuitMsg()
{
	XMSG vl_msg;

	vl_msg.v_MsgClass = X_MC_SYSTEM;
	vl_msg.v_MsgID = XMSG_SYS_QUIT;

	//X_INFO("Post QUIT to %s", GetThreadName());

	return PostThMsg(&vl_msg);
}

void x_poll_thread::PrePoll()
{
}

void x_poll_thread::PostPoll()
{

}

bool x_poll_thread::OnThreadInit()
{
	return true;
}

void x_poll_thread::OnThreadExit()
{
}

u32 x_poll_thread::StartTimer( x_asio_context vp_Context,u16 vp_ElapseInMS, bool vp_Repeat /*= true */, bool vp_delay /*false*/)
{
	t_x_timer_id vl_TimerId;

	if (m_timerList.empty())
	{
		//first time
		OnTimerInit();
	}

	x_timer vl_tm;
	vl_tm.v_Repeat = vp_Repeat;
	vl_tm.v_Context = vp_Context;

#ifdef WIN32
	vl_TimerId = ::SetTimer(NULL,0,vp_ElapseInMS,NULL);
#else
	x_asio_timer *pl_timer;

	if (m_free_timers.empty())
	{
		pl_timer = x_new x_asio_timer();

		if (pl_timer->Open())
		{
			m_timers[pl_timer->GetFd()] = pl_timer;

			vl_TimerId = pl_timer->GetFd();
		}
		else
		{
			vl_TimerId = (u32)(-1);
		}
	}
	else
	{
		pl_timer = (x_asio_timer *)(m_timers[*(m_free_timers.begin())]);
		vl_TimerId = pl_timer->GetFd();

		m_free_timers.erase(m_free_timers.begin());
	}

	m_epoll.add(pl_timer
		, vp_Context
		, MakeDelegate(this,&x_poll_thread::on_timer_evnent)
		, NULL);

	pl_timer->Start(vp_ElapseInMS, vp_Repeat, vp_delay);
#endif

	m_timerList[vl_TimerId] = vl_tm;

	return vl_TimerId;
}

bool x_poll_thread::IsTimerRepeat(u32 vp_TimerId)
{
	bool vl_ret = false;

	t_timerList::iterator it = m_timerList.find(vp_TimerId);
	if (it != m_timerList.end())
	{
		vl_ret = it->second.v_Repeat != 0;
	}

	return vl_ret;
}

void x_poll_thread::StopTimer( u32 vp_TimerId )
{
    if (!vp_TimerId)
    {
        return;
    }

	t_timerList::iterator it = m_timerList.find(vp_TimerId);
	if (it != m_timerList.end())
	{
#ifdef WIN32
		::KillTimer(NULL,vp_TimerId);
#else
#ifdef X_POSIX_TIMER_FTR
		timer_delete(vp_TimerId);
#else
		m_epoll.del((m_timers[vp_TimerId]));

		((x_asio_timer *)(m_timers[vp_TimerId]))->Stop();

		m_free_timers.insert(vp_TimerId);
#endif
#endif

		m_timerList.erase(it);
	}

	if (m_timerList.empty())
	{
		OnTimerRelease();
	}
}

void x_poll_thread::OnTimer( u32 vp_Id, x_asio_context vp_Context )
{
}

void x_poll_thread::OnTimerInit()
{
#ifdef X_POSIX_TIMER_FTR
#else
	m_epollWait = EPOLLWAIT;//1 tick
#endif
}

void x_poll_thread::OnTimerRelease()
{
	m_epollWait = ~0;
}

int x_poll_thread::SetPeriodicCb(Periodic_CallBack vp_CallBack)
{
	X_TRACE_FL();

	if (NULL != vp_CallBack)
	{
		m_periodic_callback = vp_CallBack;
	}

	X_TRACE_FL();

	return 0;
}

void x_poll_thread::AddMsgHandler(u32 vp_Msg, x_msg_Delegate vp_handler)
{
	m_usrMsgMap[vp_Msg] = vp_handler;
}

void x_poll_thread::DeleteMsgHandler(u32 vp_Msg)
{
	m_usrMsgMap.erase(vp_Msg);
}

void x_poll_thread::OnUnknownMessage(const u8* pl_msg, int vl_rxSize)
{
	X_ERR("unknown message size:%d", vl_rxSize);
}

