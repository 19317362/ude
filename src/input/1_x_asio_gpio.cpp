#include "stdafx.h"
#include "x_asio_gpio.h"
#include "x_lock.h"
#include "x_thread.h"
#include "x_gpio_config_table.h"


// 文件号
#define FILE_NO   FILE_P_X_ASIO_GPIO

#if defined( WIN32) || defined(ON_SERVER)
#define usleep(x)
#endif

//////////////////////////////////////////////////////////////////////////

x_mutex x_asio_gpio::m_lockGpio;

enum ywGPIO_INT_ENABLE_E  //GPIO中断控制枚举
{
	GPIO_INT_DISABLE = 0,
	GPIO_INT_ENABLE = 1,
};

enum ywTIMER_TICK_t
{
	TIMER_5_3US,
	TIMER_10MS,
	TIMER_10MS_REL,
	TIMER_JIFFIES64,
};//获取TICK命令种类

#define MC_SET_ARG(ID,ARG1) (((ID)&0xff)|(((ARG1)&0xff)<<8)) //组合成U16 低8位为ID，高8位为命令

/*
复用模式 现在不支持

启用中断:
1. MC_GPIO_GENERAL_SET   设置为非复用模式
2. MC_GPIO_DIR_SET       设置为输入
3. MC_INT_MODE_SET       设置中断模式
4. MC_INT_EN_SET         使能中断   GPIO_INT_ENABLE
5. MC_START_COM          启用中断netlink


关中断:
1. MC_INT_EN_SET         禁用中断  GPIO_INT_DISABLE
2. MC_TERMINATE_COM      关闭中断  netlink

普通GPIO输入输出用法
1. MC_GPIO_GENERAL_SET   设备非复用模式
2. MC_GPIO_DIR_SET       设置方向
3. MC_GET_GPIO           读GPIO
MC_WRI_GPIO           写GPIO

*/
//以下所有调用ioctl返回 -1 表示出错. 一般的是GPIO_ID错误
//返回 0 表示成功
#if defined(ON_MTK_MT6735_ANDROID)
#define MC_GPIO_GENERAL_SET(ID)      	      IoCtrl(((GPIO_IOCTMODE0)&0XFF),     MC_SET_ARG(ID,0))
//CMD  0:通用GPIO功能	1:GPIO复用功能	2，3:只对GPIO00,GPIO01--GPIO04,GPIO05有效他们有四种功能
#define MC_GPIO_GENERAL1_SET(ID,CMD)          {\
if ((CMD)== GPIO_FUNCTION)\
	IoCtrl(((GPIO_IOCTMODE0)), MC_SET_ARG(ID,0));\
if ((CMD)== GPIO_ALTERNATE_SECOND)\
	IoCtrl(((GPIO_IOCTMODE1)), MC_SET_ARG(ID,0));\
if ((CMD)== GPIO_ALTERNATE_THIRD)\
	IoCtrl(((GPIO_IOCTMODE2)), MC_SET_ARG(ID,0));\
if ((CMD)== GPIO_ALTERNATE_FOURTH)\
	IoCtrl(((GPIO_IOCTMODE3)), MC_SET_ARG(ID,0));\
}\

#define MC_GPIO_DIR_SET(ID,DIR)  		      {\
if ((DIR) == GPIO_DIR_IN)\
	IoCtrl(((GPIO_IOCSDIRIN)),MC_SET_ARG(ID,0));\
else\
	IoCtrl(((GPIO_IOCSDIROUT)),MC_SET_ARG(ID,0));\
	}\

int x_asio_gpio::MC_GET_GPIO(int ID) {
	if ((GpioDirType)IoCtrl(((GPIO_IOCQDIR)), MC_SET_ARG(ID, 0)) == GPIO_DIR_IN)
		return IoCtrl(((GPIO_IOCQDATAIN)), MC_SET_ARG(ID, 0));
	else
		return IoCtrl(((GPIO_IOCQDATAOUT)), MC_SET_ARG(ID, 0));
}

int x_asio_gpio::MC_WRI_GPIO(int ID, GpioLevelType VALUE) {
	if (VALUE == GPIO_LVL_LOW)
		return IoCtrl(((GPIO_IOCSDATALOW)), MC_SET_ARG(ID, 0));
	else
		return IoCtrl(((GPIO_IOCSDATAHIGH)), MC_SET_ARG(ID, 0));
}

#define	MC_INT_MODE_SET(ID,MODE)              
#define MC_INT_EN_SET(ID,VALUE)
#define MC_GTRACE_SET(VALUE)   	
#define MC_GPIO_SET_DEBOUNCE(VALUE)
#else

#define MC_GPIO_GENERAL_SET(ID)      	      IoCtrl(((CMD_GPIO_GEN)&0XFF),     MC_SET_ARG(ID,0))
//CMD  0:通用GPIO功能	1:GPIO复用功能	2，3:只对GPIO00,GPIO01--GPIO04,GPIO05有效他们有四种功能
#define MC_GPIO_GENERAL1_SET(ID,CMD)          IoCtrl(((CMD_GPIO_NEW_GEN)&0XFF),     MC_SET_ARG(ID,(CMD)&(0x03)))//新GPIO复用功能接口
#define MC_GPIO_DIR_SET(ID,DIR)  		      IoCtrl(((CMD_GPIO_DIR)&0xFF),	    MC_SET_ARG(ID,DIR))
#define MC_GET_GPIO(ID)  	    		      IoCtrl(((CMD_GPIO_GET)&0xFF),     MC_SET_ARG(ID,0))
#define MC_WRI_GPIO(ID,VALUE)    		      IoCtrl(((CMD_GPIO_WRI)&0xFF),	    MC_SET_ARG(ID,VALUE))

#define	MC_INT_MODE_SET(ID,MODE) 		IoCtrl(((CMD_GPIO_INT_MOD)&0xFF), MC_SET_ARG(ID,MODE))
#define MC_INT_EN_SET(ID,VALUE)  		IoCtrl(((CMD_GPIO_INT_EN)&0xFF),  MC_SET_ARG(ID,VALUE))
#define MC_GTRACE_SET(VALUE)         	IoCtrl(((CMD_GPIO_TRACE)&0xFF),   MC_SET_ARG(0,VALUE))
#define MC_GPIO_SET_DEBOUNCE(VALUE)      IoCtrl(((CMD_GPIO_SET_DEBOUCE)&0xFF),   VALUE)
#endif

//2015-11-11 T6A增加SD卡CMD和DATA线控制接口,解决sd卡的电源在关闭的时候会有电流倒灌的现象
#define MC_SET_SD_FUNC(FD)               ioctl(FD,((CMD_SET_SD_FUNC)&0XFF),MC_SET_ARG(0,0)) //是将sd卡的cmd管脚和数据管脚设置成相应的信号管脚。在给sd卡上电之前调用
#define MC_SET_SD_TO_GPIO_FUNC(FD)       ioctl(FD,((CMD_SET_SD_TO_GPIO_FUNC)&0XFF),MC_SET_ARG(0,0)) //在准备给sd卡下电之前掉用的。其中fd是打开gpio设备的文件句柄


//DEBOUNCE 指定本GPIO中断是否去抖.   0 不去抖 1 去抖
//TIMER 消息时  DEBOUNCE 不用
//也就是说: 最后一个注册的PROCESS收到通知
#define MC_START_COM(GPIO_ID,DEBOUNCE)    	IoCtrl(((CMD_START_COMM)&0XFF),   MC_SET_ARG(GPIO_ID,DEBOUNCE) )
#define MC_TERMINATE_COM(GPIO_ID)		    IoCtrl(((CMD_TERMINATE_COMM)&0XFF),    MC_SET_ARG(GPIO_ID,0) )
//注意: 如果反注册时, 发现目前注册的是别的PROCESS, 仍将保留之.

//#define CMD_SET_I2C_SPEED(VALUE)         IoCtrl(((CMD_GPIO_TRACE)&0x0F),   MC_SET_ARG(0,VALUE))
#define CMD_SET_GPIO_TRACE(VALUE)         IoCtrl(((CMD_GPIO_TRACE)&0xFF),   MC_SET_ARG(0,VALUE))

#define SW_TIMER_AUTO_FLAG      0x800

#if PF_IsHisiLinux

#define MC_START_SW_TIMER(SW_TIEMR_ID,AUTO,MS,CTX)    	IoCtrl( (((MS)&0xFFFFFL)<<12)| ((AUTO)? SW_TIMER_AUTO_FLAG : 0x0L) |(((SW_TIEMR_ID)&0x07L)<<8) |((CMD_GPIO_START_SW_TIMER)&0XFFL),   (CTX) )
#define MC_STOP_SW_TIMER(SW_TIEMR_ID)    	            IoCtrl( ( ((SW_TIEMR_ID)&0x0FL)<<8) |((CMD_GPIO_STOP_SW_TIMER)&0XFFL),   0 )

#else

#define MC_START_SW_TIMER(SW_TIEMR_ID,AUTO,MS,CTX)    	0
#define MC_STOP_SW_TIMER(SW_TIEMR_ID)    	            0

#endif


//3G模块 TIMER 接口
//Y:
//0 访问的是寄存器的值(5.3us)
//1 访问的jiffies系统变量的值
//2 访问jiffies相对值即减去了默认的初始值300*HZ   JIFFIES 脉冲时间为10MS
#ifdef RETURN_METHOD
#define MC_READ(X,Y)			read((X),NULL,(Y)&0x03)//利用return 方法返回jiffies最后8K数据时出现问题
#else
#define MC_READ(X,Y,Z)			read((X),(Z),(Y)&0x03)//利用copy_to_user方法传递参数
#endif
int x_asio_gpio::m_eventSinkCount = 0;

using namespace DeviceSpace;

x_asio_gpio::x_asio_gpio(const char* pp_DevName, bool vp_EnableEvent, u32 vp_OpenFlag /*= O_RDWR*/, int vp_HwPcbType, int vp_HwVersion)
	: x_asio_iodev(pp_DevName, vp_OpenFlag)
	, m_gpioFid(X_INVALID_IO_FD)
	, m_HwVersion(vp_HwVersion)
	, m_HwPcbType(vp_HwPcbType)
#ifdef WIN32
	, m_bitValue(0)
#endif // WIN32
	, m_bEnableEvent(vp_EnableEvent)

{ // new a object

}

x_asio_gpio::~x_asio_gpio(void)
{
	Close();
}

u32 x_asio_gpio::GetHwVersion()
{
#ifdef WIN32
	return 2;
#else
	return m_HwVersion;
#endif
}

u32 x_asio_gpio::GetHwPcbType()
{
	return m_HwPcbType;
}

bool x_asio_gpio::CreateEventSink()
{
	bool vl_ret = false;

	//	int kpeerlen;
	//	t_PacketInfo* pl_PackInfo;

	//X_ERR("bind ....%d", m_eventSinkCount);
	x_lock vl_lock(m_lockGpio);
	//X_ERR("bind ....%d", m_eventSinkCount);
	if (m_eventSinkCount == 0)
	{
		m_eventSinkCount++;
#ifdef WIN32
		return true;
#else // _DEBUG
		int skfd;
		//X_ERR("bind ....%d", m_eventSinkCount);
		skfd = socket(PF_NETLINK, SOCK_RAW, NL_IMP2);//creat socket: 目前只能用这个. 可能驱动有BUG

		ASSERT(skfd >= 0);
		int on = 1;
		if (setsockopt(skfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
		{
			//printf("setsockopt SO_REUSEADDR - %m");
			X_ERR("setsockopt = %d %s", vl_ret, strerror(errno));
			close(skfd);
		}
		else
		{
			struct sockaddr_nl local;
			x_memset(&local, 0, sizeof(local));//local
			local.nl_family = AF_NETLINK;
			local.nl_pid = getpid_ex();
			local.nl_groups = 0;//
			vl_ret = bind(skfd, (struct sockaddr*)&local, sizeof(local)) == 0;//bind socket
			if (!vl_ret)
			{
				X_ERR("bind = %d %s", vl_ret, strerror(errno));
				ASSERT(vl_ret);
				close(skfd);
			}
			else
			{
				m_fd = skfd;
			}
		}
#endif
	}

	return vl_ret;
}

bool x_asio_gpio::Open()
{ // create a new fd for gpio and a new fd for netlink
	bool vl_ret = true;
	//open GPIO
	x_asio_iodev::Open();
	m_gpioFid = m_fd;// open(m_devName.c_str(), m_oFlag);
	if (m_gpioFid < 0)
	{
		X_ERROR("can't open gpio (%d)", errno);
		vl_ret = false;
	}

	if (vl_ret)
	{
		Init();

		if (m_bEnableEvent)
		{
			vl_ret = CreateEventSink();
		}
	}

	return vl_ret;
}

void x_asio_gpio::Init(void)
{
#if defined(ON_FSL_IMX6) || defined(ON_MTK_MT6735_ANDROID) || defined(ON_ALLWINNER_V3_ANDROID) || defined(IMX6_ANDRIOD_T6A_FIRST_VERSION)
#elif defined(ON_FSL_IMX6_ANDROID)
	u8 al_GpioVersionDec[3] = { GPIOID140, GPIOID139,GPIOID156 };
#elif defined(ON_3520D_V300) && defined(ON_3520D)
	int vl_value;
	vl_value = 0;
    m_HwVersion = 0;
    return;
#else
	u8 al_GpioVersionDec[3] = { GPIOID00, GPIOID01, GPIOID02 };
#endif

	if (m_HwPcbType == 2 || m_HwPcbType == 1 || m_HwPcbType == 3) //LDK_HWPCB_P9201
	{
		//没有版本识别
		X_TRACE("curPcb=%d HwVersion:%d", m_HwPcbType, m_HwVersion);
		//m_HwVersion = g_MpuInfo.v_HwVersion;
	}
	else
	{
		m_HwVersion = 0;
#if defined(ON_FSL_IMX6) || defined(ON_MTK_MT6735_ANDROID) || defined(ON_ALLWINNER_V3_ANDROID)  || defined(IMX6_ANDRIOD_T6A_FIRST_VERSION)
		X_TRACE("ON_FSL_IMX6");
		return;
#else
		int vl_value;
		for (vl_value = 0; vl_value < (int)sizeof(al_GpioVersionDec); vl_value++)
		{
#ifndef WIN32
			MC_GPIO_GENERAL1_SET(al_GpioVersionDec[vl_value], GPIO_FUNCTION);//设置为通用GPIO功能
			MC_GPIO_DIR_SET(al_GpioVersionDec[vl_value], GPIO_DIR_IN);//方向设置为输入
#endif
#if defined(ON_FSL_IMX6_ANDROID)
			usleep(10 * 1000); //解决mos管瞬间电压不稳定问题
#endif
			m_HwVersion |= (GpioGet(al_GpioVersionDec[vl_value]) & 0x01) << vl_value;
#if defined(ON_FSL_IMX6_ANDROID)
			if (m_HwVersion & 0x04)//一代产品
			{
				m_HwVersion &= 0xFC;//做兼容性处理,由于一代产品内核对于GPIOID140, GPIOID139不允许操作,默认是0低电平
			}
#endif
			X_TRACE("Hw_Ver: %d", m_HwVersion);
		}
#endif
	}

#ifdef _GEMINI_FILE_SYSTEM
#ifdef ON_3520
#define LDK_HWPCB_P9103 10
	if (m_HwPcbType == LDK_HWPCB_P9103)
	{
		X_TRACE("F2W set low power consumption");

		//配置降低功耗
		ioctl(m_gpioFid, CMD_CTL_PWR_CONSUMPTION, 0);

		//还原原来的配置
		//ioctl(m_gpioFid, CMD_CTL_PWR_CONSUMPTION, 0x10);
	}
#endif
#endif
}

GpioLevelType x_asio_gpio::GpioGet(u32 gpioNb)//锟斤拷锟斤拷前状态
{
	if (GPIOID_MAX > gpioNb)
	{
		//X_TRACE("gpioNb = %d", gpioNb);
		return (GpioLevelType)MC_GET_GPIO(gpioNb);
	}
	else
	{
		return (GpioLevelType)0;
	}
}

void x_asio_gpio::GpioSet(u32 gpioNb, GpioLevelType vp_LevelType, const char* pp_File, int vp_Line, const char* pp_Func)
{

	if (GPIOID_MAX > gpioNb)
	{
#ifdef WIN32
		if (vp_LevelType == GPIO_LVL_LOW)
		{
			m_bitValue.reset(gpioNb);
		}
		else
		{
			m_bitValue.set(gpioNb);
		}
#else
		// LED灯之类的GPIO不应输出
#if !defined(ON_MTK_MT6735_ANDROID) && !defined(ON_ALLWINNER_V3_ANDROID)
		switch (LDK_getMarkHwPcbType())
		{
		case LDK_HWPCB_P8105: //新架构低功耗p8105 DVR版本(K3-二代)
			if ((gpioCard1LedId_K3 != gpioNb) && (gpioCard2LedId_K3 != gpioNb) && (gpioRLedId_K3 != gpioNb) && (gpioFLedId_K3 != gpioNb))
			{
				X_TRACE("gpioNb=%d level=%d getlevel=%d [%s:%d <%s()>]", gpioNb, vp_LevelType, GpioGet(gpioNb), pp_File, vp_Line, pp_Func);
			}
			break;
		case LDK_HWPCB_P9201:  //硬盘版本(F8)
			if ((gpioCard1LedId_F8 != gpioNb) && (gpioCard2LedId_F8 != gpioNb) && (gpioRLedId_F8 != gpioNb) && (gpioFLedId_F8 != gpioNb))
			{
				X_TRACE("gpioNb=%d level=%d getlevel=%d [%s:%d <%s()>]", gpioNb, vp_LevelType, GpioGet(gpioNb), pp_File, vp_Line, pp_Func);
			}
			break;
		case LDK_HWPCB_P8301: //K5(硬盘一体机)
			if ((gpioCard1LedId_K5 != gpioNb) && (gpioCard2LedId_K5 != gpioNb) && (gpioRLedId_K5 != gpioNb) && (gpioFLedId_K5 != gpioNb))
			{
				X_TRACE("gpioNb=%d level=%d getlevel=%d [%s:%d <%s()>]", gpioNb, vp_LevelType, GpioGet(gpioNb), pp_File, vp_Line, pp_Func);
			}
			break;
		case LDK_HWPCB_P9101: //新一代3G盒子(F4)
			if ((gpioCard1LedId_F4 != gpioNb) && (gpioCard2LedId_F4 != gpioNb) && (gpioRLedId_F4 != gpioNb) && (gpioFLedId_F4 != gpioNb))
			{
				X_TRACE("gpioNb=%d level=%d getlevel=%d [%s:%d <%s()>]", gpioNb, vp_LevelType, GpioGet(gpioNb), pp_File, vp_Line, pp_Func);
			}
			break;
		case LDK_HWPCB_P8106: //(K7-硬盘录像机)
			if ((gpioCard1LedId_K7 != gpioNb) && (gpioCard2LedId_K7 != gpioNb) && (gpioRLedId_K7 != gpioNb) && (gpioFLedId_K7 != gpioNb))
			{
				X_TRACE("gpioNb=%d level=%d getlevel=%d [%s:%d <%s()>]", gpioNb, vp_LevelType, GpioGet(gpioNb), pp_File, vp_Line, pp_Func);
			}
			break;
		//case LDK_HWPCB_P9101_IV: //(F8C-硬盘3G盒子) ///---已停产
		//	if ((gpioCard1LedId_F8C != gpioNb) && (gpioCard2LedId_F8C != gpioNb) && (gpioRLedId_F8C != gpioNb) && (gpioFLedId_F8C != gpioNb))
		//	{
		//		X_TRACE("gpioNb=%d level=%d getlevel=%d [%s:%d <%s()>]", gpioNb, vp_LevelType, GpioGet(gpioNb), pp_File, vp_Line, pp_Func);
		//	}
		//	break;
		case LDK_HWPCB_P9101_IT: //(F8T_存储盒子)
			if ((gpioCard1LedId_F8T != gpioNb) && (gpioCard2LedId_F8T != gpioNb) && (gpioRLedId_F8T != gpioNb) && (gpioFLedId_F8T != gpioNb))
			{
				X_TRACE("gpioNb=%d level=%d getlevel=%d [%s:%d <%s()>]", gpioNb, vp_LevelType, GpioGet(gpioNb), pp_File, vp_Line, pp_Func);
			}
			break;
		case ENUM_DEBUG_K8: //K8(硬盘录像机)
#if defined(ON_3520D_V300) && defined (ON_3520)
			if ((gpioCard1LedId_K8 != gpioNb) && (gpioCard2LedId_K8 != gpioNb) && (gpioRLedId_K8 != gpioNb) && (gpioFLedId_K8 != gpioNb))
			{
				X_TRACE("gpioNb=%d level=%d getlevel=%d [%s:%d <%s()>]", gpioNb, vp_LevelType, GpioGet(gpioNb), pp_File, vp_Line, pp_Func);
			}
#endif
			break;
		case LDK_HWPCB_P1573: //T6A
		case LDK_HWPCB_PHD10: //HD10
		case LDK_HWPCB_PDC1601: //(MTK_6735 -T4)
		case LDK_HWPCB_P9103: //(F2W)
		default:
			X_TRACE("gpioNb=%d level=%d getlevel=%d [%s:%d <%s()>]", gpioNb, vp_LevelType, GpioGet(gpioNb), pp_File, vp_Line, pp_Func);
			break;
		}
#endif
		MC_WRI_GPIO(gpioNb, vp_LevelType);
#endif
	}
	else
	{
		if (gpioNb != GPIOID_MAX)
			X_ERROR("GPIO_NB_MAX[%d] <= %d [%s:%d <%s()>]", GPIOID_MAX, gpioNb, pp_File, vp_Line, pp_Func);
	}
}

void x_asio_gpio::Close()
{
	x_asio_iodev::Close();//close fd
	if (m_bEnableEvent)
	{
		x_lock vl_lock(m_lockGpio);
		m_eventSinkCount--;
	}
	//m_gpioFid = m_fd;
	if (m_gpioFid != X_INVALID_IO_FD)
	{
#ifndef WIN32
		close(m_gpioFid);
#endif
		m_gpioFid = X_INVALID_IO_FD;
	}
}

int x_asio_gpio::Config(u32 gpioNb, const GpioCfgType* pp_CfgType)
{
	if (GPIOID_MAX > gpioNb)
	{
		MC_GPIO_GENERAL1_SET(gpioNb, (u32)pp_CfgType->function);
		MC_GPIO_DIR_SET(gpioNb, (u32)pp_CfgType->direction);
		return 0;
	}
	else
	{
		if (gpioNb != GPIOID_MAX)
			X_ERROR("GPIO_NB_MAX[%d] <= %d", GPIOID_MAX, gpioNb);
		return -1;
	}
}

int x_asio_gpio::EnableEINT(u32 gpioNb, u32 vp_DebounceMs, GpioTriggerType vp_TriggerType)
{
	if (GPIO_NB_MAX > gpioNb)
	{
		MC_INT_MODE_SET(gpioNb, (u32)vp_TriggerType);
		MC_INT_EN_SET(gpioNb, GPIO_INT_ENABLE);
		if (vp_DebounceMs > 0)
		{
			MC_GPIO_SET_DEBOUNCE(3000);
			MC_START_COM(gpioNb, 1);
		}
		else
		{
			MC_START_COM(gpioNb, 0);
		}

		return 0;
	}
	else
	{
		return -1;
	}
}

int x_asio_gpio::DisableEINT(u32 gpioNb)
{
	if (GPIO_NB_MAX > gpioNb)
	{
		MC_INT_EN_SET(gpioNb, GPIO_INT_DISABLE);
		MC_TERMINATE_COM(gpioNb);
		return 0;
	}
	else
	{
		return -1;
	}
}

void x_asio_gpio::SetDebounceValue(u32 vp_ValueInMs)
{
	MC_GPIO_SET_DEBOUNCE(vp_ValueInMs);
}

int x_asio_gpio::StartGPIOTimer(X_GPIO_TIMER_ID vp_Id, bool vp_AutoReload, u32 vp_IntervalInMS, u32 vp_Ctx)
{
	int vp_TimerID = (int)vp_Id;

	if (vp_TimerID == 0 || vp_TimerID == 1)
	{
		vp_TimerID += TIMERID0;

		printf("CMD_START_COMM:%d,%d", CMD_START_COMM, vp_TimerID);
		ioctl(m_gpioFid, ((CMD_START_COMM) & 0XFF), (((vp_TimerID) & 0xffff) | (((1) & 0xffff) << 16)));

		return 0;
	}
	else if ((vp_TimerID - 2) < HI_SOFT_TIMER_CNT)
	{
		vp_TimerID -= 2;

		return MC_START_SW_TIMER(vp_TimerID, vp_AutoReload, vp_IntervalInMS, vp_Ctx);
	}
	else
	{
		return -1;

	}
}

int x_asio_gpio::StopGPIOTimer(X_GPIO_TIMER_ID vp_Id)
{
	int vp_TimerID = (int)vp_Id;
	if (vp_TimerID == 0 || vp_TimerID == 1)
	{
		vp_TimerID += TIMERID0;
		ioctl(m_gpioFid, ((CMD_TERMINATE_COMM) & 0XFF), (((vp_TimerID) & 0xffff) | (((1) & 0xffff) << 16)));

		return 0;
	}
	else if (vp_TimerID >= 2 && (vp_TimerID - 2) < HI_SOFT_TIMER_CNT)
	{
		vp_TimerID -= 2;
		return MC_STOP_SW_TIMER(vp_TimerID);
	}
	else
	{
		return -1;
	}
}

int x_asio_gpio::Read(void* pp_Data, int vp_Size)
{
	return MC_READ(m_gpioFid, vp_Size, pp_Data);
}

int x_asio_gpio::ReadNLPacket(t_PacketInfo* pp_packet)
{
	return recv(GetFd(), (char*)pp_packet, sizeof(t_PacketInfo), 0);
}

int x_asio_gpio::IoCtrl(unsigned int vp_Code, u32 pp_Data)
{
	int vl_ret;
	if (m_gpioFid < 0)
	{
		X_ERROR("m_gpioFid = %d", m_gpioFid);
		return -1;
	}

	if ((pp_Data & 0xFF) == GPIOID_MAX)
	{
		return -1;
	}

#ifdef ON_ALLWINNER_V3_ANDROID
	//这里针对IPC-V3对LED灯控制进行适配
	vl_ret = 0;
	if (vp_Code == CMD_GPIO_WRI)
	{
		int led_value = 1 - (((pp_Data & 0xFF00) >> 8) & 0x01);
		int led_pin = pp_Data & 0xFF;
		switch (led_pin)
		{
		case gpioCard1LedId_V3:
		case gpioCard2LedId_V3:
		case gpioFLedId_V3:
			vl_ret = ioctl(m_gpioFid, 4, &led_value); //设置V3红色LED开关。	
			break;
		case gpioRLedId_V3:
			vl_ret = ioctl(m_gpioFid, 3, &led_value); //设置V3绿色LED开关。	
			break;
		}
	}
#else
	vl_ret = ioctl(m_gpioFid, vp_Code, (void *)pp_Data);
#endif	
	//X_TRACE("gpio fd %d  ret:%d  code:0x%x  data:0x%x", (int)m_gpioFid, vl_ret, vp_Code, (int)pp_Data);
	if (vl_ret == -1)
	{
		X_TRACE("GPIO IoCtl ErrCode: %s", strerror(errno));
		X_TRACE("gpio fd %d  ret:%d  code:0x%x  data:0x%x", (int)m_gpioFid, vl_ret, vp_Code, pp_Data);
	}

	return vl_ret;
}

void x_asio_gpio::SetSdFunc(void)
{
	MC_SET_SD_FUNC(GetFd());
}

void x_asio_gpio::SetSdToGpioFunc(void)
{
	MC_SET_SD_TO_GPIO_FUNC(GetFd());
}
