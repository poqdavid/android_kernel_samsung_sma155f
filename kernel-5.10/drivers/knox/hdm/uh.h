#ifndef __UH_H__
#define __UH_H__

#ifndef __ASSEMBLY__

#include <linux/types.h>
#include <linux/const.h>

/* For uH Command */
#define	PLATFORM	0
#define APP_RKP		1
#define APP_KDP		2
#define APP_HDM		6


#define UH_PREFIX			UL(0x83000000)
#define UH_APPID(APP_ID)	(((UL(APP_ID) << 8) & UL(0xFF00)) | UH_PREFIX)
#define UH_EVENT_SUSPEND	(0x8)

enum __UH_APP_ID {
	UH_PLATFORM     = UH_APPID(PLATFORM),
	UH_APP_RKP      = UH_APPID(APP_RKP),
	UH_APP_KDP      = UH_APPID(APP_KDP),
	UH_APP_HDM		= UH_APPID(APP_HDM),
};

struct test_case_struct {
	int (*fn)(void);
	char *describe;
};

unsigned long _uh_call(u64 app_id, u64 command, u64 arg0, u64 arg1, u64 arg2, u64 arg3);
inline static void uh_call(u64 app_id, u64 command, u64 arg0, u64 arg1, u64 arg2, u64 arg3)
{
	_uh_call(app_id | command, arg0, arg1, arg2, arg3, 0);
}

#endif //__ASSEMBLY__
#endif //__UH_H__
