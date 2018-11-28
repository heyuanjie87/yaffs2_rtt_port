#include <rtthread.h>
#include <dfs_fs.h>
#include <dfs_file.h>

#include "yaffs/yaffs_guts.h"
#include "yaffs/direct/yaffsfs.h"

static int dfs_yfile_open(struct dfs_fd *file)
{
    struct dfs_filesystem *fs;
    struct yaffs_obj *obj;
    int fd;
    int oflag;
    int result;

    fs = (struct dfs_filesystem *)file->data;
    obj = (struct yaffs_obj *)fs->data;

    oflag = file->flags;
    if (oflag & O_DIRECTORY)
    {
        yaffs_DIR * dir;
        if (oflag & O_CREAT)
        {
            result = yaffs_mkdir_reldir(obj, file->path, 0x777);
            if (result < 0)
                return yaffsfs_GetLastError();
        }
        /* open dir */
        dir = yaffs_opendir_reldir(obj, file->path);
        if (dir == RT_NULL)
            return yaffsfs_GetLastError();
        /* save this pointer,will used by dfs_yaffs_getdents*/
        file->data = dir;
        return 0;
    }

    /* regular file operations */
    fd = yaffs_open_reldir(obj, file->path, oflag, S_IREAD | S_IWRITE);
    if (fd < 0)
        return yaffsfs_GetLastError();

    file->data = (void *)fd;
    file->pos = yaffs_lseek(fd, 0, SEEK_CUR);
    file->size = yaffs_lseek(fd, 0, SEEK_END);
    yaffs_lseek(fd, file->pos, SEEK_SET);

    if (oflag & O_APPEND)
    {
        file->pos = file->size;
        file->size = yaffs_lseek(fd, 0, SEEK_END);
    }

    return 0;
}

static int dfs_yfile_close(struct dfs_fd *file)
{
    int oflag;
    int fd;

    oflag = file->flags;
    if (oflag & O_DIRECTORY) /* operations about dir */
    {
        if (yaffs_closedir((yaffs_DIR *)(file->data)) < 0)
            return yaffsfs_GetLastError();
        return 0;
    }

    /* regular file operations */
    fd = (int)(file->data);

    if (yaffs_close(fd) == 0)
        return 0;

    /* release memory */
    return yaffsfs_GetLastError();
}

static int dfs_yfile_ioctl(struct dfs_fd *file, int cmd, void *args)
{
    return -ENOSYS;
}

static int dfs_yfile_read(struct dfs_fd *file, void *buf, size_t len)
{
    int fd;
    int char_read;

    fd = (int)(file->data);
    char_read = yaffs_read(fd, buf, len);
    if (char_read < 0)
        return yaffsfs_GetLastError();

    /* update position */
    file->pos = yaffs_lseek(fd, 0, SEEK_CUR);

    return char_read;
}

static int dfs_yfile_write(struct dfs_fd *file, const void *buf, size_t len)
{
    int fd;
    int char_write;

    fd = (int)(file->data);

    char_write = yaffs_write(fd, buf, len);
    if (char_write < 0)
        return yaffsfs_GetLastError();

    /* update position */
    file->pos = yaffs_lseek(fd, 0, SEEK_CUR);

    return char_write;
}

static int dfs_yfile_flush(struct dfs_fd *file)
{
    int fd;
    int result;

    fd = (int)(file->data);

    result = yaffs_flush(fd);
    if (result < 0)
        return yaffsfs_GetLastError();

    return 0;
}

static int dfs_yfile_lseek(struct dfs_fd *file, rt_off_t offset)
{
    int fd;
    int result;

    fd = (int)(file->data);

    /* set offset as current offset */
    result = yaffs_lseek(fd, offset, SEEK_SET);
    if (result < 0)
        return yaffsfs_GetLastError();

    return result;
}

static int dfs_yfile_getdents(struct dfs_fd *file, struct dirent *dirp, uint32_t count)
{
    rt_uint32_t index;
    struct dirent* d;
    yaffs_DIR* dir;
    struct yaffs_dirent * yaffs_d;

    dir = (yaffs_DIR*)(file->data);
    RT_ASSERT(dir != RT_NULL);

    /* make integer count, usually count is 1 */
    count = (count / sizeof(struct dirent)) * sizeof(struct dirent);
    if (count == 0)
        return -EINVAL;

    index = 0;
    /* usually, the while loop should only be looped only once! */
    while (1)
    {
        d = dirp + index;

        yaffs_d = yaffs_readdir(dir);
        if (yaffs_d == RT_NULL)
        {
            if (yaffsfs_GetLastError() == EBADF)
                return -EBADF;

            return -1; /* a general error */
        }

        /* write the rest feilds of struct dirent* dirp  */
        d->d_namlen = rt_strlen(yaffs_d->d_name);
        d->d_reclen = (rt_uint16_t)sizeof(struct dirent);
        rt_strncpy(d->d_name, yaffs_d->d_name, rt_strlen(yaffs_d->d_name) + 1);

        index++;
        if (index * sizeof(struct dirent) >= count)
            break;
    }

    if (index == 0)
        return yaffsfs_GetLastError();

    return index * sizeof(struct dirent);
}


static int dfs_yaffs_mount(struct dfs_filesystem *fs, unsigned long rwflag, const void *data)
{
    rt_mtd_t *mtd;
    struct yaffs_dev *dev;
    int ret;

    if (fs->dev_id->type != RT_Device_Class_MTD)
        return -1;

    mtd = (rt_mtd_t*)fs->dev_id;
    if (!mtd || mtd->type != MTD_TYPE_NAND)
        return -1;
    if (!mtd->priv)
        return -1;

    dev = (struct yaffs_dev *)mtd->priv;
    ret = yaffs_mount_reldev(dev);
    if (ret == 0)
    {
        fs->data = dev->root_dir;
    }

    return ret;
}

static int dfs_yaffs_unmount(struct dfs_filesystem *fs)
{
    rt_mtd_t *mtd;
    struct yaffs_dev *dev;

    mtd = (rt_mtd_t*)fs->dev_id;
    dev = (struct yaffs_dev *)mtd->priv;

    if (yaffs_unmount_reldev(dev) < 0)
        return yaffsfs_GetLastError();

    return -ENOENT;
}

static int dfs_yaffs_mkfs(rt_device_t dev_id)
{
    rt_mtd_t *mtd;
    struct yaffs_dev *dev;
    extern int yaffs_format_reldev(struct yaffs_dev *dev,
		                           int unmount_flag,
		                           int force_unmount_flag,
		                           int remount_flag);

    if (dev_id->type != RT_Device_Class_MTD)
        return -1;

    mtd = (rt_mtd_t*)dev_id;
    if (!mtd || mtd->type != MTD_TYPE_NAND)
        return -1;
    if (!mtd->priv)
        return -1;

    dev = (struct yaffs_dev *)mtd->priv;

    return yaffs_format_reldev(dev, 1, 1, 1);
}

static int dfs_yaffs_statfs(struct dfs_filesystem *fs, struct statfs *buf)
{
    rt_mtd_t * mtd = (rt_mtd_t *)fs->dev_id;

    RT_ASSERT(mtd != RT_NULL);

    buf->f_bsize = mtd->sector_size;
    buf->f_blocks = mtd->size/ mtd->sector_size;
    buf->f_bfree = yaffs_freespace_reldev(mtd->priv) / mtd->sector_size;

    return 0;
}

static int dfs_yaffs_unlink(struct dfs_filesystem *fs, const char *path)
{
    int result;
    struct yaffs_stat s;

    /* judge file type, dir is to be delete by yaffs_rmdir, others by yaffs_unlink */
    if (yaffs_lstat_reldir(fs->data, path, &s) < 0)
    {
        return yaffsfs_GetLastError();
    }

    switch (s.st_mode & S_IFMT)
    {
    case S_IFREG:
        result = yaffs_unlink_reldir(fs->data, path);
        break;
    case S_IFDIR:
        result = yaffs_rmdir_reldir(fs->data, path);
        break;
    default:
        /* unknown file type */
        return -1;
    }
    if (result < 0)
        return yaffsfs_GetLastError();

    return 0;
}

static int dfs_yaffs_stat(struct dfs_filesystem *fs, const char *path, struct stat *st)
{
    int result;
    struct yaffs_stat s;

    result = yaffs_stat_reldir(fs->data, path, &s);
    if (result < 0)
        return yaffsfs_GetLastError();

    /* convert to dfs stat structure */
    st->st_dev = 0;
    st->st_mode = s.st_mode;
    st->st_size = s.st_size;
    st->st_mtime = s.yst_mtime;

    return 0;
}

static int dfs_yaffs_rename(struct dfs_filesystem *fs, const char *oldpath, const char *newpath)
{
    int result;

    result = yaffs_rename_reldir(fs->data, oldpath, newpath);

    if (result < 0)
        return yaffsfs_GetLastError();

    return 0;
}

static const struct dfs_file_ops _fops =
{
    dfs_yfile_open,
    dfs_yfile_close,
    dfs_yfile_ioctl,
    dfs_yfile_read,
    dfs_yfile_write,
    dfs_yfile_flush,
    dfs_yfile_lseek,
    dfs_yfile_getdents,
    RT_NULL, /* poll interface */
};

static const struct dfs_filesystem_ops _fsops =
{
    "yaffs",
    DFS_FS_FLAG_DEFAULT,
    &_fops,

    dfs_yaffs_mount,
    dfs_yaffs_unmount,
    dfs_yaffs_mkfs,
    dfs_yaffs_statfs,

    dfs_yaffs_unlink,
    dfs_yaffs_stat,
    dfs_yaffs_rename,
};

int dfs_yaffs_init(void)
{
    /* register fatfs file system */

    return dfs_register(&_fsops);
}
INIT_COMPONENT_EXPORT(dfs_yaffs_init);
