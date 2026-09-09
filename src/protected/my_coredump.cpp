#include "protected/my_coredump.h"
#include <string.h>

void app_core_dump_parse_note_section(uint8_t *ptr, elf_note_content_t *notes, size_t count)
{
    // ELF note section parsing stub
    (void)ptr;
    (void)notes;
    (void)count;
}

void app_core_dump_summary_parse_extra_info(esp_core_dump_summary_t *summary, void *data)
{
    (void)summary;
    (void)data;
}

void elf_parse_version_info(esp_core_dump_summary_t *summary, void *data)
{
    (void)summary;
    (void)data;
}

void app_core_dump_summary_parse_exc_regs(esp_core_dump_summary_t *summary, void *data)
{
    (void)summary;
    (void)data;
}

void app_core_dump_summary_parse_backtrace_info(esp_core_dump_bt_info_t *bt_info, void *vaddr, void *data, size_t size)
{
    (void)bt_info;
    (void)vaddr;
    (void)data;
    (void)size;
}

void app_elf_parse_exc_task_name(esp_core_dump_summary_t *summary, void *data)
{
    (void)summary;
    (void)data;
}