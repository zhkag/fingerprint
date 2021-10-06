/*
 * @Author: Aurora-zk
 * @Date: 2021-03-14 12:39:09
 * @LastEditors: Aurora-zk
 * @LastEditTime: 2021-10-05 20:39:18
 * @contact: pk-ing@nyist.edu.cn
 * @FilePath: \fingerprint\fingerprint_sample.c
 */

#include <fingerprint.h>
#define THREAD_PRIORITY         25
#define THREAD_STACK_SIZE       512
#define THREAD_TIMESLICE        5

#ifndef FINGERPRINT_UART_NAME
#define FINGERPRINT_UART_NAME "uart2"
#endif


#define EVENT_QUERY (1 << 1)

int fp_port(void)
{
    rt_thread_t fp_init_tid = RT_NULL;
    fp_init_tid = rt_thread_create("fp_init",
                            fp_init, FINGERPRINT_UART_NAME,
                            THREAD_STACK_SIZE,
                            THREAD_PRIORITY, THREAD_TIMESLICE);
    /* 如果获得线程控制块，启动这个线程 */
    if (fp_init_tid != RT_NULL)
        rt_thread_startup(fp_init_tid);
    return 0;
}
//INIT_ENV_EXPORT(fp_port);
MSH_CMD_EXPORT(fp_port, fp_port)



/*
 * 功能说明：删除一个指纹
 */
void fp_delet_char_cmd(int argc,char** argv)
{
    if (argc >= 2) {
        uint8_t code = fp_delet_char(atoi(argv[1]), 1);
        rt_kprintf("id:%d delet is %d !!\n", atoi(argv[1]), code);
    }else {
        rt_kprintf("Please enter the ID to verify!!\n");
    }
}

MSH_CMD_EXPORT(fp_delet_char_cmd, "fp_delet_char_cmd");


/*
 * 功能说明：删除全部指纹
 */
MSH_CMD_EXPORT(fp_empty, "fp_empty");

/*
 * 功能说明：读取指纹模块基本信息
 */
void fp_get_sys_para()
{
    SysPara p;
    fp_ack_type_t code = fp_read_sys_para(&p);

    rt_kprintf("code = %lx\n", code);
    rt_kprintf("p->state = %lx\n", p.state);
    rt_kprintf("p->sensor = %lx\n", p.sensor);
    rt_kprintf("p->capacity = %d\n", p.capacity);
    rt_kprintf("p->grade = %lx\n", p.grade);
    rt_kprintf("p->addr = %lx\n", p.addr);
    rt_kprintf("p->size = %lx\n", p.size);
    rt_kprintf("p->baud = %lx\n", p.baud);
}

MSH_CMD_EXPORT(fp_get_sys_para, "fp_read_sys_para");


/*
 * 功能说明：读取有效模板的个数
 */
void fp_get_num(void)
{
    rt_uint16_t valid_n;
    fp_ack_type_t code = fp_valid_templete_num(&valid_n);
    rt_kprintf("code = %lx\n", code);
    rt_kprintf("valid_n = %lx,", valid_n);
}
MSH_CMD_EXPORT(fp_get_num, "fp_valid_templete_num");



/*
 * 获取有效id
 */
uint16_t get_eff_id(void)
{
    uint16_t count = 0;
    uint8_t page;
    IndexTable p;
    for (page = 0; page < 4; ++page) {
        fp_read_index_table(page,&p);
        for (int var = 0; var < 32; ++var) {
            for (int i = 0; i < 8; ++i) {
                if ((p.table[var] >> i) & 0x01) {
                    count++;
                }
                else {
//                    rt_kprintf("count = %d!!!\n",count);
                    return count;
                }
            }
        }
    }
    return -1;
}

MSH_CMD_EXPORT(get_eff_id, "get_eff_id");
/*
 * 获取id的状态，有无指纹
 */
uint8_t get_id_state(uint16_t page_id)
{
    uint8_t page = page_id / 256;
    IndexTable p;
    fp_read_index_table(page,&p);
    page = page_id % 256;
    return (p.table[page/8] >> (page%8)) & 0x01;
}
void get_id_state_cmd(int argc,char** argv)
{
    if (argc >= 2) {
        uint8_t var = get_id_state(atoi(argv[1]));
        rt_kprintf("id:%d  is %d !!\n", atoi(argv[1]), var);
    }else {
        rt_kprintf("Please enter the ID to verify!!\n");
    }
}

MSH_CMD_EXPORT(get_id_state_cmd, "get_id_state_cmd");


//模块应答包确认码信息解析
//功能：解析确认码错误信息返回信息
//参数: ensure
const char *EnsureMessage(uint8_t ensure)
{
    const char *p;
    switch(ensure)
    {
        case  0x00:
            p="OK";break;
        case  0x01:
            p="数据包接收错误";break;
        case  0x02:
            p="传感器上没有手指";break;
        case  0x03:
            p="录入指纹图像失败";break;
        case  0x04:
            p="指纹图像太干、太淡而生不成特征";break;
        case  0x05:
            p="指纹图像太湿、太糊而生不成特征";break;
        case  0x06:
            p="指纹图像太乱而生不成特征";break;
        case  0x07:
            p="指纹图像正常，但特征点太少（或面积太小）而生不成特征";break;
        case  0x08:
            p="指纹不匹配";break;
        case  0x09:
            p="没搜索到指纹";break;
        case  0x0a:
            p="特征合并失败";break;
        case  0x0b:
            p="ID超出指纹库范围";
        case  0x0c:
            p="从指纹库读模板出错或无效";
        case  0x0d:
            p="上传特征失败";
        case  0x0e:
            p="模块不能接受后续数据包";
        case  0x0f:
            p="上传图像失败";
        case  0x10:
            p="删除模板失败";break;
        case  0x11:
            p="清空指纹库失败";break;
        case  0x12:
            p="不能进入低功耗状态";break;
        case  0x13:
            p="口令不正确";break;
        case  0x14:
            p="系统复位失败";break;
        case  0x15:
            p="缓冲区内没有有效原始图而生不成图像";break;
        case  0x16:
            p="在线升级失败";break;
        case  0x17:
            p="残留指纹或两次采集之间手指没有移动过";break;
        case  0x18:
            p="读写 FLASH 出错";break;
        case  0x19:
            p="未定义错误";break;
        case  0x1a:
            p="无效寄存器号";break;
        case  0x1b:
            p="寄存器设定内容错误";break;
        case  0x1c:
            p="记事本页码指定错误";break;
        case  0x1e:
            p="自动注册失败";break;
        case  0x1f:
            p="指纹库满";break;
        case  0x20:
            p="地址错误";break;
        case  0x23:
            p="指纹模板为空";break;
        case  0x24:
            p="指纹库为空";break;
        case  0x26:
            p="自动注册超时";break;
        case  0x27:
            p="自动注册指纹已存在";break;
        default :
            p=" ";break;
    }
 return p;
}


//显示确认码错误信息
void ShowErrMessage(uint8_t ensure)
{
//    rt_kprintf((uint8_t*)EnsureMessage(ensure));
//    rt_kprintf("\n");
}

/*
 * 功能说明：添加指纹，连续四次按压，并且中途需要抬起手指
 */
void fp_add_fp(void)
{
    uint8_t i=0,ensure ,processnum=1,pressCout=1, str_buffer[40];
    uint16_t ID = get_eff_id();
    while(1)
    {
        switch (processnum)
        {
            case 0: //连续按压4次指纹分别存到4个charBuffer里
                i++;
                rt_sprintf((char*)str_buffer,"Please raise your finger: %d\n",pressCout);
                rt_kprintf((char*)str_buffer);
                ensure=fp_get_image();
                if(ensure==0x02)
                {
                    i=0;
                    processnum=1;//跳到第一步
                    pressCout++;
                    if(pressCout >=5)
                     {
                        pressCout = 0;
                        processnum=2;//跳到第二步
                    }
                }
                break;

            case 1: //连续按压4次指纹分别存到4个charBuffer里
                i++;
                rt_sprintf((char*)str_buffer,"Please press your finger: %d\n",pressCout);
                rt_kprintf((char*)str_buffer);
                ensure=fp_get_image();
                if(ensure==0x00)
                {
                    i=0;
                    rt_thread_mdelay(100);//这里需要延时一下，模块内部处理图像需要时间
                    ensure=fp_gen_char(pressCout);//生成特征
                    if(ensure==0x00)
                    {
                       processnum=0;//跳到第零步
                    }else ShowErrMessage(ensure);
                }else ShowErrMessage(ensure);
                break;

            case 2:
                rt_kprintf("Generate fingerprint template!\n");
                ensure=fp_reg_model();
                if(ensure==0x00)
                {
                    rt_kprintf("Generate fingerprint template successfully!\n");
                    processnum=3;//跳到第三步
                }else {processnum=1;ShowErrMessage(ensure);}
                rt_thread_mdelay(1200);
                break;

            case 3:
                rt_kprintf("Fingerprint storage in %d!\n",ID);
                ensure=fp_store_char(CharBuffer1,ID);//储存模板
                if(ensure==0x00)
                {
                    rt_kprintf("Fingerprint entered successfully!\n");
                    uint16_t ValidN;
                    fp_valid_templete_num(&ValidN);//读库指纹个数
//                    LCD_ShowNum(56,80,LB301Para.PS_max-ValidN,3,16);//显示剩余指纹容量
                    rt_thread_mdelay(1500);
                    return ;
                }else {processnum=1;ShowErrMessage(ensure);}
                break;
        }
        rt_thread_mdelay(200);
        if(i>=20)//超过20次没有按手指则退出
        {
            break;
        }
    }
}

MSH_CMD_EXPORT(fp_add_fp, "fp_valid_templete_num");

/*
 * 功能说明：刷指纹
 */

void fp_read(void)
{
    SearchResult p;
    fp_ack_type_t code;
    code = fp_auto_identify(&p);

    rt_kprintf("Find the fingerprint! code =%d page_id=%lx, mat_score=%lx", code, p.pageID, p.mathscore);
}
MSH_CMD_EXPORT(fp_read, "fp_valid_templete_num");


