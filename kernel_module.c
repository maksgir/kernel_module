#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/genhd.h>
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/fs.h>
#include <linux/fs_struct.h>
#include <linux/path.h>

#include "block_class_header.h"

#define DEBUGFS_DIR "my_kernel_module"
#define MESSAGE_STORAGE_SIZE 10
//#define BLOCK_CLASS_ADDRESS 0x0000000000000000
//#define BLOCK_CLASS ((struct class*)BLOCK_CLASS_ADDRESS)

struct disk_stats {
    unsigned long nsecs[NR_STAT_GROUPS];
    unsigned long sectors[NR_STAT_GROUPS];
    unsigned long ios[NR_STAT_GROUPS];
    unsigned long merges[NR_STAT_GROUPS];
    unsigned long io_ticks;
};

struct Message {
    char name[DISK_NAME_LEN];
    unsigned long rd;
    unsigned long rd_sectors;
    unsigned long wr;
    unsigned long wr_sectors;
};

struct CallbackContext {
    struct Message output[MESSAGE_STORAGE_SIZE];
    int out_iter;
};

static struct dentry *debugfs_dir;
static struct CallbackContext callback_context;

// Вставляем предоставленную функцию iter_callback
int iter_callback(struct device *dev, void *context_void);

static void part_stat_read_all(struct block_device *part, struct disk_stats *stat) {
    int cpu;
    memset(stat, 0, sizeof(struct disk_stats));
    for_each_possible_cpu(cpu) {
        struct disk_stats *ptr = per_cpu_ptr(part->bd_stats, cpu);
        int group;

        for (group = 0; group < NR_STAT_GROUPS; group++) {
            stat->nsecs[group] += ptr->nsecs[group];
            stat->sectors[group] += ptr->sectors[group];
            stat->ios[group] += ptr->ios[group];
            stat->merges[group] += ptr->merges[group];
        }
        stat->io_ticks += ptr->io_ticks;
    }
}

static ssize_t read_stat(struct file *file, char __user *buffer, size_t count, loff_t *ppos) {
    pr_info("Reading block device stats\n");

    int i;
    for (i = 0; i < MESSAGE_STORAGE_SIZE; ++i) {
        if (copy_to_user(buffer + i * sizeof(struct Message), &callback_context.output[i], sizeof(struct Message))) {
            pr_err("copy_to_user() failure within read_stat");
            return -EFAULT;
        }
    }
    return i * sizeof(struct Message);
}

static const struct file_operations debugfs_fops = {
    .read = read_stat,
};

int iter_callback(struct device *dev, void *context_void) {
    struct CallbackContext *context = context_void;

    struct gendisk *gp = dev_to_disk(dev);
    struct block_device *hd = dev_to_bdev(dev);

    if (hd != NULL && gp != NULL) {
        if (bdev_is_partition(hd) || !bdev_nr_sectors(hd)) {
            pr_info("Пропуск раздела или устройства с нулевым количеством секторов\n");
            return 0; // Продолжает итерации
        }

        // Структура для хранения статистики
        struct disk_stats stat;
        // Получаем статистику для блочного устройства
        part_stat_read_all(hd, &stat);
        pr_info("Статистика успешно считана для блочного устройства: %s\n", gp->disk_name);

        struct Message msg = {0};

        // Копируем имя диска с учетом минимальной длины
        memmove(msg.name, gp->disk_name, min(sizeof(gp->disk_name), sizeof(msg.name)));
        msg.name[sizeof(msg.name) - 1] = '\0';
        pr_info("Считано имя блочного устройства: %s\n", msg.name);

        // Копируем статистику в структуру сообщения
        msg.rd = stat.ios[STAT_READ];
        msg.rd_sectors = stat.sectors[STAT_READ];
        msg.wr = stat.ios[STAT_WRITE];
        msg.wr_sectors = stat.sectors[STAT_WRITE];

        // Выводим в журнал информацию о считанных статистиках
        pr_info("Считаны статистика для устройства: %s\n", msg.name);

        // Копируем сообщение в выходное хранилище
        memcpy(&context->output[context->out_iter++], &msg, sizeof(msg));

        // Убираем лишний вывод
        // pr_info("Считаны статистика для устройства: %s\n", msg.name);
    } else {
        pr_info("Пропуск устройства без блочного устройства или без информации о диске\n");
    }

    return 0; // Продолжает итерации
}


static int __init my_kernel_module_init(void) {
    pr_info("Инициализация my_kernel_module\n");

    // Создаем директорию в debugfs для модуля
    debugfs_dir = debugfs_create_dir(DEBUGFS_DIR, NULL);
    if (!debugfs_dir) {
        pr_err("Не удалось создать директорию в debugfs\n");
        return -ENODEV; // Возвращаем код ошибки
    }

    // Создаем файл в директории debugfs для статистики блочного устройства
    if (!debugfs_create_file("block_device_stats", 0444, debugfs_dir, &callback_context, &debugfs_fops)) {
        // Чистим за собой в случае ошибки создания файла
        debugfs_remove_recursive(debugfs_dir);
        pr_err("Не удалось создать файл в debugfs\n");
        return -ENODEV;
    }
    pr_info("Файл my_kernel_module/block_device_stats создан\n");

    // Инициализируем контекст обратного вызова перед использованием
    memset(&callback_context, 0, sizeof(callback_context));
    
    pr_info("Callback контекст готов\n");

    // Используем class_for_each_device для итерации по блочным устройствам и сбора статистики
    class_for_each_device(BLOCK_CLASS, NULL, &callback_context, iter_callback);

    // Записываем информационное сообщение о успешной загрузке модуля
    pr_info("my_kernel_module загружен\n");

    return 0; // Возвращаем успешное выполнение
}


static void __exit my_kernel_module_exit(void) {
    pr_info("Unloading my_kernel_module\n");
    debugfs_remove_recursive(debugfs_dir);
    pr_info("my_kernel_module unloaded\n");
}

module_init(my_kernel_module_init);
module_exit(my_kernel_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Maksim Girya");
MODULE_DESCRIPTION("Sample kernel module using debugfs (если положу ядро, сорян)");
