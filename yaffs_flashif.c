#include "yaffs/direct/yaffs_flashif2.h"

static int write_chunk(struct yaffs_dev *dev, int nand_chunk,
                       const u8 *data, int data_len,
                       const u8 *oob, int oob_len)
{
    return rt_mtd_nand_write(RT_MTD_NAND_DEVICE(dev->driver_context), nand_chunk,  data, data_len, oob, oob_len) ? YAFFS_FAIL : YAFFS_OK;
}

static int read_chunk(struct yaffs_dev *dev, int nand_chunk,
                      u8 *data, int data_len,
                      u8 *oob, int oob_len,
                      enum yaffs_ecc_result *ecc_result)
{
    int ret = -1;

    if (data == NULL && oob == NULL)
    {
        goto exit_read_chunk;
    }

    ret = rt_mtd_nand_read(RT_MTD_NAND_DEVICE(dev->driver_context),
                           nand_chunk, data, data_len, oob, oob_len);

    if (ret == RT_MTD_EOK)
    {
        *ecc_result = YAFFS_ECC_RESULT_NO_ERROR;
    }
    else if (ret == -RT_MTD_EECC_CORRECT)
    {
        *ecc_result = YAFFS_ECC_RESULT_FIXED;
    }
    else if (ret == -RT_MTD_EECC)
    {
        *ecc_result = YAFFS_ECC_RESULT_UNFIXED;
    }

exit_read_chunk:

    return ret ? YAFFS_FAIL : YAFFS_OK;
}

static int erase(struct yaffs_dev *dev, int block_no)
{
    return rt_mtd_nand_erase_block(RT_MTD_NAND_DEVICE(dev->driver_context), block_no) ? YAFFS_FAIL : YAFFS_OK;
}

static int mark_bad(struct yaffs_dev *dev, int block_no)
{
    return rt_mtd_nand_mark_badblock(RT_MTD_NAND_DEVICE(dev->driver_context), block_no) ? YAFFS_FAIL : YAFFS_OK;
}

static int check_bad(struct yaffs_dev *dev, int block_no)
{
    return rt_mtd_nand_check_block(RT_MTD_NAND_DEVICE(dev->driver_context), block_no) ? YAFFS_FAIL : YAFFS_OK;
}

static int initialise(struct yaffs_dev *dev)
{
    return YAFFS_OK;
}

static int deinitialise(struct yaffs_dev *dev)
{
    return YAFFS_OK;
}

void yaffs_mtd_drv_install(struct yaffs_dev *dev)
{
    dev->drv.drv_deinitialise_fn = deinitialise;
    dev->drv.drv_initialise_fn   = initialise;
    dev->drv.drv_check_bad_fn    = check_bad;
    dev->drv.drv_mark_bad_fn     = mark_bad;
    dev->drv.drv_erase_fn        = erase;
    dev->drv.drv_read_chunk_fn   = read_chunk;
    dev->drv.drv_write_chunk_fn  = write_chunk;
}

RT_WEAK int yaffs_start_up(rt_mtd_nand_t psMtdNandDev, const char *pcMountingPath)
{
    rt_device_t psRTDev;
    RT_ASSERT(psMtdNandDev);

    struct yaffs_dev *psYaffsDev = (struct yaffs_dev *)rt_malloc(sizeof(struct yaffs_dev));
    if (!psYaffsDev)
    {
        rt_kprintf("Fail to memory allocation.\n");
        goto exit_yaffs_start_up;
    }
    rt_memset(psYaffsDev, 0, sizeof(struct yaffs_dev));

    psYaffsDev->param.name = pcMountingPath;
    psYaffsDev->param.inband_tags = 1;
    psYaffsDev->param.n_caches = 10;
    psYaffsDev->param.start_block = psMtdNandDev->block_start;
    psYaffsDev->param.end_block   = psMtdNandDev->block_end;;
    psYaffsDev->param.total_bytes_per_chunk = psMtdNandDev->page_size;
    psYaffsDev->param.spare_bytes_per_chunk = psMtdNandDev->oob_size;
    psYaffsDev->param.use_nand_ecc = 1;
    psYaffsDev->param.is_yaffs2 = 1;
    psYaffsDev->param.refresh_period = 1000;
    psYaffsDev->param.no_tags_ecc = 1;
    psYaffsDev->param.empty_lost_n_found = 1;
    psYaffsDev->param.n_reserved_blocks = 5;
    psYaffsDev->param.enable_xattr = 1;
    psYaffsDev->param.hide_lost_n_found = 1;
    psYaffsDev->param.always_check_erased = 0;
    psYaffsDev->param.chunks_per_block = psMtdNandDev->pages_per_block;
    psYaffsDev->driver_context = psMtdNandDev;

    yaffs_mtd_drv_install(psYaffsDev);
    yaffs_add_device(psYaffsDev);

exit_yaffs_start_up:

    psMtdNandDev->priv = (void *)psYaffsDev;
    return 0;
}

