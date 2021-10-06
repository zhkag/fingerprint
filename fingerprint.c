/*
 * @Author: Aurora-zk
 * @Date: 2021-03-14 12:39:09
 * @LastEditors: Aurora-zk
 * @LastEditTime: 2021-10-05 20:39:39
 * @contact: pk-ing@nyist.edu.cn
 * @FilePath: \fingerprint\fingerprint.c
 */

#include <fingerprint.h>

#define DBG_TAG              "fp.dev"
#define DBG_LVL              DBG_INFO
//#define DBG_LVL              DBG_LOG
#include <rtdbg.h>




#define EVENT_RX      (1 << 0)
#define EVENT_TOUCH   (1 << 1)
static struct rt_event event_fp;
static rt_device_t fp_dev;
static rt_uint8_t flag_vfy = 0; /* 握手成功标志 */
static rt_uint8_t flag_init = 0;

#if DBG_LVL == DBG_LOG
static void print_buf(rt_uint8_t *buf, rt_size_t size)
{
    for (int i = 0; i < size; i++)
    {
        rt_kprintf("%x ", buf[i]);
    }
    rt_kprintf("\r\n");
}
#endif

#define BUF_SIZE    50U
#define RETRY_CNT   20U
static rt_uint8_t tx_buf[BUF_SIZE] = {FP_HEAD_H, FP_HEAD_L, FP_ADDR_0,
                                FP_ADDR_1, FP_ADDR_2, FP_ADDR_3};
static rt_uint8_t rx_buf[BUF_SIZE];


/*
 *       发送缓存加校验和
 * @note 包长度 = 包长度至校验和（指令、参数或数据）的总字节数，包含校验和，但不包含包长度本身的字节数
 *       校验和是从包标识至校验和之间所有字节之和
 */
static void tx_buf_add_checksum(rt_uint8_t *buf)
{
    rt_uint16_t i = 0;
    rt_uint16_t checksum = 0; /* 超出两字节的进位不理会 */
    rt_uint16_t pkg_len = (buf[FP_LEN_BIT] << 8) + buf[FP_LEN_BIT+1];

    for (i = 0; i < pkg_len; i++)
    {
        checksum += buf[i+FP_LEN_BIT];
    }
    checksum += buf[FP_TOK_BIT];

    *(buf+PREFIX_SIZE+pkg_len-2) = (checksum&0xff00)>>8;
    *(buf+PREFIX_SIZE+pkg_len-1) = checksum&0x00ff;
}

/*
 *   接收验证校验和
 */

static rt_uint8_t cnt_checksum(rt_uint8_t *buf)
{
    rt_uint8_t checksum_is_ok = 0;
    rt_uint16_t i = 0;
    rt_uint16_t checksum = 0; /* 超出两字节的进位不理会 */
    rt_uint16_t pkg_len = (buf[FP_LEN_BIT] << 8) + buf[FP_LEN_BIT+1];

    if (!((rx_buf[FP_HEAD_BIT] == FP_HEAD_H) && (rx_buf[FP_HEAD_BIT+1] == FP_HEAD_L)))
        return checksum_is_ok;

    for (i = 0; i < pkg_len; i++)
    {
        checksum += buf[i+FP_LEN_BIT];
    }
    checksum += buf[FP_TOK_BIT];

    if ((*(buf+PREFIX_SIZE+pkg_len-2) == ((checksum&0xff00)>>8))
    && (*(buf+PREFIX_SIZE+pkg_len-1) == (checksum&0x00ff)))
    {
        checksum_is_ok = 1;
    }
    else
    {
        LOG_E("checksum_1:%x,checksum_2:%x,sum_1:%x,sum_2:%x", checksum&0xff00,checksum&0x00ff,*(buf+PREFIX_SIZE+pkg_len-2),*(buf+PREFIX_SIZE+pkg_len-1));
    }
    return checksum_is_ok;
}
/*
 *           发送包大小
 */
static rt_size_t cnt_tx_pkg_size(void)
{
    rt_size_t result = 0;
    rt_enter_critical();
    rt_uint8_t size_tmp = tx_buf[FP_LEN_BIT] << 8;
    result = size_tmp + tx_buf[FP_LEN_BIT+1] + PREFIX_SIZE;
    rt_exit_critical();
    return result;
}
/*
 *          接收包大小
 */
static rt_size_t cnt_rx_pkg_size(void)
{
    rt_size_t result = 0;
    rt_enter_critical();
    rt_uint8_t size_tmp = rx_buf[FP_LEN_BIT] << 8;
    result = size_tmp + rx_buf[FP_LEN_BIT+1];
    rt_exit_critical();
    if (result > BUF_SIZE)
    {
        result = (rt_size_t)(BUF_SIZE - PREFIX_SIZE);
        LOG_W("Next packet is out of buf size!");
    }

    return result;
}
/*
 *              获取接收包
 */
static rt_err_t master_get_rx(void)
{
    rt_err_t ret = RT_EOK;
    rt_uint32_t rec = 0;
    rt_size_t size = 0;
    rt_uint8_t rx_cnt = 0;

    ret = rt_event_recv(&event_fp, EVENT_RX, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, 1000, &rec);
    if (ret != RT_EOK)
    {
        return ret;
    }

    memset(rx_buf, 0x00, BUF_SIZE);
    LOG_D("after clear rx_buf[0]=%x",rx_buf[0]);

    rt_thread_mdelay(10);
    do
    {
        rt_device_read(fp_dev, -1, rx_buf, (rt_size_t)1);
        rx_cnt++;
    } while ((rx_buf[0] != 0xEF)&&(rx_cnt < RETRY_CNT)); /* 清除串口缓冲中的脏数据 */

    if (rx_cnt >= RETRY_CNT)
    {
        ret = -RT_ETIMEOUT;
        return ret;
    }

    rt_device_read(fp_dev, -1, rx_buf+1, (rt_size_t)PREFIX_SIZE-1); /* 获取包头 */
    LOG_D("after first read rx_buf[0]=%x",rx_buf[0]);
    LOG_D("after first read rx_buf[FP_LEN_BIT+1]=%x",rx_buf[FP_LEN_BIT+1]);

    rt_event_recv(&event_fp, EVENT_RX, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, 0, &rec);
    size = cnt_rx_pkg_size(); /* 计算包长度 */
    rt_device_read(fp_dev, -1, rx_buf+PREFIX_SIZE, size);  /* 获取包剩下数据 */
    rt_event_recv(&event_fp, EVENT_RX, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, 0, &rec);

#if DBG_LVL == DBG_LOG
    rt_kprintf("rx_size:%d rx: ", size+PREFIX_SIZE);
    print_buf(rx_buf, size+PREFIX_SIZE);
#endif

    return ret;
}
/*
 *    发送指令并接收应答
 */
static rt_err_t make_prefix(const char *func)
{
    rt_err_t ret = -1;
    rt_size_t size = 0;

    tx_buf_add_checksum(tx_buf);
    size = cnt_tx_pkg_size();

#if DBG_LVL == DBG_LOG
    rt_kprintf("func:%s tx_size:%d tx: ", func, size);
    print_buf(tx_buf, size);
#endif

    rt_device_write(fp_dev, 0, tx_buf, size);

    ret = master_get_rx();

    return ret;
}

/**
 * @brief Get the image object
 *        获取传感器图像
 *
 * @return fp_ack_type_t 模块确认码
 */
fp_ack_type_t fp_get_image(void)
{
    fp_ack_type_t code;
    code = CMD_OK;

    if (flag_vfy != 1)
    {
        LOG_E("Please verify the password before using the fingerprint module!");
        code = UNDEF_ERR;
        return code;
    }

    tx_buf[FP_TOK_BIT] = 0x01;
    tx_buf[FP_LEN_BIT] = 0x00;
    tx_buf[FP_LEN_BIT+1] = 0x03;
    tx_buf[FP_INS_CMD_BIT] = FP_CMD_GET_IMG;

    rt_err_t ret = make_prefix(__func__);

    if (-RT_ETIMEOUT == ret)
    {
        LOG_E("Function fp_get_image timeout!");
        code = UNDEF_ERR;
        return code;
    }

    if (cnt_checksum(rx_buf) == 1)
    {
        code = (fp_ack_type_t)rx_buf[FP_REP_ACK_BIT(0)]; /* 校验和正确则返回模块确认码 */
    }
    else
    {
        code = DAT_ERR;
    }

    return code;
}
/****************************************************************
 *  输入参数： BufferID(特征缓冲区号)
 *  返回参数： 确认字
 * ****************************************************************/
fp_ack_type_t fp_gen_char(rt_uint8_t buff_id)
{
    fp_ack_type_t code;
    code = CMD_OK;

    if (flag_vfy != 1)
    {
        LOG_E("Please verify the password before using the fingerprint module!");
        code = UNDEF_ERR;
        return code;
    }

    tx_buf[FP_TOK_BIT] = 0x01;
    tx_buf[FP_LEN_BIT] = 0x00;
    tx_buf[FP_LEN_BIT+1] = 0x04;
    tx_buf[FP_INS_CMD_BIT] = FP_CMD_GEN_CHR;
    tx_buf[FP_INS_PAR_BIT(0)] = buff_id;

    rt_err_t ret = make_prefix(__func__);

    if (-RT_ETIMEOUT == ret)
    {
        LOG_E("Function fp_gen_char timeout!");
        code = UNDEF_ERR;
        return code;
    }

    if (cnt_checksum(rx_buf) == 1)
    {
        code = (fp_ack_type_t)rx_buf[FP_REP_ACK_BIT(0)]; /* 校验和正确则返回模块确认码 */
    }
    else
    {
        code = DAT_ERR;
    }

    return code;
}

/*
 * 功能说明： 以 CharBuffer1 或 CharBuffer2 中的特征文件搜索整个或部分指纹库。若搜索到，则返回页码。
 * 输入参数： BufferID， StartPage(起始页)，PageNum（页数）
 *          page_id : 页码（相配指纹模板）
 *          mat_score : 比对得分
 * 返回参数： 确认字
 */
fp_ack_type_t fp_search(uint8_t buffer_id, uint16_t startpage, uint16_t page_num, SearchResult *p)
{
    fp_ack_type_t code;
    code = CMD_OK;

    if (flag_vfy != 1)
    {
        LOG_E("Please verify the password before using the fingerprint module!");
        code = UNDEF_ERR;
        return code;
    }

    tx_buf[FP_TOK_BIT] = 0x01;
    tx_buf[FP_LEN_BIT] = 0x00;
    tx_buf[FP_LEN_BIT+1] = 0x08;
    tx_buf[FP_INS_CMD_BIT] = FP_CMD_SEARCH;
    tx_buf[FP_INS_PAR_BIT(0)] = buffer_id;
    tx_buf[FP_INS_PAR_BIT(1)] = (startpage & 0xff00) >> 8;
    tx_buf[FP_INS_PAR_BIT(2)] = startpage & 0x00ff;
    tx_buf[FP_INS_PAR_BIT(3)] = (page_num & 0xff00) >> 8;
    tx_buf[FP_INS_PAR_BIT(4)] = page_num & 0x00ff;

    rt_err_t ret = make_prefix(__func__);

    if (-RT_ETIMEOUT == ret)
    {
        LOG_E("Function fp_search timeout!");
        code = UNDEF_ERR;
        return code;
    }

    if (cnt_checksum(rx_buf) == 1)
    {
        code = (fp_ack_type_t)rx_buf[FP_REP_ACK_BIT(0)]; /* 校验和正确则返回模块确认码 */
        p->pageID = (rx_buf[FP_REP_ACK_BIT(1)] << 8) + rx_buf[FP_REP_ACK_BIT(2)];
        p->mathscore = (rx_buf[FP_REP_ACK_BIT(3)] << 8) + rx_buf[FP_REP_ACK_BIT(4)];
        LOG_D("p->pageID=%lx, p->mathscore=%lx", p->pageID, p->mathscore);
    }
    else
    {
        code = DAT_ERR;
        return code;
    }


    return code;
}
/*
 * 功能说明： 将 CharBuffer1 与 CharBuffer2 中的特征文件合并生成模板，结果存于 CharBuffer1 与 CharBuffer2。
 * 输入参数： none
 * 返回参数： 确认字
 */
fp_ack_type_t fp_reg_model(void)
{
    fp_ack_type_t code;
    code = CMD_OK;

    if (flag_vfy != 1)
    {
        LOG_E("Please verify the password before using the fingerprint module!");
        code = UNDEF_ERR;
        return code;
    }

    tx_buf[FP_TOK_BIT] = 0x01;
    tx_buf[FP_LEN_BIT] = 0x00;
    tx_buf[FP_LEN_BIT+1] = 0x03;
    tx_buf[FP_INS_CMD_BIT] = FP_CMD_REG_MODEL;

    rt_err_t ret = make_prefix(__func__);

    if (-RT_ETIMEOUT == ret)
    {
        LOG_E("Function fp_gen_char timeout!");
        code = UNDEF_ERR;
        return code;
    }

    if (cnt_checksum(rx_buf) == 1)
        code = (fp_ack_type_t)rx_buf[FP_REP_ACK_BIT(0)]; /* 校验和正确则返回模块确认码 */
    else
        code = DAT_ERR;

    return code;
}
/*
 * 功能说明： 将 CharBuffer1 或 CharBuffer2 中的模板文件存到 PageID 号flash 数据库位置。
 * 输入参数： BufferID(缓冲区号)，PageID（指纹库位置号）
 * 返回参数： 确认字
 */
fp_ack_type_t fp_store_char(uint8_t buff_id, uint16_t page_id)
{
    fp_ack_type_t code;
    code = CMD_OK;

    if (flag_vfy != 1)
    {
        LOG_E("Please verify the password before using the fingerprint module!");
        code = UNDEF_ERR;
        return code;
    }


    tx_buf[FP_TOK_BIT] = 0x01;
    tx_buf[FP_LEN_BIT] = 0x00;
    tx_buf[FP_LEN_BIT+1] = 0x06;
    tx_buf[FP_INS_CMD_BIT] = FP_CMD_STR_CHR;
    tx_buf[FP_INS_PAR_BIT(0)] = buff_id;
    tx_buf[FP_INS_PAR_BIT(1)] = (uint8_t)((page_id&0xff00)>>8);
    tx_buf[FP_INS_PAR_BIT(2)] = (uint8_t)(page_id&0x00ff);

    rt_err_t ret = make_prefix(__func__);

    if (-RT_ETIMEOUT == ret)
    {
        LOG_E("Function fp_search timeout!");
        code = UNDEF_ERR;
        return code;
    }

    if (cnt_checksum(rx_buf) == 1)
        code = (fp_ack_type_t)rx_buf[FP_REP_ACK_BIT(0)]; /* 校验和正确则返回模块确认码 */
    else
        code = DAT_ERR;

    return code;
}
/*
 * 功能说明： 删除 flash 数据库中指定 ID 号开始的 N 个指纹模板
 * 输入参数： PageID(指纹库模板号)，N 删除的模板个数。
 * 返回参数： 确认字
 */
fp_ack_type_t fp_delet_char(uint16_t page_id, uint16_t n)
{
    fp_ack_type_t code;
    code = CMD_OK;

    if (flag_vfy != 1)
    {
        LOG_E("Please verify the password before using the fingerprint module!");
        code = UNDEF_ERR;
        return code;
    }

    tx_buf[FP_TOK_BIT] = 0x01;
    tx_buf[FP_LEN_BIT] = 0x00;
    tx_buf[FP_LEN_BIT+1] = 0x07;
    tx_buf[FP_INS_CMD_BIT] = FP_CMD_DEL_CHR;
    tx_buf[FP_INS_PAR_BIT(0)] = (rt_uint8_t)((page_id&0xff00)>>8);
    tx_buf[FP_INS_PAR_BIT(1)] = (rt_uint8_t)(page_id&0x00ff);
    tx_buf[FP_INS_PAR_BIT(2)] = (rt_uint8_t)((n&0xff00)>>8);
    tx_buf[FP_INS_PAR_BIT(3)] = (rt_uint8_t)(n&0x00ff);

    rt_err_t ret = make_prefix(__func__);
    if (-RT_ETIMEOUT == ret)
    {
        LOG_E("Function fp_search timeout!");
        code = UNDEF_ERR;
        return code;
    }

    if (cnt_checksum(rx_buf) == 1)
        code = (fp_ack_type_t)rx_buf[FP_REP_ACK_BIT(0)]; /* 校验和正确则返回模块确认码 */
    else
        code = DAT_ERR;

    return code;
}

/*
 * 功能说明： 删除 flash 数据库中所有指纹模板
 * 输入参数： none
 * 返回参数： 确认字
 */
fp_ack_type_t fp_empty(void)
{
    fp_ack_type_t code;
    code = CMD_OK;

    if (flag_vfy != 1)
    {
        LOG_E("Please verify the password before using the fingerprint module!");
        code = UNDEF_ERR;
        return code;
    }

    tx_buf[FP_TOK_BIT] = 0x01;
    tx_buf[FP_LEN_BIT] = 0x00;
    tx_buf[FP_LEN_BIT+1] = 0x03;
    tx_buf[FP_INS_CMD_BIT] = FP_CMD_EMPTY;

    rt_err_t ret = make_prefix(__func__);
    if (-RT_ETIMEOUT == ret)
    {
        LOG_E("Function fp_search timeout!");
        code = UNDEF_ERR;
        return code;
    }

    if (cnt_checksum(rx_buf) == 1)
        code = (fp_ack_type_t)rx_buf[FP_REP_ACK_BIT(0)]; /* 校验和正确则返回模块确认码 */
    else
        code = DAT_ERR;

    return code;
}


/*
 * 功能说明： 写模块寄存器
 * 输入参数： 寄存器序号 写入内容
 * 返回参数： 确认字
 */
fp_ack_type_t fp_set_reg(rt_uint8_t register_number, rt_uint8_t content)
{
    fp_ack_type_t code;
    code = CMD_OK;

    if (flag_vfy != 1)
    {
        LOG_E("Please verify the password before using the fingerprint module!");
        code = UNDEF_ERR;
        return code;
    }

    tx_buf[FP_TOK_BIT] = 0x01;
    tx_buf[FP_LEN_BIT] = 0x00;
    tx_buf[FP_LEN_BIT+1] = 0x05;
    tx_buf[FP_INS_CMD_BIT] = FP_CMD_FINGER_MOUDLE_SET;
    tx_buf[FP_INS_PAR_BIT(0)] = register_number;
    tx_buf[FP_INS_PAR_BIT(1)] = content;

    rt_err_t ret = make_prefix(__func__);
    if (-RT_ETIMEOUT == ret)
    {
        LOG_E("Function fp_search timeout!");
        code = UNDEF_ERR;
        return code;
    }

    if (cnt_checksum(rx_buf) == 1)
        code = (fp_ack_type_t)rx_buf[FP_REP_ACK_BIT(0)]; /* 校验和正确则返回模块确认码 */
    else
        code = DAT_ERR;

    return code;
}
/*
 * 功能说明： 读取模块的基本参数（波特率，包大小等）。
 * 输入参数：
 * 返回参数： 确认字
 */
fp_ack_type_t fp_read_sys_para(SysPara *p)
{
    fp_ack_type_t code;
    code = CMD_OK;

    if (flag_vfy != 1)
    {
        LOG_E("Please verify the password before using the fingerprint module!");
        code = UNDEF_ERR;
        return code;
    }

    tx_buf[FP_TOK_BIT] = 0x01;
    tx_buf[FP_LEN_BIT] = 0x00;
    tx_buf[FP_LEN_BIT+1] = 0x03;
    tx_buf[FP_INS_CMD_BIT] = FP_CMD_RD_PARAM;

    rt_err_t ret = make_prefix(__func__);
    if (-RT_ETIMEOUT == ret)
    {
        LOG_E("Function fp_search timeout!");
        code = UNDEF_ERR;
        return code;
    }

    if (cnt_checksum(rx_buf) == 1)
    {
        code = (fp_ack_type_t)rx_buf[FP_REP_ACK_BIT(0)]; /* 校验和正确则返回模块确认码 */

        p->state = (rx_buf[FP_REP_ACK_BIT(2)] << 8) + rx_buf[FP_REP_ACK_BIT(1)];
        p->sensor = (rx_buf[FP_REP_ACK_BIT(4)] << 8) + rx_buf[FP_REP_ACK_BIT(3)];
        p->capacity = (rx_buf[FP_REP_ACK_BIT(6)] << 8) + rx_buf[FP_REP_ACK_BIT(5)];
        p->grade = (rx_buf[FP_REP_ACK_BIT(8)] << 8) + rx_buf[FP_REP_ACK_BIT(7)];
        p->addr = (rx_buf[FP_REP_ACK_BIT(12)] << 24) + (rx_buf[FP_REP_ACK_BIT(11)] << 16) + (rx_buf[FP_REP_ACK_BIT(10)] << 8) + rx_buf[FP_REP_ACK_BIT(9)];
        p->size = (rx_buf[FP_REP_ACK_BIT(14)] << 8) + rx_buf[FP_REP_ACK_BIT(13)];
        p->baud = (rx_buf[FP_REP_ACK_BIT(16)] << 8) + rx_buf[FP_REP_ACK_BIT(15)];
    }
    else
        code = DAT_ERR;

    return code;
}

/********************************
 * 功能说明： 验证模块握手口令
 * 输入参数： PassWord
 * 返回参数： 确认字
 * 指令代码： 13H
 * ***************************************************/
rt_err_t fp_vfy_pwd(void)
{
    fp_ack_type_t code;
    code = CMD_OK;
    tx_buf[FP_TOK_BIT] = 0x01;
    tx_buf[FP_LEN_BIT] = 0x00;
    tx_buf[FP_LEN_BIT+1] = 0x07;
    tx_buf[FP_INS_CMD_BIT] = 0x13;
    tx_buf[FP_INS_PAR_BIT(0)] = 0x00;
    tx_buf[FP_INS_PAR_BIT(1)] = 0x00;
    tx_buf[FP_INS_PAR_BIT(2)] = 0x00;
    tx_buf[FP_INS_PAR_BIT(3)] = 0x00;

    rt_err_t ret = make_prefix(__func__);
    if (-RT_ETIMEOUT == ret)
    {
        LOG_E("Function fp_search timeout!");
        code = UNDEF_ERR;
        return code;
    }


    if (cnt_checksum(rx_buf) == 1)
    {
        //if ((rx_buf[FP_REP_ACK_BIT(0)] == 0x00) && (rx_buf[FP_REP_ACK_BIT(1)] == 0x00))
        if ((rx_buf[FP_REP_ACK_BIT(0)] == 0x00))// && (rx_buf[FP_REP_ACK_BIT(1)] == 0x00))
        {
            flag_vfy = 1;
            ret = RT_EOK;
        }
    }
    else
        ret = -RT_EINVAL;

    return ret;
}

/*
 * 功能说明：验证指纹
 * 输入参数：
 * 返回参数：
 */
fp_ack_type_t fp_auto_identify(SearchResult *p)
{

#define  SEC_LEVEL      0X03    //安全等级
#define  S_ID           0XFFFF  //ID号，0XFFFF表示搜索整个库
#define  S_PARAM        0X0007  //参数 bit0 LED亮灭   bit1 图像预处理  bit2 中途是否应答
    fp_ack_type_t code;
    code = CMD_OK;

    if (flag_vfy != 1)
    {
        LOG_E("Please verify the password before using the fingerprint module!");
        code = UNDEF_ERR;
        return code;
    }

    tx_buf[FP_TOK_BIT] = 0x01;
    tx_buf[FP_LEN_BIT] = 0x00;
    tx_buf[FP_LEN_BIT+1] = 0x08;
    tx_buf[FP_INS_CMD_BIT] = FP_CMD_AUTO_IDENTIFY;
    tx_buf[FP_INS_PAR_BIT(0)] = (rt_uint8_t)(SEC_LEVEL);
    tx_buf[FP_INS_PAR_BIT(1)] = (rt_uint8_t)((S_ID&0xff00)>>8);
    tx_buf[FP_INS_PAR_BIT(2)] = (rt_uint8_t)(S_ID&0x00ff);
    tx_buf[FP_INS_PAR_BIT(3)] = (rt_uint8_t)((S_PARAM&0xff00)>>8);
    tx_buf[FP_INS_PAR_BIT(4)] = (rt_uint8_t)(S_PARAM&0x00ff);


    rt_err_t ret = make_prefix(__func__);
    if (-RT_ETIMEOUT == ret)
    {
        LOG_E("Function fp_search timeout!");
        code = UNDEF_ERR;
        return code;
    }

    if (cnt_checksum(rx_buf) == 1)
    {
        code=rx_buf[9];
        p->pageID    = (rx_buf[11]<<8) +rx_buf[12];
        p->mathscore = (rx_buf[13]<<8) +rx_buf[14];

    }
    else
        code = DAT_ERR;

    return code;
}

/*
 * 功能说明： 读有效模板个数
 * 输入参数： none
 * 返回参数： 确认字
 */
fp_ack_type_t fp_valid_templete_num(rt_uint16_t *valid_n)
{
    fp_ack_type_t code;
    code = CMD_OK;

    if (flag_vfy != 1)
    {
        LOG_E("Please verify the password before using the fingerprint module!");
        code = UNDEF_ERR;
        return code;
    }

    tx_buf[FP_TOK_BIT] = 0x01;
    tx_buf[FP_LEN_BIT] = 0x00;
    tx_buf[FP_LEN_BIT+1] = 0x03;
    tx_buf[FP_INS_CMD_BIT] = FP_CMD_VALID_TEMPLETE_NUM;

    rt_err_t ret = make_prefix(__func__);
    if (-RT_ETIMEOUT == ret)
    {
        LOG_E("Function fp_search timeout!");
        code = UNDEF_ERR;
        return code;
    }

    if (cnt_checksum(rx_buf) == 1)
    {
        code = (fp_ack_type_t)rx_buf[FP_REP_ACK_BIT(0)]; /* 校验和正确则返回模块确认码 */
        *valid_n = (rx_buf[FP_REP_ACK_BIT(1)] << 8) + rx_buf[FP_REP_ACK_BIT(2)];
    }
    else
        code = DAT_ERR;
    return code;
}

/*
 * 功能说明： 读索引表
 * 输入参数： none
 * 返回参数： 确认字
 */
fp_ack_type_t fp_read_index_table(uint8_t Page, IndexTable *p)
{
    fp_ack_type_t code;
    code = CMD_OK;

    if (flag_vfy != 1)
    {
        LOG_E("Please verify the password before using the fingerprint module!");
        code = UNDEF_ERR;
        return code;
    }
    tx_buf[FP_TOK_BIT] = 0x01;
    tx_buf[FP_LEN_BIT] = 0x00;
    tx_buf[FP_LEN_BIT+1] = 0x04;
    tx_buf[FP_INS_CMD_BIT] = FP_CMD_READ_INDEX_TABLE;
    tx_buf[FP_INS_PAR_BIT(0)] = Page;

    rt_err_t ret = make_prefix(__func__);
    if (-RT_ETIMEOUT == ret)
    {
        LOG_E("Function fp_search timeout!");
        code = UNDEF_ERR;
        return code;
    }

    if (cnt_checksum(rx_buf) == 1)
    {
        code = (fp_ack_type_t)rx_buf[FP_REP_ACK_BIT(0)]; /* 校验和正确则返回模块确认码 */
        for (int var = 0; var < 32; ++var) {
            p->table[var] = rx_buf[FP_REP_ACK_BIT(1 + var)];
        }
    }
    else
        code = DAT_ERR;
    return code;
}

/*
 * 暂时没有测试通过
 */
fp_ack_type_t fp_auto_enroll(void)
{
    fp_ack_type_t code;
    code = CMD_OK;

    if (flag_vfy != 1)
    {
        LOG_E("Please verify the password before using the fingerprint module!");
        code = UNDEF_ERR;
        return code;
    }
    uint16_t ID = 0x0002;
    uint16_t can = 0x003B;
    tx_buf[FP_TOK_BIT] = 0x01;
    tx_buf[FP_LEN_BIT] = 0x00;
    tx_buf[FP_LEN_BIT+1] = 0x08;
    tx_buf[FP_INS_CMD_BIT] = FP_CMD_AUTO_ENROLL;
    tx_buf[FP_INS_PAR_BIT(0)] = (rt_uint8_t)((ID&0xff00)>>8);
    tx_buf[FP_INS_PAR_BIT(1)] = (rt_uint8_t)(ID&0x00ff);
    tx_buf[FP_INS_PAR_BIT(2)] = (rt_uint8_t)(4);
    tx_buf[FP_INS_PAR_BIT(3)] = (rt_uint8_t)((can&0xff00)>>8);
    tx_buf[FP_INS_PAR_BIT(4)] = (rt_uint8_t)(can&0x00ff);

    rt_err_t ret = make_prefix(__func__);
    if (-RT_ETIMEOUT == ret)
    {
        LOG_E("Function fp_search timeout!");
        code = UNDEF_ERR;
        return code;
    }

    if (cnt_checksum(rx_buf) == 1)
    {
        code = (fp_ack_type_t)rx_buf[FP_REP_ACK_BIT(0)]; /* 校验和正确则返回模块确认码 */
        rt_kprintf("code = %d, can1 = %d,can2 = %d \n",code,rx_buf[FP_REP_ACK_BIT(1)],rx_buf[FP_REP_ACK_BIT(2)]);
    }
    else
        code = DAT_ERR;
    return code;
}
//MSH_CMD_EXPORT(fp_auto_enroll, "fp_auto_enroll");

static rt_err_t fp_rx_handle(rt_device_t dev, rt_size_t size)
{
    /* 串口接收到数据后产生中断，调用此回调函数，然后发送接收信号量 */
    rt_event_send(&event_fp, EVENT_RX);
    return RT_EOK;
}

/* 
 * 打开指纹的串口设备配置并验证握手
 */
static rt_err_t fp_hand_shake(void)
{
    rt_uint8_t i;
    rt_err_t ret = RT_EOK;
    struct serial_configure fp_cfg = RT_SERIAL_CONFIG_DEFAULT;
    fp_cfg.bufsz = 128;
    ret = rt_device_control(fp_dev, RT_DEVICE_CTRL_CONFIG, &fp_cfg);

    ret = rt_device_open(fp_dev, RT_DEVICE_FLAG_INT_RX);

    ret = rt_device_set_rx_indicate(fp_dev, fp_rx_handle);

    for (i = 1; i < 2; i++) /* 使用 N*9600 的波特率进行握手测试 */
    {
        fp_cfg.baud_rate = BAUD_RATE_9600 * i;
        ret = rt_device_control(fp_dev, RT_DEVICE_CTRL_CONFIG, &fp_cfg);
        ret = fp_vfy_pwd();
        if (ret == -RT_ETIMEOUT)
        {
            LOG_I("Hand shake in %d timeout.", fp_cfg.baud_rate);
        }
        else if (ret == -RT_EINVAL)
        {
            LOG_I("Recived PKG no vaild!");
        }
        else if (ret == -RT_ERROR)
        {
            LOG_E("Hand shake error!");
        }
        else if (ret == RT_EOK)
        {
            LOG_I("Establish connection in %d successfully!", fp_cfg.baud_rate);
            return RT_EOK;
        }
    }

    LOG_I("Failed to establish contact!");
    rt_device_close(fp_dev);

    ret = -RT_ERROR;
    return ret;    
}

void fp_init(void *name)
{
    const char *name_c = name;
    rt_err_t ret = RT_EOK;
    while(!flag_init)
    {
        /* 查找系统中的串口设备 */
        fp_dev = rt_device_find(name_c);
        if (!fp_dev)
        {
            LOG_E("find %s failed!\n", name_c);
        }

        ret = rt_event_init(&event_fp, "ent-fp", RT_IPC_FLAG_FIFO);
        if (ret != RT_EOK)
            LOG_E("event fp creat error!");
        fp_hand_shake();

        flag_init = 1;
    }
}

