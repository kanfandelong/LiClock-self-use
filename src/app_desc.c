#include <string.h>
#include "esp_app_desc.h"
#include <esp_app_format.h>
#include <esp_ota_ops.h>
#include "git_info.h"

const __attribute__((section(".rodata_desc"))) esp_app_desc_t esp_app_desc = {
    .magic_word = ESP_APP_DESC_MAGIC_WORD,
    .version = GIT_COMMIT_HASH_SHORT,
    .project_name = "MusicPlayer",
    .idf_ver = IDF_VER,
#if CONFIG_IDF_TARGET_LINUX
    .app_elf_sha256 = { 0xDE, 0xAD, 0xBE, 0xEF, 0x47, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B},
#endif

#ifdef CONFIG_BOOTLOADER_APP_SECURE_VERSION
    .secure_version = CONFIG_BOOTLOADER_APP_SECURE_VERSION,
#else
    .secure_version = 0,
#endif
    .time = __TIME__,
    .date = __DATE__,
    .min_efuse_blk_rev_full = CONFIG_ESP_EFUSE_BLOCK_REV_MIN_FULL,
    .max_efuse_blk_rev_full = CONFIG_ESP_EFUSE_BLOCK_REV_MAX_FULL,
    .mmu_page_size = 31 - __builtin_clz(CONFIG_MMU_PAGE_SIZE),
};