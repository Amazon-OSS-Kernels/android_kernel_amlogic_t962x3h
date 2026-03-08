#include <config.h>
#include <common.h>
#include <malloc.h>
#include <asm/arch/io.h>
#include <asm/cpu_id.h>
#include <asm/arch/cpu.h>

#include <i2c.h>
#include <dm/device.h>


#define I2C_DEVICE_NUM 1 // I2C device
#define I2C_ADDR 0x1A
#define I2C_ADDR_LEN 4

#define msleep(a) mdelay(a)

static int WriteRegister(struct udevice *dev, unsigned int addr, unsigned int data, int data_len)
{
	int i;
	unsigned char buf[4] = {0};
	int ret;

	//printf("[%s, %d] addr=[0x%08x] data=[0x%x] data_len=%d\n", __FUNCTION__, __LINE__, addr, data, data_len);

	for(i = 0; i < data_len; i++) {
		buf[i] = (data >> (data_len - i -1) * 8) & 0xff;
		//printf("[%s, %d] buf[%d]=0x%02x\n", __FUNCTION__, __LINE__, i, buf[i]);
	}

	ret = i2c_write(dev, addr, buf, sizeof(buf));
	if(ret != 0) {
		printf("[%s, %d] error,ret=%d\n", __FUNCTION__, __LINE__, ret);
		return -1;
	}

	return 0;
}

static int ReadRegister(struct udevice *dev, unsigned int addr, unsigned int *data)
{
	unsigned char buf[4] = {0};
	int ret;

	//printf("[%s, %d] addr=[0x%08x]\n", __FUNCTION__, __LINE__, addr);

	ret = i2c_read(dev, addr, buf, 4);
	if(ret != 0) {
		printf("[%s, %d] error, ret=%d\n", __FUNCTION__, __LINE__, ret);
		return -1;
	}	

	// printf("[%s, %d] Data read: 0x%02X%02X%02X%02X\n", __FUNCTION__, __LINE__, buf[0], buf[1], buf[2], buf[3]);

	*data = (((buf[0] << 24) & 0xff000000) | ((buf[1] << 16) & 0x00ff0000) | ((buf[2] << 8) & 0x0000ff00) | (buf[3] & 0x000000ff));

	//printf("[%s, %d] %x\n", __FUNCTION__, __LINE__, *data);


	return 0;
}

static void SendWriteCmd(struct udevice *dev, unsigned int uiPageIndex)
{
	 WriteRegister(dev, 0xc0009b18, 0xAAAA0000, 4); //busy, lock
	 WriteRegister(dev, 0xc0009b08, uiPageIndex, 4); //page index
	 WriteRegister(dev, 0xc0009b04, 2, 4); //sending data
	 WriteRegister(dev, 0xc0009b00, 0x0B000000, 4); //command, start to program
}

static void SendEraseCmd(struct udevice *dev, unsigned int iLength)
{
	 WriteRegister(dev, 0xc0009b18, 0xAAAA0000, 4); //busy, lock
	 WriteRegister(dev, 0xc0009b10, iLength, 4); //the len
	 WriteRegister(dev, 0xc0009b04, 1, 4); //earse flash
	 WriteRegister(dev, 0xc0009b00, 0x0B000000, 4); //command, start to program 
}

static void SendFlashCmd(struct udevice *dev)
{

	WriteRegister(dev, 0xc0009b18, 0xAAAA0000, 4); //busy, lock
	WriteRegister(dev, 0xc0009b04, 3, 4); //write para2, start to flash the code
	WriteRegister(dev, 0xc0009b00, 0x0B000000, 4); //command, start to program
}

static void WaitForIdle(struct udevice *dev)
{
	unsigned int  uiStatus = 0;
	while(1) //wait spi finish
	{
		ReadRegister(dev, 0xC0009b18, &uiStatus);
		if((0xAAAA5555==uiStatus)) //idle
			break;
	}
}

static int get_tcon_version(struct udevice *dev)
{
	char buf[4]; // buffer for data
	int version;
	int ret;

	buf[0] = 0x03; // data address[31:24]
	buf[1] = 0x80; // data address[23:16]
	buf[2] = 0x00; // data address[15:8]
	buf[3] = 0x01; // data address[7:0]
	ret = i2c_write(dev, 0xc0009b00, buf, 4);
	if(ret != 0) {
		printf("[%s, %d] error,ret=%d\n", __FUNCTION__, __LINE__, ret);
		return -1;
	}

	msleep(1 * 1000); // wait for write to complete

	i2c_read(dev, 0xc0009b04, buf, 4);
	if(ret != 0) {
		printf("[%s, %d] error,ret=%d\n", __FUNCTION__, __LINE__, ret);
		return -1;
	}

	//printf("[%s, %d]Data read: 0x%02X%02X%02X%02X\n", __FUNCTION__, __LINE__, buf[0], buf[1], buf[2], buf[3]);

	version = ((buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3]);

	printf("tcon current version:%08X\n", version);

	return 0;
}

static int check_model_name_for_tcon_upgrade(void)
{
	char model_name[128] = {0};
	int ret;

	ret = idme_get_var_external("model_name", model_name, sizeof(model_name));
	if(0 != ret) {
		printf("[%s, %d] get model_name failed\n", __FUNCTION__, __LINE__);
		return 0;
	}

	if (!strcmp(model_name, "/tvconfig/65A51HUF/DVT_65A51HUF_vb1.ini") ||
		!strcmp(model_name, "/tvconfig/65A51HUF/EVT_65A51HUF_vb1.ini") ||
		!strcmp(model_name, "/tvconfig/65A51HUF/HVT_65A51HUF_vb1.ini") ||
		!strcmp(model_name, "/tvconfig/65A51HUF/PVT_65A51HUF_vb1.ini") ||
		!strcmp(model_name, "/tvconfig/65A51HUF/DVT_65A51HUF.ini") ||
		!strcmp(model_name, "/tvconfig/65A51HUF/EVT_65A51HUF.ini") ||
		!strcmp(model_name, "/tvconfig/65A51HUF/HVT_65A51HUF.ini") ||
		!strcmp(model_name, "/tvconfig/65A51HUF/PROTO_65A51HUF.ini") ||
		!strcmp(model_name, "/tvconfig/65A51HUF/PVT_65A51HUF.ini"))
		return 1;
	else
		return 0;
}


int Upgrade_flash(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int ret, i;
	unsigned int filesize = 0;
	unsigned int iLength = 0;
	unsigned int iPages = 0;
	unsigned int iLeft = 0;
	unsigned int uiDBAddDualImg =0;
	unsigned int uiChecksum = 0;
	unsigned int uiReadChecksum = 0xff; 
	char *m_DualData = NULL;
	char *m_bData = NULL;
	unsigned int n;
	ulong addr = 0;
	int *m_block = NULL;
	int *m_leftblock = NULL;
	struct udevice *dev;
	int cur_percents;
	int j = 1;


	ret = check_model_name_for_tcon_upgrade();
	if(1 != ret) {
		printf("[%s, %d]no need to upgrade tcon sw\r", __FUNCTION__, __LINE__);
		goto _err;
	}

	if(argc != 2) {
		printf("[%s, %d] error,argc=%d", __FUNCTION__, __LINE__, argc);
		goto _err;
	}

	filesize = (int)getenv_hex("filesize", 0);

	//get KV7636_Dual_Image.bin
	m_bData = simple_strtoul(argv[1], NULL, 16);
	if(NULL == m_bData) {
		printf("[%s, %d] error,get m_bData failed\n", __FUNCTION__, __LINE__);
		goto _err;
	}

	video_res_prepare_for_upgrade(NULL);
	/*The upgrade process needs to take place 3 seconds later after TCON
	power on to avoid interrupted by TCON Demura initialization*/
	msleep(3 * 1000);

	show_logo_to_report_burning_ex(0);
	ret = i2c_get_chip_for_busnum(I2C_DEVICE_NUM, I2C_ADDR, &dev);
	if(ret != 0) {
		printf("[%s, %d] error,ret=%d\n", __FUNCTION__, __LINE__, ret);
		goto _err;
	}

	ret = i2c_set_chip_offset_len(dev, I2C_ADDR_LEN);
	if(ret != 0) {
		printf("[%s, %d] error,ret=%d\n", __FUNCTION__, __LINE__, ret);
		goto _err;
	}

	//get_tcon_version(dev);

	//check bin file size,
	if (filesize < 0x40000)
	{
		printf( "[%s, %d] Too little bin file filesize=0x%x\n", __FUNCTION__, __LINE__, filesize);
		uiDBAddDualImg = 0;
		goto _err;
	} else if ((filesize > 0x40000) && (filesize < 0x80000)) {
		uiDBAddDualImg = 0x40000; //256KB
	} else if((filesize > 0x80000) && (filesize < 0x100000)) {
		uiDBAddDualImg = 0x80000; //512KB
	} else if((filesize > 0x400000) && (filesize < 0x800000)) {
		uiDBAddDualImg = 0x400000; //4MB
	} else {
		printf( "[%s, %d]no match bin file filesize=0x%x\n", __FUNCTION__, __LINE__, filesize);
		uiDBAddDualImg = 0;
		goto _err;
	}

	/*ReadRegister(dev, 0xC0009c20, &uiChecksum);
	if(((uiDBAddDualImg == 0x40000) && (0x76364096 == uiChecksum))
		||((uiDBAddDualImg == 0x80000) && (0x00 == uiChecksum))
		||((uiDBAddDualImg == 0x400000) && (0x76368192 == uiChecksum))) {
		printf( "[%s, %d]bin file&program match, uiDBAddDualImg=0x%x, uiChecksum=0x%x\n", __FUNCTION__, __LINE__, uiDBAddDualImg, uiChecksum);
	} else {
		printf( "[%s, %d]bin file&program nomatch, uiDBAddDualImg=0x%x, uiChecksum=0x%x\n", __FUNCTION__, __LINE__, uiDBAddDualImg, uiChecksum );
		goto _err;
	}*/

	//only upgrade the second image, alloc tmp buffer
	m_DualData = (char *)malloc(filesize - uiDBAddDualImg);
	if(NULL == m_DualData) {
		printf( "[%s, %d] malloc failed\n", __FUNCTION__, __LINE__);
		goto _err;
	}

	// copy the second image to tmp buffer
	for (n = 0; n < filesize - uiDBAddDualImg; n++)
		m_DualData[n] = m_bData[n + uiDBAddDualImg];

	iLength = filesize - uiDBAddDualImg;
	iPages = iLength / 200; // 200 bytes each page
	iLeft = iLength % 200; // size of the last page
	m_block= (int *)malloc(50); // buffer for one page
	if(NULL == m_block) {
		printf( "[%s, %d] malloc failed\n", __FUNCTION__, __LINE__);
		goto _err;
	}
	m_leftblock = (int *)malloc(iLeft / 4); // buffer for the last page
	if(NULL == m_leftblock) {
		printf( "[%s, %d] malloc failed\n", __FUNCTION__, __LINE__);
		goto _err;
	}

	printf( "[%s, %d] step 1\n", __FUNCTION__, __LINE__);
	SendEraseCmd(dev, iLength); // erase SPI flash with length to write
	printf( "[%s, %d] step 2\n", __FUNCTION__, __LINE__);
	WaitForIdle(dev); // wait for done
	printf( "[%s, %d] step 3\n", __FUNCTION__, __LINE__);

	for (i = 0; i < iPages; i++)
	{
		uiChecksum=0; // initialize checksum
		uiReadChecksum=0xff; // initialize readback checksum
		printf( "[%s, %d] step 3.1 i=%d, iPages=%d\n", __FUNCTION__, __LINE__, i, iPages);
		while (uiReadChecksum != uiChecksum) {
			uiChecksum=0; 
			uiReadChecksum=0xff;
			// send one page
			for(n=0; n<50; n++)
			{
				// byte reorder
				m_block[n] = (m_DualData[200 * i + n * 4 + 0] * 0x1000000) +
				(m_DualData[200 * i + n * 4 + 1] * 0x10000) +
				(m_DualData[200 * i + n * 4 + 2] * 0x100) +
				m_DualData[200 * i + n * 4 + 3];
				// send 4 bytes
				WriteRegister(dev, (0xc0009b2c + n * 4), m_block[n], 4);
				// calculate checksum
				uiChecksum = uiChecksum + m_DualData[200 * i + n * 4 + 0] +
				m_DualData[200 * i + n * 4 + 1] +
				m_DualData[200 * i + n * 4 + 2] +
				m_DualData[200 * i + n * 4 + 3];
			}

			SendWriteCmd(dev, i); // execute write
			//printf( "[%s, %d] step 3.1\n", __FUNCTION__, __LINE__);
			WaitForIdle(dev); // wait for done
			//printf( "[%s, %d] step 3.2\n", __FUNCTION__, __LINE__);
			// read back checksum
			ReadRegister(dev, 0xC0009b1C, &uiReadChecksum);
			cur_percents = i * 100 / iPages;
			if(cur_percents == j * 5 ) {
				show_logo_to_report_burning_ex(cur_percents);
				j ++;
			}
			printf("[%s, %d] step 3.2 iPages=%d, uiChecksum=0x%x, uiReadChecksum=0x%x, cur_percents=%d %d\n",
								__FUNCTION__, __LINE__, iPages, uiChecksum, uiReadChecksum, cur_percents, j * 5);
		}
	}

	printf( "[%s, %d] step 4\n", __FUNCTION__, __LINE__);
	WaitForIdle(dev); // wait for done
	printf( "[%s, %d] step 5\n", __FUNCTION__, __LINE__);
	// send the last page
	if (iLeft > 0) {
		for(n=0;n<(iLeft/4);n++) {
			m_leftblock[n] = (m_DualData[200 * i + n * 4 + 0] * 0x1000000) +
			(m_DualData[200 * i + n * 4 + 1] * 0x10000) +
			(m_DualData[200 * i + n * 4 + 2] * 0x100) +
			m_DualData[200 * i + n * 4 + 3];
			WriteRegister(dev, (0xc0009b2c + n * 4), m_leftblock[n], 4);
			printf( "[%s, %d] step 5.1 n=%d, iLeft=%d, iLeft/4=%d\n", __FUNCTION__, __LINE__, n, iLeft, iLeft/4);
		}
	}

	show_logo_to_report_burning_ex(100);

	printf( "[%s, %d] step 6\n", __FUNCTION__, __LINE__);
	SendWriteCmd(dev, i); // execute write
	i++;
	WaitForIdle(dev); // wait for done
	SendFlashCmd(dev); // write to spi flash
	printf( "[%s, %d] step 7\n", __FUNCTION__, __LINE__);
	msleep(5 * 1000); //Delay 5S for write flash safely
	printf( "[%s, %d] step 8\n", __FUNCTION__, __LINE__);
	WaitForIdle(dev); // wait for done
	printf( "[%s, %d] step 9\n", __FUNCTION__, __LINE__);

	printf( "[%s, %d] tcon sw upgrade success\n", __FUNCTION__, __LINE__);

	show_logo_to_report_burn_success();
	run_command("sleep 2;reset", 1);
	return CMD_RET_SUCCESS;
_err:
	show_logo_report_burn_ui_error();
	return CMD_RET_FAILURE;
}

U_BOOT_CMD(
	i2c_tcon_upgrade, 2, 0,	Upgrade_flash,
	"run uboot i2c tcon upgrade from usb device by tcon bin file",
	"[addr] - tcon bin file starting at addr\n"
);

