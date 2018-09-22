#include "stdafx.h"
#include "x_asio_gpio.h"
#include "x_lock.h"
#include "x_thread.h"
#include "x_gpio_config_table.h"


// �ļ���
#define FILE_NO   FILE_P_X_ASIO_GPIO

#if defined( WIN32) || defined(ON_SERVER)
#define usleep(x)
#endif

//////////////////////////////////////////////////////////////////////////

x_mutex x_asio_gpio::m_lockGpio;

enum ywGPIO_INT_ENABLE_E  //GPIO�жϿ���ö��
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
};//��ȡTICK��������

#define MC_SET_ARG(ID,ARG1) (((ID)&0xff)|(((ARG1)&0xff)<<8)) //��ϳ�U16 ��8λΪID����8λΪ����

/*
����ģʽ ���ڲ�֧��

�����ж�:
1. MC_GPIO_GENERAL_SET   ����Ϊ�Ǹ���ģʽ
2. MC_GPIO_DIR_SET       ����Ϊ����
3. MC_INT_MODE_SET       �����ж�ģʽ
4. MC_INT_EN_SET         ʹ���ж�   GPIO_INT_ENABLE
5. MC_START_COM          �����ж�netlink


���ж�:
1. MC_INT_EN_SET         �����ж�  GPIO_INT_DISABLE
2. MC_TERMINATE_COM      �ر��ж�  netlink

��ͨGPIO��������÷�
1. MC_GPIO_GENERAL_SET   �豸�Ǹ���ģʽ
2. MC_GPIO_DIR_SET       ���÷���
3. MC_GET_GPIO           ��GPIO
MC_WRI_GPIO           дGPIO

*/
//�������е���ioctl���� -1 ��ʾ����. һ�����GPIO_ID����
//���� 0 ��ʾ�ɹ�
#if defined(ON_MTK_MT6735_ANDROID)
#define MC_GPIO_GENERAL_SET(ID)      	      IoCtrl(((GPIO_IOCTMODE0)&0XFF),     MC_SET_ARG(ID,0))
//CMD  0:ͨ��GPIO����	1:GPIO���ù���	2��3:ֻ��GPIO00,GPIO01--GPIO04,GPIO05��Ч���������ֹ���
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
//CMD  0:ͨ��GPIO����	1:GPIO���ù���	2��3:ֻ��GPIO00,GPIO01--GPIO04,GPIO05��Ч���������ֹ���
#define MC_GPIO_GENERAL1_SET(ID,CMD)          IoCtrl(((CMD_GPIO_NEW_GEN)&0XFF),     MC_SET_ARG(ID,(CMD)&(0x03)))//��GPIO���ù��ܽӿ�
#define MC_GPIO_DIR_SET(ID,DIR)  		      IoCtrl(((CMD_GPIO_DIR)&0xFF),	    MC_SET_ARG(ID,DIR))
#define MC_GET_GPIO(ID)  	    		      IoCtrl(((CMD_GPIO_GET)&0xFF),     MC_SET_ARG(ID,0))
#define MC_WRI_GPIO(ID,VALUE)    		      IoCtrl(((CMD_GPIO_WRI)&0xFF),	    MC_SET_ARG(ID,VALUE))

#define	MC_INT_MODE_SET(ID,MODE) 		IoCtrl(((CMD_GPIO_INT_MOD)&0xFF), MC_SET_ARG(ID,MODE))
#define MC_INT_EN_SET(ID,VALUE)  		IoCtrl(((CMD_GPIO_INT_EN)&0xFF),  MC_SET_ARG(ID,VALUE))
#define MC_GTRACE_SET(VALUE)         	IoCtrl(((CMD_GPIO_TRACE)&0xFF),   MC_SET_ARG(0,VALUE))
#define MC_GPIO_SET_DEBOUNCE(VALUE)      IoCtrl(((CMD_GPIO_SET_DEBOUCE)&0xFF),   VALUE)
#endif

//2015-11-11 T6A����SD��CMD��DATA�߿��ƽӿ�,���sd���ĵ�Դ�ڹرյ�ʱ����е������������
#define MC_SET_SD_FUNC(FD)               ioctl(FD,((CMD_SET_SD_FUNC)&0XFF),MC_SET_ARG(0,0)) //�ǽ�sd����cmd�ܽź����ݹܽ����ó���Ӧ���źŹܽš��ڸ�sd���ϵ�֮ǰ����
#define MC_SET_SD_TO_GPIO_FUNC(FD)       ioctl(FD,((CMD_SET_SD_TO_GPIO_FUNC)&0XFF),MC_SET_ARG(0,0)) //��׼����sd���µ�֮ǰ���õġ�����fd�Ǵ�gpio�豸���ļ����


//DEBOUNCE ָ����GPIO�ж��Ƿ�ȥ��.   0 ��ȥ�� 1 ȥ��
//TIMER ��Ϣʱ  DEBOUNCE ����
//Ҳ����˵: ���һ��ע���PROCESS�յ�֪ͨ
#define MC_START_COM(GPIO_ID,DEBOUNCE)    	IoCtrl(((CMD_START_COMM)&0XFF),   MC_SET_ARG(GPIO_ID,DEBOUNCE) )
#define MC_TERMINATE_COM(GPIO_ID)		    IoCtrl(((CMD_TERMINATE_COMM)&0XFF),    MC_SET_ARG(GPIO_ID,0) )
//ע��: �����ע��ʱ, ����Ŀǰע����Ǳ��PROCESS, �Խ�����֮.

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


//3Gģ�� TIMER �ӿ�
//Y:
//0 ���ʵ��ǼĴ�����ֵ(5.3us)
//1 ���ʵ�jiffiesϵͳ������ֵ
//2 ����jiffies���ֵ����ȥ��Ĭ�ϵĳ�ʼֵ300*HZ   JIFFIES ����ʱ��Ϊ10MS
#ifdef RETURN_METHOD
#define MC_READ(X,Y)			read((X),NULL,(Y)&0x03)//����return ��������jiffies���8K����ʱ��������
#else
#define MC_READ(X,Y,Z)			read((X),(Z),(Y)&0x03)//����copy_to_user�������ݲ���
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
		skfd = socket(PF_NETLINK, SOCK_RAW, NL_IMP2);//creat socket: Ŀǰֻ�������. ����������BUG

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
		//û�а汾ʶ��
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
			MC_GPIO_GENERAL1_SET(al_GpioVersionDec[vl_value], GPIO_FUNCTION);//����Ϊͨ��GPIO����
			MC_GPIO_DIR_SET(al_GpioVersionDec[vl_value], GPIO_DIR_IN);//��������Ϊ����
#endif
#if defined(ON_FSL_IMX6_ANDROID)
			usleep(10 * 1000); //���mos��˲���ѹ���ȶ�����
#endif
			m_HwVersion |= (GpioGet(al_GpioVersionDec[vl_value]) & 0x01) << vl_value;
#if defined(ON_FSL_IMX6_ANDROID)
			if (m_HwVersion & 0x04)//һ����Ʒ
			{
				m_HwVersion &= 0xFC;//�������Դ���,����һ����Ʒ�ں˶���GPIOID140, GPIOID139���������,Ĭ����0�͵�ƽ
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

		//���ý��͹���
		ioctl(m_gpioFid, CMD_CTL_PWR_CONSUMPTION, 0);

		//��ԭԭ��������
		//ioctl(m_gpioFid, CMD_CTL_PWR_CONSUMPTION, 0x10);
	}
#endif
#endif
}

GpioLevelType x_asio_gpio::GpioGet(u32 gpioNb)//����ǰ״̬
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
		// LED��֮���GPIO��Ӧ���
#if !defined(ON_MTK_MT6735_ANDROID) && !defined(ON_ALLWINNER_V3_ANDROID)
		switch (LDK_getMarkHwPcbType())
		{
		case LDK_HWPCB_P8105: //�¼ܹ��͹���p8105 DVR�汾(K3-����)
			if ((gpioCard1LedId_K3 != gpioNb) && (gpioCard2LedId_K3 != gpioNb) && (gpioRLedId_K3 != gpioNb) && (gpioFLedId_K3 != gpioNb))
			{
				X_TRACE("gpioNb=%d level=%d getlevel=%d [%s:%d <%s()>]", gpioNb, vp_LevelType, GpioGet(gpioNb), pp_File, vp_Line, pp_Func);
			}
			break;
		case LDK_HWPCB_P9201:  //Ӳ�̰汾(F8)
			if ((gpioCard1LedId_F8 != gpioNb) && (gpioCard2LedId_F8 != gpioNb) && (gpioRLedId_F8 != gpioNb) && (gpioFLedId_F8 != gpioNb))
			{
				X_TRACE("gpioNb=%d level=%d getlevel=%d [%s:%d <%s()>]", gpioNb, vp_LevelType, GpioGet(gpioNb), pp_File, vp_Line, pp_Func);
			}
			break;
		case LDK_HWPCB_P8301: //K5(Ӳ��һ���)
			if ((gpioCard1LedId_K5 != gpioNb) && (gpioCard2LedId_K5 != gpioNb) && (gpioRLedId_K5 != gpioNb) && (gpioFLedId_K5 != gpioNb))
			{
				X_TRACE("gpioNb=%d level=%d getlevel=%d [%s:%d <%s()>]", gpioNb, vp_LevelType, GpioGet(gpioNb), pp_File, vp_Line, pp_Func);
			}
			break;
		case LDK_HWPCB_P9101: //��һ��3G����(F4)
			if ((gpioCard1LedId_F4 != gpioNb) && (gpioCard2LedId_F4 != gpioNb) && (gpioRLedId_F4 != gpioNb) && (gpioFLedId_F4 != gpioNb))
			{
				X_TRACE("gpioNb=%d level=%d getlevel=%d [%s:%d <%s()>]", gpioNb, vp_LevelType, GpioGet(gpioNb), pp_File, vp_Line, pp_Func);
			}
			break;
		case LDK_HWPCB_P8106: //(K7-Ӳ��¼���)
			if ((gpioCard1LedId_K7 != gpioNb) && (gpioCard2LedId_K7 != gpioNb) && (gpioRLedId_K7 != gpioNb) && (gpioFLedId_K7 != gpioNb))
			{
				X_TRACE("gpioNb=%d level=%d getlevel=%d [%s:%d <%s()>]", gpioNb, vp_LevelType, GpioGet(gpioNb), pp_File, vp_Line, pp_Func);
			}
			break;
		//case LDK_HWPCB_P9101_IV: //(F8C-Ӳ��3G����) ///---��ͣ��
		//	if ((gpioCard1LedId_F8C != gpioNb) && (gpioCard2LedId_F8C != gpioNb) && (gpioRLedId_F8C != gpioNb) && (gpioFLedId_F8C != gpioNb))
		//	{
		//		X_TRACE("gpioNb=%d level=%d getlevel=%d [%s:%d <%s()>]", gpioNb, vp_LevelType, GpioGet(gpioNb), pp_File, vp_Line, pp_Func);
		//	}
		//	break;
		case LDK_HWPCB_P9101_IT: //(F8T_�洢����)
			if ((gpioCard1LedId_F8T != gpioNb) && (gpioCard2LedId_F8T != gpioNb) && (gpioRLedId_F8T != gpioNb) && (gpioFLedId_F8T != gpioNb))
			{
				X_TRACE("gpioNb=%d level=%d getlevel=%d [%s:%d <%s()>]", gpioNb, vp_LevelType, GpioGet(gpioNb), pp_File, vp_Line, pp_Func);
			}
			break;
		case ENUM_DEBUG_K8: //K8(Ӳ��¼���)
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
	//�������IPC-V3��LED�ƿ��ƽ�������
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
			vl_ret = ioctl(m_gpioFid, 4, &led_value); //����V3��ɫLED���ء�	
			break;
		case gpioRLedId_V3:
			vl_ret = ioctl(m_gpioFid, 3, &led_value); //����V3��ɫLED���ء�	
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
