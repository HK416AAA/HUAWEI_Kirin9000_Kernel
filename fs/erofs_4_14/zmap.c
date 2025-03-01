// SPDX-License-Identifier: GPL-2.0
/*
 * linux/drivers/staging/erofs/zmap.c
 *
 * Copyright (C) 2018-2019 HUAWEI, Inc.
 *             http://www.huawei.com/
 * Created by Gao Xiang <gaoxiang25@huawei.com>
 */
#include "internal.h"
#include <asm/unaligned.h>
#include <linux/version.h>
#include <trace/events/erofs.h>

int z_erofs_fill_inode(struct inode *inode)
{
	struct erofs_vnode *const vi = EROFS_V(inode);
	struct super_block *const sb = inode->i_sb;

	if (vi->datamode == EROFS_INODE_FLAT_COMPRESSION_LEGACY) {
		vi->z_advise = 0;
		vi->z_algorithmtype[0] = 0;
		vi->z_algorithmtype[1] = 0;
		vi->z_logical_clusterbits = EROFS_SB(sb)->clusterbits;
		vi->z_physical_clusterbits[0] = vi->z_logical_clusterbits;
		vi->z_physical_clusterbits[1] = vi->z_logical_clusterbits;
		set_bit(EROFS_V_Z_INITED_BIT, &vi->flags);
	}

	inode->i_mapping->a_ops = &z_erofs_vle_normalaccess_aops;
	return 0;
}

static int fill_inode_lazy(struct inode *inode)
{
	struct erofs_vnode *const vi = EROFS_V(inode);
	struct super_block *const sb = inode->i_sb;
	int err;
	erofs_off_t pos;
	struct page *page;
	void *kaddr;
	struct z_erofs_map_header *h;

	if (test_bit(EROFS_V_Z_INITED_BIT, &vi->flags))
		return 0;

	if (wait_on_bit_lock(&vi->flags, EROFS_V_BL_Z_BIT, TASK_KILLABLE))
		return -ERESTARTSYS;

	err = 0;
	if (test_bit(EROFS_V_Z_INITED_BIT, &vi->flags))
		goto out_unlock;

	DBG_BUGON(vi->datamode == EROFS_INODE_FLAT_COMPRESSION_LEGACY);

	pos = ALIGN(iloc(EROFS_SB(sb), vi->nid) + vi->inode_isize +
		    vi->xattr_isize, 8);
	page = erofs_get_meta_page(sb, erofs_blknr(pos), false);
	if (IS_ERR(page)) {
		err = PTR_ERR(page);
		goto out_unlock;
	}

	kaddr = kmap_atomic(page);

	h = kaddr + erofs_blkoff(pos);
	vi->z_advise = le16_to_cpu(h->h_advise);
	vi->z_algorithmtype[0] = h->h_algorithmtype & 15;
	vi->z_algorithmtype[1] = h->h_algorithmtype >> 4;

	if (vi->z_algorithmtype[0] >= Z_EROFS_COMPRESSION_MAX) {
		errln("unknown compression format %u for nid %llu, please upgrade kernel",
		      vi->z_algorithmtype[0], vi->nid);
		err = -ENOTSUPP;
		goto unmap_done;
	}

	vi->z_logical_clusterbits = LOG_BLOCK_SIZE + (h->h_clusterbits & 7);
	vi->z_physical_clusterbits[0] = vi->z_logical_clusterbits +
					((h->h_clusterbits >> 3) & 3);

	if (vi->z_physical_clusterbits[0] != LOG_BLOCK_SIZE) {
		errln("unsupported physical clusterbits %u for nid %llu, please upgrade kernel",
		      vi->z_physical_clusterbits[0], vi->nid);
		err = -ENOTSUPP;
		goto unmap_done;
	}

	vi->z_physical_clusterbits[1] = vi->z_logical_clusterbits +
					((h->h_clusterbits >> 5) & 7);
unmap_done:
	kunmap_atomic(kaddr);
	unlock_page(page);
	put_page(page);

	set_bit(EROFS_V_Z_INITED_BIT, &vi->flags);
out_unlock:
	clear_and_wake_up_bit(EROFS_V_BL_Z_BIT, &vi->flags);
	return err;
}

struct z_erofs_maprecorder {
	struct inode *inode;
	struct erofs_map_blocks *map;
	void *kaddr;

	unsigned long lcn;
	/* compression extent information gathered */
	u8  type;
	u16 clusterofs;
	u16 delta[2];
	erofs_blk_t pblk;
};

static int z_erofs_reload_indexes(struct z_erofs_maprecorder *m,
				  erofs_blk_t eblk)
{
	struct super_block *const sb = m->inode->i_sb;
	struct erofs_map_blocks *const map = m->map;
	struct page *mpage = map->mpage;

	if (mpage) {
		if (mpage->index == eblk) {
			if (!m->kaddr)
				m->kaddr = kmap_atomic(mpage);
			return 0;
		}

		if (m->kaddr) {
			kunmap_atomic(m->kaddr);
			m->kaddr = NULL;
		}
		put_page(mpage);
	}

	mpage = erofs_get_meta_page(sb, eblk, false);
	if (IS_ERR(mpage)) {
		map->mpage = NULL;
		return PTR_ERR(mpage);
	}
	m->kaddr = kmap_atomic(mpage);
	unlock_page(mpage);
	map->mpage = mpage;
	return 0;
}

static int vle_legacy_load_cluster_from_disk(struct z_erofs_maprecorder *m,
					     unsigned long lcn)
{
	struct inode *const inode = m->inode;
	struct erofs_vnode *const vi = EROFS_V(inode);
	const erofs_off_t ibase = iloc(EROFS_I_SB(inode), vi->nid);
	const erofs_off_t pos = Z_EROFS_VLE_EXTENT_ALIGN(ibase +
							 vi->inode_isize +
							 vi->xattr_isize) +
		16 + lcn * sizeof(struct z_erofs_vle_decompressed_index);
	struct z_erofs_vle_decompressed_index *di;
	unsigned int advise, type;
	int err;

	err = z_erofs_reload_indexes(m, erofs_blknr(pos));
	if (err)
		return err;

	m->lcn = lcn;
	di = m->kaddr + erofs_blkoff(pos);

	advise = le16_to_cpu(di->di_advise);
	type = (advise >> Z_EROFS_VLE_DI_CLUSTER_TYPE_BIT) &
		((1 << Z_EROFS_VLE_DI_CLUSTER_TYPE_BITS) - 1);
	switch (type) {
	case Z_EROFS_VLE_CLUSTER_TYPE_NONHEAD:
		m->clusterofs = 1 << vi->z_logical_clusterbits;
		m->delta[0] = le16_to_cpu(di->di_u.delta[0]);
		m->delta[1] = le16_to_cpu(di->di_u.delta[1]);
		break;
#ifdef CONFIG_EROFS_FS_HUAWEI_EXTENSION
#define HW_COMPAT_DELTA0_LO_BITS		4
#define HW_COMPAT_DELTA0_HI_MASK		((1 << 4) - 1)
#define HW_COMPAT_ADVISE_DELTA0_HI_SHIFT	4

	case Z_EROFS_VLE_CLUSTER_TYPE_HUAWEI_COMPAT:
		m->pblk = le32_to_cpu(di->di_u.blkaddr);
		m->delta[0] = (((advise >> HW_COMPAT_ADVISE_DELTA0_HI_SHIFT) &
				HW_COMPAT_DELTA0_HI_MASK) <<
				HW_COMPAT_DELTA0_LO_BITS) |
				(le16_to_cpu((di)->di_clusterofs) >>
				 vi->z_logical_clusterbits);
		m->clusterofs = le16_to_cpu((di)->di_clusterofs) &
				((1 << vi->z_logical_clusterbits) - 1);
		break;
		/* fallthrough */
#endif
	case Z_EROFS_VLE_CLUSTER_TYPE_PLAIN:
	case Z_EROFS_VLE_CLUSTER_TYPE_HEAD:
		m->clusterofs = le16_to_cpu(di->di_clusterofs);
		m->pblk = le32_to_cpu(di->di_u.blkaddr);
		break;
	default:
		DBG_BUGON(1);
		return -EIO;
	}
	m->type = type;
	return 0;
}

static unsigned int decode_compactedbits(unsigned int lobits,
					 unsigned int lomask,
					 u8 *in, unsigned int pos, u8 *type)
{
	const unsigned int v = get_unaligned_le32(in + pos / 8) >> (pos & 7);
	const unsigned int lo = v & lomask;

	*type = (v >> lobits) & 3;
	return lo;
}

static int unpack_compacted_index(struct z_erofs_maprecorder *m,
				  unsigned int amortizedshift,
				  unsigned int eofs)
{
	struct erofs_vnode *const vi = EROFS_V(m->inode);
	const unsigned int lclusterbits = vi->z_logical_clusterbits;
	const unsigned int lomask = (1 << lclusterbits) - 1;
	unsigned int vcnt, base, lo, encodebits, nblk;
	int i;
	u8 *in, type;

	if (1 << amortizedshift == 4)
		vcnt = 2;
	else if (1 << amortizedshift == 2 && lclusterbits == 12)
		vcnt = 16;
	else
		return -ENOTSUPP;

	encodebits = ((vcnt << amortizedshift) - sizeof(__le32)) * 8 / vcnt;
	base = round_down(eofs, vcnt << amortizedshift);
	in = m->kaddr + base;

	i = (eofs - base) >> amortizedshift;

	lo = decode_compactedbits(lclusterbits, lomask,
				  in, encodebits * i, &type);
	m->type = type;
	if (type == Z_EROFS_VLE_CLUSTER_TYPE_NONHEAD) {
		m->clusterofs = 1 << lclusterbits;
		if (i + 1 != vcnt) {
			m->delta[0] = lo;
			return 0;
		}
		/*
		 * since the last lcluster in the pack is special,
		 * of which lo saves delta[1] rather than delta[0].
		 * Hence, get delta[0] by the previous lcluster indirectly.
		 */
		lo = decode_compactedbits(lclusterbits, lomask,
					  in, encodebits * (i - 1), &type);
		if (type != Z_EROFS_VLE_CLUSTER_TYPE_NONHEAD)
			lo = 0;
		m->delta[0] = lo + 1;
		return 0;
	}
	m->clusterofs = lo;
	m->delta[0] = 0;
	/* figout out blkaddr (pblk) for HEAD lclusters */
	nblk = 1;
	while (i > 0) {
		--i;
		lo = decode_compactedbits(lclusterbits, lomask,
					  in, encodebits * i, &type);
		if (type == Z_EROFS_VLE_CLUSTER_TYPE_NONHEAD)
			i -= lo;

		if (i >= 0)
			++nblk;
	}
	in += (vcnt << amortizedshift) - sizeof(__le32);
	m->pblk = le32_to_cpu(*(__le32 *)in) + nblk;
	return 0;
}

static int compacted_load_cluster_from_disk(struct z_erofs_maprecorder *m,
					    unsigned long lcn)
{
	struct inode *const inode = m->inode;
	struct erofs_vnode *const vi = EROFS_V(inode);
	const unsigned int lclusterbits = vi->z_logical_clusterbits;
	const erofs_off_t ebase = ALIGN(iloc(EROFS_I_SB(inode), vi->nid) +
					vi->inode_isize + vi->xattr_isize, 8) +
		sizeof(struct z_erofs_map_header);
	const unsigned int totalidx = DIV_ROUND_UP(inode->i_size, EROFS_BLKSIZ);
	unsigned int compacted_4b_initial, compacted_2b;
	unsigned int amortizedshift;
	erofs_off_t pos;
	int err;

	if (lclusterbits != 12)
		return -ENOTSUPP;

	if (lcn >= totalidx)
		return -EINVAL;

	m->lcn = lcn;
	/* used to align to 32-byte (compacted_2b) alignment */
	compacted_4b_initial = (32 - ebase % 32) / 4;
	if (compacted_4b_initial == 32 / 4)
		compacted_4b_initial = 0;

	if (vi->z_advise & Z_EROFS_ADVISE_COMPACTED_2B)
		compacted_2b = rounddown(totalidx - compacted_4b_initial, 16);
	else
		compacted_2b = 0;

	pos = ebase;
	if (lcn < compacted_4b_initial) {
		amortizedshift = 2;
		goto out;
	}
	pos += compacted_4b_initial * 4;
	lcn -= compacted_4b_initial;

	if (lcn < compacted_2b) {
		amortizedshift = 1;
		goto out;
	}
	pos += compacted_2b * 2;
	lcn -= compacted_2b;
	amortizedshift = 2;
out:
	pos += lcn * (1 << amortizedshift);
	err = z_erofs_reload_indexes(m, erofs_blknr(pos));
	if (err)
		return err;
	return unpack_compacted_index(m, amortizedshift, erofs_blkoff(pos));
}

static int vle_load_cluster_from_disk(struct z_erofs_maprecorder *m,
				      unsigned int lcn)
{
	const unsigned int datamode = EROFS_V(m->inode)->datamode;

	if (datamode == EROFS_INODE_FLAT_COMPRESSION_LEGACY)
		return vle_legacy_load_cluster_from_disk(m, lcn);

	if (datamode == EROFS_INODE_FLAT_COMPRESSION)
		return compacted_load_cluster_from_disk(m, lcn);

	return -EINVAL;
}

static int vle_extent_lookback(struct z_erofs_maprecorder *m,
			       unsigned int lookback_distance)
{
	struct erofs_vnode *const vi = EROFS_V(m->inode);
	struct erofs_map_blocks *const map = m->map;
	const unsigned int lclusterbits = vi->z_logical_clusterbits;
	unsigned long lcn = m->lcn;
	int err;

	if (lcn < lookback_distance) {
		DBG_BUGON(1);
		return -EIO;
	}

	/* load extent head logical cluster if needed */
	lcn -= lookback_distance;
	err = vle_load_cluster_from_disk(m, lcn);
	if (err)
		return err;

	switch (m->type) {
	case Z_EROFS_VLE_CLUSTER_TYPE_NONHEAD:
		return vle_extent_lookback(m, m->delta[0]);
	case Z_EROFS_VLE_CLUSTER_TYPE_PLAIN:
		map->m_flags &= ~EROFS_MAP_ZIPPED;
		/* fallthrough */
#ifdef CONFIG_EROFS_FS_HUAWEI_EXTENSION
	case Z_EROFS_VLE_CLUSTER_TYPE_HUAWEI_COMPAT:
		lcn -= m->delta[0];
		/* fallthrough */
#endif
	case Z_EROFS_VLE_CLUSTER_TYPE_HEAD:
		map->m_la = (lcn << lclusterbits) | m->clusterofs;
		break;
	default:
		errln("unknown type %u at lcn %lu of nid %llu",
		      m->type, lcn, vi->nid);
		DBG_BUGON(1);
		return -EIO;
	}
	return 0;
}

int z_erofs_map_blocks_iter(struct inode *inode,
			    struct erofs_map_blocks *map,
			    int flags)
{
	struct erofs_vnode *const vi = EROFS_V(inode);
	struct z_erofs_maprecorder m = {
		.inode = inode,
		.map = map,
	};
	int err = 0;
	unsigned int lclusterbits, endoff;
	unsigned long long ofs, end;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	trace_erofs_map_blocks_flatmode_enter(inode, map, flags);
#else
	trace_z_erofs_map_blocks_iter_enter(inode, map, flags);
#endif
	/* when trying to read beyond EOF, leave it unmapped */
	if (unlikely(map->m_la >= inode->i_size)) {
		map->m_llen = map->m_la + 1 - inode->i_size;
		map->m_la = inode->i_size;
		map->m_flags = 0;
		goto out;
	}

	err = fill_inode_lazy(inode);
	if (err)
		goto out;

	lclusterbits = vi->z_logical_clusterbits;
	ofs = map->m_la;
	m.lcn = ofs >> lclusterbits;
	endoff = ofs & ((1 << lclusterbits) - 1);

	err = vle_load_cluster_from_disk(&m, m.lcn);
	if (err)
		goto unmap_out;

	map->m_flags = EROFS_MAP_ZIPPED;	/* by default, compressed */
	end = (m.lcn + 1ULL) << lclusterbits;

	switch (m.type) {
	case Z_EROFS_VLE_CLUSTER_TYPE_PLAIN:
		if (endoff >= m.clusterofs)
			map->m_flags &= ~EROFS_MAP_ZIPPED;
		/* fallthrough */
#ifdef CONFIG_EROFS_FS_HUAWEI_EXTENSION
	case Z_EROFS_VLE_CLUSTER_TYPE_HUAWEI_COMPAT:
		if (m.delta[0])
			goto nonhead;
		/* fallthrough */
#endif
	case Z_EROFS_VLE_CLUSTER_TYPE_HEAD:
		if (endoff >= m.clusterofs) {
			map->m_la = (m.lcn << lclusterbits) | m.clusterofs;
			break;
		}
		/* m.lcn should be >= 1 if endoff < m.clusterofs */
		if (unlikely(!m.lcn)) {
			errln("invalid logical cluster 0 at nid %llu",
			      vi->nid);
			err = -EIO;
			goto unmap_out;
		}
		end = (m.lcn << lclusterbits) | m.clusterofs;
		map->m_flags |= EROFS_MAP_FULL_MAPPED;
		m.delta[0] = 1;
		/* fallthrough */
	case Z_EROFS_VLE_CLUSTER_TYPE_NONHEAD:
nonhead:
		/* get the correspoinding first chunk */
		err = vle_extent_lookback(&m, m.delta[0]);
		if (unlikely(err))
			goto unmap_out;
		break;
	default:
		errln("unknown type %u at offset %llu of nid %llu",
		      m.type, ofs, vi->nid);
		err = -EIO;
		goto unmap_out;
	}

	map->m_llen = end - map->m_la;
	map->m_plen = 1 << lclusterbits;
	map->m_pa = blknr_to_addr(m.pblk);
	map->m_flags |= EROFS_MAP_MAPPED;

unmap_out:
	if (m.kaddr)
		kunmap_atomic(m.kaddr);

out:
	debugln("%s, m_la %llu m_pa %llu m_llen %llu m_plen %llu m_flags 0%o",
		__func__, map->m_la, map->m_pa,
		map->m_llen, map->m_plen, map->m_flags);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	trace_erofs_map_blocks_flatmode_exit(inode, map, flags, err);
#else
	trace_z_erofs_map_blocks_iter_exit(inode, map, flags, err);
#endif
	/* aggressively BUG_ON iff CONFIG_EROFS_FS_DEBUG is on */
	DBG_BUGON(err < 0 && err != -ENOMEM);
	return err;
}

