#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 4896

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);

static int nodecount = 0;

typedef struct bigN {
    uint64_t val;
    struct list_head link;
} bigN;

static void new_bignode(struct list_head *list_head, uint64_t val)
{
    bigN *newnode = (bigN *) kmalloc(sizeof(bigN), GFP_KERNEL);
    if (!newnode)
        return;
    newnode->val = val;
    list_add_tail(&newnode->link, list_head);
    nodecount++;
}

#define MAX_64 1000000000000000000UL  // 10^18
static int add_64(uint64_t a, uint64_t *b, int carrybit)
{
    if (a + *b + carrybit >= MAX_64) {
        *b = a + *b + carrybit - MAX_64;
        return 1;
    }
    *b += a + carrybit;
    return 0;
}

static struct list_head *bigN_add(struct list_head *bigN1,
                                  struct list_head *bigN2)
{
    struct list_head **n1 = &bigN1->next, **n2 = &bigN2->next;
    for (int carrybit = 0;; n1 = &(*n1)->next, n2 = &(*n2)->next) {
        if (*n2 == bigN2) {
            if (*n1 == bigN1) {
                if (carrybit)
                    new_bignode(bigN2, 1);
                break;
            }
            new_bignode(bigN2, 0);
        }
        carrybit = add_64(list_entry(*n1, bigN, link)->val,
                          &list_entry(*n2, bigN, link)->val, carrybit);
    }
    return bigN2;
}

static void freebigN(struct list_head *list_head)
{
    struct list_head *pos, *safe;
    list_for_each_safe (pos, safe, list_head) {
        kfree(list_entry(pos, bigN, link));
    }
    kfree(list_head);
}

static char *bigN2string(struct list_head *list_head)
{
    int fklen = (nodecount & 1ull) + (nodecount >> 1);
    char *result = (char *) kzalloc(fklen * 18 + 1, GFP_KERNEL);
    if (!result)
        return NULL;

    struct list_head *pos;
    list_for_each_prev(pos, list_head)
    {
        if (strlen(result) == 0)
            snprintf(result, 19, "%lld", list_entry(pos, bigN, link)->val);
        else
            snprintf(result + strlen(result), 19, "%018lld",
                     list_entry(pos, bigN, link)->val);
    }
    return result;
}

// v1 for bigN
static long long fib_sequence_bignum(long long k, char *buf)
{
    if (k < 2) {
        char result[2];
        snprintf(result, 2, "%lld", k);
        long long n = copy_to_user(buf, result, 2);
        return n;
    }

    struct list_head *fk1 = (struct list_head *) kmalloc(
                         sizeof(struct list_head), GFP_KERNEL),
                     *fk2 = (struct list_head *) kmalloc(
                         sizeof(struct list_head), GFP_KERNEL),
                     *temp;
    INIT_LIST_HEAD(fk1);
    INIT_LIST_HEAD(fk2);
    new_bignode(fk1, 1);
    new_bignode(fk2, 0);
    for (int i = 2; i <= k; i++) {
        temp = bigN_add(fk1, fk2);
        fk2 = fk1;
        fk1 = temp;
    }
    char *result = bigN2string(fk1);
    long long n = copy_to_user(buf, result, strlen(result));
    kfree(result);
    freebigN(fk1);
    freebigN(fk2);
    return n;
}

static void bigN_add2(char *bigN1, char *bigN2, char *buf)
{
    int i, carry = 0, sum, len1 = strlen(bigN1), len2 = strlen(bigN2);

    for (i = 0; i < len2; i++) {
        sum = bigN1[i] + bigN2[i] + carry - ('0' << 1);
        buf[i] = '0' + sum % 10;
        carry = sum / 10;
    }

    for (i = len2; i < len1; i++) {
        sum = bigN1[i] - '0' + carry;
        buf[i] = '0' + sum % 10;
        carry = sum / 10;
    }

    if (carry)
        buf[i] = '0' + carry;
}

static void strrev(char *str)
{
    int i = strlen(str) - 1, j = 0;

    while (i > j) {
        char ch;
        ch = str[i];
        str[i] = str[j];
        str[j] = ch;
        i--;
        j++;
    }
}

// v4 bigN2 iterative
static long long fib_sequence_bignum2(long long k, char *buf)
{
    if (k < 2) {
        char result[2];
        snprintf(result, 2, "%lld", k);
        long long n = copy_to_user(buf, result, 2);
        return n;
    }

    char *fk1 = (char *) kmalloc(1024, GFP_KERNEL),
         *fk2 = (char *) kmalloc(1024, GFP_KERNEL),
         *temp = (char *) kmalloc(1024, GFP_KERNEL);
    fk1[0] = '1', fk2[0] = '1';  // k = 2, k = 1
    for (int i = 3; i <= k; i++) {
        bigN_add2(fk1, fk2, temp);
        strncpy(fk2, fk1, strlen(fk1));
        strncpy(fk1, temp, strlen(temp));
    }
    strrev(fk1);
    long long n = copy_to_user(buf, fk1, strlen(fk1));
    return n;
}

static char **bigN_add3(char **bigN1, char **bigN2)
{
    int i, carry = 0, sum, len1 = strlen(*bigN1), len2 = strlen(*bigN2);

    for (i = 0; i < len2; i++) {
        sum = (*bigN1)[i] + (*bigN2)[i] + carry - ('0' << 1);
        (*bigN2)[i] = '0' + sum % 10;
        carry = sum / 10;
    }

    for (i = len2; i < len1; i++) {
        sum = (*bigN1)[i] - '0' + carry;
        (*bigN2)[i] = '0' + sum % 10;
        carry = sum / 10;
    }

    if (carry)
        (*bigN2)[i] = '0' + carry;
    return bigN2;
}

// v5 bigN3 iterative
static long long fib_sequence_bignum3(long long k, char *buf)
{
    if (k < 2) {
        char result[2];
        snprintf(result, 2, "%lld", k);
        long long n = copy_to_user(buf, result, 2);
        return n;
    }
    char *buf1 = (char *) kmalloc(1024, GFP_KERNEL),
         *buf2 = (char *) kmalloc(1024, GFP_KERNEL);
    char **fk1 = &buf1, **fk2 = &buf2, **temp;
    buf1[0] = '1', buf2[0] = '1';  // k = 2, k = 1
    for (int i = 3; i <= k; i++) {
        temp = bigN_add3(fk1, fk2);
        fk2 = fk1;
        fk1 = temp;
    }
    strrev(*fk1);
    long long n = copy_to_user(buf, *fk1, strlen(*fk1));
    return n;
}

// v2
static long long fib_sequence_fdoubling(long long k)
{
    if (k < 2)
        return k;
    uint64_t msb = 62 - __builtin_clzll(k);  // msb - 1 since skip k = 0
    uint64_t f0 = 1, f1 = 1;                 // k = 1
    for (uint64_t mask = 1 << msb, t1 = 0, t2 = 0; mask; mask >>= 1) {
        t1 = f0 * (2 * f1 - f0);  // f(2k)
        t2 = f1 * f1 + f0 * f0;   // f(2k + 1)
        f0 = t1;
        f1 = t2;  // k *= 2
        if (k & mask) {
            t1 = f0 + f1;  // k++
            f0 = f1;
            f1 = t1;
        }
    }
    return f0;
}
// v3
static long long fib_sequence_fdoubling2(long long k)
{
    if (k < 2)
        return k;
    uint64_t msb = 62 - __builtin_clzll(k);  // msb - 1 since skip k = 0
    uint64_t f0 = 1, f1 = 1;                 // k = 1
    for (uint64_t mask = 1 << msb, t1 = 0, t2 = 0; mask; mask >>= 1) {
        t1 = f0 * ((f1 << 1) - f0);  // f(2k)
        t2 = f1 * f1 + f0 * f0;      // f(2k + 1)
        if (k & mask) {
            f0 = t2;
            f1 = t1 + t2;  // k = 2k + 1
        } else {
            f0 = t1;
            f1 = t2;  // k = 2k
        }
    }
    return f0;
}

// v0
static long long fib_sequence(long long k)
{
    if (k < 2)
        return k;

    long long f_k = -1, f_k1 = 1, f_k2 = 0;
    for (int i = 2; i <= k; i++) {
        f_k = f_k1 + f_k2;  // calculate f(i)
        f_k2 = f_k1;        // f_k2 = f(i - 1) = f((i + 1) - 2)
        f_k1 = f_k;         // f_k1 = f(i) = f((i + 1) - 1)
    }

    return f_k;
}

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    return (ssize_t) fib_sequence_bignum3(*offset, buf);
}

static ktime_t kt;
/* write operation is used as time measure function
 * @size : choose fib mode
 * buffer size set as 1024B
 */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    switch (size) {
    case 0:  // defalut (time measure)
        kt = ktime_get();
        fib_sequence(*offset);
        kt = ktime_sub(ktime_get(), kt);
        return (ssize_t) ktime_to_ns(kt);
    case 1:  // bignum (time measure)
        kt = ktime_get();
        fib_sequence_bignum(*offset, NULL);
        kt = ktime_sub(ktime_get(), kt);
        return (ssize_t) ktime_to_ns(kt);
    case 2:  // fast doubling (time measure)
        kt = ktime_get();
        fib_sequence_fdoubling(*offset);
        kt = ktime_sub(ktime_get(), kt);
        return (ssize_t) ktime_to_ns(kt);
    case 3:  // fast doubling2 (time measure)
        kt = ktime_get();
        fib_sequence_fdoubling2(*offset);
        kt = ktime_sub(ktime_get(), kt);
        return (ssize_t) ktime_to_ns(kt);
    case 4:  // bignum2 iterative (time measure)
        kt = ktime_get();
        fib_sequence_bignum2(*offset, NULL);
        kt = ktime_sub(ktime_get(), kt);
        return (ssize_t) ktime_to_ns(kt);
    case 5:  // bignum3 iterative (time measure)
        kt = ktime_get();
        fib_sequence_bignum3(*offset, NULL);
        kt = ktime_sub(ktime_get(), kt);
        return (ssize_t) ktime_to_ns(kt);
    default:  // make check
        return 1;
    }
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    fib_cdev->ops = &fib_fops;
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
