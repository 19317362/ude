#include "stdafx.h"

#ifndef WIN32
	#include <linux/ppp_defs.h>
	#include "if_ppp.h"

	#include <asm/unistd.h>
	#include <sys/reboot.h>
	#include <sys/types.h>
#endif

#include "ldk_i.h"
#include "app_thread.h"
#include "dnr_thread.h"
#include "matask_thread.h"
#ifdef _WITH_SCRIPT_APP_MODULE
#include "sa_thread.h"
#endif
#include "ldk_i_svc.h"
#include "ldk_ctx.h"
#include "dev_name.h"
#include "dvrr_g711.h"
#include "../x_dvrr/dvrr_entry.h" //entry to dvrr
#include "../x_dvrr/adec_thread.h"
#include "../x_dvrr/x_dvrr_version_def.h"
#include "drv/hi_ssp_inter.h"
#include "drv/ak4633_inter.h"

#include "../x_dvrr/dvrr_svc_cmd.h"
#include "ldk_transfer.h"
#include "../x_dvrr/rtp_interface.h"
#ifndef FUNC_USE_TRANSFER_LIB
#include "../x_dvrr/wfcvm/wfs/dummy_mmst.h"
#else
#include "../transfer/dummy_mmst.h"
#endif // FUNC_USE_TRANSFER_LIB

#ifndef WIN32
	#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

// 文件号
#define FILE_NO   FILE_M_API_LIB

//using namespace NetSpace;

bool vg_IsUseH330GsmModule;
bool vg_IsUseH330AcmName;


//////////////////////////////////////////////////////////////////////////
t_MdkCtx g_ctx;
////
#ifdef FUNC_UART_TEST		// 测试串口接收字节数
	// 串口发送字节数
unsigned int Uart_WriteBytes[UART_MAX_COUNT + 1];
// 串口接收字节数
unsigned int Uart_ReadBytes[UART_MAX_COUNT + 1];
#endif


// t_MDKUpdRegionInfo vl_norinfo;
#define API_LIB_TIMEOUT 2000

#define MC_AUDIO_SEM_CONSUME
#define MC_AUDIO_SEM_PRODUCE

u8 ag_I2DevStatus[EAT_MAX_IDEVICE_ID];
static t_ModemType st_ModemType = LDK_MT_UNKNOWN;

#define ONCOMMANDEXDATALEN 1200
static u8 ag_OnCommandExData[ONCOMMANDEXDATALEN];
//#define MC_CORE_PRINTF	printf

//------------------------------------------

//-----------------------------------------
static t_SVCFunc c_SVCTbl[SVC_ID_MAX];
void * ldk_GetDvrrSvcHandler()
{
	return (void*)&c_SVCTbl[0];
}

void MDK_DbgDumpMemory(void* pp_Data, size_t vp_Size, const char* pp_Desc)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000078, API_LIB_TIMEOUT, NULL, false);

	x_dump_memory(pp_Data, vp_Size, pp_Desc);
}

void MDK_DumpMemory(const void* pp_Data, size_t vp_Size, const char* pp_Desc)
{
	size_t i;
	if (g_ctx.v_TraceMask == 0)
	{
		return;
	}
	LDK_CORE_WRITEMONITORLOG(0xa0000079, API_LIB_TIMEOUT, NULL, false);

	// 修改说明:
	//		1.之前的代码只用printf输出,而不是X_PRINT,这种方式不能在monitor中输出,对于客户那里无法拆机接控制台的场景,就很难抓数据
	//		2.优化了性能,不再每个字节输出一次,而是16个字节(一行)输出一次
	std::string strLine = "";
	char strWord[4] = { 0 };
	X_PRINT("%s @%p Len %d:", pp_Desc, pp_Data, vp_Size);
	u8* pl_Data = (u8*)pp_Data;
	for (i = 0; i < vp_Size; ++i)
	{
		x_snprintf(strWord, sizeof(strWord), "%02X ", pl_Data[i]);
		strLine.append(strWord);
		if ((i & 0xF) == 0xF)
		{
			X_PRINT(strLine.c_str());
			strLine = "";
		}
	}
	if (strLine != "")
		X_PRINT(strLine.c_str());
}

//////////////////////////////////////////////////////////////////////////
//UART
#ifdef PLATFORM_UART_NEWDRIVER_ENABLE
	#define COM_FILE "/dev/ttyYW%d"
#else
	#if defined(ON_FSL_IMX6) || defined(ON_FSL_IMX6_ANDROID)
		#define COM_FILE "/dev/ttymxc%d"
	#elif defined(ON_MTK_MT6735_ANDROID)
		#define COM_FILE "/dev/ttyMT%d" // T4: MT0-控制台串口  MT1~MT2-外接拉出自定义使用 MT3-与epu通讯
	#elif defined(ON_ALLWINNER_V3_ANDROID)
		#define COM_FILE "/dev/ttyS%d"
	#else
		#define COM_FILE "/dev/ttyAMA%d"
	#endif
#endif

int ldk_recheck_ttyUsb()
{
	//检查ttyUSBx 在不在
	LDK_CORE_WRITEMONITORLOG(0xa0000001, API_LIB_TIMEOUT, NULL, false);
	int i;
	int vl_Cnt = g_ctx.v_UsbInfo.v_Flag & 0x0F;
	char al_DevName[64] = { 0 };

	for (i = 0; (i < vl_Cnt) && (vl_Cnt < MAX_TTY_NB); ++i)
	{
		x_snprintf(al_DevName, sizeof(al_DevName), EXT_COM_FILE, g_ctx.v_UsbInfo.a_ttyName[i]);
		if (access(al_DevName, F_OK) != 0)
		{
			break;
		}
		else
		{
		}
	}

	if (i != vl_Cnt)
	{
		g_ctx.v_UsbInfo.v_Flag = 0;
	}
	else
	{
	}
	return 0;
}
int MDK_DetectUsbSerial(u32* pp_idVendor, u32* pp_idProduct, u32* pp_bcdVersion)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000002, API_LIB_TIMEOUT, NULL, false);

	if (g_ctx.v_UsbInfo.v_Flag != 0)
	{
		ldk_recheck_ttyUsb();
	}

	if (g_ctx.v_UsbInfo.v_Flag == 0)
	{
		return -1;
	}
	else
	{
		*pp_idVendor = g_ctx.v_UsbInfo.a_prodId0;
		*pp_idProduct = g_ctx.v_UsbInfo.a_prodId1;
		*pp_bcdVersion = 0;

		return 0;
	}
}

int ldk_InitDetectUsbSerial()
{
	//LDK_CORE_WRITEMONITORLOG(0xa0000003, API_LIB_TIMEOUT, NULL, false);
#define FILE_USB_SERIAL "/proc/tty/driver/usbserial"
#define FILE_GHT_USB_SERIAL "/sys/bus/usb/drivers/cdc_acm/1-1:1.0/modalias"
#define FILE_GHT_USB_MINI_SERIAL "/sys/bus/usb/drivers/cdc_acm/1-2:1.0/modalias"
#define FILE_GHT_MAX_TYPE 2
	const char *al_ght_index[FILE_GHT_MAX_TYPE] = { FILE_GHT_USB_SERIAL, FILE_GHT_USB_MINI_SERIAL };
	int i;

	// * (04/10/2002) gkh
	// *	added serial_read_proc function which creates a
	// *	/proc/tty/driver/usb-serial file.
	// /proc/tty/driver # cat usbserial
	// usbserinfo:1.0 driver:v2.0
	// 0: module:option name:"Option 3G data card" vendor:12d1 product:1404 num_ports:1 port:1 path:usb-hisilicon-1
	// 1: module:option name:"Option 3G data card" vendor:12d1 product:1404 num_ports:1 port:1 path:usb-hisilicon-1
	// 2: module:option name:"Option 3G data card" vendor:12d1 product:1404 num_ports:1 port:1 path:usb-hisilicon-1
	// 3: module:option name:"Option 3G data card" vendor:12d1 product:1404 num_ports:1 port:1 path:usb-hisilicon-1
	// 4: module:option name:"Option 3G data card" vendor:12d1 product:1404 num_ports:1 port:1 path:usb-hisilicon-1
	// 5: module:option name:"Option 3G data card" vendor:12d1 product:1404 num_ports:1 port:1 path:usb-hisilicon-1
	int vl_ret = -1;
	X_TRACE("+++++++++++++USB+++++++++++");
	if (access(FILE_USB_SERIAL, F_OK) == 0)
	{
		char buf[512] = { 0 };
		char* pl_Pos;
		int len;
		int idVendor;
		int idProduct;
#if PF_IsAndroid
		int fd = open(FILE_USB_SERIAL, O_RDONLY, S_IRUSR);
#else
		int fd = open(FILE_USB_SERIAL, O_RDONLY, S_IREAD);
#endif
		if (fd > 0)
		{
			memset(buf, 0, sizeof(buf));
			len = read(fd, buf, sizeof(buf));
			if (len > 0)
			{
				pl_Pos = strstr(buf, "vendor:");
				if (NULL != pl_Pos)
				{
					vl_ret = sscanf(pl_Pos, "vendor:%04x product:%04x", &idVendor, &idProduct);
					CORE_TRACE(("%d: %04X %04X:", vl_ret, idVendor, idProduct));
					if (vl_ret == 2)
					{
						g_ctx.v_UsbInfo.a_prodId0 = idVendor;
						g_ctx.v_UsbInfo.a_prodId1 = idProduct;
						//
						for (idVendor = 0, idProduct = 0; idVendor < MAX_TTY_NB; idVendor++)
						{
							x_snprintf(buf, sizeof(buf), "/dev/ttyUSB%d", idVendor);
							if (access(buf, F_OK) == 0)
							{
								g_ctx.v_UsbInfo.a_ttyName[idProduct++] = idVendor;
							}
						}
						g_ctx.v_UsbInfo.v_Flag = idProduct;//记下数量

						//MDK_On_UsbSerialAdded(g_ctx.v_UsbInfo.a_prodId0,g_ctx.v_UsbInfo.a_prodId1,0);
						vl_ret = 0;

					}
					else
					{
						ASSERT(vl_ret == 2);
					}
				}
			}
			close(fd);
		}
	}

	for (i = 0; i < FILE_GHT_MAX_TYPE; i++)
	{
		if (access(al_ght_index[i], F_OK) == 0)
		{
			vg_IsUseH330GsmModule = true;
			X_INFO("--------GHT 3G MODEM TYPE:%d \n", i);
			break;
		}
	}

	if (i < FILE_GHT_MAX_TYPE)
	{
		char buf[512] = { 0 };
		char* pl_Pos;
		int len;
		int idVendor;
		int idProduct;
#if PF_IsAndroid
		int fd = open(al_ght_index[i], O_RDONLY, S_IRUSR);
#else
		int fd = open(al_ght_index[i], O_RDONLY, S_IREAD);
#endif
		if (fd > 0)
		{
			memset(buf, 0, sizeof(buf));
			len = read(fd, buf, sizeof(buf)-1);
			if (len > 0)
			{
				pl_Pos = strstr(buf, "v");
				if (NULL != pl_Pos)
				{
					vl_ret = sscanf(pl_Pos, "v%04xp%04x", &idVendor, &idProduct);
					CORE_TRACE(("%d: %04X %04X", vl_ret, idVendor, idProduct));
					if (vl_ret == 2)
					{
						g_ctx.v_UsbInfo.a_prodId0 = idVendor;
						g_ctx.v_UsbInfo.a_prodId1 = idProduct;
						//
						for (idVendor = 0, idProduct = 0; idVendor < MAX_TTY_NB; idVendor++)
						{
							x_snprintf(buf, sizeof(buf), EXT_COM_FILE, idVendor);

							if (access(buf, F_OK) == 0)
							{
								g_ctx.v_UsbInfo.a_ttyName[idProduct++] = idVendor;
							}
						}
						g_ctx.v_UsbInfo.v_Flag = idProduct;//记下数量

						//MDK_On_UsbSerialAdded(g_ctx.v_UsbInfo.a_prodId0,g_ctx.v_UsbInfo.a_prodId1,0);
						vl_ret = 0;
					}
					else
					{
						ASSERT(vl_ret == 2);
					}
				}
			}
			close(fd);
		}
	}

	return vl_ret;
}

// LINUX_REBOOT_CMD_CAD_OFF
// (RB_DISABLE_CAD, 0). CAD is disabled. This means that the CAD keystroke will cause a SIGINT signal to be sent to init (process 1), whereupon this process may decide upon a proper action (maybe: kill all processes, sync, reboot).
// LINUX_REBOOT_CMD_CAD_ON
// (RB_ENABLE_CAD, 0x89abcdef). CAD is enabled. This means that the CAD keystroke will immediately cause the action associated with LINUX_REBOOT_CMD_RESTART.
// LINUX_REBOOT_CMD_HALT
// (RB_HALT_SYSTEM, 0xcdef0123; since 1.1.76). The message "System halted." is printed, and the system is halted. Control is given to the ROM monitor, if there is one. If not preceded by a sync(2), data will be lost.
// LINUX_REBOOT_CMD_KEXEC (since Linux 2.6.13)
// Execute a kernel that has been loaded earlier with kexec_load(2). This option is only available if the kernel was configured with CONFIG_KEXEC.
// LINUX_REBOOT_CMD_POWER_OFF
// (0x4321fedc; since 2.1.30). The message "Power down." is printed, the system is stopped, and all power is removed from the system, if possible. If not preceded by a sync(2), data will be lost.
// LINUX_REBOOT_CMD_RESTART
// (RB_AUTOBOOT, 0x1234567). The message "Restarting system." is printed, and a default restart is performed immediately. If not preceded by a sync(2), data will be lost.
// LINUX_REBOOT_CMD_RESTART2
// (0xa1b2c3d4; since 2.1.30). The message "Restarting system with command aq%saq" is printed, and a restart (using the command string given in arg) is performed immediately. If not preceded by a sync(2), data will be lost.
void MDK_SwitchOff()
{
	LDK_CORE_WRITEMONITORLOG(0xa0000004, API_LIB_TIMEOUT, NULL, false);
	X_TRACE("MDK_SwitchOff: -----------------  POWER OFF ---------------");
#ifdef WIN32
	printf("\n -----------------  POWER OFF --------------- \n");
#else
	sync();
#ifdef EP_INTERVAL_DEBUG_FTR
	g_ep_abort_flag = 100;
	printf("\n -----------------  POWER OFF --------------- \n");
#else
	//reboot(RB_HALT_SYSTEM);//does not work
	//reboot(RB_POWER_OFF);//does not work

#ifdef ON_MTK_MT6735_ANDROID
	x_system("reboot");
#elif ON_ALLWINNER_V3_ANDROID
	x_system("reboot");
#else
	x_system("poweroff");
#endif

#endif
#endif
}

void MDK_Reboot(void)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000005, API_LIB_TIMEOUT, NULL, false);
	printf("\n-----------------MDK  REBOOT ---------------\n");
#ifdef WIN32
	printf("\n -----------------  REBOOT --------------- \n");
	X_ASSERT(0);
#else
#ifdef EP_INTERVAL_DEBUG_FTR
	g_ep_abort_flag = 100;
	printf("\n -----------------  REBOOT --------------- \n");
#else
	x_sys_reboot("MDK_Reboot");
#endif
#endif
}

#if defined(ON_3520D_V300) && defined (ON_3520)
#define V300_COM_ID1    0//ttyYW0
#define V300_COM_ID2    1//ttyYW1
#define V300_COM_ID3    3
#endif

u8 mdk_core_trans_id(u8 vp_UartID)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000006, API_LIB_TIMEOUT, NULL, false);
	//CORE_TRACE(("mdk_core_trans_id %d\n",vp_UartID));
	if (vp_UartID < MAX_UART_COUNT)
	{

#if defined(ON_3520)
#ifdef ON_3520D_V300
		if (vp_UartID == 1)
		{
			vp_UartID = V300_COM_ID1;
		}
		if (vp_UartID == 2)
		{
			vp_UartID = V300_COM_ID2;
		}
#endif

		//if (LDK_GetHwTypeInEEP() != LDK_HW_DVR && (MC_CORE_GET_HW_VER()>2))
		//{
		//if (vp_UartID == 1)
		//  {
		//    vp_UartID = 1;
		//  }
		//  else if (vp_UartID == 2)
		//  {
		//    vp_UartID = 2;
		//  }
		//}
#elif  defined(ON_FSL_IMX6) || defined(ON_MTK_MT6735_ANDROID) || defined(ON_ALLWINNER_V3_ANDROID) || defined(IMX6_ANDRIOD_T6A_FIRST_VERSION)

		//if (vp_UartID == 1)
		//{
		//   vp_UartID = 2;
		//}
		//else if (vp_UartID == 2)
		//{
		//   vp_UartID = 1;
		//}
#elif defined(ON_FSL_IMX6_ANDROID)
		if (MC_CORE_GET_HW_VER() < 4) //二代机器
		{
			if (vp_UartID == 3)
			{
				vp_UartID = 1;
			}
			else if (vp_UartID == 1)
			{
				vp_UartID = VIRTULUART8; //monitor虚拟串口
			}
		}
#else
		if (LDK_GetHwTypeInEEP() != LDK_HW_DVR && (MC_CORE_GET_HW_VER() > 2))
		{
			if (vp_UartID == 1)
			{
				vp_UartID = 2;
			}
			else if (vp_UartID == 2)
			{
				vp_UartID = 1;
			}
		}
		//CORE_TRACE(("Conver Result: UARTID:%d", vp_UartID));
#endif

		return vp_UartID;
	}
	else if (vp_UartID < EXT_UART0)
	{
		return (u8)INVALID_UART_ID;
	}
	else if (vp_UartID <= (EXT_UART0 + MAX_EXT_UART_COUNT))
	{
		return vp_UartID - EXT_UART0 + MAX_UART_COUNT;
	}
	else
	{
		return (u8)INVALID_UART_ID;
	}
}

/*
  外设控制接口
  参数:
  u8 vp_DeviceId,  1-touch 2-背光 3-lcd屏
  u8 vp_bOpt  0-关闭 1-打开
  返回值:
  0-成功
  1-设备id不存在
  2-控制失败
*/
int LDK_ExtDeviceCtrl(u8 vp_DeviceId, u8 vp_bOpt)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000006, API_LIB_TIMEOUT, NULL, false);

	int vl_ret = 0;
	if (vp_DeviceId > 3 || vp_bOpt > 2)
	{
		vl_ret = 1;
		return vl_ret;
	}

	switch (vp_DeviceId)
	{
	case 1:
	case 2:
	{
		if (vp_bOpt == 0)
		{
#if defined(ON_FSL_IMX6_ANDROID)
			vl_ret = x_system("lcdoff");
#endif
		}
		else
		{
#if defined(ON_FSL_IMX6_ANDROID)
			vl_ret = x_system("lcdon");
#endif
		}
	}
	break;
	case 3:
	{
		if (vp_bOpt == 0)
		{
#if defined(ON_FSL_IMX6_ANDROID)
			vl_ret = x_system("tftpwroff");
#endif
		}
		else
		{
		}
	}
	break;
	default:
		vl_ret = 1;
		break;
	}

	return vl_ret;
}

u8 mdk_core_transtompu_id(u8 vp_UartID)
{
	if (vp_UartID < MAX_UART_COUNT)
	{

#if defined(ON_3520)
#elif defined(ON_FSL_IMX6) || defined(ON_MTK_MT6735_ANDROID) || defined(ON_ALLWINNER_V3_ANDROID) || defined(IMX6_ANDRIOD_T6A_FIRST_VERSION)
#elif defined(ON_FSL_IMX6_ANDROID)
		if (MC_CORE_GET_HW_VER() < 4) //二代机器
		{
			if (vp_UartID == 1)
			{
				vp_UartID = 3;
			}
			else if (vp_UartID == VIRTULUART8)
			{
				vp_UartID = 1; //monitor虚拟串口
			}
		}
#else
		if (LDK_GetHwTypeInEEP() != LDK_HW_DVR && (MC_CORE_GET_HW_VER() > 2))
		{
			if (vp_UartID == 1)
			{
				vp_UartID = 2;
			}
			else if (vp_UartID == 2)
			{
				vp_UartID = 1;
			}
		}
#endif

		return vp_UartID;
	}
	else if ((MAX_UART_COUNT <= vp_UartID) && (vp_UartID < EXT_UART0))
	{
		return vp_UartID + EXT_UART0 - MAX_UART_COUNT;
	}
	else
	{
		return (u8)INVALID_UART_ID;
	}
}

#define MC_UART_ID_CHECK_RET()	if ( (vp_UartID = mdk_core_trans_id(vp_UartID))== (u8)INVALID_UART_ID ) return -1

int ldk_is_ttyReady()
{
	return g_ctx.v_UsbInfo.v_Flag != 0;
}

MDK_Result MDK_ConfigUart(u8 vp_UartID, const t_MDK_UartConfig* pp_Cfg)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000008, API_LIB_TIMEOUT, NULL, false);
	MDK_Result vl_ret = -1;
	if (!ldk_is_ttyReady() && vp_UartID >= EXT_UART0)
	{
		return -1;
	}

	MC_UART_ID_CHECK_RET();

	x_asio_uart* pl_uart = NULL;
	if (MC_GET_APP_TH())
	{
		pl_uart = MC_GET_APP_TH()->GetUartViaID(vp_UartID);
	}
	if (NULL != pl_uart)
	{
		vl_ret = pl_uart->Config(pp_Cfg);
	}
	return vl_ret;
}


MDK_Result MDK_GetUartConfig(u8 vp_UartID, t_MDK_UartConfig* pp_Cfg)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000009, API_LIB_TIMEOUT, NULL, false);
	MDK_Result vl_ret = -1;
	x_asio_uart* pl_uart = NULL;

	if (vp_UartID == VIRTULUART9 || vp_UartID == VIRTULUART10 || VIRTULUART12 == vp_UartID)
	{
		return 0;
	}

	MC_UART_ID_CHECK_RET();
	if (MC_GET_APP_TH())
	{
		pl_uart = MC_GET_APP_TH()->GetUartViaID(vp_UartID);
	}
	if (NULL != pl_uart)
	{
		pl_uart->GetCurrentConfig(pp_Cfg);
		vl_ret = 0;
	}

	return vl_ret;
}
int ldk_close_all_tty_usb()
{
	LDK_CORE_WRITEMONITORLOG(0xa000000a, API_LIB_TIMEOUT, NULL, false);
	printf("%s %u\n", __func__, g_ctx.v_UsbInfo.v_Flag);
	u8 vl_Flag = g_ctx.v_UsbInfo.v_Flag;
	ldk_recheck_ttyUsb();
	if (vl_Flag != 0)
	{
		MDK_Net_UnInit(0);
		if (MC_GET_APP_TH())
		{
			MC_GET_APP_TH()->CloseAllTty();
		}

		g_ctx.v_UsbInfo.v_Flag = 0;
	}
	return 0;
}

int MDK_UartExists(t_duart_UartID vp_UartID)
{
	LDK_CORE_WRITEMONITORLOG(0xa000000b, API_LIB_TIMEOUT, NULL, false);
	char al_Buffer[32];

	if (g_ctx.v_UsbInfo.v_Flag != 0)
	{
		ldk_recheck_ttyUsb();
	}
	if (g_ctx.v_UsbInfo.v_Flag == 0)
	{
		return -1;
	}
	MC_UART_ID_CHECK_RET();

	if (vp_UartID < MAX_UART_COUNT)
	{
		x_snprintf(al_Buffer, sizeof(al_Buffer), COM_FILE, vp_UartID);
	}
	else
	{
		x_snprintf(al_Buffer, sizeof(al_Buffer), EXT_COM_FILE, g_ctx.v_UsbInfo.a_ttyName[vp_UartID - MAX_UART_COUNT]);
	}
	//	CORE_TRACE(("checking %s\n",al_Buffer));
#ifdef WIN32
	return _access(al_Buffer, F_OK);
#else
	return access(al_Buffer, F_OK);
#endif
}

MDK_Result MDK_SetUartBreakMode(t_duart_BreakMod vp_Mode)
{
	LDK_CORE_WRITEMONITORLOG(0xa000000c, API_LIB_TIMEOUT, NULL, false);

	return 0;
}

//  UART 0x10 ev 5
//  UART read 0x1 = 0
// 	UART 0x10 ev 1d
// 	UART read 0x10 = 0
//
// 	/dev/ttyUSB2  /dev/ttyUSB4  /dev/ttyUSB6
// 	/dev/ttyUSB3  /dev/ttyUSB5  /dev/ttyUSB7
//
MDK_Result MDK_OpenUart(t_duart_UartID vp_UartID, const t_MDK_UartConfig* pp_Cfg)
{
	LDK_CORE_WRITEMONITORLOG(0xa000000d, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return -1;
	}
	MDK_Result vl_ret = -1;
	int vl_AppId = vp_UartID;
	char al_Buffer[32];

	if (!ldk_is_ttyReady() && vp_UartID >= EXT_UART0)
	{
		return -1;
	}

	if (NULL == pp_Cfg)
	{
		return -1;
	}

	X_TRACE("MDK_OpenUart: %d Rate:%d %d %d %d", vp_UartID, pp_Cfg->v_Baudrate, pp_Cfg->v_DataBits, pp_Cfg->v_StopBits, pp_Cfg->v_Parity);
	if (vp_UartID == VIRTULUART9 || vp_UartID == VIRTULUART10 || VIRTULUART12 == vp_UartID)
	{
	    if(VIRTULUART12 == vp_UartID)
			MC_GET_APP_TH()->SetNetVirturalUartClient(true);
		MC_GET_APP_TH()->OpenNetVirturalUart(vp_UartID);
		return 0;
	}

	MC_UART_ID_CHECK_RET();
	X_TRACE("MDK_OpenUart: %d Rate:%d %d %d %d", vp_UartID, pp_Cfg->v_Baudrate, pp_Cfg->v_DataBits, pp_Cfg->v_StopBits, pp_Cfg->v_Parity);

	if (pp_Cfg->v_Baudrate < MAX_UART_BAUD_RATE
		&& (pp_Cfg->v_DataBits == DataBits8 || pp_Cfg->v_DataBits == DataBits7)
		&& (pp_Cfg->v_Parity == Odd || pp_Cfg->v_Parity == None || pp_Cfg->v_Parity == Even)
		)
	{
		bool vl_DmaMode = false;
		if (vp_UartID < MAX_UART_COUNT)
		{
			if (vp_UartID < VIRTULUART5 || vp_UartID > VIRTULUART8)
			{
				x_snprintf(al_Buffer, sizeof(al_Buffer), COM_FILE, vp_UartID);
			}
			else
			{
				if (vp_UartID == VIRTULUART8)
				{
					x_snprintf(al_Buffer, sizeof(al_Buffer), "%s", VERTUAL_PORT8_MASTER);
				}
				if (vp_UartID == VIRTULUART7)
				{
					x_snprintf(al_Buffer, sizeof(al_Buffer), "%s", VERTUAL_PORT7_MASTER);
				}
				if (vp_UartID == VIRTULUART6)
				{
					x_snprintf(al_Buffer, sizeof(al_Buffer), "%s", VERTUAL_PORT6_MASTER);
				}
				if (vp_UartID == VIRTULUART5)
				{
					x_snprintf(al_Buffer, sizeof(al_Buffer), "%s", VERTUAL_PORT5_MASTER);
				}
			}
		}
		else
		{
			x_snprintf(al_Buffer, sizeof(al_Buffer), EXT_COM_FILE, g_ctx.v_UsbInfo.a_ttyName[vp_UartID - MAX_UART_COUNT]);
		}

		X_TRACE("uart dev name: %s, convert uart id: %d, old uart id: %d", al_Buffer, vp_UartID, vl_AppId);
		X_TRACE("DMA_MODE: %d", vl_DmaMode);
		vl_ret = MC_GET_APP_TH()->OpenUart(al_Buffer, vp_UartID, pp_Cfg, vl_AppId, vl_DmaMode);
	}
	else
	{
		CORE_TRACE(("open: INVALID PARAM"));
		X_TRACE("MDK_OpenUart: %d", vp_UartID);
		X_TRACE("MDK_OpenUart 1: %d", (pp_Cfg->v_Baudrate < MAX_UART_BAUD_RATE));
		X_TRACE("MDK_OpenUart 2: %d", (pp_Cfg->v_DataBits == DataBits8 || pp_Cfg->v_DataBits == DataBits7));
		X_TRACE("MDK_OpenUart 3: %d", (pp_Cfg->v_Parity == Odd || pp_Cfg->v_Parity == None || pp_Cfg->v_Parity == Even));
		X_TRACE("MDK_OpenUart 4: %d", (MC_GET_APP_TH()->IsUartOpened(vp_UartID)));
	}

	return vl_ret;
}


MDK_Result MDK_CloseUart(t_duart_UartID vp_UartID)
{
	LDK_CORE_WRITEMONITORLOG(0xa000000e, API_LIB_TIMEOUT, NULL, false);
	MDK_Result vl_ret = -1;
	if (!MC_GET_APP_TH())
	{
		return vl_ret;
	}

	X_DEBUG("-- :%d", vp_UartID);

	if (vp_UartID == VIRTULUART9 || vp_UartID == VIRTULUART10 || VIRTULUART12 == vp_UartID)
	{
		MC_GET_APP_TH()->CloseNetVirturalUart(vp_UartID);
		return 0;
	}

	MC_UART_ID_CHECK_RET();


	if (MC_GET_APP_TH()->IsUartOpened(vp_UartID))
	{
		vl_ret = MC_GET_APP_TH()->CloseUart(vp_UartID);
	}
	else
	{
		X_ERR("uart %d not open", vp_UartID);
	}
	X_TRACE_FL();
	return vl_ret;

}
int MDK_UartCommand(t_duart_UartID vp_UartID, t_UartCommand vp_Cmd, void* pp_Arg)
{
	LDK_CORE_WRITEMONITORLOG(0xa000000f, API_LIB_TIMEOUT, NULL, false);
	MDK_Result vl_ret = -1;
	if (!MC_GET_APP_TH())
	{
		return vl_ret;
	}
	MC_UART_ID_CHECK_RET();

	if (MC_GET_APP_TH()->IsUartOpened(vp_UartID))
	{
		//printf("%s %d, vp_cmd%d, value:0x%x\n",__func__,vp_UartID, vp_Cmd, (int)pp_Arg);

		vl_ret = MC_GET_APP_TH()->GetUartViaID(vp_UartID)->IoCtrl((unsigned int)vp_Cmd, pp_Arg);
	}

	return vl_ret;
}

void LDK_Monitor_Trace(void)
{
	u32 vl_len = 0;
	for (int i = 0; i < 50; i++)
	{
		char *pMsg = X_GETMONITORLOG(vl_len);
		if ((NULL != pMsg) && (vl_len > 0))
		{
#ifdef FUNC_USING_API_CALLBACK
			if (c_SVCTbl[0].p_GetSoftwareVersionFromMpu() > 0x52)
#else
		   	if (Dvrr_GetSoftwareVersionFromMpu() > 0x52)
#endif
			{
				LDK_On_DvrTrace(pMsg);
			}
		}
		else
		{
			break;
		}
	}
}

int LDK_EnableTrace(u32 vp_Enable)
{
	X_TRACE("LDK_EnableTrace mask:0x%x", vp_Enable);
#ifndef TRACE_ALWAYS_ON_DBG
	g_ctx.v_TraceMask = vp_Enable;
#endif
	return 0;
}

#define TRACE_BUF_SIZE  128

void MDK_LOG(const char* pp_Fmt, ...)
{
	if (g_ctx.v_TraceMask != 0)
	{
		LDK_CORE_WRITEMONITORLOG(0xa0000077, API_LIB_TIMEOUT, NULL, false);
		char al_Buffer[TRACE_BUF_SIZE];
		va_list  vp;
		va_start(vp, pp_Fmt);
		//memset(al_Buffer,0x00,sizeof(al_Buffer));
		x_vsnprintf(al_Buffer, TRACE_BUF_SIZE, pp_Fmt, vp);
		MDK_TRACE("%s", al_Buffer);
		va_end(vp);
	}
}

void *LDK_GetMonitorBuffer(u32 *ppBuffLen)
{
#ifdef ON_SOFT_REBOOT_DECT
	void *ptmp = X_LDK_GetMonitorBuffer(ppBuffLen);
	if (ptmp)
	{
		char v_sztmp[4] = { 0xA5,0x5A,0xCB,0xBC };
		if (memcmp((char *)(ptmp + *ppBuffLen - 4), v_sztmp, sizeof(v_sztmp)) != 0)
		{
			memcpy((char *)(ptmp + *ppBuffLen - 4), v_sztmp, sizeof(v_sztmp));
			MDK_TRACE("Hold Reboot:%x %x %x %x", *(char *)(ptmp + *ppBuffLen - 4), (char *)(ptmp + *ppBuffLen - 3), (char *)(ptmp + *ppBuffLen - 2), (char *)(ptmp + *ppBuffLen - 1));
		}
	}
	return ptmp;
#else
	return X_LDK_GetMonitorBuffer(ppBuffLen);
#endif
}


void MDK_TRACE(const char *fmt, ...)
{
	int vl_tid;
	//int vl_idx=0;
	//static int vl_Cnt =0;
	if (g_ctx.v_TraceMask == 0)
	{
		return;
	}
	LDK_CORE_WRITEMONITORLOG(0xa0000079, API_LIB_TIMEOUT, NULL, false);

	vl_tid = (int)getpid_ex();

#define MTRACE_BUF_SIZE	1024//240
	//printf("--- %s %d\n",__func__,__LINE__);

	//printf("g_ctx.v_TraceMask = %d\n", g_ctx.v_TraceMask);
	va_list  vp;
	u32 al_Buf[MTRACE_BUF_SIZE / sizeof(u32)];
	t_MdkMessage* pl_Msg = (t_MdkMessage*)al_Buf;

	char* al_TraceBuf = ((char*)al_Buf) + sizeof(t_MdkMessage);

	//////////////////////////////////////////////////////////////////////////
	pl_Msg->msg_type = MSG_SYS;
	pl_Msg->v_Code = MSG_SYS_TRACE_MSG;
	//MSG: t_MdkMessage+data

	if (al_TraceBuf - (char*)al_Buf > 4)
	{
		al_TraceBuf += sprintf(al_TraceBuf, "%s", " ");
	}

	va_start(vp, fmt);
	al_TraceBuf += x_vsnprintf(al_TraceBuf, MTRACE_BUF_SIZE - (al_TraceBuf - (char*)al_Buf), fmt, vp);
	va_end(vp);

	if (g_ctx.v_TraceMask == 0x01)
	{
		X_PRINT("%s\n", ((char*)al_Buf) + sizeof(t_MdkMessage));
	}
	else
	{
		if (MC_GET_APP_TH())
		{
			if (vl_tid == MC_GET_APP_TH()->GetThreadId())
			{//self thread -- do not send msg
				//printf("ddd sys call\n");
				//printf("--- %s %d\n",__func__,__LINE__);
#if 0
				MDK_On_SysMessage(MSG_SYS_TRACE_MSG, ((char*)al_Buf) + 8, NULL, NULL);
#endif
				X_ASSERT(0);
				//printf("--- %s %d\n",__func__,__LINE__);
			}
			else
			{
				pl_Msg->v_P1 = (void*)(strlen(((char*)al_Buf) + sizeof(t_MdkMessage)) + 1);
				MC_GET_APP_TH()->PostMDKMessage(pl_Msg);
			}
		}
	}
}




int LDK_PostUpdateEvent(const t_LpkUpdateEvent* pp_Event, const char* pp_File, int vp_Line, const char* pp_Func)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000010, API_LIB_TIMEOUT, NULL, false);
	int vl_ret = 0;
	//X_INFO("%s %d %d %d %d", __func__, __LINE__, pp_Event->m_Source, pp_Event->m_Magic, pp_Event->m_Type);

	/* ytj 20151207，HD10 V4版本，使用本地UDP，所以需要定义 ON_FSL_IMX6 */
#ifdef PLATFORM_MQ_USE_LOCALUDP		// MQ使用本地udp方式通讯
	x_asio_local_udp vl_cliDaemon(YW_DAEMON_NAME, false);
	if (!vl_cliDaemon.Open())
	{
		perror("local_udp ywdaemon");
		return -2;
	}
	if (OPEN_PPPD == pp_Event->m_Source)
	{
		vl_ret = vl_cliDaemon.Write((const void*)pp_Event, sizeof(t_LpkUpdateEvent) + pp_Event->m_MaxBakSize);

	}
	else
	{
		vl_ret = vl_cliDaemon.Write((const char*)pp_Event, sizeof(t_LpkUpdateEvent));
	}
	if (vl_ret > 0)
	{
		vl_ret = 0;
	}
	else
	{
		vl_ret = -1;
	}
	vl_cliDaemon.Close();
#else
	mqd_t vl_qid;
	//O_NONBLOCK BLOCK锟斤拷式
#if defined(WIN32)
	struct mq_attr mqattr = { O_NONBLOCK, 128, 240, 0 };
	mq_unlink(YW_DAEMON_NAME);
	if (NULL == (vl_qid = mq_open(YW_DAEMON_NAME, O_RDWR | O_CREAT | O_NONBLOCK, (0660), &mqattr)))
#else
	if ((vl_qid = mq_open(YW_DAEMON_NAME, O_RDWR | O_CREAT | O_NONBLOCK, (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH), NULL)) == -1)
#endif
	{
		perror("mq_open ywdaemon");
		X_ERROR("%d %d %d [%s:%d <%s()>]", pp_Event->m_Source, pp_Event->m_Magic, pp_Event->m_Type, pp_File, vp_Line, pp_Func);

		return -2;
	}

	if (OPEN_PPPD == pp_Event->m_Source)
	{
		vl_ret = mq_send(vl_qid, (const char*)pp_Event, sizeof(t_LpkUpdateEvent) + pp_Event->m_MaxBakSize, 0);

	}
	else
	{
		vl_ret = mq_send(vl_qid, (const char*)pp_Event, sizeof(t_LpkUpdateEvent), 0);
	}
#if defined(WIN32)
	mq_close(vl_qid);
#else
	close(vl_qid);
#endif

#endif
	//X_ASSERT(0);

	//gemini system no need mdk_core to reboot
#ifndef _GEMINI_FILE_SYSTEM
	if (LPK_SRC_USB_DAEMON == pp_Event->m_Source
		|| LPK_SRC_APP_REMOTE == pp_Event->m_Source)
	{
		if (MC_GET_APP_TH())
		{
			MC_GET_APP_TH()->PostSysMessage(MSG_SYS_KILLED, 0, 0, 0);
		}
	}
#endif
	return vl_ret;
}

//-------------------------------------------------------------------------------
int LDK_StartPartUpdate(u32 vp_LpkAddr)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000011, API_LIB_TIMEOUT, NULL, false);
	X_TRACE("LDK_StartPartUpdate @%08X meminfo size:%08X", vp_LpkAddr, g_ctx.m_meminfo.size);

	t_LpkUpdateEvent vl_ev;
	if (vp_LpkAddr >= g_ctx.m_meminfo.size
		|| (vp_LpkAddr & (g_ctx.m_meminfo.erasesize - 1)) != 0
		)
	{
		return -2;
	}
	memset(&vl_ev, 0x00, sizeof(t_LpkUpdateEvent));
	vl_ev.m_Source = LPK_SRC_APP_REMOTE;
	vl_ev.m_Magic = LPK_UPDATE_MAGIC_NUMBER;
	vl_ev.m_Abort = 0;
	vl_ev.m_Type = LPK_TYPE_PART;
	vl_ev.m_BackupAddr = 0;
	vl_ev.m_LpkAddr = vp_LpkAddr;
	vl_ev.m_MaxBakSize = 0;
	return LDK_PostUpdateEvent(&vl_ev, __FILE__, __LINE__, __func__);
}

int LDK_StartFileUpdate(u32 vp_LpkOffetInMtd, u32 vp_BackupOffsetInMtd, u32 vp_MaxSizeForBackup)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000012, API_LIB_TIMEOUT, NULL, false);
	t_LpkUpdateEvent vl_ev;
	t_LpkUpdateEvent *pl_Event;
	X_TRACE("UPDATE via FILE @%08X Bak:%08X Max:%08X", vp_LpkOffetInMtd, vp_BackupOffsetInMtd, vp_MaxSizeForBackup);

	if (vp_LpkOffetInMtd >= g_ctx.m_meminfo.size
		|| vp_BackupOffsetInMtd >= g_ctx.m_meminfo.size
#ifndef _GEMINI_FILE_SYSTEM
		|| (vp_LpkOffetInMtd & (g_ctx.m_meminfo.erasesize - 1)) == 0  //锟斤拷锟斤拷锟斤拷锟?
#endif
		|| (vp_BackupOffsetInMtd & (g_ctx.m_meminfo.erasesize - 1)) != 0
		)
	{
		return -2;
	}

	pl_Event = &vl_ev;

	memset(&vl_ev, 0x00, sizeof(t_LpkUpdateEvent));
	vl_ev.m_Magic = LPK_UPDATE_MAGIC_NUMBER;
	vl_ev.m_Abort = 0;
	vl_ev.m_Source = LPK_SRC_APP_REMOTE;
	vl_ev.m_Type = LPK_TYPE_FILE;
	vl_ev.m_BackupAddr = vp_BackupOffsetInMtd;//U锟斤拷锟斤拷锟矫憋拷锟斤拷
	vl_ev.m_LpkAddr = vp_LpkOffetInMtd;//U锟斤拷锟斤拷要锟斤拷. 锟斤拷锟斤拷锟矫碉拷址
	vl_ev.m_MaxBakSize = vp_MaxSizeForBackup;

	return LDK_PostUpdateEvent(&vl_ev, __FILE__, __LINE__, __func__);
}

int LDK_SendUsbLpkAck(
	u32 vp_Abort
	, u32 vp_SaveOffsetInMtd
	, u32 vp_MaxSizeForSave
)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000013, API_LIB_TIMEOUT, NULL, false);
	t_LpkUpdateEvent vl_ev;

	if (vp_SaveOffsetInMtd >= g_ctx.m_meminfo.size
		|| (vp_SaveOffsetInMtd & (g_ctx.m_meminfo.erasesize - 1)) != 0
		)
	{
		return -2;
	}

	memset(&vl_ev, 0x00, sizeof(t_LpkUpdateEvent));
	vl_ev.m_Magic = LPK_UPDATE_MAGIC_NUMBER;
	vl_ev.m_Source = LPK_SRC_APP_ACK;
	vl_ev.m_Abort = vp_Abort;
	vl_ev.m_Type = LPK_TYPE_PART;
	vl_ev.m_BackupAddr = 0;//U锟斤拷锟斤拷锟矫憋拷锟斤拷
	vl_ev.m_LpkAddr = vp_SaveOffsetInMtd;//U锟斤拷锟斤拷要锟斤拷. 锟斤拷锟斤拷锟矫碉拷址
	vl_ev.m_MaxBakSize = vp_MaxSizeForSave;

	return LDK_PostUpdateEvent(&vl_ev, __FILE__, __LINE__, __func__);
}


int MDK_WriteToUART(u8 vp_UartID, const void* pp_Data, u16 vp_Length)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000014, API_LIB_TIMEOUT, NULL, false);
	int vl_ret = -1;
	int vl_OrgID = vp_UartID;
	u64 vl_tmpSndTick = 0;
	//    X_DEBUG("^^^^^^^^%d %p %d", vp_UartID, pp_Data, vp_Length);
	if (!ldk_is_ttyReady() && vp_UartID >= EXT_UART0)
	{
		X_ERR("!ldk_is_ttyReady() vp_UartID = %d", vp_UartID);
		return -1;
	}

	if (vp_UartID == VIRTULUART9 || vp_UartID == VIRTULUART10 || VIRTULUART12 == vp_UartID)
	{
		MC_GET_APP_TH()->NetVirturalUartWrite(vp_UartID, (char *)pp_Data, vp_Length);
		return vl_ret;
	}

	MC_UART_ID_CHECK_RET();

	//X_DEBUG("^^^^^^^%d %s %d", vp_UartID, (char*)pp_Data, vp_Length);

	if (!MC_GET_APP_TH())
	{
		X_ERR("app_thread null = %p", MC_GET_APP_TH());
		return -1;
	}
	x_asio_uart* pl_uart = MC_GET_APP_TH()->GetUartViaID(vp_UartID);
	if (NULL != pl_uart)
	{
		switch (LDK_getMarkHwPcbType())
		{
		case LDK_HWPCB_P8105:
		case LDK_HWPCB_P9201:
		case LDK_HWPCB_P8301:
		case LDK_HWPCB_P9101:
		//case LDK_HWPCB_P9101_IV: ///---已停产
		case LDK_HWPCB_P8106:
		case LDK_HWPCB_P9103:
		case LDK_HWPCB_P9101_IT:
		case LDK_HWPCB_PK8:
			{
				if (vl_OrgID == UART1)
				{
					MC_GET_APP_TH()->setWatchdogTick();
				}
			}
			break;
		case LDK_HWPCB_PHD10:
			{
				if (vl_OrgID == UART0)
				{
					MC_GET_APP_TH()->setWatchdogTick();
				}
			}
			break;
		case LDK_HWPCB_P1573:
			{
				if (vl_OrgID == UART3)
				{
					MC_GET_APP_TH()->setWatchdogTick();
				}
			}
			break;
		default:
			break;
		}

		// 支持485串口配置
		if (GPIOID_MAX != pl_uart->GetCtrl485Io())
		{
			if (LDK_IsK3() || LDK_IsK7())
			{
				//发送
				//X_TRACE("GpioClear %d", pl_uart->GetCtrl485Io());
				GpioClear(pl_uart->GetCtrl485Io());
			}
			else
			{
				//X_TRACE("GpioSet %d", pl_uart->GetCtrl485Io());
				gr_gpioset(pl_uart->GetCtrl485Io(), __FILE__, __LINE__, __func__);
			}

			vl_tmpSndTick = MC_X_GET_MS_TICK();
		}

		if (pl_uart->IsReady())
		{
#ifdef FUNC_UART_TEST		// 测试串口接收字节数
			if (vl_OrgID >= 0 && vl_OrgID < UART_MAX_COUNT)
			{
				Uart_WriteBytes[vl_OrgID] += vp_Length;
			}
			else
			{
				Uart_WriteBytes[UART_MAX_COUNT] += vp_Length;
			}
#endif

			if (vl_OrgID >= EXT_UART0)
			{
				if (!pl_uart->IsException())
				{
					vl_ret = pl_uart->Write(pp_Data, vp_Length);
				}
				else
				{
					X_ERROR("Write IsException MappedID:%d name:%s OrgID:%d Len:%d,fd=%d", vp_UartID, pl_uart->GetDevName().c_str(), vl_OrgID, vp_Length, pl_uart->GetFd());
				}
			}
			else
			{
				vl_ret = pl_uart->Write(pp_Data, vp_Length);
			}

			if (vl_ret < 0 && errno != 11) //Resource temporarily unavailable
			{
				if (vl_OrgID >= EXT_UART0)
				{
					pl_uart->SetException();
					X_TRACE("SetException:ldk_close_all_tty_usb");
					ldk_close_all_tty_usb();
				}
				X_ERROR("Write failed MappedID:%d name:%s OrgID:%d %s Len:%d,fd=%d, errno=%d", vp_UartID, pl_uart->GetDevName().c_str(), vl_OrgID, strerror(errno), vp_Length, pl_uart->GetFd(), errno);
			}
			else
			{
				if ((int)(MC_GET_APP_TH()->GetLogUartID()) == vl_OrgID)
				{
					MDK_DumpMemory(pp_Data, vl_ret, "WRITE:");
				}
#ifdef _FUN_COMMUNICAT_STANDARD_TEST
                if(vl_OrgID == UART3)
					MDK_DbgDumpMemory((void *)pp_Data, (size_t)(vl_ret<16?vl_ret:16), "AdasW");
#endif
			}
		}

		// 支持485串口配置
		if (GPIOID_MAX != pl_uart->GetCtrl485Io())
		{
			u64 vl_tmpTick = 0;
			u64 vl_curTick = 0;

            //F8  485要求少于5毫秒
            vl_tmpTick = MC_X_GET_MS_TICK();
			vl_curTick = vl_tmpTick;
			while (!pl_uart->IsSendFinished485Data())
			{
				vl_curTick = MC_X_GET_MS_TICK();
				if ((vl_curTick - vl_tmpTick) > 800) //800ms
				{
					X_WARN("485 Serial write exception! len=%d", vp_Length);
					break;
				}
			}

			if (LDK_IsK3() || LDK_IsK7())
			{
				//接收
				gr_gpioset(pl_uart->GetCtrl485Io(), __FILE__, __LINE__, __func__);
				X_TRACE("GpioSet %d", pl_uart->GetCtrl485Io());
			}
			else
			{
				GpioClear(pl_uart->GetCtrl485Io());
				X_TRACE("GpioClear %d", pl_uart->GetCtrl485Io());
			}
			X_TRACE("difftime=%u st=%u ", (u32)(vl_curTick - vl_tmpTick), (u32)(vl_curTick - vl_tmpSndTick));
		}
		//X_TRACE("---vp_UartID=%d ret=%d", vp_UartID, vl_ret);
		//MDK_DumpMemory(pp_Data, vp_Length,"uart3:");
	}
	else
	{
#ifndef WIN32
		X_ERR("uart not found id=%d", vl_OrgID);
#endif
	}

	return vl_ret;
}

int MDK_WriteStrToUART(u8 vp_UartID, const char* pp_Data)
{
	// 	X_TRACE("%d", strlen(pp_Data));
	// 	for (int i = 0; i < strlen(pp_Data); ++i)
	// 	{
	// 		printf("%c", pp_Data[i]);
	// 	}
	// 	printf("\n");

	return MDK_WriteToUART(vp_UartID, pp_Data, (u16)strlen(pp_Data));
}

//////////////////////////////////////////////////////////////////////////
//TIMER
DLL_EXPORT MDK_Result	MDK_StartTimerEx(u8 vp_TimerID, u8 vp_AutoReload, u32 vp_IntervalInMS, u32 vp_Ctx)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000015, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return -1;
	}
	return	MC_GET_APP_TH()->StartHWTimer(vp_TimerID, vp_AutoReload, vp_IntervalInMS, vp_Ctx);
}
MDK_Result	MDK_StartTimer(u8 vp_TimerID, u8 vp_AutoReload, u32 vp_IntervalInMS)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000016, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return -1;
	}
	return	MC_GET_APP_TH()->StartHWTimer(vp_TimerID, vp_AutoReload, vp_IntervalInMS, 0);
	//return MDK_StartTimerEx( vp_TimerID, vp_AutoReload, vp_IntervalInMS, 0 );
}
DLL_EXPORT MDK_Result	MDK_StopTimer(u8 vp_TimerID)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000017, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return -1;
	}
	return	MC_GET_APP_TH()->StopHWTimer(vp_TimerID);
}

//////////////////////////////////////////////////////////////////////////
//TIME, TICK,HWTIMER

u64	MDK_TimeStamp(void)
{
#ifdef WIN32_DUMMY
	return GetTickCount64();
#else

	u64 vl_val = 0;
	//TIMER_JIFFIES64
	//MC_READ(g_ctx.v_gpioFid, 3, &vl_val);
	if (!MC_GET_APP_TH())
	{
		return 0;
	}
	MC_GET_APP_TH()->m_pGPIO->Read(&vl_val, 3 /*TIMER_JIFFIES64*/);
	return vl_val;
#endif
}

u32 MDK_GetMDKVersion(void)
{
	return LDK_VERSION;
}
u32 MDK_GetCoreVersion(void)
{
	return LDK_VERSION;
}
u32 MDK_GetHwTicks(t_MDK_HwTick vp_TickType)
{
#ifdef WIN32
	return (u32)GetTickCount64();
#elif defined(ON_MTK_MT6735_ANDROID) || defined(ON_ALLWINNER_V3_ANDROID)
	u32 vl_ticks = 0;
	switch (vp_TickType)
	{
	case HW_TICK_REG:
		vl_ticks = X_GetTickCount(); //++++暂时使用软tick
		vl_ticks = (vl_ticks * 2000);
		break;
	case HW_TICK_JIFF:
	case HW_TICK_JIFF2:
		vl_ticks = X_GetTickCount(); //++++暂时使用软tick
		break;
	default:
		break;
	}
	return vl_ticks;
#else
	u32 vl_Tick;
	if (!MC_GET_APP_TH())
	{
		return 0;
	}
	MC_GET_APP_TH()->m_pGPIO->Read(&vl_Tick, vp_TickType /*TIMER_JIFFIES64*/);
	//MC_READ(g_ctx.v_gpioFid,vp_TickType,&vl_Tick);
	return vl_Tick;
#endif
}

u32	MDK_GetTickCount(void)
{
	return MC_X_GET_TICK();
}


MDK_Result	MDK_SetDateTime(const t_MDK_DateTime* pp_DateTime)
{
	int vl_ret = 0;
	time_t vl_T;
	x_tm_t vl_tm;

	memset(&vl_tm, 0, sizeof(vl_tm));
	vl_tm.tm_year = pp_DateTime->Year;
	vl_tm.tm_mon = pp_DateTime->Month - 1;
	vl_tm.tm_mday = pp_DateTime->Day;

	vl_tm.tm_hour = pp_DateTime->Hour;
	vl_tm.tm_min = pp_DateTime->Miniute;
	vl_tm.tm_sec = pp_DateTime->Second;

	vl_T = (time_t)x_mktime(&vl_tm);
	X_INFO("APP change time to:%s", x_time_to_default_fmt(vl_tm).c_str());

#ifdef WIN32
	vl_ret = 0;
#else
#if PF_IsAndroid
    char v_szTime[128] = { 0 };
    //将UTC转化为本地时间
#ifdef FUNC_USING_API_CALLBACK
    x_gmtime(&vl_tm, c_SVCTbl[0].p_GetLocalTime(vl_T));
#else
	x_gmtime(&vl_tm, Dvrr_getLocalTime(vl_T));
#endif
    //X_DEBUG("Year: %d, Month: %d, Day: %d, Hour: %d, Min: %d, Sec: %d", vl_tm.tm_year, vl_tm.tm_mon + 1, vl_tm.tm_mday, vl_tm.tm_hour, vl_tm.tm_min, vl_tm.tm_sec);
	//android 必须用下面格式设置RTC 否则会无效
	x_snprintf(v_szTime, sizeof(v_szTime), "date -s \"%04d%02d%02d.%02d%02d%02d\"", vl_tm.tm_year, vl_tm.tm_mon + 1, vl_tm.tm_mday, vl_tm.tm_hour, vl_tm.tm_min, vl_tm.tm_sec);
	X_DEBUG("%s", v_szTime);
	x_system(v_szTime);
#else

	t_MDK_DateTime vl_tmptime;
	MDK_GetDateTime(&vl_tmptime);
	if ((pp_DateTime->Year == vl_tmptime.Year) && (pp_DateTime->Month == vl_tmptime.Month) && (pp_DateTime->Day == vl_tmptime.Day) && (pp_DateTime->Hour == vl_tmptime.Hour) && (pp_DateTime->Miniute == vl_tmptime.Miniute) && (abs(pp_DateTime->Second - vl_tmptime.Second) <= 1))
	{
		return 0;
	}
	char v_szTime[128] = { 0 };
    //将UTC转化为本地时间
#ifdef FUNC_USING_API_CALLBACK
    x_gmtime(&vl_tm, c_SVCTbl[0].p_GetLocalTime(vl_T));
#else
	x_gmtime(&vl_tm, Dvrr_getLocalTime(vl_T));
#endif
	x_snprintf(v_szTime, sizeof(v_szTime), "date -s \"%s\"", x_time_to_default_fmt(vl_tm).c_str());
	x_system(v_szTime);
#endif
#endif

	t_MDK_DateTime vl_time;
	MDK_GetDateTime(&vl_time);
	X_TRACE("MDK_GetDateTime::%04d_%02d_%02d %02d:%02d:%02d", vl_time.Year, vl_time.Month, vl_time.Day, vl_time.Hour, vl_time.Miniute, vl_time.Second);
	return vl_ret;
}

DLL_EXPORT MDK_Result	MDK_GetDateTime(t_MDK_DateTime* pp_DateTime)
{
	//返回UTC时间
	time_t vl_T;

	struct tm *time_now;

	time(&vl_T);
#ifdef FUNC_USING_API_CALLBACK
	//vl_T = (time_t)c_SVCTbl[0].p_MakeUtcTime((u32)vl_T);//会引起段错误
	vl_T -= 28800;//暂时这样进行UTC时间矫正
#else
	vl_T = (time_t)Dvrr_make_utc_time((u32)vl_T);
#endif
	time_now = gmtime(&vl_T); //gmtime 获取的时间是UTC时间localtime本地时间

	pp_DateTime->Year = time_now->tm_year + 1900;//110
	pp_DateTime->Month = time_now->tm_mon + 1;
	pp_DateTime->Day = time_now->tm_mday;

	pp_DateTime->Hour = time_now->tm_hour;
	pp_DateTime->Miniute = time_now->tm_min;
	pp_DateTime->Second = time_now->tm_sec;

	//X_TRACE("MDK_GetDateTime::%04d_%02d_%02d %02d:%02d:%02d", pp_DateTime->Year, pp_DateTime->Month, pp_DateTime->Day, pp_DateTime->Hour, pp_DateTime->Miniute, pp_DateTime->Second);
	return 0;
}
//////////////////////////////////////////////////////////////////////////
//i2c
MDK_Result MDK_SetI2C_Speed(t_MDK_I2C_Speed vp_SpeedIndex)
{
	X_TRACE("MDK_SetI2C_Speed %d", vp_SpeedIndex);
	LDK_CORE_WRITEMONITORLOG(0xa0000019, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return -1;
	}

	return MC_GET_APP_TH()->m_RfidDev->SetSpeed(vp_SpeedIndex);
}

MDK_Result MDK_I2C_SetMode(t_MDK_I2C_Mode vp_Mode)
{
	X_TRACE("MDK_I2C_SetMode %d", vp_Mode);

	LDK_CORE_WRITEMONITORLOG(0xa000001a, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return -1;
	}

	return MC_GET_APP_TH()->m_RfidDev->SetMode(vp_Mode);
}

MDK_Result MDK_I2CWrite(u8 vp_SlaveAdr, u8* pp_Data, u16 vp_Length)
{
	//X_TRACE("MDK_I2CWrite SlaveAdr=%p", vp_SlaveAdr);
	LDK_CORE_WRITEMONITORLOG(0xa000001b, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return -1;
	}
	if (LDK_IsK3C())
	{
		MC_GET_APP_TH()->setWatchdogTick();
	}

	return MC_GET_APP_TH()->m_RfidDev->Write(vp_SlaveAdr, pp_Data, vp_Length);
}

MDK_Result MDK_I2CWriteEx(u8 vp_SlaveAdr, u8* pp_Data, u16 vp_Length, u8 vp_Addr2)
{
	if (!MC_GET_APP_TH())
	{
		return -1;
	}

	return MC_GET_APP_TH()->m_RfidDev->WriteEx(vp_SlaveAdr, pp_Data, vp_Length, vp_Addr2);
}

MDK_Result MDK_I2CRead(u8 vp_SlaveAdr, u8* pp_Data, u16 vp_Length)
{
	LDK_CORE_WRITEMONITORLOG(0xa000001c, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return -1;
	}

	return MC_GET_APP_TH()->m_RfidDev->Read(vp_SlaveAdr, pp_Data, vp_Length);
}

MDK_Result MDK_I2CReadEx(u8 vp_SlaveAdr, u8* pp_Data, u16 vp_Length, u8 vp_Addr2)
{
	LDK_CORE_WRITEMONITORLOG(0xa000001d, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return -1;
	}

	return MC_GET_APP_TH()->m_RfidDev->ReadEx(vp_SlaveAdr, pp_Data, vp_Length, vp_Addr2);
}

//////////////////////////////////////////////////////////////////////////
//NOR
const void* MDK_NOR_GetReadPtr(void)
{
	X_TRACE_FL();
	LDK_CORE_WRITEMONITORLOG(0xa000001f, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return NULL;
	}
	return MC_GET_APP_TH()->m_pAppStore->GetReadPtr(MDK_NOR_BASEADDR);
}


#define NOR_WRITE_ONCE_SIZE  (128*1024)

int MDK_NOR_ReadFlash(u8* pp_OutputData, u32 vp_Address, u32 vp_SizeOfDataInBytes)
{
	int vl_ret = 0;
	LDK_CORE_WRITEMONITORLOG(0xa0000020, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return -1;
	}
	//X_TRACE("------------MDK_NOR_ReadFlash---------");

	s64 vl_offset = vp_Address;
#ifdef _GEMINI_FILE_SYSTEM
	void *vl_ppoutdata = (void *)pp_OutputData;
	vl_ret = MC_GET_APP_TH()->m_pAppStore->Read(vl_offset, vl_ppoutdata, vp_SizeOfDataInBytes);
#else
	MC_GET_APP_TH()->m_pAppStore->Seek(vp_Address, SEEK_SET);
	vl_ret = MC_GET_APP_TH()->m_pAppStore->Read(pp_OutputData, vp_SizeOfDataInBytes);
#endif
	if (vl_ret <= 0)
	{
		X_WARN("MDK_NOR_ReadFlash: vp_Address=0x%llx, size=%d vl_ret=%d", vl_offset, vp_SizeOfDataInBytes, vl_ret);
	}
	return vl_ret;
}


#define UBOOT_VER_STRING        "U-Boot 2010.06 (Sep 03 2014 - 10:24:30)"

#define UBOOT_VER_CMP_STRING    "U-Boot 20"

#define MDK_UBOOT_FLASH_FILE    "/dev/mtd0"
#define FLASH_BASE              0x58000000
#define FLASH_SIZE              0x100000


MDK_Result MDK_UPD_QueryRegionInfo(t_MDKUpdRegionInfo* pp_Info)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000021, API_LIB_TIMEOUT, NULL, false);
	int vl_ret = 0;
#ifdef _GEMINI_FILE_SYSTEM
#if PF_IsHisiLinux
	pp_Info->v_RegionBaseAddress = MDK_NOR_BASEADDR;
#else
	pp_Info->v_RegionBaseAddress = 0;
#endif
	pp_Info->v_SectorSize = MC_GET_APP_TH()->m_pAppStore->GetSectorSize();
	pp_Info->v_SectorCount = MC_GET_APP_TH()->m_pAppStore->GetSectorCount();
#else
	pp_Info->v_RegionBaseAddress = MDK_NOR_BASEADDR;
	pp_Info->v_SectorSize = g_ctx.m_meminfo.erasesize;
#ifdef T4_DEBUG
	pp_Info->v_SectorCount = MDK_NOR_UPD_SIZE / 100;//g_ctx.m_meminfo.erasesize;//测试有除0操作
#elif defined(V3_DEBUG)
	pp_Info->v_SectorCount = MDK_NOR_UPD_SIZE / 100;//g_ctx.m_meminfo.erasesize;//测试有除0操作
#else
	pp_Info->v_SectorCount = MDK_NOR_UPD_SIZE / g_ctx.m_meminfo.erasesize;
#endif
#endif
	X_TRACE("v_RegionBaseAddress=%p v_SectorSize=%d v_SectorCount=%d", pp_Info->v_RegionBaseAddress, pp_Info->v_SectorSize, pp_Info->v_SectorCount);
	return vl_ret;
}

// 参数vp_SectorAddress为Sector对齐的地址
MDK_Result MDK_UPD_EraseSector(u32 vp_SectorAddress)
{
	X_TRACE("MDK_UPD_EraseSector:0x%08x", vp_SectorAddress);
	LDK_CORE_WRITEMONITORLOG(0xa0000022, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return -1;
	}

	//X_TRACE("------------MDK_UPD_EraseSector---------");

	bool vl_bret = MC_GET_APP_TH()->m_pAppStore->EraseOneSector(vp_SectorAddress, EOST_128K);
	if (!vl_bret)
	{
		X_WARN("===MDK_UPD_EraseSector: vp_SectorAddress=%p vl_ret=%d", vp_SectorAddress, vl_bret);
	}

	int vl_ret = (vl_bret ? 0 : -1);
	return vl_ret;
}

MDK_Result MDK_UPD_Program(u32 vp_Address, const void* pp_Data, u32 vp_SizeOfDataInBytes)
{
	s64 vl_offset = vp_Address;
	int vl_ret = 0;
	LDK_CORE_WRITEMONITORLOG(0xa0000023, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return -1;
	}
	//X_TRACE("------------MDK_UPD_Program---------");

	//X_WARN("MDK_UPD_Program: vp_Address=%p, size=%d", vp_Address, vp_SizeOfDataInBytes);
	//MDK_DumpMemory(pp_Data, vp_SizeOfDataInBytes, "flash write:");
/*
#ifdef ON_3520
	//spi flash 按扇区写
	u32 vl_realEreaseSize = NOR_WRITE_ONCE_SIZE; //g_ctx.m_meminfo.erasesize
	int vl_offsetSectorbgnidx = (vp_Address/vl_realEreaseSize);
	int vl_addi = 0;
	if ((vp_Address%vl_realEreaseSize) != 0)
	{
		vl_addi = 1;
	}

	int vl_offsetSectorendidx = ((vp_Address+ vp_SizeOfDataInBytes)/vl_realEreaseSize) + vl_addi;
	if (vl_offsetSectorbgnidx == vl_offsetSectorendidx)
	{
		vl_offsetSectorendidx += 1;
	}
	u8 *vl_pTmpSector = (u8*)x_malloc(vl_realEreaseSize);
	int vl_ret = 0;
	int vl_dataOffset = 0;
	bool vl_bret;
	int i;
	for (i = vl_offsetSectorbgnidx; i < vl_offsetSectorendidx; i++)
	{
		if (vl_pTmpSector)
		{
			//X_TRACE("========== i %d =========",i);
			MC_GET_APP_TH()->m_pAppStore->Seek(i*vl_realEreaseSize, SEEK_SET);
			vl_ret = MC_GET_APP_TH()->m_pAppStore->Read(vl_pTmpSector, vl_realEreaseSize);
			if (vl_ret == (int)vl_realEreaseSize)
			{
				if (i == vl_offsetSectorbgnidx)
				{
					int vl_tmpOffset = (vl_realEreaseSize - vp_Address%vl_realEreaseSize);
					memcpy(vl_pTmpSector + (vp_Address%vl_realEreaseSize), (u8*)pp_Data + vl_dataOffset, ((((int)vp_SizeOfDataInBytes - vl_dataOffset) > vl_tmpOffset)?vl_tmpOffset:(vp_SizeOfDataInBytes - vl_dataOffset)));
					vl_dataOffset += vl_tmpOffset;
				}
				else
				{
					memcpy(vl_pTmpSector, (u8*)pp_Data + vl_dataOffset, (((vp_SizeOfDataInBytes - vl_dataOffset) > vl_realEreaseSize)?vl_realEreaseSize:(vp_SizeOfDataInBytes - vl_dataOffset)));
					vl_dataOffset += vl_realEreaseSize;
				}

#ifdef ON_3520
				if (vl_realEreaseSize == NOR_WRITE_ONCE_SIZE)
				{
					vl_bret = MC_GET_APP_TH()->m_pAppStore->EraseOneSector(i, EOST_128K);
				}
				else
				{
					vl_bret = MC_GET_APP_TH()->m_pAppStore->EraseOneSector(i, EOST_64K);
				}
#else
				vl_bret = MC_GET_APP_TH()->m_pAppStore->EraseOneSector(i);
#endif
				if (vl_bret)
				{
					MC_GET_APP_TH()->m_pAppStore->Seek(i*vl_realEreaseSize, SEEK_SET);
					vl_ret = MC_GET_APP_TH()->m_pAppStore->Write(vl_pTmpSector, vl_realEreaseSize);
					if (vl_ret != (int)vl_realEreaseSize)
					{
						X_WARN("write error!!!");
						break;
					}
				}
				else
				{
					X_WARN("erase error!!!");
				}
			}
			else
			{
				X_WARN("read error!!!");
				break;
			}
		}
		else
		{
			X_WARN("malloc error!!!");
		}
	}
	if (vl_ret == (int)vl_realEreaseSize && (i == vl_offsetSectorendidx))
	{
		vl_ret = vp_SizeOfDataInBytes;
		X_TRACE("write OK");
	}
	else
	{
		vl_ret = 0;
	}


	MDK_NOR_ReadFlash(vl_pTmpSector, vp_Address, vp_SizeOfDataInBytes);
	MDK_DumpMemory(vl_pTmpSector, vp_SizeOfDataInBytes, "flash read:");

	if (vl_pTmpSector)
	{
		x_free(vl_pTmpSector);
	}
#else
*/
#ifdef _GEMINI_FILE_SYSTEM
	vl_ret = MC_GET_APP_TH()->m_pAppStore->Write(vl_offset, (void *)pp_Data, vp_SizeOfDataInBytes);
#else
	MC_GET_APP_TH()->m_pAppStore->Seek(vp_Address, SEEK_SET);
	vl_ret = MC_GET_APP_TH()->m_pAppStore->Write(pp_Data, vp_SizeOfDataInBytes);
#endif
#if 0
	u8 *vl_pTmpSector = (u8*)x_malloc(vp_SizeOfDataInBytes);
	MDK_NOR_ReadFlash(vl_pTmpSector, vp_Address, vp_SizeOfDataInBytes);
	MDK_DumpMemory(vl_pTmpSector, vp_SizeOfDataInBytes, "flash read:");
	if (vl_pTmpSector)
	{
		x_free(vl_pTmpSector);
	}
#endif
	//#endif
	if (vl_ret <= 0)
	{
		X_WARN("MDK_UPD_Program: vp_Address=0x%llx, size=%d vl_ret=%d", vl_offset, vp_SizeOfDataInBytes, vl_ret);
	}
	return vl_ret;
}

//////////////////////////////////////////////////////////////////////////
//GPIO锟斤拷亟涌锟?
int GpioConfig(u32 gpioNb, const  GpioCfgType *pp_Config)
{
	//X_TRACE("****************GPIO:%d Dir=%d Fun=%d************", gpioNb, pp_Config->direction, pp_Config->function);

	LDK_CORE_WRITEMONITORLOG(0xa0000024, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return -1;
	}
	if (GPIO_NB_MAX > gpioNb && NULL != pp_Config)
	{
		// 		MC_GPIO_GENERAL1_SET(g_ctx.v_gpioFid,gpioNb,pConfig->function);
		// 		MC_GPIO_DIR_SET(g_ctx.v_gpioFid,gpioNb,pConfig->direction);
		MC_GET_APP_TH()->m_pGPIO->Config(gpioNb, pp_Config);
		return 0;
	}
	else
	{
		return -1;
	}
}

void GpioSetDebounceValue(u32 vp_ValueInMs)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000025, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return;
	}
	//MC_GPIO_SET_DEBOUNCE(g_ctx.v_gpioFid,vp_ValueInMs);
	MC_GET_APP_TH()->m_pGPIO->SetDebounceValue(vp_ValueInMs);
}

GpioLevelType GpioGet(u32 gpioNb)//锟斤拷锟斤拷前状态
{
	LDK_CORE_WRITEMONITORLOG(0xa0000026, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return GPIO_LVL_LOW;
	}

#if !PF_IsHisiLinux
	// add by zhq 150323 去掉刷屏
	if (gpioNb == 0)
	{
		return GPIO_LVL_LOW;
	}
#endif

	//X_TRACE("GpioGet-> num:%d");
	return MC_GET_APP_TH()->m_pGPIO->GpioGet(gpioNb);
}
void GpioSet(u32 gpioNb)
{
	gr_gpioset(gpioNb, __FILE__, __LINE__, __func__);
}

void gr_gpioset(u32 gpioNb, const char* pp_File, int vp_Line, const char* pp_Func)//置高
{
	LDK_CORE_WRITEMONITORLOG(0xa0000027, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return;
	}
	//X_TRACE("GpioSet-> num:%d", gpioNb);

	MC_GET_APP_TH()->m_pGPIO->GpioSet(gpioNb, GPIO_LVL_HIGH, pp_File, vp_Line, pp_Func);
}

void GpioClear(u32 gpioNb)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000028, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return;
	}
	//X_TRACE("GpioClear-> num:%d", gpioNb);
#if defined(ON_3520D_V300) && defined (ON_3520)
#ifdef SUPPORT_GET_USB_INFO				// 支持获取USB信息
	///--- F8T下防止对6124下电引起系统复位问题
	//if (gpioNb == V300_GPIOID01 || V300_GPIOID02 == gpioNb)
	//	return;
#endif
#endif

	MC_GET_APP_TH()->m_pGPIO->GpioSet(gpioNb, GPIO_LVL_LOW, __FILE__, __LINE__, __func__);
}
void GpioEnableInterrupt(u32 gpioNb, u32 vp_DebounceMs, GpioTriggerType type)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000029, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return;
	}
	MC_GET_APP_TH()->m_pGPIO->EnableEINT(gpioNb, vp_DebounceMs, type);
}

void GpioDisableInterrupt(u32 gpioNb)
{
	LDK_CORE_WRITEMONITORLOG(0xa000002a, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return;
	}
	MC_GET_APP_TH()->m_pGPIO->DisableEINT(gpioNb);
}

u32 GpioGetHwVersion(void)
{
	if (!MC_GET_APP_TH())
	{
		return 0;
	}
	return MC_GET_APP_TH()->m_pGPIO->GetHwVersion();
}

#ifdef FUNC_USING_API_CALLBACK

void MDK_SVC_Register(t_SvcId vp_SvcID, SVC_HANDLE handle)
{

	if (vp_SvcID == SVC_DVRR && NULL != handle)
	{
		memcpy(&c_SVCTbl[0], handle, sizeof(t_SVCFunc));
	}

}

SVC_HANDLE MDK_SVC_Open(t_SvcId vp_SvcID)
{
	LDK_CORE_WRITEMONITORLOG(0xa000002b, API_LIB_TIMEOUT, NULL, false);
	if (vp_SvcID == (t_SvcId)c_SVCTbl[0].v_Id)
	{
		c_SVCTbl[0].p_Init(&c_SVCTbl[0]);
		return &c_SVCTbl[0];
	}
	return NULL;
}

#else

extern DLL_EXPORT int OnInit(const void* pp_Ctx);
extern DLL_EXPORT CB_PARAM OnCommand(const void* pp_Data, u32 vp_Len);

SVC_HANDLE MDK_SVC_Open(t_SvcId vp_SvcID)
{
	LDK_CORE_WRITEMONITORLOG(0xa000002b, API_LIB_TIMEOUT, NULL, false);
	if (vp_SvcID == SVC_DVRR)
	{
		c_SVCTbl[0].p_Init = OnInit;
		c_SVCTbl[0].p_Command = OnCommand;
		c_SVCTbl[0].p_libHandle = (void*)0x1234;
		c_SVCTbl[0].v_Id = (u32)vp_SvcID;
		c_SVCTbl[0].p_Init(&c_SVCTbl[0]);
		return &c_SVCTbl[0];
	}
	return NULL;
}
#endif

CB_PARAM MDK_SVC_Command(SVC_HANDLE vp_SvcHandle, const void* pp_Data, u32 vp_Len)
{
	LDK_CORE_WRITEMONITORLOG(0xa000002c, API_LIB_TIMEOUT, NULL, false);
#if 1
	t_SVCFunc* pl_H = (t_SVCFunc*)vp_SvcHandle;
	if (NULL != pl_H && pl_H == &c_SVCTbl[pl_H->v_Id])
	{
		return pl_H->p_Command(pp_Data, vp_Len);
	}
	else
	{
		perror(__func__);
		return NULL;
	}
#endif
	//X_ASSERT(0);
	return NULL;
}

#ifdef FUNC_USE_HTTP_SERVER // 此为通用功能,只是在Http模块中首次使用,所以用宏隔开,以后测试正常了,可以删除宏
DLL_EXPORT int SVC_Command(const t_DVRR_Cmd* pDvrrCmd)
{
	// 计算t_DVRR_Cmd结构实际占用的长度
	u32 nCmdLen = GetDvrrCmdHeadLen() + pDvrrCmd->uLen;

	// 注意:OnCommand返回值一定是NULL,所以判断返回值无意义
	//		证明: dvrr_entry.cpp => DLL_EXPORT CB_PARAM OnCommand(const void* pp_Data,u32 vp_Len) => dvrr_thread::PostAppCmd => 固定返回NULL
	OnCommand((const void*)pDvrrCmd, nCmdLen);

	return 0;
}
#endif // FUNC_USE_HTTP_SERVER

CB_PARAM MDK_SVC_Command_Ex(SVC_HANDLE vp_SvcHandle, const void* pp_Head, u32 vp_HeadLen, const void* pp_Data, u32 vp_Len)
{
	LDK_CORE_WRITEMONITORLOG(0xa000002c, API_LIB_TIMEOUT, NULL, false);
#if 1
	t_SVCFunc* pl_H = (t_SVCFunc*)vp_SvcHandle;
	if (NULL != pl_H && pl_H == &c_SVCTbl[pl_H->v_Id] && (vp_HeadLen + vp_Len) < sizeof(ag_OnCommandExData))
	{
		if (vp_HeadLen)
		{
			memcpy(ag_OnCommandExData, pp_Head, vp_HeadLen);
		}
		if (vp_Len)
		{
			memcpy(ag_OnCommandExData + vp_HeadLen, pp_Data, vp_Len);
		}

		return pl_H->p_Command(ag_OnCommandExData, (vp_HeadLen + vp_Len));
	}
	else
	{
		perror(__func__);
		return NULL;
	}
#endif
	//X_ASSERT(0);
	return NULL;
}

/* SVC--------------------------------------------- */
DLL_EXPORT void* MDK_MapAddress(u32 vp_Addr, u32 vp_Size, u8 vp_Writable)
{
	LDK_CORE_WRITEMONITORLOG(0xa000002e, API_LIB_TIMEOUT, NULL, false);
#ifdef WIN32_DUMMY
	printf("%s %08x %08x\n", __func__, vp_Addr, vp_Size);
	return x_malloc(vp_Size);
#else
	if (vp_Writable)
	{
		return mmap(0, vp_Size, PROT_READ | PROT_WRITE, MAP_SHARED, g_ctx.v_norReadFid, vp_Addr);
	}
	else
	{
		return mmap(0, vp_Size, PROT_READ, MAP_SHARED, g_ctx.v_norReadFid, vp_Addr);
	}
#endif
}

DLL_EXPORT int MDK_UnMapAddress(const void* pp_Addr, u32 vp_Size)
{
	LDK_CORE_WRITEMONITORLOG(0xa000002f, API_LIB_TIMEOUT, NULL, false);
#ifdef WIN32_DUMMY
	printf("%s %p %08x\n", __func__, pp_Addr, vp_Size);
	x_free((void*)pp_Addr);
	return 0;
#else
	return munmap((void*)pp_Addr, vp_Size);
#endif
}

DLL_EXPORT MDK_Result MDK_SendMessage(u32 vp_Type, u32 vp_MessageID, CB_PARAM vp_Param1, CB_PARAM vp_Param2, CB_PARAM vp_Param3)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000031, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return -1;
	}
	t_MdkMessage vl_msg;
	/*锟斤拷息锟斤拷识锟斤拷*/
	vl_msg.msg_type = vp_Type;
	vl_msg.v_Code = vp_MessageID;
	vl_msg.v_P1 = (CB_PARAM)vp_Param1;
	vl_msg.v_P2 = (CB_PARAM)vp_Param2;
	vl_msg.v_P3 = (CB_PARAM)vp_Param3;

	int vl_ret = MC_GET_APP_TH()->PostMDKMessage(&vl_msg);
	return vl_ret;
}

MDK_Result MDK_SendUserMessage(u32 vp_MessageID, CB_PARAM vp_Param1, CB_PARAM vp_Param2, CB_PARAM vp_Param3)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000032, API_LIB_TIMEOUT, NULL, false);
	return MDK_SendMessage(MSG_APP, vp_MessageID, vp_Param1, vp_Param2, vp_Param3);
}

int Dvrr_PostMsgToMApp(const void* pp_Svc, const void* pp_Msg, u32 vp_Len)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000033, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return -1;
	}
	MC_GET_APP_TH()->PostSVCMessage(pp_Svc, pp_Msg, vp_Len);
	return 0;
}

//////////////////////////////////////////////////////////////////////////
//memory
static size_t g_UsedMemory = 0;
void * MDK_calloc(size_t nmemb, size_t size)
{
	g_UsedMemory += size;
	return calloc(nmemb, size);
}
DLL_EXPORT void *MDK_malloc(size_t size)
{
	g_UsedMemory += size;
	void *p = x_malloc(size);
	if (NULL == p)
	{
		X_ERROR("!!!MDK_malloc: (nil) size = %d", size);
	}
	return p;
}
DLL_EXPORT void MDK_free(void *ptr)
{
	//g_UsedMemory -=size;
	x_free(ptr);
}
void *MDK_realloc(void *ptr, size_t size)
{
	return realloc(ptr, size);
}

//////////////////////////////////////////////////////////////////////////

int	get_ppp_stats(const char* pp_IF, u32* pp_RxNb, u32* pp_TxNb)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000034, API_LIB_TIMEOUT, NULL, false);
#ifdef WIN32
	*pp_RxNb = 0;
	*pp_TxNb = 0;
	return 0;
#else
	int vl_ret = -1;
	ifpppstatsreq req;

	memset(&req, 0, sizeof(req));

	req.stats_ptr = (caddr_t)&req.stats;
	x_snprintf(req.ifr__name, sizeof(req.ifr__name), pp_IF);

	vl_ret = ioctl(g_ctx.v_ppp_sock_fd, SIOCGPPPSTATS, &req);
	if (vl_ret < 0)
	{
		//X_ERROR("ioctl error pp_IF=%s! ret=%d", pp_IF, vl_ret);
	}
	else
	{
		*pp_RxNb = req.stats.p.ppp_ibytes;
		*pp_TxNb = req.stats.p.ppp_obytes;
	}
	//X_TRACE("===get_ppp_stats: name=%s, fid=%d, ret=%d, RxNb=%d, TxNb=%d", req.ifr__name, g_ctx.v_ppp_sock_fd, vl_ret, *pp_RxNb, *pp_TxNb);
	return vl_ret;
#endif
}

DLL_EXPORT MDK_Result MDK_Get_GPRSBytesCount(PPPInstance vp_Inst, u32* pp_RxNb, u32* pp_TxNb)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000035, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return -1;
	}
	return get_ppp_stats(MC_GET_APP_TH()->m_pppCtx.a_InterfaceName, pp_RxNb, pp_TxNb);
}


//SOCKET EPOLL 处理函数
// D. J. Bernstein
// 	TCP/IP
// 	Non-blocking BSD socket connections
// Situation: You set up a non-blocking socket and do a connect() that returns -1/EINPROGRESS or -1/EWOULDBLOCK. You select() the socket for writability. This returns as soon as the connection succeeds or fails. (Exception: Under some old versions of Ultrix, select() wouldn't notice failure before the 75-second timeout.)
//
// Question: What do you do after select() returns writability? Did the connection fail? If so, how did it fail?
//
// 		  If the connection failed, the reason is hidden away inside something called so_error in the socket. Modern systems let you see so_error with getsockopt(,,SO_ERROR,,), but this isn't portable---in fact, getsockopt() can crash old systems. A different way to see so_error is through error slippage: any attempt to read or write data on the socket will return -1/so_error.
//
// 		  Sometimes you have data to immediately write to the connection. Then you can just write() the data. If connect() failed, the failure will be reported by write(), usually with the right connect() errno. This is the solution I supplied for IRC in 1990. Unfortunately, on some systems, under some circumstances, write() will substitute EPIPE for the old errno, so you lose information.
//
// 		  Another possibility is read(fd,&ch,0). If connect() succeeded, you get a 0 return value, except under Solaris, where you get -1/EAGAIN. If connect() failed, you should get the right errno through error slippage. Fatal flaw: under Linux, you will always get 0.
//
// 		  Another possibility is read(fd,&ch,1), but this leads to programming difficulties, since a character may in fact have arrived. Everything that reads the socket has to know about your pre-buffered character.
//
// 		  Another possibility is recv(,,,MSG_PEEK). Unfortunately, the recv() code for stream sockets was, historically, completely untested and a massive source of bugs. I'd steer clear of this.
//
// 		  Another possibility is to select() for readability. But this is wrong, because the connect() may have succeeded and data may have arrived before you had time to do a select().
//
// 		  Another possibility is getpeername(). If the socket is connected, getpeername() will return 0. If the socket is not connected, getpeername() will return ENOTCONN, and read(fd,&ch,1) will produce the right errno through error slippage. This is a combination of suggestions from Douglas C. Schmidt and Ken Keys.
//
// 		  Another possibility is a second connect(). This seems to be a strictly worse solution than getpeername(): it doubles network traffic if the connection failed (with some TCP stacks), and doesn't improve reliability. Ken Keys mentions that this works with SOCKS.
/*
   int nRecvBuf=32*1024;//设置为32K
   setsockopt(s,SOL_SOCKET,SO_RCVBUF,(const char*)&nRecvBuf,sizeof(int));
   1、通过上面语句可以简单设置缓冲区大小,测试证明：跟epoll结合的时候只有当单次发送的数据全被从缓冲区读完毕之后才会再次被触发，
   多次发送数据如果没有读取完毕当缓冲区未满的时候数据不会丢失，会累加到后面。

   2、 如果缓冲区未满，同一连接多次发送数据会多次收到EPOLLIN事件。
   单次发送数据>socket缓冲区大小的数据数据会被阻塞分次发送，所以循环接收可以用ENLIGE错误判断。

   3、如果缓冲区满，新发送的数据不会触发epoll事件（也无异常），每次recv都会为缓冲区腾出空间，只有当缓冲区空闲大小能够
   再次接收数据epollIN事件可以再次被触发
   接收时接收大小为
   0表示客户端断开（不可能有0数据包触发EPOLLIN）,
   -1表示异常，针对errorno进行判断可以确定是合理异常还是需要终止的异常，
   >0而不等于缓冲区大小表示单次发送结束。

   4、 如果中途临时调整接收缓存区大小，并且在上一次中数据没有完全接收到用户空间，数据不会丢失，会累加在一起

   找到一个方法, 不过还没验证:
   1，当本地还没调用connect函数，却将套接字送交epoll检测，epoll会产生一次 EPOLLPRI | EPOLLOUT | EPOLLERR， 也就是产生一个值为14的events.
   2，当本地connect事件发生了，但建立连接失败，则epoll会产生一次 EPOLLIN | EPOLLPRI | EPOLLHUP， 也就是一个值为19的events.
   3，当connect函数也调用了，而且连接也顺利建立了，则epoll会产生一次 EPOLLOUT， 值为4，即表明套接字已经可写。

   收到EPOLLOUT也不能认为是TCP层次上connect(2)已经成功，要调用getsockopt看SOL_SOCKET的SO_ERROR是否为0。
   若为0，才表明真正的TCP层次上connect成功。至于应用层次的server是否收/发数据，那是另一回事了。
   */

DLL_EXPORT BSD_SOCKET MDK_bsd_socket(int vp_AddressFamily, int vp_SocketType, int vp_Protocol, PPPInstance vp_AppInstance)
{
	X_TRACE("family %u type %d protocol %d app %d", vp_AddressFamily, vp_SocketType, vp_Protocol, vp_AppInstance);
	LDK_CORE_WRITEMONITORLOG(0xa0000036, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return -1;
	}

	return MC_GET_APP_TH()->m_ipnet.CreateSocket(vp_AddressFamily, vp_SocketType, vp_Protocol);
}

// 应用定义bsd到基本结构转换
static void ldk_net_bsd_to_base(const struct bsd_sockaddr_in * p_RemoteSockAddr, const int vp_RemoteSockAddrLen, struct sockaddr_in * remoteSockAddr, int * sockaddrlen)
{
	memset(remoteSockAddr, 0, sizeof(struct sockaddr_in));
	remoteSockAddr->sin_family = p_RemoteSockAddr->sin_family;
#ifdef WIN32
	remoteSockAddr->sin_addr.s_addr = p_RemoteSockAddr->sin_addr.s_addr;
#else
	//printf("ldk_net_bsd_to_base:%s\n", inet_ntoa(*(struct in_addr*)&p_RemoteSockAddr->sin_addr));
#ifdef s_addr
#undef s_addr
#endif
	remoteSockAddr->sin_addr.s_addr = p_RemoteSockAddr->sin_addr.S_un.S_addr;
#if 1
#define s_addr  S_un.S_addr
#endif
#endif
	remoteSockAddr->sin_port = p_RemoteSockAddr->sin_port;

	*sockaddrlen = sizeof(struct sockaddr);
}

// 基本结构到应用定义bsd转换
static void ldk_net_base_to_bsd(const struct sockaddr_in * remoteSockAddr, const int * sockaddrlen, struct bsd_sockaddr * p_RemoteSockAddr, int* vp_RemoteSockAddrLen)
{
	struct bsd_sockaddr_in * pp_RemoteSockAddr = (struct bsd_sockaddr_in *)p_RemoteSockAddr;

	pp_RemoteSockAddr->sin_family = remoteSockAddr->sin_family;
#ifdef WIN32
	pp_RemoteSockAddr->sin_addr.s_addr = remoteSockAddr->sin_addr.s_addr;
#else
	X_TRACE("%s", inet_ntoa(*(struct in_addr*)&remoteSockAddr->sin_addr));
#ifdef s_addr
#undef s_addr
#endif
	pp_RemoteSockAddr->sin_addr.S_un.S_addr = remoteSockAddr->sin_addr.s_addr;
#if 1
#define s_addr  S_un.S_addr
#endif
#endif
	pp_RemoteSockAddr->sin_port = remoteSockAddr->sin_port;

	*vp_RemoteSockAddrLen = sizeof(struct bsd_sockaddr);
}

static int ldk_tcp_connect(BSD_SOCKET vp_SocketId, const struct bsd_sockaddr * pp_RemoteSockAddr, int vp_RemoteSockAddrLen, u32 vp_EvDelaySeconds)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000037, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return -1;
	}

	// 结构转换
	struct sockaddr_in remoteSockAddr;
	int remoteSockAddrLen;
	ldk_net_bsd_to_base((const struct bsd_sockaddr_in *)pp_RemoteSockAddr, vp_RemoteSockAddrLen, &remoteSockAddr, &remoteSockAddrLen);

	return MC_GET_APP_TH()->m_ipnet.SocketConnect(vp_SocketId, (struct sockaddr *) &remoteSockAddr, remoteSockAddrLen);
}

DLL_EXPORT int MDK_bsd_connect_ex(BSD_SOCKET vp_SocketId, const struct bsd_sockaddr * pp_RemoteSockAddr, int vp_RemoteSockAddrLen, u32 vp_EvDelaySeconds)
{
	///--- 已废弃 2017.06.21
	X_INFO("obsolete connect %d", vp_SocketId);
	LDK_CORE_WRITEMONITORLOG(0xa0000038, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return -1;
	}

	return ldk_tcp_connect(vp_SocketId, pp_RemoteSockAddr, vp_RemoteSockAddrLen, vp_EvDelaySeconds);
}

DLL_EXPORT int MDK_bsd_connect(BSD_SOCKET vp_SocketId, const struct bsd_sockaddr * pp_RemoteSockAddr, int vp_RemoteSockAddrLen)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000039, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return -1;
	}

	X_TRACE("socket %u ", vp_SocketId);

	return ldk_tcp_connect(vp_SocketId, pp_RemoteSockAddr, vp_RemoteSockAddrLen, 0);
}

DLL_EXPORT int MDK_bsd_bind(BSD_SOCKET vp_SocketId, const struct bsd_sockaddr * pp_LocalSockAddr, int vp_LocalSockAddrLen)
{
	LDK_CORE_WRITEMONITORLOG(0xa000003a, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return -1;
	}

	// 结构转换
	struct sockaddr_in localSockAddr;
	int localSockAddrLen;
	ldk_net_bsd_to_base((const struct bsd_sockaddr_in *)pp_LocalSockAddr, vp_LocalSockAddrLen, &localSockAddr, &localSockAddrLen);

	return MC_GET_APP_TH()->m_ipnet.bsd_bind(vp_SocketId, (struct sockaddr *) &localSockAddr, localSockAddrLen);
}

DLL_EXPORT int MDK_bsd_recvfrom(BSD_SOCKET vp_SocketId, char * pp_Buffer, int vp_BufferLen, int vp_Flags, struct bsd_sockaddr * pp_SourceAddr, int * pp_SourceAddrLen)
{
	LDK_CORE_WRITEMONITORLOG(0xa000003b, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return -1;
	}
	int ret;
	// 结构转换
	struct sockaddr_in sourceAddr = { 0 };
	*pp_SourceAddrLen = sizeof(struct sockaddr_in);
	ret = MC_GET_APP_TH()->m_ipnet.bsd_recvfrom(vp_SocketId, pp_Buffer, vp_BufferLen, vp_Flags, (struct sockaddr*)&sourceAddr, pp_SourceAddrLen);

	ldk_net_base_to_bsd(&sourceAddr, pp_SourceAddrLen, pp_SourceAddr, pp_SourceAddrLen);

	return ret;
}

DLL_EXPORT int MDK_bsd_sendto(BSD_SOCKET vp_SocketId, const char * pp_Buffer, int vp_BufferLen, int vp_Flags, const struct bsd_sockaddr * pp_DestAddr, int vp_DestAddrLen)
{
	LDK_CORE_WRITEMONITORLOG(0xa000003c, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return -1;
	}

	// 结构转换
	struct sockaddr_in destAddr = { 0 };
	int destAddrLen;
	ldk_net_bsd_to_base((const struct bsd_sockaddr_in *)pp_DestAddr, vp_DestAddrLen, &destAddr, &destAddrLen);

	return MC_GET_APP_TH()->m_ipnet.bsd_sendto(vp_SocketId, pp_Buffer, vp_BufferLen, vp_Flags, (struct sockaddr*)&destAddr, destAddrLen);
}

DLL_EXPORT int MDK_bsd_close(BSD_SOCKET vp_SocketId)
{
	X_TRACE("socket %u", vp_SocketId);
	LDK_CORE_WRITEMONITORLOG(0xa000003c, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return -1;
	}

	return MC_GET_APP_TH()->m_ipnet.CloseSocket(vp_SocketId);
}

DLL_EXPORT int MDK_bsd_shutdown(BSD_SOCKET vp_SocketId, int vp_How)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000042, API_LIB_TIMEOUT, NULL, false);
	return MC_GET_APP_TH()->m_ipnet.bsd_shutdown(vp_SocketId, vp_How);
}
DLL_EXPORT int MDK_bsd_recv(BSD_SOCKET vp_SocketId, char * pp_Buffer, int vp_BufferLen, int vp_Flags)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000043, API_LIB_TIMEOUT, NULL, false);
#ifdef _FUN_COMMUNICAT_STANDARD_TEST
		int vl_ret = MC_GET_APP_TH()->m_ipnet.bsd_recv(vp_SocketId, pp_Buffer, vp_BufferLen, vp_Flags);
        if(vl_ret > 0 && pp_Buffer[0] == 0x7E)
			MDK_DbgDumpMemory((void *)pp_Buffer, (size_t)(vp_BufferLen<32?vp_BufferLen:32), "DnPlt");
		return vl_ret;
#else
	return MC_GET_APP_TH()->m_ipnet.bsd_recv(vp_SocketId, pp_Buffer, vp_BufferLen, vp_Flags);
#endif
}

static int ldk_tcp_send(BSD_SOCKET vp_SocketId, const char * pp_Buffer, int vp_BufferLen, int vp_Flags)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000044, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return -1;
	}
	//X_TRACE("socket %u len:%d %s %d", vp_SocketId, vp_BufferLen, __func__, __LINE__);
	//return send(vp_SocketId, pp_Buffer, vp_BufferLen, vp_Flags);
#ifdef _FUN_COMMUNICAT_STANDARD_TEST
    MDK_DbgDumpMemory((void *)pp_Buffer, (size_t)(vp_BufferLen<32?vp_BufferLen:32), "UpPlt");
#endif
	return MC_GET_APP_TH()->m_ipnet.SocketWrite(vp_SocketId, pp_Buffer, vp_BufferLen, vp_Flags);
}

DLL_EXPORT int MDK_bsd_send(BSD_SOCKET vp_SocketId, const char * pp_Buffer, int vp_BufferLen, int vp_Flags)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000045, API_LIB_TIMEOUT, NULL, false);

#ifdef FUNC_USING_API_CALLBACK
	// 此为宇通测流量的临时方案 (详见mmst_main.cpp中MMST_OnMdkBsdSend函数的注释)
	// 所以暂时不添加到安卓接口中,免得以后还得删
#else
	MMST_OnMdkBsdSend(vp_SocketId, pp_Buffer, vp_BufferLen, vp_Flags);
#endif


	return ldk_tcp_send(vp_SocketId, pp_Buffer, vp_BufferLen, vp_Flags);
}

DLL_EXPORT char* MDK_bsd_inet_ntoa(struct bsd_in_addr vp_IpAddress, char* pp_Buffer, int vp_BufferLen)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000048, API_LIB_TIMEOUT, NULL, false);
	return strncpy(pp_Buffer, inet_ntoa(*((struct in_addr*) (&vp_IpAddress))), vp_BufferLen);
}
DLL_EXPORT u32 MDK_bsd_inet_addr(const char* pp_IpAddress)
{
	return inet_addr(pp_IpAddress);
}
DLL_EXPORT u32 MDK_bsd_ntohl(u32 vp_NetworkLong)
{
	return ntohl(vp_NetworkLong);
}
DLL_EXPORT u32 MDK_bsd_htonl(u32 vp_HostLong)
{
	return htonl(vp_HostLong);
}
DLL_EXPORT u16 MDK_bsd_ntohs(u16 vp_NetworkShort)
{
	return ntohs(vp_NetworkShort);
}
DLL_EXPORT u16 MDK_bsd_htons(u16 vp_HostShort)
{
	return htons(vp_HostShort);
}
DLL_EXPORT int MDK_bsd_setsockopt(BSD_SOCKET vp_SocketId, int vp_Level, int vp_OptName, const char *pp_OptVal, int vp_OptLen)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000049, API_LIB_TIMEOUT, NULL, false);
	return MC_GET_APP_TH()->m_ipnet.bsd_setsockopt(vp_SocketId, vp_Level, vp_OptName, pp_OptVal, vp_OptLen);
}
DLL_EXPORT int MDK_bsd_getsockopt(BSD_SOCKET vp_SocketId, int vp_Level, int vp_OptName, char *pp_OptVal, int *pp_OptLen)
{
	LDK_CORE_WRITEMONITORLOG(0xa000004a, API_LIB_TIMEOUT, NULL, false);
	return MC_GET_APP_TH()->m_ipnet.bsd_getsockopt(vp_SocketId, vp_Level, vp_OptName, pp_OptVal, pp_OptLen);
}

DLL_EXPORT int MDK_bsd_getlast_error(PPPInstance anInstance)
{
	return MC_GET_APP_TH()->m_ipnet.GetLastSockError();
}

int	MDK_get_if_addr(const char* pp_IF, struct bsd_in_addr* in)
{
	LDK_CORE_WRITEMONITORLOG(0xa000004f, API_LIB_TIMEOUT, NULL, false);
	u32 S_addr = 0;
	int ret = 0;
	ret = MC_GET_APP_TH()->m_ipnet.get_if_addr(pp_IF, &S_addr);

	in->s_addr = S_addr;
	//in->s_host = temp.sin_addr.S_un.S_un_b.s_b2;
	//in->s_net = temp.sin_addr.S_un.S_un_b.s_b1;
	//in->s_imp = temp.sin_addr.S_un.S_un_w.s_w2;
	//in->s_impno = temp.sin_addr.S_un.S_un_b.s_b4;
	//in->s_lh = temp.sin_addr.S_un.S_un_b.s_b3;

	return ret;
}


DLL_EXPORT int LDK_get_eth_address(int vp_Id, struct bsd_in_addr* in)
{
	LDK_CORE_WRITEMONITORLOG(0xa000005f, API_LIB_TIMEOUT, NULL, false);
#ifdef WIN32
	vec_in_addr vl_addrList;
	vec_ip_addr vl_ip;
	in->S_un.S_addr = inet_addr("127.0.0.1");
	if (x_asio_socket::GetLocalNetworkList(vl_addrList, vl_ip))
	{
		u32 i;
		for (i = 0; i < vl_addrList.size(); ++i)
		{//取第一个最后一位不是1的. 虚拟网卡都是1
			if (vl_addrList[i].sin_addr.S_un.S_un_b.s_b4 != 1)
			{
				in->S_un.S_addr = vl_addrList[i].sin_addr.S_un.S_addr;
				break;
			}
		}
	}

	return 0;
#else

	char al_Cmd[32];
	x_snprintf(al_Cmd, sizeof(al_Cmd), "eth%d", vp_Id);
	return MDK_get_if_addr(al_Cmd, in);
#endif
}

DLL_EXPORT void MDK_Delay(u32 vl_DelayInMS)
{
	x_thread::X_Sleep(vl_DelayInMS);
}

//锟斤拷LINUX头锟侥硷拷锟饺较革拷锟斤拷. 锟斤拷锟杰伙拷锟揭达拷头锟侥硷拷.
//锟斤拷锟皆硷拷锟絊OCKET锟斤拷睾甓拷锟? 锟斤拷一锟铰伙拷锟斤拷锟斤拷锟斤拷.
#ifndef WIN32
#define BSD_SOCKET_VERIFY_FTR
#endif

#ifdef BSD_SOCKET_VERIFY_FTR

typedef struct
{
	unsigned int v_ldk;
	unsigned int v_linux;
	const char* p_Name;
}t_macro;

#define MC_CHECK_ENTRY(N)	{BSD_##N,N,#N}
#define MC_CHECK_ENTRY2(N,K)	{BSD_##N,K,#N}
const t_macro c_check[] =
{
#if 0
	MC_CHECK_ENTRY(SOCK_STREAM),
	MC_CHECK_ENTRY(SOCK_DGRAM),
	MC_CHECK_ENTRY(SOCK_RAW),

	MC_CHECK_ENTRY(IPPROTO_IP),
	MC_CHECK_ENTRY(IPPROTO_ICMP),
	MC_CHECK_ENTRY(IPPROTO_TCP),
	MC_CHECK_ENTRY(IPPROTO_UDP),

	MC_CHECK_ENTRY(IPPORT_ECHO),
	MC_CHECK_ENTRY(IPPORT_DISCARD),
	MC_CHECK_ENTRY(IPPORT_SYSTAT),
	MC_CHECK_ENTRY(IPPORT_DAYTIME),
	MC_CHECK_ENTRY(IPPORT_NETSTAT),
	MC_CHECK_ENTRY(IPPORT_FTP),
	MC_CHECK_ENTRY(IPPORT_TELNET),
	MC_CHECK_ENTRY(IPPORT_SMTP),
	MC_CHECK_ENTRY(IPPORT_TIMESERVER),
	MC_CHECK_ENTRY(IPPORT_NAMESERVER),
	MC_CHECK_ENTRY(IPPORT_WHOIS),
	MC_CHECK_ENTRY(IPPORT_MTP),

	MC_CHECK_ENTRY(IPPORT_RESERVED),//1024
	// MC_CHECK_ENTRY(TCP_MAX_ACCEPT_BACKLOG	),//128 //NXP:3
	MC_CHECK_ENTRY(MSG_OOB),//0x01	/* process out-of-band data */
	MC_CHECK_ENTRY(MSG_PEEK),//0x02    /* peek at incoming message */
	MC_CHECK_ENTRY(MSG_DONTROUTE),//0x04  /* send without using routing tables */
	// MC_CHECK_ENTRY(MSG_TRYHARD ),//    0x04       /* Synonym for MC_CHECK_ENTRY(MSG_DONTROUTE for DECnet */
	MC_CHECK_ENTRY(MSG_CTRUNC),//0x08
	//  MC_CHECK_ENTRY(MSG_PROBE	),//0x10	/* Do not send. Only probe path f.e. for MTU */
	MC_CHECK_ENTRY(MSG_TRUNC),//0x20
	MC_CHECK_ENTRY(MSG_DONTWAIT),//	0x40	/* Nonblocking io		 */
	MC_CHECK_ENTRY(MSG_EOR),//    0x80	/* End of record */
	MC_CHECK_ENTRY(MSG_WAITALL),//0x100	/* Wait for a full request */
	MC_CHECK_ENTRY(MSG_FIN),//    0x200
	MC_CHECK_ENTRY(MSG_SYN),//0x400
	MC_CHECK_ENTRY(MSG_CONFIRM),//0x800	/* Confirm path validity */
	MC_CHECK_ENTRY(MSG_RST),//0x1000
	MC_CHECK_ENTRY(MSG_ERRQUEUE),//	0x2000	/* Fetch message from error queue */
	MC_CHECK_ENTRY(MSG_NOSIGNAL),//	0x4000	/* Do not generate SIGPIPE */
	MC_CHECK_ENTRY(MSG_MORE),//0x8000	/* Sender will send more */
	// MC_CHECK_ENTRY2(MSG_EOF     ),//    MC_CHECK_ENTRY(MSG_FIN
	MC_CHECK_ENTRY(SOL_IP),//0   //NXP: 1 /* set/get IP per-packet options    */
	MC_CHECK_ENTRY(SOL_SOCKET),//1  //NXP: 0xFFFF /* options for socket level */
	MC_CHECK_ENTRY(SOL_TCP),//6
	// MC_CHECK_ENTRY(SOL_UDP		),//17
	//  MC_CHECK_ENTRY(SOL_IPV6	),//41
	//  MC_CHECK_ENTRY(SOL_ICMPV6	),//58
	//  MC_CHECK_ENTRY(SOL_SCTP	),//132
	//  MC_CHECK_ENTRY(SOL_RAW		),//255
	//  MC_CHECK_ENTRY(SOL_IPX		),//256
	//  MC_CHECK_ENTRY(SOL_AX25	),//257
	//  MC_CHECK_ENTRY(SOL_ATALK	),//258
	//  MC_CHECK_ENTRY(SOL_NETROM	),//259
	//  MC_CHECK_ENTRY(SOL_ROSE	),//260
	//  MC_CHECK_ENTRY(SOL_DECNET	),//261
	//  MC_CHECK_ENTRY(SOL_X25		),//262
	//  MC_CHECK_ENTRY(SOL_PACKET	),//263
	//  MC_CHECK_ENTRY(SOL_ATM		),//264	/* ATM layer (cell level) */
	//  MC_CHECK_ENTRY(SOL_AAL		),//265	/* ATM Adaption Layer (packet level) */
	//  MC_CHECK_ENTRY(SOL_IRDA    ),//    266
	//  MC_CHECK_ENTRY(SOL_NETBEUI	),//267
	//  MC_CHECK_ENTRY(SOL_LLC		),//268
	//  MC_CHECK_ENTRY(SOL_DCCP	),//269
	//  MC_CHECK_ENTRY(SOL_NETLINK	),//270
	MC_CHECK_ENTRY(SO_DEBUG),//1
	MC_CHECK_ENTRY(SO_REUSEADDR),//2
	MC_CHECK_ENTRY(SO_TYPE),//3
	MC_CHECK_ENTRY(SO_ERROR),//4
	MC_CHECK_ENTRY(SO_DONTROUTE),//5
	MC_CHECK_ENTRY(SO_BROADCAST),//6
	MC_CHECK_ENTRY(SO_SNDBUF),//7
	MC_CHECK_ENTRY(SO_RCVBUF),//8
	MC_CHECK_ENTRY(SO_SNDBUFFORCE),//32
	MC_CHECK_ENTRY(SO_RCVBUFFORCE),//33
	MC_CHECK_ENTRY(SO_KEEPALIVE),//	9
	MC_CHECK_ENTRY(SO_OOBINLINE),//	10
	MC_CHECK_ENTRY(SO_NO_CHECK),//11
	MC_CHECK_ENTRY(SO_PRIORITY),//12
	MC_CHECK_ENTRY(SO_LINGER),//13
	MC_CHECK_ENTRY(SO_BSDCOMPAT),//	14
	MC_CHECK_ENTRY(SO_PASSCRED),//16
	MC_CHECK_ENTRY(SO_PEERCRED),//17
	MC_CHECK_ENTRY(SO_RCVLOWAT),//18
	MC_CHECK_ENTRY(SO_SNDLOWAT),//19
	MC_CHECK_ENTRY(SO_RCVTIMEO),//20
	MC_CHECK_ENTRY(SO_SNDTIMEO),//21
	MC_CHECK_ENTRY(SO_SECURITY_AUTHENTICATION),//22
	MC_CHECK_ENTRY(SO_SECURITY_ENCRYPTION_TRANSPORT),//23
	MC_CHECK_ENTRY(SO_SECURITY_ENCRYPTION_NETWORK),//24
	MC_CHECK_ENTRY(SO_BINDTODEVICE),//25
	MC_CHECK_ENTRY(SO_ATTACH_FILTER),//26
	MC_CHECK_ENTRY(SO_DETACH_FILTER),//27
	MC_CHECK_ENTRY(SO_PEERNAME),//28
	MC_CHECK_ENTRY(SO_TIMESTAMP),//29
	MC_CHECK_ENTRY(SO_ACCEPTCONN),//30
	MC_CHECK_ENTRY(SO_PEERSEC),//31
	MC_CHECK_ENTRY(TCP_NODELAY),//1	/* Turn off Nagle's algorithm. */
	MC_CHECK_ENTRY(TCP_MAXSEG),//2	/* Limit MSS */
	MC_CHECK_ENTRY(TCP_CORK),//3	/* Never send partially complete segments */
	MC_CHECK_ENTRY(TCP_KEEPIDLE),//	4	/* Start keeplives after this period */
	MC_CHECK_ENTRY(TCP_KEEPINTVL),//	5	/* Interval between keepalives */
	MC_CHECK_ENTRY(TCP_KEEPCNT),//6	/* Number of keepalives before death */
	MC_CHECK_ENTRY(TCP_SYNCNT),//7	/* Number of SYN retransmits */
	MC_CHECK_ENTRY(TCP_LINGER2),//8	/* Life time of orphaned FIN-WAIT-2 state */
	MC_CHECK_ENTRY(TCP_DEFER_ACCEPT),//	9	/* Wake up listener only when data arrive */
	MC_CHECK_ENTRY(TCP_WINDOW_CLAMP),//	10	/* Bound advertised window */
	MC_CHECK_ENTRY(TCP_INFO),//11	/* Information about this connection. */
	MC_CHECK_ENTRY(TCP_QUICKACK),//	12	/* Block/reenable quick acks */
	//MC_CHECK_ENTRY(TCP_CONGESTION	),//	13	/* Congestion control algorithm */
	MC_CHECK_ENTRY2(SD_RECEIVE,SHUT_RD),//		0x01
	MC_CHECK_ENTRY2(SD_SEND,SHUT_WR),//		0x02
	MC_CHECK_ENTRY2(SD_BOTH,SHUT_RDWR),//		0x03
	MC_CHECK_ENTRY(AF_UNSPEC),//0
	MC_CHECK_ENTRY(AF_UNIX),//1	/* Unix domain sockets 		*/
	MC_CHECK_ENTRY(AF_LOCAL),//1	/* POSIX name for AF_UNIX	*/
	MC_CHECK_ENTRY(AF_INET),//2	/* Internet IP Protocol 	*/

	MC_CHECK_ENTRY(FIONREAD),
	MC_CHECK_ENTRY(FIONBIO),
	MC_CHECK_ENTRY(SIOCATMARK),
	MC_CHECK_ENTRY(SIOCOUTQ),


	MC_CHECK_ENTRY(INADDR_ANY),
	MC_CHECK_ENTRY(INADDR_BROADCAST),
	MC_CHECK_ENTRY(INADDR_NONE),
	MC_CHECK_ENTRY(IN_LOOPBACKNET),
	MC_CHECK_ENTRY(INADDR_LOOPBACK),
#endif

	// add above this line
	{0,0,NULL},
};
#endif


//////////////////////////////////////////////////////////////////////////
///sys/class/net/ppp0
//PPPD
// ~ # pppd call hw_wcdma &
// 	~ # Script /usr/sbin/chat -v -f /etc/ppp/chat/unicom finished (pid 412), status = 0x0
// 	Serial connection established.
// 	using channel 1
// 	Using interface ppp0
// Connect: ppp0 <--> /dev/ttyUSB0
// 		 sent [LCP ConfReq id=0x1 <asyncmap 0x0> <magic 0x62e2e5fa> <pcomp> <accomp>]
// rcvd [LCP ConfReq id=0x0 <asyncmap 0x0> <auth chap MD5> <magic 0xf673f9> <pcomp> <accomp>]
// sent [LCP ConfAck id=0x0 <asyncmap 0x0> <auth chap MD5> <magic 0xf673f9> <pcomp> <accomp>]
// rcvd [LCP ConfAck id=0x1 <asyncmap 0x0> <magic 0x62e2e5fa> <pcomp> <accomp>]
// rcvd [LCP DiscReq id=0x1 magic=0xf673f9]
// rcvd [CHAP Challenge id=0x1 <58a4545d46bdf15fde6fc1f7e4d697bc>, name = "UMTS_CHAP_SRVR"]
// sent [CHAP Response id=0x1 <1495331f741a845dda09bd548269c433>, name = "card"]
// rcvd [CHAP Success id=0x1 ""]
// CHAP authentication succeeded
// 	CHAP authentication succeeded
// 	sent [CCP ConfReq id=0x1 <deflate 15> <deflate(old#) 15> <bsd v1 15>]
// sent [IPCP ConfReq id=0x1 <compress VJ 0f 01> <addr 0.0.0.0> <ms-dns1 0.0.0.0> <ms-dns2 0.0.0.0>]
// rcvd [LCP ProtRej id=0x2 80 fd 01 01 00 0f 1a 04 78 00 18 04 78 00 15 03 2f]
// Protocol-Reject for 'Compression Control Protocol' (0x80fd) received
// 	rcvd [IPCP ConfNak id=0x1 <ms-dns1 10.11.12.13> <ms-dns2 10.11.12.14> <ms-wins 10.11.12.13> <ms-wins 10.11.12.14>]
// sent [IPCP ConfReq id=0x2 <compress VJ 0f 01> <addr 0.0.0.0> <ms-dns1 10.11.12.13> <ms-dns2 10.11.12.14> <ms-wins 10.11.12.13> <ms-wins 10.11.12.14>]
// rcvd [IPCP ConfNak id=0x2 <ms-dns1 10.11.12.13> <ms-dns2 10.11.12.14> <ms-wins 10.11.12.13> <ms-wins 10.11.12.14>]
// sent [IPCP ConfReq id=0x3 <compress VJ 0f 01> <addr 0.0.0.0> <ms-dns1 10.11.12.13> <ms-dns2 10.11.12.14> <ms-wins 10.11.12.13> <ms-wins 10.11.12.14>]
// rcvd [IPCP ConfNak id=0x3 <ms-dns1 10.11.12.13> <ms-dns2 10.11.12.14> <ms-wins 10.11.12.13> <ms-wins 10.11.12.14>]
// sent [IPCP ConfReq id=0x4 <compress VJ 0f 01> <addr 0.0.0.0> <ms-dns1 10.11.12.13> <ms-dns2 10.11.12.14> <ms-wins 10.11.12.13> <ms-wins 10.11.12.14>]
// rcvd [IPCP ConfNak id=0x4 <ms-dns1 10.11.12.13> <ms-dns2 10.11.12.14> <ms-wins 10.11.12.13> <ms-wins 10.11.12.14>]
// sent [IPCP ConfReq id=0x5 <compress VJ 0f 01> <addr 0.0.0.0> <ms-dns1 10.11.12.13> <ms-dns2 10.11.12.14> <ms-wins 10.11.12.13> <ms-wins 10.11.12.14>]
// rcvd [IPCP ConfNak id=0x5 <ms-dns1 10.11.12.13> <ms-dns2 10.11.12.14> <ms-wins 10.11.12.13> <ms-wins 10.11.12.14>]
// sent [IPCP ConfReq id=0x6 <compress VJ 0f 01> <addr 0.0.0.0> <ms-dns1 10.11.12.13> <ms-dns2 10.11.12.14> <ms-wins 10.11.12.13> <ms-wins 10.11.12.14>]
// rcvd [IPCP ConfNak id=0x6 <ms-dns1 10.11.12.13> <ms-dns2 10.11.12.14> <ms-wins 10.11.12.13> <ms-wins 10.11.12.14>]
// sent [IPCP ConfReq id=0x7 <compress VJ 0f 01> <addr 0.0.0.0> <ms-dns1 10.11.12.13> <ms-dns2 10.11.12.14> <ms-wins 10.11.12.13> <ms-wins 10.11.12.14>]
// rcvd [IPCP ConfReq id=0x0]
// sent [IPCP ConfNak id=0x0 <addr 0.0.0.0>]
// rcvd [IPCP ConfRej id=0x7 <compress VJ 0f 01> <ms-dns1 10.11.12.13> <ms-dns2 10.11.12.14> <ms-wins 10.11.12.13> <ms-wins 10.11.12.14>]
// sent [IPCP ConfReq id=0x8 <addr 0.0.0.0>]
// rcvd [IPCP ConfReq id=0x1]
// sent [IPCP ConfAck id=0x1]
// rcvd [IPCP ConfNak id=0x8 <addr 10.106.0.165>]
// sent [IPCP ConfReq id=0x9 <addr 10.106.0.165>]
// rcvd [IPCP ConfAck id=0x9 <addr 10.106.0.165>]
// Could not determine remote IP address: defaulting to 10.64.64.64
// 	not replacing existing default route via 192.168.0.252
// 	local  IP address 10.106.0.165
// 	remote IP address 10.64.64.64
// 	Script /etc/ppp/ip-up started (pid 415)
// 	Script /etc/ppp/ip-up finished (pid 415), status = 0xff

//kill锟斤拷时锟斤拷
// Connect time 13.4 minutes.
// 	Sent 0 bytes, received 0 bytes.
// 	sent [LCP TermReq id=0x2 "User request"]
// rcvd [LCP TermAck id=0x2]
// Connection terminated.
//
// 	[1] + Done(5)                    pppd call hw_wcdma
// /sys/class/net/ppp0/statistics # cat rx_bytes
// 	50
// 	/sys/class/net/ppp0/statistics # cat tx_bytes
// 	77
// /sys/class/net/ppp0/statistics # cat tx_packets
// 6
// /sys/class/net/ppp0/statistics # cat rx_packets
// 5
//锟斤拷锟斤拷锟揭伙拷锟侥拷锟斤拷畲︼拷锟?

DLL_EXPORT int MDK_Dvrr_Is_Ok_Msg(void)
{
	t_LpkUpdateEvent vl_ev;

	memset(&vl_ev, 0x00, sizeof(t_LpkUpdateEvent));
	vl_ev.m_Source = MDK_CORE_OK_ACK;
	vl_ev.m_Magic = LPK_DVRR_MAGIC_NUMBER;
	vl_ev.m_MaxBakSize = MC_GET_APP_TH()->getWatchdogTick(); //看门狗tick
	//printf("====dvrr watchdogtick=%u====\n", vl_ev.m_MaxBakSize);

	return LDK_PostUpdateEvent(&vl_ev, __FILE__, __LINE__, __func__);
}

DLL_EXPORT int MDK_Mdkcore_Is_Ok_Msg(void)
{
    static bool vs_bfirst = false;
	if (!MC_GET_APP_TH())
	{
		return -1;
	}
	t_LpkUpdateEvent vl_ev;

	memset(&vl_ev, 0x00, sizeof(t_LpkUpdateEvent));
	vl_ev.m_Source = MDK_CORE_OK_ACK;
	vl_ev.m_Magic = LPK_UPDATE_MAGIC_NUMBER;
#if 0
	if (!LDK_IsK3C())
	{
		//硬盘版本不使用i2c与epu通讯
		vl_ev.m_MaxBakSize = MC_X_GET_TICK();
	}
	else
#endif
	{
		vl_ev.m_MaxBakSize = MC_GET_APP_TH()->getWatchdogTick(); //看门狗tick
	}
    if(!vs_bfirst)
    {
		X_INFO("====watchdogtick=%u====\n", vl_ev.m_MaxBakSize);
		vs_bfirst = true;
    }

	return LDK_PostUpdateEvent(&vl_ev, __FILE__, __LINE__, __func__);
}

int MDK_Make_Open_Pppd_Msg(t_ModemType vp_modem_type, int UsbInfo_ttyName, const char *pp_apn, const char *pp_user, const char *pp_passwd)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000051, API_LIB_TIMEOUT, NULL, false);
#if !PF_IsHisiLinux
   X_TRACE("modem_type = %d UsbInfo_ttyName=%d apn=%s user=%s passwd=%s", vp_modem_type, UsbInfo_ttyName, pp_apn, pp_user, pp_passwd);
#endif
	t_LpkUpdateEvent* pl_ev;
#define UPDATE_EVENT_MSG_LEN		256
#define UPDATE_EVENT_MSG_SPACE  (UPDATE_EVENT_MSG_LEN- sizeof(t_LpkUpdateEvent) )

	u32 al_Data[UPDATE_EVENT_MSG_LEN / sizeof(u32)];
	int parm_str_size = strlen(pp_apn) + strlen(pp_user) + strlen(pp_passwd);
	int parm_size = 1 + 1 + parm_str_size + 5;
	if ((u32)parm_size > UPDATE_EVENT_MSG_SPACE) {
		perror("pppd parm size too large");
	}
	pl_ev = (t_LpkUpdateEvent*)al_Data;
	memset(al_Data, 0x00, sizeof(al_Data));
	pl_ev->m_Source = OPEN_PPPD;
	pl_ev->m_Magic = LPK_UPDATE_MAGIC_NUMBER;

	char* bufp = ((char*)al_Data) + sizeof(t_LpkUpdateEvent);
	char *p = bufp;

	p += sprintf(p, "%x", vp_modem_type);
	p++;

	p += sprintf(p, "%d", UsbInfo_ttyName);
	p++;

	p += sprintf(p, "%s", pp_apn);
	p++;

	p += sprintf(p, "%s", pp_user);
	++p;

	p += sprintf(p, "%s", pp_passwd);
	++p;
	//memcpy((char *)(&vl_ev.m_MaxBakSize + 1), bufp, parm_str_size + 4);
	pl_ev->m_MaxBakSize = p - bufp;//parm_str_size + 4;

	return LDK_PostUpdateEvent(pl_ev, __FILE__, __LINE__, __func__);
}

DLL_EXPORT MDK_Result MDK_Net_Init(t_ModemType vp_modem_type, int vp_UartID, const char *pp_apn, const char *pp_user, const char *pp_passwd)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000052, API_LIB_TIMEOUT, NULL, false);
	// pppd: In file /etc/ppp/peers/hw_wcdma: unrecognized option '/dev/ttyUSB0'
	// 		  [1] + Done(2)                    pppd call hw_wcdma

	st_ModemType = vp_modem_type;
#define MAX_FILE_SIZE 512
#define YW_PPD_CONFIG_FILE  "/etc/ppp/peers/ywpeer"

	int i;

    if(pp_apn != NULL && pp_user != NULL && pp_passwd != NULL)
		X_PRINT("--- MDK_Net_Init %d Modem_Type:%d Uartid:%d apn=%s usr=%s pw=%s\n", g_ctx.v_UsbInfo.v_Flag, (int)vp_modem_type, vp_UartID, pp_apn, pp_user, pp_passwd);
	else
		X_PRINT("--- MDK_Net_Init %d Modem_Type:%d Uartid:%d\n", g_ctx.v_UsbInfo.v_Flag, (int)vp_modem_type, vp_UartID);

	if (!ldk_is_ttyReady())
	{//不给用
		return -1;
	}

	g_ctx.v_pppUartID = vp_UartID;

	MC_UART_ID_CHECK_RET();//锟斤拷锟斤拷谋锟絭p_UartID

	if (vp_UartID < MAX_UART_COUNT)
	{//锟斤拷锟斤拷:锟斤拷准锟斤拷锟斤拷
		perror("err id");
		return -1;
	}

	int UsbInfo_ttyName = g_ctx.v_UsbInfo.a_ttyName[vp_UartID - MAX_UART_COUNT];

	//printf("--------%s-----Name:%d \n", __func__, UsbInfo_ttyName);

	i = MDK_Make_Open_Pppd_Msg(vp_modem_type, UsbInfo_ttyName, pp_apn, pp_user, pp_passwd);

	if (0 != i)
	{
		perror("exec pppd");
		g_ctx.v_pppUartID = (u32)-1;
	}
	return i;
}

//锟斤拷锟斤拷值:锟斤拷
DLL_EXPORT MDK_Result MDK_Net_UnInit(PPPInstance vp_Inst)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000053, API_LIB_TIMEOUT, NULL, false);
#ifdef WIN32_DUMMY
	g_ctx.v_pppUartID = -1;
	printf("\n -----------------  MDK_Net_UnInit --------------- \n");
	return 0;
#else

#ifdef EP_INTERVAL_DEBUG_FTR
	g_ep_abort_flag = 100;
	printf("\n -----------------  MDK_Net_UnInit --------------- \n");
#endif
	printf("\n MDK_Net_UnInit:killall pppd \n");

#if PF_IsAndroid
	int i = x_system("busybox killall pppd");
#else
	int i = x_system("killall pppd");
#endif

	if (0 != i)
	{
		perror("killall pppd");
	}
	g_ctx.v_pppUartID = (u32)-1;
	return i;
#endif
}

DLL_EXPORT t_LDK_time_t LDK_get_time_t(const u8* pp_BuildDate)
{
	return x_get_time_t(pp_BuildDate);
}

// 锟斤拷帽锟阶际憋拷锟?
void LDK_gmtime(t_LDK_tm * ptm, t_LDK_time_t tt)
{
	x_gmtime((x_tm_t*)ptm, (x_time_t)tt);
}
//////////////////////////////////////////////////////////////////////////

/*
  发送数据到VA
  参数:
  u16 vp_DataType 参见 t_MDK_DataType
  u8 * pp_DataBuff 指向要传送的数据头指针
  u16 vp_DataLen 传送数据字节个数
  返回值:
  0-成功
  非0-失败
*/
DLL_EXPORT int LDK_PostData(u16 vp_DataType, const u8 * pp_DataBuff, u16 vp_DataLen)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000054, API_LIB_TIMEOUT, NULL, false);
	if ((NULL == pp_DataBuff) || (vp_DataLen > POST_DATA_BYTES_MAX))
		return -1;
#ifdef FUNC_USING_API_CALLBACK
	if (c_SVCTbl[0].p_SetPostData(vp_DataType, pp_DataBuff, vp_DataLen))
		return 0;
#else
	if (Dvrr_SetPostData(vp_DataType, pp_DataBuff, vp_DataLen))
		return 0;
#endif

	return -1;
}

/**
* 设置3G守护参数 -- 传输状态自动为开启.
* @param[in] pp_Param	守护参数.
* @return	成功: 0. 成功.
* @return	失败: 错误码
*/
DLL_EXPORT int LDK_SetWatchParam(const t_WatchParam* pp_Param)
{
	LDK_CORE_WRITEMONITORLOG(0xa000005c, API_LIB_TIMEOUT, NULL, false);
	if (NULL != pp_Param)
	{
		int vl_ret;
		t_mmstStart vl_param;
		vl_param.p_P1 = (void *)pp_Param;
		vl_param.p_P2 = 0;

#ifdef FUNC_USING_API_CALLBACK
		vl_ret = c_SVCTbl[0].p_MmstCmd(RTP_CC_SET_WATCHPARAM, (const u8*)&vl_param, sizeof(t_mmstStart), true);
#else
		vl_ret = Rtp_SetWatchParam(vl_param);
#endif
		return vl_ret;
	}
	else
	{
		return -1;
	}
}

static t_TransferSessionID c_mmstSessionID;
DLL_EXPORT int LDK_CreateTransferSession(t_TransferSessionID* pp_SessionID, const t_TransferParam* pp_Param)
{
	// *#143*2*IP1.IP2.IP3.IP4*端口*65535*车牌号*1#

	LDK_CORE_WRITEMONITORLOG(0xa000005d, API_LIB_TIMEOUT, NULL, false);
	if (NULL != pp_Param && NULL != pp_SessionID)
	{
		int vl_ret;
		t_mmstStart vl_param;
		vl_param.p_P1 = pp_SessionID;
		vl_param.p_P2 = (void *)pp_Param;

		t_TransferParam* pTransferParam = (t_TransferParam*)pp_Param;
		if ((NULL != pTransferParam) && (7 == pTransferParam->v_AppProtoType)) // APP_PRO_TYPE_GBV808BB在mmst_session_base.h中定义,这里无法访问,所以直接填7
		{
			// 借用"文件名"字段来标记当前要创建部标协议的信令链路
			ystrcpy((char*)pp_Param->a_username, "va_cmd_link");
		}

		X_INFO("LDK_CreateTransferSession (AppPro=%u,IP=0x%08x,Port=%u) ", pTransferParam->v_AppProtoType, pTransferParam->v_RemoteAddr, pTransferParam->v_RemotePort);

		//printf("!!!! PRE\n")
#ifdef TEMP_ALWAYS_TCP_DBG //test
		((t_TransferParam*)pp_Param)->v_ProtoType = 1;
#endif

#ifdef FUNC_USING_API_CALLBACK
		vl_ret = c_SVCTbl[0].p_MmstCmd(RTP_CC_SET_SOCKET,(const u8*)&vl_param,sizeof(t_mmstStart), true);
#else
		vl_ret = mmst_Cmd(RTP_CC_SET_SOCKET,(const u8*)&vl_param,sizeof(t_mmstStart), true);
#endif
		if (0 == vl_ret)
		{
			c_mmstSessionID = *pp_SessionID;
#ifdef FUNC_USING_API_CALLBACK
			c_SVCTbl[0].p_MmstSetModemType(st_ModemType);
#else
			mmst_set_modem_type(st_ModemType);
#endif
		}
		else
		{
			c_mmstSessionID = 0;
		}
		//printf("!!!! POST %d\n",*pp_SessionID);
		return vl_ret;
	}
	else
	{
		return -1;
	}
}
DLL_EXPORT int LDK_StartTrans(t_TransferSessionID vp_SessionID, t_TrasferCause vp_Cause)
{

	LDK_CORE_WRITEMONITORLOG(0xa000005d, API_LIB_TIMEOUT, NULL, false);
#ifdef FUNC_USING_API_CALLBACK
	return c_SVCTbl[0].p_RtpIoctrl(vp_SessionID,TCC_RESTART,(void*)vp_Cause,NULL);
#else
	return rtp_ioctrl(vp_SessionID,TCC_RESTART,(void*)vp_Cause,NULL);
#endif
}
// 设置带宽流量等控制信息
DLL_EXPORT int LDK_TransferIOCtrl(t_TransferSessionID vp_SessionID, t_TrasferCtrlCode vp_Cmd, void* pp_Code1, void* pp_Code2)
{
	LDK_CORE_WRITEMONITORLOG(0xa000005e, API_LIB_TIMEOUT, NULL, false);
#ifdef FUNC_USING_API_CALLBACK
	return c_SVCTbl[0].p_RtpIoctrl(vp_SessionID,vp_Cmd,pp_Code1,pp_Code2);
#else
	return rtp_ioctrl(vp_SessionID,vp_Cmd,pp_Code1,pp_Code2);
#endif
}
DLL_EXPORT int LDK_StopTrans(t_TransferSessionID vp_SessionID, t_TrasferCause vp_Cause)
{
#ifdef FUNC_USING_API_CALLBACK
	return c_SVCTbl[0].p_RtpIoctrl(vp_SessionID,TCC_STOP,(void*)vp_Cause,NULL);
#else
	return rtp_ioctrl(vp_SessionID,TCC_STOP,(void*)vp_Cause,NULL);
#endif
}

DLL_EXPORT int LDK_PostTransferPendingResult(t_TransferSessionID vp_SessionID, t_TrasferPendingResult vp_Result, u32 vp_Code1, u32 vp_Code2)
{
	///--- 已废弃 2017.06.21
	X_ERR("obsolete PostTransferPendingResult %d %s %d", vp_SessionID, __func__, __LINE__);
	return -1;
}
DLL_EXPORT int LDK_GetTransferSessionInfo(t_TransferSessionID vp_SessionID, t_TransferSessionInfo* pp_Info)
{
#ifdef FUNC_USING_API_CALLBACK
	return c_SVCTbl[0].p_RtpIoctrl(vp_SessionID,TCC_GET_SESSION_INFO,(void*)pp_Info,NULL);
#else
	return rtp_ioctrl(vp_SessionID,TCC_GET_SESSION_INFO,(void*)pp_Info,NULL);
#endif
}

DLL_EXPORT int LDK_KillTransferSession(t_TransferSessionID vp_SessionID)
{
#ifdef FUNC_USING_API_CALLBACK
	int vl_ret = c_SVCTbl[0].p_MmstCmd(RTP_CC_CLEAN_MMST,(const u8*)&vp_SessionID,sizeof(t_TransferSessionID), true);
#else
	int vl_ret = mmst_Cmd(RTP_CC_CLEAN_MMST,(const u8*)&vp_SessionID,sizeof(t_TransferSessionID), true);
#endif
	if (vl_ret ==0)
	{
		c_mmstSessionID = 0;
	}

	return vl_ret;
}

DLL_EXPORT int LDK_SendMediaCommProtocolMsg(u32 vp_SessionIndx, t_MediaProtocolMsgInfo *pp_MsgInfo)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000060, API_LIB_TIMEOUT, NULL, false);
	if (MC_GET_APP_TH())
	{
		return MC_GET_APP_TH()->PostMdkCommProtocolMsg(vp_SessionIndx, pp_MsgInfo);
	}
	return -1;
}

int LDK_ProcMediaCommProtocolMsg(u32 vp_SessionIndx, t_MediaProtocolMsgInfo *pp_MsgInfo)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000061, API_LIB_TIMEOUT, NULL, false);
#ifdef FUNC_USING_API_CALLBACK
	return c_SVCTbl[0].p_MmstProcMediaCommProtocolMsg(vp_SessionIndx, pp_MsgInfo);
#else
    return MMST_ProcMediaCommProtocolMsg(vp_SessionIndx, pp_MsgInfo);
#endif
}


int LDK_ProcMediaParamSet(u32 vp_SessionIndx, t_MediaParamInfo *pp_MediaParamInfo)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000062, API_LIB_TIMEOUT, NULL, false);
#ifdef FUNC_USING_API_CALLBACK
	return c_SVCTbl[0].p_MmstProcMediaParamSet(vp_SessionIndx, pp_MediaParamInfo);
#else
    return MMST_ProcMediaParamSet(vp_SessionIndx, pp_MediaParamInfo);
#endif
}

int LDK_ProcMediaParamQuery(u32 vp_SessionIndx, t_MediaParamInfo *pp_MediaParamInfo)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000063, API_LIB_TIMEOUT, NULL, false);
#ifdef FUNC_USING_API_CALLBACK
	return c_SVCTbl[0].p_MmstProcMediaParamQuery(vp_SessionIndx, pp_MediaParamInfo);
#else
    return MMST_ProcMediaParamQuery(vp_SessionIndx, pp_MediaParamInfo);
#endif
}

#ifdef DXR_ADD_EXT_APP_DATA				// 扩展应用数据分区管理
/**
* 是否存在磁盘扩展存储卡(跨线程调用)
* @return	0       不存在扩展卡.
*           大于0    存在扩展卡
*           小于0    失败
*/
int LDK_IsExistExtdCard()
{
	LDK_CORE_WRITEMONITORLOG(0xa0000064, API_LIB_TIMEOUT, NULL, false);
#ifdef FUNC_USING_API_CALLBACK
	return c_SVCTbl[0].p_IsExistExtdCard();
#else
	return Dvrr_IsExistExtdCard();
#endif
}
/**
* 获取扩展存储有效索引项个数(跨线程调用)(废弃)
* @return	小于0    失败
*           其他	个数
*/
int LDK_GetValidExtdDataIndexItemCnt()
{
	LDK_CORE_WRITEMONITORLOG(0xa0000064, API_LIB_TIMEOUT, NULL, false);
	return -1;
}
/**
* 根据下标获取索引项数据(跨线程调用)(废弃)
* @param[in] idx   索引下标 从0开始
* @param[out] pType 索引项对应类型
* @return	NULL    未找到
*           非空	索引指针
*/
void * LDK_GetValidExtdDataIndexItem(int idx, u32* pType)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000064, API_LIB_TIMEOUT, NULL, false);
	return NULL;
}
/**
* 异步获取索引对应数据内容		//回应 void LDK_OnFinishExtdData(unsigned int context, bool result, void * idata)(跨线程调用)(废弃)
* @param[in] context   索引下标 从0开始
* @param[in] idx   索引下标 从0开始
* @return	无
*/
void LDK_AsyncFetchValidExtdIndexData(unsigned int context, int idx)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000064, API_LIB_TIMEOUT, NULL, false);
}
/**
* 获取指定扩展数据类型有效项个数(跨线程调用)
* @param[out] uType 数据类型
* @return	小于0		失败
*           大于等于0	个数
*/
int LDK_GetValidExtdDataItemCnt(u32 uType)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000064, API_LIB_TIMEOUT, NULL, false);
#ifdef FUNC_USING_API_CALLBACK
	return c_SVCTbl[0].p_GetValidExtdDataItemCnt(uType);
#else
	return Dvrr_GetValidExtdDataItemCnt(uType);
#endif
}
/**
* 异步读取扩展数据内容(跨线程调用)
* @param[out] uType		文件类型，参考【e_AppDataType】
* @param[out] uSeq		文件序号
* @param[out] uOffset	偏移字节数
* @param[out] uLen		读取长度
* @param[out] uContext	上下文
* @param[out] uParam1	参数1
* @param[out] uParam2	参数2
* @param[out] uFlag		标记
* @return	无
* @remark	通过LDK_OnReadExtdDataFinish回调读取数据
*/
void LDK_AsyncReadExtdData(u32 uType, u32 uSeq, u32 uOffset, u32 uLen, u32 uContext, u32 uParam1, u32 uParam2, u32 uFlag)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000064, API_LIB_TIMEOUT, NULL, false);
#ifdef FUNC_USING_API_CALLBACK
	return c_SVCTbl[0].p_AsyncReadExtdData(uType, uSeq, uOffset, uLen, (void*)((long)uContext), uParam1, uParam2, uFlag);
#else
	return Dvrr_AsyncReadExtdData(uType, uSeq, uOffset, uLen, (void*)((long)uContext), uParam1, uParam2, uFlag);
#endif
}
/**
* 准备写操作(跨线程调用)
* @param[out] uType		文件类型，参考【e_AppDataType】
* @param[out] uSeq		文件序号
* @param[out] uLen		读取长度
* @param[out] uContext	上下文
* @param[out] uParam1	参数1
* @param[out] uParam2	参数2
* @param[out] uFlag		标记
* @return	无
* @remark	通过LDK_OnPrepareWriteExtdDataFinish回调读取数据
*/
void LDK_PrepareWriteExtdData(u32 uType, u32 uSeq, u32 uLen, u32 uContext, u32 uParam1, u32 uParam2, u32 uFlag)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000064, API_LIB_TIMEOUT, NULL, false);
#ifdef FUNC_USING_API_CALLBACK
	return c_SVCTbl[0].p_PrepareWriteExtdData(uType, uSeq, uLen, (void*)((long)uContext), uParam1, uParam2, uFlag);
#else
	return Dvrr_PrepareWriteExtdData(uType, uSeq, uLen, (void*)((long)uContext), uParam1, uParam2, uFlag);
#endif
}
/**
* 同步写操作(跨线程调用)
* @param[out] uType		文件类型，参考【e_AppDataType】
* @param[out] uSeq		文件序号
* @param[out] uOffset	偏移字节数
* @param[out] pBuff		数据BUFF
* @param[out] uLen		读取长度
* @return	大于等于0表示成功，小于0表示失败
* @remark	通过LDK_OnPrepareWriteExtdDataFinish回调读取数据
*/
int LDK_WriteExtdData(u32 uType, u32 uSeq, u32 uOffset, const u8* pBuff, u32 uLen)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000064, API_LIB_TIMEOUT, NULL, false);
#ifdef FUNC_USING_API_CALLBACK
	return c_SVCTbl[0].p_WriteExtdData(uType, uSeq, uOffset, pBuff, uLen);
#else
	return Dvrr_WriteExtdData(uType, uSeq, uOffset, pBuff, uLen);
#endif
}
/**
* 将数据同步到磁盘(跨线程调用)
* @param[out] uType		文件类型，参考【e_AppDataType】
* @param[out] uSeq		文件序号
* @param[out] uContext	上下文
* @param[out] uParam1	参数1
* @param[out] uParam2	参数2
* @param[out] uFlag		标记
* @return	无
* @remark	通过LDK_OnFlushExtdDataFinish回调读取数据
*/
void LDK_FlushExtdData(u32 uType, u32 uSeq, u32 uContext, u32 uParam1, u32 uParam2, u32 uFlag)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000064, API_LIB_TIMEOUT, NULL, false);
#ifdef FUNC_USING_API_CALLBACK
	return c_SVCTbl[0].p_FlushExtdData(uType, uSeq, (void*)((long)uContext), uParam1, uParam2, uFlag);
#else
	return Dvrr_FlushExtdData(uType, uSeq, (void*)((long)uContext), uParam1, uParam2, uFlag);
#endif
}
/**
* 异步删除扩展数据(跨线程调用)
* @param[out] uType		文件类型，参考【e_AppDataType】
* @param[out] uSeq		文件序号
* @param[out] uContext	上下文
* @param[out] uParam1	参数1
* @param[out] uParam2	参数2
* @param[out] uFlag		标记
* @return	无
* @remark	通过LDK_OnDeleteExtdDataFinish回调读取数据
*/
void LDK_AsyncDeleteExtdData(u32 uType, u32 uSeq, u32 uContext, u32 uParam1, u32 uParam2, u32 uFlag)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000064, API_LIB_TIMEOUT, NULL, false);
#ifdef FUNC_USING_API_CALLBACK
	return c_SVCTbl[0].p_AsyncDeleteExtdData(uType, uSeq, (void*)((long)uContext), uParam1, uParam2, uFlag);
#else
	return Dvrr_AsyncDeleteExtdData(uType, uSeq, (void*)((long)uContext), uParam1, uParam2, uFlag);
#endif
}
#endif

int LDK_ProcMediaAlarmNotify(t_MediaAlarmNotify *pp_AlarmInfo)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000064, API_LIB_TIMEOUT, NULL, false);
#ifdef FUNC_USING_API_CALLBACK
	return c_SVCTbl[0].p_MmstProcMediaAlarmNotify(pp_AlarmInfo);
#else
    return MMST_ProcMediaAlarmNotify(pp_AlarmInfo);
#endif
}

int LDK_GetLogicChannel(u8 vp_PhysicChl, u8 vp_Type)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000065, API_LIB_TIMEOUT, NULL, false);
#ifdef FUNC_USING_API_CALLBACK
	return c_SVCTbl[0].p_MmstGetLogicChannel(vp_PhysicChl, vp_Type);
#else
    return MMST_GetLogicChannel(vp_PhysicChl, vp_Type);
#endif
}

int LDK_SetAlarmDuration(u32 uAlarmType, u32 uAlarmDuration, u32 uAlarmBeforTime)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000066, API_LIB_TIMEOUT, NULL, false);
#ifdef FUNC_USING_API_CALLBACK
	return c_SVCTbl[0].p_MmstSetAlarmDuration(uAlarmType, uAlarmDuration, uAlarmBeforTime);
#else
    return MMST_SetAlarmDuration(uAlarmType, uAlarmDuration, uAlarmBeforTime);
#endif
}

#define TTS_FTR

//TTS +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
/*
 * Provided interface
 */
 //--------------------INTER
#define MC_TTS_INTER_OP(CMD,PARAM)   MC_GET_MATASK_TH()->TTS_InterOp( (u32)(CMD),(void*)(PARAM),0 )
#define MC_TTS_INTER_OP_INDEX(CMD,PARAM,IDX)  MC_GET_MATASK_TH()->TTS_InterOp( (u32)(CMD),(void*)(PARAM),(void*)IDX )

ivTTSErrID TTS_uninit(void)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000067, API_LIB_TIMEOUT, NULL, false);
	X_FUNC_LN();
	return MC_TTS_INTER_OP(TTS_CMD_UNINIT, NULL);
}

DLL_EXPORT u32 LDK_TTS_GetVersion(void)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000068, API_LIB_TIMEOUT, NULL, false);
	X_FUNC_LN();
	return MC_TTS_INTER_OP(TTS_CMD_GETVERSION, NULL);
}

DLL_EXPORT u32 LDK_TTS_Init(const t_TTS_CreateParam* pp_Param)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000069, API_LIB_TIMEOUT, NULL, false);
	X_FUNC_LN();
	return MC_TTS_INTER_OP(TTS_CMD_INIT, pp_Param);
}
DLL_EXPORT u32 LDK_TTS_UnInit(void)
{
	LDK_CORE_WRITEMONITORLOG(0xa000006a, API_LIB_TIMEOUT, NULL, false);
	X_FUNC_LN();
	return MC_TTS_INTER_OP(TTS_CMD_UNINIT, 0);
}
DLL_EXPORT u32 LDK_TTS_SynthText(void* pp_TextData, u32 vp_SizeOfData)
{
	LDK_CORE_WRITEMONITORLOG(0xa000006b, API_LIB_TIMEOUT, NULL, false);
	X_FUNC_LN();
	//X_TRACE("pp_TextData size=%d, %s", vp_SizeOfData, (char*)pp_TextData);
	//MDK_DumpMemory(pp_TextData,vp_SizeOfData,"tts data");
	return MC_TTS_INTER_OP_INDEX(TTS_CMD_SYNC_TEXT, pp_TextData, vp_SizeOfData);
}
DLL_EXPORT u32 LDK_TTS_SetParam(u32 vp_ParamID, u32 vp_ParamValue)
{
	LDK_CORE_WRITEMONITORLOG(0xa000006c, API_LIB_TIMEOUT, NULL, false);
	X_FUNC_LN();
	//X_TRACE("vp_ParamID=%d,vp_ParamValue=%d", vp_ParamID, vp_ParamValue);
	return MC_TTS_INTER_OP_INDEX(TTS_CMD_SET_PARAM, vp_ParamID, vp_ParamValue);
}
u32 LDK_TTS_GetParam(u32 vp_ParamID, u32* pp_ParamValue)
{
	LDK_CORE_WRITEMONITORLOG(0xa000006d, API_LIB_TIMEOUT, NULL, false);
	X_FUNC_LN();
	return MC_TTS_INTER_OP_INDEX(TTS_CMD_GET_PARAM, vp_ParamID, pp_ParamValue);
}
u32 LDK_TTS_Run(void)
{
	LDK_CORE_WRITEMONITORLOG(0xa000006e, API_LIB_TIMEOUT, NULL, false);
	X_FUNC_LN();
	return MC_TTS_INTER_OP(TTS_CMD_RUN, 0);
}
u32 LDK_TTS_Exit(void)
{
	LDK_CORE_WRITEMONITORLOG(0xa000006f, API_LIB_TIMEOUT, NULL, false);
	X_FUNC_LN();
	return MC_TTS_INTER_OP(TTS_CMD_EXIT, 0);
}

//////////////////////////////////////////////////////////////////////////

u32 LDK_GetLPTCrc(const t_LptHeader* pp_Hdr)
{
	const u8* pl_Data = (const u8*)pp_Hdr;
	int vl_offset = MC_LDK_OFFSETOF_MEMBER(t_LptHeader, v_LptHeadSize);
	return LDK_GetCrc32(pl_Data + vl_offset, sizeof(t_LptHeader) - vl_offset, 0);
}

u32 LDK_IsValidLPTHeader(const t_LptHeader* pp_Hdr)
{
	return memcmp(pp_Hdr->v_TagBeg, LPT_TAGBEG, LPX_TAG_LEN) == 0
		&& LDK_GetLPTCrc(pp_Hdr) == pp_Hdr->v_LptCrc;
}

u32 LDK_GetLPKCrc(const t_LpkHeader* pp_Hdr)
{
	const u8* pl_Data = (const u8*)pp_Hdr;
	int vl_offset = MC_LDK_OFFSETOF_MEMBER(t_LpkHeader, v_LpkVersion);
	return LDK_GetCrc32(pl_Data + vl_offset, pp_Hdr->v_LpkHeadSize - vl_offset, 0);
}

//----------------------------- AUDIO OPERA

#define DATA_CHN_AMR 2
#define DATA_CHN_TTS 3
int prepare_mapp_cmd(u8 vp_Cmd, t_DVRR_Cmd* pl_Cmd, u16 vp_Len)
{
	memset((void*)pl_Cmd, 0, GetDvrrCmdHeadLen() + vp_Len);
	pl_Cmd->pCtx = NULL;
	//pl_Cmd->uTag1 = CMD_TAG1;
	//pl_Cmd->uTag2 = CMD_TAG2;
	pl_Cmd->uCmdID = vp_Cmd;
	pl_Cmd->uLen = vp_Len;
	return GetDvrrCmdHeadLen() + vp_Len;
}

#define HI_G711_HEAD 0X00A00100

DLL_EXPORT int LDK_StartAudioPlay(E_AUDIO_TYPE vp_Type // 锟斤拷频锟斤拷锟斤拷锟斤拷锟
	, void * vp_AudioProp //锟斤拷频锟斤拷锟斤拷
	, const u8* pp_AmrData //锟斤拷锟
	, u32 vp_AmrSize //锟街斤拷锟斤拷
	, u32 vp_Context
)//锟斤拷锟斤拷锟斤拷频
{
	LDK_CORE_WRITEMONITORLOG(0xa0000053, API_LIB_TIMEOUT, NULL, false);
#ifdef FUNC_USING_API_CALLBACK
	adec_thread* pl_th = c_SVCTbl[0].p_GetAdecThread();
#else
	adec_thread* pl_th = MC_GET_AUDIO_TH();
#endif
	X_ASSERT(NULL != pl_th);
	AUDIO_FRAME_TYPE_E vl_type;
	const u8* pl_Data = pp_AmrData;
	u32 vl_size = vp_AmrSize;

	switch (vp_Type)
	{
	case EAT_AMR:
		vl_type = AFT_AMR;
#ifdef _SAVE_TTS_AMR
		//if (pl_Data[0] != 0x3C)
		{
			FILE * fpPcmFile = NULL;
			fpPcmFile = fopen("/res/testamr.amr", "wb");
			if (fpPcmFile)
			{
				const char *pl_amr_head = "#!AMR\n";
				fwrite(pl_amr_head, 1, strlen(pl_amr_head), fpPcmFile);
				fwrite(pl_Data, 1, vl_size, fpPcmFile);
				fclose(fpPcmFile);
			}
		}
#endif
		break;
	case EAT_PCM_MONO:
	{
		//#define _SAVE_TTS_PCM
		vl_type = AFT_PCM;
#ifdef _SAVE_TTS_PCM
		FILE * fpPcmFile = NULL;
		fpPcmFile = fopen("/res/testpcm.pcm", "wb");
		if (fpPcmFile)
		{
			fwrite(pl_Data, 1, vl_size, fpPcmFile);
			fclose(fpPcmFile);
		}
		else
		{
			X_ERR("open file: %s", "/res/testpcm.pcm");
		}
#endif
	}
	break;
	case EAT_G711:
		vl_type = AFT_G711;
		break;
	case EAT_G726:
		vl_type = AFT_G726;
		break;
	default:
		X_ERR("APlay: UNKOWN audio type %d", vp_Type);
		return -1;
	}

	X_TRACE("APlay: InType=%u, OutType=%u, Context=%d, vl_size=%u", vp_Type, vl_type, vp_Context, vl_size);

#ifdef FUNC_USING_API_CALLBACK
	return c_SVCTbl[0].p_PlayAudio(vl_type, vp_AudioProp
		, pl_Data
		, vl_size
		, vp_Context
		, (void*)vp_Type
		, MakeDelegate(MC_GET_APP_TH(), &app_thread::AudioPlayDoneDelegate)
		);
#else
	return pl_th->PlayAudio(vl_type, vp_AudioProp
		, pl_Data
		, vl_size
		, vp_Context
		, (void*)vp_Type
		, MakeDelegate(MC_GET_APP_TH(), &app_thread::AudioPlayDoneDelegate)
		);
#endif
}

DLL_EXPORT int LDK_StopAudioPlay(void)//停止锟斤拷锟节诧拷锟脚碉拷锟斤拷频
{
	LDK_CORE_WRITEMONITORLOG(0xa0000054, API_LIB_TIMEOUT, NULL, false);

#ifdef FUNC_USING_API_CALLBACK
	adec_thread* pl_th = c_SVCTbl[0].p_GetAdecThread();
#else
	adec_thread* pl_th = MC_GET_AUDIO_TH();
#endif

	X_TRACE("-----LDK_StopAudioPlay----");
#ifdef FUNC_USING_API_CALLBACK
	return c_SVCTbl[0].p_StopAudio();
#else
	return pl_th->StopAudio();
#endif
	/*
	t_DVRR_Cmd* pl_Cmd;
	t_DVRR_Cmd al_DvrcCmd = { 0 };
	u32 vl_PackLen =0;

	pl_Cmd = (t_DVRR_Cmd*)&al_DvrcCmd;

	vl_PackLen = prepare_mapp_cmd(PUdp_CMDID_DVRR_ASTOP, pl_Cmd, 0);
	MDK_SVC_Command(&c_SVCTbl[SVC_DVRR], pl_Cmd, vl_PackLen);
	MC_AUDIO_SEM_CONSUME;

	return 0;
	*/
}

void LDK_Tlv320ConfigInterFace(u8 vp_cmd, u8 vp_param1, u8 vp_param2, u8 vp_param3, void* vp_param)
{
	//X_TRACE("----LDK_Tlv320AudioVolumeAdjust: trace_type = %d, vol value = %d", vp_RlChn, vp_VolValue);

	t_DVRR_Cmd* pl_Cmd;
	t_DVRR_Cmd al_DvrcCmd = { 0 };
	u32 vl_PackLen = 0;

	pl_Cmd = (t_DVRR_Cmd*)&al_DvrcCmd;

	vl_PackLen = prepare_mapp_cmd(PUdp_CMDID_DVRR_TLV320_AVOLUME, pl_Cmd, 8);

	pl_Cmd->buff.data[0] = vp_cmd;
	pl_Cmd->buff.data[1] = vp_param1;
	pl_Cmd->buff.data[2] = vp_param2;
	pl_Cmd->buff.data[3] = vp_param3;

	MDK_SVC_Command(&c_SVCTbl[SVC_DVRR], pl_Cmd, vl_PackLen);
	MC_AUDIO_SEM_CONSUME;
}

/*TLV302AIC23B 设置播放音量*/
DLL_EXPORT int LDK_Tlv320AudioVolumeAdjust(e_TLV320_TRACK_TYPE vp_RlChn, u8 vp_VolValue)//停止正在播放的音频
{
	//X_TRACE("----LDK_Tlv320AudioVolumeAdjust: trace_type = %d, vol value = %d", vp_RlChn, vp_VolValue);
	LDK_CORE_WRITEMONITORLOG(0xa0000056, API_LIB_TIMEOUT, NULL, false);

	LDK_Tlv320ConfigInterFace(0x00, (u8)vp_RlChn, vp_VolValue, 0x00, NULL);
	LDK_APlayCodecVolumeSet(vp_VolValue);

	return 0;
}

DLL_EXPORT int LDK_Tlv320TrackPriorityChoose(e_TLV320_TRACK_TYPE vp_RlChn)//停止正在播放的音频
{
	//X_TRACE("----LDK_Tlv320TrackPriorityChoose: trace_type = %d", vp_RlChn);
	LDK_CORE_WRITEMONITORLOG(0xa0000057, API_LIB_TIMEOUT, NULL, false);
	LDK_Tlv320ConfigInterFace(0x01, (u8)vp_RlChn, 0x00, 0x00, NULL);
	LDK_APlayCodecInputChnnSel(ADEV_PLAY_TLV320, 1);

	return 0;
}

int MDK_On_DvrSpecMsg(SVC_HANDLE vp_Handle, void* pp_Msg, u32 vp_Len)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000058, API_LIB_TIMEOUT, NULL, false);
	t_DVRR_Cmd* pl_Cmd = (t_DVRR_Cmd*)pp_Msg;
	int vl_consumed = 0;

	if ((vp_Handle == &c_SVCTbl[SVC_DVRR]))//DVR COMMON MSG
	{
		switch (pl_Cmd->uCmdID)
		{
		case PUdp_CMDID_DVRR_AUDIODONE:
			{
				LDK_OnPlayDone(pl_Cmd->buff.data[0], pl_Cmd->buff.data[1], *((u32*)(pl_Cmd->buff.data + 4)));
			}
			break;
		case PUdp_CMDID_DVRR_MKAV_COLLECT_DONE:
			{
				//MSG_SYS_MEDIA_PACK_DONE,//12. 媒体打包结束.  P1:Context. P2:const t_LDKMediaPackResult*. P3:保留
				t_LDKMediaPackResult vl_pkgResult;
				//DLL_EXPORT int LDK_MkavOnPlayDone(E_MKAV_RETTYPE vp_RetType, E_DVRR_MODE vp_WorkMod, u32 vp_Ch, u32 vp_MkavSize, u32 vp_Context);
				//LDK_MkavOnPlayDone(pl_Cmd->buff.data[0], pl_Cmd->buff.data[1], pl_Cmd->buff.data[2], *((u32*)(pl_Cmd->buff.data+8)), *((u32*)(pl_Cmd->buff.data+4)));
				vl_pkgResult.v_RetType = (E_MKAV_RETTYPE)pl_Cmd->buff.data[0];
				vl_pkgResult.v_WorkMod = (E_DVRR_MODE)pl_Cmd->buff.data[1];
				vl_pkgResult.v_ChMode = pl_Cmd->buff.data[2];
				vl_pkgResult.v_SizePacked = *((u32*)(pl_Cmd->buff.data + 8));

				MDK_On_SysMessage(MSG_SYS_MEDIA_PACK_DONE, (CB_PARAM)(*((u32*)(pl_Cmd->buff.data + 4))), &vl_pkgResult, NULL);
				vl_consumed = 1;
			}
			break;
		default:
			break;
		}
	}

	return 0;
}

/************************************************************************/

DLL_EXPORT int LDK_GetIdevStatus(u8 ap_DevStatus[EAT_MAX_IDEVICE_ID])
{
#ifdef FUNC_USING_API_CALLBACK

#else
    if(ap_DevStatus[0] == 0x80 && ap_DevStatus[1] == 0x80)//�½ӿڶ��巽ʽ
    {
        //BYTE[2]:��ʾоƬ���� BYTE[3]:��ʾоƬ״̬0-��Ч1-��Ч
        Dvrr_GetCurAudioChip(&ap_DevStatus[2], &ap_DevStatus[3]);
    }
	else
	{
		memcpy(ap_DevStatus, ag_I2DevStatus, EAT_MAX_IDEVICE_ID);
	}
#endif
	return 0;
}


/****************************AK4633 PCM DEV OPERA INTERFACE********************************/

#define LDK_STRUCT_INIT(x, y) memset(&x, 0x00, sizeof(y))

DLL_EXPORT int LDK_DEV_AK4633PathConfigure(t_LdkAK4633PathSel vp_PathSel)
{
	LDK_CORE_WRITEMONITORLOG(0xa0000065, API_LIB_TIMEOUT, NULL, false);
	int vl_ret;
	t_PcmDevParam vl_PcmParam;
	LDK_STRUCT_INIT(vl_PcmParam, t_PcmDevParam);

	vl_PcmParam.v_Conf = (unsigned char)vp_PathSel;
	vl_ret = ioctl(g_ctx.v_ak4633Fid, CMD_PCM_CHIP_CONFIG, &vl_PcmParam);
	return vl_ret;
}

/****************************!!!AK4633 PCM DEV OPERA INTERFACE END !!!***************/

#ifdef _WITH_SCRIPT_APP_MODULE
MDK_Result Afm_SendMsgToSa(const u8 *pp_Data, const u32 vp_Size)
{
	if (!MC_GET_SA_TH())
	{
		return -1;
	}

	return MC_GET_SA_TH()->PostMaMsg(pp_Data, vp_Size);
}
#endif

#ifndef ON_FSL_IMX6_ANDROID
#define LIB_MAINVER_SUBVER      "V1.0.1(D/R)"
#ifdef T4_DEBUG
	#define YW_SHARE_LIB_VERSION    "YW_LIBMDKCORE-" LIB_MAINVER_SUBVER "[" __DATE__ "][" __TIME__ "]"
#else
	#define YW_SHARE_LIB_VERSION    "YW_LIBMDKCORE-"LIB_MAINVER_SUBVER"["__DATE__"]["__TIME__"]"
#endif
const char* MDKCORE_GetLibVersion(void)
{
	return YW_SHARE_LIB_VERSION;
}

int Afm_GetLibsVersionString(char* vp_versionInfo, u32 vp_len)
{
    char vl_versionString[LIBS_VERSION_MAX_LEN] = {0};
	x_snprintf(vl_versionString, sizeof(vl_versionString),
			"%s\n%s\n%s\n",
			MDKCORE_GetLibVersion(),
			XPLATFORM_GetLibVersion(),
#ifdef FUNC_USING_API_CALLBACK
			c_SVCTbl[0].p_VaGetLibVersion()
#else
			Va_GetLibVersion()
#endif
	);

	memcpy(vp_versionInfo, vl_versionString, vp_len);

    return 0;
}
#endif

#ifndef FUNC_USING_API_CALLBACK
extern int get_dvrr_build_version(char* pp_Date,char* pp_Time);//IMPL in DVRR
#endif

#define HI_VERSION_TAG_LEN 12

#define HI_TYPEID_LEN	4

#define HI_BUILDTIME_LEN 	24
#define HI_VERSION_INFO_LEN	64
#define HI_RES_LEN	 	(HI_VERSION_INFO_LEN-HI_VERSION_TAG_LEN-HI_BUILDTIME_LEN-HI_TYPEID_LEN*4)

typedef struct HI_VERSION_INFO
{
	u8 	v_VersionTag[HI_VERSION_TAG_LEN];
	u8 	v_TypeId[HI_TYPEID_LEN];
	u32	v_Len;
	u32 v_Version;
	u8	v_BuildTime[HI_BUILDTIME_LEN];
	u32	v_Time;
	u8	v_Res[HI_RES_LEN];
}t_Hi_VerInfo;

const u8 c_mtd0Tag[16] =
{
	0xAA,0x55,0xA5,0x5A,0xA5,0x5A,0x55,0xAA,0x00,0x00,0x00,0x00,0x4D,0x54,0x44,0x30
};

int ldk_find_uboot_ver(t_LDKItemVersion* pp_Ver)
{
	return 0;
}

int ldk_get_file_ver(const char* pp_Fn, t_LDKItemVersion* pp_Ver)
{
	int vl_ret = -1;
	int vl_size;
	t_Hi_VerInfo vl_ver;

#ifdef CRAMFS_ARRIBUTE_SET
	int fd_version = open(pp_Fn, O_RDONLY);
#else
	int fd_version = open(pp_Fn, O_RDWR);
#endif

	if (fd_version < 0)
	{
		perror("open verfile");
	}
	else
	{
		vl_size = read(fd_version, &vl_ver, sizeof(t_Hi_VerInfo));
		//printf("ver file %s version:%x\n",pp_Fn,vl_ver);
		if (vl_size >= 0)//= sizeof(t_Hi_VerInfo))
		{
			pp_Ver->v_Time = LDK_get_build_time_t((const char *)vl_ver.v_BuildTime, (const char *)(vl_ver.v_BuildTime + 12));//vl_ver.v_Time;
			pp_Ver->v_Version = vl_ver.v_Version;
			vl_ret = 0;
		}
		close(fd_version);
	}
	if (0 != vl_ret)
	{
		memset(pp_Ver, 0, sizeof(t_LDKItemVersion));
	}
	return vl_ret;
}

#ifndef _GEMINI_FILE_SYSTEM
#define MTD_FS_VER_CNT  4
static const char* c_verFn[MTD_FS_VER_CNT] =
{
	"/dev/misc/hi_verdev",
	"/VERSION_MTD2_MAIN",
	"/res/VERSION_MTD3_RES",
	"/usr/VERSION_MTD4_APP",
};
#endif
#ifdef _GEMINI_FILE_SYSTEM
DLL_EXPORT long pal_loaded_images_version(
	u32 *version,
	u32 *timestamp,
	u32 *is_shadow
);
#endif
int LDK_GetSysVersionInfo(t_LDKSysVersion* pp_Version)
{
	static t_LDKSysVersion sl_Version;
	static int sl_Done = 0;

	if (NULL != pp_Version && pp_Version->v_SizeOfThis == sizeof(t_LDKSysVersion))
	{
		if (!sl_Done)
		{
			char al_Time[16];
			char al_Date[16];
			//MDK_CORE version
			sl_Version.a_MainVer[LDK_MV_CORE].v_Version =LDK_VERSION;
#ifdef FUNC_USING_API_CALLBACK
			sl_Version.v_HwVersion = c_SVCTbl[0].p_GetDvrrBuildVersion(al_Date, al_Time);
#else
			sl_Version.v_HwVersion = get_dvrr_build_version(al_Date, al_Time);
#endif
			sl_Version.a_MainVer[LDK_MV_CORE].v_Time = LDK_get_build_time_t(al_Date, al_Time);

			sl_Version.a_MainVer[LDK_MV_DVRR].v_Version = LDK_VERSION;// LDK_CORE
			sl_Version.a_MainVer[LDK_MV_DVRR].v_Time = LDK_get_build_time_t(al_Date, al_Time);

			//u-boot --
#ifdef _GEMINI_FILE_SYSTEM
			u32 PartitionVerionArray[EEP_MTD_COUNT];
			u32 PartitionTimeStampArray[EEP_MTD_COUNT];
			u32 is_shadow = 0;
			int vl_ret = 0;

			X_INFO("---------Call LDK_GetSysVersionInfo--------");
			vl_ret = pal_loaded_images_version(PartitionVerionArray, PartitionTimeStampArray, &is_shadow);
			if (vl_ret != e_ok)
			{
				printf("pal_loaded_images_version failed, vl_ret:%d\n", vl_ret);
				return -1;
			}

			if (is_shadow)
			{
				LDK_GetMtdVersion(PartitionVerionArray);
				LDK_GetMtdTimestamp(PartitionTimeStampArray);
			}

			sl_Version.a_MtdVer[MTD_0_LOADER].v_Version = PartitionVerionArray[0]; // E_GEMINI_IMG_BOOTER
			sl_Version.a_MtdVer[MTD_1_KERNEL].v_Version = PartitionVerionArray[1]; // E_GEMINI_IMG_BOOTER
			sl_Version.a_MtdVer[MTD_2_MAIN_FS].v_Version = PartitionVerionArray[2]; // E_GEMINI_IMG_CORER
			sl_Version.a_MtdVer[MTD_3_RES_FS].v_Version = PartitionVerionArray[3]; // E_GEMINI_IMG_APPSET
			sl_Version.a_MtdVer[MTD_4_APP_FS].v_Version = 0;

			sl_Version.a_MtdVer[MTD_0_LOADER].v_Time = PartitionTimeStampArray[0]; // E_GEMINI_IMG_BOOTER
			sl_Version.a_MtdVer[MTD_1_KERNEL].v_Time = PartitionTimeStampArray[1]; // E_GEMINI_IMG_BOOTER
			sl_Version.a_MtdVer[MTD_2_MAIN_FS].v_Time = PartitionTimeStampArray[2]; // E_GEMINI_IMG_CORER
			sl_Version.a_MtdVer[MTD_3_RES_FS].v_Time = PartitionTimeStampArray[3]; // E_GEMINI_IMG_APPSET
			sl_Version.a_MtdVer[MTD_4_APP_FS].v_Time = 0;

			X_TRACE("LDK_GetSysVersionInfo:\nver_loader:0x%X, ver_booter:0x%X, ver_corer:0x%X, ver_appset:0x%X "
				"timestamp_loader:0x%X, timestamp_booter:0x%X, timestamp_corer:0x%X, timestamp_appset:0x%X"
				, PartitionVerionArray[0], PartitionVerionArray[1], PartitionVerionArray[2], PartitionVerionArray[3]
				, PartitionTimeStampArray[0], PartitionTimeStampArray[1], PartitionTimeStampArray[2], PartitionTimeStampArray[3]
			);
#else
			ldk_find_uboot_ver(&sl_Version.a_MtdVer[MTD_0_LOADER]);
			//kernel & FS
			for (int i = 0; i < MTD_FS_VER_CNT; ++i)
			{
				ldk_get_file_ver(c_verFn[i], &sl_Version.a_MtdVer[MTD_1_KERNEL + i]);
			}

			X_TRACE("LDK_GetSysVersionInfo:\nver_kernel:0x%X, ver_mainfs:0x%X, ver_resfs:0x%X, ver_appfs:0x%X "
				"timestamp_kernel:0x%X, timestamp_mainfs:0x%X, timestamp_ver_resfs:0x%X, timestamp_appfs:0x%X"
				, pp_Version->a_MtdVer[MTD_1_KERNEL].v_Version, pp_Version->a_MtdVer[MTD_2_MAIN_FS].v_Version, pp_Version->a_MtdVer[MTD_3_RES_FS].v_Version, pp_Version->a_MtdVer[MTD_4_APP_FS].v_Version
				, pp_Version->a_MtdVer[MTD_1_KERNEL].v_Time, pp_Version->a_MtdVer[MTD_2_MAIN_FS].v_Time, pp_Version->a_MtdVer[MTD_3_RES_FS].v_Time, pp_Version->a_MtdVer[MTD_4_APP_FS].v_Time
			);
#endif
			sl_Done = 1;
		}
		memcpy((u8 *)pp_Version, (u8 *)&sl_Version, sizeof(t_LDKSysVersion));

		return 0;
	}
	else
	{
		X_TRACE("-----%s-----ParamErr-", __func__);
		return -1;
	}
}


/*****************************YW MEM RW INTERFACE**************************************/

enum MEM_ACCESS_CELL
{
	CELL_BYTE,
	CELL_SHORT,
	CELL_DWORD,

	CELL_MAX = 0X07,
};

enum MEM_ACCESS_CMD
{
	CMD_MEM_READ,
	CMD_MEM_WRITE,
};

enum MEM_ADDR_TYPE
{
	ADDR_LOG_TYPE,
	ADDR_PHY_TYPE,
};

#define GET_ACCESS_CELL(X)	((X)&(CELL_MAX))
#define GET_ACCESS_CMD(X)	(((X)&(1<<6))>>6)
#define GET_ACCESS_ADDR_TYPE(X)	(((X)&(1<<7))>>7)
//
// static int DLK_MemLogicOpera(int vp_mod, unsigned int vp_addr, unsigned int * pp_data, int vp_count)
// {
// 	int vl_ret = 0;
// 	int vl_i;
// 	int vl_len = vp_count;
// 	unsigned char *pl_data = (unsigned char *)vp_addr;
// 	unsigned char *pl_indata = (unsigned char *)pp_data;
//
// 	int vl_CellType = GET_ACCESS_CELL(vp_mod);
// 	int vl_cmd = GET_ACCESS_CMD(vp_mod);
//
// 	switch (vl_CellType)
// 	{
// 		case CELL_BYTE:
// 			break;
//
// 		case CELL_SHORT:
// 			vl_len <<= 1;
// 			break;
//
// 		case CELL_DWORD:
// 			vl_len <<= 2;
// 			break;
//
// 		default:
// 			printf("%s UNKOWN CMD:%d \n", __func__, vl_CellType);
// 			vl_ret = -1;
// 			break;
// 	}
//
// 	if (0 == vl_ret)
// 	{
// 		if (vl_cmd == CMD_MEM_WRITE)
// 		{
// 			for (vl_i=0; vl_i<vl_len; vl_i++)
// 			{
// 				pl_data[vl_i] = *pl_indata;
// 				pl_indata++;
// 			}
// 		}
// 		else
// 		{
// 			for (vl_i=0; vl_i<vl_len; vl_i++)
// 			{
// 				pl_indata[vl_i] = *pl_data;
// 				pl_data++;
// 			}
// 		}
// 	}
// 	return vl_ret;
// }
///////////////////////////////////////////////////////////////////

#if 0
BIT0 - BIT2  0  BYTE
1  SHORT
2  DWORD
BIT6       0  READ
1  WRITE
BIT7       0  LOG ADDR
1  PH  ADDR
< 0  err code
>0  bytes
#endif

DLL_EXPORT int LDK_MediaPackCtrl(E_MKAV_CMD vp_Cmd, E_MKAV_TYPE vp_AvType
	, u32 vp_Ch
	, u32 vp_Timeout
	, u8* pp_Data
	, u32 vp_BuffSize
	, u32 vp_Context
	, const t_MkavVedioParam * pp_VedioParam
)//
{
	t_DVRR_Cmd* pl_Cmd;
	t_DVRR_Cmd al_DvrcCmd = { 0 };
	u32 vl_addr = (u32)pp_Data;
	u32 vl_size = vp_BuffSize;
	u32 vl_PackLen = 0;

	if (vp_Cmd == EAT_MKAV_START)
	{
		if (vl_size == 0 || NULL == pp_Data || (vp_AvType == EAT_MKAV_VIDEO && NULL == pp_VedioParam))
		{
			MDK_LOG("ERR: %s Cmd:%d Type:%d Param: Size:%d, Addr:%p VideoParam:%p \n", __func__, vp_Cmd, vp_AvType, vp_BuffSize, pp_Data, pp_VedioParam);
			return -1;
		}
	}

	pl_Cmd = (t_DVRR_Cmd*)&al_DvrcCmd;
	vl_PackLen = prepare_mapp_cmd(PUdp_CMDID_DVRR_MKAV_CHECK_MOD, pl_Cmd, 30);

	pl_Cmd->buff.data[0] = (u8)vp_Cmd;//AdecType 1:PT_AMR 2:G711

	if (vp_Cmd == EAT_MKAV_START)
	{
		pl_Cmd->buff.data[1] = (u8)vp_AvType;
		pl_Cmd->buff.data[2] = vp_Ch;
		pl_Cmd->buff.data[3] = vp_Timeout;

		memcpy(&pl_Cmd->buff.data[4], &vl_addr, sizeof(int));
		memcpy(&pl_Cmd->buff.data[8], &vl_size, sizeof(int));

		if ((vp_AvType == EAT_MKAV_VIDEO) && (NULL != pp_VedioParam))
		{
			t_MkavVedioParam *pl_Video = (t_MkavVedioParam *)pp_VedioParam;

			memcpy(&pl_Cmd->buff.data[16], &pl_Video->v_BitRate, sizeof(u32));
			pl_Cmd->buff.data[20] = pl_Video->v_TargetRate;
		}
	}

	memcpy(&pl_Cmd->buff.data[12], &vp_Context, sizeof(u32));


	MDK_SVC_Command(&c_SVCTbl[SVC_DVRR], pl_Cmd, vl_PackLen);

	return 0;
}



/*
三个方案:
1. 增加一个常驻线程: 在线程中调用系统的API, 使用MQ接收请求. 不使用自己做的这个(不放心啊), 似乎是网上找来改的.
2. 不增加线程: 仍在现有的MA线程中, 使用epoll + 自己实现的这个 API, 用非BLOCKING的方式实现.  这个方案潜在风险是这个自己做的API 某种情况下出问题.
3. 增加一个临时线程: 在临时线程中不用MQ.  调用系统API, BLOCKING方式完成任务后, 发消息到APP线程. 然后线程结束.
已读
朋森
方案4: 在APP线程中, 采用 EPOLL 方式,, 非BLOCKING方式, 使用自己的API来实现.

我的建议是方案3, 先快速解决问题, 避免MQ问题与自己实现的潜在问题.  更优方案以后再改造.

张总同意用方案3. 我来改一下,先解决问题
*/
int LDK_GetThreadId()
{
#ifdef _MSC_VER
	return (int)GetCurrentThreadId();

#else
	return (int)syscall(__NR_gettid);
#endif
}
#define FUNC_NEW_DNR_FTR

int LDK_GetIpAddress(u32 vp_Context, const char* pp_Domain)
{
	return LDK_GetIpAddressEx(vp_Context, pp_Domain, 0);
	//SEND MSG TO GET IP THREAD
	//if (!MC_GET_MATASK_TH())
	//{
	//	return -1;
	//}

	//return X_GetIpAddress(vp_Context, pp_Domain, MC_GET_MATASK_TH(), NULL);
}
//通过指定域名服务器获取域名解析服务 -- 假定是在 APP
int LDK_GetIpAddressEx(u32 vp_Context, const char* pp_Domain, u32 vp_DomainServer)
{
#ifdef FUNC_NEW_DNR_FTR
	//AppFormsSpace::Afw_MaDataMsg 在DVRR线程
	int errCode = -1;
 	app_thread* pl_AppTh = MC_GET_APP_TH();
	//下面的判断似乎不可行

	if (NULL != pl_AppTh)
	{//app 线程调过来的
		int vl_CurTid = LDK_GetThreadId();
		X_TRACE("vl_CurTid %d vs pl_AppTh->GetThreadId() %d", vl_CurTid, pl_AppTh->GetThreadId());
		dnr_thread* pl_dnrTh = MC_GET_DNR_TH();
		if (NULL != pl_dnrTh)
		{
			pl_dnrTh->DoMAWork(vp_Context, pp_Domain, NULL, vp_DomainServer,pl_AppTh);
			errCode = 0;
		}
	}

	return errCode;
#else
	//SEND MSG TO GET IP THREAD
	if (!MC_GET_MATASK_TH())
	{
		return -1;
	}
    X_TRACE("ct=%d domain=%s ds=0x%08x", vp_Context, pp_Domain, vp_DomainServer);
	return MC_GET_MATASK_TH()->GetIpAddress(MC_GET_APP_TH(), vp_Context, pp_Domain, MC_GET_MATASK_TH(), NULL, vp_DomainServer);
#endif
}


DLL_EXPORT int X_GetIpAddress(u32 vp_Context, const char* pp_Domain, void* pCaller, x_MaOnDNRResult_Delegate OnDNRResult)
{
#ifdef FUNC_NEW_DNR_FTR
	//只有 CBmWatchCtrl 中有调用, 委托方式 汤工说是在DVRR运行时中
	int errCode = -1;
	dnr_thread* pl_dnrTh = MC_GET_DNR_TH();
	if (NULL != pl_dnrTh)
	{
		X_TRACE("DNR for VA on going");
		errCode = pl_dnrTh->DoVAWork(vp_Context, pp_Domain, pCaller, 0, OnDNRResult);
	}

	return errCode;

#else
	//SEND MSG TO GET IP THREAD
	if (!MC_GET_MATASK_TH())
	{
		return -1;
	}

	return MC_GET_MATASK_TH()->GetIpAddress(MC_GET_APP_TH(), vp_Context, pp_Domain, pCaller, OnDNRResult);
#endif
}

// 执行标准IPC同步处理业务
DLL_EXPORT int X_DealIpcJob(u32 vp_Context, int jobType, void* pCaller, x_onipcjobmsg_Delegate OnIpcJob)
{
	//SEND MSG TO DEAL IPC JOB THREAD
	if (!MC_GET_MATASK_TH())
	{
		return -1;
	}

	return MC_GET_MATASK_TH()->DealIpcJob(MC_GET_APP_TH(), vp_Context, jobType, pCaller, OnIpcJob);
}

//执行脚本文件，因为脚本文件都有睡眠等待，需要单独抽离出来
DLL_EXPORT int X_DealScript(u32 vp_Context, int vp_ScriptType, void *pCaller, x_ScriptJob_Delegate OnScriptJob)
{
	//SEND MSG TO DEAL SCRIPT JOB THREAD
	if (!MC_GET_MATASK_TH())
	{
		return -1;
	}

	return MC_GET_MATASK_TH()->DealScript(MC_GET_APP_TH(), vp_Context, vp_ScriptType, MC_GET_MATASK_TH(), OnScriptJob);
}

#ifdef FUNC_MOD_ASYNC_MERGE_SEG
// 执行查询耗时业务
DLL_EXPORT int X_DealQueryJob(u32 vp_Context, void* pCaller, x_onqueryjobmsg_Delegate OnQueryJob)
{
	//SEND MSG TO DEAL QUERY JOB THREAD
	if (!MC_GET_MATASK_TH())
	{
		return -1;
	}

	return MC_GET_MATASK_TH()->DealQueryJob(MC_GET_APP_TH(), vp_Context, pCaller, OnQueryJob);
}
#endif

///////////////////////////////////////////////////////////////////////////////////////

DLL_EXPORT u16 MDK_Gb2Unicode(u16 gbkCode)
{
	return X_Gb2Unicode(gbkCode);
}

DLL_EXPORT u16 MDK_Unicode2Gb(u16 ucs2Code)
{
	return X_Unicode2Gb(ucs2Code);
}

////////////////////////////////////////////////////////////////////////////////////////
void CONVERT_PIC_EVENTMASK_TO_STATUSMASK(u32 vp_event_mask, u32 pp_PicStatus[])
{
	if (vp_event_mask & MEIDA_EVENT_CENTER_CMD)
	{
		pp_PicStatus[0] |= BIT0;
	}
	if (vp_event_mask & MEIDA_EVENT_TYPE_SOS)
	{
		pp_PicStatus[0] |= BIT1;
	}
	if (vp_event_mask & MEIDA_EVENT_TYPE_LOAD)
	{
		pp_PicStatus[0] |= BIT4;
	}
	if (vp_event_mask & MEIDA_EVENT_JUDGE)
	{
		pp_PicStatus[1] |= 0x80000000;//BIT63
	}
}

#ifdef FUNC_FLOW_AND_MEDIA_11
DLL_EXPORT Handle LDK_QueryMediaEx(TQueryMediaParam vp_queryparam, LDK_OnMediaListEx_Cb vp_onMediaList_Callback, Handle vp_handle)
{
	X_INFO("LDK_QueryMediaEx: in (Mode=%u,Fmt=%u,ChnlMask=0x%x,Bgn=%u,End=%u,Media=%u,Event=%u)", vp_queryparam.vp_mode, vp_queryparam.vp_format, vp_queryparam.vp_chlmask, vp_queryparam.vp_bgntime, vp_queryparam.vp_endtime, vp_queryparam.vp_type, vp_queryparam.vp_eventmask);

	if (NULL == MC_GET_APP_TH())
		return -1;

	u32 pl_PicStatus[4] = { 0 };
	if (DXR_MDMASK_PICTURE == vp_queryparam.vp_type)
	{
		CONVERT_PIC_EVENTMASK_TO_STATUSMASK(vp_queryparam.vp_eventmask, pl_PicStatus);
		vp_queryparam.vp_eventmask = 0;
	}

	return MC_GET_APP_TH()->QueryMediaEx(vp_queryparam, vp_onMediaList_Callback, vp_handle, vp_queryparam.vp_mode, pl_PicStatus);
}

DLL_EXPORT MDK_Result LDK_ExportMedia(TExportMediaParam vp_exportparam, LDK_OnMediaExportFinished_Cb vp_onMediaFinished_Callback, Handle vp_handle)
{
	// 字符串必须保证末字节为0,否则会导致打印越界
	vp_exportparam.vp_fullpath[sizeof(vp_exportparam.vp_fullpath) - 1] = '\0';
	X_INFO("LDK_ExportMedia: in (ChnlMask=%u,Bgn=%u,End=%u,Media=%u,Event=0x%x,Format=%u,PathType=%u,Path=%s)", vp_exportparam.vp_chlmask, vp_exportparam.vp_bgntime, vp_exportparam.vp_endtime, vp_exportparam.vp_type, vp_exportparam.vp_eventmask, vp_exportparam.vp_format, vp_exportparam.vp_pathtype, vp_exportparam.vp_fullpath);
	if (NULL == MC_GET_APP_TH())
		return -1;

	return MC_GET_APP_TH()->ExportMedia(vp_exportparam, vp_onMediaFinished_Callback, vp_handle);
}
DLL_EXPORT MDK_Result LDK_StopExportMedia(Handle vp_handle)
{
	X_INFO("LDK_StopExportMedia: in (Handle=%u)", vp_handle);
	return MC_GET_APP_TH()->StopExportMedia(vp_handle);
}
#endif // FUNC_FLOW_AND_MEDIA_11


//媒体查询接口
//参数:
//int vp_type,      媒体类型掩码 :DXR_MDMASK_XX
//int vp_bgntime,   媒体开始时间2000年开始的格林威治时间秒数
//int vp_endtime,   媒体结束时间2000年开始的格林威治时间秒数
//int vp_eventmask, 事件掩码
//int vp_chl，      媒体通道,从0开始
//返回值:
//Handle    查询句柄id
DLL_EXPORT Handle LDK_QueryMedia(u8 vp_type, u32 vp_bgntime, u32 vp_endtime, u32 vp_eventmask, u8 vp_chl, LDK_OnMediaList_Cb vp_onMediaList_Callback, Handle vp_handle)
{
	if (MC_GET_APP_TH())
	{
		u32 pl_PicStatus[4] = { 0 };
		if (DXR_MDMASK_PICTURE == vp_type)
		{
			CONVERT_PIC_EVENTMASK_TO_STATUSMASK(vp_eventmask, pl_PicStatus);
			vp_eventmask = 0;
		}

		return MC_GET_APP_TH()->QueryMedia(vp_type, vp_bgntime, vp_endtime, vp_eventmask, vp_chl, vp_onMediaList_Callback, vp_handle, E_APP_MEDIA_QUERY_NORMAL, pl_PicStatus);
	}

	return -1;
}

//停止媒体查询接口
DLL_EXPORT MDK_Result LDK_StopQueryMedia(Handle vp_queryHandle)
{
	if (MC_GET_APP_TH())
	{
		return MC_GET_APP_TH()->StopQueryMedia(vp_queryHandle) ? 0 : 1;
	}
	return -1;
}

//媒体数据提取函数
DLL_EXPORT MDK_Result LDK_GetMedia(int vp_mediaId, int vp_offset, LDK_OnMediaData_Cb vp_onMediaData_Callback, LDK_OnMediaFinished_Cb vp_onMediaFinished_Callback)
{
	if (MC_GET_APP_TH())
	{
		return MC_GET_APP_TH()->GetMedia(vp_mediaId, vp_offset, vp_onMediaData_Callback, vp_onMediaFinished_Callback) ? 0 : 1;
	}
	return -1;
}

//媒体数据提取暂停函数
DLL_EXPORT MDK_Result LDK_PauseMedia(int vp_mediaId)
{
	if (MC_GET_APP_TH())
	{
		return MC_GET_APP_TH()->PauseMedia(vp_mediaId) ? 0 : 1;
	}
	return -1;
}

//媒体数据提取恢复函数
DLL_EXPORT MDK_Result LDK_ResumeMedia(int vp_mediaId)
{
	if (MC_GET_APP_TH())
	{
		return MC_GET_APP_TH()->ResumeMedia(vp_mediaId) ? 0 : 1;
	}
	return -1;
}

//媒体数据提取停止函数
DLL_EXPORT MDK_Result LDK_StopMedia(int vp_mediaId)
{
	if (MC_GET_APP_TH())
	{
		return MC_GET_APP_TH()->StopMedia(vp_mediaId) ? 0 : 1;
	}
	return -1;
}

//根据媒体ID获取媒体信息
DLL_EXPORT MDK_Result LDK_GetMediaInfo(int vp_mediaId, t_MediaList_Fetch *pp_MediaInfo)
{
	if (MC_GET_APP_TH())
	{
		return MC_GET_APP_TH()->GetMediaInfo(vp_mediaId, pp_MediaInfo) ? 0 : 1;
	}
	return -1;
}

//获取媒体数据
DLL_EXPORT MDK_Result LDK_FetchMedia(t_MediaListEx_Fetch *pp_FetchParam, LDK_OnMediaData_Cb vp_onMediaData_Callback, LDK_OnMediaFinished_Cb vp_onMediaFinished_Callback)
{
	if (NULL == pp_FetchParam)
		return -1;

	X_INFO("LDK_FetchMedia: in (ChnlMask=0x%02x,Bgn=%u,End=%u,Media=%u,Event=%u,Size=%u,MediaID=%u,Fmt=%u)", pp_FetchParam->usChannel, pp_FetchParam->uBeginTime, pp_FetchParam->uEndTime, pp_FetchParam->usType, pp_FetchParam->uEventMask, pp_FetchParam->uSize, pp_FetchParam->uMediaId, pp_FetchParam->uFormat);
	if (MC_GET_APP_TH())
	{
		return MC_GET_APP_TH()->FetchMedia(pp_FetchParam, vp_onMediaData_Callback, vp_onMediaFinished_Callback) ? 0 : 1;
	}

	return -1;
}

//配置wifi相关参数
/**#143*9*APN*200* uartlog
pp_data: 输入参数　配置参数串与串口协议体相同
vp_length: 输入参数　配置串长度
vp_version:输入参数　版本号　默认０
bDel: 输入参数　是否删除
*/
DLL_EXPORT MDK_Result LDK_WifiConfig(const u8 *pp_data, int vp_length, int vp_version, int bDel)
{
	if (MC_GET_APP_TH())
	{
		bool vl_bDel = false;
		if (bDel > 0)
		{
			vl_bDel = true;
		}
		return MC_GET_APP_TH()->WifiConfigCmd(pp_data, vp_length, vp_version, vl_bDel);
	}
	return -1;
}



/////////////////////////////////////////////////////////////////////////////////////////////

#if defined(ON_FSL_IMX6) || defined(ON_FSL_IMX6_ANDROID)
static x_imx_lcd* sp_imx_lcd = NULL;
#endif

// LCD初始化
DLL_EXPORT MDK_Result LDK_Lcd_Init()
{
	int vl_ret = 0;

	app_thread* pl_app_th = MC_GET_APP_TH();
	if (NULL == pl_app_th)
	{
	    X_ERROR("pl_app_th=%p", pl_app_th);
		return -1;
	}

	// IMX6才能操作
#if defined(ON_FSL_IMX6) || defined(ON_FSL_IMX6_ANDROID)
	if (NULL == sp_imx_lcd)
	{
		sp_imx_lcd = x_new x_imx_lcd(LCD_DEVICE_NAME, pl_app_th->m_pGPIO, 132, 64);
		X_ASSERT(NULL != sp_imx_lcd);
	}

	if (sp_imx_lcd->Open())
	{
		vl_ret = sp_imx_lcd->Lcd_Init();
	}
	else
	{
		X_TRACE("open x_imx_lcd failed");
		vl_ret = -1;
	}
	X_TRACE("call LDK_Lcd_Init ok");

#endif

	return vl_ret;
}

DLL_EXPORT MDK_Result LDK_Lcd_Init_Ex(void *vp_pGpio)
{
	int vl_ret = 0;

	if (NULL == vp_pGpio)
	{
	    X_ERROR("vp_pGpio=%p", vp_pGpio);
		return -1;
	}

	// IMX6才能操作
#if defined(ON_FSL_IMX6) || defined(ON_FSL_IMX6_ANDROID)
	if (NULL == sp_imx_lcd)
	{
		sp_imx_lcd = x_new x_imx_lcd(LCD_DEVICE_NAME, (x_asio_gpio*)vp_pGpio, 132, 64);
		X_ASSERT(NULL != sp_imx_lcd);
	}

	if (sp_imx_lcd->Open())
	{
		vl_ret = sp_imx_lcd->Lcd_Init();
	}
	else
	{
		X_TRACE("open x_imx_lcd failed");
		vl_ret = -1;
	}
	X_TRACE("call LDK_Lcd_Init ok");

#endif

	return vl_ret;
}


// LCD反初始化
DLL_EXPORT MDK_Result LDK_Lcd_Uninit()
{
	int vl_ret = 0;
#if defined(ON_FSL_IMX6) || defined(ON_FSL_IMX6_ANDROID)
	if (NULL != sp_imx_lcd)
	{
		sp_imx_lcd->Lcd_Unint();
		sp_imx_lcd->Close();
		delete sp_imx_lcd;
		sp_imx_lcd = NULL;
	}
#endif

	return vl_ret;
}

// LCD画矩形
DLL_EXPORT MDK_Result LDK_Lcd_DrawRect(u32 x, u32 y, u32 wid, u32 high, u8* pp_color, int vp_show)
{
	int vl_ret = -1;
#if defined(ON_FSL_IMX6) || defined(ON_FSL_IMX6_ANDROID)
	if (NULL != sp_imx_lcd)
	{
		vl_ret = sp_imx_lcd->Lcd_DrawRect(x, y, wid, high, pp_color, vp_show != 0);
	}
#endif

	return vl_ret;
}

// LCD画区域
DLL_EXPORT MDK_Result LDK_Lcd_DrawRegon(u32 x, u32 y, u32 wid, u32 high, u8* pp_data, int vp_len, int vp_show)
{
	int vl_ret = -1;
#if defined(ON_FSL_IMX6) || defined(ON_FSL_IMX6_ANDROID)
	if (NULL != sp_imx_lcd)
	{
		vl_ret = sp_imx_lcd->Lcd_DrawRegon(x, y, wid, high, pp_data, vp_len, vp_show != 0);
	}
#endif

	return vl_ret;
}

// LCD区域反色
DLL_EXPORT MDK_Result LDK_Lcd_DrawRegonXor(u32 x, u32 y, u32 wid, u32 high, int vp_show)
{
	int vl_ret = -1;
#if defined(ON_FSL_IMX6) || defined(ON_FSL_IMX6_ANDROID)
	if (NULL != sp_imx_lcd)
	{
		vl_ret = sp_imx_lcd->Lcd_DrawRegonXor(x, y, wid, high, vp_show != 0);
	}
#endif

	return vl_ret;
}

// LCD区域刷新
DLL_EXPORT MDK_Result LDK_Lcd_InvalidRegon(u32 x, u32 y, u32 wid, u32 high, int vp_show)
{
	int vl_ret = -1;
#if defined(ON_FSL_IMX6) || defined(ON_FSL_IMX6_ANDROID)
	if (NULL != sp_imx_lcd)
	{
		vl_ret = sp_imx_lcd->Lcd_InvalidRegon(x, y, wid, high, vp_show != 0);
	}
#endif

	return vl_ret;
}

// LCD整屏刷新
DLL_EXPORT MDK_Result LDK_Lcd_InvalideWholeScreen(int vp_show)
{
	int vl_ret = -1;
#if defined(ON_FSL_IMX6) || defined(ON_FSL_IMX6_ANDROID)
	if (NULL != sp_imx_lcd)
	{
		vl_ret = sp_imx_lcd->Lcd_InvalideWholeScreen(vp_show != 0);
	}
#endif

	return vl_ret;
}

// LCD进入睡眠
DLL_EXPORT void LDK_Lcd_IntoSleep()
{
#if defined(ON_FSL_IMX6) || defined(ON_FSL_IMX6_ANDROID)
	if (NULL != sp_imx_lcd)
	{
		sp_imx_lcd->Lcd_IntoSleep();
	}
#endif
}

// LCD退出睡眠
DLL_EXPORT void LDK_Lcd_ExitSleep()
{
#if defined(ON_FSL_IMX6) || defined(ON_FSL_IMX6_ANDROID)
	if (NULL != sp_imx_lcd)
	{
		sp_imx_lcd->Lcd_ExitSleep();
	}
#endif
}

// LCD是否允许睡眠
DLL_EXPORT int LDK_Lcd_IsAllowSleep()
{
	int vl_ret = -1;
#if defined(ON_FSL_IMX6) || defined(ON_FSL_IMX6_ANDROID)
	if (NULL != sp_imx_lcd)
	{
		vl_ret = sp_imx_lcd->Lcd_IsAllowSleep();
	}
#endif

	return vl_ret;
}

// LCD背光控制。uLevelRatio表示电平比率，100=全高，0=全低，30=%30为高电平
DLL_EXPORT void LDK_Lcd_CtrlBkLgt(u32 vp_LevelRatio)
{
#if defined(ON_FSL_IMX6) || defined(ON_FSL_IMX6_ANDROID)
	if (NULL != sp_imx_lcd)
	{
		sp_imx_lcd->Lcd_CtrlBkLgt(vp_LevelRatio);
	}
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////////

// 获得图标的点阵信息
/**************************************************
输入参数：
	byte icon 图标编号
输出参数：
	byte* pBuffer 字符点阵
返回值：
	读取的点阵字节数
**************************************************/
DLL_EXPORT u32 LDK_GetIconLat(u8 icon, u8* pBuffer)
{
	u32 vl_ret = 0;

	if (NULL != pBuffer)
	{
		vl_ret = CLat_GetIconLat(icon, pBuffer);
	}
	return vl_ret;
}

// 获得文字的点阵信息
/*************************************************
输入参数：
	byte wordtype 文字类型 0-ascii 1-汉字 2-俄文
	byte latseq   点阵编号
	u16 lpc       字符值
输出参数：
	byte* pBuffer 字符点阵
返回值：
	读取的点阵字节数
*************************************************/
DLL_EXPORT u32 LDK_GetWordLat(u8 wordtype, u8 latseq, u16 lpc, u8* pBuffer)
{
	u32 vl_ret = 0;

	if (NULL != pBuffer)
	{
		vl_ret = CLat_GetWordLat(wordtype, latseq, lpc, pBuffer);
	}
	return vl_ret;
}

// 喊话与通话切换
DLL_EXPORT s32 LDK_AudioHprFuncSelect(t_LdkVoiceFunc vp_Func)
{
#if defined(ON_3520)
	LDK_Tlv320ConfigInterFace(0x02, (u8)vp_Func, 0x00, 0x00, NULL);
#endif

	return 0;
}

// 喊话音量设置
DLL_EXPORT s32 LDK_AudioHprVolumeSet(u8 vp_Volume)
{
#if defined(ON_3520)
	LDK_Tlv320ConfigInterFace(0x03, vp_Volume, 0x00, 0x00, NULL);
#endif

	return 0;
}




//////////////////////////////////////////////////////////////////////////////////////////////////
//设置模拟按键
DLL_EXPORT s32 LDK_SetEmudkey(u32 vp_keycode, int vp_row, int vp_col)
{
	int vl_ret = -1;
#if defined(ON_FSL_IMX6_ANDROID) || defined(ON_MTK_MT6735_ANDROID)
	if (MC_GET_APP_TH())
	{
		if (MC_GET_APP_TH()->m_emuKey.SetEmudkey(vp_keycode, vp_row, vp_col))
		{
			vl_ret = 0;
		}
	}
#endif

	return vl_ret;
}

//初始化结构体. 必须先初始化
DLL_EXPORT void kCirc_Init(t_kCirc* pp_Circ, void* pp_Base, u32 vp_Size, void* pp_LockCtx, kCircLock pp_Lock, kCircUnLock pp_UnLock)
{
	return X_kCirc_Init(pp_Circ, pp_Base, vp_Size, pp_LockCtx, pp_Lock, pp_UnLock);
}
//Push 数据 -- 返回成功PUSH的数据 0 or ALL
DLL_EXPORT u32 kCirc_PushData(t_kCirc* pp_Circ, void* pp_Data, u32 vp_I_DataLength)
{
	return X_kCirc_PushData(pp_Circ, pp_Data, vp_I_DataLength);
}

//Pop 数据 -- 返回成功POP的数据. 可能小于 vp_I_MaxDataLength
DLL_EXPORT u32 kCirc_PopData(t_kCirc* pp_Circ, u32 vp_I_MaxDataLength, void *pp_O_Data)
{
	return X_kCirc_PopData(pp_Circ, vp_I_MaxDataLength, pp_O_Data);
}

//Get 数据 -- 不 POP. 可能小于 vp_I_MaxDataLength
DLL_EXPORT u32 kCirc_GetData(t_kCirc* pp_Circ, u32 vp_StartPos, u32 vp_I_MaxDataLength, void *pp_O_Data)
{
	return X_kCirc_GetData(pp_Circ, vp_StartPos, vp_I_MaxDataLength, pp_O_Data);
}

//返回数据长度. 空环返回 0
DLL_EXPORT u32 kCirc_GetLen(t_kCirc* pp_Circ)
{
	return X_kCirc_GetLen(pp_Circ);
}

//返回容量
DLL_EXPORT u32 kCirc_GetCapacity(t_kCirc* pp_Circ)
{
	return X_kCirc_GetCapacity(pp_Circ);
}

//清空
DLL_EXPORT void kCirc_Clear(t_kCirc* pp_Circ)
{
	return X_kCirc_Clear(pp_Circ);
}

//释放空间
DLL_EXPORT void kCirc_Release(t_kCirc* pp_Circ)
{
	return X_kCirc_Release(pp_Circ);
}


DLL_EXPORT int LDK_StartGsmUsbToSuspend(void)
{
	int vl_ret = 0;

#if defined(ON_FSL_IMX6)
	vl_ret = Start_GsmUsbSuspendFunc();
#endif

	return vl_ret;
}


#if defined(ON_FSL_IMX6_ANDROID)
int Tda7719_CodecInputChnnSel(int vp_AdevInputChnn, bool vp_bOpen)
{
	return 0;
}
int Tda7719_CodecVolumeSet(int vp_Volume)
{
	return 0;
}
int Tda7719_CodecMixRouteSel(int vp_SrcChnn, int vp_TargetChnnMask, int vp_SrcVolume)
{
	return 0;
}
int Mic_InputChnnSel(int vp_Mic_Num, int vp_TargetAdevInputChnn, bool vp_bOpen)
{
	return 0;
}
#endif

DLL_EXPORT int LDK_APlayCodecInputChnnSel(int vp_AdevInputChnn, int vp_bOpen)
{
	int vl_ret = 0;

#if defined(ON_FSL_IMX6_ANDROID)
	vl_ret = Tda7719_CodecInputChnnSel(vp_AdevInputChnn, (vp_bOpen ? true : false));
#endif
	return vl_ret;
}

DLL_EXPORT int LDK_APlayCodecVolumeSet(int vp_Volume)
{
	int vl_ret = 0;

#if defined(ON_FSL_IMX6_ANDROID)
	vl_ret = Tda7719_CodecVolumeSet(vp_Volume);
#endif

	return vl_ret;
}

DLL_EXPORT int LDK_APlayCodecMixRouteSel(int vp_SrcChnn, int vp_OutChnnSel, int vp_SrcVolume)
{
	int vl_ret = 0;

#if defined(ON_FSL_IMX6_ANDROID)
	vl_ret = Tda7719_CodecMixRouteSel(vp_SrcChnn, vp_OutChnnSel, vp_SrcVolume);
#endif

	return vl_ret;
}

DLL_EXPORT int LDK_MicInputChnnSel(int vp_Mic_Num, int vp_TargetAdevInputChnn, int vp_bOpen)
{
	int vl_ret = 0;
#if defined(ON_FSL_IMX6_ANDROID)
	vl_ret = Mic_InputChnnSel(vp_Mic_Num, vp_TargetAdevInputChnn, vp_bOpen);
#endif
	return vl_ret;
}


//////////////////////////////////////////////////////////////////////////////////////////////////

extern void Dvrr_CloseWifiApFlag();
/*
获得当前线程ip接收缓冲区 长度见 SOCKET_READ_BUFFER_SIZE
输入参数无
返回：NULL 无，非NULL 缓存区
*/
DLL_EXPORT char * LDK_GetCurTmpIpRevBuff(int* psize)
{
	if ((NULL != psize) && MC_GET_APP_TH())
	{
		return MC_GET_APP_TH()->GetCurTmpIpRevBuff(psize);
	}
	return NULL;
}

// 锟斤拷锟紺RC32锟斤拷校锟斤拷锟? uInitCrc锟角筹拷始值锟斤拷锟斤拷0锟斤拷始锟斤拷锟斤拷锟街帮拷锟捷碉拷校锟斤拷锟?
///---需要移除 2017.07.10
DLL_EXPORT u32 LDK_GetCrc32(const void * pData, u32 uSize, u32 uInitCrc)
{
	return x_crc32_Calc(pData, uSize, uInitCrc);
}

///---需要移除 2017.07.10
DLL_EXPORT t_LDK_time_t LDK_get_build_time_t(const char* pp_Date, const char* pp_Time)
{
	return x_get_build_time_t(pp_Date, pp_Time);
}

///---需要移除 2017.07.10
DLL_EXPORT MDK_Result MDK_NOR_ReadUbootTimeString(u8* pp_OutputData, u32 vp_SizeOfDataInBytes)
{
#ifdef _GEMINI_FILE_SYSTEM
	u32 PartitionVerionArray[EEP_MTD_COUNT];
	u32 PartitionTimeStampArray[EEP_MTD_COUNT];
	u32 is_shadow = 0;
	int vl_ret = 0;

	vl_ret = pal_loaded_images_version(PartitionVerionArray, PartitionTimeStampArray, &is_shadow);
	if (vl_ret != e_ok)
	{
		printf("pal_loaded_images_version failed, vl_ret:%d\n", vl_ret);
		return -1;
	}

	if (is_shadow)
	{
		LDK_GetMtdTimestamp(PartitionTimeStampArray);
	}

	char al_timebuf[128] = { 0 };

	time_t uboot_timestamp = (time_t)PartitionTimeStampArray[0];
	uboot_timestamp += GEMINI_SECOND_DIFF;
	strftime(al_timebuf, 60, "(%b %d %Y - %H:%M:%S)", gmtime(&uboot_timestamp));
	X_TRACE("uboot time String is: %s", al_timebuf);
	strcpy((char*)pp_OutputData, al_timebuf);

	return 0;
#else
#define UBOOT_BLOCK_SIZE_IN_BYTE          0x80000

	u8* pl_readBuf = NULL;
	u8* pl_Cur;
	MDK_Result vl_ret = -1;
	int vl_strLen = strlen(UBOOT_VER_STRING);
	x_store_nor* pl_ubootNorStore = NULL;

	// 	int vl_MemHandle;
	// 	void* pl_Flashbaseaddr;//直接读取

	if (NULL == pp_OutputData)
	{
		return -1;
	}
	if (vp_SizeOfDataInBytes < (u32)(vl_strLen + 1))
	{
		return -1;
	}

	pl_readBuf = (u8*)x_malloc(sizeof(u8)*UBOOT_BLOCK_SIZE_IN_BYTE);
	if (NULL == pl_readBuf)
	{
		return -1;
	}

#if 1
	// MTD方式
	pl_ubootNorStore = x_new x_store_nor(MDK_UBOOT_FLASH_FILE, false);
	if ((NULL == pl_ubootNorStore) || !pl_ubootNorStore->Open())
	{
		X_ERR("NOR STORE OPEN FAILED");
		vl_ret = -1;
		goto ERR;
	}

	pl_ubootNorStore->Seek(0, SEEK_SET);
	if (-1 == pl_ubootNorStore->Read(pl_readBuf, UBOOT_BLOCK_SIZE_IN_BYTE))
	{
		printf("read flash error\n");
		vl_ret = -1;
		goto ERR;
	}
#else
	// 内存映射(不行)
	vl_MemHandle = open("/dev/mem", O_RDWR);
	if (vl_MemHandle == -1)
	{
		vl_ret = -1;
		goto ERR;
	}

	pl_Flashbaseaddr = mmap(0, FLASH_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, vl_MemHandle, FLASH_BASE);
	memcpy((void*)pl_readBuf, pl_Flashbaseaddr, UBOOT_BLOCK_SIZE_IN_BYTE);
	close(vl_MemHandle);

#endif

	for (pl_Cur = pl_readBuf; pl_Cur - pl_readBuf < 0x80000; pl_Cur++)
	{
		if (*pl_Cur == 'U')
		{
			if ((strncmp((char*)pl_Cur, UBOOT_VER_CMP_STRING, strlen(UBOOT_VER_CMP_STRING)) == 0)
				//&& ((*(pl_Cur+14)) == ' ')
				)
			{
				memset(pp_OutputData, 0, vp_SizeOfDataInBytes);
				strncpy((char*)pp_OutputData, (char*)pl_Cur, vl_strLen);
				vl_ret = 0;
				break;
			}
		}
	}

ERR:
	free(pl_readBuf);

	if (NULL != pl_ubootNorStore)
	{
		pl_ubootNorStore->Close();
		delete pl_ubootNorStore;
	}

	return vl_ret;
#endif
}

///---需要移除 2017.07.10
DLL_EXPORT MDK_Result MDK_NOR_ReadUbootVersion()
{
	int vl_ret = 0;
#ifdef _GEMINI_FILE_SYSTEM

	u32 PartitionVerionArray[EEP_MTD_COUNT];
	u32 PartitionTimeStampArray[EEP_MTD_COUNT];
	u32 is_shadow = 0;
	int ret = 0;

	ret = pal_loaded_images_version(PartitionVerionArray, PartitionTimeStampArray, &is_shadow);
	if (ret != e_ok)
	{
		printf("pal_loaded_images_version failed, vl_ret:%d\n", ret);
		return -1;
	}

	if (is_shadow)
	{
		LDK_GetMtdVersion(PartitionVerionArray);
	}

	return PartitionVerionArray[0];

#else
	vl_ret = 0x1000000;
#endif
	return vl_ret;
}

// 获得未发送及未读数据的字节数 BSD_FIONREAD BSD_SIOCOUTQ
DLL_EXPORT int MDK_bsd_ioctl(BSD_SOCKET vp_SocketId, long vp_Cmd, u32 * pp_Arg)
{
	LDK_CORE_WRITEMONITORLOG(0xa000003e, API_LIB_TIMEOUT, NULL, false);
	if (!MC_GET_APP_TH())
	{
		return -1;
	}

	return MC_GET_APP_TH()->m_ipnet.bsd_ioctl(vp_SocketId, vp_Cmd, pp_Arg);
}

///---需要移除 2017.07.10
DLL_EXPORT t_LDK_time_t LDK_mktime(t_LDK_tm * ptm)
{
	return x_mktime((x_tm_t*)ptm);
}

///---需要移除 2017.07.10
DLL_EXPORT int LDK_get_build_time(const char* pp_Date, const char* pp_Time, u8* pp_BuildDate)
{
	return x_get_build_time(pp_Date, pp_Time, pp_BuildDate);
}

///---需要移除 2017.07.10
DLL_EXPORT int LDK_get_core_build_version(u32* pp_Ver, u8* pp_BuildDate)
{
	*pp_Ver = LDK_VERSION;
	LDK_get_build_time(__DATE__, __TIME__, pp_BuildDate);
	return 0;
}

DLL_EXPORT int LDK_GetProbeWifiList(char *ppwifilist, int psize, int pmode)
{
	if (!pmode)
		return x_getProbeDiffWifiList(ppwifilist, psize);
	else
		return x_getProbeWifiList(ppwifilist, psize);
}

DLL_EXPORT int LDK_CfgInterfacePrio(u8 vpId, u8 vpType, u8 vpPrio)
{
    if (MC_GET_APP_TH())
    {
        if (vpType == IT_UART)
        {
			x_asio_uart* pl_uart = MC_GET_APP_TH()->GetUartViaID(vpId);
			if (NULL != pl_uart)
				return pl_uart->ConfigPrio(vpPrio);
			else
				return -1;
	    }
		else
		{
		    return -1;
		}
    }
	else
	{
	    return -1;
	}
}

DLL_EXPORT int LDK_CommInfo(u16 vp_InfoType, TCommInfo_S* pp_Info, u16* pp_InfoLen)
{
    if(NULL == pp_Info || NULL == pp_InfoLen)
    {
        MDK_TRACE("pp_Info=%p pp_InfoLen=%p", pp_Info, pp_InfoLen);
		return -1;
    }

	switch(vp_InfoType)
	{
	case 0x0010: //获取mac
	    if(*pp_InfoLen >= 10)
	    {
			*pp_InfoLen = 10;
	        if(pp_Info->reserved == 1)
				return get_net_inf_mac("eth0", (char *)(pp_Info->Info));
			else if(pp_Info->reserved == 2)
				return get_net_inf_mac("wlan0", (char *)(pp_Info->Info));
	    }
		break;
	case 0x0011: //获取本次点火录像时间
		if (*pp_InfoLen >= 8)
		{
			pp_Info->uInfoType = 0x0011;
			// 本次录像持续时长
#ifdef FUNC_USING_API_CALLBACK
#else
			Dvrr_GetCurRecTime((char *)(pp_Info->Info));
#endif
			*pp_InfoLen = 8;
			//MDK_DumpMemory(pp_Info, *pp_InfoLen, "acc rec time");
		}
		break;
	case 0x0012://获取wifi配置信息
		if (*pp_InfoLen >= 4+sizeof(t_wifi_item_S))
		{
			ymemset(pp_Info, 0, *pp_InfoLen);
			pp_Info->uInfoType = 0x0012;
			MDK_TRACE("Ma<=Va uInfoType %#x len %u", pp_Info->uInfoType, *pp_InfoLen);
			int vl_ret = -1;
#ifdef FUNC_USING_API_CALLBACK
#else
			vl_ret = Dvrr_GetWifiCfg((char *)(pp_Info));
#endif
			//MDK_DumpMemory(pp_Info, *pp_InfoLen, "wifi config");
			if (0 == vl_ret)
			{
				*pp_InfoLen = 4 + sizeof(t_wifi_item_S);
			}
			else
				*pp_InfoLen = 0;

			return vl_ret;
		}
		break;
	case 0x0013://获取状态信息（与0x94消息的mode为2的数据内容相同）
		if (*pp_InfoLen >= 16)
		{
			pp_Info->uInfoType = 0x0013;
			MDK_TRACE("Ma<=Va uInfoType %#x", pp_Info->uInfoType);
			int vl_ret = -1;
#ifdef FUNC_USING_API_CALLBACK
#else
			vl_ret = Dvrr_GetDevState((u8 *)(pp_Info->Info));
#endif
			if (0 == vl_ret)
			{
				*pp_InfoLen = 16;
				//MDK_DumpMemory(pp_Info, *pp_InfoLen, "dev state");
			}
			else
				*pp_InfoLen = 0;
			
			return vl_ret;
		}
		break;
	case 0x0020:// 获取守护参数
	{
		pp_Info->uInfoType = 0x0020;
		MDK_TRACE("Ma<=Va uInfoType %#x %d", pp_Info->uInfoType, *pp_InfoLen);
		if (*pp_InfoLen >= 4 + sizeof(t_WatchParamEx))
		{
#ifdef FUNC_USING_API_CALLBACK
#else
			// 守护通道
			u8 nChn = pp_Info->reserved;
			int vl_ret = Dvrr_GetWatchParam(nChn, pp_Info->Info);
			if (0 == vl_ret)
			{
				*pp_InfoLen = 4 + sizeof(t_WatchParamEx);
				//MDK_DumpMemory(pp_Info, *pp_InfoLen, "link param");
			}
			else
				*pp_InfoLen = 0;

			return vl_ret;
#endif
		}
	}
	break;
	default:
		break;
	}
	return 0;
}


