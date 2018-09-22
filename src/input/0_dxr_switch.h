#pragma once
#define TEST_FOR_PRODUCE			// 生产测试模式
#define USE_FUNCTION_TALK_TEMP		// 实时对讲

// 缺省摄像头制式 参见《VIDEO_NORM_E》
#define DXR_CAMERA_MODE_DEFAUIL				VIDEO_ENCODING_MODE_PAL
// T3A屏幕方式
#define DXR_SEND_STREAM_LINE_OUT			0
// 本地传输采用localsession
#define DXR_SEND_STREAM_USE_LOCAL_SOCKET   	1
// 本地视频采用FB显示
#define DXR_SEND_STREAM_TO_FRAME_BUFFER    	2

#define DXR_VIDEO_G2D_MODE                  1
#define DXR_VIDEO_CAM2FB_MODE               2

#define VM_MMST_FLOWCTL_PROP				(1)		// 媒体传输流量控制属性

// 不支持任何Mp4库
#define	PROP_MP4LIB_NULL	0
// Mp4V2库
#define	PROP_MP4LIB_MP4V2	1

/* T6A，手麦对应音频通道号为2 */
#if defined(ON_FSL_IMX6_ANDROID)
	#define TALK_UPLOADING_CHANNEL_DEFAULT  2
	/* 手麦对讲测试模式 */
	//#define TALK_UPLOADING_TEST_MODE 1

	#define DXR_SEND_STREAM_MODE	DXR_SEND_STREAM_USE_LOCAL_SOCKET

	#define HAL_THREAD												// 此处开启,需提供GetI2C相关接口 20170602
	#define HAL_THREAD_ISLOAD

	#define SUPPORT_DVRR_GUI										// char/picture basic operate
	#define _ADEC_AMR_FRAME_COMPUTE									// amr音频长度计算宏
	#define _ADEC_AO_SERVICE_SURPPORT								// 音频输出使用ao服务

	#define FUNC_USED_MP4LIB		PROP_MP4LIB_NULL
	#define SURPPORT_AUDIO_AO										// 支持ao输出功能
	#define SURPPORT_VIDEO_DEC										// 支持video decode功能
	#define DXR_ADJUST_TIMESTAMP									// 校准视频时间戳

	#define AUTO_ETH_ONLINE
	#define SUPPORT_IPC_FUN											// 支持V3_IPC
	#define FUNC_ADD_IPC_DEVICE_MNG 								// 增加IPC到设备管理
	#define FUNC_SUPPORT_SELET_IPC_CORER_VERSION					 //增加查询IPC内核版本信息

	//#define SUPPORT_V3_IPC_ACCESS				// 支持V3-IPC接入	20180725 现行代码，本地显示(实时/历史)，V3_IPC由GA解码，普通摄像头直接刷内存，两者只能使用一种
	#ifdef SUPPORT_V3_IPC_ACCESS	
		#define DXR_VIDEO_ONLY_LOCAL_TRANSFER		// 仅用于本地视频传输
		#define FUNC_SUPPORT_VIDEO_SDK			   //支持T4视频SDK(默认不支持,需注释掉)
		#define SUPPORT_IPCCLIENT_HISTORY_FUN
		//#define DXR_RECORD_VIDEO_POWERON_AUTO_START	// 开机自动开启视频录像功能
		#define PROP_PRO_TYPE_YWRTP_ON				// 有为传输协议
		#define FUNC_NOT_SUPPORT_IPC_POWER_MNG     //不支持终端对IPC-V3的通信异常时的掉电恢复策略
		#ifndef FUNC_NOT_SUPPORT_IPC_POWER_MNG
		#define FUNC_SUPPORT_IPC_POWEROFF_DELAY     //支持终端延时关闭IPC-V3电源的策略
		#endif
	#endif 
	#define FUNC_MOD_G2D_SHOW_DETECT				// 修改写显存间隔(处理本地显示卡顿问题)

#elif defined(ON_FSL_IMX6)
	#define TALK_UPLOADING_CHANNEL_DEFAULT  0
	#define AUTO_ETH_ONLINE

	#define VOLUME_ADJUST_MAX
	#define FUNC_TEST_AVEXPORT_TOUDISK								// 功能测试F/T U盘导出已录音视频

	#define DXR_SEND_STREAM_MODE	DXR_SEND_STREAM_TO_FRAME_BUFFER

	#define DXR_FRAME_CROP											// 对视频帧进行裁剪 消除马赛克和黑边

	#define ON_FSL_IMX6_TVP5158
	#define DXR_FLASH_DATABASE										// Flash数据库功能
	#define DXR_ADJUST_TIMESTAMP									// 校准视频时间戳

	#define FUNC_HD10_DEBUG											// HD10调试功能
	#define PROP_EXPORT_EMMC_TO_UNFORMAT_UDISK						// EMMC导出U盘文件功能
	#define FUNC_EXPORT_TO_PIC										// 导出图片文件功能

	#define DXR_STOP_BEFORE_FETCH									// HD10提取前停止录像
	#define DXR_MOD_5158_CAPTURE									// 修改5158获取视频问题

	#define SUPPORT_DVRR_GUI										// char/picture basic operate
	#define _ADEC_AMR_FRAME_COMPUTE									//amr音频长度计算宏
	#define _ADEC_AO_SERVICE_SURPPORT								//音频输出使用ao服务

	#define FUNC_USED_MP4LIB	PROP_MP4LIB_MP4V2
	#define SURPPORT_AUDIO_AO										//支持ao输出功能
	#define SURPPORT_VIDEO_DEC										//支持video decode功能
	#define FUNC_SLEEP_SANP											//熄火拍照正常
	#define FUNC_CUT_PARTITION_DATA_SIZE							// 裁剪媒体区大小(裁剪512MB给用户区使用)

#elif defined(ON_MTK_MT6735_ANDROID)
	#define TALK_UPLOADING_CHANNEL_DEFAULT  2
	/* 手麦对讲测试模式 */
	//#define TALK_UPLOADING_TEST_MODE 1
	#define AUTO_ETH_ONLINE

	#define DXR_SEND_STREAM_MODE	DXR_SEND_STREAM_USE_LOCAL_SOCKET // 

	#define HAL_THREAD												// 此处开启,需提供GetI2C相关接口 20170602
    //#define HAL_THREAD_ISLOAD

	#define SUPPORT_IPC_FUN											// 支持V3_IPC

	#define SUPPORT_DVRR_GUI										// char/picture basic operate
	#define _ADEC_AMR_FRAME_COMPUTE									//amr音频长度计算宏
	#define _ADEC_AO_SERVICE_SURPPORT								//音频输出使用ao服务

	#define FUNC_USED_MP4LIB	PROP_MP4LIB_NULL
	#define SURPPORT_AUDIO_AO										//支持ao输出功能
	#define SURPPORT_VIDEO_DEC										//支持video decode功能
	#define FUNC_ADD_IPC_DEVICE_MNG 								// 增加IPC到设备管理
	#define FUNC_SUPPORT_SELET_IPC_CORER_VERSION                     //增加查询IPC内核版本信息
	#define DXR_BUSSTATION_WATERMARK								// 新增业务数据水印显示

#elif defined(ON_3520)
	#define TALK_UPLOADING_CHANNEL_DEFAULT  0
	#define AUTO_ETH_ONLINE

	#define DXR_PIC_SERVER_SUPPORT					// 支持独立图片服务器(WIFI连接媒体服务器,支持媒体服务器广播连接的功能)

	#define HAL_THREAD								// 此处开启
    #define HAL_THREAD_ISLOAD

	#define SUPPORT_IPC_FUN							// 支持V3_IPC

	#define SUPPORT_IRA_FUN							// 支持红外功能
	#define SUPPORT_GUI_FUN							// 支持图像界面功能

	///--- 此处在imx6开启宏会引起系统运行异常，可能和堆栈溢出有关
	// 增加川标错误上报
	#define DXR_ADD_ERRINFO_REPORT
	
	#define FUNC_MA_CONF_DEF_RECORD_PARAM		    //MA配置默认录像参数
	#define FUNC_SD_DEFAULT_PARAMA_ACTIVE           //SD卡默认参数生效
	//#define SUPPORT_G722_ENCODE_TEST	// G722测试宏定义

	#ifdef ON_3520D_V300
		#define FUNC_CAMERA_OCCLUSION_DETECTION		// 摄像头遮挡检测功能
		//#define MOD_SUPPORT_1080P					// 支持1080P
		#ifdef MOD_SUPPORT_1080P
			#define FUN_VIDEO_MODE_7             	// 视频模式7，支持1080P
			#define CORRECT_VIDEOMASK				// MA配置正确的掩码
			#define FUN_REPLAY_1080P				// 1080P本地回放
		#endif
		#define MOD_VIDEO_OUTPUT_CTRL				// 测试屏

		#define FUN_VIDEO_PARAM_CONFIG				// 录像模式配置 注 目前1080P还在使用该宏定义 2018.05.23 tgs
		//#define XINJIANG_AUDIO					// 新疆招标使用，音频需要320的采样点
		//#define SUPPORT_FUNC_STORE_DIAGNOSE		// 支持存储介质检测
	#else
		//#define SUPPORT_EXTERNAL_IPC_FUN			// 支持标准IPC，缺省关闭，仅对特定单号开放
	#endif

	#define FUN_ADAPTIVE_VIDEO_MODE				// 自适应视频模式
	#ifdef FUN_ADAPTIVE_VIDEO_MODE
		#undef FUN_VIDEO_PARAM_CONFIG
		//#define SUPPORT_RECOGNITION_N_CAMERA
		//#define FUNC_ADD_AUTO_SWITCH_NP				// 支持自动切换N/P制录像
	#endif

	#define DXR_RESPONSE_PACKET_QUANTITY_CONTROL	// T3屏查询视频段回应包数量控制
	#define DXR_ADD_REPLAY_CTRL						// 增加播放控制逻辑
	#define DXR_LOCAL_REPLAY_AUTOFIT_SPEED			// 本地视频回放控制速度

	#define FUNC_MOD_MKAV_TEST						// 修改MKAV音频测试
	//#define DEBUG_MKAV_MODE                         // 调试MKAV模式音频视频图片上传
	#define SUPPORT_DVRR_GUI						// char/picture basic operate
	#define FUNC_PCM5100_POWER_SEQ					// 解决pcm5100开电时序问题

	#define FUNC_USED_MP4LIB	PROP_MP4LIB_MP4V2
	#define SURPPORT_AUDIO_AO						// 支持ao输出功能
	#define SURPPORT_VIDEO_DEC						// 支持video decode功能
	//#define SURPPORT_AMR_HARD_CODE				// 支持AMR硬件编码

	#define DXR_RECORD_VIDEO_STEP_START				// 支持录像分步启动 修正水印跳动较大问题

	#define FUNC_ADD_IPC_DEVICE_MNG					// 增加IPC到设备管理
	#define FUNC_SLEEP_SANP							//熄火拍照正常

	#define DXR_AUDIO_MAIN_SUM_STREAM				// 支持音频主子码流功能
	#define USE_NOISE_SUPPRESSION_LIB				// 音频降噪处理（降噪级别为2）

	#define FUNC_ADD_BATCH_READ						// 增加批量读操作
	#define DXR_ADD_EXT_APP_DATA					// 扩展应用数据分区管理
	#define USBNET_USING_DHCP						// USB网络版本核对连接是否使用DHCP获取动态IP地址
	#define FUNC_MOD_RM_INS_MOD						// 调整磁盘驱动卸载/装载修复代码（fqy 20180801）
	//#define FUNC_MOD_SYSTEM_FAIL					// 使用fopen替换system(解决system返回-1问题)
	#define FUNC_MOD_LOCAL_VO						// [fqy 20180806]修正本地实时视频输出

	//#define SUPPORT_FUNC_STORE_DIAGNOSE_DEBUG		// 支持存储介质检测调试
#ifdef SUPPORT_FUNC_STORE_DIAGNOSE_DEBUG		// 支持存储介质检测调试
#	ifndef SUPPORT_FUNC_STORE_DIAGNOSE
#		define SUPPORT_FUNC_STORE_DIAGNOSE
#	endif
#endif
#elif defined(ON_ALLWINNER_V3_ANDROID)

	#define TALK_UPLOADING_CHANNEL_DEFAULT	0

	#define HAL_THREAD								//此处开启,需提供GetI2C相关接口 20170602
	#define SUPPORT_IPC_FUN							// 支持V3_IPC
	//#define SURPPORT_AUDIO_AO						//支持ao输出功能
	//#define SURPPORT_VIDEO_DEC					//支持video decode功能

	#define FUNC_USED_MP4LIB	PROP_MP4LIB_NULL
    #define FUNC_SUPPORT_SELET_IPC_CORER_VERSION    //增加查询IPC内核版本信息
//	#define FUNC_IS_SUPPORT_CMI_INTERFACE           //是否使用新增的CMI接口
#else
	#define TALK_UPLOADING_CHANNEL_DEFAULT  0
	#define AUTO_ETH_ONLINE
	#define SUPPORT_DVRR_GUI						// char/picture basic operate

	#define FUNC_USED_MP4LIB	PROP_MP4LIB_NULL

#endif

#if !defined(ON_3520)
#	ifdef FUNC_ADD_BATCH_READ
#		undef FUNC_ADD_BATCH_READ					// 增加批量读操作
#	endif
#	ifdef DXR_ADD_EXT_APP_DATA
#		undef DXR_ADD_EXT_APP_DATA					// 扩展应用数据分区管理
#	endif
#endif

//#ifndef ON_ALLWINNER_V3_ANDROID
	//2016-11-22 add
//#	define NETVIRTUALUART
//#endif

#define TALK_TRANS_CHANNEL      0xF
// #define _SAVE_AUDIO_TALK_TEST
// #define _GET_AUDIO_TEST

// #define SC_K5_SELF_TALK_TEST

#define NEW_REQUEST_STOP_OLD_SESSION
#define USING_SEPARATE_LOGIC_CHANNEL
//#define SAVE_MEDIA_ATTR_TO_EEP
#define USING_CHANNEL_ATTRSET_FIRST
#define USING_YW_MEDIA_POSTFIX

#define QUERY_FETCHING_ALL_SD
//#define USING_CURRENT_MONTH_TIMESTAMP
//#define USING_OPPOSITE_TIME_MEDIAID
#ifdef USING_OPPOSITE_TIME_MEDIAID
#	define OPPOSITE_TIME_BASING_OFFSET 473385600 //base on 2000~2015  15 years seconds base
#endif
//#define MEDIA_SAVING_H264_FILE
#define VIDEO_MOD_CTL	// 预留MPU控制OSD叠加接口
#define ACCESS_SD_DIRECTLY_FTR
#define MAJORVSMINOR //子码流

/*使用tlv320aic23b 作为DA芯片*/
#define AUDIO_TLV320_DECODE_DESIGN

#ifdef TEST_FOR_PRODUCE
#	define TEST_FOR_AV	//AI---AENC--FILE--ADEC--AO
#endif

#define DISP_VEDIO_LOST_IN_TRANS_STREAM

//#define _VEDIO_CRC_CHECK

//#define _GBV808_SPLIT_PKG

//////////////////////////////////////////////////////////////////////////////////

#ifdef _GEMINI_FILE_SYSTEM
	//#define SUI_REPLAY_STOP_REC   //Gemini系统回放时不停止录像，目前回放最大支持4路D1
#else
	#define SUI_REPLAY_STOP_REC
#endif

#define DXR_USE_SNAP_CMD_GPS_DATA

#if defined(ON_FSL_IMX6) || defined(ON_FSL_IMX6_ANDROID) || defined(ON_MTK_MT6735_ANDROID)
	#define DXR_SHOW_VIDEO_MODE 					DXR_VIDEO_G2D_MODE
	//#define DXR_SHOW_VIDEO_MODE  					DXR_VIDEO_CAM2FB_MODE

	#define DXR_VIDEO_FIFO_TRANSFER_MODE

	#define DXR_VIDEO_EPOLL_RECV					// 使用EPOLL接收视频数据

	/* 播放历史视频时，各路视频时间差测试宏开关 */
	//#define VDEC_TIME_DIFFERENT_TEST            	1
#else
	#define DXR_SEND_STREAM_MODE  					DXR_SEND_STREAM_LINE_OUT
#endif

////2016-1-26 add 默认不需要开启
//#define _0x10_CHECK
//#define _0x10_REPARE

#define _SZ_PROTOCOL_TEST

#define MENU_VIDEOUTPUT_MODE_SELECT
#define ADD_FORMAT_SETTING

#define FUNC_TTS_PLAY_DELAYMOD						// TTS播报延时修复

// 自动测试调试
#define FUN_ADD_AUTO_TEST_POWER					// 自动测试电源控制

//#define FUNC_USE_STD_LIST							// 使用标准容器链表

//#define K7_V300_VIDEO4SHOW

//#define FUNC_MMST_EPOLL_20						// 媒体传输网络版本2.0 使用EPOLL边缘触发 模块化

////////////// 下面为1076过检需要开启的宏定义////////////////////////
#if !defined(ON_ALLWINNER_V3_ANDROID)
	#define SUPPORT_BMIPC_EPOLL_20						// IPC使用Epoll 边缘触发
#endif

//#ifdef SUPPORT_BMIPC_EPOLL_20						// IPC使用Epoll 边缘触发
	#define IPC_SUPPORT_DEV_REBOOT					// IPC支持終端重啟
	#define IPC_SUPPORT_UPGRADE		1				// IPC是否支持升級	若終端重啟功能未完善，該宏不生效
	//#define EXT_UPGRADE_REBOOT_BY_NOTIFY			// V3-IPC远程升级通知
//#endif

#if defined(ON_3520)
	#define PROP_USE_MMST_BUBIAO					// 支持部标传输协议
	#define PROP_BB1076_RELEASE						// 1076部标功能出货版本(Release)
	//#define FUNC_BB1076_DETECTION					// 1076部标功能过检版本(Debug)
	#ifdef FUNC_BB1076_DETECTION
		#undef PROP_BB1076_RELEASE
	#endif
	#define FUN_ADJUST_UPDATE_VPP_TIME                 //自动校准水印的跳变时间
	//#define FUNC_TRACE_DEBUG_INFO						// 输出调试信息
#endif

#if defined(ON_ALLWINNER_V3_ANDROID)    			// IPC-V3默认不支持部标，需注释掉
	//#define PROP_USE_MMST_BUBIAO					// 支持部标传输协议
	//#define PROP_BB1076_RELEASE					// 1076部标功能出货版本(Release)	
#endif

/* MP4通过减少buff，仅处理168字节文件头，需核实生成中是否有读mp4的操作*/
////////////////////////////////////////////////////////////////

//#ifdef PROP_USE_MMST_BUBIAO // 部标过检默认使用固定码率
	//#define DXR_ADD_INSEQUENCE_FETCH			// 倒序提取数据(仅支持I帧)
	//#define DXR_MOD_REMOTE_REPLAY_CTRL			// 修改远程历史录像回放控制
//#endif

#if defined(FUNC_BB1076_DETECTION)				// 部标1076过检用
	//#define FUNC_MP4_FILE_HEAD						// 自动生成MP4文件头
	#define MMSTBB_ADJUST_RATE						// 允许用*#命令来纠正码率
	#define DXR_PLATFORM_CHANGE_MAIN_STREAM 		// 平台下发主码流参数保存到EEP
	#ifdef FUN_VIDEO_PARAM_CONFIG					// 录像模式配置
		#undef FUN_VIDEO_PARAM_CONFIG				// 录像模式配置
	#endif
	#define DXR_SYNC_GPS_AND_OSD					// 同步水印时间和段GPS时间
	#define DXR_UNLIMIT_FRMRATE_BITRATE_GOP_CHECK	//解除帧率、码率、I帧间隔检查

	#ifdef VM_MMST_FLOWCTL_PROP
		#undef VM_MMST_FLOWCTL_PROP
	#endif
	#define VM_MMST_FLOWCTL_PROP			(0)		// 媒体传输流量控制属性

	//#define CUT_LOG_SAVE							// 移除日志保存功能
	//#define DXR_ADD_AUTO_CHANGESEG					// 增加自动切换段功能
	//#define DXR_MOD_LOST_ALARM_FLAG					// 修改报警标记丢失问题
	//#define DXR_CTRL_SEG_SAVE_TIME_LONG				// 控制段时长
	//#ifdef COMBINE_VIDEO_FTR
	//	#undef COMBINE_VIDEO_FTR					// 移除子码流并包处理
	//#endif
	//#define MMSTBB_BITRATE_ADJUST					// 实时播放时,可以让码率+5%
	//#undef RC_VBR
	//#define RC_VBR 0
	//#define FUNC_DELAYCLOSE_CHNLPAUSE				// 暂停通道延时断开TCP
	//#define FUNC_DELAY_REPORT_DISK_STATE			// 延时上报磁盘状态
	//#define FUNC_FORCE_QUERY_EVENT					// 强制查询事件
	//#define FUNC_MODIFY_VLID_RISE					// 确保集ID和段ID增长
	//#define FUNC_MODIFY_CHG_SEGMENT					// 修正切换段处理
	#define FUNC_EXPORT_MP4_BY_DELSEG				// 通过删除段导出mp4文件
	#define SUPPORT_CAMERABLOCK_EVENT				// 支持摄像头遮挡
#elif defined(PROP_BB1076_RELEASE)
	#define DXR_PLATFORM_CHANGE_MAIN_STREAM 		// 平台下发主码流参数保存到EEP
	//#define FUNC_MP4_FILE_HEAD						// 自动生成MP4文件头
	//#define CUT_LOG_SAVE							// 移除日志保存功能
	//#define DXR_ADD_AUTO_CHANGESEG					// 增加自动切换段功能
	//#define DXR_MOD_LOST_ALARM_FLAG					// 修改报警标记丢失问题
	//#define DXR_CTRL_SEG_SAVE_TIME_LONG				// 控制段时长
	//#ifdef COMBINE_VIDEO_FTR
	//	#undef COMBINE_VIDEO_FTR					// 移除子码流并包处理
	//#endif
	//#define MMSTBB_BITRATE_ADJUST					// 实时播放时,可以让码率+5%
	//#undef RC_VBR
	//#define RC_VBR 0
	//#define FUNC_DELAYCLOSE_CHNLPAUSE				// 暂停通道延时断开TCP
	//#define FUNC_DELAY_REPORT_DISK_STATE			// 延时上报磁盘状态
	//#define FUNC_FORCE_QUERY_EVENT					// 强制查询事件
	//#define FUNC_MODIFY_VLID_RISE					// 确保集ID和段ID增长
	//#define FUNC_MODIFY_CHG_SEGMENT					// 修正切换段处理
	//#define FUNC_EXPORT_MP4_BY_DELSEG				// 通过删除段导出mp4文件
	//#define SUPPORT_CAMERABLOCK_EVENT				// 支持摄像头遮挡
#endif

//#ifdef FUNC_MP4_FILE_HEAD					// 自动生成MP4文件头
	//#define FUNC_EXPORT_TO_MP4						// 部标视频上传用的是mp4
//#else
//#endif

#ifdef FUNC_IS_IPCV3
	#undef VM_MMST_FLOWCTL_PROP
	#define VM_MMST_FLOWCTL_PROP			(0)		// 媒体传输流量控制属性
#endif

#define FUNC_PROC_SCRIPT	// 处理系统脚本文件

// 针对不同的功能号做相应的功能配置调整
#ifndef VDFS
	#define VDFS						VDFS_DVR_3G	// 3G DVR功能（多以MA功能号为主）
#endif

#if (VDFS_CAMERA_IP_A == VDFS)		// 直接对接T4的标准IPC功能
	// 实时传输子码流
	#define DXR_STOR_LOCAL_SUPPORT				// 本地存储媒体数据
	#define PROP_STOR_DATA_FETCH				// 提取磁盘数据属性+++
	///--- 本地历史提取功能
	// 采集双码流支持

	#define DXR_RECORD_VIDEO_POWERON_AUTO_START	// 开机自动开启视频录像功能
	//#define DXR_TRAN_WIRELESS_SUPPORT			// 支持无线媒体传输功能
	//#define DXR_MA_CTRL_ENABLE				// 支持MA控制
	//#define PROP_PRO_TYPE_LOCALTRANS_ON		// 本地传输
	#define PROP_PRO_TYPE_IPCSELF_ON			// IPC协议
	//#define DXR_VIDEO_ONLY_LOCAL_TRANSFER		// 仅用于本地视频传输
	#define DXR_VIDEO_IPC_ADAS					// IPC adas功能
	#define SUPPORT_IPCCLIENT_FUN
	#define SUPPORT_IPCCLIENT_UPGRADE_FUN
	#define SUPPORT_IPCCLIENT_HISTORY_FUN

	#define FUNC_STORE_NORMAL_CAM_ENABLE		// 启用普通摄像头数据存储

	#define FUNC_DATA_TRAN_EPU					// EPU命令数据传输支持

	#ifdef PROP_USE_MMST_BUBIAO             // 支持部标传输协议
		#define FUNC_EXPORT_TO_MP4				// 部标视频上传用的是mp4
	#endif		

	//#define FUNC_SUPPORT_AUDIO_G726         // IPC-V3音频传输和存储采用G726编码
	
#elif (VDFS_CAMERA_IP_B == VDFS)	// 通过F8T存储再对接T4标准IPC功能
	//#define DXR_STOR_LOCAL_SUPPORT			// 本地存储媒体数据
	//#define PROP_STOR_DATA_FETCH				// 提取磁盘数据属性+++

	#define DXR_RECORD_VIDEO_POWERON_AUTO_START	// 开机自动开启视频录像功能
	//#define DXR_TRAN_WIRELESS_SUPPORT			// 支持无线媒体传输功能
	//#define DXR_MA_CTRL_ENABLE				// 支持MA控制
	//#define PROP_PRO_TYPE_LOCALTRANS_ON		// 本地传输
	#define PROP_PRO_TYPE_IPCSELF_ON			// IPC协议
	//#define DXR_VIDEO_ONLY_LOCAL_TRANSFER		// 仅用于本地视频传输
	//#define DXR_VIDEO_IPC_ADAS					// IPC adas功能
	#define SUPPORT_IPCCLIENT_FUN
	#define SUPPORT_IPCCLIENT_UPGRADE_FUN
	#define SUPPORT_IPCCLIENT_HISTORY_FUN

	#define FUNC_STORE_NORMAL_CAM_ENABLE		// 启用普通摄像头数据存储

	#define FUNC_DATA_TRAN_EPU					// EPU命令数据传输支持

#elif (VDFS_DR_IPC_EXCHG_SCR == VDFS)		// 高清IPC数据中转（用于现有T4）
	#define DXR_STOR_LOCAL_SUPPORT				// 本地存储媒体数据
	#define PROP_STOR_DATA_FETCH				// 提取磁盘数据属性+++

	#define DXR_RECORD_VIDEO_POWERON_AUTO_START	// 开机自动开启视频录像功能
	//#define DXR_TRAN_WIRELESS_SUPPORT			// 支持无线媒体传输功能
	#define DXR_MA_CTRL_ENABLE					// 支持MA控制

	#define PROP_PRO_TYPE_LOCALTRANS_ON			// 本地传输
	//#define PROP_PRO_TYPE_IPCSELF_ON			// IPC协议
	#define DXR_VIDEO_ONLY_LOCAL_TRANSFER		// 仅用于本地视频传输
	//#define DXR_VIDEO_IPC_ADAS				// IPC adas功能
	#define SUPPORT_V3_IPC_ACCESS				// 支持V3-IPC接入

	#define PROP_PRO_TYPE_YWRTP_ON				// 有为传输协议

	#define FUNC_STORE_NORMAL_CAM_ENABLE		// 启用普通摄像头数据存储

	#define FUNC_DATA_TRAN_EPU					// EPU命令数据传输支持

	#define FUNC_SUPPORT_VIDEO_SDK			//支持T4视频SDK(默认不支持,需注释掉)

	#define FUNC_NOT_SUPPORT_IPC_POWER_MNG     //不支持终端对IPC-V3的通信异常时的掉电恢复策略
	#ifndef FUNC_NOT_SUPPORT_IPC_POWER_MNG
		#define FUNC_SUPPORT_IPC_POWEROFF_DELAY     //支持终端延时关闭IPC-V3电源的策略
	#endif

#elif (VDFS_DR_IPC_STOR_SCR_905 == VDFS)
	#define DXR_STOR_LOCAL_SUPPORT				// 本地存储媒体数据
	#define PROP_STOR_DATA_FETCH				// 提取磁盘数据属性+++

	#define DXR_RECORD_VIDEO_POWERON_AUTO_START	// 开机自动开启视频录像功能
	//#define DXR_TRAN_WIRELESS_SUPPORT			// 支持无线媒体传输功能
	#define DXR_MA_CTRL_ENABLE					// 支持MA控制

	#define PROP_PRO_TYPE_LOCALTRANS_ON			// 本地传输
	//#define PROP_PRO_TYPE_IPCSELF_ON			// IPC协议
	#define DXR_VIDEO_ONLY_LOCAL_TRANSFER		// 仅用于本地视频传输
	//#define DXR_VIDEO_IPC_ADAS				// IPC adas功能
	#define SUPPORT_V3_IPC_ACCESS				// 支持V3-IPC接入

	#define FUNC_STORE_NORMAL_CAM_ENABLE		// 启用普通摄像头数据存储

	#define MC_USE_HAL_ARCH	//仅过检使用//!!!@
	#define _905_PROTOCOL	//仅过检使用//!!!@
	#define SUPPORT_905_DETECTION					//支持905过检

	#define PROP_PRO_TYPE_YWRTP_ON				// 有为传输协议

	#define FUNC_DATA_TRAN_EPU					// EPU命令数据传输支持

#elif (VDFS_DR_IPC_EXCHG_SCR_GONGAN == VDFS)		// 高清IPC数据中转（用于现有T4对接F8T） (公安项目)
	#define DXR_STOR_LOCAL_SUPPORT				// 本地存储媒体数据
	#define PROP_STOR_DATA_FETCH				// 提取磁盘数据属性+++

	#define DXR_RECORD_VIDEO_POWERON_AUTO_START	// 开机自动开启视频录像功能
	//#define DXR_TRAN_WIRELESS_SUPPORT			// 支持无线媒体传输功能
	#define DXR_MA_CTRL_ENABLE					// 支持MA控制

	#define PROP_PRO_TYPE_LOCALTRANS_ON			// 本地传输
	//#define PROP_PRO_TYPE_IPCSELF_ON			// IPC协议
	#define DXR_VIDEO_ONLY_LOCAL_TRANSFER		// 仅用于本地视频传输
	//#define DXR_VIDEO_IPC_ADAS				// IPC adas功能
	//#define SUPPORT_V3_IPC_ACCESS				// 支持V3-IPC接入

	#define PROP_PRO_TYPE_YWRTP_ON				// 有为传输协议

	#define DXR_IPC_SUPPORT_F8T						//ipc支持F8T

	#define FUNC_STORE_NORMAL_CAM_ENABLE		// 启用普通摄像头数据存储

	#define FUNC_DATA_TRAN_EPU					// EPU命令数据传输支持
#elif (VDFS_DR_IPC_STOR_SCR == VDFS)		// 高清IPC数据中转（用于现有F8T）
	#define DXR_STOR_LOCAL_SUPPORT				// 本地存储媒体数据
	#define PROP_STOR_DATA_FETCH				// 提取磁盘数据属性+++

	#define DXR_RECORD_VIDEO_POWERON_AUTO_START	// 开机自动开启视频录像功能
	//#define DXR_TRAN_WIRELESS_SUPPORT			// 支持无线媒体传输功能
	#define DXR_TRAN_SAME_UPC_SUPORT			// 类UPC传输会话功能支持

	//#define PROP_PRO_TYPE_LOCALTRANS_ON		// 本地传输
	#define PROP_PRO_TYPE_IPCSELF_ON			// IPC协议
	//#define DXR_VIDEO_ONLY_LOCAL_TRANSFER		// 仅用于本地视频传输
	//#define DXR_VIDEO_IPC_ADAS				// IPC adas功能
	#define SUPPORT_IPCCLIENT_HISTORY_FUN
	#define SUPPORT_V3_IPC_ACCESS				// 支持V3-IPC接入

	//#define FUNC_STORE_NORMAL_CAM_ENABLE		// 启用普通摄像头数据存储

	//#define FUNC_DATA_TRAN_EPU					// EPU命令数据传输支持
	#define SUPPORT_GET_USB_INFO				// 支持获取USB信息

#else
	#define DXR_STOR_LOCAL_SUPPORT				// 本地存储媒体数据
	#define PROP_STOR_DATA_FETCH				// 提取磁盘数据属性+++

	//#define DXR_RECORD_VIDEO_POWERON_AUTO_START	// 开机自动开启视频录像功能
	#define DXR_TRAN_WIRELESS_SUPPORT			// 支持无线媒体传输功能
	#define DXR_MA_CTRL_ENABLE					// 支持MA控制

	#define PROP_PRO_TYPE_LOCALTRANS_ON			// 本地传输
	//#define PROP_PRO_TYPE_IPCSELF_ON			// IPC协议(V3-IPC特有)
	//#define DXR_VIDEO_ONLY_LOCAL_TRANSFER		// 仅用于本地视频传输
	//#define DXR_VIDEO_IPC_ADAS				// IPC adas功能(V3-IPC特有)
	#define FUNC_EXPORT_TO_MP4					// 部标视频上传用的是mp4

	#define FUNC_STORE_NORMAL_CAM_ENABLE		// 启用普通摄像头数据存储

	#define FUNC_DATA_TRAN_EPU					// EPU命令数据传输支持
#endif 



#ifdef DXR_VIDEO_ONLY_LOCAL_TRANSFER		// 仅用于本地视频传输
	#define DXR_IPC_TRANSMODE                       //IPC传输模式
	#define DXR_IPC_QUERY_TIMEOUT                   //IPC查询超时
	//#define DXR_IPC_VIDEO_SW_OPTIM					//IPC视频切换优化
	//#define DXR_IPC_VIDEO_CRC_CHECK                 //IPC视频校验
	#define DXR_IPC_MULTI_HIST_TRANS                // IPC多路历史传输 实时和远程
	#define  DXR_SEND_STREAM_MODE					DXR_SEND_STREAM_USE_LOCAL_SOCKET
#endif

#ifdef ON_MTK_MT6735_ANDROID
#ifdef SUPPORT_905_DETECTION					//支持905过检
#	define SUPPORT_V3_MAINSTREAM					// 支持V3主码流存储
#	define _NOT_SUPPORT_INNER_CAM					// 不支持自身摄像头存储
#else
#	define _NOT_SUPPORT_905_DETECTION					//不支持905过检
#endif
#endif

#ifdef DXR_IPC_SUPPORT_F8T
#else
#	define DXR_IPC_NOT_SUPPORT_F8T						//ipc不支持F8T
#endif

#ifdef DXR_TRAN_WIRELESS_SUPPORT			// 支持无线媒体传输功能
	//#define PROP_PRO_TYPE_GBV808SZ_ON			// 开启深圳协议
	//#define PROP_PRO_TYPE_GBV808GJ_ON			// 公交标准 
	//#define PROP_PRO_TYPE_GBV808SD_ON			// 开启山东协议
	#define PROP_PRO_TYPE_GBV808SC_ON			// 协议支持四川标准
	#define PROP_PRO_TYPE_GBV808BB_ON			// 部标标准
	#define PROP_PRO_TYPE_GBV808JS_ON			// 支持江苏标准
	#define PROP_PRO_TYPE_YWRTP_ON				// 有为传输协议

	#define DXR_TRAN_WATCH_SUPPORT				// 支持企业守护功能

	#define PROP_FTP_UP_MP4						// 上传mp4文件
	#define PROP_BB_HIST_DOWN_OVER_APPEND		// 部标上传文件完毕后,还要再传一些附加信息,主要是给我司的平台用
#endif

#ifdef DXR_STOR_LOCAL_SUPPORT				// 本地存储媒体数据
	#define PROP_DXR3DOT0_BASE					// DXR3.0基本功能属性
	#define PROP_STOR_DISK_FORMAT				// 格式化磁盘属性+++
	#define PROP_STOR_DISK_LOAD					// 磁盘装载属性+++
	#define PROP_STOR_DATA_SAVE					// 存储数据属性+++
	#define PROP_MP4_UDISK_EXPORT				// U导出mp4属性
	#define SUPPORT_UDISK_FETCH					// 支持U盘媒体提取

	#define PROP_STOR_MIAN		1				// 支持主存
	#define PROP_STOR_ASSIST	1				// 支持辅存
	#define PROP_STOR_BACKUP	1				// 支持备份存储		暂未使用备份存储,注：开启备份存储时需要处理U盘作为备份存储和作为U盘使用时的冲突问题
	#define PROP_STOR_CACHE		1				// 支持内部Nand缓冲
#endif

#ifdef PROP_PRO_TYPE_GBV808GJ_ON			// 公交标准 
	#ifndef PROP_PRO_TYPE_GBV808SZ_ON			// 开启深圳协议
		#define PROP_PRO_TYPE_GBV808SZ_ON			// 开启深圳协议
	#endif
#endif

#ifndef PROP_STOR_DATA_FETCH			// 提取磁盘数据属性
	#ifdef SUPPORT_IPCCLIENT_HISTORY_FUN
		#undef SUPPORT_IPCCLIENT_HISTORY_FUN
	#endif

	#ifdef SUPPORT_IRA_FUN							// 支持红外功能
		#undef SUPPORT_IRA_FUN							// 支持红外功能
	#endif
	#ifdef SUPPORT_GUI_FUN							// 支持图像界面功能
		#undef SUPPORT_GUI_FUN							// 支持图像界面功能
	#endif
#endif

#ifdef PROP_DXR3DOT0_BASE					// DXR3.0基本功能属性
	//////////////////////////////////////
	/**ADD BY FENG***********************/
	// 换节处理
	#define DXR_CHANGE_SECTION
	// 检测BWR阻塞
	#define DXR_CHECK_BWRBLOCK
	// 录像相关事件
	#define DXR_RECORD_EVENT
	// 跳过装载中的坏扇区
	#define DXR_SKIP_BAD_SECTOR
	// 检测设备挂起
	#define DXR_CHECK_DEV_HANGUP
	// 删除应用搜索任意磁盘
	#define DXR_ANY_DISK_CUT

	// 查询段大小信息
	//#define DXR_ADD_FETCH_SEGSIZE
	#ifdef DXR_ADD_FETCH_SEGSIZE
		// 完善查询段信息中的段大小
		#define DXR_MOD_FETCH_SEGINFO
	#endif
	// 校验Cache
	#define FUNC_CHECK_CACHE_MEMORY
	// 异步合并段信息
	#define FUNC_MOD_ASYNC_MERGE_SEG
	// 实时更新内存中TSUM信息
	#define FUNC_REAL_UPDATE_TSUM
	// 兼容实时视频未由任务控制问题
	#define FUNC_COMPATIBLE_NO_MMSTMISSION
	// 使用回收器异步销毁search
	#define FUNC_ENABLE_SEARCH_DESTROY
	// 兼容提取异常组合包
	#define FUNC_COMPATIBLE_ABNORMAL_GPKG
	// 格式化失败重试机制
	#define FUNC_ADD_FORMAT_RETRY
	// 修改录像修复机制
	#define FUNC_MOD_REPAIR_COLLECT
	// 媒体索引分区
	//#define DXR_STOR_MEDIA_INDEX_ON
	#ifdef DXR_STOR_MEDIA_INDEX_ON
		// 调整DXAT分区大小
		#define FUNC_MOD_DXCB_SIZE
		// H264帧类型检测
		#define DXR_H264_FRM_TYPE_CHECK
		// 帧数据全校验 测试时开启
		//#define DXR_H264_FRM_WHOLE_CHECK
		// 组合包限制媒体时长（通常媒体时长限定在1.2秒内）
		#define DXR_GPK_LIMIT_TS
		// 支持媒体索引生成MP4头
		#define FUNC_SUPPORT_IDX_TO_MP4
		// 检测sps长度
		#define FUNC_SPS_CHECK
	#endif
/************************************/
//////////////////////////////////////
#endif

#ifdef _905_PROTOCOL
	#define DVRC_SEND_MEDIA_NEED_ACK
	#define FUNC_FETCHIMG_BY_EVENTID

	#define HISTORY_QUERY_GET_MEDIA_SIZE
#endif

#ifdef HISTORY_QUERY_GET_MEDIA_SIZE
	// 获取段大小信息（老的查询方式）
	#define DXR_OLD_FETCH_SEGSIZE
#endif

//#if !(defined(ON_MTK_MT6735_ANDROID)) || defined(ON_FSL_IMX6_ANDROID)
#define FUNC_FLOW_AND_MEDIA_11
//#endif

#ifdef FUNC_SUPPORT_VIDEO_SDK
	#ifndef PROP_PRO_TYPE_YWRTP_ON				// 开启有为传输协议
		#define PROP_PRO_TYPE_YWRTP_ON				// 开启有为传输协议	
	#endif	
#endif

#ifdef FUNC_SUPPORT_WITHOUT_MA
	#ifdef FUNC_DATA_TRAN_EPU 
		#undef FUNC_DATA_TRAN_EPU
	#endif
#endif

// 苏标必须开启如下宏
#ifdef PROP_PRO_TYPE_GBV808JS_ON
	#define FUNC_DESTROY_STORAGE_TEMP_FOR_USB // 苏标支持U盘升级
	#define NETVIRTUALUART	// 支持苏标的网络接口的ADAS及DSM设备,但这个功能与IPC是有冲突的,如果开启了这个IPC是没法正常使用的.
	#ifdef NETVIRTUALUART
		//#ifdef NETVIRTUALUART
		//	#undef NETVIRTUALUART
		//#endif
		#ifdef SUPPORT_IPC_FUN
			#undef SUPPORT_IPC_FUN				// 不支持V3IPC
		#endif
		#ifdef SUPPORT_EXTERNAL_IPC_FUN		
			#undef SUPPORT_EXTERNAL_IPC_FUN		// 不支持标准IPC
		#endif
	#endif
#endif

//#define PROP_PRO_TYPE_YUTONG // 宇通,部标,导出mp4,导出图片功能
#ifdef PROP_PRO_TYPE_YUTONG
	#define FUNC_DESTROY_STORAGE_TEMP_FOR_USB // 支持U盘升级
	#define FUNC_MOD_FETCH_PIC					// 提取图片不完整问题
	#define DXR_ADD_ALARM_WATERMARK			// 增加报警水印
#endif

//#define PROP_PRO_TYPE_SHEN_YANG // 郑州神阳
#ifdef PROP_PRO_TYPE_SHEN_YANG
	//// 查询段大小
	//#ifndef DXR_ADD_FETCH_SEGSIZE
	//	#define DXR_ADD_FETCH_SEGSIZE // 支持从自然段中截断数据查询,比如一个15分钟的段,现在只查其中的30秒,要把30秒的媒体字节数精确的返回
	//#endif
	//#define DXR_MOD_FETCH_SEGINFO // 完善查询段信息中的段大小
	//#ifdef FUNC_MOD_ASYNC_MERGE_SEG
	//	#undef FUNC_MOD_ASYNC_MERGE_SEG // 取消宏定义,不用子线程来合并段,而改用主线程
	//#endif
#endif

#ifdef FUNC_IS_IPCV3
// V3-IPC关闭对讲功能
#ifdef USE_FUNCTION_TALK_TEMP		// 实时对讲
	#undef USE_FUNCTION_TALK_TEMP		// 实时对讲
#endif
#endif

#ifdef SUPPORT_EXTERNAL_IPC_FUN
	#define ONLY_SUPPORT_ONE_EXT_IPC
	#define REAL_TRANS_USING_MAIN_STREAM
	#define IPC_MEDIA_DATA_TRANS_ON
	#define IPC_STOP_PLAY_TO_RECONNECT
	#define SUPPORT_DARWIN_RTSP						// 达尔文RTSP功能
	#define FUNC_EXTIPC_SNAP						// ipc抓拍功能
	#define FUNC_EXTIPC_FAST_RECOVER
#endif

//支持快速检测视频异常
#define FUNC_FAST_CHECK_VIDEO

//支持大帧大小控制重编码功能
#define FUNC_SUPPER_FRAME_CTRL

// 支持从MA获取通用信息
#define PROP_MA_COMM_INFO
// 支持部标信令链路功能
#ifdef PROP_PRO_TYPE_GBV808BB_ON
#define PROP_PRO_TYPE_GBV808BB_CMD_CTRL
#endif

// mp4生成器(CMemMp4)采用动态内存方式,而取代以前固定的2M内存
#define MEMMP4_DYNAMIC_MEM

//֧������gpio��ʼ����(eep)
#define FUN_EEP_GPIO_TABLE

// 保持音频播放常开
#define FUNC_KEEP_AUDIO_OPEN

#define FUMC_MMST_BALANCE_SEND // 实时视频的均衡发送
// 增加上报磁盘检测信息
//#define FUNC_ADD_WATCH_DISK

#define FUNC_USE_TRANSFER_LIB // 使用Transfer库
//#define FUNC_USE_MPM_LIB // 使用mpm库

//#define DXR_WATCH_ADD_MEDIUM_DETECT			// 守护介质检测
//#define FUNC_FETCH_FAIL_CB		// 提取失败回调
#define FUN_ADD_DISCONN_PROP	// 增加断开连接属性

#define DXR_WATCH_EXTENTION						// 企业守护扩充

#define FUNC_MOUNT_USB			// 挂载usb设备 k7v40上使用
//#define FUNC_MOD_GPKG_COMMIT	// 改造单包提交为多包同时提交

#ifdef DXR_TRAN_WATCH_SUPPORT
#define FUNC_ADD_WATCH_DOUBLE_MNG		// 增加企业守护双通道管理
#endif

#define FUNC_DISABLE_MOD_MP4_TIME		// 禁用修改MP4段时间(根据事件分段功能已失效，禁用根据事件控制时长、起始时间、结束时间)