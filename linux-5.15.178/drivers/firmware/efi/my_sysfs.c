#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/acpi.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/module.h>

#define HRTI_SIGNATURE    "HRTI"  // Hardware Runtime Info

struct hrti_table {
    struct acpi_table_header header;
    u32 device_id;
    u16 cpu_temperature;
    char firmware_version[32];
} __packed;


static struct hrti_table *hrti_tbl = NULL;
static struct kobject *hwinfo_kobj = NULL;

static ssize_t device_id_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    if (!hrti_tbl)
        return -ENODEV;
    
    return sysfs_emit(buf, "0x%08x\n", hrti_tbl->device_id);
}

static ssize_t cpu_temperature_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    if (!hrti_tbl)
        return -ENODEV;
    
    return sysfs_emit(buf, "%d\n", hrti_tbl->cpu_temperature);
}

static ssize_t firmware_version_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    if (!hrti_tbl)
        return -ENODEV;
    
    return sysfs_emit(buf, "%s\n", hrti_tbl->firmware_version);
}

static ssize_t show_all(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    if (!hrti_tbl)
        return -ENODEV;
        
    return sysfs_emit(buf, 
                  "Device ID: 0x%08x\n"
                  "CPU Temperature: %d°C\n"
                  "Firmware Version: %s\n",
                  hrti_tbl->device_id,
                  hrti_tbl->cpu_temperature,
                  hrti_tbl->firmware_version);
}

// 定义 sysfs 属性
static struct kobj_attribute device_id_attribute = 
    __ATTR(device_id, 0444, device_id_show, NULL);
    
static struct kobj_attribute cpu_temperature_attribute = 
    __ATTR(cpu_temperature, 0444, cpu_temperature_show, NULL);
    
static struct kobj_attribute firmware_version_attribute = 
    __ATTR(firmware_version, 0444, firmware_version_show, NULL);

static struct kobj_attribute all_info_attribute = 
    __ATTR(all_info, 0444, show_all, NULL);

// 属性数组
static struct attribute *hwinfo_attrs[] = {
    &device_id_attribute.attr,
    &cpu_temperature_attribute.attr,
    &firmware_version_attribute.attr,
    &all_info_attribute.attr,
    NULL,
};

static const struct attribute_group hwinfo_attr_group = {
    .attrs = hwinfo_attrs,
};


static int hwinfo_table_handler(struct acpi_table_header *table)
{
    hrti_tbl = (struct hrti_table *)table;

    pr_info("Hardware Info ACPI Table found:\n");
    pr_info("  Device ID: 0x%08x\n", hrti_tbl->device_id);
    pr_info("  CPU Temperature: %d°C\n", hrti_tbl->cpu_temperature);
    pr_info("  Firmware Version: %s\n", hrti_tbl->firmware_version);

    return 0;
}

static int __init hwinfo_init(void)
{
    int ret;
    
    ret = acpi_table_parse(HRTI_SIGNATURE, hwinfo_table_handler);
    hwinfo_kobj = kobject_create_and_add("hwinfo", firmware_kobj);
    ret = sysfs_create_group(hwinfo_kobj, &hwinfo_attr_group);
    
    pr_info("Hardware Info sysfs interface created at /sys/firmware/hwinfo/\n");
    return 0;
}

static void __exit hwinfo_exit(void)
{
    if (hwinfo_kobj) {
        sysfs_remove_group(hwinfo_kobj, &hwinfo_attr_group);
        kobject_put(hwinfo_kobj);
    }
    
    pr_info("Hardware Info module unloaded\n");
}

module_init(hwinfo_init);
module_exit(hwinfo_exit);
