    /******************** (C) COPYRIGHT  风驰iCreate嵌入式开发工作室 ***************************
     * 文件名  ：i2c_ee.c
     * 描述    ：I2C 总线配置读写函数库    
     * 实验平台：iCreate STM8开发板
     * 库版本  ：V2.0.0
     * 作者    ：ling_guansheng  QQ:779814207
     * 博客    ：
     * 淘宝    ：http://shop71177993.taobao.com/
     *修改时间 ：2011-12-20
      iCreate STM8开发板硬件连接
        |--------------------|
        |  I2C_SCL-PE1       |
        |  I2C_SDA-PE2       |
        |--------------------|
    
    #define I2C_Speed              100000
    #define I2C1_SLAVE_ADDRESS7    0xA0
    #define I2C_DUTYCYCLE_2        0x00
    #define I2C_ACK_CURR           0x01   /*!< Acknowledge on the current byte */
    #define I2C_EVENT_MASTER_START_SENT         0x1701 /*!< EV5: SB=1 */
    #define I2C_EVENT_MASTER_ADDRESS_ACKED      0x1702  /*!< EV6: ADDR=1 */
    #define I2C_EVENT_MASTER_BYTE_RECEIVED    0x1740  /*!< EV7: RXNE=1 */
    #define I2C_EVENT_MASTER_BYTE_TRANSMITTING  0x1780  /*!< EV8: TXE=1 */
    #define I2C_EVENT_MASTER_BYTE_TRANSMITTED   0x1784  /*!< EV8-2: TXE=1, BTF=1 */
    #define I2C_DIRECTION_TX     0x00  /*!< Transmission direction */
    #define I2C_DIRECTION_RX     0x01   /*!< Reception direction */
    #define I2C_ADDMODE_7BIT       0x00  /*!< 7-bit slave address (10-bit address not acknowledged) */
    #define I2C_FLAG_BUSBUSY       0x3002  /*!< Bus Busy Flag */
    #define I2C_FLAG_ACKNOWLEDGEFAILURE  0x2104  /*!< Acknowledge Failure Flag */
    #define I2C_FLAG_TRANSMITTERRECEIVER 0x3004  /*!< Transmitter Receiver Flag */
    #define I2C_FLAG_ADDRESSSENTMATCHED  0x1302  /*!< Address Sent/Matched (master/slave) flag */
    #define I2C_ACK_NONE   0x00  /*!< No acknowledge */
    #define EEPROM_BASE_ADDRESS    0x0000
    #define Page_Byte_Size    ((u8)8)   /*EEPROM 每页最多写8Byte*/
    #define EEPROM_ADDRESS         0xA0
    #define Input_Clock            16
    #define DISABLE            0
    #define ENABLE             1
    #define ERROR              0
    #define SUCCESS            1
    #define RESET              0
    #define SET                1
    #define I2C_CCRL_CCR     ((uint8_t)0xFF) 
    
    ****************************************************************************************/
    
    /* Includes ------------------------------------------------------------------*/
    #include "i2c_ee.h"
    /**************************************************************************
     * 函数名：I2C_Init
     * 描述  ：I2C配置函数
     * 输入  ：OutputClockFrequencyHz   I2C总线的时钟频率
               OwnAddress               从地址      
               DutyCycle                I2C总线占空比
               Ack                      应答模式
               AddMode                  地址模式
               InputClockFrequencyMHz   系统时钟
     *
     * 输出  ：无
     * 返回  ：无 
     * 调用  ：外部调用 
     
     主模式操作顺序：
     1.在FREQR寄存器中设定输入时钟；
     2.配置时钟控制寄存器；
     3.配置上升时间寄存器；
     4.编程CR1启动外设；    至此 初始化完成
     5.置CR1的START位为1；
     6.输入时钟频率下线： 标准至少1mhz，快速至少4mhz
     
    CR1[7]:NOSTRETCH->  时钟延展禁止（从模式） 该应用模式下当ADDR或者BTF标志置位时禁止时钟延展。1:禁止
    CR1[6]:ENGC->       广播呼叫使能，1:对地址00h相应。
    CR1[0]:PE->         I2C模块使能，如果清除PE时通讯进行，当通讯结束后I2C被禁用并返回空闲，所有的位被置0
      
    CCRH[7]:F/S->         I2C主模式选择，0：标准I2C；1：快速I2C；
    CCRH[6]:DUTY->        快速模式下的占空比。 0: Tlow/Thigh = 2; 1: Tlow/Thigh = 16/9;  CCR?
    CCRH[3:0]:CCR[11:8]
    CCRL[7:0]:CCR[7:0]    -> CCR[11:0] 时钟控制寄存器（主）。控制主模式下SCLH的时钟 参考pdf；
    只有I2C禁止时才能配置CCR;

     
     *************************************************************************/
     
    void I2C_Init(u32 OutputClockFrequencyHz, u16 OwnAddress, u8 DutyCycle,\
                 u8 Ack, u8 AddMode, u8 InputClockFrequencyMHz )
    {
      u16 result = 0x0004;/* Set the minimum allowed value */
      
      u8 tmpccrh = 0;
      /*------------------------- I2C FREQ Configuration ------------------------*/
      /* Clear frequency bits */
      I2C_FREQR &= (u8)(~MASK_I2C_FREQR_FREQ);  //清零FREQ
      /* Write new value */
      I2C_FREQR |= InputClockFrequencyMHz;      //FREQ[5:0] 支持从1Mhz到50Mhz之间设置
    
      /*--------------------------- I2C CCR Configuration ------------------------*/

      /* Disable I2C to configure TRISER */
      I2C_CR1 &= (u8)(~MASK_I2C_CR1_PE);

   
      /* Clear CCRH & CCRL */
      I2C_CCRH &= (u8)(~(MASK_I2C_CCRH_F_S  | MASK_I2C_CCRH_DUTY | MASK_I2C_CCRH_CCR ));
      I2C_CCRL &= (u8)(~I2C_CCRL_CCR);
      
        /* Calculate standard mode speed */
        result = (u16)((InputClockFrequencyMHz * 1000000) / (OutputClockFrequencyHz << (u8)1));
    
        /* Verify and correct CCR value if below minimum value */
        if (result < (u16)0x0004)
        {
          /* Set the minimum allowed value */
          result = (u16)0x0004;
        }
        /* Set Maximum Rise Time: 1000ns max in Standard Mode
        = [1000ns/(1/InputClockFrequencyMHz.10e6)]+1
        = InputClockFrequencyMHz+1 */
        I2C_TRISER = (u8)(InputClockFrequencyMHz + 1);
        
          /* Write CCR with new calculated value */
        I2C_CCRL = (u8)result;
        I2C_CCRH = (u8)(((u8)(result >> 8) & MASK_I2C_CCRH_CCR) | tmpccrh);
    
        /* Enable I2C */
        I2C_CR1 |= MASK_I2C_CR1_PE;
    
    //配置完成；
    
        /* Configure I2C acknowledgement */
        I2C_AcknowledgeConfig(Ack);
    
      /*--------------------------- I2C OAR Configuration 寻址地址寄存器 ------------------------*/
      I2C_OARL = (u8)(OwnAddress);
      I2C_OARH = (u8)((u8)AddMode |
                       MASK_I2C_OARH_ADDCONF |
                       (u8)((OwnAddress & (u16)0x0300) >> (u8)7));
    /*
    OARH[7]:ADDMODE->   寻址模式（从）  1:10位从地址，0:7位从地址
    OARH[6]:ADDCONF->   地址模式，只能置1；
    OARH[2:1]:ADD[9:8]->    add接口地址；
    OARL[7:1]:ADD[7:1]->    
    OARL[0]: ADD0 ->    10位地址则置0,7位不需要关心；
    */
    }
    /**************************************************************************
     * 函数名：I2C_AcknowledgeConfig
     * 描述  ：I2C应答配置函数
     * 输入  ：Ack   I2C总线应答模式
     *
     * 输出  ：无
     * 返回  ：无 
     * 调用  ：外部调用 
     
     CR2:
     [7]:SWRST->     软件复位；
     [3]:POS->      接收数据时应答的位置：
                        0->控制当前接收字节的应答或不应答；
                        1->控制下一个字节应答或不应答；
     [2]:ACK->      应答使能， 1:接收到一个字节后返回应答；
     [1]:STOP->     停止位产生，
     [0]:START->    起始位产生
     
     *************************************************************************/
    void I2C_AcknowledgeConfig(u8 Ack)
    {
    
        /* Enable the acknowledgement */
        I2C_CR2 |= MASK_I2C_CR2_ACK;
    
        if (Ack == I2C_ACK_CURR)
        {
          /* Configure (N)ACK on current byte */
          I2C_CR2 &= (u8)(~MASK_I2C_CR2_POS);
        }
    
     
    
    }
    /**************************************************************************
     * 函数名：I2C_EEInit
     * 描述  ：EEPROM初始化配置函数
               配置I2C总线频率为100K,从地址为0xA0,时钟的占空比是50%,
               应答模式为当前数据应答模式,7位从地址模式,系统输入时钟为16M
              
     * 输入  ：无
     *
     * 输出  ：无
     * 返回  ：无 
     * 调用  ：外部调用 
     *************************************************************************/
    void I2C_EEInit(void)
    {
       
      /* I2C Peripheral Enable */
      /* Enable I2C peripheral */
      I2C_CR1 |= MASK_I2C_CR1_PE;
      /* Apply I2C configuration after enabling it */
      I2C_Init(I2C_Speed, I2C1_SLAVE_ADDRESS7, I2C_DUTYCYCLE_2,\
                I2C_ACK_CURR, I2C_ADDMODE_7BIT, Input_Clock);
    }
    
    /**************************************************************************
     * 函数名：I2C_GenerateSTART
     * 描述  ：使能I2C起始位      
     * 输入  ：NewState
     *
     * 输出  ：无
     * 返回  ：无 
     * 调用  ：外部调用 
     *************************************************************************/
    void I2C_GenerateSTART(FunctionalState NewState)
    {
    
      if (NewState != DISABLE)
      {
        /* Generate a START condition */
        I2C_CR2 |= MASK_I2C_CR2_START;
      }
      else /* NewState == DISABLE */
      {
        /* Disable the START condition generation */
        I2C_CR2 &= (u8)(~MASK_I2C_CR2_START);
      }
    }
    /**************************************************************************
     * 函数名：I2C_GenerateSTOP
     * 描述  ：使能I2C结束位      
     * 输入  ：NewState
     *
     * 输出  ：无
     * 返回  ：无 
     * 调用  ：外部调用 
     *************************************************************************/
    void I2C_GenerateSTOP(FunctionalState NewState)
    {
    
      if (NewState != DISABLE)
      {
        /* Generate a STOP condition */
        I2C_CR2 |= MASK_I2C_CR2_STOP;
      }
      else /* NewState == DISABLE */
      {
        /* Disable the STOP condition generation */
        I2C_CR2 &= (u8)(~MASK_I2C_CR2_STOP);
      }
    }
    
    /**************************************************************************
     * 函数名：I2C_CheckEvent
     * 描述  ：检查I2C事件是否准确      
     * 输入  ：I2C_Event
     *
     * 输出  ：无
     * 返回  ：SUCCESS or  ERROR
     * 调用  ：外部调用 
     *************************************************************************/
    ErrorStatus I2C_CheckEvent(u16 I2C_Event)
    {
    
      u8 flag1 = 0;
      u8 flag2 = 0;
      ErrorStatus status = ERROR;
    
      /* Check the parameters */
      flag1 = I2C_SR1;
      flag2 = I2C_SR2;
    
      /* Check which SRx register must be read */
      if (((u16)I2C_Event & (u16)0x0F00) == 0x0700)
      {
        /* Check whether the last event is equal to I2C_EVENT */
        if (flag1 & (u8)I2C_Event)
        {
          /* SUCCESS: last event is equal to I2C_EVENT */
          status = SUCCESS;
        }
        else
        {
          /* ERROR: last event is different from I2C_EVENT */
          status = ERROR;
        }
      }
      else /* Returns whether the status register to check is SR2 */
      {
        if (flag2 & (u8)I2C_Event)
        {
          /* SUCCESS: last event is equal to I2C_EVENT */
          status = SUCCESS;
        }
        else
        {
          /* ERROR: last event is different from I2C_EVENT */
          status = ERROR;
        }
      }
    
      /* Return status */
      return status;
    
    }
    /**************************************************************************
     * 函数名：I2C_Send7bitAddress
     * 描述  ：设置I2C 7位地址模式      
     * 输入  ：Address    地址
               Direction  方向 主模式或者从模式
     *
     * 输出  ：无
     * 返回  ：无
     * 调用  ：外部调用 
     *************************************************************************/
    void I2C_Send7bitAddress(u8 Address, u8 Direction)
    {
     
      /* Clear bit0 (direction) just in case */
      Address &= (u8)0xFE;
    
      /* Send the Address + Direction */
      I2C_DR = (u8)(Address | (u8)Direction);
    }
    /**************************************************************************
     * 函数名：I2C_SendData
     * 描述  ：I2C发送一个字节      
     * 输入  ：Data
     *
     * 输出  ：无
     * 返回  ：无
     * 调用  ：外部调用 
     *************************************************************************/
    void I2C_SendData(u8 Data)
    {
      /* Write in the DR register the data to be sent */
      I2C_DR = Data;
    }
    /**************************************************************************
     * 函数名：I2C_ReceiveData
     * 描述  ：I2C接收一个字节      
     * 输入  ：无
     *
     * 输出  ：无
     * 返回  ：一个字节
     * 调用  ：外部调用 
     *************************************************************************/
    u8 I2C_ReceiveData(void)
    {
      /* Return the data present in the DR register */
      return ((u8)I2C_DR);
    }
    /**************************************************************************
     * 函数名：I2C_GetFlagStatus
     * 描述  ：获取I2C的状态标志      
     * 输入  ：Flag
     *
     * 输出  ：无
     * 返回  ：RESET or SET
     * 调用  ：外部调用 
     *************************************************************************/
    FlagStatus I2C_GetFlagStatus(u16 Flag)
    {
    
      FlagStatus bitstatus = RESET;
    
      switch ((u16)Flag & (u16)0xF000)
      {
    
          /* Returns whether the status register to check is SR1 */
        case 0x1000:
          /* Check the status of the specified I2C flag */
          if ((I2C_SR1 & (u8)Flag) != 0)
          {
            /* Flag is set */
            bitstatus = SET;
          }
          else
          {
            /* Flag is reset */
            bitstatus = RESET;
          }
          break;
    
          /* Returns whether the status register to check is SR2 */
        case 0x2000:
          /* Check the status of the specified I2C flag */
          if ((I2C_SR2 & (u8)Flag) != 0)
          {
            /* Flag is set */
            bitstatus = SET;
          }
          else
          {
            /* Flag is reset */
            bitstatus = RESET;
          }
          break;
    
          /* Returns whether the status register to check is SR3 */
        case 0x3000:
          /* Check the status of the specified I2C flag */
          if ((I2C_SR3 & (u8)Flag) != 0)
          {
            /* Flag is set */
            bitstatus = SET;
          }
          else
          {
            /* Flag is reset */
            bitstatus = RESET;
          }
          break;
    
        default:
          break;
    
      }
      /* Return the flag status */
      return bitstatus;
    }
    /**************************************************************************
     * 函数名：I2C_ClearFlag
     * 描述  ：清除I2C的状态标志      
     * 输入  ：Flag
     *
     * 输出  ：无
     * 返回  ：无
     * 调用  ：外部调用 
     *************************************************************************/
    void I2C_ClearFlag(u16 Flag)
    {
      u8 tmp1 = 0;
      u8 tmp2 = 0;
      u16 tmp3 = 0;
    
    
      /* Check the clear flag methodology index */
      tmp3 = ((u16)Flag & (u16)0x0F00);
      
       /* Clear the flag directly in the SR2 register */
      if(tmp3 == 0x0100)
      {
    	/* Clear the selected I2C flag */
          I2C_SR2 = (u8)(~(u8)Flag);
      }
      /* Flags that need a read of SR1 register and a dummy write in CR2 register to be cleared */
      else if(tmp3 == 0x0200)
      {
          /* Read the SR1 register */
          tmp1 = I2C_SR1;
          /* Dummy write in the CR2 register */
          I2C_CR2 = I2C_CR2;
      }
      /* Flags that need a read of SR1 register followed by a read of SR3 register to be cleared */
      else if(tmp3 == 0x0300)
      {
    	  /* 2 variables are used to avoid any compiler optimization */
          /* Read the SR1 register */
          tmp1 = I2C_SR1;
          /* Read the SR3 register */
          tmp2 = I2C_SR3;
      }
      /* Flags that need a read of SR1 register followed by a read of DR register to be cleared */
      else if(tmp3 == 0x0400)
      {
          /* 2 variables are used to avoid any compiler optimization */
          /* Read the SR1 register */
          tmp1 = I2C_SR1;
          /* Read the DR register */
          tmp2 = I2C_DR;
      }
    }
    
     /*******************************************************************************
    * Function Name  : I2C_EE_ByteWrite
    * Description    : Writes one byte to the I2C EEPROM.
    * Input          : - pBuffer : pointer to the buffer  containing the data to be 
    *                    written to the EEPROM.
    *                  - WriteAddr : EEPROM's internal address to write to.
    * Output         : None
    * Return         : None
    *******************************************************************************/
    void I2C_EE_ByteWrite(u8* pBuffer, u16 WriteAddr)
    {
      /* Send STRAT condition */
      I2C_GenerateSTART(ENABLE);
    
      /* Test on EV5 and clear it */
      while(!I2C_CheckEvent(I2C_EVENT_MASTER_START_SENT));  
    
      /* Send EEPROM address for write */
      I2C_Send7bitAddress(EEPROM_ADDRESS, I2C_DIRECTION_TX);
      
      /* Test on EV6 and clear it */
      while(!I2C_CheckEvent(I2C_EVENT_MASTER_ADDRESS_ACKED));
          
      /* Send Address (on 2 bytes) of first byte to be written & wait event detection */
      I2C_SendData((u8)(WriteAddr >> 8)); /* MSB */
      /* Test on EV8 and clear it */
      while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTING));
      I2C_SendData((u8)(WriteAddr)); /* LSB */
      /* Test on EV8 and clear it */
      while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTING));
     
      /* Send the byte to be written */
      I2C_SendData(*pBuffer); 
       
      /* Test on EV8_2 and clear it */
      while(!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));
      
      /* Send STOP condition */
      I2C_GenerateSTOP(ENABLE);
    }
    
    /*******************************************************************************
    * Function Name  : I2C_EE_PageWrite
    * Description    : Writes more than one byte to the EEPROM with a single WRITE
    *                  cycle. The number of byte can't exceed the EEPROM page size.
    * Input          : - pBuffer : pointer to the buffer containing the data to be 
    *                    written to the EEPROM.
    *                  - WriteAddr : EEPROM's internal address to write to.
    *                  - NumByteToWrite : number of bytes to write to the EEPROM.
    * Output         : None
    * Return         : None
    *******************************************************************************/
    void I2C_EE_PageWrite(u8* pBuffer, u16 WriteAddr, u8 NumByteToWrite)
    {
      /* While the bus is busy */
      while(I2C_GetFlagStatus(I2C_FLAG_BUSBUSY));
      
      /* Send START condition */
      I2C_GenerateSTART(ENABLE);
      
      /* Test on EV5 and clear it */
      while(!I2C_CheckEvent(I2C_EVENT_MASTER_START_SENT)); 
      
      /* Send EEPROM address for write */
      I2C_Send7bitAddress(EEPROM_ADDRESS, I2C_DIRECTION_TX);
    
      /* Test on EV6 and clear it */
      while(!I2C_CheckEvent(I2C_EVENT_MASTER_ADDRESS_ACKED));
      I2C_ClearFlag(I2C_FLAG_ADDRESSSENTMATCHED);  
    
      /* Send Address (on 2 bytes) of first byte to be written & wait event detection */
      I2C_SendData((u8)(WriteAddr >> 8)); /* MSB */
      /* Test on EV8 and clear it */
      while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTING));
      I2C_SendData((u8)(WriteAddr)); /* LSB */
      /* Test on EV8 and clear it */
      while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTING));  
    
    
      /* While there is data to be written */
      while(NumByteToWrite--)  
      {
        /* Send the current byte */
        I2C_SendData(*pBuffer); 
    
        /* Point to the next byte to be written */
        pBuffer++; 
      
        /* Test on EV8 and clear it */
        while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));
      }
    
      /* Send STOP condition */
      I2C_GenerateSTOP(ENABLE);
    }
    
    /*******************************************************************************
    * Function Name  : I2C_EE_BufferRead
    * Description    : Reads a block of data from the EEPROM.
    * Input          : - pBuffer : pointer to the buffer that receives the data read 
    *                    from the EEPROM.
    *                  - ReadAddr : EEPROM's internal address to read from.
    *                  - NumByteToRead : number of bytes to read from the EEPROM.
    * Output         : None
    * Return         : None
    *******************************************************************************/
    void I2C_EE_BufferRead(u8* pBuffer, u16 ReadAddr, u8 NumByteToRead)
    {  
        /* While the bus is busy */
      while(I2C_GetFlagStatus(I2C_FLAG_BUSBUSY));
      
      /* Generate start & wait event detection */
        I2C_GenerateSTART(ENABLE);
      /* Test on EV5 and clear it */
      while (!I2C_CheckEvent(I2C_EVENT_MASTER_START_SENT));
      
      /* Send slave Address in write direction & wait detection event */
        I2C_Send7bitAddress(EEPROM_ADDRESS, I2C_DIRECTION_TX);
       /* Test on EV6 and clear it */
        while (!I2C_CheckEvent(I2C_EVENT_MASTER_ADDRESS_ACKED));
        I2C_ClearFlag(I2C_FLAG_ADDRESSSENTMATCHED);
        
       /* Send Address of first byte to be read & wait event detection */
        I2C_SendData((u8)(ReadAddr >> 8)); /* MSB */
        /* Test on EV8 and clear it */
        while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));
        I2C_SendData((u8)(ReadAddr)); /* LSB */
      /* Test on EV8 and clear it */
        while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));
      
      /* Send STRAT condition a second time */  
      I2C_GenerateSTART(ENABLE);
        /* Test on EV5 and clear it */
       while (!I2C_CheckEvent(I2C_EVENT_MASTER_START_SENT));
      
      /* Send slave Address in read direction & wait event */
        I2C_Send7bitAddress(EEPROM_ADDRESS, I2C_DIRECTION_RX);
       /* Test on EV6 and clear it */
        while (!I2C_CheckEvent(I2C_EVENT_MASTER_ADDRESS_ACKED));
        I2C_ClearFlag(I2C_FLAG_ADDRESSSENTMATCHED);
      
      /* While there is data to be read */
      while(NumByteToRead)  
      {
        if(NumByteToRead == 1)
        {
          /* Disable Acknowledgement */
          I2C_AcknowledgeConfig(I2C_ACK_NONE);
          
          /* Send STOP Condition */
          I2C_GenerateSTOP(ENABLE);
        }
    
        /* Test on EV7 and clear it */
        if(I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_RECEIVED))  
        {      
          /* Read a byte from the EEPROM */
          *pBuffer = I2C_ReceiveData();
    
          /* Point to the next location where the byte read will be saved */
          pBuffer++; 
          
          /* Decrement the read bytes counter */
          NumByteToRead--;        
        }   
      }
    
      /* Enable Acknowledgement to be ready for another reception */
      I2C_AcknowledgeConfig(I2C_ACK_CURR);
    }
    
    
    
    uint8_t I2C_ReadRegister_SR1()
    {
      uint8_t temp;
      temp=I2C_SR1;
      return temp;
    }
    
    void I2C_EE_WaitEepromStandbyState(void)      
    {
      u8 SR1_Tmp = 0;
      do
      {
        /* Send START condition */
        I2C_GenerateSTART(ENABLE);
        /* Read I2C1 SR1 register */
        SR1_Tmp =I2C_ReadRegister_SR1();
        /* Send EEPROM address for write */
        I2C_Send7bitAddress(EEPROM_ADDRESS, I2C_DIRECTION_TX);;
      }while(!(I2C_ReadRegister_SR1()&0x02));
      
      /* Clear AF flag */
      I2C_ClearFlag(I2C_FLAG_ACKNOWLEDGEFAILURE);
    }
    
    
    
    /*******************************************************************************
    * Function Name  : I2C_EE_BufferWrite
    * Description    : Writes buffer of data to the I2C EEPROM.
    * Input          : - pBuffer : pointer to the buffer  containing the data to be 
    *                    written to the EEPROM.
    *                  - WriteAddr : EEPROM's internal address to write to.
    *                  - NumByteToWrite : number of bytes to write to the EEPROM.
    * Output         : None
    * Return         : None
    *******************************************************************************/
    void I2C_EE_BufferWrite(u8* pBuffer, u8 WriteAddr, u16 NumByteToWrite)
    {
      u8 NumOfPage = 0, NumOfSingle = 0, Addr = 0, count = 0;
    
      Addr = WriteAddr % Page_Byte_Size ; 
      count = Page_Byte_Size  - Addr;
      NumOfPage =  NumByteToWrite / Page_Byte_Size ;
      NumOfSingle = NumByteToWrite % Page_Byte_Size ;
     
      /* If WriteAddr is I2C_PageSize aligned  */
      if(Addr == 0) 
      {
        /* If NumByteToWrite < I2C_PageSize */
        if(NumOfPage == 0) 
        {
          I2C_EE_PageWrite(pBuffer, WriteAddr, NumOfSingle);
          I2C_EE_WaitEepromStandbyState();
        }
        /* If NumByteToWrite > I2C_PageSize */
        else  
        {
          while(NumOfPage--)
          {
            I2C_EE_PageWrite(pBuffer, WriteAddr, Page_Byte_Size ); 
        	I2C_EE_WaitEepromStandbyState();
            WriteAddr +=  Page_Byte_Size ;
            pBuffer += Page_Byte_Size ;
          }
    
          if(NumOfSingle!=0)
          {
            I2C_EE_PageWrite(pBuffer, WriteAddr, NumOfSingle);
            I2C_EE_WaitEepromStandbyState();
          }
        }
      }
      /* If WriteAddr is not I2C_PageSize aligned  */
      else 
      {
        /* If NumByteToWrite < I2C_PageSize */
        if(NumOfPage== 0) 
        {
          I2C_EE_PageWrite(pBuffer, WriteAddr, NumOfSingle);
          I2C_EE_WaitEepromStandbyState();
        }
        /* If NumByteToWrite > I2C_PageSize */
        else
        {
          NumByteToWrite -= count;
          NumOfPage =  NumByteToWrite / Page_Byte_Size ;
          NumOfSingle = NumByteToWrite % Page_Byte_Size ;	
          
          if(count != 0)
          {  
            I2C_EE_PageWrite(pBuffer, WriteAddr, count);
            I2C_EE_WaitEepromStandbyState();
            WriteAddr += count;
            pBuffer += count;
          } 
          
          while(NumOfPage--)
          {
            I2C_EE_PageWrite(pBuffer, WriteAddr, Page_Byte_Size );
            I2C_EE_WaitEepromStandbyState();
            WriteAddr += Page_Byte_Size ;
            pBuffer += Page_Byte_Size ;  
          }
          if(NumOfSingle != 0)
          {
            I2C_EE_PageWrite(pBuffer, WriteAddr, NumOfSingle); 
            I2C_EE_WaitEepromStandbyState();
          }
        }
      }  
    }
    
    
    /******************* (C) COPYRIGHT 风驰iCreate嵌入式开发工作室 *****END OF FILE****/
