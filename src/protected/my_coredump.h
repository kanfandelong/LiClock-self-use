#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_core_dump.h"

#ifdef __cplusplus
extern "C" {
#endif

// ELF note 类型常量
#define ELF_ESP_CORE_DUMP_EXTRA_INFO_TYPE  0x1000
#define ELF_ESP_CORE_DUMP_INFO_TYPE        0x1001

// ELF header 结构
typedef struct {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} elfhdr;

// ELF program header
typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} elf_phdr;

#define PT_LOAD 1

// ELF note 内容
typedef struct {
    uint32_t n_type;
    void *n_ptr;
} elf_note_content_t;

// 解析 ELF note section
void app_core_dump_parse_note_section(uint8_t *ptr, elf_note_content_t *notes, size_t count);

// 解析 extra info
void app_core_dump_summary_parse_extra_info(esp_core_dump_summary_t *summary, void *data);

// 解析版本信息
void elf_parse_version_info(esp_core_dump_summary_t *summary, void *data);

// 解析异常寄存器
void app_core_dump_summary_parse_exc_regs(esp_core_dump_summary_t *summary, void *data);

// 解析回溯信息
void app_core_dump_summary_parse_backtrace_info(esp_core_dump_bt_info_t *bt_info, void *vaddr, void *data, size_t size);

// 解析异常任务名
void app_elf_parse_exc_task_name(esp_core_dump_summary_t *summary, void *data);

#ifdef __cplusplus
}
#endif