/*
 * secure_boot.c
 *
 * Copyright 2020 - 2022 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#include <asm-generic/gpio.h>
#include <asm/arch/secure_apb.h>
#include <asm/io.h>
#include <common.h>
#include <ctype.h>
#include "amzn_secure_boot.h"
#if defined(UFBL_FEATURE_UNLOCK)
#include <amzn_unlock.h>
#include <u-boot/sha256.h>
#endif
#if defined(UFBL_FEATURE_ONETIME_UNLOCK)
#include "onetime_unlock_key.h"
#include <amzn_onetime_unlock.h>
#endif
#if defined(UFBL_FEATURE_TEMP_UNLOCK)
#include <config.h>
#include <asm/arch/io.h>
#include <malloc.h>
#include <asm/arch/secure_apb.h>
#include <tee.h>
#include <errno.h>
#include <amzn_temp_unlock.h>
#include "amzn_temp_unlock_aml_impl.h"
// temp unlock share the same key with onetime unlock
#include "onetime_unlock_key.h"
#endif

#define ANTIROLLBACK_MVN_REG1	(AO_SEC_SD_CFG11)
#define ANTIROLLBACK_MVN_REG2	(AO_SEC_SD_CFG13)

bool secure_boot_enabled(void)
{
	const unsigned long cfg10 = readl(AO_SEC_SD_CFG10);
	return ( (cfg10 & (0x1<< 4)) ? true : false );
	/* 4th bit indicates secure boot status */
}

void read_arb_version(uint32_t *mvn_1_p, uint32_t *mvn_2_p)
{
	uint32_t mvn_1 = 0;
	uint32_t mvn_2 = 0;
	*mvn_1_p = mvn_1 = readl(ANTIROLLBACK_MVN_REG1);
	*mvn_2_p = mvn_2 = readl(ANTIROLLBACK_MVN_REG2);
	printf("mvn_1=0X%x,mvn_2=0X%x \n",mvn_1,mvn_2);
}

bool anti_rollback_enabled(void)
{
	const unsigned long cfg10 = readl(AO_SEC_SD_CFG10);
	return ( (cfg10 & (0x1<< 25)) ? true : false );
}


const char *amzn_target_device_name(void)
{
	static char target_name[16]={0};
	int i=0;
        if(strlen(CONFIG_DEVICE_PRODUCT) >= sizeof(target_name)) {
                return NULL;
        }
	strncpy(target_name,CONFIG_DEVICE_PRODUCT,strlen(CONFIG_DEVICE_PRODUCT));
	while(i<strlen(target_name)){
		target_name[i]=tolower(target_name[i]);
		i++;
	}
	printf("target_device_name is %s\n",target_name);
	return target_name;
}

int amzn_target_device_type(void)
{

	/* Is anti-rollback enabled? */
	if (anti_rollback_enabled() == true) {
		return AMZN_PRODUCTION_DEVICE;
	}
	else {
		return AMZN_ENGINEERING_DEVICE;
	}
}

bool amzn_target_is_lockdown()
{
	bool ret = true;
	/* Is this an engineering device? */
	if (amzn_target_device_type() == AMZN_ENGINEERING_DEVICE)
		ret = false;

	/* Are we un-locked? */
	if (amzn_target_is_unlocked())
		ret = false;

#if defined(UFBL_FEATURE_ONETIME_UNLOCK)
	if (amzn_target_is_onetime_unlocked())
		ret = false;
#endif

#if defined(UFBL_FEATURE_TEMP_UNLOCK)
	if (amzn_target_is_temp_unlocked())
		ret = false;
#endif

	return ret;
}
#if defined(UFBL_FEATURE_UNLOCK)
#define CHIPID_UPPER		(6)
#define CHIPID_LOWER		(7)
#define CHIPID_BUF_SIZE		(16)
#define HASH_BUF_SIZE		(32)

int amzn_get_unlock_code(unsigned char *code, unsigned int *len)
{
	sha256_context ctx;
	uint8_t buff[CHIPID_BUF_SIZE] = {0};
	uint8_t hash[HASH_BUF_SIZE] = {0};

	if (!code || !len || *len < (16 + 1))
		return -1;

	if (get_chip_id(&buff[0], sizeof(buff)))
		return -1;
	/**
	 * To sync with Amazon serial number from kernel's /proc/cpuinfo,
	 * the unlock_code is low 64 bit of sha256(SoC Chipid 128bits).
	 */
	sha256_starts(&ctx);
	sha256_update(&ctx, &buff[0], sizeof(buff));
	sha256_finish(&ctx, &hash[0]);
	u32 *hashcode = (u32 *) &hash[0];
	snprintf(code, CHIPID_BUF_SIZE+1, "%08x%08x",be32_to_cpu(hashcode[CHIPID_UPPER]),
			be32_to_cpu(hashcode[CHIPID_LOWER]));

	*len = 16;
	return 0;
}

const unsigned char *amzn_get_unlock_key(unsigned int *key_len)
{
	/* hazel_unlock.pub.der */
	static const  unsigned char hazel_unlock_pub_der[] = {
		0x30, 0x82, 0x01, 0x22, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
		0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0f, 0x00,
		0x30, 0x82, 0x01, 0x0a, 0x02, 0x82, 0x01, 0x01, 0x00, 0xc2, 0xca, 0x87,
		0xf7, 0xc5, 0xb7, 0x73, 0x79, 0xab, 0x4c, 0x77, 0x23, 0xf7, 0x68, 0x3f,
		0xbb, 0xf3, 0xfe, 0xc8, 0x78, 0xc6, 0xcc, 0xc3, 0x74, 0x3e, 0x5a, 0xe7,
		0x7d, 0x01, 0xdd, 0xb3, 0x71, 0xc1, 0xcf, 0xba, 0x49, 0x1b, 0x30, 0x3b,
		0x5d, 0x19, 0xa8, 0x5a, 0xb9, 0x6b, 0xbf, 0xcc, 0x75, 0xf1, 0x2b, 0x77,
		0xeb, 0x19, 0x74, 0xca, 0x84, 0x86, 0x65, 0xa3, 0x85, 0x46, 0x9b, 0xe5,
		0x15, 0xb2, 0xf7, 0xa8, 0x3c, 0xff, 0xe0, 0xac, 0x64, 0xf1, 0xbe, 0xe1,
		0xf8, 0x79, 0x5d, 0x0c, 0x2e, 0xf9, 0x70, 0x23, 0xc4, 0x34, 0x68, 0x93,
		0x18, 0xb5, 0xb6, 0xd7, 0x2a, 0x84, 0x99, 0xc8, 0x9b, 0x2c, 0x67, 0xcd,
		0x32, 0x54, 0x2b, 0xd5, 0xfc, 0x89, 0x35, 0x8e, 0xb5, 0xab, 0x63, 0x5d,
		0x47, 0xa7, 0x67, 0x43, 0x00, 0x26, 0x19, 0x56, 0xc9, 0xd5, 0xee, 0x1b,
		0xca, 0x24, 0x53, 0x8c, 0x7c, 0x06, 0x3a, 0xe6, 0x54, 0x75, 0x06, 0x43,
		0xa5, 0x52, 0x27, 0xa9, 0xec, 0x6b, 0x37, 0xaf, 0x07, 0xf5, 0x2a, 0x08,
		0x86, 0xe4, 0x61, 0x3f, 0xf7, 0xb7, 0xac, 0x8a, 0x55, 0x15, 0x4e, 0x84,
		0xc3, 0x93, 0xa4, 0x73, 0x93, 0xa6, 0x87, 0x30, 0x85, 0xe9, 0x33, 0x6c,
		0x5b, 0xdb, 0xe0, 0x17, 0x38, 0x5d, 0x8f, 0xa6, 0x98, 0xda, 0x0f, 0x7e,
		0x75, 0xe3, 0x1a, 0xf1, 0x73, 0x4b, 0xda, 0x6d, 0x55, 0x60, 0x2f, 0xb5,
		0x28, 0xe7, 0x3a, 0x67, 0x09, 0xd4, 0x41, 0x45, 0x3c, 0xf6, 0x8f, 0x26,
		0x3b, 0x6d, 0xd3, 0x1c, 0xe3, 0x8f, 0x50, 0x53, 0x01, 0x36, 0xa5, 0x69,
		0x1a, 0xcb, 0xda, 0xc1, 0xe0, 0xa6, 0xcf, 0xd5, 0x2f, 0xec, 0x2b, 0x55,
		0xe5, 0xa7, 0x90, 0x40, 0xf4, 0x0e, 0x82, 0x93, 0xe0, 0xa7, 0x8f, 0x18,
		0xe0, 0xe6, 0xd5, 0xff, 0x3c, 0x25, 0x94, 0x75, 0xd7, 0x3b, 0x5c, 0x11,
		0x97, 0x02, 0x03, 0x01, 0x00, 0x01
	};


	const int unlock_key_size = sizeof(hazel_unlock_pub_der);
	if (!key_len)
		return NULL;

	*key_len = unlock_key_size;

	return hazel_unlock_pub_der;
}

#ifdef UFBL_FEATURE_SECURE_FLASHING
static unsigned int get_random_number(int s)
{
	unsigned int seed = (unsigned int)get_timer(0) + s;
	seed ^= (seed << 13);
	seed ^= (seed >> 17);
	seed ^= (seed << 5);
	return seed;
}

int amzn_get_sec_flashing_code(unsigned char *code, unsigned int *len)
{
	static unsigned char sec_flashing_code[UNLOCK_CODE_LEN + 1] = {0};
	static unsigned char code_generated = 0;
	unsigned int unlock_code_len = UNLOCK_CODE_LEN;
	unsigned int rand1, rand2;

	if (!code || !len || *len < UNLOCK_CODE_LEN)
		return -1;

	if (!code_generated) {
		if(amzn_get_unlock_code(sec_flashing_code, &unlock_code_len)){
			return -1;
		}
		rand1 = get_random_number(0x1AB126);
		rand2 = get_random_number(rand1);
		sprintf(&sec_flashing_code[16], "%08x%08x", rand1, rand2);
		code_generated = 1;
	}
	memcpy(code, sec_flashing_code, UNLOCK_CODE_LEN);
	*len = UNLOCK_CODE_LEN;

	return 0;
}

const unsigned char *amzn_get_sec_flashing_root_pubkey(unsigned int *key_len)
{
	static const unsigned char root_key[] =
		"\x30\x82\x01\x22\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01"
		"\x01\x05\x00\x03\x82\x01\x0f\x00\x30\x82\x01\x0a\x02\x82\x01\x01"
		"\x00\xb9\x20\xa0\x41\x68\x31\x06\xf4\x97\x32\x0d\xfc\x3a\x6c\x6a"
		"\xe9\x41\x6e\xfd\x57\x47\xd3\xdc\xef\xd7\x75\x24\x79\x33\x39\x71"
		"\x02\xd8\x72\x37\xd0\xdc\xc4\xed\x3d\x40\x6a\x20\xfa\xc7\x3f\x8e"
		"\x61\x81\xee\xff\x83\xaf\xbe\xb4\x51\xd8\xd2\x01\x42\xd5\x16\xda"
		"\x57\x12\x49\xaa\x3b\x50\xc7\x7e\xec\x47\x0b\x96\x31\xde\xa7\x4a"
		"\x9d\x7f\x7a\x44\xb3\xc2\x62\x8c\xa5\xe0\x0d\x48\xd9\x50\xa9\x69"
		"\xdc\x29\x42\x22\x33\xbb\xb0\x87\xfa\x51\x27\xd5\xf7\x11\x0c\x17"
		"\xbc\xe5\x5c\xa5\x60\x41\xd7\x07\xc0\xc2\x23\x65\x10\xb0\xc2\xa9"
		"\x12\xc4\x56\x80\xb9\xab\xf9\x1a\x89\xf0\x69\x98\xb3\xce\x9d\x22"
		"\x5a\xdf\xf2\x72\xf1\x93\x6e\xf9\xf4\x43\x87\xd0\x7c\xea\x21\x1b"
		"\xfd\xd9\xeb\xda\xba\x1c\x2a\x40\x3b\x3f\x22\xa8\xbc\x18\x5e\x85"
		"\x00\x84\xad\xb5\x88\xd1\x7f\x3d\x96\x73\x9a\x04\x78\xe5\x10\x5f"
		"\xdf\xed\x8c\xe2\x41\x8f\x21\x64\xf7\x54\xa7\xf2\xec\xc1\xe3\x09"
		"\x6e\x5f\xca\xdb\x78\x37\x29\xc0\x2a\xe1\xc5\x77\x32\xce\x5a\x0d"
		"\x4a\x30\xfd\x27\x8d\xa6\x11\x87\x62\xf6\x43\x44\xa7\x3a\xc6\x80"
		"\x03\xfc\x61\xfc\x6d\xae\xc5\x55\xcf\x5c\xee\x04\x24\x31\xb6\x7a"
		"\x5b\x02\x03\x01\x00\x01";
	const int key_size = sizeof(root_key);
	if (!key_len)
		return NULL;
	*key_len = key_size;
	return root_key;
}
#endif /* UFBL_FEATURE_SECURE_FLASHING */


#if defined(UFBL_FEATURE_ONETIME_UNLOCK)
int amzn_get_one_tu_code(unsigned char *code, unsigned int *len)
{
	static unsigned char code_generated = 0;
	static unsigned char one_tu_code[ONETIME_UNLOCK_CODE_LEN + 1] = {0};

	if (!code || !len || *len < ONETIME_UNLOCK_CODE_LEN)
		return -1;

	if (!code_generated) {
/**
 * Different SoC/Product may have different scheme to add entropy into PRNG.
 * 
 */
#define ENTROPY_LEN (UNLOCK_CODE_LEN + 8)
		static unsigned char entropy[ENTROPY_LEN] = {0};
		unsigned int unlock_code_len = UNLOCK_CODE_LEN;
		if (amzn_get_unlock_code(entropy, &unlock_code_len)) {
			return -1;
		}
		sprintf(&entropy[unlock_code_len], "%08x", get_timer(0));
/**
 * amzn_get_onetime_random_number will return a binary string which is not readable.
 * The binary string cannot be returned via fastboot so using base64 encode it.
 */
// compute how many random bytes do we need so that the converted size is the target length
#define RANDOM_BYTES_SIZE (ONETIME_UNLOCK_CODE_LEN + 3) / 4 * 3
		uint8_t random_bytes[RANDOM_BYTES_SIZE] = {0};
		unsigned int out_len = sizeof(one_tu_code);

		if (amzn_get_onetime_random_number(entropy, strlen(entropy),
						random_bytes, sizeof(random_bytes)))
			return -1;

		if (amzn_onetime_unlock_b64_encode(random_bytes, sizeof(random_bytes),
						one_tu_code, &out_len)) {
			return -1;
		}
		code_generated = 1;
	}
	memcpy(code, one_tu_code, ONETIME_UNLOCK_CODE_LEN);
	*len = ONETIME_UNLOCK_CODE_LEN;
	return 0;
}

int amzn_get_onetime_unlock_root_pubkey(const unsigned char **key, unsigned int *key_len)
{
	char oem_data[64] = {0};
	idme_get_var_external("oem_data", oem_data, sizeof(oem_data));

	static const unsigned char onetime_unlock_key[] = ONETIME_UNLOCK_KEY;
	const int onetime_unlock_key_size = sizeof(onetime_unlock_key);
	static const unsigned char onetime_unlock_key_tm[] = ONETIME_UNLOCK_KEY_TM;
	const int onetime_unlock_key_size_tm = sizeof(onetime_unlock_key_tm);

	if (!key || !key_len)
		return -1;

	if (strstr(oem_data, "hazel-tm") != NULL){ //hazel-tm
		*key_len = onetime_unlock_key_size_tm;
		*key = onetime_unlock_key_tm;
	}
	else {
		*key_len = onetime_unlock_key_size;
		*key = onetime_unlock_key;
	}
	return 0;
}
#endif //UFBL_FEATURE_ONETIME_UNLOCK

#if defined(UFBL_FEATURE_TEMP_UNLOCK)

static struct boot_tag_temp_unlock g_temp_unlock_data;

void amzn_save_temp_unlock_data(void)
{
	int ret = 0;
	struct udevice *dev = NULL;
	struct tee_open_session_arg open_arg;
	struct tee_invoke_arg invoke_arg;
	const struct tee_optee_ta_uuid uuid = TA_TEMP_UNLOCK_UUID;
	struct tee_param params[1];
	const size_t buf_len = sizeof(struct boot_tag_temp_unlock);

	params[0].attr = TEE_PARAM_ATTR_TYPE_MEMREF_OUTPUT;
	params[0].u.memref.size = buf_len;

	dev = tee_find_device(NULL, NULL, NULL, NULL);
	if (dev == NULL) {
		printf("[Temp unlock] tee_find_device() failed");
		ret = -ENODEV;
		goto exit;
	}

	memset(&open_arg, 0, sizeof(open_arg));
	tee_optee_ta_uuid_to_octets(open_arg.uuid, &uuid);
	ret = tee_open_session(dev, &open_arg, 0, NULL);
	if (ret) {
		printf("[Temp unlock] tee_open_session() failed, ret = 0x%x\n", ret);
		goto exit;
	} else if (open_arg.ret != TEE_SUCCESS) {
		printf("tee_open_session() failed, ret = 0x%x, ret_origin=0x%x\n",
				open_arg.ret, open_arg.ret_origin);
		ret = open_arg.ret;
		goto exit;
	}

	memset(&invoke_arg, 0, sizeof(invoke_arg));
	invoke_arg.session = open_arg.session;
	invoke_arg.func = CMD_GET_TEMP_UNLOCK_DATA;
	ret = tee_shm_alloc(dev, buf_len, TEE_SHM_ALLOC | TEE_SHM_REGISTER, &params[0].u.memref.shm);
	if (ret) {
		printf("[Temp unlock] tee_shm_alloc() failed, ret = 0x%x\n", ret);
		goto exit;
	}

	ret = tee_invoke_func(dev, &invoke_arg, sizeof(params) / sizeof(struct tee_param), params);
	if (ret) {
		printf("[Temp unlock] tee_invoke_func() failed, ret = 0x%x\n", ret);
		goto exit;
	} else if (invoke_arg.ret != TEE_SUCCESS) {
		printf("[Temp unlock] tee_invoke_func() failed, ret = 0x%x, origin = %d\n",
				invoke_arg.ret, invoke_arg.ret_origin);
		ret = invoke_arg.ret;
		goto exit;
	}

	memset(&g_temp_unlock_data, 0, buf_len);
	memcpy(&g_temp_unlock_data, params[0].u.memref.shm->addr, buf_len);

	// We only need CFG_AMZN_TEMP_UNLOCK_REBOOT_CNT temp unlock codes
	if (g_temp_unlock_data.temp_unlock_reboot_cnt > CFG_AMZN_TEMP_UNLOCK_REBOOT_CNT) {
		g_temp_unlock_data.temp_unlock_reboot_cnt = CFG_AMZN_TEMP_UNLOCK_REBOOT_CNT;
		memset(g_temp_unlock_data.temp_unlock_hmac + (CFG_AMZN_TEMP_UNLOCK_REBOOT_CNT * AMZN_TEMP_UNLOCK_HMAC_HASH_SIZE),
		0,
		sizeof(g_temp_unlock_data.temp_unlock_hmac) - (CFG_AMZN_TEMP_UNLOCK_REBOOT_CNT * AMZN_TEMP_UNLOCK_HMAC_HASH_SIZE));
	}

	printf("[Temp unlock] Save temp unlock data success!\n");

exit:
	tee_shm_free(params[0].u.memref.shm);
}

int amzn_get_temp_unlock_codes(unsigned char **codes, unsigned int *reboot_cnt)
{
	if (codes == NULL || reboot_cnt == NULL)
		return -1;

	if (g_temp_unlock_data.magic != AMZN_TEMP_UNLOCK_BOOT_TAG_MAGIC) {
		printf("Warning: No valid temp unlock codes from sboot!\n");
		*reboot_cnt = 0;
		*codes = NULL;
	return -1;
	}

	*reboot_cnt = g_temp_unlock_data.temp_unlock_reboot_cnt;
	*codes = g_temp_unlock_data.temp_unlock_hmac;
	return 0;
}

int amzn_get_temp_unlock_root_pubkey(unsigned char **key, unsigned int *key_len)
{
	// temp-unlock and one-time-unlock share the same root public key
	return amzn_get_onetime_unlock_root_pubkey(key, key_len);
}

#endif //UFBL_FEATURE_TEMP_UNLOCK

#endif

