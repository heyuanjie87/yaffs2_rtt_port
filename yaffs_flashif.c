#include "yaffs/direct/yaffs_flashif2.h"

static int write_chunk(struct yaffs_dev *dev, int nand_chunk,
                       const u8 *data, int data_len,
                       const u8 *oob, int oob_len)
{
    struct mtd_io_desc desc = { 0 };
    int ret;
    loff_t addr;

    desc.datbuf = (u8*)data;
    desc.datlen = data_len;
    desc.oobbuf = (u8*)oob;
    desc.ooblen = oob_len;
    desc.mode = MTD_OPM_AUTO_OOB;
    addr = dev->param.total_bytes_per_chunk * nand_chunk;

    ret = rt_mtd_write_oob(dev->driver_context, addr, &desc);

    return ret? YAFFS_FAIL : YAFFS_OK;
}

static int read_chunk(struct yaffs_dev *dev, int nand_chunk,
                      u8 *data, int data_len,
                      u8 *oob, int oob_len,
                      enum yaffs_ecc_result *ecc_result)
{
    struct mtd_io_desc desc = { 0 };
    int ret;
    loff_t addr;

    desc.datbuf = data;
    desc.datlen = data_len;
    desc.oobbuf = oob;
    desc.ooblen = oob_len;
    desc.mode = MTD_OPM_AUTO_OOB;
    addr = dev->param.total_bytes_per_chunk * nand_chunk;

    ret = rt_mtd_read_oob(dev->driver_context, addr, &desc);
    *ecc_result = YAFFS_ECC_RESULT_NO_ERROR;

    return ret ? YAFFS_FAIL : YAFFS_OK;
}

static int erase(struct yaffs_dev *dev, int block_no)
{
    int ret;

    ret = rt_mtd_block_erase(dev->driver_context, block_no);

    return ret ? YAFFS_FAIL : YAFFS_OK;
}

static int mark_bad(struct yaffs_dev *dev, int block_no)
{
    rt_mtd_block_markbad(dev->driver_context, block_no);

    return YAFFS_OK;
}

static int check_bad(struct yaffs_dev *dev, int block_no)
{
    int ret;

    ret = rt_mtd_block_isbad(dev->driver_context, block_no);

    return ret ? YAFFS_FAIL : YAFFS_OK;
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
    dev->drv.drv_write_chunk_fn   = write_chunk;
}

RT_WEAK int yaffs_start_up(void)
{
    static struct yaffs_dev flash;
    rt_mtd_t *mtd;

    mtd = rt_mtd_get("nand1");
    if (!mtd)
        return -1;
    if (mtd->type != MTD_TYPE_NAND)
        return -1;

    rt_memset(&flash, 0, sizeof(flash));
    yaffsfs_OSInitialisation();

    flash.param.name = "/nf";
    flash.param.n_caches = 0;
    flash.param.start_block = 0;
    flash.param.end_block   = (mtd->size / mtd->block_size) - 1;
    flash.param.total_bytes_per_chunk = mtd->sector_size;
    flash.param.spare_bytes_per_chunk = mtd->oob_size;
    flash.param.use_nand_ecc = 0;
    flash.param.is_yaffs2 = 1;
    flash.param.n_reserved_blocks = 32;
    flash.param.chunks_per_block = mtd->block_size / mtd->sector_size;
    flash.driver_context = mtd;
    mtd->priv = &flash;

    yaffs_mtd_drv_install(&flash);
    yaffs_add_device(&flash);

    return 0;
}
