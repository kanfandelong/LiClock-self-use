#include <stdint.h>
#include "soc/rtc.h"
#include "soc/rtc_cntl_reg.h"  // 或 "soc/rtc_cntl_reg.h"
#include "esp_attr.h"

bool en_ulp_wakeup = false; // 全局变量，控制是否启用ULP唤醒

// 声明原始函数（将由链接器自动解析为 __real_rtc_sleep_start）
extern "C" uint32_t __real_rtc_sleep_start(uint32_t wakeup_opt, uint32_t reject_opt, uint32_t lslp_mem_inf_fpu);

// 包装函数
extern "C" uint32_t __wrap_rtc_sleep_start(uint32_t wakeup_opt, uint32_t reject_opt, uint32_t lslp_mem_inf_fpu)
{
    // 【关键】添加 RISC-V 唤醒标志
    if (en_ulp_wakeup)
        wakeup_opt |= (RTC_COCPU_TRIG_EN | RTC_COCPU_TRAP_TRIG_EN);
    // 调用原始函数（内部会正常调用静态的 rtc_sleep_finish）
    return __real_rtc_sleep_start(wakeup_opt, reject_opt, lslp_mem_inf_fpu);
}