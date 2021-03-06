#include "mpu6050.h"

u8 mpu6050::Init(bool wait)
{
	if(wait)
		mI2C->WaitTransmitComplete();
	//向命令队列里添加命令
	u8 IIC_Write_Temp;
	IIC_Write_Temp=0;
	mI2C->AddCommand(MPU6050_ADDRESS,PWR_MGMT_1,&IIC_Write_Temp,1,0,0);
	IIC_Write_Temp=2;
	mI2C->AddCommand(MPU6050_ADDRESS,INT_PIN_CFG,&IIC_Write_Temp,1,0,0);
	IIC_Write_Temp=7;
	mI2C->AddCommand(MPU6050_ADDRESS,SMPLRT_DIV,&IIC_Write_Temp,1,0,0);
	IIC_Write_Temp=7;
	mI2C->AddCommand(MPU6050_ADDRESS,USER_CTRL,&IIC_Write_Temp,1,0,0);
	IIC_Write_Temp=6;
	mI2C->AddCommand(MPU6050_ADDRESS,MPU6050_CONFIG,&IIC_Write_Temp,1,0,0);
	IIC_Write_Temp=0x18;
	mI2C->AddCommand(MPU6050_ADDRESS,GYRO_CONFIG,&IIC_Write_Temp,1,0,0);//+-2000 °/s
	IIC_Write_Temp=9;
	mI2C->AddCommand(MPU6050_ADDRESS,ACCEL_CONFIG,&IIC_Write_Temp,1,0,0);//+-4g   5Hz
	
	mI2C->StartCMDQueue();//开始执行命令
	BypassMode();
	if(wait)
		if(!mI2C->WaitTransmitComplete(true,true,false))
			return false;
	return true;
}


bool mpu6050::BypassMode(bool wait)
{
	u8 IIC_Write_Temp;
	if(wait)
		mI2C->WaitTransmitComplete();
	
	//向命令队列里添加命令
	IIC_Write_Temp=0x0D;
	mI2C->AddCommand(MPU6050_ADDRESS,I2C_MST_CTRL,&IIC_Write_Temp,1,0,0);// config the mpu6050  master bus rate as 400kHz
	IIC_Write_Temp=0x00;
	mI2C->AddCommand(MPU6050_ADDRESS,USER_CTRL,&IIC_Write_Temp,1,0,0);//Disable mpu6050 Master mode
	IIC_Write_Temp=0x02;
	mI2C->AddCommand(MPU6050_ADDRESS,INT_PIN_CFG,&IIC_Write_Temp,1,0,0); //为了直接接到辅助IIC接口    Pass-Through Mode：I2C_MST_EN=0(USER_CTRL bit5)  I2C_BYPASS_EN =1(INT_PIN_CFG bit1)即可直接接到旁路IIC，注意修改IIC读/写的从机地址
																		//Enable bypass mode
	if(!mI2C->StartCMDQueue())//开始执行命令
		return false;
	if(wait)
		if(!mI2C->WaitTransmitComplete(true,true,false))
			return false;
	return true;
}
#ifdef MPU6050_USE_TASKMANAGER
mpu6050::mpu6050(I2C &i2c,u16 maxUpdateFrequency)
{
	mI2C=&i2c;
	mMaxUpdateFrequency=maxUpdateFrequency;
}
#else
mpu6050::mpu6050(I2C &i2c)
{
	mI2C=&i2c;
}
#endif

#ifdef MPU6050_USE_TASKMANAGER
void mpu6050::SetMaxUpdateFrequency(u16 maxUpdateFrequency)
{
	mMaxUpdateFrequency=maxUpdateFrequency;
}
u16 mpu6050::GetMaxUpdateFrequency()
{
	return mMaxUpdateFrequency;
}
#endif
mpu6050::~mpu6050()
{
	
}


Vector3<int> mpu6050::GetAccRaw()
{
	Vector3<int> temp;
	temp.x=((int16_t)(mData.acc_XH<<8)) | mData.acc_XL;
	temp.y=((int16_t)(mData.acc_YH<<8)) | mData.acc_YL;
	temp.z=((int16_t)(mData.acc_ZH<<8)) | mData.acc_ZL;
	return temp;
}
Vector3<int> mpu6050::GetGyrRaw()
{
	Vector3<int> temp;
	temp.x=((int16_t)(mData.gyro_XH<<8))|mData.gyro_XL;
	temp.y=((int16_t)(mData.gyro_YH<<8))|mData.gyro_YL;
	temp.z=((int16_t)(mData.gyro_ZH<<8))|mData.gyro_ZL;
	return temp;
}
int mpu6050::GetTempRaw()
{
	return ((int16_t)(mData.imu_TH<<8))|mData.imu_TL;
}

Vector3<float> mpu6050::Get_Angle()
{
	Vector3<float> temp;
	return temp;
}

u8 mpu6050::Update(bool wait,Vector3<int> *acc, Vector3<int> *gyro)
{
	#ifdef MPU6050_USE_TASKMANAGER
	static double oldTime=0,timeNow=0;
	
	timeNow = TaskManager::Time();
	if((timeNow-oldTime)< (1.0/this->mMaxUpdateFrequency))
	{
		return MOD_BUSY;
	}
	oldTime=timeNow;
	#endif
	if(wait)
		mI2C->WaitTransmitComplete();
	
	mI2C->AddCommand(MPU6050_ADDRESS,ACCEL_XOUT_H,0,0,&mData.acc_XH,14);//获取IMU加速度、角速度、IMU温度计数值
	/*mI2C->AddCommand(MPU6050_ADDRESS,MPU6050_CONFIG,0,0,&mRegisterConfig,1);//获取CONFIGH寄存器状态 无法读取，虽然手册上写的可读可写 */
	mI2C->AddCommand(MPU6050_ADDRESS,0x75,0,0,&mWhoAmI,1);//获取who am i ，检测mpu是否存在
	if(!mI2C->StartCMDQueue())
	{
		mI2C->Init();
		this->Init(wait);
		return MOD_ERROR;
	}
	if(GetHealth()==2)//未初始化状态
	{
		if(!this->Init(wait))//iic总线错误
		{
			return MOD_ERROR;
		}
	}
	//如果需要等待
	if(wait)
		if(!mI2C->WaitTransmitComplete(true,true,false))
			return MOD_ERROR;
		
	//获取数据的值
	if(acc!=0)
		*acc=this->GetAccRaw();
	if(gyro!=0)
		*gyro=this->GetGyrRaw();

	return MOD_READY;
}



///////////////////////////////////
///检测mpu6050状态
///@retval 0:失去连接   1:正常状态     2:未初始化
//////////////////////////////////
u8 mpu6050::GetHealth()
{
	if(!mI2C->IsHealth()||mWhoAmI!=0x68)//iic 错误或者mpu错误
		return 0;
	if((mData.acc_XH==0) && (mData.acc_XL==0) && (mData.acc_YH==0) && (mData.acc_ZH==0))
	{
		return 2;
	}
	return 1;
}


