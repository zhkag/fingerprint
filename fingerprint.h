/*
 * @Author: Aurora-zk
 * @Date: 2021-03-14 12:39:09
 * @LastEditors: Aurora-zk
 * @LastEditTime: 2021-10-05 20:39:57
 * @contact: pk-ing@nyist.edu.cn
 * @FilePath: \fingerprint\fingerprint.h
 */

#ifndef _FINGERPRINT_H
#define _FINGERPRINT_H

#include "rtthread.h"
#include <rtdevice.h>
#include <rtdef.h>
#include <string.h>

#define HEAD_SIZE         2U
#define PREFIX_SIZE       9U

#define FP_HEAD_BIT       0U  /* 包头位 */
#define FP_ADDR_BIT       2U  /* 地址位 */
#define FP_TOK_BIT        6U  /* 标识位 */
#define FP_LEN_BIT        7U  /* 长度位 */

#define FP_INS_CMD_BIT    9U  /* 指令包指令码位 */
#define FP_INS_PAR_BIT(n) ((n)+10U) /* 指令包参数位 */
#define FP_INS_SUM_BIT(n) FP_INS_PAR_BIT(n) /* 指令包校验和位 */

#define FP_REP_ACK_BIT(n) ((n)+9U)  /* 应答包确认码位 */
#define FP_REP_SUM_BIT(n) FP_REP_ACK_BIT(n) /* 应答包校验和位 */


#define CharBuffer1 0x01
#define CharBuffer2 0x02
#define CharBuffer3 0x03
#define CharBuffer4 0x04

#define FP_HEAD_H     0xEF    /* 包头为 0xEF01 */
#define FP_HEAD_L     0x01
#define FP_ADDR_0     0xFF    /* 默认地址为 0xFFFFFFFF */
#define FP_ADDR_1     0xFF
#define FP_ADDR_2     0xFF
#define FP_ADDR_3     0xFF
#define FP_TOK_CMD    0x01
#define FP_TOK_DAT    0x02
#define FP_TOK_END    0x08

/* fp 系列命令列表 */
#define FP_CMD_GET_IMG              0x01    /* 从传感器上读入图像存于图像缓冲区 */
#define FP_CMD_GEN_CHR              0x02    /* 根据原始图像生成指纹特征存于 CharBuffer1 、 CharBuffer2、CharBuffer1 、 CharBuffer2 */
//#define FP_CMD_MATCH      0x03 /* 精确比对 CharBuffer1 与 CharBuffer2 中的特征文件 */
#define FP_CMD_SEARCH               0x04    /* 以特征文件搜索整个或部分指纹库 */
#define FP_CMD_REG_MODEL            0x05    /* 将生成的特征文件合并生成新模板存于特征文件缓冲区 */
#define FP_CMD_STR_CHR              0x06    /* 将特征缓冲区中的文件储存到 flash 指纹库中 */
#define FP_CMD_FINGER_CHAR_UP       0x07    /* 特征上传 */                                                      //暂无
#define FP_CMD_FINGER_CHAR_DOWN     0x08    /* 特征下载 */                                                      //暂无
//#define FP_CMD_DOWN_CHR   0x09 /* 从上位机下载一个特征文件到特征缓冲区 */
#define FP_CMD_UP_IMG               0x0A    /* 上传原始图像 */                                                      //暂无
//#define FP_CMD_DOWN_IMG   0x0B /* 下载原始图像 */
#define FP_CMD_DEL_CHR    0x0C /* 删除 flash 指纹库中的一个特征文件 */
#define FP_CMD_EMPTY      0x0D /* 清空 flash 指纹库 */
#define FP_CMD_FINGER_MOUDLE_SET    0x0E    /* 设置模组参数 */
#define FP_CMD_RD_PARAM   0x0F /* 读系统基本参数 */
//#define FP_CMD_ENROLL     0x10 /* 注册模板 */
//#define FP_CMD_IDENTIFY   0x11 /* 验证指纹 */
#define FP_CMD_SET_PWD              0x12    /* 设置设备握手口令 */
#define FP_CMD_VFY_PWD              0x13    /* 验证设备握手口令 */
//#define FP_CMD_GET_RAMD   0x14 /* 采样随机数 */
//#define FP_CMD_SET_ADDR   0x15 /* 设置芯片地址 */
#define FP_CMD_READ_SYS_PARA        0x16    /* 读取系统基本参数 */
//#define FP_CMD_PORT_CNTL  0x17 /* 通讯端口（UART/USB）开关控制 */
//#define FP_CMD_WR_NOTEPAD 0x18 /* 写记事本 */
//#define FP_CMD_RD_NOTEPAD 0x19 /* 读记事本 */
//#define FP_CMD_BURN_CODE  0x1A /* 烧写片内 FLASH */
//#define FP_CMD_HS_SEARCH  0x1B /* 高速搜索 FLASH */
//#define FP_CMD_GEN_BIN_IM 0x1C /* 生成二值化指纹图像 */

#define FP_CMD_VALID_TEMPLETE_NUM   0x1D    /* 读取有效模板个数 */
#define FP_CMD_READ_INDEX_TABLE     0x1F    /* 读索引表 */

#define FP_CMD_GET_RNROLL_IMG       0x29    /* 注册用获取图像 */
#define FP_CMD_CANCEL               0x30    /* 取消指令 */
#define FP_CMD_AUTO_ENROLL          0x31    /* 自动注册模板 */
#define FP_CMD_AUTO_IDENTIFY        0x32    /* 自动验证指纹 */
#define FP_CMD_SLEEP                0x33    /* 休眠指令（还可以使用0X60） */
#define FP_CMD_GET_CHIP_UID         0x34    /* 获取芯片序列号  */
#define FP_CMD_GET_CHIP_ECHO        0x35    /* 握手命令  */
#define FP_CMD_AUTO_CAI_SENSOR      0x36    /* 校验传感器  */

/**
 * @brief 定义了指令应答的确认码
 * 
 */
enum fp_ack_type
{
    CMD_OK        = 0x00, /* 表示指令执行完毕或 OK */
    DAT_ERR       = 0x01, /* 表示数据包接收错误 */
    NO_FINGER     = 0x02, /* 表示传感器上没有手指 */
//    INPUT_FIN     = 0x03, /* 表示录入指纹图像失败 */
//    TOO_DRY       = 0x04, /* 表示指纹图像太干、太淡而生不成特征 */
//    TOO_WET       = 0x05, /* 表示指纹图像太湿、太糊而生不成特征 */
    TOO_MESS      = 0x06, /* 表示指纹图像太乱而生不成特征 */
    FEATURE_PT    = 0x07, /* 表示指纹图像正常，但特征点太少（或面积太小）而生不成特征 */
//    FINGER_MIS    = 0x08, /* 表示指纹不匹配 */
    SERCH_ERR     = 0x09, /* 表示没搜索到指纹 */
    MERGE_FAIL    = 0x0A, /* 表示特征合并失败 */
    OVER_SIZE     = 0x0B, /* 表示访问指纹库时地址序号超出指纹库范围 */
//    RD_TMPL_ERR   = 0x0C, /* 表示从指纹库读模板出错或无效 */
//    UP_FEAT_ERR   = 0x0D, /* 表示上传特征失败 */
//    NO_SUBSEQ_PKG = 0x0E, /* 表示模块不能接受后续数据包 */
//    UP_IMG_ERR    = 0x0F, /* 表示上传图像失败 */
    DEL_TMPL_ERR  = 0x10, /* 表示删除模板失败 */
    CLR_FP_ERR    = 0x11, /* 表示清空指纹库失败 */
//    LP_MODE_ERR   = 0x12, /* 表示不能进入低功耗状态 */
    PWD_ERR       = 0x13, /* 表示口令不正确 */
//    SYS_RST_ERR   = 0x14, /* 表示系统复位失败 */
    BUF_NO_IMG    = 0x15, /* 表示缓冲区内没有有效原始图而生不成图像 */
//    OTA_ERR       = 0x16, /* 表示在线升级失败 */
    RESIDUAL_FP   = 0x17, /* 表示残留指纹或两次采集之间手指没有移动过 */
    RW_FLASH_ERR  = 0x18, /* 表示读写 FLASH 出错 */
    UNDEF_ERR     = 0x19, /* 未定义错误 */
    INVALID_REG   = 0x1A, /* 无效寄存器号 */
    CFG_REG_ERR   = 0x1B, /* 寄存器设定内容错误号 */
//    NP_PAGE_ERR   = 0x1C, /* 记事本页码指定错误 */
//    PORT_CNTL_ERR = 0x1D, /* 端口操作失败 */
    ENROLL_ERR    = 0x1E, /* 自动注册（enroll）失败 */
    FP_FULL       = 0x1F, /* 指纹库满 */
    ADDR_ERR    = 0x20, /* 地址错误 */
    FP_TEM_NO_EMPTY    = 0x22, /* 指纹模板非空 */
    FP_TEM_EMPTY    = 0x22, /* 指纹模板为空 */
    FP_LIB_EMPTY      = 0x24, /* 指纹库为空 */
    INPUT_TIMES_SET_ERR      = 0x24, /* 录入次数设置错误 */
    OVERTIME      = 0x26, /* 超时 */
    FP_EXIST      = 0x27, /* 指纹已存在 */

//    SUBSEQ_OK     = 0xF0, /* 有后续数据包的指令，正确接收后用 0xf0 应答 */
//    SUBSEQ_CMD    = 0xF1, /* 有后续数据包的指令，命令包用 0xf1 应答 */
//    BURN_SUM_ERR  = 0xF2, /* 表示烧写内部 FLASH 时，校验和错误 */
//    BURN_TOK_ERR  = 0xF3, /* 表示烧写内部 FLASH 时，包标识错误 */
//    BURN_LEN_ERR  = 0xF4, /* 表示烧写内部 FLASH 时，包长度错误 */
//    BURN_COD_EXSZ = 0xF5, /* 表示烧写内部 FLASH 时，代码长度太长 */
//    BURN_FUNC_ERR = 0xF6, /* 表示烧写内部 FLASH 时，烧写 FLASH 失败 */
};

typedef enum fp_ack_type fp_ack_type_t;

typedef struct
{
    uint16_t pageID;    //指纹ID
    uint16_t mathscore; //匹配得分
}SearchResult;

typedef struct
{
    rt_uint16_t state;      //1、状态寄存器
    rt_uint16_t sensor;     //2、传感器类型  0：fpc1011c；2：祥群 c500；3：祥群 s500 条状；7：深圳芯微条状；9：用户自定义传感器；
	rt_uint16_t capacity;   //3、指纹库容量
	rt_uint16_t grade;      //4、安全等级  1/2/3/4/5
	rt_uint32_t addr;       //5、设备地址
	rt_uint16_t size;       //6、数据包大小    0：32bytes 1：62bytes 2：128bytes 3：256bytes
	rt_uint16_t baud;       //7、波特率设置    (波特率为 9600*N bps)
}SysPara;

typedef struct
{
    uint8_t table[32];
}IndexTable;

/**
 * @brief Get the image object
 *        获取传感器图像
 * 
 * @return fp_ack_type_t 模块确认码
 */
fp_ack_type_t fp_get_image(void);

/****************************************************************
 *  输入参数： BufferID(特征缓冲区号)
 *  返回参数： 确认字
 * ****************************************************************/
fp_ack_type_t fp_gen_char(rt_uint8_t buff_id);

/*
 * 功能说明： 以 CharBuffer1 或 CharBuffer2 中的特征文件搜索整个或部分指纹库。若搜索到，则返回页码。
 * 输入参数： BufferID， StartPage(起始页)，PageNum（页数）
 *          page_id : 页码（相配指纹模板）
 *          mat_score : 比对得分
 * 返回参数： 确认字
 */
fp_ack_type_t fp_search(uint8_t buffer_id, uint16_t startpage, uint16_t page_num, SearchResult *p);

/*
 * 功能说明： 将 CharBuffer1 与 CharBuffer2 中的特征文件合并生成模板，结果存于 CharBuffer1 与 CharBuffer2。
 * 输入参数： none
 * 返回参数： 确认字
 */
fp_ack_type_t fp_reg_model(void);

/*
 * 功能说明： 将 CharBuffer1 或 CharBuffer2 中的模板文件存到 PageID 号flash 数据库位置。
 * 输入参数： BufferID(缓冲区号)，PageID（指纹库位置号）
 * 返回参数： 确认字
 */
fp_ack_type_t fp_store_char(rt_uint8_t buff_id, rt_uint16_t page_id);

/*
 * 功能说明： 删除 flash 数据库中指定 ID 号开始的 N 个指纹模板
 * 输入参数： PageID(指纹库模板号)，N 删除的模板个数。
 * 返回参数： 确认字
 */
fp_ack_type_t fp_delet_char(rt_uint16_t page_id, rt_uint16_t n);

/*
 * 功能说明： 删除 flash 数据库中所有指纹模板
 * 输入参数： none
 * 返回参数： 确认字
 */
fp_ack_type_t fp_empty(void);

/*
 * 功能说明： 写模块寄存器
 * 输入参数： 寄存器序号 写入内容
 * 返回参数： 确认字
 */
fp_ack_type_t fp_set_reg(rt_uint8_t register_number, rt_uint8_t content);
/*
 * 功能说明： 写模块寄存器
 * 输入参数： 寄存器序号 写入内容
 * 返回参数： 确认字
 */
fp_ack_type_t fp_write_reg(rt_uint8_t register_number, rt_uint8_t content);

/*
 * 功能说明： 读取模块的基本参数（波特率，包大小等）。
 * 输入参数：
 * 返回参数： 确认字
 */
fp_ack_type_t fp_read_sys_para(SysPara *p);

/**
 * @brief verify password
 *        验证密码
 * 
 * @param param 
 * @return rt_err_t 
 */
rt_err_t fp_vfy_pwd(void);

/*
 * 功能说明：验证指纹
 * 输入参数：
 * 返回参数：
 */
fp_ack_type_t fp_auto_identify(SearchResult *p);

/*
 * 功能说明： 读有效模板个数
 * 输入参数： none
 * 返回参数： 确认字
 */
fp_ack_type_t fp_valid_templete_num(rt_uint16_t *valid_n);

/*
 * 功能说明： 读索引表
 * 输入参数： none
 * 返回参数： 确认字
 */
fp_ack_type_t fp_read_index_table(uint8_t Page, IndexTable *p);



/* 上层 API */


/**
 * @brief 模块初始化函数
 * 
 * @param name 
 */
void fp_init(void *name);






#endif
