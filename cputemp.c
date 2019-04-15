#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include "fcntl.h"

#define TEMP_DEVICE_PATH "/dev/i2c-0"
#define TEMP_ADDRESS 0x48
#define RESOLUTION_TEMP (0.0625)

#define assert_cputemp(x)  \
{ \
    if( (x) == 0) { \
        printf(" ASSERT (%s|%s|%d)\r\n", __FILE__, __func__, __LINE__); \
        abort();\
    } \
}

__s32 i2c_smbus_access(int file, char read_write, __u8 command,
               int size, union i2c_smbus_data *data)
{
    struct i2c_smbus_ioctl_data args;
    __s32 err;

    args.read_write = read_write;
    args.command = command;
    args.size = size;
    args.data = data;

    err = ioctl(file, I2C_SMBUS, &args);
    if (err == -1)
        err = -errno;
    return (err);
}

static int fd = -1;

void cputemp_init(void)
{
    int ret;

    fd = open(TEMP_DEVICE_PATH, O_RDWR);
    assert_cputemp(fd >= 0);
    printf("Open %s successful[%d]!\r\n", TEMP_DEVICE_PATH, fd);

    ret = ioctl(fd, I2C_SLAVE_FORCE, TEMP_ADDRESS);
    assert_cputemp(ret >= 0);
}

void cputemp_deinit(void)
{
    close(fd);
}

float cputemp_read(void)
{
    int ret;
    union i2c_smbus_data data;
    float temperature = 0;
    int msb = 0, lsb = 0, value16;

    ret = i2c_smbus_access(fd, I2C_SMBUS_WRITE, 0, I2C_SMBUS_BYTE, NULL);
    assert_cputemp(ret >= 0);
    ret = i2c_smbus_access(fd, I2C_SMBUS_READ, 0, I2C_SMBUS_BYTE, &data);
    assert_cputemp(ret >= 0);
    msb = data.byte;
    ret = i2c_smbus_access(fd, I2C_SMBUS_WRITE, 1, I2C_SMBUS_BYTE, NULL);
    assert_cputemp(ret >= 0);
    ret = i2c_smbus_access(fd, I2C_SMBUS_READ, 0, I2C_SMBUS_BYTE, &data);
    assert_cputemp(ret >= 0);
    lsb = data.byte;

    value16 = msb << 8;
    value16 |= lsb;
    value16 = value16 >> 4;

    if(value16 <= 0x7ff) {
        temperature = value16 * RESOLUTION_TEMP;
    } else if(msb >= 0xc90) {
        msb = ~msb;
        msb &= 0x0fff;
        msb += 1;
        temperature = msb * RESOLUTION_TEMP;
        temperature = -temperature;
    } else {
        temperature = 0;
    }

    return (temperature);
}

