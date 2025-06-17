#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/acpi.h>
#include <linux/slab.h> // For kzalloc/kfree

// Matches UEFI-side definition
#define MY_HRTI_ACPI_SIGNATURE "HRTI" // Must be 4 chars,
                                     // null-terminated string for acpi_get_table

// Matches UEFI-side EFI_ACPI_MY_HARDWARE_INFO_TABLE structure
#pragma pack(1)
struct my_hrti_acpi_table {
    struct acpi_table_header header; // Standard ACPI header
    u32 device_id;
    u64 boot_counter;
    u16 cpu_temperature;
    char firmware_version[32];
};
#pragma pack()

static struct my_hrti_acpi_table *g_hrti_data = NULL;
static struct kobject *hrti_kobj;

// Sysfs attribute show functions
static ssize_t device_id_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    if (!g_hrti_data) return -ENODATA;
    return scnprintf(buf, PAGE_SIZE, "0x%08X\n", g_hrti_data->device_id);
}

static ssize_t boot_counter_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    if (!g_hrti_data) return -ENODATA;
    return scnprintf(buf, PAGE_SIZE, "%llu\n", g_hrti_data->boot_counter);
}

static ssize_t cpu_temperature_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    if (!g_hrti_data) return -ENODATA;
    return scnprintf(buf, PAGE_SIZE, "%u\n", g_hrti_data->cpu_temperature);
}

static ssize_t firmware_version_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    if (!g_hrti_data) return -ENODATA;
    // Ensure null termination for safety, though UEFI side should guarantee it.
    char fw_ver_safe[sizeof(g_hrti_data->firmware_version) + 1];
    strncpy(fw_ver_safe, g_hrti_data->firmware_version, sizeof(g_hrti_data->firmware_version));
    fw_ver_safe[sizeof(g_hrti_data->firmware_version)] = '\0';
    return scnprintf(buf, PAGE_SIZE, "%s\n", fw_ver_safe);
}

// Define attributes
static struct kobj_attribute device_id_attr = __ATTR_RO(device_id);
static struct kobj_attribute boot_counter_attr = __ATTR_RO(boot_counter);
static struct kobj_attribute cpu_temperature_attr = __ATTR_RO(cpu_temperature);
static struct kobj_attribute firmware_version_attr = __ATTR_RO(firmware_version);

static struct attribute *hrti_attrs[] = {
    &device_id_attr.attr,
    &boot_counter_attr.attr,
    &cpu_temperature_attr.attr,
    &firmware_version_attr.attr,
    NULL, // Terminate the list
};

static const struct attribute_group hrti_attr_group = {
    .attrs = hrti_attrs,
};

static int __init hrti_sysfs_init(void) {
    struct acpi_table_header *header = NULL;
    acpi_status status;
    int ret;

    printk(KERN_INFO "HRTI: Loading My Hardware Info Sysfs module\n");

    // Get ACPI table
    // Note: acpi_get_table takes a SIGNATURE (4 chars), not a null-terminated string internally,
    // but the API expects a char* for convenience.
    status = acpi_get_table(MY_HRTI_ACPI_SIGNATURE, 0, &header);

    if (ACPI_FAILURE(status) || !header) {
        printk(KERN_ERR "HRTI: Failed to get ACPI table %s. Status: 0x%x\n", MY_HRTI_ACPI_SIGNATURE, status);
        if (header) acpi_put_table(header); // Should not happen if status failed, but good practice
        return -ENODEV;
    }

    // Basic validation (Length should match at least)
    if (header->length < sizeof(struct my_hrti_acpi_table)) {
        printk(KERN_ERR "HRTI: ACPI table %s has incorrect size. Expected %zu, got %u\n",
               MY_HRTI_ACPI_SIGNATURE, sizeof(struct my_hrti_acpi_table), header->length);
        acpi_put_table(header);
        return -EINVAL;
    }

    // Store a copy of the data (ACPI tables obtained this way are mapped and should not be written to,
    // and their lifetime is managed by acpi_put_table)
    g_hrti_data = kzalloc(header->length, GFP_KERNEL);
    if (!g_hrti_data) {
        printk(KERN_ERR "HRTI: Failed to_allocate memory for HRTI data\n");
        acpi_put_table(header);
        return -ENOMEM;
    }
    memcpy(g_hrti_data, header, header->length);
    acpi_put_table(header); // Release mapping to the original table

    printk(KERN_INFO "HRTI: ACPI table %s found and data copied. DeviceID: 0x%X\n",
           MY_HRTI_ACPI_SIGNATURE, g_hrti_data->device_id);
    
    // Create kobject under /sys/firmware/
    hrti_kobj = kobject_create_and_add("my_hardware_info", firmware_kobj);
    if (!hrti_kobj) {
        printk(KERN_ERR "HRTI: Failed to create kobject\n");
        kfree(g_hrti_data);
        g_hrti_data = NULL;
        return -ENOMEM;
    }

    // Create sysfs files
    ret = sysfs_create_group(hrti_kobj, &hrti_attr_group);
    if (ret) {
        printk(KERN_ERR "HRTI: Failed to create sysfs group\n");
        kobject_put(hrti_kobj); // This will also call kobject_del
        kfree(g_hrti_data);
        g_hrti_data = NULL;
        return ret;
    }

    printk(KERN_INFO "HRTI: Sysfs interface created at /sys/firmware/my_hardware_info\n");
    return 0;
}

static void __exit hrti_sysfs_exit(void) {
    printk(KERN_INFO "HRTI: Unloading My Hardware Info Sysfs module\n");
    if (hrti_kobj) {
        sysfs_remove_group(hrti_kobj, &hrti_attr_group);
        kobject_put(hrti_kobj);
    }
    if (g_hrti_data) {
        kfree(g_hrti_data);
        g_hrti_data = NULL;
    }
}

module_init(hrti_sysfs_init);
module_exit(hrti_sysfs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Exposes custom hardware info from HRTI ACPI table via sysfs");
MODULE_VERSION("0.1");