#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "key.h"
//#include "lwip_comm.h"
//#include "LAN8720.h"
#include "usmart.h"
#include "timer.h"
#include "lcd.h"
#include "sram.h"
#include "malloc.h"
//#include "lwip_comm.h"
#include "includes.h"
#include "24cxx.h"
//#include "lwipopts.h"
//#include "tcp_client_demo.h"
#include "acclib.h"
//#include "cJSON.h"
#include "main.h"
#include "stdlib.h"

//Copyright@acc--20160126
//集总商用化灯控系统控制器V4.0,设备信息烧写调试程序.

const u8 dev_infomation[] = "FF SmartHome Light,CopyRight by acc";

//任务优先级、堆栈大小、堆栈大小
#define KEY_TASK_PRIO 8 //KEY任务
#define KEY_STK_SIZE 128 //任务堆栈大小
OS_STK KEY_TASK_STK[KEY_STK_SIZE]; //任务堆栈

#define LED_TASK_PRIO 9 //LED任务
#define LED_STK_SIZE 64 //任务堆栈大小
OS_STK	LED_TASK_STK[LED_STK_SIZE]; //任务堆栈

#define DISPLAY_TASK_PRIO 10 //在LCD上显示地址信息任务
#define DISPLAY_STK_SIZE 128 //任务堆栈大小
OS_STK	DISPLAY_TASK_STK[DISPLAY_STK_SIZE]; //任务堆栈

#define START_TASK_PRIO 11 //START任务
#define START_STK_SIZE 128 //任务堆栈大小
OS_STK START_TASK_STK[START_STK_SIZE]; //任务堆栈

#define MAIN_TASK_PRIO 12 //主任务
#define MAIN_STK_SIZE 128 //堆栈大小
OS_STK MAIN_TASK_STK[MAIN_STK_SIZE]; //堆栈函数

//各个任务函数
void key_task(void *pdata);
void led_task(void *pdata);
void display_task(void *pdata);
void start_task(void *pdata);
void main_task(void *pdata);
/*************************************************/
u8 tick = 0;
u8 SwitchStatNum = 0;
//extern u8 SwitchStat;

// struct DevInfo *RealDevInfo, RealDevInfo_t;
// struct SubDevInfo *LamDevInfo, LamDevInfo_t;

//extern OS_EVENT * q_msg;
OS_TMR *tmr1; //定时器1
OS_TMR *tmr2; //定时器2
//OS_TMR   * tmr3;
//OS_FLAG_GRP * flags_key;
void *MsgGrp[500];

void tmr1_callback(OS_TMR *ptmr,void *p_arg);
void tmr2_callback(OS_TMR *ptmr,void *p_arg);

void dev_init(void);
void ReadDevInfo(cDevInfo *dev);

u8 net_config(void);

//在LCD上显示地址信息
//mode:1 显示DHCP获取到的地址
//	  其他 显示静态地址
void show_address(u8 mode)
{

}

int main(void)
{

	delay_init(168);       	//延时初始化
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);	//中断分组配置
	uart_init(115200);    	//串口波特率设置
	usmart_dev.init(84); 	//初始化USMART
	LED_Init();  			//LED初始化
	KEY_Init();  			//按键初始化
	LCD_Init();  			//LCD初始化

	FSMC_SRAM_Init();		//SRAM初始化

	AT24CXX_Init(); //E2PROM初始化

	mymem_init(SRAMIN);  	//初始化内部内存池
	mymem_init(SRAMEX);  	//初始化外部内存池
	mymem_init(SRAMCCM); 	//初始化CCM内存池

	POINT_COLOR = RED; 		//红色字体
	LCD_ShowString(30,30,200,20,16,"Explorer STM32F4");
	LCD_ShowString(30,50,200,20,16,"TCP CLIENT NETCONN Test");
	LCD_ShowString(30,70,200,20,16,"ATOM@ALIENTEK");
	LCD_ShowString(30,90,200,20,16,"KEY0:Send data");
	LCD_ShowString(30,110,200,20,16,"2014/9/1");
	POINT_COLOR = BLUE; //蓝色字体

	while(AT24CXX_Check())
	{
		printf("AT24CXX Check Error!\n");
		delay_ms(500);
		LED1=!LED1;//DS1闪烁
		delay_ms(500);
	}
	dev_init();

	OSInit(); //UCOS初始化

	OSTaskCreate(start_task,(void*)0,(OS_STK*)&START_TASK_STK[START_STK_SIZE-1],START_TASK_PRIO);
	OSStart(); //开启UCOS
}
void dev_init(void)
{
	u16 res, res1;
	u8 *p = NULL;
	u8 *p_t = NULL;
	u8 *p2 = NULL;
	u8 *tmp, *tmp1;

	struct cDevInfo *Info_t,Info_tt; /*p是空指针，你在结构体中 继续定义 一个b ，p指向b就ok*/
	MD5_CTX md5;
	u8 decrypt[16];
	u16 i, NumCh = 0;

	// u8 DevMAC_t[7] = {0x04, 0x2D, 0xB4, 0x00, 0x00, 0x00};
	// u8 SubDevLightStatus_t[5]= {0x04, 0x2D, 0xB4, 0x02};
	u8 DevIP_t[13]= {192, 168, 1, 120, 255, 255, 255, 0, 192, 168, 1, 120};
	u8 ServerIP_t[5] = {192, 168, 1, 120};

	//申请数据内存，插入设备信息
	tmp = mymalloc(SRAMEX, 500);	//申请13个字节的内存
	res = AddStringToDevInfo(tmp, "DevName", "DYSS", 0); //公司名称
	res = strlen((char *)tmp);
	AddStringToDevInfo((tmp + res), "DevID", "SLM011601700001", 0); //设备ID
	res = strlen((char *)tmp);
	// AddArrayToDevInfo((tmp + res), "DevMAC", DevMAC_t, 6); //设备MAC
	AddStringToDevInfo((tmp + res), "DevMAC", "042DB4000000", 0);	
	res = strlen((char *)tmp);
	AddStringToDevInfo((tmp + res), "DevType", "SML", 0); //设备类型
	res = strlen((char *)tmp);
	AddNumberToDevInfo((tmp + res), "SubDevQual", 5); //子设备数量
	res = strlen((char *)tmp);
	AddArrayIPToDevInfo((tmp + res), "DevIP", DevIP_t, 12); //设备IP信息
	res = strlen((char *)tmp);
	AddNumberToDevInfo((tmp + res), "DevPort", 12000); //设备端口号
	res = strlen((char *)tmp);
	AddArrayIPToDevInfo((tmp + res), "SerIP", ServerIP_t, 4); //服务器IP信息
	res = strlen((char *)tmp);
	AddNumberToDevInfo((tmp + res), "SerPort", 8082); //服务器端口号
	res = strlen((char *)tmp);
	// AddArrayToDevInfo((tmp + res), "SubDevLiStat", SubDevLightStatus_t, 4);
	AddStringToDevInfo((tmp + res), "SubLamStat", "11111111111111111111111111111111", 0);

	res = strlen((char *)tmp); //计算出有效数据串的真正长度

	AT24CXX_Write(0, "DYSS", 4); //头
	delay_ms(1);
	AT24CXX_WriteOneByte(4, res / 256); //字节高
	AT24CXX_WriteOneByte(5, res % 256); //字节低

	res = strlen((char *)tmp);
	// printf("OriData:%s\nLength:%d\n", tmp, res);

	MD5Init(&md5);
	MD5Update(&md5, tmp, res); //MD5校验
	MD5Final(&md5, decrypt); //MD5解析

	res = CRC_DataOr(tmp, res, 0); //计算要写入数据的长度
	AT24CXX_Write(6, tmp, res); //写入数据

	res1 = CRC_DataOr(decrypt, 16, 0);
	
	// for(i = 0; i < 16; i++)
		// printf("MD5[%d]:%02x\n", i,decrypt[i]); //decrypt直接存放了MD5串
	
	AT24CXX_Write(6 + res, decrypt, res1); //写入数据

	delay_ms(200);
	/**********************************************************************/
	tmp1 = mymalloc(SRAMEX, 500);	//申请13个字节的内存

	AT24CXX_Read(0, tmp1, 4);
	if(!(strncmp((char *)tmp1, "DYSS", 4) == 0))
	{
		printf("Device'Company:%s Error!\n", tmp1);
		while(1);
	}
	else
	{
		res = (AT24CXX_ReadOneByte(4) * 256) + AT24CXX_ReadOneByte(5);
		printf("Device'Company is DYSS!\n");
		printf("Device'Information DataLength:%d!\n", res);
		AT24CXX_Read(6, tmp1, res);
		res = CRC_DataOr(tmp1, res, 0);
		tmp1[res] = '\0';

		strcpy((char *)tmp, (char *)tmp1);
		res = strlen((char *)tmp);
		// printf("NowData:%s\nLength:%d\n", tmp, res);

		AT24CXX_Read(6 + res, tmp1, 16);

		res1 = CRC_DataOr(tmp1, 16, 0); //
		// for(i = 0; i < 16; i++)
		// printf("MMD5[%d]:%02x\n", i, tmp1[i]); //		
		printf("OriMD5:");
		for(i = 0; i < 16; i++)
		{
			if(i == 15)
				printf("%02x\n", tmp1[i]); //
			else
				printf("%02x", tmp1[i]); //
		}
		tmp1[i] = '\0';

		MD5Init(&md5);
		res = strlen((char *)tmp);
		MD5Update(&md5, tmp, res); //MD5校验
		MD5Final(&md5, decrypt); //MD5解析

		// for(i = 0; i < 16; i++)
		// printf("MD5[%d]:%02x\n", i, decrypt[i]); //打印解密后MD5串
		printf("NowMD5:");
		for(i = 0; i < 16; i++)
		{
			if(i == 15)
				printf("%02x\n", decrypt[i]); //
			else
				printf("%02x", decrypt[i]); //
		}

		if(strncmp((char *)tmp1, (char *)decrypt, 16) != 0)
		{
			printf("Device'Company-MD5 Error!\n");
			while(1);
		}
		else
		{
			printf("Device'Company-MD5 Success!\n");

			Info_t = &Info_tt;
			p_t = tmp;

			strncat((char *)tmp1, (char *)tmp, strlen((char *)tmp));
			p = tmp1;
			NumCh = Calc_CharNum(tmp, '&');

			p = ParseToDevInfoEle(p);//, 1)
			if(!p)
				printf("NULL:%s", tmp1);
			else
			{
				p2 = acc_strchr(p, '=');
				// printf("Value:%s\n", ++p2);

				Info_t->valuestring = ++p2;
				// printf("\nInfo_t.valuestring:%s\n", Info_t->valuestring);

				res = strlen((char *)p);
				*(p + res - strlen((char *)p2) - 1) = '\0';
				Info_t->str = p;
				// printf("Info_t.str:%s\n", Info_t->str);

				ReadDevInfo(Info_t);
			}

			for(i = 1; i < NumCh; i++)
			{
				//p_t = p_t + strlen(p) + 1;
				p_t = p_t + res + 1;

				strncat((char *)tmp1, (char *)p_t, strlen((char *)p_t));

				p = tmp1;

				p = ParseToDevInfoEle(p);//, 1)
				if(!p)
				{
					printf("NULL:%s", tmp1);
					break;
				}
				else
				{
					p2 = acc_strchr(p, '=');
					// printf("Value:%s\n", ++p2);

					Info_t->valuestring = ++p2;
					// printf("\nInfo_t.valuestring:%s\n", Info_t->valuestring);

					res = strlen((char *)p);
					*(p + res - strlen((char *)p2) - 1) = '\0';
					Info_t->str = p;
					// printf("Info_t.str:%s\n", Info_t->str);

					ReadDevInfo(Info_t);
				}
			}
		}
	}
	myfree(SRAMEX, tmp);
	myfree(SRAMEX, tmp1);
}
/*
	u8 DevName[5]; //公司名称
	u8 DevID[17]; //设备ID号
	u8 DevMAC[7]; //设备的MAC
	u8 DevType[4]; //设备类型

	u8 DevIP[13]; //设备IP地址信息
	u8 SerIP[5]; //设备服务器地址信息

	u8 DataTpye; //设备服务状态信息
	u16 SubDevQual;	//子设备个数

	u16 DevPort; //设备端口号
	u16 SerPort; //设备服务器端口号

	u8 DevCtrlFlag; //标识设备控制方式（数量）
*/
void ReadDevInfo(cDevInfo *dev)
{
	u16 i;

//	if(!(strncmp((char *)tmp1, (char *)decrypt1, strlen((char *)decrypt1)) == 0))
	if(strcmp((char *)dev->str, "DevName") == 0)
	{
		strcpy((char *)DevInfoV4.DevName, (char *)dev->valuestring);
		printf("DevInfoV4.DevName:%s\n", DevInfoV4.DevName);
	}
	else if(strcmp((char *)dev->str, "DevID") == 0)
	{
		strcpy((char *)DevInfoV4.DevID, (char *)dev->valuestring);
		printf("DevInfoV4.DevID:%s\n", DevInfoV4.DevID);
	}
	else if(strcmp((char *)dev->str, "DevMAC") == 0)
	{
		// strcpy((char *)DevInfoV4.DevMAC, (char *)dev->valuestring);
		// printf("DevInfoV4.DevMAC:%s\n", DevInfoV4.DevMAC);
		Char2Hex(DevInfoV4.DevMAC, (char *)dev->valuestring, 12);
		printf("DevInfoV4.DevMAC:");
		for(i = 0; i < 6; i++)
		{
			if(i == 5)
				printf("%02X\n", DevInfoV4.DevMAC[i]);
			else
				printf("%02X-", DevInfoV4.DevMAC[i]);
		}
	}
	else if(strcmp((char *)dev->str, "DevType") == 0)
	{
		strcpy((char *)DevInfoV4.DevType, (char *)dev->valuestring);
		printf("DevInfoV4.DevType:%s\n", DevInfoV4.DevType);
	}
	else if(strcmp((char *)dev->str, "SubDevQual") == 0)
	{
//		strcpy((char *)DevInfoV4->SubDevQual, (char *)dev->valuestring);
//		printf("DevInfoV4->SubDevQual:%s\n", DevInfoV4->SubDevQual);
		DevInfoV4.SubDevQual = atoi((char *)dev->valuestring);
		printf("DevInfoV4.SubDevQual:%d\n", DevInfoV4.SubDevQual);
	}
	else if(strcmp((char *)dev->str, "DevIP") == 0)
	{
		// strcpy((char *)DevInfoV4.DevIP, (char *)dev->valuestring);
		// printf("DevInfoV4.DevIP:%s\n", DevInfoV4.DevIP);
		Char2IP(DevInfoV4.DevIP, dev->valuestring, strlen((char *)dev->valuestring), 1);
		printf("DevInfoV4.DevIP:");
		for(i = 0; i < 12; i++)
		{
			if(i == 11)
				printf("%d\n", DevInfoV4.DevIP[i]);
			else
				printf("%d.", DevInfoV4.DevIP[i]);
		}
	}
	else if(strcmp((char *)dev->str, "DevPort") == 0)
	{
		// strcpy((char *)DevInfoV4.DevPort, (char *)dev->valuestring);
//		printf("DevInfoV4.DevPort:%s\n", DevInfoV4.DevPort);
		DevInfoV4.DevPort = atoi((char *)dev->valuestring);
		printf("DevInfoV4.DevPort:%d\n", DevInfoV4.DevPort);
	}
	else if(strcmp((char *)dev->str, "SerIP") == 0)
	{
		// strcpy((char *)DevInfoV4.SerIP, (char *)dev->valuestring);
		// printf("DevInfoV4.SerIP:%s\n", DevInfoV4.SerIP);
		Char2IP(DevInfoV4.SerIP, dev->valuestring, strlen((char *)dev->valuestring), 2);
		printf("DevInfoV4.SerIP:");
		for(i = 0; i < 4; i++)
		{
			if(i == 3)
				printf("%d\n", DevInfoV4.SerIP[i]);
			else
				printf("%d.", DevInfoV4.SerIP[i]);
		}
	}
	else if(strcmp((char *)dev->str, "SerPort") == 0)
	{
//		strcpy((char *)DevInfoV4.SerPort, (char *)dev->valuestring);
		DevInfoV4.SerPort = atoi((char *)dev->valuestring);
		printf("DevInfoV4.SerPort:%d\n", DevInfoV4.SerPort);
	}
	else if(strcmp((char *)dev->str, "SubLamStat") == 0)
	{
		printf("Lam-NumLength:%d\n", strlen((char *)dev->valuestring));
		for(i = 0; i < strlen((char *)dev->valuestring); i++)
		{
			if(*(dev->valuestring + i) == '1')
			{
				SubLamDevV4.SubStatus[i / 8] |= (1 << (7 - (i % 8)));
				// printf("SubLamDevV4[%d].SubStatus:%02X\n", i / 8, SubLamDevV4.SubStatus[i / 8]);
			}
			else if(*(dev->valuestring + i) == '0')
			{
				SubLamDevV4.SubStatus[i / 8] &= ~(1 << (7 - (i % 8)));
				// printf("SubLamDevV4[%d].SubStatus:%02X\n", i / 8, SubLamDevV4.SubStatus[i / 8]);
			}
			else {}
		}
		if(strlen((char *)dev->valuestring) % 8)
		{
			for(i = 0; i <= (strlen((char *)dev->valuestring) / 8); i++)
				printf("SubLamDevV4.SubStatus[%d]:%02X\n", i, SubLamDevV4.SubStatus[i]);
		}
		else
		{
			for(i = 0; i < (strlen((char *)dev->valuestring) / 8); i++)
				printf("SubLamDevV4.SubStatus[%d]:%02X\n", i, SubLamDevV4.SubStatus[i]);

		}
		// strcpy((char *)SubLamDevV4.SubStatus, (char *)dev->valuestring);
		// printf("SubLamDevV4.SubStatus:%s\n", SubLamDevV4.SubStatus);
	}
	else {}
	
	DevInfoV4.DataTpye = 0;
	DevInfoV4.DevCtrlFlag = 0;
}
//start任务
void start_task(void *pdata)
{
	u8 err;

	OS_CPU_SR cpu_sr;
	pdata = pdata ;
//	q_msg = OSQCreate(&MsgGrp[0],256); //创建消息队列

	OSStatInit(); //初始化统计任务
	OS_ENTER_CRITICAL(); //关中断

//	OSTaskCreate(main_task,(void *)0,(OS_STK*)&MAIN_TASK_STK[MAIN_STK_SIZE-1],MAIN_TASK_PRIO);

	//创建LED任务
	OSTaskCreate(led_task,(void*)0,(OS_STK*)&LED_TASK_STK[LED_STK_SIZE-1],LED_TASK_PRIO);
//	OSTaskCreate(display_task,(void*)0,(OS_STK*)&DISPLAY_TASK_STK[DISPLAY_STK_SIZE-1],DISPLAY_TASK_PRIO); //显示任务

	OSTaskCreate(key_task,(void*)0,(OS_STK*)&KEY_TASK_STK[KEY_STK_SIZE-1],KEY_TASK_PRIO);

	tmr1=OSTmrCreate(10,100,OS_TMR_OPT_PERIODIC,(OS_TMR_CALLBACK)tmr1_callback,0,"tmr1",&err);		//100ms
	OSTmrStart(tmr1,&err);

	OSTaskSuspend(OS_PRIO_SELF); //挂起start_task任务
	OS_EXIT_CRITICAL();  //开中断
}
u8 net_config(void)
{
	
	return 0;
}
//主任务，主要解决系统刚上电不插网线，系统进入死循环的BUG
void main_task(void *pdata)
{
	while(1)
	{
		net_config();
	}
}
//显示地址等信息的任务函数
void display_task(void *pdata)
{
	while(1)
	{
#if LWIP_DHCP									//当开启DHCP的时候
		if(lwipdev.dhcpstatus != 0) 			//开启DHCP
		{
			show_address(lwipdev.dhcpstatus );	//显示地址信息
			OSTaskSuspend(OS_PRIO_SELF); 		//显示完地址信息后挂起自身任务
		}
#else
		show_address(0); 						//显示静态地址
		OSTaskSuspend(OS_PRIO_SELF); 			//显示完地址信息后挂起自身任务
#endif //LWIP_DHCP
		OSTimeDlyHMSM(0,0,0,500);
	}
}


/*
key任务
	接受网络数据后进行数据分析--后期加Json处理
	IO检测处理
	串口2数据处理
*/
void key_task(void *pdata)
{
	u8 key;//, err, result;
//	u8 *msg_p;
//	u16 tmp_switch[2];

	while(1)
	{
		key = KEY_Scan(0);
		if(key==KEY0_PRES) //发送数据
		{
			//tcp_client_flag |= LWIP_SEND_DATA; //标记LWIP有数据要发送
//			SwitchStat |= 0x40;
		}

//		//处理网络数据包信息
//		msg_p = mymalloc(SRAMEX, 500);	//申请13个字节的内存
//		if(msg_p)
//		{
//			msg_p = OSQPend(q_msg,0,&err);//请求消息队列
//			LCD_ShowString(30,250,210,16,16,msg_p);//显示消息

//			//开中断之前处理网络接收的数据
//			if (strncmp("success", (char *)msg_p, strlen("success")) == 0)
//			{
//				//		DataConfiged = 1;
//				if (!(SwitchStat & 0x80))
//				{
//					SwitchStatNum++;
//					if (SwitchStatNum > 2) //用于心跳链接计数，避免假在线
//					{
//						SwitchStat |= 0x80; //清除重连位
//						SwitchStatNum = 0;
//					}
//				}
//				else if (SwitchStat & 0x20)
//					SwitchStat &= 0xdf; //清除控制位
//				else if (SwitchStat & 0x40)
//				{
//					SwitchStat &= 0xbf; //清除更新位
//					//flag_ctrl = 0x01;
//				}
//				else if (SwitchStat & 0x10)
//					SwitchStat &= ~0x10;
//				else
//				{
//				}
//			}
//			else if (strncmp("error", (char *)msg_p, strlen("error")) == 0)
//			{
//				//		DataConfiged = 0;
//				if (!(SwitchStat & 0x80))
//					SwitchStat &= 0x7f; //重新标识重连位
//				else if (SwitchStat & 0x20)
//					SwitchStat |= 0x20; //重新标识控制位
//				else if (SwitchStat & 0x40)
//					SwitchStat |= 0x40; //重新标识控制位
//				else if (SwitchStat & 0x10)
//					SwitchStat |= 0x10;
//			}

//			/*
//			 格式：		head_mac_devtype_datatype_content\n
//			 状态反馈：	deng_mac_8_1/2_xxxxx\n
//			 (x表示每一路的状态，可能是16路，也可能是24路)
//			 控制命令：	deng_0_x_x\n
//			 (x_x表示第几路开/关)
//			 */
//			//deng_0_11_1\n
//			//64 65 6E 67 5F 30 5F 31 31 5F 31 0a
////			else if ((strncmp("deng_0_", (char *)msg_p, strlen("deng_0_")) == 0) && (msg_p[data_len - 1] == '\n'))
//			else if ((strncmp("deng_0_", (char *)msg_p, strlen("deng_0_")) == 0) && (msg_p[strlen((char *)msg_p) - 1] == '\n'))
//			{
//				if (strlen((char *)msg_p) >= 11) //保证数据的格式最小字数
//				{
//					result = Check_Num(tmp_switch, msg_p + strlen("deng_0_"), strlen((char *)msg_p) - strlen("deng_0_"));
//					if (result != 1)
//					{

//						// dev_light_which[0] = (u8) tmp_switch[0]; //第几路
//						// dev_light_which[1] = (u8) tmp_switch[1]; //开/关
//						// SwControl = SwitchCtrl; //处理完数据后，要对控制位清零
//					}
//				}
//			}
//			//deng_6_1_1\n
//			else if ((strncmp("deng_6_", (char *)msg_p, strlen("deng_6_")) == 0) && (msg_p[strlen((char *)msg_p) - 1] == '\n'))
//			{
//				if(strlen((char *)msg_p) >= 11) //保证数据的格式最小字数
//				{
//					result = Check_Num(tmp_switch, msg_p + strlen("deng_6_"), strlen((char *)msg_p) - strlen("deng_6_"));
//					if (result != 1)
//					{
//						// dev_cl_which_tmp[0] = (u8) tmp_switch[0]; //第几路
//						// dev_cl_which_tmp[1] = (u8) tmp_switch[1]; //开/关
//						// SwControl = ClCtrl; //处理完数据后，要对控制位清零
//					}
//				}
//			}
//			else {}

//			myfree(SRAMEX, msg_p);
//			//delay_ms(500);
//		}
		OSTimeDlyHMSM(0,0,0,10);  //延时10ms
	}
}

//led任务
void led_task(void *pdata)
{
	while(1)
	{
		LED0 = !LED0;
		OSTimeDlyHMSM(0,0,0,500);  //延时500ms
	}
}

/*
软件定时器1的回调函数
每100ms执行一次，检测是否有数据更新到服务器
*/
void tmr1_callback(OS_TMR *ptmr,void *p_arg)
{
//	if(!(SwitchStat & 0x80)) //数据包
//	{
//		if(tick >= 1)
//		{
////			tcp_client_flag |= LWIP_SEND_DATA; //标记LWIP有数据要发送;
//			//printf("Data ok\n");
//			tick = 0;
//		}
//	}
//	else
//	{
//		if((SwitchStat & 0x40)) //数据更新包
//		{
//			if(tick >= 1)
//			{
////				tcp_client_flag |= LWIP_SEND_DATA; //标记LWIP有数据要发送;
//				printf("Update ok\n");
//				tick = 0;
//			}
//		}
//		else //心跳包
//		{
//			if(tick >= 10)
//			{
////				tcp_client_flag |= LWIP_SEND_DATA; //标记LWIP有数据要发送;
//				//printf("Tick ok\n");
//				tick = 0;
//			}
//		}
//	}
//	tick++;
}

void tmr2_callback(OS_TMR *ptmr,void *p_arg)
{

}
